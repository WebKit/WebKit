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

#include "config.h"

#if ENABLE_WEBRTC
#include "MockWebRTCPeerConnectionHandler.h"

#include "MockConstraints.h"
#include "MockWebRTCDTMFSenderHandler.h"
#include "MockWebRTCDataChannelHandler.h"
#include "TestInterfaces.h"
#include "WebTestDelegate.h"
#include <public/WebMediaConstraints.h>
#include <public/WebMediaStream.h>
#include <public/WebMediaStreamTrack.h>
#include <public/WebRTCPeerConnectionHandlerClient.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCStatsRequest.h>
#include <public/WebRTCStatsResponse.h>
#include <public/WebRTCVoidRequest.h>
#include <public/WebString.h>
#include <public/WebVector.h>

using namespace WebKit;

namespace WebTestRunner {

class RTCSessionDescriptionRequestSuccededTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestSuccededTask(MockWebRTCPeerConnectionHandler* object, const WebRTCSessionDescriptionRequest& request, const WebRTCSessionDescription& result)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebRTCSessionDescriptionRequest m_request;
    WebRTCSessionDescription m_result;
};

class RTCSessionDescriptionRequestFailedTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestFailedTask(MockWebRTCPeerConnectionHandler* object, const WebRTCSessionDescriptionRequest& request)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailed("TEST_ERROR");
    }

private:
    WebRTCSessionDescriptionRequest m_request;
};

class RTCStatsRequestSucceededTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCStatsRequestSucceededTask(MockWebRTCPeerConnectionHandler* object, const WebKit::WebRTCStatsRequest& request, const WebKit::WebRTCStatsResponse& response)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_response(response)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_response);
    }

private:
    WebKit::WebRTCStatsRequest m_request;
    WebKit::WebRTCStatsResponse m_response;
};

class RTCVoidRequestTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCVoidRequestTask(MockWebRTCPeerConnectionHandler* object, const WebRTCVoidRequest& request, bool succeeded)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_succeeded(succeeded)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        if (m_succeeded)
            m_request.requestSucceeded();
        else
            m_request.requestFailed("TEST_ERROR");
    }

private:
    WebRTCVoidRequest m_request;
    bool m_succeeded;
};

class RTCPeerConnectionStateTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCPeerConnectionStateTask(MockWebRTCPeerConnectionHandler* object, WebRTCPeerConnectionHandlerClient* client, WebRTCPeerConnectionHandlerClient::ICEConnectionState connectionState, WebRTCPeerConnectionHandlerClient::ICEGatheringState gatheringState)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_client(client)
        , m_connectionState(connectionState)
        , m_gatheringState(gatheringState)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_client->didChangeICEGatheringState(m_gatheringState);
        m_client->didChangeICEConnectionState(m_connectionState);
    }

private:
    WebRTCPeerConnectionHandlerClient* m_client;
    WebRTCPeerConnectionHandlerClient::ICEConnectionState m_connectionState;
    WebRTCPeerConnectionHandlerClient::ICEGatheringState m_gatheringState;
};

class RemoteDataChannelTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RemoteDataChannelTask(MockWebRTCPeerConnectionHandler* object, WebRTCPeerConnectionHandlerClient* client, WebTestDelegate* delegate)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_client(client)
        , m_delegate(delegate)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        WebRTCDataChannelHandler* remoteDataChannel = new MockWebRTCDataChannelHandler("MockRemoteDataChannel", true, m_delegate);
        m_client->didAddRemoteDataChannel(remoteDataChannel);
    }

private:
    WebRTCPeerConnectionHandlerClient* m_client;
    WebTestDelegate* m_delegate;
};

/////////////////////

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client, TestInterfaces* interfaces)
    : m_client(client)
    , m_stopped(false)
    , m_streamCount(0)
    , m_interfaces(interfaces)
{
}

bool MockWebRTCPeerConnectionHandler::initialize(const WebRTCConfiguration&, const WebMediaConstraints& constraints)
{
    if (MockConstraints::verifyConstraints(constraints)) {
        m_interfaces->delegate()->postTask(new RTCPeerConnectionStateTask(this, m_client, WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted, WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));
        return true;
    }

    return false;
}

void MockWebRTCPeerConnectionHandler::createOffer(const WebRTCSessionDescriptionRequest& request, const WebMediaConstraints& constraints)
{
    WebString shouldSucceed;
    if (constraints.getMandatoryConstraintValue("succeed", shouldSucceed) && shouldSucceed == "true") {
        WebRTCSessionDescription sessionDescription;
        sessionDescription.initialize("offer", "local");
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::createAnswer(const WebRTCSessionDescriptionRequest& request, const WebMediaConstraints&)
{
    if (!m_remoteDescription.isNull()) {
        WebRTCSessionDescription sessionDescription;
        sessionDescription.initialize("answer", "local");
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::setLocalDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& localDescription)
{
    if (!localDescription.isNull() && localDescription.sdp() == "local") {
        m_localDescription = localDescription;
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, true));
    } else
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, false));
}

void MockWebRTCPeerConnectionHandler::setRemoteDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& remoteDescription)
{
    if (!remoteDescription.isNull() && remoteDescription.sdp() == "remote") {
        m_remoteDescription = remoteDescription;
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, true));
    } else
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, false));
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::localDescription()
{
    return m_localDescription;
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::remoteDescription()
{
    return m_remoteDescription;
}

bool MockWebRTCPeerConnectionHandler::updateICE(const WebRTCConfiguration&, const WebMediaConstraints&)
{
    return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(const WebRTCICECandidate& iceCandidate)
{
    m_client->didGenerateICECandidate(iceCandidate);
    return true;
}

bool MockWebRTCPeerConnectionHandler::addStream(const WebMediaStream& stream, const WebMediaConstraints&)
{
    ++m_streamCount;
    m_client->negotiationNeeded();
    return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(const WebMediaStream& stream)
{
    --m_streamCount;
    m_client->negotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::getStats(const WebRTCStatsRequest& request)
{
    WebRTCStatsResponse response = request.createResponse();
    double currentDate = m_interfaces->delegate()->getCurrentTimeInMillisecond();
    if (request.hasSelector()) {
        WebMediaStream stream = request.stream();
        // FIXME: There is no check that the fetched values are valid.
        size_t reportIndex = response.addReport();
        response.addElement(reportIndex, true, currentDate);
        response.addStatistic(reportIndex, true, "type", "video");
    } else {
        for (int i = 0; i < m_streamCount; ++i) {
            size_t reportIndex = response.addReport();
            response.addElement(reportIndex, true, currentDate);
            response.addStatistic(reportIndex, true, "type", "audio");
            reportIndex = response.addReport();
            response.addElement(reportIndex, true, currentDate);
            response.addStatistic(reportIndex, true, "type", "video");
            // We add an empty remote report element.
            response.addElement(reportIndex, false, currentDate);
        }
    }
    m_interfaces->delegate()->postTask(new RTCStatsRequestSucceededTask(this, request, response));
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::createDataChannel(const WebString& label, bool reliable)
{
    m_interfaces->delegate()->postTask(new RemoteDataChannelTask(this, m_client, m_interfaces->delegate()));

    return new MockWebRTCDataChannelHandler(label, reliable, m_interfaces->delegate());
}

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::createDTMFSender(const WebMediaStreamTrack& track)
{
    return new MockWebRTCDTMFSenderHandler(track, m_interfaces->delegate());
}

void MockWebRTCPeerConnectionHandler::stop()
{
    m_stopped = true;
}

}

#endif // ENABLE_WEBRTC
