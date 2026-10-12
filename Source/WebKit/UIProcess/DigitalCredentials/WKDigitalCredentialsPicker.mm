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
#import "WKDigitalCredentialsPicker.h"

#if ENABLE(WEB_AUTHN)

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#import "DigitalCredentialsCoordinatorMessages.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WKWebView.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/ConsoleTypes.h>
#import <Security/SecTrust.h>
#import <WebCore/DigitalCredentialGetRequest.h>
#import <WebCore/DigitalCredentialsProtocols.h>
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DigitalCredentialsResponseData.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/UnvalidatedDigitalCredentialRequest.h>
#import <WebCore/ValidatedMobileDocumentRequest.h>
#import <WebCore/ValidatedOpenID4VPRequest.h>
#import <WebCore/X509SubjectKeyIdentifier.h>
#import <WebKit/WKIdentityDocumentPresentmentController.h>
#import <WebKit/WKIdentityDocumentPresentmentError.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentOpenID4VPRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRawRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRequest.h>
#import <wtf/BlockPtr.h>
#import <wtf/JSONValues.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringCommon.h>
#import <wtf/text/TextStream.h>
#import <wtf/text/WTFString.h>

#import "WebKitSwiftSoftLink.h"

using WebCore::ExceptionCode;
using WebCore::DigitalCredentialPresentationProtocol;

#pragma mark - WKDigitalCredentialsPickerDelegate

@interface WKDigitalCredentialsPickerDelegate : NSObject {
@protected
    WeakObjCPtr<id<WKDigitalCredentialsPickerDelegate>> _digitalCredentialsPickerDelegate;
}

- (instancetype)initWithDigitalCredentialsPickerDelegate:(id<WKDigitalCredentialsPickerDelegate>)digitalCredentialsPickerDelegate;

@end // WKDigitalCredentialsPickerDelegate

@implementation WKDigitalCredentialsPickerDelegate

- (instancetype)initWithDigitalCredentialsPickerDelegate:(id<WKDigitalCredentialsPickerDelegate>)digitalCredentialsPickerDelegate
{
    if (!(self = [super init]))
        return nil;

    _digitalCredentialsPickerDelegate = digitalCredentialsPickerDelegate;

    return self;
}

@end // WKDigitalCredentialsPickerDelegate

@interface WKRequestDataResult : NSObject

@property (nonatomic, strong) NSData *requestDataBytes;
@property (nonatomic, assign) DigitalCredentialPresentationProtocol protocol;

- (instancetype)initWithRequestDataBytes:(NSData *)requestDataBytes protocol:(DigitalCredentialPresentationProtocol)protocol;

@end

@implementation WKRequestDataResult

- (instancetype)initWithRequestDataBytes:(NSData *)requestDataBytes protocol:(DigitalCredentialPresentationProtocol)protocol
{
    self = [super init];
    if (self) {
        self.requestDataBytes = requestDataBytes;
        self.protocol = protocol;
    }
    return self;
}

- (void)dealloc
{
    self.requestDataBytes = nil;

    [super dealloc];
}

@end // WKRequestDataResult

#pragma mark - Adapter functions

static RetainPtr<NSArray<NSData *>> mapIssuerIdentifiersFromX509Identifiers(const std::optional<WebCore::ISO18013IssuerIdentifiers>& identifiers)
{
    if (!identifiers || identifiers->isEmpty())
        return nil;

    RetainPtr mappedIdentifiers = adoptNS([[NSMutableArray alloc] initWithCapacity:identifiers->size()]);

    for (const auto& identifier : *identifiers) {
        RetainPtr nsData = toNSData(identifier.data);
        [mappedIdentifiers addObject:nsData.get()];
    }

    return mappedIdentifiers;
}

static RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mapDocumentRequests(const Vector<WebCore::ISO18013DocumentRequest>& documentRequests)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mappedDocumentRequests = adoptNS([[NSMutableArray alloc] init]);
    for (auto&& validatedDocumentRequest : documentRequests) {
        RetainPtr<NSMutableDictionary<NSString *, NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *> *>> namespaces = adoptNS([[NSMutableDictionary alloc] init]);
        for (auto&& namespacePair : validatedDocumentRequest.namespaces) {
            RetainPtr<NSMutableDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *>> namespaceDictionary = adoptNS([[NSMutableDictionary alloc] init]);

            auto namespaceIdentifier = namespacePair.first;
            auto elements = namespacePair.second;

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
    for (auto&& validatedDocumentSet : documentRequestSets) {
        RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *>> mappedDocumentRequests = mapDocumentRequests(validatedDocumentSet.requests);
        [mappedDocumentSets addObject:mappedDocumentRequests.get()];
    }
    return mappedDocumentSets;
}

static RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *>> mapPresentmentRequests(const Vector<WebCore::ISO18013PresentmentRequest>& presentmentRequests)
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *>> mappedPresentmentRequests = adoptNS([[NSMutableArray alloc] init]);
    for (auto&& validatedPresentmentRequest : presentmentRequests) {
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

#pragma mark - WKDigitalCredentialsPicker

@implementation WKDigitalCredentialsPicker {
    WeakPtr<WebKit::WebPageProxy> _page;
    RetainPtr<WKDigitalCredentialsPickerDelegate> _digitalCredentialsPickerDelegate;
    RetainPtr<WKIdentityDocumentPresentmentController> _presentmentController;
    WeakObjCPtr<id<WKDigitalCredentialsPickerDelegate>> _delegate;
    WeakObjCPtr<WKWebView> _webView;
    CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> _completionHandler;
}

- (instancetype)initWithView:(WKWebView *)view page:(WebKit::WebPageProxy *)page
{
    self = [super init];
    if (!self)
        return nil;

    _webView = view;
    _page = page;
    return self;
}

- (void)dealloc
{
    if (_completionHandler)
        _completionHandler(makeUnexpected(WebCore::ExceptionData { ExceptionCode::OperationError, "The digital credential request was interrupted."_s }));

    [super dealloc];
}

- (id<WKDigitalCredentialsPickerDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id<WKDigitalCredentialsPickerDelegate>)delegate
{
    _delegate = delegate;
}

- (CocoaWindow *)presentationAnchor
{
    if (RetainPtr webView = _webView.get())
        return [webView window];
    return nil;
}

- (void)fetchRawRequestsWithCompletionHandler:(void (^)(NSArray<WKIdentityDocumentPresentmentRawRequest *> *))completionHandler
{
    LOG(DigitalCredentials, "Fetching raw requests from web content process");
    RefPtr page = _page.get();
    if (!page) {
        LOG(DigitalCredentials, "Cannot fetch raw requests: page is null");
        completionHandler(@[]);
        return;
    }
    page->fetchRawDigitalCredentialRequests([completionHandler = makeBlockPtr(completionHandler)](WebCore::DigitalCredentialsRawRequests&& unvalidatedRequests) mutable {
        WTF::switchOn(WTF::move(unvalidatedRequests),
            [completionHandler = WTF::move(completionHandler)](Vector<WebCore::UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests) {
                RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentRawRequest *>> rawRequests = adoptNS([[NSMutableArray alloc] init]);

                for (auto &&unvalidatedRequest : unvalidatedRequests) {
                    auto* mobileDocumentRequest = std::get_if<WebCore::MobileDocumentRequest>(&unvalidatedRequest);
                    if (!mobileDocumentRequest) {
                        auto protocolAndJSON = WebCore::openID4VPRequestJSON(unvalidatedRequest);
                        if (!protocolAndJSON) {
                            completionHandler(@[]);
                            return;
                        }

                        auto& [protocol, json] = *protocolAndJSON;
                        RetainPtr requestProtocol = WebCore::digitalCredentialPresentationProtocolToString(protocol).createNSString();
                        RetainPtr requestData = [json.createNSString() dataUsingEncoding:NSUTF8StringEncoding];
                        if (!requestData) {
                            LOG(DigitalCredentials, "Failed to encode an OpenID4VP raw request as UTF-8.");
                            completionHandler(@[]);
                            return;
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
                        completionHandler(@[]);
                        return;
                    }

                    RetainPtr rawRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRawRequestInstance() initWithRequestProtocol:@"org.iso.mdoc" requestData:requestDataBytes.get()]);
                    [rawRequests addObject:rawRequest.get()];
                }

                completionHandler(rawRequests.get());
        }
        );

    });
}

- (void)presentWithRequestData:(const WebCore::DigitalCredentialsRequestData &)requestData completionHandler:(CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> &&)completionHandler
{
    WTF::switchOn(requestData,
        [](const auto& requestData) {
            LOG_WITH_STREAM(DigitalCredentials, stream << "WKDigitalCredentialsPicker: Digital Credentials - Presenting with request data: "_s << requestData.topOrigin.toString() << "."_s);
    });
    _completionHandler = WTF::move(completionHandler);

    ASSERT(!_presentmentController);

    [self setupPresentmentController];

    _digitalCredentialsPickerDelegate = adoptNS([[WKDigitalCredentialsPickerDelegate alloc] initWithDigitalCredentialsPickerDelegate:self]);

    if (auto* mobileDocumentRequestData = std::get_if<WebCore::DigitalCredentialsMobileDocumentRequestData>(&requestData))
        [self performRequest:*mobileDocumentRequestData];
    else if (auto* openID4VPRequestData = std::get_if<WebCore::DigitalCredentialsOpenID4VPRequestData>(&requestData))
        [self performOpenID4VPRequest:*openID4VPRequestData];
    else
        ASSERT_NOT_REACHED();

    if ([self.delegate respondsToSelector:@selector(digitalCredentialsPickerDidPresent:)])
        [self.delegate digitalCredentialsPickerDidPresent:self];
}

- (void)dismissWithCompletionHandler:(CompletionHandler<void(bool)> &&)completionHandler
{
    LOG(DigitalCredentials, "WKDigitalCredentialsPicker Dismissing with completion handler.");
    [self dismiss];
    completionHandler(true);
}

#pragma mark - Helper Methods

- (void)performRequest:(const WebCore::DigitalCredentialsMobileDocumentRequestData &)requestData
{
    RetainPtr mobileDocumentRequests = adoptNS([[NSMutableArray alloc] init]);

    for (auto&& validatedRequest : requestData.requests) {

        RetainPtr presentmentRequests = mapPresentmentRequests(validatedRequest.presentmentRequests);
        RetainPtr authenticationCertificates = mapRequestAuthentications(validatedRequest.requestAuthentications);

        RetainPtr mobileDocumentRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentRequestInstance() initWithPresentmentRequests:presentmentRequests.get() authenticationCertificates:authenticationCertificates.get()]);
        [mobileDocumentRequests addObject:mobileDocumentRequest.get()];
    }

    if (![mobileDocumentRequests count]) {
        LOG(DigitalCredentials, "No supported mobile document requests to present.");
        WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "No supported document requests to present."_s };
        [self completeWith:makeUnexpected(exceptionData)];
        return;
    }

    RetainPtr mappedOrigin = requestData.topOrigin.toURL().createNSURL();
    RetainPtr mappedRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestInstance() initWithOrigin:mappedOrigin.get() mobileDocumentRequests:mobileDocumentRequests.get() openID4VPRequests:@[]]);

    [self presentRequest:mappedRequest.get()];
}

- (void)performOpenID4VPRequest:(const WebCore::DigitalCredentialsOpenID4VPRequestData &)requestData
{
    bool hasMismatchedProtocol = requestData.requests.containsIf([&](auto& request) {
        return !WebCore::isOpenID4VPPresentationProtocol(request.protocol) || request.protocol != requestData.requests.first().protocol;
    });
    if (hasMismatchedProtocol) {
        LOG(DigitalCredentials, "OpenID4VP requests to present do not share a single OpenID4VP protocol.");
        [self completeWith:makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "No supported credential requests to present."_s })];
        return;
    }

    RetainPtr openID4VPRequests = createNSArray(requestData.requests, [](auto& validatedRequest) {
        return mapOpenID4VPRequest(validatedRequest);
    });

    if (![openID4VPRequests count]) {
        LOG(DigitalCredentials, "No supported OpenID4VP requests to present.");
        WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "No supported credential requests to present."_s };
        [self completeWith:makeUnexpected(exceptionData)];
        return;
    }

    RetainPtr mappedOrigin = requestData.topOrigin.toURL().createNSURL();
    RetainPtr mappedRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestInstance() initWithOrigin:mappedOrigin.get() mobileDocumentRequests:@[] openID4VPRequests:openID4VPRequests.get()]);

    [self presentRequest:mappedRequest.get()];
}

- (void)presentRequest:(WKIdentityDocumentPresentmentRequest *)request
{
    [_presentmentController performRequest:request completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKDigitalCredentialsPicker>(self)](WKIdentityDocumentPresentmentResponse *response, NSError *error) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        [strongSelf handlePresentmentCompletionWithResponse:response error:error];
    }).get()];
}

- (void)setupPresentmentController
{
    _presentmentController = adoptNS([WebKit::allocWKIdentityDocumentPresentmentControllerInstance() init]);
    [_presentmentController.get() setDelegate:self];
}

- (void)handlePresentmentCompletionWithResponse:(WKIdentityDocumentPresentmentResponse *)response error:(NSError *)error
{
    if (!response && !error) {
        LOG(DigitalCredentials, "No response or error from document provider.");
        WebCore::ExceptionData exceptionData = { ExceptionCode::OperationError, "No response from document provider."_s };
        [self completeWith:makeUnexpected(exceptionData)];
        return;
    }

    if (response) {
        if (!response.responseData.length) {
            LOG(DigitalCredentials, "Document provider returned zero-length response data.");
            WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Document provider returned an invalid format."_s };
            [self completeWith:makeUnexpected(exceptionData)];
            return;
        }

        RetainPtr<NSString> protocol = response.protocolString;

        if ([protocol isEqualToString:@"org.iso.mdoc"]) {
            String responseData = base64URLEncodeToString(span(response.responseData));

            if (responseData.isNull()) {
                LOG(DigitalCredentials, "Failed to encode response bytes to URL-safe Base64.");
                WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Document provider returned an invalid format."_s };
                [self completeWith:makeUnexpected(exceptionData)];
                return;
            }

            LOG_WITH_STREAM(DigitalCredentials, stream << "The document provider returned response data: "_s << responseData << "."_s);
            Ref object = JSON::Object::create();
            object->setString("response"_s, responseData);
            WebCore::DigitalCredentialsResponseData responseObject { DigitalCredentialPresentationProtocol::OrgIsoMdoc, object->toJSONString() };
            [self completeWith:WTF::move(responseObject)];
            return;
        }

        auto openID4VPProtocol = WebCore::digitalCredentialPresentationProtocolFromString(String(protocol.get()));
        if (openID4VPProtocol && WebCore::isOpenID4VPPresentationProtocol(*openID4VPProtocol)) {
            RetainPtr responseString = adoptNS([[NSString alloc] initWithData:response.responseData encoding:NSUTF8StringEncoding]);

            if (!responseString) {
                LOG(DigitalCredentials, "Failed to decode OpenID4VP response data as UTF-8.");
                WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Document provider returned an invalid format."_s };
                [self completeWith:makeUnexpected(exceptionData)];
                return;
            }

            WebCore::DigitalCredentialsResponseData responseObject { *openID4VPProtocol, String(responseString.get()) };
            [self completeWith:WTF::move(responseObject)];
            return;
        }

        LOG(DigitalCredentials, "Unknown protocol response from document provider. Can't convert it %s.", [protocol UTF8String]);
        WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Unknown protocol response from document."_s };
        [self completeWith:makeUnexpected(exceptionData)];
        return;
    }

    [self handleNSError:error];
}

- (void)handleNSError:(NSError *)error
{
    WebCore::ExceptionData exceptionData;

    switch (error.code) {
    case WKIdentityDocumentPresentmentErrorNotEntitled:
        exceptionData = { ExceptionCode::NotAllowedError, "Not allowed because not entitled."_s };
        break;
    case WKIdentityDocumentPresentmentErrorInvalidRequest:
        exceptionData = { ExceptionCode::TypeError, "Invalid request."_s };
        break;
    case WKIdentityDocumentPresentmentErrorRequestInProgress:
        exceptionData = { ExceptionCode::InvalidStateError, "Request already in progress."_s };
        break;
    default:
        LOG(DigitalCredentials, "The error code was not in the case statement? %zd.", error.code);
        exceptionData = { ExceptionCode::OperationError, "The credential request failed."_s };
        RetainPtr debugDescription = error.userInfo[NSDebugDescriptionErrorKey] ?: error.userInfo[NSLocalizedDescriptionKey];
        LOG(DigitalCredentials, "Internal error: %@", debugDescription ? debugDescription.get() : @"Unknown error with no description.");
        break;
    }

    if (RefPtr page = _page.get()) {
        String consoleMessage = exceptionData.message;
        RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
        if ([debugDescription length])
            consoleMessage = makeString(consoleMessage, " ("_s, String(debugDescription.get()), ")"_s);

        auto targetFrameID = page->focusedFrame() ? page->focusedFrame()->frameID() : page->mainFrame()->frameID();
        page->addConsoleMessage(targetFrameID, MessageSource::JS, MessageLevel::Error, makeString("Digital Credential request failed: "_s, consoleMessage));
    }

    [self completeWith:makeUnexpected(exceptionData)];
}

- (void)dismiss
{
    [_presentmentController cancelRequest];
    _presentmentController = nil;

    if (_completionHandler)
        _completionHandler(makeUnexpected(WebCore::ExceptionData { ExceptionCode::OperationError, "The digital credential request was cancelled."_s }));

    if ([self.delegate respondsToSelector:@selector(digitalCredentialsPickerDidDismiss:)])
        [self.delegate digitalCredentialsPickerDidDismiss:self];
}

- (void)completeWith:(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)result
{
    if (!_completionHandler) {
        LOG(DigitalCredentials, "Completion handler is null.");
        [self dismiss];
        return;
    }

    _completionHandler(WTF::move(result));

    [self dismiss];
}

@end // WKDigitalCredentialsPicker

#endif // ENABLE(WEB_AUTHN)
