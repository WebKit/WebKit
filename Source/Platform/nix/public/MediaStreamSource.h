/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef Nix_MediaStreamSource_h
#define Nix_MediaStreamSource_h

#include "Common.h"
#include "PrivatePtr.h"

namespace WebCore {
class MediaStreamSource;
}

namespace Nix {

class MediaStreamSource {
public:

    enum Type {
        Audio,
        Video
    };

    enum ReadyState {
        ReadyStateLive = 0,
        ReadyStateMuted = 1,
        ReadyStateEnded = 2
    };

    MediaStreamSource() { }
    MediaStreamSource(const MediaStreamSource& other) { assign(other); }
    virtual ~MediaStreamSource() { reset(); }

    MediaStreamSource& operator=(const MediaStreamSource& other)
    {
        assign(other);
        return *this;
    }

    NIX_EXPORT void assign(const MediaStreamSource&);

    NIX_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    NIX_EXPORT const char* id() const;
    NIX_EXPORT Type type() const;
    NIX_EXPORT const char* name() const;

    NIX_EXPORT void setReadyState(ReadyState);
    NIX_EXPORT ReadyState readyState() const;

    NIX_EXPORT bool enabled() const;
    NIX_EXPORT void setEnabled(bool);

    NIX_EXPORT bool muted() const;
    NIX_EXPORT void setMuted(bool);

    NIX_EXPORT bool readonly() const;
    NIX_EXPORT void setReadonly(bool);

    // FIXME Add support for capabilites.

#if BUILDING_NIX__
    MediaStreamSource(const WTF::PassRefPtr<WebCore::MediaStreamSource>&);
    MediaStreamSource& operator=(WebCore::MediaStreamSource*);
    operator WTF::PassRefPtr<WebCore::MediaStreamSource>() const;
    operator WebCore::MediaStreamSource*() const;
#endif

protected:
    PrivatePtr<WebCore::MediaStreamSource> m_private;
};

} // namespace Nix

#endif // Nix_MediaStreamSource_h

