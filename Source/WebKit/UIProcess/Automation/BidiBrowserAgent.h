/*
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(GTK)
#include <wtf/glib/GRefPtr.h>
typedef struct _WebKitWebContext WebKitWebContext;
#endif

namespace WebKit {

class BidiUserContext;
class WebAutomationSession;
class WebPageProxy;
class WebProcessPool;
class WebsiteDataStore;

class BidiBrowserAgent final : public Inspector::BidiBrowserBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiBrowserAgent);
public:
    BidiBrowserAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiBrowserAgent() override;

    void didCreatePage(WebPageProxy&);
    void willClosePage(const WebPageProxy&);

    bool isValidUserContext(const String& userContextID) const;

private:
    struct BidiUserContextDeletionRecord;

    // Inspector::BidiBrowserBackendDispatcherHandler methods.
    Inspector::CommandResult<void> close() override;
    Inspector::CommandResult<String> createUserContext() override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowser::UserContextInfo>>> getUserContexts() override;
    void removeUserContext(const String& userContext, Inspector::CommandCallback<void>&&) override;

    std::unique_ptr<BidiUserContext> platformCreateUserContext(String& error);

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiBrowserBackendDispatcher> m_browserDomainDispatcher;
    HashMap<String, std::unique_ptr<BidiUserContext>> m_userContexts;
    HashMap<String, std::unique_ptr<BidiUserContextDeletionRecord>> m_userContextsPendingDeletion;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
