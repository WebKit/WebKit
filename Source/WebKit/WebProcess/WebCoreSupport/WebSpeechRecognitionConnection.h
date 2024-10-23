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
#include <WebCore/PageIdentifier.h>
#include <WebCore/SpeechRecognitionConnection.h>

namespace WebCore {

class SpeechRecognitionConnectionClient;
class SpeechRecognitionUpdate;

}

namespace WebKit {

class WebPage;

using SpeechRecognitionConnectionIdentifier = WebCore::PageIdentifier;

class WebSpeechRecognitionConnection final : public WebCore::SpeechRecognitionConnection, private IPC::MessageReceiver, private IPC::MessageSender {
public:
    static Ref<WebSpeechRecognitionConnection> create(SpeechRecognitionConnectionIdentifier);

    void start(WebCore::SpeechRecognitionConnectionClientIdentifier, const String& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&&, WebCore::FrameIdentifier) final;
    void stop(WebCore::SpeechRecognitionConnectionClientIdentifier) final;
    void abort(WebCore::SpeechRecognitionConnectionClientIdentifier) final;

private:
    explicit WebSpeechRecognitionConnection(SpeechRecognitionConnectionIdentifier);
    ~WebSpeechRecognitionConnection();

    void registerClient(WebCore::SpeechRecognitionConnectionClient&) final;
    void unregisterClient(WebCore::SpeechRecognitionConnectionClient&) final;
    void didReceiveUpdate(WebCore::SpeechRecognitionUpdate&&) final;
    void invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    SpeechRecognitionConnectionIdentifier m_identifier;
    HashMap<WebCore::SpeechRecognitionConnectionClientIdentifier, WeakPtr<WebCore::SpeechRecognitionConnectionClient>> m_clientMap;
};

} // namespace WebKit
