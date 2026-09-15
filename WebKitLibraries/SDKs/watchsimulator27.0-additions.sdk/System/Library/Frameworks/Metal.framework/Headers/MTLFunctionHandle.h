//
//  MTLFunctionHandle.h
//  Framework
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLLibrary.h>

API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) NS_SWIFT_SENDABLE
@protocol MTLFunctionHandle <NSObject>
@property (readonly) MTLFunctionType functionType;
@property (readonly, nonnull) NSString* name;
@property (readonly, nonnull) id<MTLDevice> device;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Intersection Function Buffer.
 @discussion The handle must have been created from an intersection function annotated with the `intersection_function_buffer` tag.
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(26.0), ios(26.0));

@end

#import <Metal/MTLVisibleFunctionTable.h>

