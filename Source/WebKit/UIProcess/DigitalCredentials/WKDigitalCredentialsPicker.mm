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

#include "config.h"

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKDigitalCredentialsPickerAdditions.mm>)
#import <WebKitAdditions/WKDigitalCredentialsPickerAdditions.mm>

#else
#import "WKDigitalCredentialsPicker.h"

#if HAVE(DIGITAL_CREDENTIALS_UI)

#import "Logging.h"
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DigitalCredentialsResponseData.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/IdentityCredentialProtocol.h>
#import <wtf/Expected.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#endif

using WebCore::ExceptionData;
using WebCore::ExceptionCode;
using WebCore::IdentityCredentialProtocol;

#pragma clang diagnostic push

#pragma mark - WKDigitalCredentialsPickerDelegate

@interface WKDigitalCredentialsPickerDelegate : NSObject {
@protected
    WeakObjCPtr<id<WKDigitalCredentialsPickerDelegate>> _digitalCredentialsPickerDelegate;
}

- (instancetype _Nonnull)initWithDigitalCredentialsPickerDelegate:(id<WKDigitalCredentialsPickerDelegate> _Nonnull)digitalCredentialsPickerDelegate;

@end // WKDigitalCredentialsPickerDelegate

@implementation WKDigitalCredentialsPickerDelegate

- (instancetype _Nonnull)initWithDigitalCredentialsPickerDelegate:(id<WKDigitalCredentialsPickerDelegate> _Nonnull)digitalCredentialsPickerDelegate
{
    if (!(self = [super init]))
        return nil;

    _digitalCredentialsPickerDelegate = digitalCredentialsPickerDelegate;

    return self;
}

@end // WKDigitalCredentialsPickerDelegate

@interface WKRequestDataResult : NSObject

@property (nonatomic, strong, nonnull) NSData *requestDataBytes;
@property (nonatomic, assign) IdentityCredentialProtocol protocol;

- (instancetype _Nonnull)initWithRequestDataBytes:(NSData *_Nonnull)requestDataBytes protocol:(IdentityCredentialProtocol)protocol;

@end

@implementation WKRequestDataResult

- (instancetype _Nonnull)initWithRequestDataBytes:(NSData *_Nonnull)requestDataBytes protocol:(IdentityCredentialProtocol)protocol
{
    self = [super init];
    if (self) {
        _requestDataBytes = requestDataBytes;
        _protocol = protocol;
    }
    return self;
}

@end // WKRequestDataResult

#pragma mark - WKDigitalCredentialsPicker

@implementation WKDigitalCredentialsPicker {
    WeakObjCPtr<WKWebView> _webView;
    WeakObjCPtr<id<WKDigitalCredentialsPickerDelegate>> _delegate;
    WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> _completionHandler;
    RetainPtr<WKDigitalCredentialsPickerDelegate> _digitalCredentialsPickerDelegate;
}

- (instancetype _Nonnull)initWithView:(WKWebView *_Nonnull)view
{
    self = [super init];
    if (self)
        _webView = view;
    return self;
}

- (id<WKDigitalCredentialsPickerDelegate> _Nullable)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<WKDigitalCredentialsPickerDelegate> _Nonnull)delegate
{
    _delegate = delegate;
}

- (void)presentWithRequestData:(const WebCore::DigitalCredentialsRequestData &)requestData completionHandler:(WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData> &&)> &&)completionHandler
{
    LOG(DigitalCredentials, "WKDigitalCredentialsPicker: Digital Credentials - Presenting with request data from origin: %s.", requestData.topOrigin.toString().utf8().data());
    _completionHandler = WTFMove(completionHandler);

    _digitalCredentialsPickerDelegate = adoptNS([[WKDigitalCredentialsPickerDelegate alloc] initWithDigitalCredentialsPickerDelegate:self]);

    WebCore::ExceptionData exceptionData { ExceptionCode::NotSupportedError, "Not implemented."_s };
    [self completeWith:makeUnexpected(exceptionData)];
}

- (void)dismissWithCompletionHandler:(WTF::CompletionHandler<void(bool)> &&)completionHandler
{
    LOG(DigitalCredentials, "WKDigitalCredentialsPicker Dismissing with completion handler.");
    [self dismiss];
    completionHandler(true);
}

#pragma mark - Helper Methods

- (void)dismiss
{
    [self dismissWithResponse:nil];
}

- (void)dismissWithResponse:(NSObject *_Nullable)response
{
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

    _completionHandler = nullptr;
    [self dismiss];
}

@end // WKDigitalCredentialsPicker

#endif // HAVE(DIGITAL_CREDENTIALS_UI)
#endif // USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKDigitalCredentialsPickerAdditions.mm>)
