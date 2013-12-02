/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Nix_MediaStreamAudioSource_h
#define Nix_MediaStreamAudioSource_h

#include "Common.h"
#include "MediaStreamSource.h"

namespace WebCore {
class MediaStreamAudioSource;
}

namespace Nix {
class AudioDestinationConsumer;

class MediaStreamAudioSource : public MediaStreamSource {
public:
    NIX_EXPORT MediaStreamAudioSource();
    MediaStreamAudioSource(const MediaStreamAudioSource& other) : MediaStreamSource(other) { assign(other); }
    ~MediaStreamAudioSource() { reset(); }

    MediaStreamAudioSource& operator=(const MediaStreamAudioSource& other)
    {
        assign(other);
        return *this;
    }

    NIX_EXPORT void assign(const MediaStreamAudioSource&);

    NIX_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    NIX_EXPORT const char* deviceId() const;
    NIX_EXPORT void setDeviceId(const char*);

    // The Nix::AudioDestinationConsumer is not owned, and has to be disposed of separately
    // after calling removeAudioConsumer.
    NIX_EXPORT void addAudioConsumer(AudioDestinationConsumer*);
    NIX_EXPORT bool removeAudioConsumer(AudioDestinationConsumer*);

#if BUILDING_NIX__
    MediaStreamAudioSource(const WTF::PassRefPtr<WebCore::MediaStreamAudioSource>&);
    MediaStreamAudioSource& operator=(WebCore::MediaStreamAudioSource*);
    operator WTF::PassRefPtr<WebCore::MediaStreamAudioSource>() const;
    operator WebCore::MediaStreamAudioSource*() const;
private:
    WebCore::MediaStreamAudioSource* toWebCoreAudioSource() const;
#endif

};

} // namespace Nix

#endif // Nix_MediaStreamAudioSource_h

