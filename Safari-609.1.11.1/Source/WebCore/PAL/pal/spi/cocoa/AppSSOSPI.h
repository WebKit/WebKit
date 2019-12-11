/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if HAVE(APP_SSO)

#if USE(APPLE_INTERNAL_SDK)

#import <AppSSO/AppSSO.h>

#else

#if __has_include(<UIKit/UIKit.h>)
#import <UIKit/UIKit.h>
typedef UIViewController * SOAuthorizationViewController;
#elif __has_include(<AppKit/AppKit.h>)
typedef id SOAuthorizationViewController;
#endif

NS_ASSUME_NONNULL_BEGIN

#define kSOErrorAuthorizationPresentationFailed -7

extern NSErrorDomain const SOErrorDomain;

typedef NSString * SOAuthorizationOperation;

@class SOAuthorization;
@class SOAuthorizationParameters;

@protocol SOAuthorizationDelegate <NSObject>
@optional
- (void)authorizationDidNotHandle:(SOAuthorization *)authorization;
- (void)authorizationDidCancel:(SOAuthorization *)authorization;
- (void)authorizationDidComplete:(SOAuthorization *)authorization;
- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPAuthorizationHeaders:(NSDictionary<NSString *, NSString *> *)httpAuthorizationHeaders;
- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPResponse:(NSHTTPURLResponse *)httpResponse httpBody:(NSData *)httpBody;
- (void)authorization:(SOAuthorization *)authorization didCompleteWithError:(NSError *)error;
- (void)authorization:(SOAuthorization *)authorization presentViewController:(SOAuthorizationViewController)viewController withCompletion:(void (^)(BOOL success, NSError * _Nullable error))completion;
@end

typedef NSString * SOAuthorizationOption;
extern SOAuthorizationOption const SOAuthorizationOptionUserActionInitiated;
extern SOAuthorizationOption const SOAuthorizationOptionInitiatorOrigin;
extern SOAuthorizationOption const SOAuthorizationOptionInitiatingAction;

typedef NS_ENUM(NSInteger, SOAuthorizationInitiatingAction) {
    SOAuthorizationInitiatingActionRedirect,
    SOAuthorizationInitiatingActionPopUp,
    SOAuthorizationInitiatingActionKerberos,
    SOAuthorizationInitiatingActionSubframe,
};

@interface SOAuthorization : NSObject
@property (weak) id<SOAuthorizationDelegate> delegate;
@property (retain, nullable) dispatch_queue_t delegateDispatchQueue;
@property (copy, nonatomic) NSDictionary *authorizationOptions;
@property (nonatomic) BOOL enableEmbeddedAuthorizationViewController;
+ (BOOL)canPerformAuthorizationWithURL:(NSURL *)url responseCode:(NSInteger)responseCode;
+ (BOOL)canPerformAuthorizationWithURL:(NSURL *)url responseCode:(NSInteger)responseCode useInternalExtensions:(BOOL)useInternalExtensions;
- (void)beginAuthorizationWithURL:(NSURL *)url httpHeaders:(NSDictionary <NSString *, NSString *> *)httpHeaders httpBody:(NSData *)httpBody;
- (void)beginAuthorizationWithOperation:(nullable SOAuthorizationOperation)operation url:(NSURL *)url httpHeaders:(NSDictionary <NSString *, NSString *> *)httpHeaders httpBody:(NSData *)httpBody;
- (void)beginAuthorizationWithParameters:(SOAuthorizationParameters *)parameters;
- (void)cancelAuthorization;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(APP_SSO)
