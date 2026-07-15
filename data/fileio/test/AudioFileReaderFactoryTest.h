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

#ifndef TEST_AUDIO_FILE_READER_FACTORY_H
#define TEST_AUDIO_FILE_READER_FACTORY_H

#include "../AudioFileReaderFactory.h"

#include <QObject>
#include <QtTest>

using namespace sv;

class AudioFileReaderFactoryTest : public QObject
{
    Q_OBJECT

    // Estimated sample count (frames x channels) for a stereo file
    // of the given number of minutes at 44.1 kHz
    static sv_frame_t stereoMinutes(int minutes) {
        return sv_frame_t(minutes) * 60 * 44100 * 2;
    }

private slots:

    void shortFileIsWorthCopyingToMemory() {
        // A few minutes of stereo audio decodes to a modest amount of
        // memory: copying it into an in-memory cache for speed is fine
        QVERIFY(AudioFileReaderFactory::isWorthCopyingToMemory
                (stereoMinutes(5)));
    }

    void longFileIsNotWorthCopyingToMemory() {
        // The scenario from the issue: a ~40-minute stereo file
        // decodes to ~850 MB, which must not be speculatively copied
        // into memory when it could be read directly from disc
        QVERIFY(!AudioFileReaderFactory::isWorthCopyingToMemory
                (stereoMinutes(40)));
    }

    void thresholdBoundaryIsRespected() {
        sv_frame_t capSamples =
            sv_frame_t(AudioFileReaderFactory::maxBytesWorthCopyingToMemory /
                       sizeof(float));
        QVERIFY(AudioFileReaderFactory::isWorthCopyingToMemory(capSamples));
        QVERIFY(!AudioFileReaderFactory::isWorthCopyingToMemory(capSamples + 1));
    }

    void unknownSizeIsNotWorthCopyingToMemory() {
        // If we have no usable estimate, err on the side of not
        // committing memory
        QVERIFY(!AudioFileReaderFactory::isWorthCopyingToMemory(0));
        QVERIFY(!AudioFileReaderFactory::isWorthCopyingToMemory(-1));
    }
};

#endif
