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

#include <public/WebMediaConstraints.h>
#include <public/WebRTCPeerConnectionHandlerClient.h>
#include <public/WebRTCSessionDescription.h>
#include <public/WebRTCSessionDescriptionRequest.h>
#include <public/WebRTCVoidRequest.h>
#include <public/WebString.h>
#include <public/WebVector.h>

using namespace WebKit;

class RTCSessionDescriptionRequestSuccededTask : public MethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestSuccededTask(MockWebRTCPeerConnectionHandler* object, const WebKit::WebRTCSessionDescriptionRequest& request, const WebKit::WebRTCSessionDescription& result)
        : MethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebKit::WebRTCSessionDescriptionRequest m_request;
    WebKit::WebRTCSessionDescription m_result;
};

class RTCSessionDescriptionRequestFailedTask : public MethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestFailedTask(MockWebRTCPeerConnectionHandler* object, const WebKit::WebRTCSessionDescriptionRequest& request)
        : MethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailed("TEST_ERROR");
    }

private:
    WebKit::WebRTCSessionDescriptionRequest m_request;
};

class RTCVoidRequestTask : public MethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCVoidRequestTask(MockWebRTCPeerConnectionHandler* object, const WebKit::WebRTCVoidRequest& request, bool succeeded)
        : MethodTask<MockWebRTCPeerConnectionHandler>(object)
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
    WebKit::WebRTCVoidRequest m_request;
    bool m_succeeded;
};

/////////////////////

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
    : m_client(client)
{
}

static bool isSupportedConstraint(const WebString& constraint)
{
    return constraint == "valid_and_supported_1" || constraint == "valid_and_supported_2";
}

static bool isValidConstraint(const WebString& constraint)
{
    return isSupportedConstraint(constraint) || constraint == "valid_but_unsupported_1" || constraint == "valid_but_unsupported_2";
}

bool MockWebRTCPeerConnectionHandler::initialize(const WebRTCConfiguration&, const WebMediaConstraints& constraints)
{
    WebVector<WebString> mandatoryConstraintNames;
    constraints.getMandatoryConstraintNames(mandatoryConstraintNames);
    if (mandatoryConstraintNames.size()) {
        for (size_t i = 0; i < mandatoryConstraintNames.size(); ++i) {
            if (!isSupportedConstraint(mandatoryConstraintNames[i]))
                return false;
            WebString value;
            constraints.getMandatoryConstraintValue(mandatoryConstraintNames[i], value);
            if (value != "1")
                return false;
        }
    }

    WebVector<WebString> optionalConstraintNames;
    constraints.getOptionalConstraintNames(optionalConstraintNames);
    if (optionalConstraintNames.size()) {
        for (size_t i = 0; i < optionalConstraintNames.size(); ++i) {
            if (!isValidConstraint(optionalConstraintNames[i]))
                return false;
            WebString value;
            constraints.getOptionalConstraintValue(optionalConstraintNames[i], value);
            if (value != "0")
                return false;
        }
    }

    return true;
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
    m_client->didAddRemoteStream(stream);
    return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(const WebMediaStreamDescriptor& stream)
{
    m_client->didRemoveRemoteStream(stream);
}

void MockWebRTCPeerConnectionHandler::stop()
{
}

#endif // ENABLE(MEDIA_STREAM)
