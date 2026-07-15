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

#ifndef TEST_DENSE_3D_MODEL_PEAK_CACHE_H
#define TEST_DENSE_3D_MODEL_PEAK_CACHE_H

#include "../Dense3DModelPeakCache.h"
#include "../EditableDenseThreeDimensionalModel.h"

#include "base/StorageAdviser.h"

#include <QObject>
#include <QtTest>

using namespace sv;

class TestDense3DModelPeakCache : public QObject
{
    Q_OBJECT

    ModelId makeSource(int width, int height) {
        auto source = std::make_shared<EditableDenseThreeDimensionalModel>
            (44100, 512, height, false);
        for (int x = 0; x < width; ++x) {
            EditableDenseThreeDimensionalModel::Column column(height, 0.5f);
            source->setColumn(x, column);
        }
        return ModelById::add(source);
    }

    size_t plannedMemory() {
        return StorageAdviser::getPlannedAllocation
            (StorageAdviser::MemoryAllocation);
    }

    size_t expectedKbFor(const Dense3DModelPeakCache &cache) {
        return (size_t(cache.getWidth()) * cache.getHeight() *
                sizeof(float)) / 1024;
    }

private slots:

    void plansFullExtentOnConstruction() {
        // The cache fills lazily, but must be accounted for at its
        // full potential extent from the moment it is created, so
        // that decisions about creating further caches see it
        size_t base = plannedMemory();
        ModelId sourceId = makeSource(100, 64);
        {
            Dense3DModelPeakCache cache(sourceId, 1);
            QCOMPARE(cache.getWidth(), 100);
            QCOMPARE(cache.getHeight(), 64);
            QCOMPARE(plannedMemory(), base + expectedKbFor(cache));
        }
        // ...and released when it is destroyed
        QCOMPARE(plannedMemory(), base);
        ModelById::release(sourceId);
    }

    void plansReducedExtentAtCoarserResolution() {
        size_t base = plannedMemory();
        ModelId sourceId = makeSource(100, 64);
        {
            Dense3DModelPeakCache cache(sourceId, 4);
            QCOMPARE(cache.getWidth(), 25);
            QCOMPARE(plannedMemory(), base + expectedKbFor(cache));
        }
        QCOMPARE(plannedMemory(), base);
        ModelById::release(sourceId);
    }

    void plansAccumulateAcrossCaches() {
        // The overcommit scenario: each new cache must be visible to
        // the accounting before any of them has filled
        size_t base = plannedMemory();
        ModelId sourceId = makeSource(200, 128);
        {
            Dense3DModelPeakCache first(sourceId, 1);
            size_t each = expectedKbFor(first);
            QVERIFY(each > 0);
            QCOMPARE(plannedMemory(), base + each);
            {
                Dense3DModelPeakCache second(sourceId, 1);
                QCOMPARE(plannedMemory(), base + 2 * each);
            }
            QCOMPARE(plannedMemory(), base + each);
        }
        QCOMPARE(plannedMemory(), base);
        ModelById::release(sourceId);
    }

    void plansNothingForInvalidSource() {
        size_t base = plannedMemory();
        {
            Dense3DModelPeakCache cache(ModelId(), 1);
            QCOMPARE(plannedMemory(), base);
        }
        QCOMPARE(plannedMemory(), base);
    }
};

#endif
