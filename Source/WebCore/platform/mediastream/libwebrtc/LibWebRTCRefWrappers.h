/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include <WebCore/LibWebRTCMacros.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
#include <webrtc/api/data_channel_interface.h>
#include <webrtc/api/dtls_transport_interface.h>
#include <webrtc/api/dtmf_sender_interface.h>
#include <webrtc/api/frame_transformer_interface.h>
#include <webrtc/api/ice_transport_interface.h>
#include <webrtc/api/media_stream_interface.h>
#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/api/rtp_receiver_interface.h>
#include <webrtc/api/rtp_sender_interface.h>
#include <webrtc/api/rtp_transceiver_interface.h>
#include <webrtc/api/sctp_transport_interface.h>
IGNORE_CLANG_WARNINGS_END
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WTF {

template<typename T> struct RTCDefaultRefDerefTraits {
    static ALWAYS_INLINE T* refIfNotNull(T* buffer)
    {
        if (buffer) [[likely]]
            buffer->AddRef();
        return buffer;
    }
    static ALWAYS_INLINE T& ref(T& ref)
    {
        ref.AddRef();
        return ref;
    }
    static ALWAYS_INLINE void derefIfNotNull(T* buffer)
    {
        if (buffer) [[likely]]
            buffer->Release();
    }
};

#define WEBRTC_REFTRAITS(T) template<> struct DefaultRefDerefTraits<T> : RTCDefaultRefDerefTraits<T> { };

WEBRTC_REFTRAITS(webrtc::AudioTrackInterface);
WEBRTC_REFTRAITS(webrtc::DataChannelInterface);
WEBRTC_REFTRAITS(webrtc::DtlsTransportInterface);
WEBRTC_REFTRAITS(webrtc::DtmfSenderInterface);
WEBRTC_REFTRAITS(webrtc::IceTransportInterface);
WEBRTC_REFTRAITS(webrtc::MediaStreamInterface);
WEBRTC_REFTRAITS(webrtc::MediaStreamTrackInterface);
WEBRTC_REFTRAITS(webrtc::PeerConnectionFactoryInterface);
WEBRTC_REFTRAITS(webrtc::PeerConnectionInterface);
WEBRTC_REFTRAITS(webrtc::RtpReceiverInterface);
WEBRTC_REFTRAITS(webrtc::RtpSenderInterface);
WEBRTC_REFTRAITS(webrtc::RtpTransceiverInterface);
WEBRTC_REFTRAITS(webrtc::TransformedFrameCallback);
WEBRTC_REFTRAITS(webrtc::SctpTransportInterface);
WEBRTC_REFTRAITS(webrtc::VideoFrameBuffer);
WEBRTC_REFTRAITS(webrtc::VideoTrackInterface);

template<typename T>
Ref<T> toRef(webrtc::scoped_refptr<T>&& buffer)
{
    ASSERT(buffer);
    return adoptRef(*buffer.release());
}

template<typename T>
RefPtr<T> toRefPtr(webrtc::scoped_refptr<T>&& buffer)
{
    return adoptRef(buffer.release());
}
}

using WTF::toRef;
using WTF::toRefPtr;

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
