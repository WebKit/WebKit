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

#include "WebCoreArgumentCoders.h"

namespace WebKit {
using namespace WebCore;

void NetworkResourceLoadParameters::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << webPageProxyID;
    encoder << webPageID;
    encoder << webFrameID;
    encoder << parentPID;
    encoder << request;

    encoder << static_cast<bool>(request.httpBody());
    if (request.httpBody()) {
        request.httpBody()->encode(encoder);

        const Vector<FormDataElement>& elements = request.httpBody()->elements();
        size_t fileCount = 0;
        for (size_t i = 0, count = elements.size(); i < count; ++i) {
            if (WTF::holds_alternative<FormDataElement::EncodedFileData>(elements[i].data))
                ++fileCount;
        }

        SandboxExtension::HandleArray requestBodySandboxExtensions;
        requestBodySandboxExtensions.allocate(fileCount);
        size_t extensionIndex = 0;
        for (size_t i = 0, count = elements.size(); i < count; ++i) {
            const FormDataElement& element = elements[i];
            if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
                const String& path = fileData->filename;
                SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly, requestBodySandboxExtensions[extensionIndex++]);
            }
        }
        encoder << requestBodySandboxExtensions;
    }

    if (request.url().isLocalFile()) {
        SandboxExtension::Handle requestSandboxExtension;
#if HAVE(SANDBOX_ISSUE_READ_EXTENSION_TO_PROCESS_BY_AUDIT_TOKEN)
        if (networkProcessAuditToken)
            SandboxExtension::createHandleForReadByAuditToken(request.url().fileSystemPath(), *networkProcessAuditToken, requestSandboxExtension);
        else
            SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly, requestSandboxExtension);
#else
        SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly, requestSandboxExtension);
#endif
        encoder << requestSandboxExtension;
    }

    encoder.encodeEnum(contentSniffingPolicy);
    encoder.encodeEnum(contentEncodingSniffingPolicy);
    encoder.encodeEnum(storedCredentialsPolicy);
    encoder.encodeEnum(clientCredentialPolicy);
    encoder.encodeEnum(shouldPreconnectOnly);
    encoder << shouldClearReferrerOnHTTPSToHTTPRedirect;
    encoder << needsCertificateInfo;
    encoder << isMainFrameNavigation;
    encoder << isMainResourceNavigationForAnyFrame;
    encoder << maximumBufferingTime;

    encoder << static_cast<bool>(sourceOrigin);
    if (sourceOrigin)
        encoder << *sourceOrigin;
    encoder << static_cast<bool>(topOrigin);
    if (sourceOrigin)
        encoder << *topOrigin;
    encoder << options;
    encoder << cspResponseHeaders;
    encoder << originalRequestHeaders;

    encoder << shouldRestrictHTTPResponseAccess;

    encoder.encodeEnum(preflightPolicy);

    encoder << shouldEnableCrossOriginResourcePolicy;

    encoder << frameAncestorOrigins;
    encoder << isHTTPSUpgradeEnabled;
    encoder << pageHasResourceLoadClient;
    encoder << parentFrameID;

#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkersMode;
    encoder << serviceWorkerRegistrationIdentifier;
    encoder << httpHeadersToKeep;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    encoder << mainDocumentURL;
    encoder << userContentControllerIdentifier;
#endif
}

Optional<NetworkResourceLoadParameters> NetworkResourceLoadParameters::decode(IPC::Decoder& decoder)
{
    NetworkResourceLoadParameters result;

    if (!decoder.decode(result.identifier))
        return WTF::nullopt;
        
    Optional<WebPageProxyIdentifier> webPageProxyID;
    decoder >> webPageProxyID;
    if (!webPageProxyID)
        return WTF::nullopt;
    result.webPageProxyID = *webPageProxyID;

    Optional<PageIdentifier> webPageID;
    decoder >> webPageID;
    if (!webPageID)
        return WTF::nullopt;
    result.webPageID = *webPageID;

    if (!decoder.decode(result.webFrameID))
        return WTF::nullopt;

    if (!decoder.decode(result.parentPID))
        return WTF::nullopt;

    if (!decoder.decode(result.request))
        return WTF::nullopt;

    bool hasHTTPBody;
    if (!decoder.decode(hasHTTPBody))
        return WTF::nullopt;

    if (hasHTTPBody) {
        RefPtr<FormData> formData = FormData::decode(decoder);
        if (!formData)
            return WTF::nullopt;
        result.request.setHTTPBody(WTFMove(formData));

        Optional<SandboxExtension::HandleArray> requestBodySandboxExtensionHandles;
        decoder >> requestBodySandboxExtensionHandles;
        if (!requestBodySandboxExtensionHandles)
            return WTF::nullopt;
        for (size_t i = 0; i < requestBodySandboxExtensionHandles->size(); ++i) {
            if (auto extension = SandboxExtension::create(WTFMove(requestBodySandboxExtensionHandles->at(i))))
                result.requestBodySandboxExtensions.append(WTFMove(extension));
        }
    }

    if (result.request.url().isLocalFile()) {
        Optional<SandboxExtension::Handle> resourceSandboxExtensionHandle;
        decoder >> resourceSandboxExtensionHandle;
        if (!resourceSandboxExtensionHandle)
            return WTF::nullopt;
        result.resourceSandboxExtension = SandboxExtension::create(WTFMove(*resourceSandboxExtensionHandle));
    }

    if (!decoder.decodeEnum(result.contentSniffingPolicy))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.contentEncodingSniffingPolicy))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.storedCredentialsPolicy))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.clientCredentialPolicy))
        return WTF::nullopt;
    if (!decoder.decodeEnum(result.shouldPreconnectOnly))
        return WTF::nullopt;
    if (!decoder.decode(result.shouldClearReferrerOnHTTPSToHTTPRedirect))
        return WTF::nullopt;
    if (!decoder.decode(result.needsCertificateInfo))
        return WTF::nullopt;
    if (!decoder.decode(result.isMainFrameNavigation))
        return WTF::nullopt;
    if (!decoder.decode(result.isMainResourceNavigationForAnyFrame))
        return WTF::nullopt;
    if (!decoder.decode(result.maximumBufferingTime))
        return WTF::nullopt;

    bool hasSourceOrigin;
    if (!decoder.decode(hasSourceOrigin))
        return WTF::nullopt;
    if (hasSourceOrigin) {
        result.sourceOrigin = SecurityOrigin::decode(decoder);
        if (!result.sourceOrigin)
            return WTF::nullopt;
    }

    bool hasTopOrigin;
    if (!decoder.decode(hasTopOrigin))
        return WTF::nullopt;
    if (hasTopOrigin) {
        result.topOrigin = SecurityOrigin::decode(decoder);
        if (!result.topOrigin)
            return WTF::nullopt;
    }

    Optional<FetchOptions> options;
    decoder >> options;
    if (!options)
        return WTF::nullopt;
    result.options = *options;

    if (!decoder.decode(result.cspResponseHeaders))
        return WTF::nullopt;
    if (!decoder.decode(result.originalRequestHeaders))
        return WTF::nullopt;

    Optional<bool> shouldRestrictHTTPResponseAccess;
    decoder >> shouldRestrictHTTPResponseAccess;
    if (!shouldRestrictHTTPResponseAccess)
        return WTF::nullopt;
    result.shouldRestrictHTTPResponseAccess = *shouldRestrictHTTPResponseAccess;

    if (!decoder.decodeEnum(result.preflightPolicy))
        return WTF::nullopt;

    Optional<bool> shouldEnableCrossOriginResourcePolicy;
    decoder >> shouldEnableCrossOriginResourcePolicy;
    if (!shouldEnableCrossOriginResourcePolicy)
        return WTF::nullopt;
    result.shouldEnableCrossOriginResourcePolicy = *shouldEnableCrossOriginResourcePolicy;

    if (!decoder.decode(result.frameAncestorOrigins))
        return WTF::nullopt;

    Optional<bool> isHTTPSUpgradeEnabled;
    decoder >> isHTTPSUpgradeEnabled;
    if (!isHTTPSUpgradeEnabled)
        return WTF::nullopt;
    result.isHTTPSUpgradeEnabled = *isHTTPSUpgradeEnabled;

    Optional<bool> pageHasResourceLoadClient;
    decoder >> pageHasResourceLoadClient;
    if (!pageHasResourceLoadClient)
        return WTF::nullopt;
    result.pageHasResourceLoadClient = *pageHasResourceLoadClient;
    
    Optional<Optional<FrameIdentifier>> parentFrameID;
    decoder >> parentFrameID;
    if (!parentFrameID)
        return WTF::nullopt;
    result.parentFrameID = WTFMove(*parentFrameID);

#if ENABLE(SERVICE_WORKER)
    Optional<ServiceWorkersMode> serviceWorkersMode;
    decoder >> serviceWorkersMode;
    if (!serviceWorkersMode)
        return WTF::nullopt;
    result.serviceWorkersMode = *serviceWorkersMode;

    Optional<Optional<ServiceWorkerRegistrationIdentifier>> serviceWorkerRegistrationIdentifier;
    decoder >> serviceWorkerRegistrationIdentifier;
    if (!serviceWorkerRegistrationIdentifier)
        return WTF::nullopt;
    result.serviceWorkerRegistrationIdentifier = *serviceWorkerRegistrationIdentifier;

    Optional<OptionSet<HTTPHeadersToKeepFromCleaning>> httpHeadersToKeep;
    decoder >> httpHeadersToKeep;
    if (!httpHeadersToKeep)
        return WTF::nullopt;
    result.httpHeadersToKeep = WTFMove(*httpHeadersToKeep);
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    if (!decoder.decode(result.mainDocumentURL))
        return WTF::nullopt;

    Optional<Optional<UserContentControllerIdentifier>> userContentControllerIdentifier;
    decoder >> userContentControllerIdentifier;
    if (!userContentControllerIdentifier)
        return WTF::nullopt;
    result.userContentControllerIdentifier = *userContentControllerIdentifier;
#endif

    return result;
}
    
} // namespace WebKit
