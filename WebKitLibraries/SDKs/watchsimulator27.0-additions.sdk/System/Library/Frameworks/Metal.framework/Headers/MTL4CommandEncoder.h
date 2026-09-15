//
//  MTL4CommandEncoder.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4CommandEncoder_h
#define MTL4CommandEncoder_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLCommandEncoder.h>


NS_ASSUME_NONNULL_BEGIN

/// Memory consistency options for synchronization commands.
typedef NS_OPTIONS(NSUInteger, MTL4VisibilityOptions)
{
    /// Don't flush caches. When you use this option on a barrier, it turns it into an execution barrier.
    MTL4VisibilityOptionNone          = 0,
    
    /// Flushes caches to the GPU (device) memory coherence point.
    MTL4VisibilityOptionDevice        = 1 << 0,
    
    
    /// Flushes caches to ensure that aliased virtual addresses are memory consistent.
    ///
    /// On some systems this may be the GPU+CPU (system) memory coherence point
    /// and on other systems it may be the GPU (device) memory coherence point.
    MTL4VisibilityOptionResourceAlias = 1 << 1,
    
} API_AVAILABLE(macos(26.0), ios(26.0));

/// An encoder that writes GPU commands into a command buffer.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4CommandEncoder <NSObject>

/// Provides an optional label to assign to the command encoder for debug purposes.
@property (nullable, copy, atomic) NSString *label;

/// Returns the command buffer that is currently encoding commands.
///
/// This property may return undefined results if you call it after calling ``endEncoding``.
@property (nullable, readonly, nonatomic) id<MTL4CommandBuffer> commandBuffer;

/// Encodes a consumer barrier on work you commit to the same command queue.
///
/// Encode a barrier that guarantees that any subsequent work you encode in the current command encoder that corresponds
/// to the `beforeStages` stages doesn't proceed until Metal completes all work prior to the current command encoder
/// corresponding to the `afterQueueStages` stages, completes.
///
/// Metal can reorder the exact point where it applies the barrier, so encode the barrier as close to the command that
/// consumes the resource as possible. Don't use this method for synchronizing resource access within the same pass.
///
/// If you need to synchronize work within a pass that you encode with an instance of a subclass of ``MTLCommandEncoder``,
/// use memory barriers instead. For subclasses of ``MTL4CommandEncoder``, use encoder barriers.
///
/// You can specify `afterQueueStages` and `beforeStages` that contain ``MTLStages`` unrelated to the current command
/// encoder.
///
/// - Parameters:
///   - afterQueueStages: ``MTLStages`` mask that represents the stages of work to wait for.
///                        This argument applies to work corresponding to these stages you
///                        encode in prior command encoders, and not for the current encoder.
///   - beforeStages:     ``MTLStages`` mask that represents the stages of work that wait.
///                        This argument applies to work you encode in the current command encoder.
///   - visibilityOptions: ``MTL4VisibilityOptions`` of the barrier.
- (void)barrierAfterQueueStages:(MTLStages)afterQueueStages
                   beforeStages:(MTLStages)beforeStages
              visibilityOptions:(MTL4VisibilityOptions)visibilityOptions;

/// Encodes a producer barrier on work committed to the same command queue.
///
/// This method encodes a barrier that guarantees that any work you encode using *subsequent command encoders*,
/// corresponding to `beforeQueueStages`, don't begin until all commands you previously encode in the current
/// encoder (and prior encoders), corresponding to `afterStages`, complete.
///
/// When calling this method, you can pass any ``MTLStages`` to parameters `afterStages` and `beforeQueueStages`,
/// even stages that don't relate to the current or prior command encoders.
///
/// - Parameters:
///   - afterStages:       ``MTLStages`` mask that represents the stages of work to wait for.
///                         This argument applies to work corresponding to these stages you encode in
///                         the current command encoder prior to this barrier command.
///   - beforeQueueStages: ``MTLStages`` mask that represents the stages of work that need to wait.
///                         This argument applies to subsequent encoders and not to work in the current
///                         command encoder.
///   - visibilityOptions: ``MTL4VisibilityOptions`` of the barrier, controlling cache flush behavior.
- (void)barrierAfterStages:(MTLStages)afterStages
         beforeQueueStages:(MTLStages)beforeQueueStages
         visibilityOptions:(MTL4VisibilityOptions)visibilityOptions;

/// Encodes an intra-pass barrier that instructs the GPU to pause before running stages of subsequent commands until
/// stages of previous commands complete.
///
/// Encode a barrier that guarantees that any subsequent work you encode in the *current command encoder*,
/// corresponding to `beforeEncoderStages`, doesn't begin until all prior commands in this command encoder,
/// corresponding to `afterEncoderStages`, completes.
///
/// When calling this method, it's your responsibility to ensure parameters `afterEncoderStages` and `beforeEncoderStages`
/// contain a combination of ``MTLStages`` for which this encoder can encode commands. For example, for a
/// ``MTL4ComputeCommandEncoder`` instance, you can provide any combination of ``MTLStages/MTLStageDispatch``,
/// ``MTLStages/MTLStageBlit`` and ``MTLStages/MTLStageAccelerationStructure``.
///
/// - Parameters:
///   - afterEncoderStages: ``MTLStages`` the stages of the previous commands of this pass that need to complete before
///                          the stages in `beforeEncoderStages` start for subsequent commands you encode in this pass.
///   - beforeEncoderStages: ``MTLStages`` the stages of the subsequent commands you encode in this pass that wait for
///                          the stages in `afterEncoderStages`, within this pass, to complete.
///   - visibilityOptions: ``MTL4VisibilityOptions`` of the barrier, controlling cache flush behavior.
- (void)barrierAfterEncoderStages:(MTLStages)afterEncoderStages
              beforeEncoderStages:(MTLStages)beforeEncoderStages
                visibilityOptions:(MTL4VisibilityOptions)visibilityOptions;

/// Encodes a command to update a GPU fence.
///
/// This method encodes a command that updates a ``MTLFence`` instance after all previously-encoded commands in the
/// current command encoder, corresponding to `afterEncoderStages`, complete.
///
/// Use parameter `afterEncoderStages` to pass in a combination of ``MTLStages`` for which this encoder can encode work.
/// For example, for a ``MTL4ComputeCommandEncoder`` you can provide any combination of ``MTLStages/MTLStageDispatch``,
/// ``MTLStages/MTLStageBlit`` and ``MTLStages/MTLStageAccelerationStructure``.
///
/// - Parameters:
///   - fence:              ``MTLFence`` instance to update.
///   - afterEncoderStages: ``MTLStages`` value that represents the stages of work to wait for.
///                          This argument only applies to work encoded in the current command encoder.
- (void)updateFence:(id<MTLFence>)fence
 afterEncoderStages:(MTLStages)afterEncoderStages;

/// Encodes a command to wait on a GPU fence.
///
/// Encode a command that guarantees that any subsequent work you encode via this current command encoder,
/// corresponding to `beforeEncoderStages`, doesn't begin until all prior updates to the fence is complete.
///
/// To successfully wait for a fence update, schedule update and wait operations on the same command queue.
///
/// Use parameter `beforeEncoderStages` to pass in a combination of ``MTLStages`` for which this encoder can encode
/// work. For example, for a ``MTL4ComputeCommandEncoder`` you can provide any combination of
/// ``MTLStages/MTLStageDispatch``, ``MTLStages/MTLStageBlit`` and ``MTLStages/MTLStageAccelerationStructure``.
///
/// - Parameters:
///   - fence:              ``MTLFence`` instance to wait for.
///   - beforeEncoderStages:``MTLStages`` value that represents the stages of work that wait.
///                         This argument only applies to work you encode in the current command encoder.
- (void)waitForFence:(id<MTLFence>)fence
 beforeEncoderStages:(MTLStages)beforeEncoderStages;

/// Inserts a debug string into the frame data to aid debugging.
///
/// Calling this method doesn't change any behaviors, but can be useful for debugging purposes.
///
/// - Parameter string: The debug string to insert as a signpost.
- (void)insertDebugSignpost:(NSString *)string;

/// Pushes a string onto this encoder's stack of debug groups.
///
/// - Parameter string: The debug string to push.
- (void)pushDebugGroup:(NSString *)string;

/// Pops the latest debug group string from this encoder's stack of debug groups.
- (void)popDebugGroup;

/// Declares that all command generation from this encoder is complete.
- (void)endEncoding;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4CommandEncoder_h
