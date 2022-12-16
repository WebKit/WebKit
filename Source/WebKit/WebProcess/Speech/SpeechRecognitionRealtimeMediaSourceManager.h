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

#if ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "SandboxExtension.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/RealtimeMediaSourceIdentifier.h>

namespace WebCore {
class CaptureDevice;
}

namespace WebKit {

class SpeechRecognitionRealtimeMediaSourceManager final : public IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SpeechRecognitionRealtimeMediaSourceManager(Ref<IPC::Connection>&&);
    ~SpeechRecognitionRealtimeMediaSourceManager();

private:
    // Messages::SpeechRecognitionRealtimeMediaSourceManager
    void createSource(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, WebCore::PageIdentifier);
    void deleteSource(WebCore::RealtimeMediaSourceIdentifier);
    void start(WebCore::RealtimeMediaSourceIdentifier);
    void stop(WebCore::RealtimeMediaSourceIdentifier);
#if ENABLE(SANDBOX_EXTENSIONS)
    void grantSandboxExtensions(SandboxExtension::Handle&&, SandboxExtension::Handle&&, SandboxExtension::Handle&&);
    void revokeSandboxExtensions();
#endif

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    Ref<IPC::Connection> m_connection;

    class Source;
    friend class Source;
    HashMap<WebCore::RealtimeMediaSourceIdentifier, std::unique_ptr<Source>> m_sources;

#if ENABLE(SANDBOX_EXTENSIONS)
    RefPtr<SandboxExtension> m_machBootstrapExtension;
    RefPtr<SandboxExtension> m_sandboxExtensionForTCCD;
    RefPtr<SandboxExtension> m_sandboxExtensionForMicrophone;
#endif
};

} // namespace WebKit

#endif
