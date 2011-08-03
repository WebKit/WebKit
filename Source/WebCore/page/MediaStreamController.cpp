/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "MediaStreamController.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamFrameController.h"
#include "MediaStreamTrackList.h"
#include "SecurityOrigin.h"
#include <wtf/Vector.h>

namespace WebCore {

// FIXME: This is used for holding list of PeerConnection as well,
// so rename class is in order.
class MediaStreamController::Request {
public:
    Request()
        : m_localId(0)
        , m_frameController(0) { }

    Request(int localId, MediaStreamFrameController* frameController)
        : m_localId(localId)
        , m_frameController(frameController) { }

    int localId() const { return m_localId; }
    MediaStreamFrameController* frameController() const { return m_frameController; }

private:
    int m_localId;
    MediaStreamFrameController* m_frameController;
};

MediaStreamController::MediaStreamController(MediaStreamClient* client)
    : m_client(client)
    , m_nextGlobalRequestId(1)
    , m_nextGlobalPeerConnectionId(1)
{
}

MediaStreamController::~MediaStreamController()
{
    if (m_client)
        m_client->mediaStreamDestroyed();
}

bool MediaStreamController::isClientAvailable() const
{
    return m_client;
}

void MediaStreamController::unregisterFrameController(MediaStreamFrameController* frameController)
{
    ASSERT(frameController);

    // Done in two steps to avoid problems about iterators being invalidated while removing.
    Vector<int> frameRequests;
    for (RequestMap::iterator it = m_requests.begin(); it != m_requests.end(); ++it)
        if (it->second.frameController() == frameController)
            frameRequests.append(it->first);

    for (Vector<int>::iterator it = frameRequests.begin(); it != frameRequests.end(); ++it)
        m_requests.remove(*it);

    Vector<String> frameStreams;
    for (StreamMap::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
        if (it->second == frameController)
            frameStreams.append(it->first);

    for (Vector<String>::iterator it = frameStreams.begin(); it != frameStreams.end(); ++it)
        m_streams.remove(*it);

    // FIXME: Add unregister functionality for peer connection objects.
}

int MediaStreamController::registerRequest(int localId, MediaStreamFrameController* frameController)
{
    ASSERT(localId > 0);
    ASSERT(frameController);
    m_requests.add(m_nextGlobalRequestId, Request(localId, frameController));
    return m_nextGlobalRequestId++;
}

void MediaStreamController::registerStream(const String& streamLabel, MediaStreamFrameController* frameController)
{
    ASSERT(frameController);
    ASSERT(!m_streams.contains(streamLabel));
    m_streams.add(streamLabel, frameController);
}

void MediaStreamController::generateStream(MediaStreamFrameController* frameController, int localId, GenerateStreamOptionFlags flags, PassRefPtr<SecurityOrigin> securityOrigin)
{
    ASSERT(m_client);
    int controllerRequestId = registerRequest(localId, frameController);
    m_client->generateStream(controllerRequestId, flags, securityOrigin);
}

void MediaStreamController::stopGeneratedStream(const String& streamLabel)
{
    m_client->stopGeneratedStream(streamLabel);
}

void MediaStreamController::setMediaStreamTrackEnabled(const String& trackId, bool enabled)
{
    m_client->setMediaStreamTrackEnabled(trackId, enabled);
}

void MediaStreamController::streamGenerated(int controllerRequestId, const String& streamLabel, PassRefPtr<MediaStreamTrackList> tracks)
{
    // Don't assert since the frame controller can have been destroyed while the request reply was coming back.
    if (m_requests.contains(controllerRequestId)) {
        const Request& request = m_requests.get(controllerRequestId);
        registerStream(streamLabel, request.frameController());
        m_requests.remove(controllerRequestId);
        ASSERT(request.frameController());
        request.frameController()->streamGenerated(request.localId(), streamLabel, tracks);
    }
}

void MediaStreamController::streamGenerationFailed(int controllerRequestId, NavigatorUserMediaError::ErrorCode code)
{
    // Don't assert since the frame controller can have been destroyed while the request reply was coming back.
    if (m_requests.contains(controllerRequestId)) {
        const Request& request = m_requests.get(controllerRequestId);
        m_requests.remove(controllerRequestId);
        request.frameController()->streamGenerationFailed(request.localId(), code);
    }
}

void MediaStreamController::streamFailed(const String& streamLabel)
{
    // Don't assert since the frame controller can have been destroyed by the time this is called.
    if (m_streams.contains(streamLabel))
        m_streams.get(streamLabel)->streamFailed(streamLabel);
}

void MediaStreamController::onSignalingMessage(int controllerPeerConnectionId, const String& message)
{
    // Don't assert since the frame controller can have been destroyed.
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onSignalingMessage(peerConnection.localId(), message);
    }
}

bool MediaStreamController::frameToPagePeerConnectionId(MediaStreamFrameController* frameController, int framePeerConnectionId, int& pagePeerConnectionId)
{
    pagePeerConnectionId = -1;
    // FIXME: Is there a better way to find the global ID in the map?
    for (RequestMap::iterator it = m_peerConnections.begin(); it != m_peerConnections.end(); ++it) {
        if (it->second.frameController() == frameController && it->second.localId() == framePeerConnectionId) {
            pagePeerConnectionId = it->first;
            return true;
        }
    }
    return false;
}

void MediaStreamController::processSignalingMessage(MediaStreamFrameController* frameController, int framePeerConnectionId, const String& message)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->processSignalingMessage(pagePeerConnectionId, message);
}

void MediaStreamController::message(MediaStreamFrameController* frameController, int framePeerConnectionId, const String& message)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->message(pagePeerConnectionId, message);
}

void MediaStreamController::onMessage(int controllerPeerConnectionId, const String& message)
{
    // Don't assert since the frame controller can have been destroyed
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onMessage(peerConnection.localId(), message);
    }
}

void MediaStreamController::addStream(MediaStreamFrameController* frameController, int framePeerConnectionId, const String& streamLabel)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->addStream(pagePeerConnectionId, streamLabel);
}

void MediaStreamController::onAddStream(int controllerPeerConnectionId, const String& streamLabel, PassRefPtr<MediaStreamTrackList> tracks)
{
    // Don't assert since the frame controller can have been destroyed
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onAddStream(peerConnection.localId(), streamLabel, tracks);
    }
}

void MediaStreamController::removeStream(MediaStreamFrameController* frameController, int framePeerConnectionId, const String& streamLabel)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->removeStream(pagePeerConnectionId, streamLabel);
}

void MediaStreamController::onRemoveStream(int controllerPeerConnectionId, const String& streamLabel)
{
    // Don't assert since the frame controller can have been destroyed
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        // TODO: Better name of peerConnection variable?
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onRemoveStream(peerConnection.localId(), streamLabel);
    }
}

void MediaStreamController::newPeerConnection(MediaStreamFrameController* frameController, int framePeerConnectionId, const String& configuration)
{
    int pagePeerConnectionId = m_nextGlobalPeerConnectionId++;
    m_peerConnections.add(pagePeerConnectionId, Request(framePeerConnectionId, frameController));

    m_client->newPeerConnection(pagePeerConnectionId, configuration);
}

void MediaStreamController::closePeerConnection(MediaStreamFrameController* frameController, int framePeerConnectionId)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->closePeerConnection(pagePeerConnectionId);
}

void MediaStreamController::startNegotiation(MediaStreamFrameController* frameController, int framePeerConnectionId)
{
    int pagePeerConnectionId;
    if (frameToPagePeerConnectionId(frameController, framePeerConnectionId, pagePeerConnectionId))
        m_client->startNegotiation(pagePeerConnectionId);
}

void MediaStreamController::onNegotiationStarted(int controllerPeerConnectionId)
{
    // Don't assert since the frame controller can have been destroyed
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onNegotiationStarted(peerConnection.localId());
    }
}

void MediaStreamController::onNegotiationDone(int controllerPeerConnectionId)
{
    // Don't assert since the frame controller can have been destroyed
    if (m_peerConnections.contains(controllerPeerConnectionId)) {
        const Request& peerConnection = m_peerConnections.get(controllerPeerConnectionId);
        ASSERT(peerConnection.frameController());
        peerConnection.frameController()->onNegotiationDone(peerConnection.localId());
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
