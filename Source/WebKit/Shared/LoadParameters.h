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

#pragma once

#include "NetworkResourceLoadIdentifier.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "UserData.h"
#include "WebsitePoliciesData.h"
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/OwnerPermissionsPolicyData.h>
#include <WebCore/PublicSuffixStore.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/SubstituteData.h>

OBJC_CLASS NSDictionary;

namespace WebCore {
using SandboxFlags = int;
class SharedBuffer;
}

namespace WebKit {

struct LoadParameters {
    WebCore::PublicSuffix publicSuffix;

    std::optional<WebCore::NavigationIdentifier> navigationID;
    std::optional<WebCore::FrameIdentifier> frameIdentifier;

    WebCore::ResourceRequest request;
    SandboxExtension::Handle sandboxExtensionHandle;

    RefPtr<WebCore::SharedBuffer> data;
    String MIMEType;
    String encodingName;

    String baseURLString;
    String unreachableURLString;
    String provisionalLoadErrorURLString;

    std::optional<WebsitePoliciesData> websitePolicies;

    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy { WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad { WebCore::ShouldTreatAsContinuingLoad::No };
    UserData userData;
    WebCore::LockHistory lockHistory { WebCore::LockHistory::No };
    WebCore::LockBackForwardList lockBackForwardList { WebCore::LockBackForwardList::No };
    WebCore::SubstituteData::SessionHistoryVisibility sessionHistoryVisibility { WebCore::SubstituteData::SessionHistoryVisibility::Visible };
    String clientRedirectSourceForHistory;
    WebCore::SandboxFlags effectiveSandboxFlags { 0 };
    std::optional<WebCore::OwnerPermissionsPolicyData> ownerPermissionsPolicy;
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain;
    std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume;
    bool isServiceWorkerLoad { false };

#if PLATFORM(COCOA)
    std::optional<double> dataDetectionReferenceDate;
#endif
    bool isRequestFromClientOrUserInput { false };
    bool isPerformingHTTPFallback { false };

    std::optional<OptionSet<WebCore::AdvancedPrivacyProtections>> advancedPrivacyProtections;
};

} // namespace WebKit
