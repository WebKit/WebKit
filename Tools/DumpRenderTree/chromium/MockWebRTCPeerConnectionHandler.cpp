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
#if ENABLE(MEDIA_STREAM)

#include "MockWebRTCPeerConnectionHandler.h"

#include "MockConstraints.h"
#include "Task.h"
#include <public/WebMediaConstraints.h>
#include <public/WebMediaStreamComponent.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebRTCPeerConnectionHandlerClient.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCStatsRequest.h>
#include <public/WebRTCStatsResponse.h>
#include <public/WebRTCVoidRequest.h>
#include <public/WebString.h>
#include <public/WebVector.h>
#include <wtf/DateMath.h>

using namespace WebKit;
using namespace WebTestRunner;

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

class StringDataTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    StringDataTask(MockWebRTCPeerConnectionHandler* object, const WebRTCDataChannel& dataChannel, const WebString& data)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_dataChannel(dataChannel)
        , m_data(data)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_dataChannel.dataArrived(m_data);
    }

private:
    WebRTCDataChannel m_dataChannel;
    WebString m_data;
};

class CharPtrDataTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    CharPtrDataTask(MockWebRTCPeerConnectionHandler* object, const WebRTCDataChannel& dataChannel, const char* data, size_t length)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_dataChannel(dataChannel)
        , m_length(length)
    {
        m_data = new char[m_length];
        memcpy(m_data, data, m_length);
    }

    virtual void runIfValid() OVERRIDE
    {
        m_dataChannel.dataArrived(m_data, m_length);
        delete m_data;
    }

private:
    WebRTCDataChannel m_dataChannel;
    char* m_data;
    size_t m_length;
};

class DataChannelReadyStateTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    DataChannelReadyStateTask(MockWebRTCPeerConnectionHandler* object, const WebRTCDataChannel& dataChannel, WebRTCDataChannel::ReadyState state)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_dataChannel(dataChannel)
        , m_state(state)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_dataChannel.readyStateChanged(m_state);
    }

private:
    WebRTCDataChannel m_dataChannel;
    WebRTCDataChannel::ReadyState m_state;
};

class RTCPeerConnectionReadyStateTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCPeerConnectionReadyStateTask(MockWebRTCPeerConnectionHandler* object, WebRTCPeerConnectionHandlerClient* client, WebRTCPeerConnectionHandlerClient::ReadyState state)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_client(client)
        , m_state(state)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_client->didChangeReadyState(m_state);
    }

private:
    WebRTCPeerConnectionHandlerClient* m_client;
    WebRTCPeerConnectionHandlerClient::ReadyState m_state;
};

/////////////////////

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
    : m_client(client)
    , m_stopped(false)
    , m_streamCount(0)
{
}

bool MockWebRTCPeerConnectionHandler::initialize(const WebRTCConfiguration&, const WebMediaConstraints& constraints)
{
    if (MockConstraints::verifyConstraints(constraints)) {
        postTask(new RTCPeerConnectionReadyStateTask(this, m_client, WebRTCPeerConnectionHandlerClient::ReadyStateActive));
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
        postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::createAnswer(const WebRTCSessionDescriptionRequest& request, const WebMediaConstraints&)
{
    if (!m_remoteDescription.isNull()) {
        WebRTCSessionDescription sessionDescription;
        sessionDescription.initialize("answer", "local");
        postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::setLocalDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& localDescription)
{
    if (!localDescription.isNull() && localDescription.sdp() == "local") {
        m_localDescription = localDescription;
        postTask(new RTCVoidRequestTask(this, request, true));
    } else
        postTask(new RTCVoidRequestTask(this, request, false));
}

void MockWebRTCPeerConnectionHandler::setRemoteDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& remoteDescription)
{
    if (!remoteDescription.isNull() && remoteDescription.sdp() == "remote") {
        m_remoteDescription = remoteDescription;
        postTask(new RTCVoidRequestTask(this, request, true));
    } else
        postTask(new RTCVoidRequestTask(this, request, false));
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
    m_client->didChangeICEState(WebRTCPeerConnectionHandlerClient::ICEStateGathering);
    return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(const WebRTCICECandidate& iceCandidate)
{
    m_client->didGenerateICECandidate(iceCandidate);
    return true;
}

bool MockWebRTCPeerConnectionHandler::addStream(const WebMediaStreamDescriptor& stream, const WebMediaConstraints&)
{
    m_streamCount += 1;
    m_client->didAddRemoteStream(stream);
    m_client->negotiationNeeded();
    return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(const WebMediaStreamDescriptor& stream)
{
    m_streamCount -= 1;
    m_client->didRemoveRemoteStream(stream);
    m_client->negotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::getStats(const WebRTCStatsRequest& request)
{
    WebRTCStatsResponse response = request.createResponse();
    double currentDate = WTF::jsCurrentTime();
    if (request.hasSelector()) {
        WebMediaStreamDescriptor stream = request.stream();
        WebMediaStreamComponent component = request.component();
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
        }
    }
    postTask(new RTCStatsRequestSucceededTask(this, request, response));
}

void MockWebRTCPeerConnectionHandler::stop()
{
    m_stopped = true;
}

bool MockWebRTCPeerConnectionHandler::openDataChannel(const WebRTCDataChannel& dataChannel)
{
    if (m_stopped)
        return false;

    postTask(new DataChannelReadyStateTask(this, dataChannel, WebRTCDataChannel::ReadyStateOpen));
    return true;
}

void MockWebRTCPeerConnectionHandler::closeDataChannel(const WebRTCDataChannel& dataChannel)
{
    postTask(new DataChannelReadyStateTask(this, dataChannel, WebRTCDataChannel::ReadyStateClosed));
}

bool MockWebRTCPeerConnectionHandler::sendStringData(const WebRTCDataChannel& dataChannel, const WebString& data)
{
    if (m_stopped)
        return false;

    postTask(new StringDataTask(this, dataChannel, data));
    return true;
}

bool MockWebRTCPeerConnectionHandler::sendRawData(const WebRTCDataChannel& dataChannel, const char* data, size_t length)
{
    if (m_stopped)
        return false;

    postTask(new CharPtrDataTask(this, dataChannel, data, length));
    return true;
}

#endif // ENABLE(MEDIA_STREAM)
