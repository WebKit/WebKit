/*
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

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RTCDataChannelHandler.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/data_channel_interface.h>

ALLOW_UNUSED_PARAMETERS_END

namespace webrtc {
struct DataChannelInit;
}

namespace WebCore {

class Document;
class RTCDataChannelEvent;
class RTCDataChannelHandlerClient;
struct RTCDataChannelInit;
class ScriptExecutionContext;

class LibWebRTCDataChannelHandler final : public RTCDataChannelHandler, private webrtc::DataChannelObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LibWebRTCDataChannelHandler(rtc::scoped_refptr<webrtc::DataChannelInterface>&& channel) : m_channel(WTFMove(channel)) { ASSERT(m_channel); }
    ~LibWebRTCDataChannelHandler();

    static webrtc::DataChannelInit fromRTCDataChannelInit(const RTCDataChannelInit&);
    static Ref<RTCDataChannelEvent> channelEvent(Document&, rtc::scoped_refptr<webrtc::DataChannelInterface>&&);

private:
    // RTCDataChannelHandler API
    void setClient(RTCDataChannelHandlerClient&) final;
    void checkState();
    bool sendStringData(const String&) final;
    bool sendRawData(const char*, size_t) final;
    void close() final;

    // webrtc::DataChannelObserver API
    void OnStateChange();
    void OnMessage(const webrtc::DataBuffer&);
    void OnBufferedAmountChange(uint64_t);

    rtc::scoped_refptr<webrtc::DataChannelInterface> m_channel;
    RTCDataChannelHandlerClient* m_client { nullptr };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
