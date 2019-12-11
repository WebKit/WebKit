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

#pragma once

#include "APIClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "WKBundlePage.h"
#include "WebEvent.h"
#include <WebCore/RenderSnapshottedPlugIn.h>
#include <wtf/Forward.h>

namespace API {
class Object;

template<> struct ClientTraits<WKBundlePageUIClientBase> {
    typedef std::tuple<WKBundlePageUIClientV0, WKBundlePageUIClientV1, WKBundlePageUIClientV2, WKBundlePageUIClientV3, WKBundlePageUIClientV4> Versions;
};
}

namespace WebKit {

class InjectedBundlePageUIClient : public API::Client<WKBundlePageUIClientBase>, public API::InjectedBundle::PageUIClient {
public:
    explicit InjectedBundlePageUIClient(const WKBundlePageUIClientBase*);

    void willAddMessageToConsole(WebPage*, MessageSource, MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) override;
    void willSetStatusbarText(WebPage*, const String&) override;
    void willRunJavaScriptAlert(WebPage*, const String&, WebFrame*) override;
    void willRunJavaScriptConfirm(WebPage*, const String&, WebFrame*) override;
    void willRunJavaScriptPrompt(WebPage*, const String&, const String&, WebFrame*) override;
    void mouseDidMoveOverElement(WebPage*, const WebCore::HitTestResult&, OptionSet<WebEvent::Modifier>, RefPtr<API::Object>& userData) override;
    void pageDidScroll(WebPage*) override;

    UIElementVisibility statusBarIsVisible(WebPage*) override;
    UIElementVisibility menuBarIsVisible(WebPage*) override;
    UIElementVisibility toolbarsAreVisible(WebPage*) override;

    bool didReachApplicationCacheOriginQuota(WebPage*, API::SecurityOrigin*, int64_t totalBytesNeeded) override;
    uint64_t didExceedDatabaseQuota(WebPage*, API::SecurityOrigin*, const String& databaseName, const String& databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes) override;

    String plugInStartLabelTitle(const String& mimeType) const override;
    String plugInStartLabelSubtitle(const String& mimeType) const override;
    String plugInExtraStyleSheet() const override;
    String plugInExtraScript() const override;

    void didClickAutoFillButton(WebPage&, InjectedBundleNodeHandle&, RefPtr<API::Object>& userData) override;

    void didResignInputElementStrongPasswordAppearance(WebPage&, InjectedBundleNodeHandle&, RefPtr<API::Object>& userData) override;
};

} // namespace WebKit
