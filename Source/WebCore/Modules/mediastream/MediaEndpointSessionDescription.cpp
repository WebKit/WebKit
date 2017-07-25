/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "config.h"
#include "MediaEndpointSessionDescription.h"

#if ENABLE(WEB_RTC)

#include "SDPProcessor.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

#define STRING_FUNCTION(name) \
    static const String& name##String() \
    { \
        static NeverDestroyed<const String> name(MAKE_STATIC_STRING_IMPL(#name)); \
        return name; \
    }

STRING_FUNCTION(offer)
STRING_FUNCTION(pranswer)
STRING_FUNCTION(answer)
STRING_FUNCTION(rollback)

Ref<MediaEndpointSessionDescription> MediaEndpointSessionDescription::create(RTCSdpType type, RefPtr<MediaEndpointSessionConfiguration>&& configuration)
{
    return adoptRef(*new MediaEndpointSessionDescription(type, WTFMove(configuration), nullptr));
}

ExceptionOr<Ref<MediaEndpointSessionDescription>> MediaEndpointSessionDescription::create(RefPtr<RTCSessionDescription>&& rtcDescription, const SDPProcessor& sdpProcessor)
{
    RefPtr<MediaEndpointSessionConfiguration> configuration;
    auto result = sdpProcessor.parse(rtcDescription->sdp(), configuration);
    if (result != SDPProcessor::Result::Success) {
        if (result == SDPProcessor::Result::ParseError)
            return Exception { InvalidAccessError, ASCIILiteral("SDP content is invalid") };
        LOG_ERROR("SDPProcessor internal error");
        return Exception { AbortError, ASCIILiteral("Internal error") };
    }
    return adoptRef(*new MediaEndpointSessionDescription(rtcDescription->type(), WTFMove(configuration), WTFMove(rtcDescription)));
}

RefPtr<RTCSessionDescription> MediaEndpointSessionDescription::toRTCSessionDescription(const SDPProcessor& sdpProcessor) const
{
    String sdpString;
    SDPProcessor::Result result = sdpProcessor.generate(*m_configuration, sdpString);
    if (result != SDPProcessor::Result::Success) {
        LOG_ERROR("SDPProcessor internal error");
        return nullptr;
    }

    // If this object was created from an RTCSessionDescription, toRTCSessionDescription will return
    // that same instance but with an updated sdp. It is used for RTCPeerConnection's description
    // atributes (e.g. localDescription and pendingLocalDescription).
    if (m_rtcDescription) {
        m_rtcDescription->setSdp(WTFMove(sdpString));
        return m_rtcDescription;
    }

    return RTCSessionDescription::create(m_type, WTFMove(sdpString));
}

// FIXME: unify with LibWebRTCMediaEndpoint sessionDescriptionType().
const String& MediaEndpointSessionDescription::typeString() const
{
    switch (m_type) {
    case RTCSdpType::Offer:
        return offerString();
    case RTCSdpType::Pranswer:
        return pranswerString();
    case RTCSdpType::Answer:
        return answerString();
    case RTCSdpType::Rollback:
        return rollbackString();
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

bool MediaEndpointSessionDescription::isLaterThan(MediaEndpointSessionDescription* other) const
{
    return !other || configuration()->sessionVersion() > other->configuration()->sessionVersion();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
