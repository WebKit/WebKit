/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "BidiPermissionsAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "Logging.h"
#include "PageLoadState.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/RegistrableDomain.h>
#include <wtf/CallbackAggregator.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiPermissionsAgent);

BidiPermissionsAgent::BidiPermissionsAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
{
}

BidiPermissionsAgent::~BidiPermissionsAgent() = default;

static Vector<Ref<WebPageProxy>> allPageProxiesFor(const WebAutomationSession& session)
{
    Vector<Ref<WebPageProxy>> pages;
    for (Ref process : session.protectedProcessPool()->processes()) {
        for (Ref page : process->pages()) {
            if (!page->isControlledByAutomation())
                continue;
            pages.append(WTFMove(page));
        }
    }

    return pages;
}

void BidiPermissionsAgent::setPermission(Ref<JSON::Object>&& descriptor, const String& origin, Inspector::Protocol::BidiPermissions::PermissionState state, const String& optionalUserContext, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto permissionName = descriptor->getString("name"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!permissionName, MissingParameter, "The parameter 'name' was not found."_s);

    if (permissionName == "storage-access"_s) {
        auto topFrameOrigin = RegistrableDomain { URL { origin } };
        auto subFrameURLString = descriptor->getString("subFrameURL"_s);
        bool embeddedOriginIsWildcard = subFrameURLString == "*"_s;
        auto embeddedOrigin = RegistrableDomain { URL { subFrameURLString } };

        Ref callbackAggregator = CallbackAggregator::create([callback = WTFMove(callback)]() {
            callback({ });
        });

        for (auto page : allPageProxiesFor(*session)) {
            auto pageOrigin = RegistrableDomain { page->protectedPageLoadState()->origin() };
            if (pageOrigin != topFrameOrigin)
                continue;

            Ref store = page->websiteDataStore();
            bool granted = state == Inspector::Protocol::BidiPermissions::PermissionState::Granted;
            if (!granted)
                store->clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
            store->setStorageAccessPermissionForTesting(granted, page->identifier(), topFrameOrigin.string(), (embeddedOriginIsWildcard ? topFrameOrigin : embeddedOrigin).string(), [callbackAggregator] () { });
        }
        return;
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, makeString("Permission '"_s, permissionName, "' not supported yet."_s));
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
