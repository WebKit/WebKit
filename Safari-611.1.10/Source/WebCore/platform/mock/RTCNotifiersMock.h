/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCDataChannelHandlerClient.h"
#include "RTCPeerConnectionHandlerClient.h"
#include "TimerEventBasedMock.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCSessionDescriptionDescriptor;
class RTCSessionDescriptionRequest;
class RTCVoidRequest;

class SessionRequestNotifier : public MockNotifier {
public:
    SessionRequestNotifier(RefPtr<RTCSessionDescriptionRequest>&&, RefPtr<RTCSessionDescriptionDescriptor>&&, const String& = emptyString());

    void fire() override;

private:
    RefPtr<RTCSessionDescriptionRequest> m_request;
    RefPtr<RTCSessionDescriptionDescriptor> m_descriptor;
    String m_errorName;
};

class VoidRequestNotifier : public MockNotifier {
public:
    VoidRequestNotifier(RefPtr<RTCVoidRequest>&&, bool, const String& = emptyString());

    void fire() override;

private:
    RefPtr<RTCVoidRequest> m_request;
    bool m_success;
    String m_errorName;
};

class IceConnectionNotifier : public MockNotifier {
public:
    IceConnectionNotifier(RTCPeerConnectionHandlerClient*, RTCIceConnectionState, RTCIceGatheringState);

    void fire() override;

private:
    RTCPeerConnectionHandlerClient* m_client;
    RTCIceConnectionState m_connectionState;
    RTCIceGatheringState m_gatheringState;
};

class SignalingStateNotifier : public MockNotifier {
public:
    SignalingStateNotifier(RTCPeerConnectionHandlerClient*, RTCSignalingState);

    void fire() override;

private:
    RTCPeerConnectionHandlerClient* m_client;
    RTCSignalingState m_signalingState;
};

class RemoteDataChannelNotifier : public MockNotifier {
public:
    RemoteDataChannelNotifier(RTCPeerConnectionHandlerClient*);

    void fire() override;

private:
    RTCPeerConnectionHandlerClient* m_client;
};

class DataChannelStateNotifier : public MockNotifier {
public:
    DataChannelStateNotifier(RTCDataChannelHandlerClient*, RTCDataChannelState);

    void fire() override;

private:
    RTCDataChannelHandlerClient* m_client;
    RTCDataChannelState m_state;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
