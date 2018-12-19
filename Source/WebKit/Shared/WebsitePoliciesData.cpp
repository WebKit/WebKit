/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebsitePoliciesData.h"

#include "ArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>

namespace WebKit {

void WebsitePoliciesData::encode(IPC::Encoder& encoder) const
{
    encoder << contentBlockersEnabled;
    encoder << autoplayPolicy;
    encoder << allowedAutoplayQuirks;
    encoder << customHeaderFields;
    encoder << popUpPolicy;
    encoder << websiteDataStoreParameters;
    encoder << customUserAgent;
    encoder << customNavigatorPlatform;
}

std::optional<WebsitePoliciesData> WebsitePoliciesData::decode(IPC::Decoder& decoder)
{
    std::optional<bool> contentBlockersEnabled;
    decoder >> contentBlockersEnabled;
    if (!contentBlockersEnabled)
        return std::nullopt;
    
    std::optional<WebsiteAutoplayPolicy> autoplayPolicy;
    decoder >> autoplayPolicy;
    if (!autoplayPolicy)
        return std::nullopt;
    
    std::optional<OptionSet<WebsiteAutoplayQuirk>> allowedAutoplayQuirks;
    decoder >> allowedAutoplayQuirks;
    if (!allowedAutoplayQuirks)
        return std::nullopt;
    
    std::optional<Vector<WebCore::HTTPHeaderField>> customHeaderFields;
    decoder >> customHeaderFields;
    if (!customHeaderFields)
        return std::nullopt;

    std::optional<WebsitePopUpPolicy> popUpPolicy;
    decoder >> popUpPolicy;
    if (!popUpPolicy)
        return std::nullopt;

    std::optional<std::optional<WebsiteDataStoreParameters>> websiteDataStoreParameters;
    decoder >> websiteDataStoreParameters;
    if (!websiteDataStoreParameters)
        return std::nullopt;

    std::optional<String> customUserAgent;
    decoder >> customUserAgent;
    if (!customUserAgent)
        return std::nullopt;

    std::optional<String> customNavigatorPlatform;
    decoder >> customNavigatorPlatform;
    if (!customNavigatorPlatform)
        return std::nullopt;
    
    return { {
        WTFMove(*contentBlockersEnabled),
        WTFMove(*allowedAutoplayQuirks),
        WTFMove(*autoplayPolicy),
        WTFMove(*customHeaderFields),
        WTFMove(*popUpPolicy),
        WTFMove(*websiteDataStoreParameters),
        WTFMove(*customUserAgent),
        WTFMove(*customNavigatorPlatform),
    } };
}

void WebsitePoliciesData::applyToDocumentLoader(WebsitePoliciesData&& websitePolicies, WebCore::DocumentLoader& documentLoader)
{
    documentLoader.setCustomHeaderFields(WTFMove(websitePolicies.customHeaderFields));
    documentLoader.setCustomUserAgent(websitePolicies.customUserAgent);
    documentLoader.setCustomNavigatorPlatform(websitePolicies.customNavigatorPlatform);
    
    // Only setUserContentExtensionsEnabled if it hasn't already been disabled by reloading without content blockers.
    if (documentLoader.userContentExtensionsEnabled())
        documentLoader.setUserContentExtensionsEnabled(websitePolicies.contentBlockersEnabled);

    OptionSet<WebCore::AutoplayQuirk> quirks;
    const auto& allowedQuirks = websitePolicies.allowedAutoplayQuirks;
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::InheritedUserGestures))
        quirks.add(WebCore::AutoplayQuirk::InheritedUserGestures);
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::SynthesizedPauseEvents))
        quirks.add(WebCore::AutoplayQuirk::SynthesizedPauseEvents);
    
    if (allowedQuirks.contains(WebsiteAutoplayQuirk::ArbitraryUserGestures))
        quirks.add(WebCore::AutoplayQuirk::ArbitraryUserGestures);

    documentLoader.setAllowedAutoplayQuirks(quirks);

    switch (websitePolicies.autoplayPolicy) {
    case WebsiteAutoplayPolicy::Default:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Default);
        break;
    case WebsiteAutoplayPolicy::Allow:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Allow);
        break;
    case WebsiteAutoplayPolicy::AllowWithoutSound:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::AllowWithoutSound);
        break;
    case WebsiteAutoplayPolicy::Deny:
        documentLoader.setAutoplayPolicy(WebCore::AutoplayPolicy::Deny);
        break;
    }

    switch (websitePolicies.popUpPolicy) {
    case WebsitePopUpPolicy::Default:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Default);
        break;
    case WebsitePopUpPolicy::Allow:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Allow);
        break;
    case WebsitePopUpPolicy::Block:
        documentLoader.setPopUpPolicy(WebCore::PopUpPolicy::Block);
        break;
    }

    if (websitePolicies.websiteDataStoreParameters) {
        if (auto* frame = documentLoader.frame()) {
            if (auto* page = frame->page()) {
                auto sessionID = websitePolicies.websiteDataStoreParameters->networkSessionParameters.sessionID;
                WebProcess::singleton().addWebsiteDataStore(WTFMove(*websitePolicies.websiteDataStoreParameters));
                page->setSessionID(sessionID);
            }
        }
    }
}

}

