/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebRTCDataChannel_h
#define WebRTCDataChannel_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore {
class RTCDataChannelDescriptor;
}

namespace WebKit {

class WebRTCDataChannel {
public:
    class ExtraData {
    public:
        virtual ~ExtraData() { }
    };

    enum ReadyState {
        ReadyStateConnecting = 0,
        ReadyStateOpen = 1,
        ReadyStateClosing = 2,
        ReadyStateClosed = 3,
    };

    WebRTCDataChannel() { }
    WebRTCDataChannel(const WebRTCDataChannel& other) { assign(other); }
    ~WebRTCDataChannel() { reset(); }

    WebRTCDataChannel& operator=(const WebRTCDataChannel& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void initialize(const WebString& label, bool reliable);

    WEBKIT_EXPORT void assign(const WebRTCDataChannel&);

    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString label() const;
    WEBKIT_EXPORT bool reliable() const;

    WEBKIT_EXPORT void setBufferedAmount(unsigned long) const;
    WEBKIT_EXPORT void readyStateChanged(ReadyState) const;
    WEBKIT_EXPORT void dataArrived(const WebString&) const;
    WEBKIT_EXPORT void dataArrived(const char*, size_t) const;
    WEBKIT_EXPORT void error() const;

    // Extra data associated with this WebRTCDataChannel.
    // If non-null, the extra data pointer will be deleted when the object is destroyed.
    // Setting the extra data pointer will cause any existing non-null
    // extra data pointer to be deleted.
    WEBKIT_EXPORT ExtraData* extraData() const;
    WEBKIT_EXPORT void setExtraData(ExtraData*);

#if WEBKIT_IMPLEMENTATION
    WebRTCDataChannel(const WTF::PassRefPtr<WebCore::RTCDataChannelDescriptor>&);
    WebRTCDataChannel(WebCore::RTCDataChannelDescriptor*);
    operator WTF::PassRefPtr<WebCore::RTCDataChannelDescriptor>() const;
    operator WebCore::RTCDataChannelDescriptor*() const;
#endif

private:
    WebPrivatePtr<WebCore::RTCDataChannelDescriptor> m_private;
};

} // namespace WebKit

#endif // WebRTCDataChannel_h
