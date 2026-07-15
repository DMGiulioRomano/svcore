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

#ifndef TEST_READ_ONLY_WAVE_FILE_MODEL_H
#define TEST_READ_ONLY_WAVE_FILE_MODEL_H

#include "../ReadOnlyWaveFileModel.h"

#include "data/fileio/AudioFileReader.h"

#include <QObject>
#include <QtTest>

#include <atomic>

using namespace sv;

class TestReadOnlyWaveFileModel : public QObject
{
    Q_OBJECT

    static constexpr sv_frame_t MOCK_FRAME_COUNT = 8192;
    static constexpr int MOCK_CHANNEL_COUNT = 2;

    static float valueFor(sv_frame_t frame, int channel) {
        float v = float(int(frame % 128)) / 128.f;
        return channel == 0 ? v : -v;
    }

    // A deterministic in-memory reader that counts how many times it
    // is asked for samples, so we can assert on the read behaviour of
    // the model's direct-read path
    class CountingReader : public AudioFileReader
    {
    public:
        CountingReader() {
            m_frameCount = MOCK_FRAME_COUNT;
            m_channelCount = MOCK_CHANNEL_COUNT;
            m_sampleRate = 44100;
        }

        QString getLocation() const override { return "test:counting"; }
        QString getLocalFilename() const override { return ""; }
        QString getTitle() const override { return "counting"; }
        QString getMaker() const override { return ""; }
        bool isQuicklySeekable() const override { return true; }

        floatvec_t getInterleavedFrames(sv_frame_t start,
                                        sv_frame_t count) const override {
            ++m_readCount;
            floatvec_t frames;
            for (sv_frame_t f = start; f < start + count; ++f) {
                if (f < 0 || f >= m_frameCount) break;
                for (int c = 0; c < m_channelCount; ++c) {
                    frames.push_back(valueFor(f, c));
                }
            }
            return frames;
        }

        int getReadCount() const { return m_readCount.load(); }
        void resetReadCount() { m_readCount = 0; }

    private:
        mutable std::atomic<int> m_readCount { 0 };
    };

    bool waitUntilReady(ReadOnlyWaveFileModel &model) {
        // The range-cache fill thread completes via queued signals,
        // so we must keep the event loop turning
        for (int i = 0; i < 1000; ++i) {
            if (model.isReady(nullptr)) return true;
            QTest::qWait(10);
        }
        return model.isReady(nullptr);
    }

private slots:

    void directReadSummariesAreCorrect() {
        // Block sizes below the coarsest range cache (64) force
        // getSummaries to read directly from the reader
        CountingReader reader;
        ReadOnlyWaveFileModel model("counting", &reader);
        QVERIFY(model.isOK());
        QVERIFY(waitUntilReady(model));

        const sv_frame_t start = 512;
        const sv_frame_t count = 1024;
        int blockSize = 16;

        for (int channel = 0; channel < MOCK_CHANNEL_COUNT; ++channel) {

            RangeSummarisableTimeValueModel::RangeBlock ranges;
            model.getSummaries(channel, start, count, ranges, blockSize);

            QCOMPARE(int(ranges.size()), int(count / blockSize));

            for (int i = 0; in_range_for(ranges, i); ++i) {
                float expectedMin = 0.f, expectedMax = 0.f;
                for (sv_frame_t j = 0; j < blockSize; ++j) {
                    float sample = valueFor(start + i * blockSize + j,
                                            channel);
                    if (j == 0 || sample < expectedMin) expectedMin = sample;
                    if (j == 0 || sample > expectedMax) expectedMax = sample;
                }
                QCOMPARE(ranges[i].min(), expectedMin);
                QCOMPARE(ranges[i].max(), expectedMax);
            }
        }
    }

    void alternatingDirectReadsDoNotEvictEachOther() {
        // The scenario from the issue: multiple views (here, two
        // distinct visible ranges, each summarised per channel)
        // repaint repeatedly while nothing changes. Each distinct
        // range must be read from the file only once, not once per
        // repaint - previously the single-entry read cache meant
        // that any two views with different ranges evicted each
        // other's data on every repaint
        CountingReader reader;
        ReadOnlyWaveFileModel model("counting", &reader);
        QVERIFY(model.isOK());
        QVERIFY(waitUntilReady(model));

        reader.resetReadCount();

        int blockSize = 16;

        for (int repaint = 0; repaint < 10; ++repaint) {
            for (int channel = 0; channel < MOCK_CHANNEL_COUNT; ++channel) {

                RangeSummarisableTimeValueModel::RangeBlock first, second;
                model.getSummaries(channel, 0, 1024, first, blockSize);
                model.getSummaries(channel, 4096, 1024, second, blockSize);

                QCOMPARE(int(first.size()), int(1024 / blockSize));
                QCOMPARE(int(second.size()), int(1024 / blockSize));
            }
        }

        QCOMPARE(reader.getReadCount(), 2);
    }
};

#endif
