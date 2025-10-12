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

#if HAVE(DIGITAL_CREDENTIALS_UI)

#import "WKIdentityDocumentPresentmentDelegate.h"

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif
#import <WebCore/DigitalCredential.h>
#import <wtf/Forward.h>

OBJC_CLASS WKWebView;

namespace WebKit {
class WebPageProxy;
}

@class WKDigitalCredentialsPicker;

namespace WebCore {
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct ExceptionData;
struct OpenID4VPRequest;
}

@protocol WKDigitalCredentialsPickerDelegate <NSObject>
@optional
- (void)digitalCredentialsPickerDidPresent:(WKDigitalCredentialsPicker *)picker;
- (void)digitalCredentialsPickerDidDismiss:(WKDigitalCredentialsPicker *)picker;
@end

@interface WKDigitalCredentialsPicker : NSObject <WKDigitalCredentialsPickerDelegate, WKIdentityDocumentPresentmentDelegate>

@property (nonatomic, weak) id<WKDigitalCredentialsPickerDelegate> delegate;

- (instancetype)initWithView:(WKWebView *)view page:(WebKit::WebPageProxy*)page;

- (void)presentWithRequestData:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler;
- (void)dismissWithCompletionHandler:(CompletionHandler<void(bool)>&&)completionHandler;
@end

#endif // HAVE(DIGITAL_CREDENTIALS_UI)
