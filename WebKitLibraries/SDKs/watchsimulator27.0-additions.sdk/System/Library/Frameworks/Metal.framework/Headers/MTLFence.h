//
//  MTLFence.h
//  Metal
//
//  Copyright (c) 2016 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>

API_AVAILABLE(macos(10.13), ios(10.0)) NS_SWIFT_SENDABLE
@protocol MTLFence <NSObject>
@property (nonnull, readonly) id <MTLDevice> device;

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

@end
