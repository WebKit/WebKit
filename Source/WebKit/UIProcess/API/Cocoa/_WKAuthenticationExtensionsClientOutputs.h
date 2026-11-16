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

// Staging protocol for Safari's category to use
@protocol _WKAuthenticationExtensionsLargeBlobOutputsStaging
@property (nonatomic, readonly) BOOL supported;
@property (nullable, nonatomic, readonly, copy) NSData *blob;
@property (nonatomic, readonly) BOOL written;
@end

// Concrete class that conforms to the staging protocol
WK_CLASS_AVAILABLE(macos(26.4), ios(26.4))
@interface _WKAuthenticationExtensionsLargeBlobOutputs : NSObject <_WKAuthenticationExtensionsLargeBlobOutputsStaging>
@property (nonatomic, readonly) BOOL supported;
@property (nullable, nonatomic, readonly, copy) NSData *blob;
@property (nonatomic, readonly) BOOL written;
@end

WK_CLASS_AVAILABLE(macos(12.0), ios(15.0))
@interface _WKAuthenticationExtensionsClientOutputs : NSObject

@property (nonatomic, readonly) BOOL appid;
@property (nonatomic, readonly) BOOL prfEnabled;
@property (nullable, nonatomic, readonly, copy) NSData *prfFirst;
@property (nullable, nonatomic, readonly, copy) NSData *prfSecond;
@property (nullable, nonatomic, readonly, strong) id <_WKAuthenticationExtensionsLargeBlobOutputsStaging> largeBlob;

@end

NS_ASSUME_NONNULL_END
