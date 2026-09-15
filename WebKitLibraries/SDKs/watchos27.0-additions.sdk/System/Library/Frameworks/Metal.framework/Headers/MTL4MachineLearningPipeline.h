//
//  MTL4MachineLearningPipeline.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4MachineLearningPipeline_h
#define MTL4MachineLearningPipeline_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4PipelineState.h>
#import <Metal/MTL4FunctionDescriptor.h>
#import <Metal/MTLTensor.h>

NS_ASSUME_NONNULL_BEGIN

/// Description for a machine learning pipeline state.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4MachineLearningPipelineDescriptor : MTL4PipelineDescriptor

/// Assigns an optional string that helps identify pipeline states you create from this descriptor.
@property (nullable, copy, nonatomic) NSString* label;

/// Assigns the function that the machine learning pipeline you create from this descriptor executes.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* machineLearningFunctionDescriptor;

/// Sets the dimension of an input tensor at a buffer index.
///
/// When the compiled model declares the input as unranked (unknown rank), any concrete `dimensions` are accepted.
/// Otherwise `dimensions.rank` must equal the model's input rank, and each static (non `-1`) dimension must match.
///
/// - Parameters:
///   - dimensions: the dimensions of the tensor.
///   - bufferIndex: Index of the tensor to modify.
- (void)setInputDimensions:(MTLTensorExtents * _Nullable)dimensions atBufferIndex:(NSInteger)bufferIndex;

/// Sets the dimensions of multiple input tensors on a range of buffer bindings.
///
/// Use this method to specify the dimensions of multiple input tensors at a range of indices in a single call.
///
/// You can indicate that any tensors in the range have unspecified dimensions by providing `NSNull` at the their
/// corresponding index location in the array.
///
/// - Important: The range's length property needs to match the number of dimensions you provide. Specifically,
/// `range.length` needs to match `dimensions.count`.
///
/// - Parameters:
///   - dimensions: An array of tensor extents.
///   - range: The range of inputs of the `dimensions` argument.
///   The range's `length` needs to match the dimensions' `count` property.
- (void)setInputDimensions:(NSArray<MTLTensorExtents *> *)dimensions withRange:(NSRange)range;

/// Obtains the dimensions of the input tensor at `bufferIndex` if set, `nil` otherwise.
- (MTLTensorExtents * _Nullable)inputDimensionsAtBufferIndex:(NSInteger)bufferIndex;

/// Resets the descriptor to its default values.
- (void)reset;

@end

/// Represents reflection information for a machine learning pipeline state.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0)) NS_SWIFT_SENDABLE
@interface MTL4MachineLearningPipelineReflection : NSObject

/// Describes every input and output of the pipeline.
@property (nonnull, readonly) NSArray<id<MTLBinding>> *bindings;

@end

/// A pipeline state that you can use with machine-learning encoder instances.
///
/// See ``MTL4MachineLearningCommandEncoder`` for more information.
API_AVAILABLE(macos(26.0), ios(26.0)) NS_SWIFT_SENDABLE
@protocol MTL4MachineLearningPipelineState <MTLAllocation, NSObject>

/// Queries the string that helps identify this object.
@property (nullable, readonly) NSString *label;

/// Returns the device the pipeline state belongs to.
@property (readonly) id <MTLDevice> device;

/// Returns reflection information for this machine learning pipeline state.
@property (nullable, readonly) MTL4MachineLearningPipelineReflection *reflection;

/// Obtain the size of the heap, in bytes, this pipeline requires during the execution.
///
/// Use this value to allocate a ``MTLHeap`` instance of sufficient size that you can then provide to
/// ``MTL4MachineLearningCommandEncoder/dispatchNetworkWithIntermediatesHeap:``.
///
/// Metal uses this heap to store intermediate data as it executes the pipeline. It is your responsibility to provide
/// a heap at least as large as this property requests.
@property (readonly) NSUInteger intermediatesHeapSize;


@end

NS_ASSUME_NONNULL_END

#endif // MTL4MachineLearningPipeline_h
