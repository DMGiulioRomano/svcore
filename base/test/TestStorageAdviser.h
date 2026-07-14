/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef TEST_STORAGE_ADVISER_H
#define TEST_STORAGE_ADVISER_H

#include "../StorageAdviser.h"

#include "system/System.h"

#include <QObject>
#include <QtTest>

#include <thread>
#include <vector>

using namespace sv;

class TestStorageAdviser : public QObject
{
    Q_OBJECT

    typedef StorageAdviser SA;

    size_t plannedTotal() {
        return SA::getPlannedAllocation(SA::MemoryAllocation) +
            SA::getPlannedAllocation(SA::DiscAllocation);
    }

    // An amount (in KB) that is guaranteed to exceed the machine's
    // free memory, so that planning it forces subsequent
    // recommendations away from memory
    size_t hugeKb() {
        ssize_t memoryFree = -1, memoryTotal = -1;
        GetRealMemoryMBAvailable(memoryFree, memoryTotal);
        if (memoryTotal > 0) {
            return size_t(memoryTotal) * 2 * 1024;
        } else {
            return size_t(1) << 33; // 8 TB in KB; more than any
                                    // plausible test machine
        }
    }

private slots:

    void notifyAccountingBalances() {
        size_t memBase = SA::getPlannedAllocation(SA::MemoryAllocation);
        size_t discBase = SA::getPlannedAllocation(SA::DiscAllocation);

        SA::notifyPlannedAllocation(SA::MemoryAllocation, 1000);
        SA::notifyPlannedAllocation(SA::DiscAllocation, 500);
        QCOMPARE(SA::getPlannedAllocation(SA::MemoryAllocation),
                 memBase + 1000);
        QCOMPARE(SA::getPlannedAllocation(SA::DiscAllocation),
                 discBase + 500);

        SA::notifyDoneAllocation(SA::MemoryAllocation, 1000);
        SA::notifyDoneAllocation(SA::DiscAllocation, 500);
        QCOMPARE(SA::getPlannedAllocation(SA::MemoryAllocation), memBase);
        QCOMPARE(SA::getPlannedAllocation(SA::DiscAllocation), discBase);
    }

    void notifyDoneClampsAtZero() {
        size_t memBase = SA::getPlannedAllocation(SA::MemoryAllocation);
        SA::notifyPlannedAllocation(SA::MemoryAllocation, 10);
        SA::notifyDoneAllocation(SA::MemoryAllocation,
                                 memBase + 1000000);
        QCOMPARE(SA::getPlannedAllocation(SA::MemoryAllocation), size_t(0));
    }

    void recommendWithTokenPlansImmediately() {
        size_t base = plannedTotal();

        SA::AllocationToken token;
        QVERIFY(!token.isActive());

        SA::Recommendation rec =
            SA::recommend(SA::NoCriteria, 100, 200, token);

        // The planned allocation must be registered before the
        // caller has allocated anything at all: this is the heart of
        // the accounting fix
        QVERIFY(token.isActive());
        QCOMPARE(token.getSize(), size_t(200));
        QCOMPARE(plannedTotal(), base + 200);

        // The planned area must match the area the recommendation
        // steers the caller towards (memory iff UseMemory or
        // PreferMemory, as AudioFileReaderFactory interprets it)
        if (rec & (SA::UseMemory | SA::PreferMemory)) {
            QCOMPARE(int(token.getArea()), int(SA::MemoryAllocation));
        } else {
            QCOMPARE(int(token.getArea()), int(SA::DiscAllocation));
        }

        SA::releaseAllocationToken(token);
        QVERIFY(!token.isActive());
        QCOMPARE(plannedTotal(), base);
    }

    void releaseTokenIsIdempotent() {
        size_t base = plannedTotal();

        SA::AllocationToken token;
        (void)SA::recommend(SA::NoCriteria, 50, 50, token);
        QCOMPARE(plannedTotal(), base + 50);

        SA::releaseAllocationToken(token);
        QCOMPARE(plannedTotal(), base);

        // Releasing again must not affect the accounting
        SA::releaseAllocationToken(token);
        QCOMPARE(plannedTotal(), base);

        // Nor must releasing a default-constructed token
        SA::AllocationToken inactive;
        SA::releaseAllocationToken(inactive);
        QCOMPARE(plannedTotal(), base);
    }

    void successiveRecommendationsAccumulate() {
        // The scenario from the issue: N recommendations are made in
        // advance of any of the resulting allocations. The (N+1)th
        // must see the sum of all of the previous ones as planned
        size_t base = plannedTotal();
        const int n = 10;
        const size_t sizeEach = 300;

        std::vector<SA::AllocationToken> tokens(n);
        for (int i = 0; i < n; ++i) {
            QCOMPARE(plannedTotal(), base + size_t(i) * sizeEach);
            (void)SA::recommend(SA::NoCriteria, sizeEach, sizeEach,
                                tokens[i]);
        }
        QCOMPARE(plannedTotal(), base + size_t(n) * sizeEach);

        for (int i = 0; i < n; ++i) {
            SA::releaseAllocationToken(tokens[i]);
        }
        QCOMPARE(plannedTotal(), base);
    }

    void plannedMemoryStopsMemoryRecommendations() {
        // Once enough memory has been planned, no amount of free
        // memory at measurement time may lead to another
        // memory-favouring recommendation
        size_t huge = hugeKb();
        SA::notifyPlannedAllocation(SA::MemoryAllocation, huge);

        SA::AllocationToken token;
        SA::Recommendation rec = SA::NoRecommendation;
        try {
            rec = SA::recommend(SA::NoCriteria, 16, 16, token);
        } catch (...) {
            SA::notifyDoneAllocation(SA::MemoryAllocation, huge);
            throw;
        }

        QVERIFY(!(rec & SA::UseMemory));
        QVERIFY(!(rec & SA::PreferMemory));
        QVERIFY(rec & SA::UseDisc);
        QCOMPARE(int(token.getArea()), int(SA::DiscAllocation));

        SA::releaseAllocationToken(token);
        SA::notifyDoneAllocation(SA::MemoryAllocation, huge);
    }

    void plainRecommendDoesNotPlan() {
        size_t base = plannedTotal();
        (void)SA::recommend(SA::NoCriteria, 100, 200);
        QCOMPARE(plannedTotal(), base);
    }

    void concurrentNotificationsAreAccounted() {
        // Decodes run on multiple threads (DecodeThreaded), so the
        // accounting must be thread-safe: no update may be lost
        size_t memBase = SA::getPlannedAllocation(SA::MemoryAllocation);

        const int nthreads = 8;
        const int iterations = 5000;
        const size_t sizeEach = 3;

        std::vector<std::thread> threads;
        for (int t = 0; t < nthreads; ++t) {
            threads.push_back(std::thread([=]() {
                for (int i = 0; i < iterations; ++i) {
                    SA::notifyPlannedAllocation(SA::MemoryAllocation,
                                                sizeEach);
                }
            }));
        }
        for (auto &t: threads) t.join();

        QCOMPARE(SA::getPlannedAllocation(SA::MemoryAllocation),
                 memBase + size_t(nthreads) * iterations * sizeEach);

        SA::notifyDoneAllocation(SA::MemoryAllocation,
                                 size_t(nthreads) * iterations * sizeEach);
        QCOMPARE(SA::getPlannedAllocation(SA::MemoryAllocation), memBase);
    }

    void concurrentTokensAreAccounted() {
        size_t base = plannedTotal();

        const int nthreads = 4;
        const int iterations = 25;
        const size_t sizeEach = 40;

        std::vector<std::thread> threads;
        for (int t = 0; t < nthreads; ++t) {
            threads.push_back(std::thread([=]() {
                for (int i = 0; i < iterations; ++i) {
                    SA::AllocationToken token;
                    try {
                        (void)SA::recommend(SA::NoCriteria,
                                            sizeEach, sizeEach, token);
                    } catch (...) {
                        // InsufficientDiscSpace should not happen for
                        // these sizes, but the accounting must stay
                        // balanced regardless
                    }
                    SA::releaseAllocationToken(token);
                }
            }));
        }
        for (auto &t: threads) t.join();

        QCOMPARE(plannedTotal(), base);
    }
};

#endif
