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

#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <wtf/RuntimeApplicationChecks.h>

namespace WebKit {
using namespace WebCore;

void NetworkResourceLoadParameters::createSandboxExtensionHandlesIfNecessary()
{
    if (request.httpBody()) {
        for (const FormDataElement& element : request.httpBody()->elements()) {
            auto* fileData = std::get_if<FormDataElement::EncodedFileData>(&element.data);
            if (!fileData)
                continue;
            const String& path = fileData->filename;
            if (auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly))
                requestBodySandboxExtensions.append(WTFMove(*handle));
        }
    }

    if (request.url().protocolIsFile()) {
#if HAVE(AUDIT_TOKEN)
        if (auto networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken()) {
            if (auto handle = SandboxExtension::createHandleForReadByAuditToken(request.url().fileSystemPath(), *networkProcessAuditToken))
                resourceSandboxExtension = WTFMove(*handle);
        } else
#endif
        {
            if (auto handle = SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly))
                resourceSandboxExtension = WTFMove(*handle);
        }
    }
}

RefPtr<SecurityOrigin> NetworkResourceLoadParameters::parentOrigin() const
{
    if (frameAncestorOrigins.isEmpty())
        return nullptr;
    return frameAncestorOrigins.first().ptr();
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
        requiredCookiesVersion
    };
}

} // namespace WebKit
