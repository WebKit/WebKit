/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "RTCDataChannelHandler.h"
#include "RTCDataChannelIdentifier.h"
#include "RTCDataChannelState.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct DetachedRTCDataChannel {
    WTF_MAKE_NONCOPYABLE(DetachedRTCDataChannel);
    WTF_MAKE_FAST_ALLOCATED;
public:
    DetachedRTCDataChannel(RTCDataChannelIdentifier identifier, const String& label, const RTCDataChannelInit& options, RTCDataChannelState state)
        : identifier(identifier)
        , label(label.isolatedCopy())
        , options(options.isolatedCopy())
        , state(state)
    {
    }

    size_t memoryCost() const { return label.sizeInBytes(); }

    RTCDataChannelIdentifier identifier;
    String label;
    RTCDataChannelInit options;
    RTCDataChannelState state { RTCDataChannelState::Closed };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::unique_ptr<DetachedRTCDataChannel> decode(Decoder&);
};

template<class Encoder> void DetachedRTCDataChannel::encode(Encoder& encoder) const
{
    encoder << identifier << label << options << state;
}

template<class Decoder> std::unique_ptr<DetachedRTCDataChannel> DetachedRTCDataChannel::decode(Decoder& decoder)
{
    std::optional<RTCDataChannelIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return { };

    String label;
    if (!decoder.decode(label))
        return { };

    std::optional<RTCDataChannelInit> options;
    decoder >> options;
    if (!options)
        return { };

    std::optional<RTCDataChannelState> state;
    decoder >> state;
    if (!state)
        return { };

    return makeUnique<DetachedRTCDataChannel>(*identifier, WTFMove(label), *options, *state);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
