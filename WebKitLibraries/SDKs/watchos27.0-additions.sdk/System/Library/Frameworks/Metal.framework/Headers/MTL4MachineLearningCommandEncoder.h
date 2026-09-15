//
//  MTL4MachineLearningCommandEncoder.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4MachineLearningCommandEncoder_h
#define MTL4MachineLearningCommandEncoder_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLBlitCommandEncoder.h>

#import <Metal/MTL4CommandEncoder.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTL4ArgumentTable;
@protocol MTL4MachineLearningPipelineState;

/// Encodes commands for dispatching machine learning networks on Apple silicon.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4MachineLearningCommandEncoder <MTL4CommandEncoder>

/// Configures the encoder with a machine learning pipeline state instance.
///
/// The pipeline state instance affects all subsequent Machine Learning commands.
///
/// - Parameters:
///   - pipelineState: A Machine Learning pipeline state instance.
-(void)setPipelineState:(id<MTL4MachineLearningPipelineState>)pipelineState;

/// Sets an argument table for the command encoder's machine learning shader stage.
///
/// The argument table provides inputs to all subsequent Machine Learning dispatches.
/// - Parameters:
///   - argumentTable: An argument table to set on the command encoder's Machine Learning stage.
-(void)setArgumentTable:(nullable id <MTL4ArgumentTable>)argumentTable;

/// Dispatches a machine learning network using the current pipeline state and argument table.
///
/// This method takes a parameter consisting of a `MTLHeap` that Metal can use to allocate intermediate tensors.
/// You can query the minimum size Metal requires for this heap by calling
/// ``MTL4MachineLearningPipelineState/intermediatesHeapSize``.
///
/// - Parameters:
///   - heap: a heap that Metal can use to allocate intermediate tensors.
-(void) dispatchNetworkWithIntermediatesHeap:(id<MTLHeap>)heap;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4MachineLearningCommandEncoder_h
