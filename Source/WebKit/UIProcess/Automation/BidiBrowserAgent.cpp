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

#include "config.h"
#include "BidiBrowserAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "APIAutomationClient.h"
#include "AutomationProtocolObjects.h"
#include "BidiUserContext.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <pal/SessionID.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

using namespace Inspector;
using UserContextInfo = Inspector::Protocol::BidiBrowser::UserContextInfo;

namespace {

String toUserContextIDProtocolString(const PAL::SessionID& sessionID)
{
    StringBuilder builder;
    builder.append(hex(sessionID.toUInt64(), 16));
    return builder.toString();
}

const String& defaultUserContextID()
{
    static NeverDestroyed<const String> contextID(MAKE_STATIC_STRING_IMPL("default"));
    return contextID;
}

} // namespace

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiBrowserAgent);

BidiBrowserAgent::BidiBrowserAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_browserDomainDispatcher(BidiBrowserBackendDispatcher::create(backendDispatcher, this))
{
}

BidiBrowserAgent::~BidiBrowserAgent() = default;

// MARK: Inspector::BidiBrowserDispatcherHandler methods.

Inspector::CommandResult<void> BidiBrowserAgent::close()
{
    RefPtr session = m_session.get();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    session->terminate();

    // FIXME: this should not depend on a particular process pool.
    if (auto* client = session->protectedProcessPool()->automationClient())
        client->closeBrowser();
    else
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "unable to close browser"_s); // Fixme: this should be a Bidi error code.

    return { };
}

Inspector::CommandResult<String> BidiBrowserAgent::createUserContext()
{
    String error;
    auto userContext = platformCreateUserContext(error);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!userContext, InternalError, error);

    PAL::SessionID sessionID = userContext->dataStore().sessionID();
    String userContextID = toUserContextIDProtocolString(sessionID);
    m_userContexts.set(userContextID, WTFMove(userContext));

    return userContextID;
}

Inspector::CommandResult<Ref<JSON::ArrayOf<UserContextInfo>>> BidiBrowserAgent::getUserContexts()
{
    auto userContexts = JSON::ArrayOf<UserContextInfo>::create();
    userContexts->addItem(UserContextInfo::create()
        .setUserContext(defaultUserContextID())
        .release());
    for (const auto& pair : m_userContexts) {
        userContexts->addItem(UserContextInfo::create()
            .setUserContext(pair.key)
            .release());
    }
    return userContexts;
}

Inspector::CommandResult<void> BidiBrowserAgent::removeUserContext(const String& userContext)
{
    // https://www.w3.org/TR/webdriver-bidi/#command-browser-removeUserContext step 2.
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(userContext == defaultUserContextID(), InvalidParameter, "Cannot delete default user context."_s);

    auto it = m_userContexts.find(userContext);
    // https://www.w3.org/TR/webdriver-bidi/#command-browser-removeUserContext step 4.
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(it == m_userContexts.end(), InvalidParameter, "no such user context"_s);

    // TODO: track and close all pages that belong to this user context.
    m_userContexts.remove(it);
    return { };
}

#if !USE(GLIB)
std::unique_ptr<BidiUserContext> BidiBrowserAgent::platformCreateUserContext(String& error)
{
    error = "User context creation is not implemented for this platform yet."_s;
    return nullptr;
}
#endif

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

