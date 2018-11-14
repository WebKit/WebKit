/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "HidConnection.h"
#include "MockWebAuthenticationConfiguration.h"
#include <WebCore/FidoHidMessage.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

// The following basically simulates an external HID token that:
//    1. Only supports CTAP2 protocol,
//    2. Doesn't support resident keys,
//    3. Doesn't support user verification.
// There are four stages for each WebAuthN request:
// FSM: Info::Init => Info::Msg => Request::Init => Request::Msg
// According to different combinations of error and stages, error will manifest differently.
class MockHidConnection final : public CanMakeWeakPtr<MockHidConnection>, public HidConnection {
public:
    MockHidConnection(IOHIDDeviceRef, const MockWebAuthenticationConfiguration&);

private:
    void send(Vector<uint8_t>&& data, DataSentCallback&&) final;
    void initialize() final;
    void terminate() final;
    void registerDataReceivedCallbackInternal() final;

    void assembleRequest(Vector<uint8_t>&&);
    void parseRequest();
    void feedReports();
    bool stagesMatch() const;
    void shouldContinueFeedReports();
    void continueFeedReports();

    MockWebAuthenticationConfiguration m_configuration;
    std::optional<fido::FidoHidMessage> m_requestMessage;
    MockWebAuthenticationConfiguration::Hid::Stage m_stage { MockWebAuthenticationConfiguration::Hid::Stage::Info };
    MockWebAuthenticationConfiguration::Hid::SubStage m_subStage { MockWebAuthenticationConfiguration::Hid::SubStage::Init };
    uint32_t m_currentChannel { fido::kHidBroadcastChannel };
    bool m_requireResidentKey { false };
    bool m_requireUserVerification  { false };
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
