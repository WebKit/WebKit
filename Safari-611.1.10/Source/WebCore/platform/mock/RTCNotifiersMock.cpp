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

#include "config.h"

#if ENABLE(WEB_RTC)

#include "RTCNotifiersMock.h"

#include "RTCDataChannelHandlerMock.h"
#include "RTCSessionDescriptionDescriptor.h"
#include "RTCSessionDescriptionRequest.h"
#include "RTCVoidRequest.h"

namespace WebCore {

SessionRequestNotifier::SessionRequestNotifier(RefPtr<RTCSessionDescriptionRequest>&& request, RefPtr<RTCSessionDescriptionDescriptor>&& descriptor, const String& errorName)
    : m_request(WTFMove(request))
    , m_descriptor(WTFMove(descriptor))
    , m_errorName(errorName)
{
}

void SessionRequestNotifier::fire()
{
    if (m_descriptor)
        m_request->requestSucceeded(*m_descriptor);
    else
        m_request->requestFailed(m_errorName);
}

VoidRequestNotifier::VoidRequestNotifier(RefPtr<RTCVoidRequest>&& request, bool success, const String& errorName)
    : m_request(WTFMove(request))
    , m_success(success)
    , m_errorName(errorName)
{
}

void VoidRequestNotifier::fire()
{
    if (m_success)
        m_request->requestSucceeded();
    else
        m_request->requestFailed(m_errorName);
}

IceConnectionNotifier::IceConnectionNotifier(RTCPeerConnectionHandlerClient* client, RTCIceConnectionState connectionState, RTCIceGatheringState gatheringState)
    : m_client(client)
    , m_connectionState(connectionState)
    , m_gatheringState(gatheringState)
{
}

void IceConnectionNotifier::fire()
{
    m_client->didChangeIceGatheringState(m_gatheringState);
    m_client->didChangeIceConnectionState(m_connectionState);
}

SignalingStateNotifier::SignalingStateNotifier(RTCPeerConnectionHandlerClient* client, RTCSignalingState signalingState)
    : m_client(client)
    , m_signalingState(signalingState)
{
}

void SignalingStateNotifier::fire()
{
    m_client->didChangeSignalingState(m_signalingState);
}

RemoteDataChannelNotifier::RemoteDataChannelNotifier(RTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
}

void RemoteDataChannelNotifier::fire()
{
    m_client->didAddRemoteDataChannel(makeUnique<RTCDataChannelHandlerMock>("RTCDataChannelHandlerMock", RTCDataChannelInit()));
}

DataChannelStateNotifier::DataChannelStateNotifier(RTCDataChannelHandlerClient* client, RTCDataChannelState state)
    : m_client(client)
    , m_state(state)
{
}

void DataChannelStateNotifier::fire()
{
    m_client->didChangeReadyState(m_state);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
