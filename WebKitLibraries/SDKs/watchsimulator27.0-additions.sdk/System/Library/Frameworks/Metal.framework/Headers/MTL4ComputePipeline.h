//
//  MTL4ComputePipeline.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4ComputePipeline_h
#define MTL4ComputePipeline_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4PipelineState.h>
#import <Metal/MTLComputePipeline.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTL4BinaryFunction;
@protocol MTLDynamicLibrary;
@class MTL4FunctionDescriptor;
@class MTLComputePipelineReflection;
@class MTL4StaticLinkingDescriptor;

/// Describes a compute pipeline state.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4ComputePipelineDescriptor : MTL4PipelineDescriptor

/// A descriptor representing the compute pipeline's function.
///
/// You don't assign instances of ``MTL4FunctionDescriptor`` to this property directly, instead
/// assign an instance of one of its subclasses, such as ``MTL4LibraryFunctionDescriptor``, which
/// represents a function from a Metal library.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* computeFunctionDescriptor;

/// A boolean value indicating whether each dimension of the threadgroup size is a multiple of its
/// corresponding thread execution width.
@property (readwrite, nonatomic) BOOL threadGroupSizeIsMultipleOfThreadExecutionWidth;

/// The maximum total number of threads that Metal can execute in a single threadgroup for the
/// compute function.
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup;

/// The required number of threads per threadgroup for compute dispatches.
///
/// When you set this value, you are responsible for ensuring that the `threadsPerThreadgroup` argument of any compute
/// dispatch matches it.
///
/// Setting this property is optional, except in cases where the pipeline uses *CooperativeTensors*.
///
/// This property's default value is `0`, which disables its effect.
@property(readwrite, nonatomic) MTLSize requiredThreadsPerThreadgroup;

/// A boolean value indicating whether the compute pipeline supports linking binary functions.
@property (readwrite, nonatomic) BOOL supportBinaryLinking;

/// An object that contains information about functions to link to the compute pipeline.
@property (nullable, copy, nonatomic) MTL4StaticLinkingDescriptor* staticLinkingDescriptor;

/// A value indicating whether the pipeline supports Metal indirect command buffers.
@property (readwrite, nonatomic) MTL4IndirectCommandBufferSupportState supportIndirectCommandBuffers;


/// Resets the descriptor to its default values.
- (void)reset;
@end

NS_ASSUME_NONNULL_END

#endif // MTL4ComputePipeline_h
