/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebRTCSessionDescription_h
#define WebRTCSessionDescription_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"

namespace WebCore {
class RTCSessionDescriptionDescriptor;
}

namespace WebKit {
class WebString;

//  In order to establish the media plane, PeerConnection needs specific
//  parameters to indicate what to transmit to the remote side, as well
//  as how to handle the media that is received. These parameters are
//  determined by the exchange of session descriptions in offers and
//  answers, and there are certain details to this process that must be
//  handled in the JSEP APIs.
//
//  Whether a session description was sent or received affects the
//  meaning of that description. For example, the list of codecs sent to
//  a remote party indicates what the local side is willing to decode,
//  and what the remote party should send.

class WebRTCSessionDescription {
public:
    WebRTCSessionDescription() { }
    WebRTCSessionDescription(const WebRTCSessionDescription& other) { assign(other); }
    ~WebRTCSessionDescription() { reset(); }

    WebRTCSessionDescription& operator=(const WebRTCSessionDescription& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebRTCSessionDescription&);

    WEBKIT_EXPORT void initialize(const WebString& type, const WebString& sdp);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString type() const;
    WEBKIT_EXPORT void setType(const WebString&);
    WEBKIT_EXPORT WebString sdp() const;
    WEBKIT_EXPORT void setSDP(const WebString&);

#if WEBKIT_IMPLEMENTATION
    WebRTCSessionDescription(const WTF::PassRefPtr<WebCore::RTCSessionDescriptionDescriptor>&);

    operator WTF::PassRefPtr<WebCore::RTCSessionDescriptionDescriptor>() const;
#endif

private:
    WebPrivatePtr<WebCore::RTCSessionDescriptionDescriptor> m_private;
};

} // namespace WebKit

#endif // WebRTCSessionDescription_h
