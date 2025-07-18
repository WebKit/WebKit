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

#if HAVE(DIGITAL_CREDENTIALS_UI)

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
#import <WebCore/DigitalCredentialRequest.h>
#import <WebCore/DigitalCredentialsProtocols.h>
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DigitalCredentialsResponseData.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/ValidatedMobileDocumentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentController.h>
#import <WebKit/WKIdentityDocumentPresentmentError.h>
#import <WebKit/WKIdentityDocumentPresentmentMobileDocumentRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRawRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentRequest.h>
#import <wtf/BlockPtr.h>
#import <wtf/Expected.h>
#import <wtf/JSONValues.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringCommon.h>
#import <wtf/text/WTFString.h>

#import "WebKitSwiftSoftLink.h"

using WebCore::ExceptionCode;
using WebCore::IdentityCredentialProtocol;

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
@property (nonatomic, assign) IdentityCredentialProtocol protocol;

- (instancetype)initWithRequestDataBytes:(NSData *)requestDataBytes protocol:(IdentityCredentialProtocol)protocol;

@end

@implementation WKRequestDataResult

- (instancetype)initWithRequestDataBytes:(NSData *)requestDataBytes protocol:(IdentityCredentialProtocol)protocol
{
    self = [super init];
    if (self) {
        _requestDataBytes = requestDataBytes;
        _protocol = protocol;
    }
    return self;
}

@end // WKRequestDataResult

#pragma mark - Adapter functions

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
        RetainPtr mappedDocumentRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequestInstance() initWithDocumentType:documentType.get() namespaces:namespaces.get()]);
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

static RetainPtr<NSArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *>> mapRequestAuthentications(const Vector<WebCore::CertificateInfo>& requestAuthentications)
{
    RetainPtr<NSMutableArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *>> mappedRequestAuthenticationCertificates = adoptNS([[NSMutableArray alloc] init]);

    for (auto&& certificateInfo : requestAuthentications) {
        RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *>> mappedCertificateChain = adoptNS([[NSMutableArray alloc] init]);
        auto certificateChain = adoptCF(SecTrustCopyCertificateChain(certificateInfo.trust().get()));

        CFIndex count = CFArrayGetCount(certificateChain.get());
        for (CFIndex i = 0; i < count; ++i) {
            auto certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(certificateChain.get(), i));
            RetainPtr mappedCertificate = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestAuthenticationCertificateInstance() initWithCertificate:certificate]);
            [mappedCertificateChain addObject:mappedCertificate.get()];
        }
        [mappedRequestAuthenticationCertificates addObject:mappedCertificateChain.get()];
    }

    return mappedRequestAuthenticationCertificates;
}

#pragma mark - WKDigitalCredentialsPicker

@implementation WKDigitalCredentialsPicker {
    WeakPtr<WebKit::WebPageProxy> _page;
    RetainPtr<WKDigitalCredentialsPickerDelegate> _digitalCredentialsPickerDelegate;
    RetainPtr<WKIdentityDocumentPresentmentController> _presentmentController;
    WeakObjCPtr<id<WKDigitalCredentialsPickerDelegate>> _delegate;
    WeakObjCPtr<WKWebView> _webView;
    CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> _completionHandler;
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

- (id<WKDigitalCredentialsPickerDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<WKDigitalCredentialsPickerDelegate>)delegate
{
    _delegate = delegate;
}

- (CocoaWindow *)presentationAnchor
{
    return [_webView window];
}

- (void)fetchRawRequestsWithCompletionHandler:(void (^)(NSArray<WKIdentityDocumentPresentmentRawRequest *> *))completionHandler
{
    LOG(DigitalCredentials, "Fetching raw requests from web content process");
    _page->fetchRawDigitalCredentialRequests([completionHandler = makeBlockPtr(completionHandler)](auto&& unvalidatedRequests) {
        RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentRawRequest *>> rawRequests = adoptNS([[NSMutableArray alloc] init]);

        for (auto&& unvalidatedRequest : unvalidatedRequests) {
            if (!std::holds_alternative<WebCore::MobileDocumentRequest>(unvalidatedRequest)) {
                LOG(DigitalCredentials, "Incoming request is not a supported type, skipping for return to raw request");
                continue;
            }

            const auto &mobileDocumentRequest = std::get<WebCore::MobileDocumentRequest>(unvalidatedRequest);
            RetainPtr deviceRequest = mobileDocumentRequest.deviceRequest.createNSString();
            RetainPtr encryptionInfo = mobileDocumentRequest.encryptionInfo.createNSString();

            RetainPtr<NSDictionary<NSString *, id>> jsonRequest = @{
                @"deviceRequest": deviceRequest.get(),
                @"encryptionInfo": encryptionInfo.get()
            };

            NSError *error = nil;
            RetainPtr requestDataBytes = [NSJSONSerialization dataWithJSONObject:jsonRequest.get() options:0 error:&error];

            if (!requestDataBytes) {
                LOG(DigitalCredentials, "Failed to serialize JSON for raw request: %s", error.localizedDescription.UTF8String);
                continue;
            }

            RetainPtr rawRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRawRequestInstance() initWithRequestProtocol:@"org.iso.mdoc" requestData:requestDataBytes.get()]);
            [rawRequests addObject:rawRequest.get()];
        }

        completionHandler(rawRequests.get());
    });
}

- (void)presentWithRequestData:(const WebCore::DigitalCredentialsRequestData &)requestData completionHandler:(CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> &&)completionHandler
{
    LOG(DigitalCredentials, "WKDigitalCredentialsPicker: Digital Credentials - Presenting with request data: %s.", requestData.topOrigin.toString().utf8().data());
    _completionHandler = WTFMove(completionHandler);

    ASSERT(!_presentmentController);

    [self setupPresentmentController];

    _digitalCredentialsPickerDelegate = adoptNS([[WKDigitalCredentialsPickerDelegate alloc] initWithDigitalCredentialsPickerDelegate:self]);

    [self performRequest:requestData];

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

- (void)performRequest:(const WebCore::DigitalCredentialsRequestData &)requestData
{
    RetainPtr<NSMutableArray<WKIdentityDocumentPresentmentMobileDocumentRequest *>> mobileDocumentRequests = adoptNS([[NSMutableArray alloc] init]);

    for (auto&& request : requestData.requests) {
        if (!std::holds_alternative<WebCore::ValidatedMobileDocumentRequest>(request)) {
            LOG(DigitalCredentials, "Incoming request is not a supported type.");
            continue;
        }

        auto validatedRequest = std::get<WebCore::ValidatedMobileDocumentRequest>(request);

        RetainPtr<NSArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *>> presentmentRequests = mapPresentmentRequests(validatedRequest.presentmentRequests);
        RetainPtr<NSArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *>> authenticationCertificates = mapRequestAuthentications(validatedRequest.requestAuthentications);

        RetainPtr mobileDocumentRequest = [WebKit::allocWKIdentityDocumentPresentmentMobileDocumentRequestInstance() initWithPresentmentRequests:presentmentRequests.get() authenticationCertificates:authenticationCertificates.get()];
        [mobileDocumentRequests addObject:mobileDocumentRequest.get()];
    }

    RetainPtr mappedOrigin = requestData.topOrigin.toURL().createNSURL();
    RetainPtr mappedRequest = adoptNS([WebKit::allocWKIdentityDocumentPresentmentRequestInstance() initWithOrigin:mappedOrigin.get() mobileDocumentRequests:mobileDocumentRequests.get()]);

    if (![mobileDocumentRequests count]) {
        LOG(DigitalCredentials, "No supported mobile document requests to present.");
        WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "No supported document requests to present."_s };
        [self completeWith:makeUnexpected(exceptionData)];
        return;
    }

    [_presentmentController performRequest:mappedRequest.get() completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKDigitalCredentialsPicker>(self)](WKIdentityDocumentPresentmentResponse *response, NSError *error) {
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
        WebCore::ExceptionData exceptionData = { ExceptionCode::UnknownError, "No response from document provider."_s };
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

        String responseData = base64URLEncodeToString(span(response.responseData));

        if (responseData.isNull()) {
            LOG(DigitalCredentials, "Failed to encode response bytes to URL-safe Base64.");
            WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Document provider returned an invalid format."_s };
            [self completeWith:makeUnexpected(exceptionData)];
            return;
        }

        LOG(DigitalCredentials, "The document provider returned response data: %s.", responseData.utf8().data());
        RetainPtr<NSString> protocol = response.protocolString;

        if ([protocol isEqualToString:@"org.iso.mdoc"]) {
            Ref object = JSON::Object::create();
            object->setString("response"_s, responseData);
            auto responseObject = WebCore::DigitalCredentialsResponseData(IdentityCredentialProtocol::OrgIsoMdoc, object->toJSONString());
            [self completeWith:WTFMove(responseObject)];
        } else if ([protocol isEqualToString:@"openid4vp"]) {
            WebCore::ExceptionData exceptionData = { ExceptionCode::NotSupportedError, "OpenID4VP protocol is not supported."_s };
            [self completeWith:makeUnexpected(exceptionData)];
        } else {
            LOG(DigitalCredentials, "Unknown protocol response from document provider. Can't convert it %s.", [protocol UTF8String]);
            WebCore::ExceptionData exceptionData = { ExceptionCode::TypeError, "Unknown protocol response from document."_s };
            [self completeWith:makeUnexpected(exceptionData)];
        }
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
    case WKIdentityDocumentPresentmentErrorCancelled:
        exceptionData = { ExceptionCode::AbortError, "Request was cancelled."_s };
        break;
    default:
        LOG(DigitalCredentials, "The error code was not in the case statement? %d.", error.code);
        exceptionData = { ExceptionCode::UnknownError, "Some other error."_s };
        RetainPtr debugDescription = error.userInfo[NSDebugDescriptionErrorKey] ?: error.userInfo[NSLocalizedDescriptionKey];
        LOG(DigitalCredentials, "Internal error: %@", debugDescription ? debugDescription.get() : @"Unknown error with no description.");
        break;
    }

    if (_page) {
        String consoleMessage = exceptionData.message;
        RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
        if ([debugDescription length])
            consoleMessage = makeString(consoleMessage, " ("_s, String(debugDescription.get()), ")"_s);

        auto targetFrameID = _page->focusedFrame() ? _page->focusedFrame()->frameID() : _page->mainFrame()->frameID();
        auto logLevel = exceptionData.code == ExceptionCode::AbortError ? MessageLevel::Warning : MessageLevel::Error;

        _page->addConsoleMessage(targetFrameID, MessageSource::JS, logLevel, makeString("Digital Credential request failed: "_s, consoleMessage));
    }

    [self completeWith:makeUnexpected(exceptionData)];
}

- (void)dismiss
{
    [_presentmentController cancelRequest];
    _presentmentController = nil;

    if ([self.delegate respondsToSelector:@selector(digitalCredentialsPickerDidDismiss:)])
        [self.delegate digitalCredentialsPickerDidDismiss:self];
}

- (void)completeWith:(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)result
{
    if (!_completionHandler) {
        LOG(DigitalCredentials, "Completion handler is null.");
        [self dismiss];
        return;
    }

    _completionHandler(WTFMove(result));

    [self dismiss];
}

@end // WKDigitalCredentialsPicker

#endif // HAVE(DIGITAL_CREDENTIALS_UI)
