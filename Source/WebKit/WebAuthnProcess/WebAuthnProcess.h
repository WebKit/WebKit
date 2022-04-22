/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "AuxiliaryProcess.h"
#include "WebAuthnProcessMessages.h"
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct MockWebAuthenticationConfiguration;
}

namespace WebKit {

class AuthenticatorManager;
class WebAuthnConnectionToWebProcess;
struct WebAuthnProcessCreationParameters;

class WebAuthnProcess : public AuxiliaryProcess, public ThreadSafeRefCounted<WebAuthnProcess> {
    WTF_MAKE_NONCOPYABLE(WebAuthnProcess);
public:
    explicit WebAuthnProcess(AuxiliaryProcessInitializationParameters&&);
    ~WebAuthnProcess();
    static constexpr WebCore::AuxiliaryProcessType processType = WebCore::AuxiliaryProcessType::WebAuthn;

    void removeWebAuthnConnectionToWebProcess(WebAuthnConnectionToWebProcess&);

    void prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&&);
    void processDidResume();
    void resume();

    void connectionToWebProcessClosed(IPC::Connection&);

    WebAuthnConnectionToWebProcess* webProcessConnection(WebCore::ProcessIdentifier) const;

    AuthenticatorManager& authenticatorManager() { return m_authenticatorManager.get(); }
    void setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&&);

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif

private:
    void platformInitializeWebAuthnProcess(const WebAuthnProcessCreationParameters&);
    void lowMemoryHandler(Critical);

    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;

    // Message Handlers
    void initializeWebAuthnProcess(WebAuthnProcessCreationParameters&&);
    void createWebAuthnConnectionToWebProcess(WebCore::ProcessIdentifier, CompletionHandler<void(std::optional<IPC::Attachment>&&)>&&);

    // Connections to WebProcesses.
    HashMap<WebCore::ProcessIdentifier, Ref<WebAuthnConnectionToWebProcess>> m_webProcessConnections;

    UniqueRef<AuthenticatorManager> m_authenticatorManager;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
