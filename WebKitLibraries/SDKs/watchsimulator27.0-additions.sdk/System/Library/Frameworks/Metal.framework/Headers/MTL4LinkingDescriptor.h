//
//  MTL4LinkingDescriptor.h
//  Metal
//
//  Copyright © 2025 Apple, Inc. All rights reserved.
//

#ifndef MTL4LinkingDescriptor_h
#define MTL4LinkingDescriptor_h

#import <Metal/MTLDefines.h>


#import <Foundation/Foundation.h>

#import <Metal/MTLDevice.h>
#import <Metal/MTL4ComputePipeline.h>
#import <Metal/MTL4RenderPipeline.h>
#import <Metal/MTL4FunctionDescriptor.h>
#import <Metal/MTL4BinaryFunction.h>

#import <Metal/MTLDynamicLibrary.h>

@class MTL4BinaryFunction;
@class MTLDynamicLibrary;

NS_ASSUME_NONNULL_BEGIN
/// Groups together properties to drive a static linking process.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4StaticLinkingDescriptor : NSObject<NSCopying>

/// Provides an array of functions to link at the Metal IR level.
@property (readwrite, nonatomic, copy, nullable) NSArray<MTL4FunctionDescriptor*> *functionDescriptors;

/// Provides an array of private functions to link at the Metal IR level.
///
/// You specify private functions to link separately from ``functionDescriptors`` because pipelines don't export private functions as ``MTLFunctionHandle`` instances.
/// - Note: You can link private functions even when your ``MTLDevice`` doesn't support function pointers.
@property (readwrite, nonatomic, copy, nullable) NSArray<MTL4FunctionDescriptor*> *privateFunctionDescriptors;

/// Assigns groups of functions to match call-site attributes in shader code.
///
/// Function groups help the compiler reduce the number of candidate functions it needs to evaluate for shader function calls, potentially increasing runtime performance.
@property (readwrite, nonatomic, copy, nullable) NSDictionary<NSString*, NSArray<MTL4FunctionDescriptor*>*> *groups;

@end


/// Groups together properties to drive the dynamic linking process of a pipeline stage.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4PipelineStageDynamicLinkingDescriptor : NSObject<NSCopying>

/// Limits the maximum depth of the call stack for indirect function calls in the pipeline stage function.
@property (readwrite, nonatomic) NSUInteger maxCallStackDepth;

/// Provides the array of binary functions to link.
///
/// Binary functions are shader functions that you compile from Metal IR to machine code ahead of time
/// using instances of ``MTL4Compiler``.
@property (readwrite, nonatomic, copy, nullable) NSArray<id<MTL4BinaryFunction>> *binaryLinkedFunctions;

/// Provides an array of dynamic libraries the compiler loads when it builds the pipeline.
@property (readwrite, nonnull, nonatomic, copy) NSArray<id<MTLDynamicLibrary>>* preloadedLibraries;

@end

/// Groups together properties that provide linking properties for render pipelines.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPipelineDynamicLinkingDescriptor : NSObject<NSCopying>

/// Controls properties for linking the vertex stage of the render pipeline.
@property (readonly, nonatomic) MTL4PipelineStageDynamicLinkingDescriptor* vertexLinkingDescriptor;

/// Controls properties for linking the fragment stage of the render pipeline.
@property (readonly, nonatomic) MTL4PipelineStageDynamicLinkingDescriptor* fragmentLinkingDescriptor;

/// Controls properties for linking the tile stage of the render pipeline.
@property (readonly, nonatomic) MTL4PipelineStageDynamicLinkingDescriptor* tileLinkingDescriptor;

/// Controls properties for link the object stage of the render pipeline.
@property (readonly, nonatomic) MTL4PipelineStageDynamicLinkingDescriptor* objectLinkingDescriptor;

/// Controls properties for linking the mesh stage of the render pipeline.
@property (readonly, nonatomic) MTL4PipelineStageDynamicLinkingDescriptor* meshLinkingDescriptor;

@end

NS_ASSUME_NONNULL_END

#endif // MTL4LinkingDescriptor_h
