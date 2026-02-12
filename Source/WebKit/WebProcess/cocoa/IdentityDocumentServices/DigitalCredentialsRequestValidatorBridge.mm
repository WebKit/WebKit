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

#if ENABLE(WEB_AUTHN)

#import "Logging.h"
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
#import <wtf/Box.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import "WebKitSwiftSoftLink.h"

namespace WebKit {
using namespace WebCore;

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
        requestAuthentications.append(WebCore::CertificateInfo { WTF::move(trust) });
    }

    return requestAuthentications;
}

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
static WebCore::ISO18013Any convertObjCValueToISO18013Any(id value)
{
    WebCore::ISO18013Any result;

    if ([value isKindOfClass:[NSString class]]) {
        result.data = String(static_cast<NSString*>(value));
    } else if ([value isKindOfClass:[NSNumber class]]) {
        NSNumber* number = static_cast<NSNumber*>(value);
        NSString* objCType = @(number.objCType);
        if ([objCType isEqualToString:@(@encode(BOOL))] || [objCType isEqualToString:@(@encode(bool))])
            result.data = static_cast<bool>([number boolValue]);
        else
            result.data = static_cast<int>([number intValue]);
    } else if ([value isKindOfClass:[NSArray class]]) {
        NSArray* array = static_cast<NSArray*>(value);
        Vector<Box<WebCore::ISO18013Any>> vecArray;
        for (id item in array) {
            auto convertedItem = convertObjCValueToISO18013Any(item);
            vecArray.append(Box<WebCore::ISO18013Any>::create(WTF::move(convertedItem)));
        }
        result.data = WTF::move(vecArray);
    } else if ([value isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = static_cast<NSDictionary*>(value);
        HashMap<String, Box<WebCore::ISO18013Any>> hashMap;
        for (NSString* key in dict) {
            auto convertedValue = convertObjCValueToISO18013Any(dict[key]);
            hashMap.set(String(key), Box<WebCore::ISO18013Any>::create(WTF::move(convertedValue)));
        }
        result.data = WTF::move(hashMap);
    } else
        result.data = std::monostate();

    return result;
}

static WebCore::ISO18013DocumentRequestInfoExtension convertApplicationSpecificExtensions(NSDictionary<NSString*, id> *extensions)
{
    WebCore::ISO18013DocumentRequestInfoExtension result;

    if (!extensions)
        return result;

    for (NSString* key in extensions) {
        auto convertedValue = convertObjCValueToISO18013Any(extensions[key]);
        result.set(String(key), WTF::move(convertedValue));
    }

    return result;
}
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

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
            innerVector.append(std::make_pair(WTF::move(mappedElementIdentifier), WTF::move(elementInfo)));
        }

        mappedDocumentRequest.namespaces.append(std::make_pair(WTF::move(mappedNamespaceKey), WTF::move(innerVector)));
    }

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
    if (individualDocumentRequest.applicationSpecificExtensions) {
        WebCore::ISO18013DocumentRequestInfo requestInfo;
        requestInfo.extension = convertApplicationSpecificExtensions(individualDocumentRequest.applicationSpecificExtensions);
        mappedDocumentRequest.requestInfo = WTF::move(requestInfo);
    }
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

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

            mappedPresentmentRequest.documentRequestSets.append(WTF::move(mappedDocumentSet));
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

Vector<WebCore::ValidatedMobileDocumentRequest> DigitalCredentials::validateRequests(const SecurityOrigin &topOrigin, const Document &document, const Vector<WebCore::MobileDocumentRequest> &unvalidatedRequests)
{
    RetainPtr convertedTopOrigin = topOrigin.toURL().createNSURL().get();
    RetainPtr validator = adoptNS([WebKit::allocWKIdentityDocumentRawRequestValidatorInstance() init]);

    Vector<WebCore::ValidatedMobileDocumentRequest> validatedRequests;

    for (auto mobileDocumentRequest : unvalidatedRequests) {

        RetainPtr convertedEncryptionInfo = mobileDocumentRequest.encryptionInfo.createNSString();
        RetainPtr convertedDeviceRequest = mobileDocumentRequest.deviceRequest.createNSString();

        RetainPtr iso18013Request = adoptNS([WebKit::allocWKISO18013RequestInstance() initWithEncryptionInfo:convertedEncryptionInfo.get() deviceRequest:convertedDeviceRequest.get()]);

        NSError *error = nil;
        RetainPtr validatedISORequest = [validator validateISO18013Request:iso18013Request.get() origin:convertedTopOrigin.get() error:&error];

        if (validatedISORequest) {
            auto validatedMobileDocumentRequest = buildValidatedRequest(validatedISORequest.get());
            validatedRequests.append(WTF::move(validatedMobileDocumentRequest));
        } else if (error) {
            RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
            String errorMessage = "An error occurred validating the incoming 'org-iso-mdoc' request. The request will be ignored."_s;

            if ([debugDescription length])
                errorMessage = makeString(errorMessage, " ("_s, String(debugDescription.get()), ")"_s);

            const_cast<Document &>(document).addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(
                MessageSource::JS,
                MessageType::Log,
                MessageLevel::Warning,
                errorMessage));

            LOG(DigitalCredentials, "DigitalCredentials::validateRequests() - WebProcess: Validation failed for request: %@", error);
        }
    }

    LOG(DigitalCredentials, "DigitalCredentials::validateRequests() - WebProcess: returning %zu validated requests", validatedRequests.size());
    return validatedRequests;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
