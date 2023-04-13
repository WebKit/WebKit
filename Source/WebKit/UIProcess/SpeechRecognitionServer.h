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

#pragma once

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "SpeechRecognitionPermissionRequest.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/SpeechRecognitionError.h>
#include <WebCore/SpeechRecognitionRequest.h>
#include <WebCore/SpeechRecognitionResultData.h>
#include <WebCore/SpeechRecognizer.h>
#include <wtf/Deque.h>

namespace WebCore {
enum class SpeechRecognitionUpdateType : uint8_t;
struct CaptureSourceOrError;
struct ClientOrigin;
}

namespace WebKit {

class WebProcessProxy;

using SpeechRecognitionServerIdentifier = WebCore::PageIdentifier;
using SpeechRecognitionPermissionChecker = Function<void(WebCore::SpeechRecognitionRequest&, SpeechRecognitionPermissionRequestCallback&&)>;
using SpeechRecognitionCheckIfMockSpeechRecognitionEnabled = Function<bool()>;

class SpeechRecognitionServer : public IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if ENABLE(MEDIA_STREAM)
    using RealtimeMediaSourceCreateFunction = Function<WebCore::CaptureSourceOrError()>;
    SpeechRecognitionServer(Ref<IPC::Connection>&&, SpeechRecognitionServerIdentifier, SpeechRecognitionPermissionChecker&&, SpeechRecognitionCheckIfMockSpeechRecognitionEnabled&&, RealtimeMediaSourceCreateFunction&&);
#else
    SpeechRecognitionServer(Ref<IPC::Connection>&&, SpeechRecognitionServerIdentifier, SpeechRecognitionPermissionChecker&&, SpeechRecognitionCheckIfMockSpeechRecognitionEnabled&&);
#endif

    void start(WebCore::SpeechRecognitionConnectionClientIdentifier, String&& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&&, WebCore::FrameIdentifier);
    void stop(WebCore::SpeechRecognitionConnectionClientIdentifier);
    void abort(WebCore::SpeechRecognitionConnectionClientIdentifier);
    void invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier);
    void mute();

private:
    void requestPermissionForRequest(WebCore::SpeechRecognitionRequest&);
    void handleRequest(UniqueRef<WebCore::SpeechRecognitionRequest>&&);
    void sendUpdate(WebCore::SpeechRecognitionConnectionClientIdentifier, WebCore::SpeechRecognitionUpdateType, std::optional<WebCore::SpeechRecognitionError> = std::nullopt, std::optional<Vector<WebCore::SpeechRecognitionResultData>> = std::nullopt);
    void sendUpdate(const WebCore::SpeechRecognitionUpdate&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    Ref<IPC::Connection> m_connection;
    SpeechRecognitionServerIdentifier m_identifier;
    HashMap<WebCore::SpeechRecognitionConnectionClientIdentifier, std::unique_ptr<WebCore::SpeechRecognitionRequest>> m_requests;
    SpeechRecognitionPermissionChecker m_permissionChecker;
    std::unique_ptr<WebCore::SpeechRecognizer> m_recognizer;
    SpeechRecognitionCheckIfMockSpeechRecognitionEnabled m_checkIfMockSpeechRecognitionEnabled;
    bool m_isResetting { false };

#if ENABLE(MEDIA_STREAM)
    RealtimeMediaSourceCreateFunction m_realtimeMediaSourceCreateFunction;
#endif
};

} // namespace WebKit
