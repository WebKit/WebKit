/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebSpeechRecognitionConnection.h"

#include "SpeechRecognitionServerMessages.h"
#include "WebFrame.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebSpeechRecognitionConnectionMessages.h"
#include <WebCore/SpeechRecognitionConnectionClient.h>
#include <WebCore/SpeechRecognitionRequestInfo.h>
#include <WebCore/SpeechRecognitionUpdate.h>

namespace WebKit {

Ref<WebSpeechRecognitionConnection> WebSpeechRecognitionConnection::create(SpeechRecognitionConnectionIdentifier identifier)
{
    return adoptRef(*new WebSpeechRecognitionConnection(identifier));
}

WebSpeechRecognitionConnection::WebSpeechRecognitionConnection(SpeechRecognitionConnectionIdentifier identifier)
    : m_identifier(identifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebSpeechRecognitionConnection::messageReceiverName(), m_identifier, *this);
    send(Messages::WebProcessProxy::CreateSpeechRecognitionServer(m_identifier), 0);

#if ENABLE(MEDIA_STREAM)
    WebProcess::singleton().ensureSpeechRecognitionRealtimeMediaSourceManager();
#endif
}

WebSpeechRecognitionConnection::~WebSpeechRecognitionConnection()
{
    send(Messages::WebProcessProxy::DestroySpeechRecognitionServer(m_identifier), 0);
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebSpeechRecognitionConnection::registerClient(WebCore::SpeechRecognitionConnectionClient& client)
{
    m_clientMap.add(client.identifier(), makeWeakPtr(client));
}

void WebSpeechRecognitionConnection::unregisterClient(WebCore::SpeechRecognitionConnectionClient& client)
{
    m_clientMap.remove(client.identifier());
}

void WebSpeechRecognitionConnection::start(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, const String& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&& clientOrigin, WebCore::FrameIdentifier frameIdentifier)
{
    send(Messages::SpeechRecognitionServer::Start(clientIdentifier, lang, continuous, interimResults, maxAlternatives, WTFMove(clientOrigin), frameIdentifier));
}

void WebSpeechRecognitionConnection::stop(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    send(Messages::SpeechRecognitionServer::Stop(clientIdentifier));
}

void WebSpeechRecognitionConnection::abort(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    send(Messages::SpeechRecognitionServer::Abort(clientIdentifier));
}

void WebSpeechRecognitionConnection::invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    send(Messages::SpeechRecognitionServer::Invalidate(clientIdentifier));
}

void WebSpeechRecognitionConnection::didReceiveUpdate(WebCore::SpeechRecognitionUpdate&& update)
{
    auto clientIdentifier = update.clientIdentifier();
    if (!m_clientMap.contains(clientIdentifier))
        return;

    auto client = m_clientMap.get(clientIdentifier);
    if (!client) {
        m_clientMap.remove(clientIdentifier);
        // Inform server that client does not exist any more.
        invalidate(clientIdentifier);
        return;
    }

    switch (update.type()) {
    case WebCore::SpeechRecognitionUpdateType::Start:
        client->didStart();
        break;
    case WebCore::SpeechRecognitionUpdateType::AudioStart:
        client->didStartCapturingAudio();
        break;
    case WebCore::SpeechRecognitionUpdateType::SoundStart:
        client->didStartCapturingSound();
        break;
    case WebCore::SpeechRecognitionUpdateType::SpeechStart:
        client->didStartCapturingSpeech();
        break;
    case WebCore::SpeechRecognitionUpdateType::SpeechEnd:
        client->didStopCapturingSpeech();
        break;
    case WebCore::SpeechRecognitionUpdateType::SoundEnd:
        client->didStopCapturingSound();
        break;
    case WebCore::SpeechRecognitionUpdateType::AudioEnd:
        client->didStopCapturingAudio();
        break;
    case WebCore::SpeechRecognitionUpdateType::NoMatch:
        client->didFindNoMatch();
        break;
    case WebCore::SpeechRecognitionUpdateType::Result:
        client->didReceiveResult(update.result());
        break;
    case WebCore::SpeechRecognitionUpdateType::Error:
        client->didError(update.error());
        break;
    case WebCore::SpeechRecognitionUpdateType::End:
        client->didEnd();
    }
}

IPC::Connection* WebSpeechRecognitionConnection::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t WebSpeechRecognitionConnection::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

} // namespace WebKit
