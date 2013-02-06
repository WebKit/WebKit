/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebMediaStreamTrack_h
#define WebMediaStreamTrack_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"

namespace WebCore {
class MediaStreamComponent;
}

namespace WebKit {
class WebMediaStream;
class WebMediaStreamSource;
class WebString;

class WebMediaStreamTrack {
public:
    WebMediaStreamTrack() { }
    WebMediaStreamTrack(const WebMediaStreamTrack& other) { assign(other); }
    ~WebMediaStreamTrack() { reset(); }

    WebMediaStreamTrack& operator=(const WebMediaStreamTrack& other)
    {
        assign(other);
        return *this;
    }
    WEBKIT_EXPORT void assign(const WebMediaStreamTrack&);
    WEBKIT_EXPORT void initialize(const WebMediaStreamSource&);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString id() const;

    WEBKIT_EXPORT WebMediaStream stream() const;
    WEBKIT_EXPORT WebMediaStreamSource source() const;
    WEBKIT_EXPORT bool isEnabled() const;

#if WEBKIT_IMPLEMENTATION
    WebMediaStreamTrack(PassRefPtr<WebCore::MediaStreamComponent>);
    WebMediaStreamTrack(WebCore::MediaStreamComponent*);
    WebMediaStreamTrack& operator=(WebCore::MediaStreamComponent*);
    operator WTF::PassRefPtr<WebCore::MediaStreamComponent>() const;
    operator WebCore::MediaStreamComponent*() const;
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamComponent> m_private;
};

} // namespace WebKit

#endif // WebMediaStreamTrack_h
