/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ExceptionCode.h"
#include "LibWebRTCMacros.h"
#include "LibWebRTCUtils.h"

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#include <webrtc/api/peer_connection_interface.h>

ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

template<typename Endpoint>
class CreateSessionDescriptionObserver final : public webrtc::CreateSessionDescriptionObserver {
public:
    explicit CreateSessionDescriptionObserver(Endpoint &endpoint)
        : m_endpoint(endpoint)
    {
    }

    void OnSuccess(webrtc::SessionDescriptionInterface* sessionDescription) final { m_endpoint.createSessionDescriptionSucceeded(std::unique_ptr<webrtc::SessionDescriptionInterface>(sessionDescription)); }
    void OnFailure(webrtc::RTCError error) final { m_endpoint.createSessionDescriptionFailed(toExceptionCode(error.type()), error.message()); }

    void AddRef() const { m_endpoint.AddRef(); }
    rtc::RefCountReleaseStatus Release() const { return m_endpoint.Release(); }

private:
    Endpoint& m_endpoint;
};

template<typename Endpoint>
class SetLocalSessionDescriptionObserver final : public webrtc::SetSessionDescriptionObserver {
public:
    explicit SetLocalSessionDescriptionObserver(Endpoint &endpoint)
        : m_endpoint(endpoint)
    {
    }

    void OnSuccess() final { m_endpoint.setLocalSessionDescriptionSucceeded(); }
    void OnFailure(webrtc::RTCError error) final { m_endpoint.setLocalSessionDescriptionFailed(toExceptionCode(error.type()), error.message()); }

    void AddRef() const { m_endpoint.AddRef(); }
    rtc::RefCountReleaseStatus Release() const { return m_endpoint.Release(); }

private:
    Endpoint& m_endpoint;
};

template<typename Endpoint>
class SetRemoteSessionDescriptionObserver final : public webrtc::SetSessionDescriptionObserver {
public:
    explicit SetRemoteSessionDescriptionObserver(Endpoint &endpoint)
        : m_endpoint(endpoint)
    {
    }

    void OnSuccess() final { m_endpoint.setRemoteSessionDescriptionSucceeded(); }
    void OnFailure(webrtc::RTCError error) final { m_endpoint.setRemoteSessionDescriptionFailed(toExceptionCode(error.type()), error.message()); }

    void AddRef() const { m_endpoint.AddRef(); }
    rtc::RefCountReleaseStatus Release() const { return m_endpoint.Release(); }

private:
    Endpoint& m_endpoint;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
