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
#import <WebCore/ValidatedOpenID4VPRequest.h>
#import <WebCore/X509SubjectKeyIdentifier.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentOpenID4VPRequest.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
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

    if (individualDocumentRequest.issuerIdentifiers && [individualDocumentRequest.issuerIdentifiers count] > 0) {
        Vector<WebCore::X509SubjectKeyIdentifier> issuerIdentifiers;
        issuerIdentifiers.reserveInitialCapacity([individualDocumentRequest.issuerIdentifiers count]);

        for (NSData *data in individualDocumentRequest.issuerIdentifiers)
            issuerIdentifiers.append(WebCore::X509SubjectKeyIdentifier { makeVector(data) });

        if (!issuerIdentifiers.isEmpty()) {
            if (!mappedDocumentRequest.requestInfo)
                mappedDocumentRequest.requestInfo = WebCore::ISO18013DocumentRequestInfo { };
            mappedDocumentRequest.requestInfo->issuerIdentifiers = WTF::move(issuerIdentifiers);
        }
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

#if HAVE(DIGITAL_CREDENTIALS_OPENID4VP)
static std::optional<OpenID4VPCredentialFormat> convertCredentialFormat(NSString *format)
{
    if (!format)
        return OpenID4VPCredentialFormat::Unknown;
    if ([format isEqualToString:@"mso_mdoc"])
        return OpenID4VPCredentialFormat::MsoMdoc;
    if ([format isEqualToString:@"dc+sd-jwt"])
        return OpenID4VPCredentialFormat::DcSdJwt;
    return std::nullopt;
}

static std::optional<OpenID4VPTrustedAuthorityType> convertTrustedAuthorityType(NSString *type)
{
    if ([type isEqualToString:@"aki"])
        return OpenID4VPTrustedAuthorityType::AuthorityKeyIdentifier;
    if ([type isEqualToString:@"etsi_tl"])
        return OpenID4VPTrustedAuthorityType::ETSITrustedList;
    if ([type isEqualToString:@"openid_federation"])
        return OpenID4VPTrustedAuthorityType::OpenIDFederation;
    return std::nullopt;
}

static Vector<String> convertStrings(NSArray<NSString *> *strings)
{
    return makeVector(strings, [](NSString *string) -> std::optional<String> {
        return String(string);
    });
}

static Vector<Vector<String>> convertStringSets(NSArray<NSArray<NSString *> *> *stringSets)
{
    return makeVector(stringSets, [](NSArray<NSString *> *strings) -> std::optional<Vector<String>> {
        return convertStrings(strings);
    });
}

static std::optional<OpenID4VPClaimPathComponent> convertClaimPathComponent(WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent *component)
{
    if (component.key)
        return OpenID4VPClaimPathComponent { String(component.key) };
    if (component.index)
        return OpenID4VPClaimPathComponent { static_cast<uint64_t>(component.index.unsignedLongLongValue) };
    return OpenID4VPClaimPathComponent { OpenID4VPAllArrayElements { } };
}

static std::optional<OpenID4VPClaimValue> convertClaimValue(WKIdentityDocumentPresentmentOpenID4VPClaimValue *value)
{
    if (value.stringValue)
        return OpenID4VPClaimValue { String(value.stringValue) };
    if (value.integerValue)
        return OpenID4VPClaimValue { static_cast<int64_t>(value.integerValue.longLongValue) };
    if (value.booleanValue)
        return OpenID4VPClaimValue { static_cast<bool>(value.booleanValue.boolValue) };
    return std::nullopt;
}

static ValidatedOpenID4VPRequestObject buildValidatedOpenID4VPRequestObject(WKIdentityDocumentPresentmentOpenID4VPRequest *request)
{
    ValidatedOpenID4VPRequestObject requestObject;

    requestObject.credentials = makeVector(request.credentials, [](WKIdentityDocumentPresentmentOpenID4VPCredentialQuery *credential) -> std::optional<OpenID4VPCredentialQuery> {
        auto format = convertCredentialFormat(credential.format);
        if (!format)
            return std::nullopt;

        return OpenID4VPCredentialQuery {
            .identifier = credential.identifier,
            .format = *format,
            .allowsMultiple = static_cast<bool>(credential.allowsMultiple),
            .documentType = credential.documentType,
            .verifiableCredentialTypes = convertStrings(credential.verifiableCredentialTypes),
            .trustedAuthorities = makeVector(credential.trustedAuthorities, [](WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery *query) -> std::optional<OpenID4VPTrustedAuthoritiesQuery> {
                auto type = convertTrustedAuthorityType(query.type);
                if (!type)
                    return std::nullopt;
                return OpenID4VPTrustedAuthoritiesQuery { *type, convertStrings(query.values) };
            }),
            .requiresCryptographicHolderBinding = static_cast<bool>(credential.requiresCryptographicHolderBinding),
            .claims = makeVector(credential.claims, [](WKIdentityDocumentPresentmentOpenID4VPClaimsQuery *claim) -> std::optional<OpenID4VPClaimsQuery> {
                std::optional<bool> intentToRetain;
                if (claim.intentToRetain)
                    intentToRetain = claim.intentToRetain.boolValue;

                return OpenID4VPClaimsQuery {
                    .identifier = claim.identifier,
                    .path = makeVector(claim.path, convertClaimPathComponent),
                    .values = makeVector(claim.values, convertClaimValue),
                    .intentToRetain = intentToRetain,
                };
            }),
            .claimSets = convertStringSets(credential.claimSets),
        };
    });

    requestObject.credentialSets = makeVector(request.credentialSets, [](WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery *credentialSet) -> std::optional<OpenID4VPCredentialSetQuery> {
        return OpenID4VPCredentialSetQuery { convertStringSets(credentialSet.options), static_cast<bool>(credentialSet.isRequired) };
    });

    requestObject.verifierIdentities = makeVector(request.verifierIdentities, [](WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity *identity) -> std::optional<OpenID4VPVerifierIdentity> {
        auto certificateChain = makeVector(identity.certificateChain, [](WKIdentityDocumentPresentmentRequestAuthenticationCertificate *certificate) -> std::optional<RetainPtr<SecCertificateRef>> {
            return RetainPtr<SecCertificateRef> { certificate.certificate };
        });

        return OpenID4VPVerifierIdentity {
            .clientIdentifierPrefix = identity.clientIdentifierPrefix,
            .identifier = identity.identifier,
            .certificateChain = CertificateInfo { createSecTrustForChain(certificateChain) },
        };
    });

    return requestObject;
}

static std::optional<std::pair<DigitalCredentialPresentationProtocol, RetainPtr<NSData>>> createOpenID4VPRequestData(const WebCore::UnvalidatedDigitalCredentialRequest &unvalidatedRequest)
{
    auto protocolAndJSON = WebCore::openID4VPRequestJSON(unvalidatedRequest);
    if (!protocolAndJSON)
        return std::nullopt;

    auto& [protocol, json] = *protocolAndJSON;
    RetainPtr requestData = [json.createNSString() dataUsingEncoding:NSUTF8StringEncoding];
    if (!requestData)
        return std::nullopt;

    return std::make_pair(protocol, WTF::move(requestData));
}
#endif // HAVE(DIGITAL_CREDENTIALS_OPENID4VP)

static void reportValidationFailure(const Document &document, const String &protocolName, NSError *error)
{
    RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
    String errorMessage = makeString("An error occurred validating the incoming '"_s, protocolName, "' request. The request will be ignored."_s);

    if ([debugDescription length])
        errorMessage = makeString(errorMessage, " ("_s, String(debugDescription.get()), ")"_s);

    const_cast<Document &>(document).addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(
        MessageSource::JS,
        MessageType::Log,
        MessageLevel::Warning,
        errorMessage));

    LOG(DigitalCredentials, "DigitalCredentials::validateRequests() - WebProcess: Validation failed for request: %@", error);
}

static std::optional<WebCore::ValidatedDigitalCredentialRequest> validateRequest(WKIdentityDocumentRawRequestValidator *validator, NSURL *topOrigin, const Document &document, const WebCore::UnvalidatedDigitalCredentialRequest &unvalidatedRequest)
{
    auto* mobileDocumentRequest = std::get_if<WebCore::MobileDocumentRequest>(&unvalidatedRequest);
    if (!mobileDocumentRequest) {
#if HAVE(DIGITAL_CREDENTIALS_OPENID4VP)
        auto protocolAndRequestData = createOpenID4VPRequestData(unvalidatedRequest);
        if (!protocolAndRequestData)
            return std::nullopt;

        auto [protocol, requestData] = *protocolAndRequestData;
        RetainPtr requestType = WebCore::digitalCredentialPresentationProtocolToString(protocol).createNSString();

        NSError *openID4VPError = nil;
        RetainPtr validatedOpenID4VPRequest = [validator validateOpenID4VPRequest:requestData.get() requestType:requestType.get() origin:topOrigin error:&openID4VPError];

        if (validatedOpenID4VPRequest) {
            auto requestObject = buildValidatedOpenID4VPRequestObject(validatedOpenID4VPRequest.get());
            if (WebCore::isWithinOpenID4VPRequestLimits(requestObject))
                return WebCore::ValidatedOpenID4VPRequest { protocol, WTF::move(requestObject) };
            RetainPtr limitsError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSValidationErrorMaximum userInfo:@{ NSDebugDescriptionErrorKey: @"The request is too large." }];
            reportValidationFailure(document, WebCore::digitalCredentialPresentationProtocolToString(protocol), limitsError.get());
            return std::nullopt;
        }
        if (openID4VPError)
            reportValidationFailure(document, WebCore::digitalCredentialPresentationProtocolToString(protocol), openID4VPError);
#else
        LOG(DigitalCredentials, "DigitalCredentials::validateRequests() - WebProcess: no platform support for OpenID4VP; the request will be ignored.");
#endif // HAVE(DIGITAL_CREDENTIALS_OPENID4VP)
        return std::nullopt;
    }

    RetainPtr convertedEncryptionInfo = mobileDocumentRequest->encryptionInfo.createNSString();
    RetainPtr convertedDeviceRequest = mobileDocumentRequest->deviceRequest.createNSString();

    RetainPtr iso18013Request = adoptNS([WebKit::allocWKISO18013RequestInstance() initWithEncryptionInfo:convertedEncryptionInfo.get() deviceRequest:convertedDeviceRequest.get()]);

    NSError *error = nil;
    RetainPtr validatedISORequest = [validator validateISO18013Request:iso18013Request.get() origin:topOrigin error:&error];

    if (validatedISORequest)
        return buildValidatedRequest(validatedISORequest.get());
    if (error)
        reportValidationFailure(document, "org-iso-mdoc"_s, error);
    return std::nullopt;
}

Vector<std::optional<WebCore::ValidatedDigitalCredentialRequest>> DigitalCredentials::validateRequests(const SecurityOrigin &topOrigin, const Document &document, const Vector<WebCore::UnvalidatedDigitalCredentialRequest> &unvalidatedRequests)
{
    RetainPtr convertedTopOrigin = topOrigin.toURL().createNSURL().get();
    RetainPtr validator = adoptNS([WebKit::allocWKIdentityDocumentRawRequestValidatorInstance() init]);

    auto validatedRequests = unvalidatedRequests.map([&](auto& unvalidatedRequest) {
        return validateRequest(validator.get(), convertedTopOrigin.get(), document, unvalidatedRequest);
    });

    LOG(DigitalCredentials, "DigitalCredentials::validateRequests() - WebProcess: validated %zu of %zu requests", static_cast<size_t>(std::ranges::count_if(validatedRequests, [](auto& request) {
        return request.has_value();
    })), validatedRequests.size());
    return validatedRequests;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
