/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAutomationSession.h"

#include "APIAutomationSessionClient.h"
#include "InspectorProtocolObjects.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>

using namespace Inspector;

#define FAIL_WITH_PREDEFINED_ERROR_MESSAGE(messageName) \
do { \
    auto enumValue = Inspector::Protocol::Automation::ErrorMessage::messageName; \
    errorString = Inspector::Protocol::getEnumConstantValue(enumValue); \
    return; \
} while (false)

namespace WebKit {

WebAutomationSession::WebAutomationSession()
    : m_client(std::make_unique<API::AutomationSessionClient>())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_domainDispatcher(AutomationBackendDispatcher::create(m_backendDispatcher, this))
{
}

WebAutomationSession::~WebAutomationSession()
{
    ASSERT(!m_client);
}

void WebAutomationSession::setClient(std::unique_ptr<API::AutomationSessionClient> client)
{
    m_client = WTFMove(client);
}

// NOTE: this class could be split at some point to support local and remote automation sessions.
// For now, it only works with a remote automation driver over a RemoteInspector connection.

#if ENABLE(REMOTE_INSPECTOR)

// Inspector::RemoteAutomationTarget API

void WebAutomationSession::dispatchMessageFromRemote(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

void WebAutomationSession::connect(Inspector::FrontendChannel* channel, bool isAutomaticConnection)
{
    UNUSED_PARAM(isAutomaticConnection);

    m_remoteChannel = channel;
    m_frontendRouter->connectFrontend(channel);

    setIsPaired(true);
}

void WebAutomationSession::disconnect(Inspector::FrontendChannel* channel)
{
    ASSERT(channel == m_remoteChannel);

    m_remoteChannel = nullptr;
    m_frontendRouter->disconnectFrontend(channel);

    setIsPaired(false);

    if (m_client)
        m_client->didDisconnectFromRemote(this);
}

#endif // ENABLE(REMOTE_INSPECTOR)

// Inspector::AutomationBackendDispatcherHandler API

void WebAutomationSession::getBrowsingContexts(Inspector::ErrorString& errorString, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>>& contexts)
{
    FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NotImplemented);
}

void WebAutomationSession::createBrowsingContext(Inspector::ErrorString& errorString, String* handle)
{
    FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NotImplemented);
}

void WebAutomationSession::closeBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NotImplemented);
}

void WebAutomationSession::switchToBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NotImplemented);
}

} // namespace WebKit
