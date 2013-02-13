/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE_WEBRTC
#include "MockWebRTCDTMFSenderHandler.h"

#include "WebTestDelegate.h"
#include <assert.h>
#include <public/WebMediaStreamSource.h>
#include <public/WebRTCDTMFSenderHandlerClient.h>

using namespace WebKit;

namespace WebTestRunner {

class DTMFSenderToneTask : public WebMethodTask<MockWebRTCDTMFSenderHandler> {
public:
    DTMFSenderToneTask(MockWebRTCDTMFSenderHandler* object, WebRTCDTMFSenderHandlerClient* client)
        : WebMethodTask<MockWebRTCDTMFSenderHandler>(object)
        , m_client(client)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        WebString tones = m_object->currentToneBuffer();
        m_object->clearToneBuffer();
        m_client->didPlayTone(tones);
    }

private:
    WebRTCDTMFSenderHandlerClient* m_client;
};

/////////////////////

MockWebRTCDTMFSenderHandler::MockWebRTCDTMFSenderHandler(const WebMediaStreamTrack& track, WebTestDelegate* delegate)
    : m_client(0)
    , m_track(track)
    , m_delegate(delegate)
{
}

void MockWebRTCDTMFSenderHandler::setClient(WebRTCDTMFSenderHandlerClient* client)
{
    m_client = client;
}

WebString MockWebRTCDTMFSenderHandler::currentToneBuffer()
{
    return m_toneBuffer;
}

bool MockWebRTCDTMFSenderHandler::canInsertDTMF()
{
    assert(m_client && !m_track.isNull());
    return m_track.source().type() == WebMediaStreamSource::TypeAudio && m_track.isEnabled() && m_track.source().readyState() == WebMediaStreamSource::ReadyStateLive;
}

bool MockWebRTCDTMFSenderHandler::insertDTMF(const WebString& tones, long duration, long interToneGap)
{
    assert(m_client);
    if (!canInsertDTMF())
        return false;

    m_toneBuffer = tones;
    m_delegate->postTask(new DTMFSenderToneTask(this, m_client));
    m_delegate->postTask(new DTMFSenderToneTask(this, m_client));
    return true;
}

}

#endif // ENABLE_WEBRTC
