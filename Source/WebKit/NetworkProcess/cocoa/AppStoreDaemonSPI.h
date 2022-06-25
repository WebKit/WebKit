/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(SKADNETWORK_v4)
#if USE(APPLE_INTERNAL_SDK)

#import <AppStoreDaemon/ASDInstallAttribution.h>
#import <AppStoreDaemon/ASDInstallWebAttributionParamsConfig.h>

#else // USE(APPLE_INTERNAL_SDK)

@interface ASDInstallWebAttributionParamsConfig : NSObject <NSSecureCoding>
typedef NS_ENUM(NSInteger, ASDInstallWebAttributionContext) {
    AttributionTypeDefault = 0,
};
@property (nonatomic, strong) NSNumber *appAdamId;
@property (nonatomic, strong) NSString *adNetworkRegistrableDomain;
@property (nonatomic, strong) NSString *impressionId;
@property (nonatomic, strong) NSString *sourceWebRegistrableDomain;
@property (nonatomic, strong) NSString *version;
@property (nonatomic, assign) ASDInstallWebAttributionContext attributionContext;
@end

@interface ASDInstallAttribution : NSObject
+ (instancetype)sharedInstance;
- (void)addInstallWebAttributionParamsWithConfig:(ASDInstallWebAttributionParamsConfig *)config completionHandler:(void (^)(NSError *error))completionHandler;
@end

#endif // USE(APPLE_INTERNAL_SDK)
#endif // HAVE(SKADNETWORK_v4)
