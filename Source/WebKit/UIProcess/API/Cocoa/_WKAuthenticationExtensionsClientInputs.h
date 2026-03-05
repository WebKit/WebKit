/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Staging protocols for Safari's category to use
@protocol _WKAuthenticationPRFInputValuesStaging
@property (nullable, nonatomic, copy) NSData *prfSalt1;
@property (nullable, nonatomic, copy) NSData *prfSalt2;
@end

@protocol _WKAuthenticationExtensionsLargeBlobInputsStaging
@property (nullable, nonatomic, copy) NSString *support;
@property (nonatomic) BOOL read;
@property (nullable, nonatomic, copy) NSData *write;
@end

// Concrete classes that conform to staging protocols
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKAuthenticationPRFInputValues : NSObject <_WKAuthenticationPRFInputValuesStaging>
@property (nullable, nonatomic, copy) NSData *prfSalt1;
@property (nullable, nonatomic, copy) NSData *prfSalt2;
@end

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKAuthenticationExtensionsLargeBlobInputs : NSObject <_WKAuthenticationExtensionsLargeBlobInputsStaging>
@property (nullable, nonatomic, copy) NSString *support;
@property (nonatomic) BOOL read;
@property (nullable, nonatomic, copy) NSData *write;
@end

WK_CLASS_AVAILABLE(macos(12.0), ios(15.0))
@interface _WKAuthenticationExtensionsClientInputs : NSObject

@property (nullable, nonatomic, copy) NSString *appid;
@property (nonatomic) BOOL prf;
@property (nonatomic, nullable, copy) NSDictionary<NSData *, id <_WKAuthenticationPRFInputValuesStaging>> *evalByCredential;
@property (nullable, nonatomic, copy) id <_WKAuthenticationPRFInputValuesStaging> eval;
@property (nullable, nonatomic, copy) id <_WKAuthenticationExtensionsLargeBlobInputsStaging> largeBlob;

@end

NS_ASSUME_NONNULL_END
