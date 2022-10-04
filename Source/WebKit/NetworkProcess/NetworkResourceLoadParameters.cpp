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

RefPtr<SecurityOrigin> NetworkResourceLoadParameters::parentOrigin() const
{
    if (frameAncestorOrigins.isEmpty())
        return nullptr;
    return frameAncestorOrigins.first();
}

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

        Vector<SandboxExtension::Handle> requestBodySandboxExtensions;
        for (const FormDataElement& element : request.httpBody()->elements()) {
            if (auto* fileData = std::get_if<FormDataElement::EncodedFileData>(&element.data)) {
                const String& path = fileData->filename;
                if (auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly))
                    requestBodySandboxExtensions.append(WTFMove(*handle));
            }
        }
        encoder << requestBodySandboxExtensions;
    }

    if (request.url().isLocalFile()) {
        SandboxExtension::Handle requestSandboxExtension;
#if HAVE(AUDIT_TOKEN)
        if (networkProcessAuditToken) {
            if (auto handle = SandboxExtension::createHandleForReadByAuditToken(request.url().fileSystemPath(), *networkProcessAuditToken))
                requestSandboxExtension = WTFMove(*handle);
        } else
#endif
        {
            if (auto handle = SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly))
                requestSandboxExtension = WTFMove(*handle);
        }

        encoder << requestSandboxExtension;
    }

    encoder << contentSniffingPolicy;
    encoder << contentEncodingSniffingPolicy;
    encoder << storedCredentialsPolicy;
    encoder << clientCredentialPolicy;
    encoder << shouldPreconnectOnly;
    encoder << shouldClearReferrerOnHTTPSToHTTPRedirect;
    encoder << needsCertificateInfo;
    encoder << isMainFrameNavigation;
    encoder << isMainResourceNavigationForAnyFrame;
    encoder << shouldRelaxThirdPartyCookieBlocking;
    encoder << maximumBufferingTime;

    encoder << static_cast<bool>(sourceOrigin);
    if (sourceOrigin)
        encoder << *sourceOrigin;
    encoder << static_cast<bool>(topOrigin);
    if (sourceOrigin)
        encoder << *topOrigin;
    encoder << options;
    encoder << cspResponseHeaders;
    encoder << parentFrameURL;
    encoder << frameURL;
    encoder << parentCrossOriginEmbedderPolicy;
    encoder << crossOriginEmbedderPolicy;
    encoder << originalRequestHeaders;

    encoder << shouldRestrictHTTPResponseAccess;

    encoder << preflightPolicy;

    encoder << shouldEnableCrossOriginResourcePolicy;

    encoder << frameAncestorOrigins;
    encoder << pageHasResourceLoadClient;
    encoder << parentFrameID;
    encoder << crossOriginAccessControlCheckEnabled;
    encoder << networkConnectionIntegrityEnabled;
    encoder << allowPrivacyProxy;

    encoder << documentURL;

    encoder << isCrossOriginOpenerPolicyEnabled;
    encoder << isDisplayingInitialEmptyDocument;
    encoder << effectiveSandboxFlags;
    encoder << openerURL;
    encoder << sourceCrossOriginOpenerPolicy;

    encoder << navigationID;
    encoder << navigationRequester;

#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkersMode;
    encoder << serviceWorkerRegistrationIdentifier;
    encoder << httpHeadersToKeep;
    encoder << navigationPreloadIdentifier;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    encoder << mainDocumentURL;
    encoder << userContentControllerIdentifier;
#endif
    
    encoder << isNavigatingToAppBoundDomain;
    encoder << hadMainFrameMainResourcePrivateRelayed;
}

std::optional<NetworkResourceLoadParameters> NetworkResourceLoadParameters::decode(IPC::Decoder& decoder)
{
    NetworkResourceLoadParameters result;

    if (!decoder.decode(result.identifier))
        return std::nullopt;
        
    std::optional<WebPageProxyIdentifier> webPageProxyID;
    decoder >> webPageProxyID;
    if (!webPageProxyID)
        return std::nullopt;
    result.webPageProxyID = *webPageProxyID;

    std::optional<PageIdentifier> webPageID;
    decoder >> webPageID;
    if (!webPageID)
        return std::nullopt;
    result.webPageID = *webPageID;

    if (!decoder.decode(result.webFrameID))
        return std::nullopt;

    if (!decoder.decode(result.parentPID))
        return std::nullopt;

    if (!decoder.decode(result.request))
        return std::nullopt;

    bool hasHTTPBody;
    if (!decoder.decode(hasHTTPBody))
        return std::nullopt;

    if (hasHTTPBody) {
        RefPtr<FormData> formData = FormData::decode(decoder);
        if (!formData)
            return std::nullopt;
        result.request.setHTTPBody(WTFMove(formData));

        std::optional<Vector<SandboxExtension::Handle>> requestBodySandboxExtensionHandles;
        decoder >> requestBodySandboxExtensionHandles;
        if (!requestBodySandboxExtensionHandles)
            return std::nullopt;
        for (auto& handle : WTFMove(*requestBodySandboxExtensionHandles)) {
            if (auto extension = SandboxExtension::create(WTFMove(handle)))
                result.requestBodySandboxExtensions.append(WTFMove(extension));
        }
    }

    if (result.request.url().isLocalFile()) {
        std::optional<SandboxExtension::Handle> resourceSandboxExtensionHandle;
        decoder >> resourceSandboxExtensionHandle;
        if (!resourceSandboxExtensionHandle)
            return std::nullopt;
        result.resourceSandboxExtension = SandboxExtension::create(WTFMove(*resourceSandboxExtensionHandle));
    }

    if (!decoder.decode(result.contentSniffingPolicy))
        return std::nullopt;
    if (!decoder.decode(result.contentEncodingSniffingPolicy))
        return std::nullopt;
    if (!decoder.decode(result.storedCredentialsPolicy))
        return std::nullopt;
    if (!decoder.decode(result.clientCredentialPolicy))
        return std::nullopt;
    if (!decoder.decode(result.shouldPreconnectOnly))
        return std::nullopt;
    if (!decoder.decode(result.shouldClearReferrerOnHTTPSToHTTPRedirect))
        return std::nullopt;
    if (!decoder.decode(result.needsCertificateInfo))
        return std::nullopt;
    if (!decoder.decode(result.isMainFrameNavigation))
        return std::nullopt;
    if (!decoder.decode(result.isMainResourceNavigationForAnyFrame))
        return std::nullopt;
    if (!decoder.decode(result.shouldRelaxThirdPartyCookieBlocking))
        return std::nullopt;
    if (!decoder.decode(result.maximumBufferingTime))
        return std::nullopt;

    bool hasSourceOrigin;
    if (!decoder.decode(hasSourceOrigin))
        return std::nullopt;
    if (hasSourceOrigin) {
        result.sourceOrigin = SecurityOrigin::decode(decoder);
        if (!result.sourceOrigin)
            return std::nullopt;
    }

    bool hasTopOrigin;
    if (!decoder.decode(hasTopOrigin))
        return std::nullopt;
    if (hasTopOrigin) {
        result.topOrigin = SecurityOrigin::decode(decoder);
        if (!result.topOrigin)
            return std::nullopt;
    }

    std::optional<FetchOptions> options;
    decoder >> options;
    if (!options)
        return std::nullopt;
    result.options = *options;

    if (!decoder.decode(result.cspResponseHeaders))
        return std::nullopt;

    std::optional<URL> parentFrameURL;
    decoder >> parentFrameURL;
    if (!parentFrameURL)
        return std::nullopt;
    result.parentFrameURL = WTFMove(*parentFrameURL);

    std::optional<URL> frameURL;
    decoder >> frameURL;
    if (!frameURL)
        return std::nullopt;
    result.frameURL = WTFMove(*frameURL);

    std::optional<WebCore::CrossOriginEmbedderPolicy> parentCrossOriginEmbedderPolicy;
    decoder >> parentCrossOriginEmbedderPolicy;
    if (!parentCrossOriginEmbedderPolicy)
        return std::nullopt;
    result.parentCrossOriginEmbedderPolicy = WTFMove(*parentCrossOriginEmbedderPolicy);

    std::optional<WebCore::CrossOriginEmbedderPolicy> crossOriginEmbedderPolicy;
    decoder >> crossOriginEmbedderPolicy;
    if (!crossOriginEmbedderPolicy)
        return std::nullopt;
    result.crossOriginEmbedderPolicy = WTFMove(*crossOriginEmbedderPolicy);

    if (!decoder.decode(result.originalRequestHeaders))
        return std::nullopt;

    std::optional<bool> shouldRestrictHTTPResponseAccess;
    decoder >> shouldRestrictHTTPResponseAccess;
    if (!shouldRestrictHTTPResponseAccess)
        return std::nullopt;
    result.shouldRestrictHTTPResponseAccess = *shouldRestrictHTTPResponseAccess;

    if (!decoder.decode(result.preflightPolicy))
        return std::nullopt;

    std::optional<bool> shouldEnableCrossOriginResourcePolicy;
    decoder >> shouldEnableCrossOriginResourcePolicy;
    if (!shouldEnableCrossOriginResourcePolicy)
        return std::nullopt;
    result.shouldEnableCrossOriginResourcePolicy = *shouldEnableCrossOriginResourcePolicy;

    if (!decoder.decode(result.frameAncestorOrigins))
        return std::nullopt;

    std::optional<bool> pageHasResourceLoadClient;
    decoder >> pageHasResourceLoadClient;
    if (!pageHasResourceLoadClient)
        return std::nullopt;
    result.pageHasResourceLoadClient = *pageHasResourceLoadClient;
    
    std::optional<std::optional<FrameIdentifier>> parentFrameID;
    decoder >> parentFrameID;
    if (!parentFrameID)
        return std::nullopt;
    result.parentFrameID = WTFMove(*parentFrameID);

    std::optional<bool> crossOriginAccessControlCheckEnabled;
    decoder >> crossOriginAccessControlCheckEnabled;
    if (!crossOriginAccessControlCheckEnabled)
        return std::nullopt;
    result.crossOriginAccessControlCheckEnabled = *crossOriginAccessControlCheckEnabled;

    std::optional<bool> networkConnectionIntegrityEnabled;
    decoder >> networkConnectionIntegrityEnabled;
    if (!networkConnectionIntegrityEnabled)
        return std::nullopt;
    result.networkConnectionIntegrityEnabled = *networkConnectionIntegrityEnabled;

    std::optional<bool> allowPrivacyProxy;
    decoder >> allowPrivacyProxy;
    if (!allowPrivacyProxy)
        return std::nullopt;
    result.allowPrivacyProxy = *allowPrivacyProxy;

    std::optional<URL> documentURL;
    decoder >> documentURL;
    if (!documentURL)
        return std::nullopt;
    result.documentURL = *documentURL;

    std::optional<bool> isCrossOriginOpenerPolicyEnabled;
    decoder >> isCrossOriginOpenerPolicyEnabled;
    if (!isCrossOriginOpenerPolicyEnabled)
        return std::nullopt;
    result.isCrossOriginOpenerPolicyEnabled = *isCrossOriginOpenerPolicyEnabled;

    std::optional<bool> isDisplayingInitialEmptyDocument;
    decoder >> isDisplayingInitialEmptyDocument;
    if (!isDisplayingInitialEmptyDocument)
        return std::nullopt;
    result.isDisplayingInitialEmptyDocument = *isDisplayingInitialEmptyDocument;

    std::optional<SandboxFlags> effectiveSandboxFlags;
    decoder >> effectiveSandboxFlags;
    if (!effectiveSandboxFlags)
        return std::nullopt;
    result.effectiveSandboxFlags = *effectiveSandboxFlags;

    std::optional<URL> openerURL;
    decoder >> openerURL;
    if (!openerURL)
        return std::nullopt;
    result.openerURL = WTFMove(*openerURL);

    std::optional<CrossOriginOpenerPolicy> sourceCrossOriginOpenerPolicy;
    decoder >> sourceCrossOriginOpenerPolicy;
    if (!sourceCrossOriginOpenerPolicy)
        return std::nullopt;
    result.sourceCrossOriginOpenerPolicy = WTFMove(*sourceCrossOriginOpenerPolicy);

    std::optional<uint64_t> navigationID;
    decoder >> navigationID;
    if (!navigationID)
        return std::nullopt;
    result.navigationID = *navigationID;

    std::optional<std::optional<NavigationRequester>> navigationRequester;
    decoder >> navigationRequester;
    if (!navigationRequester)
        return std::nullopt;

    result.navigationRequester = WTFMove(*navigationRequester);

#if ENABLE(SERVICE_WORKER)
    std::optional<ServiceWorkersMode> serviceWorkersMode;
    decoder >> serviceWorkersMode;
    if (!serviceWorkersMode)
        return std::nullopt;
    result.serviceWorkersMode = *serviceWorkersMode;

    std::optional<std::optional<ServiceWorkerRegistrationIdentifier>> serviceWorkerRegistrationIdentifier;
    decoder >> serviceWorkerRegistrationIdentifier;
    if (!serviceWorkerRegistrationIdentifier)
        return std::nullopt;
    result.serviceWorkerRegistrationIdentifier = *serviceWorkerRegistrationIdentifier;

    std::optional<OptionSet<HTTPHeadersToKeepFromCleaning>> httpHeadersToKeep;
    decoder >> httpHeadersToKeep;
    if (!httpHeadersToKeep)
        return std::nullopt;
    result.httpHeadersToKeep = WTFMove(*httpHeadersToKeep);

    std::optional<std::optional<FetchIdentifier>> navigationPreloadIdentifier;
    decoder >> navigationPreloadIdentifier;
    if (!navigationPreloadIdentifier)
        return std::nullopt;
    result.navigationPreloadIdentifier = *navigationPreloadIdentifier;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    if (!decoder.decode(result.mainDocumentURL))
        return std::nullopt;

    std::optional<std::optional<UserContentControllerIdentifier>> userContentControllerIdentifier;
    decoder >> userContentControllerIdentifier;
    if (!userContentControllerIdentifier)
        return std::nullopt;
    result.userContentControllerIdentifier = *userContentControllerIdentifier;
#endif

    std::optional<std::optional<NavigatingToAppBoundDomain>> isNavigatingToAppBoundDomain;
    decoder >> isNavigatingToAppBoundDomain;
    if (!isNavigatingToAppBoundDomain)
        return std::nullopt;
    result.isNavigatingToAppBoundDomain = *isNavigatingToAppBoundDomain;
    
    std::optional<bool> hadMainFrameMainResourcePrivateRelayed;
    decoder >> hadMainFrameMainResourcePrivateRelayed;
    if (!hadMainFrameMainResourcePrivateRelayed)
        return std::nullopt;
    result.hadMainFrameMainResourcePrivateRelayed = *hadMainFrameMainResourcePrivateRelayed;

    return result;
}
    
} // namespace WebKit
