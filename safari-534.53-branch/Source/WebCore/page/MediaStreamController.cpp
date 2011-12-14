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
#include "SecurityOrigin.h"
#include <wtf/Vector.h>

namespace WebCore {

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

void MediaStreamController::streamGenerated(int controllerRequestId, const String& streamLabel)
{
    // Don't assert since the frame controller can have been destroyed while the request reply was coming back.
    if (m_requests.contains(controllerRequestId)) {
        const Request& request = m_requests.get(controllerRequestId);
        registerStream(streamLabel, request.frameController());
        m_requests.remove(controllerRequestId);
        ASSERT(request.frameController());
        request.frameController()->streamGenerated(request.localId(), streamLabel);
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

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
