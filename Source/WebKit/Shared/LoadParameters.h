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

#include "DataReference.h"
#include "NetworkResourceLoadIdentifier.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "UserData.h"
#include "WebsitePoliciesData.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <WebCore/SubstituteData.h>

OBJC_CLASS NSDictionary;

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
typedef int SandboxFlags;
}

namespace WebKit {

struct LoadParameters {
    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, LoadParameters&);

    void platformEncode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool platformDecode(IPC::Decoder&, LoadParameters&);

    uint64_t navigationID { 0 };

    WebCore::ResourceRequest request;
    SandboxExtension::Handle sandboxExtensionHandle;

    IPC::DataReference data;
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
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain;
    std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume;
    bool isServiceWorkerLoad { false };

#if PLATFORM(COCOA)
    RetainPtr<NSDictionary> dataDetectionContext;
#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    Vector<SandboxExtension::Handle> networkExtensionSandboxExtensionHandles;
#if PLATFORM(IOS)
    std::optional<SandboxExtension::Handle> contentFilterExtensionHandle;
    std::optional<SandboxExtension::Handle> frontboardServiceExtensionHandle;
#endif // PLATFORM(IOS)
#endif // !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
#endif
#if ENABLE(PUBLIC_SUFFIX_LIST)
    String topPrivatelyControlledDomain;
#endif
};

} // namespace WebKit
