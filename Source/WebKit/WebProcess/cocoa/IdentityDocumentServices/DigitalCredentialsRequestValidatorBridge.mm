/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DigitalCredentialsRequestValidatorBridge.h"

#import "WKIdentityDocumentRawRequestValidator.h"
#import <Foundation/Foundation.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/DigitalCredentialsProtocols.h>
#import <WebCore/Document.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/ISO18013.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/UnvalidatedDigitalCredentialRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>

namespace WebKit {

using namespace WebCore;

static RetainPtr<SecTrustRef> createSecTrustForChain(const Vector<SecCertificateRef> &chain)
{
    if (chain.isEmpty())
        return nil;

    RetainPtr cfChain = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
    for (auto certificateRef : chain)
        CFArrayAppendValue(cfChain.get(), certificateRef);

    RetainPtr policy = adoptCF(SecPolicyCreateBasicX509());

    SecTrustRef tmpTrust { nil };
    OSStatus status = SecTrustCreateWithCertificates(cfChain.get(), policy.get(), &tmpTrust);

    if (status != errSecSuccess || !tmpTrust)
        return nil;

    return adoptCF(tmpTrust);
}

static Vector<CertificateInfo> buildRequestAuthentications(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    Vector<CertificateInfo> requestAuthentications;

    for (NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *certificateChain in mobileDocumentRequest.authenticationCertificates) {
        Vector<SecCertificateRef> certificateChainVector;
        for (WKIdentityDocumentPresentmentRequestAuthenticationCertificate *certificate in certificateChain)
            certificateChainVector.append(certificate.certificate);

        auto trust = createSecTrustForChain(certificateChainVector);
        CertificateInfo info { WTFMove(trust) };
        requestAuthentications.append(WTFMove(info));
    }

    return requestAuthentications;
}

static ISO18013DocumentRequest buildDocumentRequest(WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *individualDocumentRequest)
{
    ISO18013DocumentRequest mappedDocumentRequest;

    mappedDocumentRequest.documentType = individualDocumentRequest.documentType;

    for (NSString *namespaceKey in individualDocumentRequest.namespaces) {
        String mappedNamespaceKey = namespaceKey;

        using ElementDictionaryType = NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *>;
        RetainPtr<ElementDictionaryType> elementDictionary = individualDocumentRequest.namespaces[namespaceKey];

        ISO18013ElementNamespaceVector innerVector;
        for (NSString *elementIdentifier in elementDictionary.get()) {
            String mappedElementIdentifier = elementIdentifier;
            ISO18013ElementInfo elementInfo { elementDictionary.get()[elementIdentifier].isRetaining };
            innerVector.append(std::make_pair(WTFMove(mappedElementIdentifier), WTFMove(elementInfo)));
        }

        mappedDocumentRequest.namespaces.append(std::make_pair(WTFMove(mappedNamespaceKey), WTFMove(innerVector)));
    }

    return mappedDocumentRequest;
}

static Vector<ISO18013PresentmentRequest> buildPresentmentRequests(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    Vector<ISO18013PresentmentRequest> presentmentRequests;

    for (WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *presentmentRequest in mobileDocumentRequest.presentmentRequests) {
        ISO18013PresentmentRequest mappedPresentmentRequest;
        mappedPresentmentRequest.isMandatory = presentmentRequest.isMandatory;

        for (NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *documentSet in presentmentRequest.documentSets) {
            ISO18013DocumentRequestSet mappedDocumentSet;

            for (WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *documentRequest in documentSet) {
                ISO18013DocumentRequest mappedDocumentRequest = buildDocumentRequest(documentRequest);
                mappedDocumentSet.requests.append(mappedDocumentRequest);
            }

            mappedPresentmentRequest.documentRequestSets.append(WTFMove(mappedDocumentSet));
        }

        presentmentRequests.append(mappedPresentmentRequest);
    }

    return presentmentRequests;
}

static ValidatedMobileDocumentRequest buildValidatedRequest(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    auto requestAuthentications = buildRequestAuthentications(mobileDocumentRequest);
    auto presentmentRequests = buildPresentmentRequests(mobileDocumentRequest);

    ValidatedMobileDocumentRequest validatedRequest;
    validatedRequest.requestAuthentications = requestAuthentications;
    validatedRequest.presentmentRequests = presentmentRequests;

    return validatedRequest;
}

Vector<ValidatedDigitalCredentialRequest> DigitalCredentials::validateRequests(const SecurityOrigin &topOrigin, const Document &document, const Vector<UnvalidatedDigitalCredentialRequest> &unvalidatedRequests)
{
    RetainPtr convertedTopOrigin = topOrigin.toURL().createNSURL().get();
    RetainPtr validator = adoptNS([[WKIdentityDocumentRawRequestValidator alloc] init]);

    Vector<ValidatedDigitalCredentialRequest> validatedRequests;

    for (auto request : unvalidatedRequests) {
        if (!std::holds_alternative<WebCore::MobileDocumentRequest>(request)) {
            LOG(DigitalCredentials, "Incoming unvalidated request is not a supported type.");
            continue;
        }

        auto mobileDocumentRequest = std::get<MobileDocumentRequest>(request);

        RetainPtr convertedEncryptionInfo = mobileDocumentRequest.encryptionInfo.createNSString();
        RetainPtr convertedDeviceRequest = mobileDocumentRequest.deviceRequest.createNSString();

        RetainPtr iso18013Request = adoptNS([[WKISO18013Request alloc] initWithEncryptionInfo:convertedEncryptionInfo.get() deviceRequest:convertedDeviceRequest.get()]);

        auto validatedISORequest = [validator validateISO18013Request:iso18013Request.get() origin:convertedTopOrigin.get()];
        if (validatedISORequest) {
            auto validatedMobileDocumentRequest = buildValidatedRequest(validatedISORequest);
            auto resultVariant = WTF::Variant<ValidatedMobileDocumentRequest, OpenID4VPRequest>(validatedMobileDocumentRequest);
            validatedRequests.append(WTFMove(resultVariant));
        }
    }

    return validatedRequests;
}

}
