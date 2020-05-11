/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "DownloadID.h"
#include "NavigatingToAppBoundDomain.h"
#include "SandboxExtension.h"
#include "WebsitePoliciesData.h"
#include <wtf/Forward.h>

namespace WebKit {

struct PolicyDecision {
    WebCore::PolicyCheckIdentifier identifier { };
    Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain { WTF::nullopt };
    WebCore::PolicyAction policyAction { WebCore::PolicyAction::Ignore };
    uint64_t navigationID { 0 };
    DownloadID downloadID { 0 };
    Optional<WebsitePoliciesData> websitePoliciesData { WTF::nullopt };
    Optional<SandboxExtension::Handle> sandboxExtensionHandle { WTF::nullopt };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << identifier;
        encoder << isNavigatingToAppBoundDomain;
        encoder << policyAction;
        encoder << navigationID;
        encoder << downloadID;
        encoder << websitePoliciesData;
        encoder << sandboxExtensionHandle;
    }

    template<class Decoder>
    static Optional<PolicyDecision> decode(Decoder& decoder)
    {
        Optional<WebCore::PolicyCheckIdentifier> decodedIdentifier;
        decoder >> decodedIdentifier;
        if (!decodedIdentifier)
            return WTF::nullopt;
        
        Optional<Optional<NavigatingToAppBoundDomain>> decodedIsNavigatingToAppBoundDomain;
        decoder >> decodedIsNavigatingToAppBoundDomain;
        if (!decodedIsNavigatingToAppBoundDomain)
            return WTF::nullopt;

        Optional<WebCore::PolicyAction> decodedPolicyAction;
        decoder >> decodedPolicyAction;
        if (!decodedPolicyAction)
            return WTF::nullopt;

        Optional<uint64_t> decodedNavigationID;
        decoder >> decodedNavigationID;
        if (!decodedNavigationID)
            return WTF::nullopt;

        Optional<DownloadID> decodedDownloadID;
        decoder >> decodedDownloadID;
        if (!decodedDownloadID)
            return WTF::nullopt;

        Optional<Optional<WebsitePoliciesData>> decodedWebsitePoliciesData;
        decoder >> decodedWebsitePoliciesData;
        if (!decodedWebsitePoliciesData)
            return WTF::nullopt;

        Optional<Optional<SandboxExtension::Handle>> sandboxExtensionHandle;
        decoder >> sandboxExtensionHandle;
        if (!sandboxExtensionHandle)
            return WTF::nullopt;

        return {{ WTFMove(*decodedIdentifier), WTFMove(*decodedIsNavigatingToAppBoundDomain), WTFMove(*decodedPolicyAction), WTFMove(*decodedNavigationID), WTFMove(*decodedDownloadID), WTFMove(*decodedWebsitePoliciesData), WTFMove(*sandboxExtensionHandle)}};
    }
};

} // namespace WebKit
