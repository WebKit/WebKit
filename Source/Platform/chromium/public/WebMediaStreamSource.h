/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WebMediaStreamSource_h
#define WebMediaStreamSource_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebVector.h"

namespace WebCore {
class MediaStreamSource;
}

namespace WebKit {
class WebAudioDestinationConsumer;
class WebString;

class WebMediaStreamSource {
public:
    class ExtraData {
    public:
        virtual ~ExtraData() { }
    };

    enum Type {
        TypeAudio,
        TypeVideo
    };

    enum ReadyState {
        ReadyStateLive = 0,
        ReadyStateMuted = 1,
        ReadyStateEnded = 2
    };

    WebMediaStreamSource() { }
    WebMediaStreamSource(const WebMediaStreamSource& other) { assign(other); }
    ~WebMediaStreamSource() { reset(); }

    WebMediaStreamSource& operator=(const WebMediaStreamSource& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebMediaStreamSource&);

    WEBKIT_EXPORT void initialize(const WebString& id, Type, const WebString& name);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString id() const;
    WEBKIT_EXPORT Type type() const;
    WEBKIT_EXPORT WebString name() const;

    WEBKIT_EXPORT void setReadyState(ReadyState);
    WEBKIT_EXPORT ReadyState readyState() const;

    // Extra data associated with this object.
    // If non-null, the extra data pointer will be deleted when the object is destroyed.
    // Setting the extra data pointer will cause any existing non-null
    // extra data pointer to be deleted.
    WEBKIT_EXPORT ExtraData* extraData() const;
    WEBKIT_EXPORT void setExtraData(ExtraData*);

    WEBKIT_EXPORT WebString deviceId() const;
    WEBKIT_EXPORT void setDeviceId(const WebString&);

    // Only used if if this is a WebAudio source.
    // The WebAudioDestinationConsumer is not owned, and has to be disposed of separately
    // after calling removeAudioConsumer.
    WEBKIT_EXPORT bool requiresAudioConsumer() const;
    WEBKIT_EXPORT void addAudioConsumer(WebAudioDestinationConsumer*);
    WEBKIT_EXPORT bool removeAudioConsumer(WebAudioDestinationConsumer*);

#if WEBKIT_IMPLEMENTATION
    WebMediaStreamSource(const WTF::PassRefPtr<WebCore::MediaStreamSource>&);
    WebMediaStreamSource& operator=(WebCore::MediaStreamSource*);
    operator WTF::PassRefPtr<WebCore::MediaStreamSource>() const;
    operator WebCore::MediaStreamSource*() const;
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamSource> m_private;
};

} // namespace WebKit

#endif // WebMediaStreamSource_h
