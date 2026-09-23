/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "NavigationActionData.h"
#include "NetworkActivityTracker.h"
#include "NetworkCacheKey.h"
#include "PolicyDecision.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/ProcessID.h>

namespace WebKit {

enum class PreconnectOnly : bool { No, Yes };

// Present when the load may use a compression dictionary. NetworkDataTask follows redirects itself
// and has to rematch for the new URL, so destination outlives any one match; match is unset when
// nothing matched.
struct CompressionDictionaryParameters {
    struct Match {
        NetworkCache::Key key;
        std::array<uint8_t, 32> hash;
        String id;
    };

    WebCore::FetchOptions::Destination destination { WebCore::FetchOptions::Destination::EmptyString };
    std::optional<Match> match { };
};

struct NetworkLoadParameters {
    Markable<WebPageProxyIdentifier> webPageProxyID;
    Markable<WebCore::PageIdentifier> webPageID;
    Markable<WebCore::FrameIdentifier> webFrameID;
    RefPtr<WebCore::SecurityOrigin> topOrigin;
    RefPtr<WebCore::SecurityOrigin> sourceOrigin;
    WTF::ProcessID parentPID { 0 };
    WebCore::ResourceRequest request;
    WebCore::ContentSniffingPolicy contentSniffingPolicy { WebCore::ContentSniffingPolicy::SniffContent };
    WebCore::ContentEncodingSniffingPolicy contentEncodingSniffingPolicy { WebCore::ContentEncodingSniffingPolicy::Default };
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy { WebCore::StoredCredentialsPolicy::DoNotUse };
    WebCore::ClientCredentialPolicy clientCredentialPolicy { WebCore::ClientCredentialPolicy::CannotAskClientForCredentials };
    bool shouldClearReferrerOnHTTPSToHTTPRedirect { true };
    bool needsCertificateInfo { false };
    bool isMainFrameNavigation { false };
    std::optional<NavigationActionData> mainResourceNavigationDataForAnyFrame { };
    Vector<Ref<WebCore::BlobDataFileReference>> blobFileReferences { };
    PreconnectOnly shouldPreconnectOnly { PreconnectOnly::No };
    std::optional<NetworkActivityTracker> networkActivityTracker { };
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain { NavigatingToAppBoundDomain::No };
    bool hadMainFrameMainResourcePrivateRelayed { false };
    bool allowPrivacyProxy { true };
    OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections { };
    bool isInitiatedByDedicatedWorker { false };

    uint64_t requiredCookiesVersion { 0 };

    // The navigation initiator lost the navigated frame's Storage Access API grant (computed in
    // the WebProcess). We should block storage access cookies on this load's network requests without
    // revoking the frame's storage from JS until the navigation load commits.
    bool navigationLosesFrameSpecificStorageAccess { false };

    std::optional<CompressionDictionaryParameters> compressionDictionary { };
};

} // namespace WebKit
