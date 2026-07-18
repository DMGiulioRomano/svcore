/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef AUDIO_FILE_READER_FACTORY_H
#define AUDIO_FILE_READER_FACTORY_H

#include <QString>

#include "FileSource.h"
#include "base/BaseTypes.h"

namespace sv {

class AudioFileReader;
class ProgressReporter;

class AudioFileReaderFactory
{
public:
    /**
     * Return the file extensions that we have audio file readers for,
     * in a format suitable for use with QFileDialog.  For example,
     * "*.wav *.aiff *.ogg".
     */
    static QString getKnownExtensions();

    enum class Normalisation {

        /**
         * Do not normalise file data.
         */
        None,

        /**
         * Normalise file data to abs(max) == 1.0.
         */
        Peak
    };

    enum class GaplessMode {

        /** 
         * Any encoder delay and padding found in file metadata will
         * be compensated for, giving gapless decoding (assuming the
         * metadata are correct). This is currently only applicable to
         * mp3 files: all other supported files are always gapless
         * where the file metadata provides for it. See documentation
         * for MP3FileReader::GaplessMode for details of the specific
         * implementation.
         */
        Gapless,

        /**
         * No delay compensation will happen and the results will be
         * equivalent to the behaviour of audio readers before the
         * compensation logic was implemented. This is currently only
         * applicable to mp3 files: all other supported files are
         * always gapless where the file metadata provides for it. See
         * documentation for MP3FileReader::GaplessMode for details of
         * the specific implementation.
         */
        Gappy
    };

    enum class ThreadingMode {
        
        /** 
         * Any necessary decoding will happen synchronously when the
         * reader is created.
         */
        NotThreaded,
        
        /**        
         * If the reader supports threaded decoding, it will be used
         * and the file will be decoded in a background thread. If the
         * reader does not support threaded decoding, behaviour will
         * be as for NotThreaded.
         */
        Threaded
    };

    struct Parameters {

        /**
         * Sample rate to open the file at. If zero (the default), the
         * file's native rate will be used. If non-zero, the file will
         * be automatically resampled to that rate.  You can query
         * reader->getNativeRate() if you want to find out whether the
         * file needed to be resampled.
         */
        sv_samplerate_t targetRate;

        /**
         * Normalisation to use. The default is Normalisation::None.
         */
        Normalisation normalisation;

        /**
         * Gapless mode to use. The default is GaplessMode::Gapless.
         */
        GaplessMode gaplessMode;

        /**
         * Threading mode. The default is ThreadingMode::NotThreaded.
         */
        ThreadingMode threadingMode;

        /**
         * True if the file is still being written and the frame count
         * may change. Not all readers support this.
         */
        bool fileUpdating;
        
        Parameters() :
            targetRate(0),
            normalisation(Normalisation::None),
            gaplessMode(GaplessMode::Gapless),
            threadingMode(ThreadingMode::NotThreaded),
            fileUpdating(false)
        { }
    };
    
    /**
     * Return an audio file reader initialised to the file at the
     * given path, or NULL if no suitable reader for this path is
     * available or the file cannot be opened.
     *
     * If a ProgressReporter is provided, it will be updated with
     * progress status. This will only be meaningful if decoding is
     * being carried out in non-threaded mode (either because the
     * threaded parameter was not supplied or because the specific
     * file reader used does not support it); in threaded mode,
     * reported progress will jump straight to 100% before threading
     * takes over. Caller retains ownership of the reporter object.
     *
     * Caller owns the returned object and must delete it after use.
     */
    static AudioFileReader *createReader(FileSource source,
                                         Parameters parameters,
                                         ProgressReporter *reporter = 0);

    /**
     * Return true if the given source has a file extension that
     * indicates a supported file type. This does not necessarily mean
     * that it can be opened; conversely it may theoretically be
     * possible to open some files without supported extensions,
     * depending on the readers available.
     */
    static bool isSupported(FileSource source);

    /**
     * The largest decoded size, in bytes, for which it is worth
     * making an in-memory copy of a file that could otherwise be
     * read directly from disc. See isWorthCopyingToMemory().
     */
    static constexpr size_t maxBytesWorthCopyingToMemory =
        size_t(256) * 1024 * 1024;

    /**
     * Return true if a file whose decoded data would occupy
     * approximately the given number of samples (frames x channels,
     * at the rate it will be read at, as estimated by
     * AudioFileSizeEstimator) is small enough to be worth copying
     * into an in-memory cache purely for speed of subsequent access,
     * in a situation where it could instead be read directly from
     * disc at no loading cost (i.e. a quickly-seekable file needing
     * no decoding, resampling, or normalisation).
     *
     * For short files the copy makes later random access faster and
     * costs little; for long files it costs hundreds of megabytes of
     * memory and a full read of the file at loading time, far
     * outweighing any subsequent benefit. A non-positive estimate is
     * treated as unknown and returns false.
     */
    static bool isWorthCopyingToMemory(sv_frame_t estimatedSamples) {
        if (estimatedSamples <= 0) return false;
        return size_t(estimatedSamples) <=
            maxBytesWorthCopyingToMemory / sizeof(float);
    }
};

} // end namespace sv

#endif

