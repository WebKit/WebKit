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
#import <JavaScriptCore/ConsoleMessage.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/DigitalCredentialsProtocols.h>
#import <WebCore/Document.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/ISO18013.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/UnvalidatedDigitalCredentialRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import "WebKitSwiftSoftLink.h"

namespace WebKit {

static RetainPtr<SecTrustRef> createSecTrustForChain(const Vector<RetainPtr<SecCertificateRef>> &chain)
{
    if (chain.isEmpty())
        return nullptr;

    RetainPtr cfChain = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));

    for (RetainPtr cert : chain)
        CFArrayAppendValue(cfChain.get(), cert.get());

    SUPPRESS_UNRETAINED_LOCAL RetainPtr policy = adoptCF(SecPolicyCreateBasicX509());

    SecTrustRef rawTrust = nullptr;
    OSStatus status = SecTrustCreateWithCertificates(cfChain.get(), policy.get(), &rawTrust);
    if (status != errSecSuccess || !rawTrust)
        return nullptr;

    return adoptCF(rawTrust);
}

static Vector<WebCore::CertificateInfo> buildRequestAuthentications(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    Vector<WebCore::CertificateInfo> requestAuthentications;

    for (NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *certificateChain in mobileDocumentRequest.authenticationCertificates) {

        Vector<RetainPtr<SecCertificateRef>> certificateChainVector;
        certificateChainVector.reserveInitialCapacity(certificateChain.count);

        for (WKIdentityDocumentPresentmentRequestAuthenticationCertificate *certificate in certificateChain)
            certificateChainVector.append(RetainPtr<SecCertificateRef>(certificate.certificate));

        auto trust = createSecTrustForChain(certificateChainVector);
        requestAuthentications.append(WebCore::CertificateInfo { WTFMove(trust) });
    }

    return requestAuthentications;
}

static WebCore::ISO18013DocumentRequest buildDocumentRequest(WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *individualDocumentRequest)
{
    WebCore::ISO18013DocumentRequest mappedDocumentRequest;

    mappedDocumentRequest.documentType = individualDocumentRequest.documentType;

    for (NSString *namespaceKey in individualDocumentRequest.namespaces) {
        String mappedNamespaceKey = namespaceKey;

        using ElementDictionaryType = NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *>;
        RetainPtr<ElementDictionaryType> elementDictionary = individualDocumentRequest.namespaces[namespaceKey];

        WebCore::ISO18013ElementNamespaceVector innerVector;
        for (NSString *elementIdentifier in elementDictionary.get()) {
            String mappedElementIdentifier = elementIdentifier;
            WebCore::ISO18013ElementInfo elementInfo {
                static_cast<bool>(elementDictionary.get()[elementIdentifier].isRetaining)
            };
            innerVector.append(std::make_pair(WTFMove(mappedElementIdentifier), WTFMove(elementInfo)));
        }

        mappedDocumentRequest.namespaces.append(std::make_pair(WTFMove(mappedNamespaceKey), WTFMove(innerVector)));
    }

    return mappedDocumentRequest;
}

static Vector<WebCore::ISO18013PresentmentRequest> buildPresentmentRequests(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    Vector<WebCore::ISO18013PresentmentRequest> presentmentRequests;

    for (WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *presentmentRequest in mobileDocumentRequest.presentmentRequests) {
        WebCore::ISO18013PresentmentRequest mappedPresentmentRequest;
        mappedPresentmentRequest.isMandatory = presentmentRequest.isMandatory;

        for (NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *documentSet in presentmentRequest.documentSets) {
            WebCore::ISO18013DocumentRequestSet mappedDocumentSet;

            for (WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *documentRequest in documentSet) {
                WebCore::ISO18013DocumentRequest mappedDocumentRequest = buildDocumentRequest(documentRequest);
                mappedDocumentSet.requests.append(mappedDocumentRequest);
            }

            mappedPresentmentRequest.documentRequestSets.append(WTFMove(mappedDocumentSet));
        }

        presentmentRequests.append(mappedPresentmentRequest);
    }

    return presentmentRequests;
}

static WebCore::ValidatedMobileDocumentRequest buildValidatedRequest(WKIdentityDocumentPresentmentMobileDocumentRequest *mobileDocumentRequest)
{
    auto requestAuthentications = buildRequestAuthentications(mobileDocumentRequest);
    auto presentmentRequests = buildPresentmentRequests(mobileDocumentRequest);

    WebCore::ValidatedMobileDocumentRequest validatedRequest;
    validatedRequest.requestAuthentications = requestAuthentications;
    validatedRequest.presentmentRequests = presentmentRequests;
    return validatedRequest;
}

Vector<WebCore::ValidatedDigitalCredentialRequest> DigitalCredentials::validateRequests(const SecurityOrigin &topOrigin, const Document &document, const Vector<UnvalidatedDigitalCredentialRequest> &unvalidatedRequests)
{
    RetainPtr convertedTopOrigin = topOrigin.toURL().createNSURL().get();
    RetainPtr validator = adoptNS([WebKit::allocWKIdentityDocumentRawRequestValidatorInstance() init]);

    Vector<WebCore::ValidatedDigitalCredentialRequest> validatedRequests;

    for (auto request : unvalidatedRequests) {
        if (!std::holds_alternative<WebCore::MobileDocumentRequest>(request)) {
            LOG(DigitalCredentials, "Incoming unvalidated request is not a supported type.");

            const_cast<Document&>(document).addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(
                MessageSource::JS,
                MessageType::Log,
                MessageLevel::Warning,
                "Encountered an unsupported request protocol. The request will be ignored."_s
            ));

            continue;
        }

        auto mobileDocumentRequest = std::get<WebCore::MobileDocumentRequest>(request);

        RetainPtr convertedEncryptionInfo = mobileDocumentRequest.encryptionInfo.createNSString();
        RetainPtr convertedDeviceRequest = mobileDocumentRequest.deviceRequest.createNSString();

        RetainPtr iso18013Request = adoptNS([WebKit::allocWKISO18013RequestInstance() initWithEncryptionInfo:convertedEncryptionInfo.get() deviceRequest:convertedDeviceRequest.get()]);

        NSError *error = nil;
        auto validatedISORequest = [validator validateISO18013Request:iso18013Request.get() origin:convertedTopOrigin.get() error:&error];

        if (validatedISORequest) {
            auto validatedMobileDocumentRequest = buildValidatedRequest(validatedISORequest);
            auto resultVariant = WTF::Variant<WebCore::ValidatedMobileDocumentRequest, WebCore::OpenID4VPRequest>(validatedMobileDocumentRequest);
            validatedRequests.append(WTFMove(resultVariant));
        } else if (error) {
            RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
            String errorMessage = "An error occurred validating the incoming 'org-iso-mdoc' request. The request will be ignored."_s;

            if ([debugDescription length])
                errorMessage = makeString(errorMessage, " ("_s, String(debugDescription.get()), ")"_s);

            const_cast<Document&>(document).addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(
                MessageSource::JS,
                MessageType::Log,
                MessageLevel::Warning,
                errorMessage
            ));

            LOG(DigitalCredentials, "Validation failed for request: %@", error);
        }
    }

    return validatedRequests;
}

}
