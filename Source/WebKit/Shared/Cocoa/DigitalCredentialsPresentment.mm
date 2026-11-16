/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "DigitalCredentialsPresentment.h"

#if ENABLE(WEB_AUTHN)

#import "Logging.h"
#import <Foundation/Foundation.h>
#import <Security/SecTrust.h>
#import <WebCore/DigitalCredentialsProtocols.h>
#import <WebCore/UnvalidatedDigitalCredentialRequest.h>
#import <WebCore/ValidatedMobileDocumentRequest.h>
#import <WebCore/ValidatedOpenID4VPRequest.h>
#import <WebCore/X509SubjectKeyIdentifier.h>
#import <WebKit/WKIdentityDocumentPresentmentError.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentOpenID4VPRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRawRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentResponse.h>
#import <wtf/Borrow.h>
#import <wtf/JSONValues.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>
#import <wtf/text/TextStream.h>

#import "WebKitSwiftSoftLink.h"

namespace WebKit {

using WebCore::DigitalCredentialPresentationProtocol;
using WebCore::ExceptionCode;

static RetainPtr<NSArray<NSData *>> mapIssuerIdentifiersFromX509Identifiers(const std::optional<WebCore::ISO18013IssuerIdentifiers>& identifiers)
{
    if (!identifiers || identifiers->isEmpty())
        return nil;

    RetainPtr mappedIdentifiers = adoptNS([[NSMutableArray alloc] initWithCapacity:identifiers->size()]);

    for (const auto& identifier : borrow(*identifiers)) {
        RetainPtr nsData = toNSData(borrow(identifier.data)->span());
        [mappedIdentifiers addObject:nsData.get()];
    }

    return mappedIdentifiers;
}

static RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mapDocumentRequests(const Vector<WebCore::ISO18013DocumentRequest>& documentRequests)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mappedDocumentRequests = adoptNS([[NSMutableArray alloc] init]);
    for (auto&& validatedDocumentRequest : borrow(documentRequests)) {
        RetainPtr<NSMutableDictionary<NSString *, NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *> *>> namespaces = adoptNS([[NSMutableDictionary alloc] init]);
        for (auto&& namespacePair : borrow(validatedDocumentRequest.namespaces)) {
            RetainPtr<NSMutableDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *>> namespaceDictionary = adoptNS([[NSMutableDictionary alloc] init]);

            auto namespaceIdentifier = namespacePair.first;
            const Vector elements = namespacePair.second;

            for (auto&& elementPair : elements) {
                RetainPtr mappedElementIdentifier = elementPair.first.createNSString();
                RetainPtr mappedElementValue = adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentElementInfoInstance() initWithIsRetaining:elementPair.second.isRetaining]);

                [namespaceDictionary setObject:mappedElementValue.get() forKey:mappedElementIdentifier.get()];
            }

            RetainPtr mappedNamespaceIdentifier = namespaceIdentifier.createNSString();
            [namespaces setObject:namespaceDictionary.get() forKey:mappedNamespaceIdentifier.get()];
        }

        RetainPtr documentType = validatedDocumentRequest.documentType.createNSString();

        RetainPtr issuerIdentifiers = mapIssuerIdentifiersFromX509Identifiers(validatedDocumentRequest.requestInfo ? validatedDocumentRequest.requestInfo->issuerIdentifiers : std::nullopt);

        RetainPtr mappedDocumentRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequestInstance() initWithDocumentType:documentType.get() namespaces:namespaces.get() issuerIdentifiers:issuerIdentifiers.get()]);
        [mappedDocumentRequests addObject:mappedDocumentRequest.get()];
    }

    return mappedDocumentRequests;
}

static RetainPtr<NSArray<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *>> mapDocumentRequestSets(const Vector<WebCore::ISO18013DocumentRequestSet>& documentRequestSets)
{
    RetainPtr<NSMutableArray<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *>> mappedDocumentSets = adoptNS([[NSMutableArray alloc] init]);
    for (auto&& validatedDocumentSet : borrow(documentRequestSets)) {
        RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mappedDocumentRequests = mapDocumentRequests(validatedDocumentSet.requests);
        [mappedDocumentSets addObject:mappedDocumentRequests.get()];
    }
    return mappedDocumentSets;
}

static RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *>> mapPresentmentRequests(const Vector<WebCore::ISO18013PresentmentRequest>& presentmentRequests)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *>> mappedPresentmentRequests = adoptNS([[NSMutableArray alloc] init]);
    for (auto&& validatedPresentmentRequest : borrow(presentmentRequests)) {
        RetainPtr<NSArray<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *>> mappedDocumentSets = mapDocumentRequestSets(validatedPresentmentRequest.documentRequestSets);
        RetainPtr mappedPresentmentRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentPresentmentRequestInstance() initWithDocumentSets:mappedDocumentSets.get() isMandatory:validatedPresentmentRequest.isMandatory]);
        [mappedPresentmentRequests addObject:mappedPresentmentRequest.get()];
    }
    return mappedPresentmentRequests;
}

static RetainPtr<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *>> mapCertificateChain(const WebCore::CertificateInfo& certificateInfo)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *>> mappedCertificateChain = adoptNS([[NSMutableArray alloc] init]);
    if (!certificateInfo.trust())
        return mappedCertificateChain;

    RetainPtr certificateChain = adoptCF(SecTrustCopyCertificateChain(certificateInfo.trust().get()));
    CFIndex count = CFArrayGetCount(certificateChain.get());
    for (CFIndex i = 0; i < count; ++i) {
        RetainPtr certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(certificateChain.get(), i));
        RetainPtr mappedCertificate = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestAuthenticationCertificateInstance() initWithCertificate:certificate.get()]);
        [mappedCertificateChain addObject:mappedCertificate.get()];
    }

    return mappedCertificateChain;
}

static RetainPtr<NSArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *>> mapRequestAuthentications(const Vector<WebCore::CertificateInfo>& requestAuthentications)
{
    return createNSArray(requestAuthentications, [](const WebCore::CertificateInfo& certificateInfo) {
        return mapCertificateChain(certificateInfo);
    });
}

static RetainPtr<NSString> mapCredentialFormat(WebCore::OpenID4VPCredentialFormat format)
{
    switch (format) {
    case WebCore::OpenID4VPCredentialFormat::MsoMdoc:
        return @"mso_mdoc";
    case WebCore::OpenID4VPCredentialFormat::DcSdJwt:
        return @"dc+sd-jwt";
    case WebCore::OpenID4VPCredentialFormat::Unknown:
        return nil;
    }
    ASSERT_NOT_REACHED();
    return nil;
}

static RetainPtr<NSString> mapTrustedAuthorityType(WebCore::OpenID4VPTrustedAuthorityType type)
{
    switch (type) {
    case WebCore::OpenID4VPTrustedAuthorityType::AuthorityKeyIdentifier:
        return @"aki";
    case WebCore::OpenID4VPTrustedAuthorityType::ETSITrustedList:
        return @"etsi_tl";
    case WebCore::OpenID4VPTrustedAuthorityType::OpenIDFederation:
        return @"openid_federation";
    }
    ASSERT_NOT_REACHED();
    return nil;
}

static RetainPtr<NSArray<NSString *>> mapStrings(const Vector<String>& strings)
{
    return createNSArray(strings, [](const String& string) {
        return string.createNSString();
    });
}

static RetainPtr<NSArray<NSArray<NSString *> *>> mapStringSets(const Vector<Vector<String>>& stringSets)
{
    return createNSArray(stringSets, [](const Vector<String>& strings) {
        return mapStrings(strings);
    });
}

static RetainPtr<WKIdentityDocumentPresentmentOpenID4VPClaimsQuery> mapClaimsQuery(const WebCore::OpenID4VPClaimsQuery& claim)
{
    RetainPtr path = createNSArray(claim.path, [](const WebCore::OpenID4VPClaimPathComponent& component) {
        return WTF::switchOn(component,
            [](const String& key) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimPathComponentInstance() initWithKey:key.createNSString().get() index:nil]);
            },
            [](const WebCore::OpenID4VPAllArrayElements&) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimPathComponentInstance() initWithKey:nil index:nil]);
            },
            [](uint64_t index) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimPathComponentInstance() initWithKey:nil index:@(index)]);
            });
    });

    RetainPtr values = createNSArray(claim.values, [](const WebCore::OpenID4VPClaimValue& value) {
        return WTF::switchOn(value,
            [](const String& string) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimValueInstance() initWithStringValue:string.createNSString().get() integerValue:nil booleanValue:nil]);
            },
            [](int64_t integer) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimValueInstance() initWithStringValue:nil integerValue:@(integer) booleanValue:nil]);
            },
            [](bool boolean) {
                return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimValueInstance() initWithStringValue:nil integerValue:nil booleanValue:@(boolean)]);
            });
    });

    RetainPtr<NSNumber> intentToRetain = claim.intentToRetain ? @(*claim.intentToRetain) : nil;

    return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPClaimsQueryInstance() initWithIdentifier:nsStringNilIfNull(claim.identifier).get() path:path.get() values:values.get() intentToRetain:intentToRetain.get()]);
}

static RetainPtr<WKIdentityDocumentPresentmentMobileDocumentRequest> mapMobileDocumentRequest(const WebCore::ValidatedMobileDocumentRequest& validatedRequest)
{
    RetainPtr presentmentRequests = mapPresentmentRequests(validatedRequest.presentmentRequests);
    RetainPtr authenticationCertificates = mapRequestAuthentications(validatedRequest.requestAuthentications);
    return adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentRequestInstance() initWithPresentmentRequests:presentmentRequests.get() authenticationCertificates:authenticationCertificates.get()]);
}

static RetainPtr<WKIdentityDocumentPresentmentOpenID4VPRequest> mapOpenID4VPRequest(const WebCore::ValidatedOpenID4VPRequest& validatedRequest)
{
    auto& request = validatedRequest.request;

    RetainPtr credentials = createNSArray(request.credentials, [](const WebCore::OpenID4VPCredentialQuery& credential) {
        RetainPtr trustedAuthorities = createNSArray(credential.trustedAuthorities, [](const WebCore::OpenID4VPTrustedAuthoritiesQuery& query) {
            return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQueryInstance() initWithType:mapTrustedAuthorityType(query.type).get() values:mapStrings(query.values).get()]);
        });

        RetainPtr claims = createNSArray(credential.claims, mapClaimsQuery);

        return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPCredentialQueryInstance()
            initWithIdentifier:credential.identifier.createNSString().get()
            format:mapCredentialFormat(credential.format).get()
            allowsMultiple:credential.allowsMultiple
            documentType:nsStringNilIfNull(credential.documentType).get()
            verifiableCredentialTypes:mapStrings(credential.verifiableCredentialTypes).get()
            trustedAuthorities:trustedAuthorities.get()
            requiresCryptographicHolderBinding:credential.requiresCryptographicHolderBinding
            claims:claims.get()
            claimSets:mapStringSets(credential.claimSets).get()]);
    });

    RetainPtr credentialSets = createNSArray(request.credentialSets, [](const WebCore::OpenID4VPCredentialSetQuery& credentialSet) {
        return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPCredentialSetQueryInstance() initWithOptions:mapStringSets(credentialSet.options).get() isRequired:credentialSet.isRequired]);
    });

    RetainPtr verifierIdentities = createNSArray(request.verifierIdentities, [](const WebCore::OpenID4VPVerifierIdentity& identity) {
        return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPVerifierIdentityInstance() initWithClientIdentifierPrefix:nsStringNilIfNull(identity.clientIdentifierPrefix).get() identifier:identity.identifier.createNSString().get() certificateChain:mapCertificateChain(identity.certificateChain).get()]);
    });

    RetainPtr requestType = WebCore::digitalCredentialPresentationProtocolToString(validatedRequest.protocol).createNSString();

    return adoptNS([WebKit::allocWKIdentityDocumentPresentmentOpenID4VPRequestInstance() initWithRequestType:requestType.get() credentials:credentials.get() credentialSets:credentialSets.get() verifierIdentities:verifierIdentities.get()]);
}

RetainPtr<WKIdentityDocumentPresentmentRequest> createPresentmentRequest(const WebCore::DigitalCredentialsRequestData& requestData)
{
    bool hasNonOpenID4VPProtocol = requestData.requests.containsIf([](auto& request) {
        auto* openID4VPRequest = std::get_if<WebCore::ValidatedOpenID4VPRequest>(&request);
        return openID4VPRequest && !WebCore::isOpenID4VPPresentationProtocol(openID4VPRequest->protocol);
    });
    if (hasNonOpenID4VPProtocol) {
        LOG(DigitalCredentials, "OpenID4VP requests to present have a protocol that is not an OpenID4VP protocol.");
        return nil;
    }

    RetainPtr requests = createNSArray(requestData.requests, [](auto& validatedRequest) -> RetainPtr<NSObject<WKIdentityDocumentPresentmentCredentialRequest>> {
        return WTF::switchOn(validatedRequest,
            [](const WebCore::ValidatedMobileDocumentRequest& request) -> RetainPtr<NSObject<WKIdentityDocumentPresentmentCredentialRequest>> {
                return mapMobileDocumentRequest(request);
            },
            [](const WebCore::ValidatedOpenID4VPRequest& request) -> RetainPtr<NSObject<WKIdentityDocumentPresentmentCredentialRequest>> {
                return mapOpenID4VPRequest(request);
            });
    });
    if (![requests count]) {
        LOG(DigitalCredentials, "No supported credential requests to present.");
        return nil;
    }

    RetainPtr origin = requestData.topOrigin.toURL().createNSURL();
    return adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestInstance() initWithOrigin:origin.get() requests:requests.get()]);
}

RetainPtr<NSArray> createPresentmentRawRequests(const WebCore::DigitalCredentialsRawRequests& unvalidatedRequests)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentRawRequest *>> rawRequests = adoptNS([[NSMutableArray alloc] init]);

    for (auto& unvalidatedRequest : borrow(unvalidatedRequests)) {
        auto* mobileDocumentRequest = std::get_if<WebCore::MobileDocumentRequest>(&unvalidatedRequest);
        if (!mobileDocumentRequest) {
            auto protocolAndJSON = WebCore::openID4VPRequestJSON(unvalidatedRequest);
            if (!protocolAndJSON)
                return nil;

            auto& [protocol, json] = *protocolAndJSON;
            RetainPtr requestProtocol = WebCore::digitalCredentialPresentationProtocolToString(protocol).createNSString();
            RetainPtr requestData = [json.createNSString() dataUsingEncoding:NSUTF8StringEncoding];
            if (!requestData) {
                LOG(DigitalCredentials, "Failed to encode an OpenID4VP raw request as UTF-8.");
                return nil;
            }

            RetainPtr rawRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRawRequestInstance() initWithRequestProtocol:requestProtocol.get() requestData:requestData.get()]);
            [rawRequests addObject:rawRequest.get()];
            continue;
        }
        RetainPtr deviceRequest = mobileDocumentRequest->deviceRequest.createNSString();
        RetainPtr encryptionInfo = mobileDocumentRequest->encryptionInfo.createNSString();

        RetainPtr<NSDictionary<NSString *, id>> jsonRequest = @{
            @"deviceRequest" : deviceRequest.get(),
            @"encryptionInfo" : encryptionInfo.get()
        };

        NSError *error = nil;
        RetainPtr requestDataBytes = [NSJSONSerialization dataWithJSONObject:jsonRequest.get() options:0 error:&error];

        if (!requestDataBytes) {
            LOG(DigitalCredentials, "Failed to serialize JSON for raw request: %s", error.localizedDescription.UTF8String);
            return nil;
        }

        RetainPtr rawRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRawRequestInstance() initWithRequestProtocol:@"org.iso.mdoc" requestData:requestDataBytes.get()]);
        [rawRequests addObject:rawRequest.get()];
    }

    return rawRequests;
}

std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> responseDataFromPresentment(WKIdentityDocumentPresentmentResponse *response, NSError *error)
{
    if (!response && !error) {
        LOG(DigitalCredentials, "No response or error from document provider.");
        return makeUnexpected(WebCore::ExceptionData { ExceptionCode::OperationError, "No response from document provider."_s });
    }
    if (!response)
        return makeUnexpected(exceptionDataFromPresentmentError(error));

    if (!response.responseData.length) {
        LOG(DigitalCredentials, "Document provider returned zero-length response data.");
        return makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "Document provider returned an invalid format."_s });
    }

    RetainPtr<NSString> protocol = response.protocolString;
    if ([protocol isEqualToString:@"org.iso.mdoc"]) {
        String responseData = base64URLEncodeToString(span(response.responseData));
        if (responseData.isNull()) {
            LOG(DigitalCredentials, "Failed to encode response bytes to URL-safe Base64.");
            return makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "Document provider returned an invalid format."_s });
        }
        Ref object = JSON::Object::create();
        object->setString("response"_s, responseData);
        return WebCore::DigitalCredentialsResponseData { DigitalCredentialPresentationProtocol::OrgIsoMdoc, object->toJSONString() };
    }

    auto openID4VPProtocol = WebCore::digitalCredentialPresentationProtocolFromString(String(protocol.get()));
    if (openID4VPProtocol && WebCore::isOpenID4VPPresentationProtocol(*openID4VPProtocol)) {
        RetainPtr responseString = adoptNS([[NSString alloc] initWithData:response.responseData encoding:NSUTF8StringEncoding]);
        if (!responseString) {
            LOG(DigitalCredentials, "Failed to decode OpenID4VP response data as UTF-8.");
            return makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "Document provider returned an invalid format."_s });
        }
        return WebCore::DigitalCredentialsResponseData { *openID4VPProtocol, String(responseString.get()) };
    }

    LOG(DigitalCredentials, "Unknown protocol response from document provider. Can't convert it %s.", [protocol UTF8String]);
    return makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "Unknown protocol response from document."_s });
}

WebCore::ExceptionData exceptionDataFromPresentmentError(NSError *error)
{
    switch (error.code) {
    case WKIdentityDocumentPresentmentErrorNotEntitled:
        return { ExceptionCode::NotAllowedError, "Not allowed because not entitled."_s };
    case WKIdentityDocumentPresentmentErrorInvalidRequest:
        return { ExceptionCode::TypeError, "Invalid request."_s };
    case WKIdentityDocumentPresentmentErrorRequestInProgress:
        return { ExceptionCode::InvalidStateError, "Request already in progress."_s };
    default: {
        LOG(DigitalCredentials, "The error code was not in the case statement? %zd.", error.code);
        RetainPtr debugDescription = error.userInfo[NSDebugDescriptionErrorKey] ?: error.userInfo[NSLocalizedDescriptionKey];
        LOG(DigitalCredentials, "Internal error: %@", debugDescription ? debugDescription.get() : @"Unknown error with no description.");
        return { ExceptionCode::OperationError, "The credential request failed."_s };
    }
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
