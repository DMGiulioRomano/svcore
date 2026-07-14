
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_STORAGE_ADVISER_H
#define SV_STORAGE_ADVISER_H

#include <cstdlib>

#include <QString>
#include <QMutex>

namespace sv {

/**
 * A utility class designed to help decide whether to store cache data
 * (for example FFT outputs) in memory or on disk in the TempDirectory.
 * This is basically a compendium of simple rules of thumb.
 */

class StorageAdviser
{
public:
    // pass to recommend() zero or more of these OR'd together
    enum Criteria {
        NoCriteria           = 0,
        SpeedCritical        = 1,
        PrecisionCritical    = 2,
        LongRetentionLikely  = 4,
        FrequentLookupLikely = 8
    };

    // recommend() returns one or two of these OR'd together
    enum Recommendation {
        NoRecommendation   = 0,
        UseMemory          = 1, // Disc is strongly contraindicated
        PreferMemory       = 2, // Either would do; memory probably better
        PreferDisc         = 4, // Either would do; disc probably better
        UseDisc            = 8, // Probably won't fit in memory
        ConserveSpace      = 16,// Whatever you choose, keep it compact
        UseAsMuchAsYouLike = 32 // Take my advice and there'll be space for all
    };

    /**
     * Recommend where to store some data, given certain storage and
     * recall criteria.  The minimum size is the approximate amount of
     * data in kilobytes that will be stored if the recommendation is
     * to ConserveSpace; the maximum size is approximately the amount
     * that will be used if UseAsMuchAsYouLike is returned.
     *
     * May throw InsufficientDiscSpace exception if there appears to
     * be nowhere the minimum amount of data can be stored.
     *
     * Note that this overload does not account for the allocation
     * that the recommendation is presumably about to lead to: if you
     * call it a number of times before allocating anything, all of
     * the recommendations will be counting the same free space. See
     * the overload taking an AllocationToken argument for a version
     * that registers the planned allocation immediately.
     */
    static Recommendation recommend(Criteria criteria,
                                    size_t minimumSize,
                                    size_t maximumSize);

    enum AllocationArea {
        MemoryAllocation,
        DiscAllocation
    };

    /**
     * A token representing an allocation planned on behalf of a
     * caller by the token-accepting overload of recommend(). A
     * default-constructed token is inactive and represents no
     * planned allocation. Tokens are freely copyable, but each
     * planned allocation must be released exactly once through
     * releaseAllocationToken() - so treat the active token as unique
     * and pass it around by reference.
     */
    class AllocationToken {
    public:
        AllocationToken() : m_area(MemoryAllocation), m_size(0) { }

        /** Area the allocation was planned in. Meaningless if the
         *  token is inactive. */
        AllocationArea getArea() const { return m_area; }

        /** Size in kilobytes registered as planned by this token, or
         *  zero if the token is inactive. */
        size_t getSize() const { return m_size; }

        bool isActive() const { return m_size > 0; }

    private:
        friend class StorageAdviser;
        AllocationToken(AllocationArea area, size_t size) :
            m_area(area), m_size(size) { }
        AllocationArea m_area;
        size_t m_size;
    };

    /**
     * Recommend where to store some data, as above, and atomically
     * register a planned allocation of approximately maximumSize
     * kilobytes in the area the recommendation steers the caller
     * towards (memory if UseMemory or PreferMemory is returned, disc
     * otherwise). This ensures that subsequent recommendations take
     * the allocation into account even before it has actually been
     * made, so a series of recommendations made in advance of any of
     * the resulting allocations cannot all promise the same free
     * space to different prospective users.
     *
     * The token describing the planned allocation is returned
     * through the token argument, replacing its previous value. The
     * caller must eventually pass it to releaseAllocationToken(),
     * either after abandoning the plan, or after completing the
     * allocation and separately accounting the actual size through
     * notifyPlannedAllocation()/notifyDoneAllocation().
     *
     * May throw InsufficientDiscSpace, in which case nothing is
     * registered and the token is left untouched.
     */
    static Recommendation recommend(Criteria criteria,
                                    size_t minimumSize,
                                    size_t maximumSize,
                                    AllocationToken &token);

    /**
     * Release an allocation planned through the token-accepting
     * overload of recommend(), removing it from the planned-space
     * accounting and resetting the token to inactive. Releasing an
     * inactive token has no effect, so this is safe to call on any
     * failure or cleanup path.
     */
    static void releaseAllocationToken(AllocationToken &token);

    /**
     * Specify that we are planning to use a given amount of storage
     * (in kilobytes), but haven't allocated it yet.
     */
    static void notifyPlannedAllocation(AllocationArea area, size_t size);

    /**
     * Specify that we have now allocated, or abandoned the allocation
     * of, the given amount (in kilobytes) of a storage area that was
     * previously notified using notifyPlannedAllocation.
     */
    static void notifyDoneAllocation(AllocationArea area, size_t size);

    /**
     * Return the total amount of storage (in kilobytes) currently
     * registered as planned in the given area. Intended for
     * diagnostics and tests.
     */
    static size_t getPlannedAllocation(AllocationArea area);

    /**
     * Force all subsequent recommendations to use the (perhaps
     * partial) specification given here.  If NoRecommendation given
     * here, this will reset to the default free behaviour.
     */
    static void setFixedRecommendation(Recommendation recommendation);

    /**
     * Set the greed level for RAM. This is a value from 0 to 1, by
     * which we scale the measured amount of free RAM before using it
     * in any heuristics. A value of 1 (the default) indicates that we
     * should behave as if we are the main thing the user is running
     * and that the free RAM is generally available to us. A value of
     * 0 indicates that we should act as if there is never any free
     * RAM at all.
     */
    static void setRAMGreed(double greed);
    
private:
    // All statics guarded by m_mutex: recommendations may be
    // requested, and allocations notified, from any thread
    static QMutex m_mutex;
    static size_t m_discPlanned;
    static size_t m_memoryPlanned;
    static Recommendation m_baseRecommendation;
    static double m_greed;

    enum StorageStatus {
        Unknown,
        Insufficient,
        Marginal,
        Sufficient
    };

    static QString criteriaToString(int);
    static QString recommendationToString(int);
    static QString storageStatusToString(StorageStatus);

    // These must be called with m_mutex held
    static Recommendation recommendLocked(Criteria criteria,
                                          size_t minimumSize,
                                          size_t maximumSize);
    static void recordPlannedAllocation(AllocationArea area, size_t size);
    static void recordDoneAllocation(AllocationArea area, size_t size);
};

} // end namespace sv

#endif

