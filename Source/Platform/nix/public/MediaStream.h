/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef Nix_MediaStream_h
#define Nix_MediaStream_h

#include "Common.h"
#include "PrivatePtr.h"

#include <cstddef>
#include <vector>

namespace WebCore {
class MediaStreamPrivate;
}

namespace Nix {

class MediaStreamSource;
class MediaStreamAudioSource;

class MediaStream {
public:
    MediaStream() { }
    MediaStream(const MediaStream& other) { assign(other); }
    ~MediaStream() { reset(); }

    MediaStream& operator=(const MediaStream& other)
    {
        assign(other);
        return *this;
    }

    NIX_EXPORT void assign(const MediaStream&);

    // Takes ownership of the MediaStreamSource* pointers, deleting them.
    NIX_EXPORT void initialize(std::vector<MediaStreamSource*>& audioTracks, std::vector<MediaStreamSource*>& videoTracks);

    NIX_EXPORT void reset();
    NIX_EXPORT bool isNull() const;


    NIX_EXPORT const char* id() const;

    // Caller must delete the returned MediaStreamSource*
    NIX_EXPORT std::vector<MediaStreamSource*> audioSources() const;
    NIX_EXPORT std::vector<MediaStreamSource*> videoSources() const;

    NIX_EXPORT void addSource(const MediaStreamSource&);
    NIX_EXPORT void removeSource(const MediaStreamSource&);

#if BUILDING_NIX__
    MediaStream(WebCore::MediaStreamPrivate*);
    MediaStream(const WTF::PassRefPtr<WebCore::MediaStreamPrivate>&);
    operator WTF::PassRefPtr<WebCore::MediaStreamPrivate>() const;
    operator WebCore::MediaStreamPrivate*() const;
    MediaStream& operator=(const WTF::PassRefPtr<WebCore::MediaStreamPrivate>&);
#endif

private:
    PrivatePtr<WebCore::MediaStreamPrivate> m_private;
};

} // namespace Nix

#endif // Nix_MediaStream_h

