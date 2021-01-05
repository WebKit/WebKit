/*
 *  Copyright (C) 2014 Haiku, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioFileReader.h"

#include "AudioBus.h"
#include "NotImplemented.h"

#include <File.h>
#include <MediaFile.h>
#include <MediaTrack.h>

namespace WebCore {

class AudioFileReader {
    WTF_MAKE_NONCOPYABLE(AudioFileReader);
public:
    AudioFileReader(const char* filePath);
    AudioFileReader(const void* data, size_t dataSize);
    ~AudioFileReader();

    PassRefPtr<AudioBus> createBus(float sampleRate, bool mixToMono);

private:
    BDataIO* m_data;
    BMediaFile* m_file;
};

AudioFileReader::AudioFileReader(const char* filePath)
{
    m_data = new BFile(filePath, B_READ_ONLY);
    m_file = new BMediaFile(m_data);
}

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
{
    m_data = new BMemoryIO(data, dataSize);
        // TODO should we take ownership of the data, or copy it?
    m_file = new BMediaFile(m_data);
}

AudioFileReader::~AudioFileReader()
{
    delete m_file;
    delete m_data;
}

PassRefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    BMediaTrack* track = m_file->TrackAt(0);

    if (track && track->InitCheck() == B_OK) {
        unsigned channels = mixToMono ? 1 : 2;
        RefPtr<AudioBus> audioBus = AudioBus::create(channels, track->CountFrames(), true);
        audioBus->setSampleRate(sampleRate);

        notImplemented();
            // TODO fill the audio bus with the frames!

        m_file->ReleaseTrack(track);
        return audioBus;
    } else {
        // Reading the file failed, return an empty bus
        RefPtr<AudioBus> audioBus = AudioBus::create(0, 0, true);
        m_file->ReleaseTrack(track);
        return audioBus;
    }
}

PassRefPtr<AudioBus> createBusFromAudioFile(const char* filePath, bool mixToMono, float sampleRate)
{
    return AudioFileReader(filePath).createBus(sampleRate, mixToMono);
}

PassRefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    return AudioFileReader(data, dataSize).createBus(sampleRate, mixToMono);
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
