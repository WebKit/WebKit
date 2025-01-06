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

#if HAVE(ASD_INSTALL_WEB_ATTRIBUTION_SERVICE)
#import <AppStoreDaemon/ASDInstallWebAttributionService.h>
#endif

#import <AppStoreDaemon/ASDInstallWebAttributionParamsConfig.h>

#else // USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

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

#if HAVE(ASD_INSTALL_WEB_ATTRIBUTION_SERVICE)
@interface ASDInstallWebAttributionService : NSObject
@property (class, readonly) ASDInstallWebAttributionService *sharedInstance;
- (void)addInstallWebAttributionParamsWithConfig:(ASDInstallWebAttributionParamsConfig *)config completionHandler:(nullable void (^)(NSError *error))completionHandler;
@end
#endif

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

// FIXME: Move these to the !USE(APPLE_INTERNAL_SDK) section once rdar://137446922 is complete.
#if HAVE(AD_ATTRIBUTION_KIT_PRIVATE_BROWSING)
NS_ASSUME_NONNULL_BEGIN
@interface ASDInstallWebAttributionService (Staging_for_137446922)
- (void)removeInstallWebAttributionParamsFromPrivateBrowsingSessionId:(NSUUID *)sessionId completionHandler:(nullable void (^)(NSError *__nullable error))completionHandler;
@end
@interface ASDInstallWebAttributionParamsConfig (Staging_for_137446922)
@property (nullable, nonatomic, strong) NSUUID *privateBrowsingSessionId;
@end
NS_ASSUME_NONNULL_END
#endif // HAVE(AD_ATTRIBUTION_KIT_PRIVATE_BROWSING)

#endif // HAVE(SKADNETWORK_v4)
