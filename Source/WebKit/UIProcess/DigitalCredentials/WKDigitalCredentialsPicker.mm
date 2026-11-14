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
#import "DigitalCredentialsPresentment.h"
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
#import <wtf/Borrow.h>
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
    page->fetchRawDigitalCredentialRequests([completionHandler = makeBlockPtr(completionHandler)](WebCore::DigitalCredentialsRawRequests&& unvalidatedRequests) {
        RetainPtr rawRequests = WebKit::createPresentmentRawRequests(unvalidatedRequests);
        completionHandler(rawRequests ? rawRequests.get() : @[]);
    });
}

- (void)presentWithRequestData:(const WebCore::DigitalCredentialsRequestData &)requestData completionHandler:(CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> &&)completionHandler
{
    LOG_WITH_STREAM(DigitalCredentials, stream << "WKDigitalCredentialsPicker: Digital Credentials - Presenting with request data: "_s << requestData.topOrigin.toString() << "."_s);
    _completionHandler = WTF::move(completionHandler);

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
    RetainPtr request = WebKit::createPresentmentRequest(requestData);
    if (!request) {
        [self completeWith:makeUnexpected(WebCore::ExceptionData { ExceptionCode::TypeError, "No supported credential requests to present."_s })];
        return;
    }
    [self presentRequest:request.get()];
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
    auto result = WebKit::responseDataFromPresentment(response, error);
    if (!result && error) {
        if (RefPtr page = _page) {
            String consoleMessage = result.error().message;
            RetainPtr debugDescription = dynamic_objc_cast<NSString>(error.userInfo[NSDebugDescriptionErrorKey]);
            if ([debugDescription length])
                consoleMessage = makeString(consoleMessage, " ("_s, String(debugDescription.get()), ")"_s);

            auto targetFrameID = page->focusedFrame() ? page->focusedFrame()->frameID() : page->mainFrame()->frameID();
            page->addConsoleMessage(targetFrameID, MessageSource::JS, MessageLevel::Error, makeString("Digital Credential request failed: "_s, consoleMessage));
        }
    }
    [self completeWith:WTF::move(result)];
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
