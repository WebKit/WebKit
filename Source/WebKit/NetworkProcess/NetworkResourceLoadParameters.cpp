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
    encoder << webPageID;
    encoder << webFrameID;
    encoder << sessionID;
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
                const String& path = fileData->shouldGenerateFile ? fileData->generatedFilename : fileData->filename;
                SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly, requestBodySandboxExtensions[extensionIndex++]);
            }
        }
        encoder << requestBodySandboxExtensions;
    }

    if (request.url().isLocalFile()) {
        SandboxExtension::Handle requestSandboxExtension;
        SandboxExtension::createHandle(request.url().fileSystemPath(), SandboxExtension::Type::ReadOnly, requestSandboxExtension);
        encoder << requestSandboxExtension;
    }

    encoder.encodeEnum(contentSniffingPolicy);
    encoder.encodeEnum(contentEncodingSniffingPolicy);
    encoder.encodeEnum(storedCredentialsPolicy);
    encoder.encodeEnum(clientCredentialPolicy);
    encoder.encodeEnum(shouldPreconnectOnly);
    encoder << shouldClearReferrerOnHTTPSToHTTPRedirect;
    encoder << defersLoading;
    encoder << needsCertificateInfo;
    encoder << isMainFrameNavigation;
    encoder << maximumBufferingTime;

    encoder << static_cast<bool>(sourceOrigin);
    if (sourceOrigin)
        encoder << *sourceOrigin;
    encoder << options;
    encoder << cspResponseHeaders;
    encoder << originalRequestHeaders;

    encoder << shouldRestrictHTTPResponseAccess;

    encoder.encodeEnum(preflightPolicy);

    encoder << shouldEnableCrossOriginResourcePolicy;

    encoder << frameAncestorOrigins;

#if ENABLE(CONTENT_EXTENSIONS)
    encoder << mainDocumentURL;
    encoder << userContentControllerIdentifier;
#endif
}

bool NetworkResourceLoadParameters::decode(IPC::Decoder& decoder, NetworkResourceLoadParameters& result)
{
    if (!decoder.decode(result.identifier))
        return false;

    if (!decoder.decode(result.webPageID))
        return false;

    if (!decoder.decode(result.webFrameID))
        return false;

    if (!decoder.decode(result.sessionID))
        return false;

    if (!decoder.decode(result.request))
        return false;

    bool hasHTTPBody;
    if (!decoder.decode(hasHTTPBody))
        return false;

    if (hasHTTPBody) {
        RefPtr<FormData> formData = FormData::decode(decoder);
        if (!formData)
            return false;
        result.request.setHTTPBody(WTFMove(formData));

        Optional<SandboxExtension::HandleArray> requestBodySandboxExtensionHandles;
        decoder >> requestBodySandboxExtensionHandles;
        if (!requestBodySandboxExtensionHandles)
            return false;
        for (size_t i = 0; i < requestBodySandboxExtensionHandles->size(); ++i) {
            if (auto extension = SandboxExtension::create(WTFMove(requestBodySandboxExtensionHandles->at(i))))
                result.requestBodySandboxExtensions.append(WTFMove(extension));
        }
    }

    if (result.request.url().isLocalFile()) {
        Optional<SandboxExtension::Handle> resourceSandboxExtensionHandle;
        decoder >> resourceSandboxExtensionHandle;
        if (!resourceSandboxExtensionHandle)
            return false;
        result.resourceSandboxExtension = SandboxExtension::create(WTFMove(*resourceSandboxExtensionHandle));
    }

    if (!decoder.decodeEnum(result.contentSniffingPolicy))
        return false;
    if (!decoder.decodeEnum(result.contentEncodingSniffingPolicy))
        return false;
    if (!decoder.decodeEnum(result.storedCredentialsPolicy))
        return false;
    if (!decoder.decodeEnum(result.clientCredentialPolicy))
        return false;
    if (!decoder.decodeEnum(result.shouldPreconnectOnly))
        return false;
    if (!decoder.decode(result.shouldClearReferrerOnHTTPSToHTTPRedirect))
        return false;
    if (!decoder.decode(result.defersLoading))
        return false;
    if (!decoder.decode(result.needsCertificateInfo))
        return false;
    if (!decoder.decode(result.isMainFrameNavigation))
        return false;
    if (!decoder.decode(result.maximumBufferingTime))
        return false;

    bool hasSourceOrigin;
    if (!decoder.decode(hasSourceOrigin))
        return false;
    if (hasSourceOrigin) {
        result.sourceOrigin = SecurityOrigin::decode(decoder);
        if (!result.sourceOrigin)
            return false;
    }

    Optional<FetchOptions> options;
    decoder >> options;
    if (!options)
        return false;
    result.options = *options;

    if (!decoder.decode(result.cspResponseHeaders))
        return false;
    if (!decoder.decode(result.originalRequestHeaders))
        return false;

    Optional<bool> shouldRestrictHTTPResponseAccess;
    decoder >> shouldRestrictHTTPResponseAccess;
    if (!shouldRestrictHTTPResponseAccess)
        return false;
    result.shouldRestrictHTTPResponseAccess = *shouldRestrictHTTPResponseAccess;

    if (!decoder.decodeEnum(result.preflightPolicy))
        return false;

    Optional<bool> shouldEnableCrossOriginResourcePolicy;
    decoder >> shouldEnableCrossOriginResourcePolicy;
    if (!shouldEnableCrossOriginResourcePolicy)
        return false;
    result.shouldEnableCrossOriginResourcePolicy = *shouldEnableCrossOriginResourcePolicy;

    if (!decoder.decode(result.frameAncestorOrigins))
        return false;
    
#if ENABLE(CONTENT_EXTENSIONS)
    if (!decoder.decode(result.mainDocumentURL))
        return false;

    Optional<Optional<UserContentControllerIdentifier>> userContentControllerIdentifier;
    decoder >> userContentControllerIdentifier;
    if (!userContentControllerIdentifier)
        return false;
    result.userContentControllerIdentifier = *userContentControllerIdentifier;
#endif

    return true;
}
    
} // namespace WebKit
