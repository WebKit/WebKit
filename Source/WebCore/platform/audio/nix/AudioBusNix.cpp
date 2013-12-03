/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"

#include "AudioFileReader.h"
#include "FileSystem.h"
#include <cstdio>
#include <public/MultiChannelPCMData.h>
#include <public/Platform.h>
#include <sys/stat.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

PassRefPtr<AudioBus> decodeAudioFileData(const char* data, size_t size, double sampleRate)
{
    OwnPtr<Nix::MultiChannelPCMData> pcmData = adoptPtr(Nix::Platform::current()->decodeAudioResource(data, size, sampleRate));
    if (pcmData)
        return adoptRef(static_cast<AudioBus*>(pcmData->getInternalData()));
    return nullptr;
}

PassRefPtr<AudioBus> AudioBus::loadPlatformResource(const char* name, float sampleRate)
{
    // FIXME: This assumes the file system uses latin1 or UTF-8 encoding, but this comment also assumes
    // that non-ascii file names would appear here.
    CString absoluteFilename(makeString(DATA_DIR, "/webaudio/resources/", name, ".wav").utf8());
    struct stat statData;
    if (::stat(absoluteFilename.data(), &statData) == -1)
        absoluteFilename = makeString(UNINSTALLED_AUDIO_RESOURCES_DIR, "/", name, ".wav").utf8();
    if (::stat(absoluteFilename.data(), &statData) == -1)
        return nullptr;

    FILE* file = fopen(absoluteFilename.data(), "rb");
    if (!file)
        return nullptr;

    WTF::Vector<char> fileContents;
    fileContents.resize(statData.st_size);
    size_t bytesRead = fread(&fileContents[0], 1, fileContents.size(), file);
    fclose(file);
    if (bytesRead < fileContents.size())
        fileContents.resize(bytesRead);

    return decodeAudioFileData(&fileContents[0], fileContents.size(), sampleRate);
}

PassRefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    RefPtr<AudioBus> audioBus = decodeAudioFileData(static_cast<const char*>(data), dataSize, sampleRate);
    if (!audioBus)
        return nullptr;

    // If the bus needs no conversion then return as is.
    if (!mixToMono || audioBus->numberOfChannels() == 1)
        return audioBus.release();

    return AudioBus::createBySampleRateConverting(audioBus.get(), mixToMono, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)

