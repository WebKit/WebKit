/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkResourceLoadParameters.h"

#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/RuntimeApplicationChecks.h>

namespace WebKit {
using namespace WebCore;

void NetworkResourceLoadParameters::createSandboxExtensionHandlesIfNecessary()
{
    if (!request.url().protocolIsFile())
        return;

    String path = request.url().fileSystemPath();
    // Log the file-load sandbox extension mint outcome at release level for diagnosis when it fails.
#if HAVE(AUDIT_TOKEN)
    if (auto networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken()) {
        auto handle = SandboxExtension::createHandleForReadByAuditToken(path, *networkProcessAuditToken);
        if (handle)
            resourceSandboxExtension = WTF::move(*handle);
        else
            RELEASE_LOG_ERROR(Sandbox, "NetworkResourceLoadParameters::createSandboxExtensionHandlesIfNecessary: file load, createHandleForReadByAuditToken(networkProcess) failed for '%" PRIVATE_LOG_STRING "'", path.utf8().data());
        return;
    }
    RELEASE_LOG_ERROR(Sandbox, "NetworkResourceLoadParameters::createSandboxExtensionHandlesIfNecessary: file load, network process audit token unavailable; falling back to non-targeted read handle for '%" PRIVATE_LOG_STRING "'", path.utf8().data());
#endif
    auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly);
    if (handle)
        resourceSandboxExtension = WTF::move(*handle);
    else
        RELEASE_LOG_ERROR(Sandbox, "NetworkResourceLoadParameters::createSandboxExtensionHandlesIfNecessary: file load, createHandle(ReadOnly) failed for '%" PRIVATE_LOG_STRING "'", path.utf8().data());
}

RefPtr<SecurityOrigin> NetworkResourceLoadParameters::parentOrigin() const
{
    if (frameAncestorOrigins.isEmpty())
        return nullptr;
    return frameAncestorOrigins.first().ptr();
}

SecurityOriginData NetworkResourceLoadParameters::topOriginForServiceWorkers(const URL& requestURL) const
{
    if (isMainFrameNavigation) {
        auto url = requestURL.protocolIsBlob() ? URL { requestURL.path().toString() } : requestURL;
        return SecurityOriginData::fromURLWithoutStrictOpaqueness(url);
    }
    return topOrigin->data();
}

NetworkLoadParameters NetworkResourceLoadParameters::networkLoadParameters() const
{
    return {
        webPageProxyID,
        webPageID,
        webFrameID,
        topOrigin,
        sourceOrigin,
        parentPID,
        request,
        contentSniffingPolicy,
        contentEncodingSniffingPolicy,
        storedCredentialsPolicy,
        clientCredentialPolicy,
        shouldClearReferrerOnHTTPSToHTTPRedirect,
        needsCertificateInfo,
        isMainFrameNavigation,
        mainResourceNavigationDataForAnyFrame,
        { }, // blobFileReferences
        shouldPreconnectOnly,
        std::nullopt, // networkActivityTracker
        isNavigatingToAppBoundDomain,
        hadMainFrameMainResourcePrivateRelayed,
        allowPrivacyProxy,
        advancedPrivacyProtections,
        isInitiatedByDedicatedWorker,
        requiredCookiesVersion,
        navigationLosesFrameSpecificStorageAccess
    };
}

} // namespace WebKit
