/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "InjectedBundlePageUIClient.h"

#include "APISecurityOrigin.h"
#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleNodeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

InjectedBundlePageUIClient::InjectedBundlePageUIClient(const WKBundlePageUIClientBase* client)
{
    initialize(client);
}

void InjectedBundlePageUIClient::willAddMessageToConsole(WebPage* page, MessageSource, MessageLevel, const String& message, unsigned lineNumber, unsigned /*columnNumber*/, const String& /*sourceID*/)
{
    if (m_client.willAddMessageToConsole)
        m_client.willAddMessageToConsole(toAPI(page), toAPI(message.impl()), lineNumber, m_client.base.clientInfo);
}

void InjectedBundlePageUIClient::willSetStatusbarText(WebPage* page, const String& statusbarText)
{
    if (m_client.willSetStatusbarText)
        m_client.willSetStatusbarText(toAPI(page), toAPI(statusbarText.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptAlert(WebPage* page, const String& alertText, WebFrame* frame)
{
    if (m_client.willRunJavaScriptAlert)
        m_client.willRunJavaScriptAlert(toAPI(page), toAPI(alertText.impl()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptConfirm(WebPage* page, const String& message, WebFrame* frame)
{
    if (m_client.willRunJavaScriptConfirm)
        m_client.willRunJavaScriptConfirm(toAPI(page), toAPI(message.impl()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptPrompt(WebPage* page, const String& message, const String& defaultValue, WebFrame* frame)
{
    if (m_client.willRunJavaScriptPrompt)
        m_client.willRunJavaScriptPrompt(toAPI(page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), m_client.base.clientInfo);
}

void InjectedBundlePageUIClient::mouseDidMoveOverElement(WebPage* page, const HitTestResult& coreHitTestResult, OptionSet<WebEvent::Modifier> modifiers, RefPtr<API::Object>& userData)
{
    if (!m_client.mouseDidMoveOverElement)
        return;

    auto hitTestResult = InjectedBundleHitTestResult::create(coreHitTestResult);

    WKTypeRef userDataToPass = 0;
    m_client.mouseDidMoveOverElement(toAPI(page), toAPI(hitTestResult.ptr()), toAPI(modifiers), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageUIClient::pageDidScroll(WebPage* page)
{
    if (!m_client.pageDidScroll)
        return;

    m_client.pageDidScroll(toAPI(page), m_client.base.clientInfo);
}

static API::InjectedBundle::PageUIClient::UIElementVisibility toUIElementVisibility(WKBundlePageUIElementVisibility visibility)
{
    switch (visibility) {
    case WKBundlePageUIElementVisibilityUnknown:
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown;
    case WKBundlePageUIElementVisible:
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    case WKBundlePageUIElementHidden:
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Hidden;
    }

    ASSERT_NOT_REACHED();
    return API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown;
}

API::InjectedBundle::PageUIClient::UIElementVisibility InjectedBundlePageUIClient::statusBarIsVisible(WebPage* page)
{
    if (!m_client.statusBarIsVisible)
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown;
    
    return toUIElementVisibility(m_client.statusBarIsVisible(toAPI(page), m_client.base.clientInfo));
}

API::InjectedBundle::PageUIClient::UIElementVisibility InjectedBundlePageUIClient::menuBarIsVisible(WebPage* page)
{
    if (!m_client.menuBarIsVisible)
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown;
    
    return toUIElementVisibility(m_client.menuBarIsVisible(toAPI(page), m_client.base.clientInfo));
}

API::InjectedBundle::PageUIClient::UIElementVisibility InjectedBundlePageUIClient::toolbarsAreVisible(WebPage* page)
{
    if (!m_client.toolbarsAreVisible)
        return API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown;
    
    return toUIElementVisibility(m_client.toolbarsAreVisible(toAPI(page), m_client.base.clientInfo));
}

bool InjectedBundlePageUIClient::didReachApplicationCacheOriginQuota(WebPage* page, API::SecurityOrigin* origin, int64_t totalBytesNeeded)
{
    if (!m_client.didReachApplicationCacheOriginQuota)
        return false;

    m_client.didReachApplicationCacheOriginQuota(toAPI(page), toAPI(origin), totalBytesNeeded, m_client.base.clientInfo);
    return true;
}

uint64_t InjectedBundlePageUIClient::didExceedDatabaseQuota(WebPage* page, API::SecurityOrigin* origin, const String& databaseName, const String& databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes)
{
    if (!m_client.didExceedDatabaseQuota)
        return 0;

    return m_client.didExceedDatabaseQuota(toAPI(page), toAPI(origin), toAPI(databaseName.impl()), toAPI(databaseDisplayName.impl()), currentQuotaBytes, currentOriginUsageBytes, currentDatabaseUsageBytes, expectedUsageBytes, m_client.base.clientInfo);
}

String InjectedBundlePageUIClient::plugInStartLabelTitle(const String& mimeType) const
{
    if (!m_client.createPlugInStartLabelTitle)
        return String();

    RefPtr<API::String> title = adoptRef(toImpl(m_client.createPlugInStartLabelTitle(toAPI(mimeType.impl()), m_client.base.clientInfo)));
    return title ? title->string() : String();
}

String InjectedBundlePageUIClient::plugInStartLabelSubtitle(const String& mimeType) const
{
    if (!m_client.createPlugInStartLabelSubtitle)
        return String();

    RefPtr<API::String> subtitle = adoptRef(toImpl(m_client.createPlugInStartLabelSubtitle(toAPI(mimeType.impl()), m_client.base.clientInfo)));
    return subtitle ? subtitle->string() : String();
}

String InjectedBundlePageUIClient::plugInExtraStyleSheet() const
{
    if (!m_client.createPlugInExtraStyleSheet)
        return String();

    RefPtr<API::String> styleSheet = adoptRef(toImpl(m_client.createPlugInExtraStyleSheet(m_client.base.clientInfo)));
    return styleSheet ? styleSheet->string() : String();
}

String InjectedBundlePageUIClient::plugInExtraScript() const
{
    if (!m_client.createPlugInExtraScript)
        return String();

    RefPtr<API::String> script = adoptRef(toImpl(m_client.createPlugInExtraScript(m_client.base.clientInfo)));
    return script ? script->string() : String();
}

void InjectedBundlePageUIClient::didClickAutoFillButton(WebPage& page, InjectedBundleNodeHandle& nodeHandle, RefPtr<API::Object>& userData)
{
    if (!m_client.didClickAutoFillButton)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didClickAutoFillButton(toAPI(&page), toAPI(&nodeHandle), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageUIClient::didResignInputElementStrongPasswordAppearance(WebPage& page, InjectedBundleNodeHandle& nodeHandle, RefPtr<API::Object>& userData)
{
    if (!m_client.didResignInputElementStrongPasswordAppearance)
        return;

    WKTypeRef userDataToPass = nullptr;
    m_client.didResignInputElementStrongPasswordAppearance(toAPI(&page), toAPI(&nodeHandle), &userDataToPass, m_client.base.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

} // namespace WebKit
