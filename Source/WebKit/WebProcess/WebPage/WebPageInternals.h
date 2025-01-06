/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "IdentifierTypes.h"
#include "InjectedBundlePageFullScreenClient.h"
#include "WebPage.h"
#include <WebCore/ScrollTypes.h>
#include <WebCore/VisibleSelection.h>

#if ENABLE(APP_HIGHLIGHTS)
#include <WebCore/AppHighlight.h>
#endif

namespace WebKit {

struct WebPage::Internals {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
#if PLATFORM(IOS_FAMILY)
    WebCore::VisibleSelection storedSelectionForAccessibility { WebCore::VisibleSelection() };
    FocusedElementInformationIdentifier lastFocusedElementInformationIdentifier;
    TransactionID lastTransactionIDWithScaleChange;
    std::optional<std::pair<TransactionID, double>> lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage;
#endif
#if ENABLE(APP_HIGHLIGHTS)
    WebCore::CreateNewGroupForHighlight highlightIsNewGroup { WebCore::CreateNewGroupForHighlight::No };
    WebCore::HighlightRequestOriginatedInApp highlightRequestOriginatedInApp { WebCore::HighlightRequestOriginatedInApp::No };
#endif
    std::optional<WebsitePoliciesData> pendingWebsitePolicies;
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient fullScreenClient;
#endif
    WebCore::ScrollPinningBehavior scrollPinningBehavior { WebCore::ScrollPinningBehavior::DoNotPin };
    mutable EditorStateIdentifier lastEditorStateIdentifier;
    HashMap<WebCore::RegistrableDomain, HashSet<WebCore::RegistrableDomain>> domainsWithPageLevelStorageAccess;
    HashSet<WebCore::RegistrableDomain> loadedSubresourceDomains;
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    struct LinkDecorationFilteringConditionals {
        HashSet<WebCore::RegistrableDomain> domains;
        Vector<String> paths;
    };
    HashMap<String, LinkDecorationFilteringConditionals> linkDecorationFilteringData;
    HashMap<WebCore::RegistrableDomain, HashSet<String>> allowedQueryParametersForAdvancedPrivacyProtections;
#endif
};

} // namespace WebKit
