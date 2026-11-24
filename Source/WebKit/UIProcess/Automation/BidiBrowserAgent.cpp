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

#include "AutomationProtocolObjects.h"
#include "BidiUserContext.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <pal/SessionID.h>
#include <wtf/Borrow.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

using namespace Inspector;
using ClientWindowInfo = Inspector::Protocol::BidiBrowser::ClientWindowInfo;
using ClientWindowState = Inspector::Protocol::BidiBrowser::ClientWindowState;
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

ClientWindowState toProtocol(WebAutomationSession::ClientWindowState state)
{
    switch (state) {
    case WebAutomationSession::ClientWindowState::Normal:
        return ClientWindowState::Normal;
    case WebAutomationSession::ClientWindowState::Minimized:
        return ClientWindowState::Minimized;
    case WebAutomationSession::ClientWindowState::Maximized:
        return ClientWindowState::Maximized;
    case WebAutomationSession::ClientWindowState::Fullscreen:
        return ClientWindowState::Fullscreen;
    }

    ASSERT_NOT_REACHED();
    return ClientWindowState::Normal;
}

// Takes the pages in reverse order, so that each step can take the next one from the end.
void appendClientWindowInfo(WebAutomationSession& session, Vector<Ref<WebPageProxy>>&& pages, Ref<JSON::ArrayOf<ClientWindowInfo>>&& clientWindows, CommandCallback<Ref<JSON::ArrayOf<ClientWindowInfo>>>&& callback)
{
    if (pages.isEmpty()) {
        callback(WTF::move(clientWindows));
        return;
    }

    Ref page = pages.takeLast();
    page->getWindowFrameWithCallback([session = Ref { session }, page = page.copyRef(), pages = WTF::move(pages), clientWindows = WTF::move(clientWindows), callback = WTF::move(callback)](WebCore::FloatRect windowFrame) mutable {
        // https://w3c.github.io/webdriver-bidi/#get-the-client-window-info
        auto info = ClientWindowInfo::create()
            .setActive(session->isClientWindowActiveForPage(page))
            .setClientWindow(WebAutomationSession::clientWindowForPage(page))
            .setHeight(std::max(0, clampTo<int>(std::round(windowFrame.height()))))
            .setState(toProtocol(session->clientWindowStateForPage(page)))
            .setWidth(std::max(0, clampTo<int>(std::round(windowFrame.width()))))
            .setX(clampTo<int>(std::round(windowFrame.x())))
            .setY(clampTo<int>(std::round(windowFrame.y())))
            .release();
        clientWindows->addItem(WTF::move(info));

        appendClientWindowInfo(session, WTF::move(pages), WTF::move(clientWindows), WTF::move(callback));
    });
}

} // namespace

struct BidiBrowserAgent::BidiUserContextDeletionRecord {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(BidiUserContextDeletionRecord);
    WTF_MAKE_NONCOPYABLE(BidiUserContextDeletionRecord);
public:
    BidiUserContextDeletionRecord(std::unique_ptr<BidiUserContext>&& userContext, size_t pageCount, CommandCallback<void>&& callback)
        : userContext(WTF::move(userContext))
        , pageCount(pageCount)
        , callback(WTF::move(callback)) { }

    std::unique_ptr<BidiUserContext> userContext;
    size_t pageCount;
    CommandCallback<void> callback;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiBrowserAgent);

BidiBrowserAgent::BidiBrowserAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_browserDomainDispatcher(BidiBrowserBackendDispatcher::create(backendDispatcher, this))
{
}

BidiBrowserAgent::~BidiBrowserAgent() = default;

void BidiBrowserAgent::didCreatePage(WebPageProxy& page)
{
    String userContextID = toUserContextIDProtocolString(page.sessionID());
    auto it = m_userContextsPendingDeletion.find(userContextID);
    if (it == m_userContextsPendingDeletion.end())
        return;

    ++it->value->pageCount;
    page.askClientToClose();
}

void BidiBrowserAgent::willClosePage(const WebPageProxy& page)
{
    String userContextID = toUserContextIDProtocolString(page.sessionID());
    auto it = m_userContextsPendingDeletion.find(userContextID);
    if (it == m_userContextsPendingDeletion.end())
        return;

    --it->value->pageCount;
    if (it->value->pageCount)
        return;

    it->value->callback({ });
    m_userContextsPendingDeletion.remove(it);
}

bool BidiBrowserAgent::isValidUserContext(const String& userContextID) const
{
    if (userContextID.isEmpty())
        return false;

    if (userContextID == defaultUserContextID())
        return true;

    return m_userContexts.contains(userContextID);
}

String BidiBrowserAgent::userContextForPage(const WebPageProxy& page) const
{
    String userContextID = toUserContextIDProtocolString(page.sessionID());
    if (m_userContexts.contains(userContextID))
        return userContextID;
    return defaultUserContextID();
}

// MARK: Inspector::BidiBrowserDispatcherHandler methods.

CommandResult<void> BidiBrowserAgent::close()
{
    RefPtr session = m_session.get();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    session->terminate();

    return { };
}

CommandResult<String> BidiBrowserAgent::createUserContext()
{
    String error;
    auto userContext = platformCreateUserContext(error);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!userContext, InternalError, error);

    ASSERT(!userContext->dataStore().isPersistent());

    PAL::SessionID sessionID = userContext->dataStore().sessionID();
    String userContextID = toUserContextIDProtocolString(sessionID);
    m_userContexts.set(userContextID, WTF::move(userContext));

    return userContextID;
}

void BidiBrowserAgent::getClientWindows(CommandCallback<Ref<JSON::ArrayOf<ClientWindowInfo>>>&& callback)
{
    // https://w3c.github.io/webdriver-bidi/#command-browser-getClientWindows
    RefPtr session = m_session;
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    RefPtr processPool = session->processPool();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!processPool, InternalError);

    // Only windows hosting a top-level browsing context under automation are reported, never other windows
    // of the browser. Pages are found through their main frame's process; with site isolation, the other
    // processes hosting the page's frames only have a RemotePageProxy for it, so each page is seen once.
    HashSet<String> clientWindowIDs;
    Vector<Ref<WebPageProxy>> pages;
    for (Ref process : borrow(processPool->processes()).get()) {
        for (Ref page : process->mainPages()) {
            if (!page->isControlledByAutomation())
                continue;

            if (!clientWindowIDs.add(WebAutomationSession::clientWindowForPage(page)).isNewEntry)
                continue;

            pages.append(WTF::move(page));
        }
    }

    pages.reverse();
    appendClientWindowInfo(*session, WTF::move(pages), JSON::ArrayOf<ClientWindowInfo>::create(), WTF::move(callback));
}

CommandResult<Ref<JSON::ArrayOf<UserContextInfo>>> BidiBrowserAgent::getUserContexts()
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

void BidiBrowserAgent::removeUserContext(const String& userContextID, CommandCallback<void>&& callback)
{
    // https://www.w3.org/TR/webdriver-bidi/#command-browser-removeUserContext step 2.
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(userContextID == defaultUserContextID(), InvalidParameter, "Cannot delete default user context."_s);

    auto it = m_userContexts.find(userContextID);
    // https://www.w3.org/TR/webdriver-bidi/#command-browser-removeUserContext step 4.
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(it == m_userContexts.end(), InvalidParameter, "no such user context"_s);

    auto userContext = m_userContexts.take(it);
    size_t pageCount = userContext->allPages().size();
    if (!pageCount) {
        callback({ });
        return;
    }

    m_userContextsPendingDeletion.set(userContextID, makeUnique<BidiUserContextDeletionRecord>(WTF::move(userContext), pageCount, WTF::move(callback)));
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

