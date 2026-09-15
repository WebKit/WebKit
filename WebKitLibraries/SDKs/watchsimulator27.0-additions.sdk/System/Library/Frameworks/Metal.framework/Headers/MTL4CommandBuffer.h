//
//  MTL4CommandBuffer.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4CommandBuffer_h
#define MTL4CommandBuffer_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTL4RenderCommandEncoder.h>
#import <Metal/MTL4BufferRange.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLLogState;
@protocol MTLResidencySet;
@protocol MTL4CommandAllocator;
@protocol MTL4ComputeCommandEncoder;
@protocol MTL4CounterHeap;

@protocol MTL4MachineLearningCommandEncoder;

@class MTL4RenderPassDescriptor;

/// Options to configure a command buffer before encoding work into it.
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CommandBufferOptions : NSObject <NSCopying>

/// Contains information related to shader logging.
///
/// To enable shader logging, call ``MTL4CommandBuffer/beginCommandBufferWithAllocator:options:`` with an instance
/// of ``MTL4CommandBufferOptions`` that contains a non-`nil` ``MTLLogState`` instance in this property.
///
/// Shader functions log messages until the command buffer ends.
@property (readwrite, nonatomic, nullable, retain) id<MTLLogState> logState;

@end

/// Records a sequence of GPU commands.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4CommandBuffer <NSObject>

/// Returns the GPU device that this command buffer belongs to.
@property (readonly) id<MTLDevice> device;

/// Assigns an optional label with this command buffer.
@property (nullable, copy, atomic) NSString *label;

/// Prepares a command buffer for encoding.
///
/// Attaches the command buffer to the specified ``MTL4CommandAllocator`` and declares that the
/// application is ready to encode commands into the command buffer.
///
/// Command allocators only service a single command buffer at a time. If you need to issue multiple
/// calls to this method simultaneously, for example, in a multi-threaded command encoding scenario,
/// create multiple instances of ``MTLCommandAllocator`` and use one for each call.
///
/// You can safely reuse command allocators after ending the command buffer using it by calling
/// ``endCommandBuffer``.
///
/// After calling this method, any prior calls to ``useResidencySet:`` and ``useResidencySets:count:``
/// on this command buffer instance no longer apply. Make sure to call these methods again to signal
/// your residency requirements to Metal.
///
/// - Parameter allocator: ``MTL4CommandAllocator`` to attach to.
- (void)beginCommandBufferWithAllocator:(id<MTL4CommandAllocator>)allocator;

/// Prepares a command buffer for encoding with additional options.
///
/// Attaches the command buffer to the specified ``MTL4CommandAllocator`` and declares that the
/// application is ready to encode commands into the command buffer.
///
/// Command allocators only service a single command buffer at a time. If you need to issue multiple
/// calls to this method simultaneously, for example, in a multi-threaded command encoding scenario,
/// create multiple instances of ``MTLCommandAllocator`` and use one for each call.
///
/// You can safely reuse command allocators after ending the command buffer using it by calling
/// ``endCommandBuffer``.
///
/// After calling this method, any prior calls to ``useResidencySet:`` and ``useResidencySets:count:``
/// on this command buffer instance no longer apply. Make sure to call these methods again to signal
/// your residency requirements to Metal.
///
/// The options you provide configure the command buffer only until the command buffer ends, in the
/// next call to ``endCommandBuffer``.
///
/// - Parameters:
///   - allocator: ``MTL4CommandAllocator`` to attach to.
///   - options: ``MTL4CommandBufferOptions`` to configure the command buffer.
- (void)beginCommandBufferWithAllocator:(id<MTL4CommandAllocator>)allocator
                                options:(MTL4CommandBufferOptions *)options;

/// Closes a command buffer to prepare it for submission to a command queue.
///
/// Explicitly ending the command buffer allows you to reuse the ``MTL4CommandAllocator`` to start servicing other
/// command buffers. It is an error to call ``commit`` on a command buffer previously recording before calling this
/// method.
- (void)endCommandBuffer;

/// Creates a render command encoder from a render pass descriptor.
///
/// - Parameters:
///   - descriptor: Descriptor for the render pass.
/// - Returns: The created ``MTL4RenderCommandEncoder`` instance, or `nil` if the function failed.
- (nullable id<MTL4RenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTL4RenderPassDescriptor *)descriptor;

/// Creates a render command encoder from a render pass descriptor with additional options.
///
/// This method creates a render command encoder to encode a render pass, whilst providing you the option to define
/// some render pass characteristics via an instance of ``MTL4RenderEncoderOptions``.
///
/// Use these options to configure suspending/resuming render command encoders, which allow you to encode render passes
/// from multiple threads simultaneously.
///
/// - Parameters:
///   - descriptor: Descriptor for the render pass.
///   - options: ``MTL4RenderEncoderOptions`` instance that provide render pass options.
/// - Returns: The created ``MTL4RenderCommandEncoder`` instance, or `nil` if the function fails.
- (nullable id<MTL4RenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTL4RenderPassDescriptor *)descriptor
                                                                    options:(MTL4RenderEncoderOptions)options;

/// Creates a compute command encoder.
///
/// - Returns: The created ``MTL4ComputeCommandEncoder`` instance, or `nil` if the function fails.
- (nullable id<MTL4ComputeCommandEncoder>)computeCommandEncoder;

/// Creates a machine learning command encoder.
///
/// - Returns: The created ``MTL4MachineLearningCommandEncoder`` instance , or `nil` if the function fails.
- (nullable id<MTL4MachineLearningCommandEncoder>)machineLearningCommandEncoder;

/// Marks a residency set as part of the command buffer's execution.
///
/// Ensures that Metal makes resident the resources that residency set contains during execution of this command buffer.
///
/// - Parameter residencySet: ``MTLResidencySet`` instance to mark resident.
- (void)useResidencySet:(id<MTLResidencySet>)residencySet;

/// Marks an array of residency sets as part of the command buffer's execution.
///
/// Ensures that Metal makes resident the resources that residency sets contain during execution of this command buffer.
///
/// - Parameters:
///   - residencySets: Array of ``MTLResidencySet`` instances to mark resident.
///   - count: Number of ``MTLResidencySet`` instances in the array.
- (void)useResidencySets:(const id<MTLResidencySet> _Nonnull[_Nonnull])residencySets
                   count:(NSUInteger)count;

/// Pushes a string onto a stack of debug groups for this command buffer.
///
/// - Parameter string: The string to push.
- (void)pushDebugGroup:(NSString *)string;

/// Pops the latest string from the stack of debug groups for this command buffer.
- (void)popDebugGroup;

/// Writes a GPU timestamp into the given counter heap.
///
/// This method captures a timestamp after work prior to this command in the command buffer is complete.
/// Work after this call may or may not have started.
///
/// You are responsible for ensuring the `counterHeap` is of type ``MTL4CounterHeapType/MTL4CounterHeapTypeTimestamp``.
///
/// - Parameters:
///   - counterHeap: ``MTL4CounterHeap`` to write the timestamp into.
///   - index: The index within the ``MTL4CounterHeap`` that Metal writes the timestamp to.
- (void)writeTimestampIntoHeap:(id<MTL4CounterHeap>)counterHeap
                       atIndex:(NSUInteger) index
API_AVAILABLE(macos(26.0), ios(26.0));


/// Encodes a command that resolves an opaque counter heap into a buffer.
///
/// The command this method encodes converts the data within `counterHeap` into a common format
/// and stores it into the `bufferRange` parameter.
///
/// The command places each entry in the counter heap within `range` sequentially, starting at `alignedOffset`.
/// Each entry needs to be a fixed size that you can query by calling the
/// ``MTLDevice/sizeOfCounterHeapEntry:`` method.
///
/// This command runs during the `MTLStageBlit` stage of the GPU timeline. Barrier against this stage
/// to ensure the data is present in the resolve buffer parameter before you access it.
///
/// - Note: Your app needs ensure the GPU places data in the heap before you resolve it by
/// synchronizing this stage with other GPU operations.
///
/// Similarly, your app needs to synchronize any GPU accesses to `bufferRange` after
/// the command completes with barrier.
///
/// If your app needs to access `bufferRange` from the CPU, signal an ``MTLSharedEvent``
/// to notify the CPU when it's ready.
/// Alternatively, you can resolve the heap's data from the CPU by calling
/// the heap's ``MTL4CounterHeap/resolveCounterRange:`` method.
///
/// - Parameters:
///   - counterHeap: A heap the command resolves.
///   - range: A range of index values within the heap the command resolves.
///   - bufferRange: The buffer the command saves the data it resolves into.
///   - fenceToWait: A fence the GPU waits for before starting, if applicable; otherwise `nil`.
///   - fenceToUpdate: A fence the system updates after the command finishes resolving the data; otherwise `nil`.
 - (void)resolveCounterHeap:(id<MTL4CounterHeap>)counterHeap
                  withRange:(NSRange)range
                 intoBuffer:(MTL4BufferRange)bufferRange
                  waitFence:(nullable id<MTLFence>)fenceToWait
                updateFence:(nullable id<MTLFence>)fenceToUpdate
 API_AVAILABLE(macos(26.0), ios(26.0));

@end

NS_ASSUME_NONNULL_END


#endif //MTL4CommandBuffer_h
