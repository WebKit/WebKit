/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCPriorityType.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct RTCDataChannelInit {
    Optional<bool> ordered;
    Optional<unsigned short> maxPacketLifeTime;
    Optional<unsigned short> maxRetransmits;
    String protocol;
    Optional<bool> negotiated;
    Optional<unsigned short> id;
    RTCPriorityType priority { RTCPriorityType::Low };

    RTCDataChannelInit isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<RTCDataChannelInit> decode(Decoder&);
};

inline RTCDataChannelInit RTCDataChannelInit::isolatedCopy() const
{
    auto copy = *this;
    copy.protocol = protocol.isolatedCopy();
    return copy;
}

template<class Encoder> void RTCDataChannelInit::encode(Encoder& encoder) const
{
    encoder << ordered << maxPacketLifeTime << maxRetransmits << protocol << negotiated << id << priority;
}

template<class Decoder> Optional<RTCDataChannelInit> RTCDataChannelInit::decode(Decoder& decoder)
{
    Optional<bool> ordered;
    decoder >> ordered;
    if (!ordered)
        return { };

    Optional<unsigned short> maxPacketLifeTime;
    decoder >> maxPacketLifeTime;
    if (!maxPacketLifeTime)
        return { };

    Optional<unsigned short> maxRetransmits;
    decoder >> maxRetransmits;
    if (!maxRetransmits)
        return { };

    String protocol;
    if (!decoder.decode(protocol))
        return { };

    Optional<bool> negotiated;
    decoder >> negotiated;
    if (!negotiated)
        return { };

    Optional<unsigned short> id;
    decoder >> id;
    if (!id)
        return { };

    Optional<RTCPriorityType> priority;
    decoder >> priority;
    if (!priority)
        return { };

    return RTCDataChannelInit { *ordered, *maxPacketLifeTime, *maxRetransmits, WTFMove(protocol), *negotiated, *id, *priority };
}

class RTCDataChannelHandlerClient;

class RTCDataChannelHandler {
public:
    virtual ~RTCDataChannelHandler() = default;

    virtual void setClient(RTCDataChannelHandlerClient&, ScriptExecutionContextIdentifier) = 0;

    virtual bool sendStringData(const CString&) = 0;
    virtual bool sendRawData(const char*, size_t) = 0;
    virtual void close() = 0;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
