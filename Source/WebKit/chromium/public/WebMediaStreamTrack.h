/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace WebCore {
class MediaStreamTrack;
}

namespace WebKit {

class WebString;

class WebMediaStreamTrack {
public:
    WebMediaStreamTrack() { }
    ~WebMediaStreamTrack() { reset(); }

    WEBKIT_EXPORT void initialize(const WebString& id, const WebString& kind, const WebString& label);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

#if WEBKIT_IMPLEMENTATION
    WebMediaStreamTrack(const WTF::PassRefPtr<WebCore::MediaStreamTrack>&);
    operator WTF::PassRefPtr<WebCore::MediaStreamTrack>() const;
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamTrack> m_private;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

#endif // WebMediaStreamTrack_h
