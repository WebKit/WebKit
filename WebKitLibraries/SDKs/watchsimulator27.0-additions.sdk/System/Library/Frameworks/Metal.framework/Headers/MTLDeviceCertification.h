//
//  MTLDeviceCertification.h
//  Metal
//
//  Copyright (c) 2014-2024 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Foundation/Foundation.h>


typedef NSInteger NSDeviceCertification NS_TYPED_ENUM API_AVAILABLE(ios(18.0), macos(15.0));
API_AVAILABLE(ios(18.0), macos(15.0)) extern NSDeviceCertification const NSDeviceCertificationiPhonePerformanceGaming;

typedef NSInteger NSProcessPerformanceProfile NS_TYPED_ENUM API_AVAILABLE(ios(18.0), macos(15.0));
API_AVAILABLE(ios(18.0), macos(15.0)) extern NSProcessPerformanceProfile const NSProcessPerformanceProfileDefault;
API_AVAILABLE(ios(18.0), macos(15.0)) extern NSProcessPerformanceProfile const NSProcessPerformanceProfileSustained;

API_AVAILABLE(ios(18.0), macos(15.0)) extern const NSNotificationName NSProcessInfoPerformanceProfileDidChangeNotification;

API_AVAILABLE(ios(18.0), macos(15.0))
@interface NSProcessInfo (NSDeviceCertification)

- (BOOL)isDeviceCertifiedFor:(NSDeviceCertification)performanceTier;

- (BOOL)hasPerformanceProfile:(NSProcessPerformanceProfile)performanceProfile;

@end

