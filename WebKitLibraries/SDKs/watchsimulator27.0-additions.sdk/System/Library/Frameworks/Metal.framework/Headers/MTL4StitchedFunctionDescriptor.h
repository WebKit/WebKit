//
//  MTL4StitchedFunctionDescriptor.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4StitchedFunctionDescriptor_h
#define MTL4StitchedFunctionDescriptor_h

#import <Metal/MTLDefines.h>

#import <Metal/MTL4FunctionDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

@class MTLFunctionStitchingGraph;

/// Groups together properties that describe a shader function suitable for stitching.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4StitchedFunctionDescriptor : MTL4FunctionDescriptor

/// Sets the graph representing how to stitch functions together.
@property (nullable, copy, nonatomic) MTLFunctionStitchingGraph* functionGraph;

/// Configures an array of function descriptors with references to functions that contribute to the stitching process.
@property (nullable, copy, nonatomic) NSArray<MTL4FunctionDescriptor*>* functionDescriptors;
@end

NS_ASSUME_NONNULL_END

#endif // MTL4StitchedFunctionDescriptor_h
