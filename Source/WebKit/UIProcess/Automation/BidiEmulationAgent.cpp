/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "BidiEmulationAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "BidiBrowserAgent.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProcessor.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/Borrow.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiEmulationAgent);

BidiEmulationAgent::BidiEmulationAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_emulationDomainDispatcher(BidiEmulationBackendDispatcher::create(backendDispatcher, this))
{
}

BidiEmulationAgent::~BidiEmulationAgent() = default;

static Vector<Ref<WebPageProxy>> pagesControlledByAutomation(const WebAutomationSession& session)
{
    Vector<Ref<WebPageProxy>> pages;
    RefPtr processPool = session.processPool();
    if (!processPool)
        return pages;

    for (Ref process : borrow(processPool->processes()).get()) {
        for (Ref page : process->mainPages()) {
            if (page->isControlledByAutomation())
                pages.append(WTF::move(page));
        }
    }
    return pages;
}

void BidiEmulationAgent::didCreatePage(WebPageProxy& page)
{
    page.setScrollbarStyleOverride(scrollbarTypeOverrideForPage(page));
}

std::optional<ScrollbarStyle> BidiEmulationAgent::scrollbarTypeOverrideForPage(const WebPageProxy& page) const
{
    if (auto styleOverride = m_scrollbarTypeOverridesForBrowsingContexts.getOptional(page))
        return styleOverride;

    if (RefPtr session = m_session) {
        if (auto styleOverride = m_scrollbarTypeOverridesForUserContexts.getOptional(session->bidiProcessor().browserAgent().userContextForPage(page)))
            return styleOverride;
    }

    return m_scrollbarTypeOverride;
}

CommandResult<void> BidiEmulationAgent::setScrollbarTypeOverride(Ref<JSON::Value>&& scrollbarTypeValue, RefPtr<JSON::Array>&& optionalContexts, RefPtr<JSON::Array>&& optionalUserContexts)
{
    RefPtr session = m_session;
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    std::optional<ScrollbarStyle> scrollbarType;
    if (scrollbarTypeValue->type() != JSON::Value::Type::Null) {
        auto scrollbarTypeString = scrollbarTypeValue->asString();
        if (scrollbarTypeString == "classic"_s)
            scrollbarType = ScrollbarStyle::AlwaysVisible;
        else if (scrollbarTypeString == "overlay"_s)
            scrollbarType = ScrollbarStyle::Overlay;
        else
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'scrollbarType' must be \"classic\", \"overlay\" or null."_s);
    }

#if PLATFORM(IOS_FAMILY)
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(scrollbarType, NotImplemented, "Scrollbar type overrides are not supported on this platform."_s);
#endif

    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalContexts && optionalUserContexts, InvalidParameter, "The parameters 'contexts' and 'userContexts' cannot both be given."_s);

    if (optionalContexts) {
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalContexts->length(), InvalidParameter, "The parameter 'contexts' cannot be empty."_s);

        Vector<Ref<WebPageProxy>> pages;
        for (const auto& contextValue : *optionalContexts) {
            auto context = contextValue->asString();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(context.isNull(), InvalidParameter, "The parameter 'contexts' must contain strings."_s);

            auto handles = session->extractBrowsingContextHandles(context);
            if (!handles)
                return makeUnexpected(handles.error().toProtocolString());
            auto& [pageHandle, frameHandle] = handles.value();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!frameHandle.isEmpty(), InvalidParameter, "The parameter 'contexts' must contain top-level browsing contexts."_s);

            RefPtr page = session->webPageProxyForHandle(pageHandle);
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, FrameNotFound);
            pages.append(page.releaseNonNull());
        }

        for (Ref page : pages) {
            if (scrollbarType)
                m_scrollbarTypeOverridesForBrowsingContexts.set(page.get(), *scrollbarType);
            else
                m_scrollbarTypeOverridesForBrowsingContexts.remove(page.get());
            page->setScrollbarStyleOverride(scrollbarTypeOverrideForPage(page));
        }
        return { };
    }

    if (optionalUserContexts) {
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalUserContexts->length(), InvalidParameter, "The parameter 'userContexts' cannot be empty."_s);

        HashSet<String> userContexts;
        for (const auto& userContextValue : *optionalUserContexts) {
            auto userContext = userContextValue->asString();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(userContext.isNull(), InvalidParameter, "The parameter 'userContexts' must contain strings."_s);
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session->isValidUserContext(userContext), NoSuchUserContext);
            userContexts.add(userContext);
        }

        for (auto& userContext : userContexts) {
            if (scrollbarType)
                m_scrollbarTypeOverridesForUserContexts.set(userContext, *scrollbarType);
            else
                m_scrollbarTypeOverridesForUserContexts.remove(userContext);
        }

        auto& browserAgent = session->bidiProcessor().browserAgent();
        for (Ref page : pagesControlledByAutomation(*session)) {
            if (userContexts.contains(browserAgent.userContextForPage(page)))
                page->setScrollbarStyleOverride(scrollbarTypeOverrideForPage(page));
        }
        return { };
    }

    m_scrollbarTypeOverride = scrollbarType;
    for (Ref page : pagesControlledByAutomation(*session))
        page->setScrollbarStyleOverride(scrollbarTypeOverrideForPage(page));
    return { };
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
