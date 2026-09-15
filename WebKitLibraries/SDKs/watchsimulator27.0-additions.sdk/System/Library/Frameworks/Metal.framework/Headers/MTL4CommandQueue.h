//
//  MTL4CommandQueue.h
//  Metal
//
//  Copyright © 2024-2025 Apple, Inc. All rights reserved.
//

#ifndef MTL4CommandQueue_h
#define MTL4CommandQueue_h

#import <Metal/MTL4CommandBuffer.h>
#import <Metal/MTL4CommandEncoder.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLEvent.h>
#import <Metal/MTL4CommitFeedback.h>
#import <Metal/MTLResourceStateCommandEncoder.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLDrawable;
@protocol MTLResidencySet;

/// Enumeration of kinds of errors that committing an array of command buffers instances can produce.
typedef NS_ENUM(NSInteger, MTL4CommandQueueError)
{
    /// Indicates the absence of any problems.
    MTL4CommandQueueErrorNone          = 0,
    
    /// Indicates the workload takes longer to execute than the system allows.
    MTL4CommandQueueErrorTimeout       = 1,
    
    /// Indicates a process doesn’t have access to a GPU device.
    MTL4CommandQueueErrorNotPermitted  = 2,
    
    /// Indicates the GPU doesn’t have sufficient memory to execute a command buffer.
    MTL4CommandQueueErrorOutOfMemory   = 3,
    
    /// Indicates the physical removal of the GPU before the command buffer completed.
    MTL4CommandQueueErrorDeviceRemoved API_DEPRECATED("MTL4CommandQueueErrorDeviceRemoved cannot occur on Apple Silicon", macos(26.0, 27.0), ios(26.0, 27.0)) = 4,
    
    /// Indicates that the system revokes GPU access because it’s responsible for too many timeouts or hangs.
    MTL4CommandQueueErrorAccessRevoked = 5,
    
    /// Indicates an internal problem in the Metal framework.
    MTL4CommandQueueErrorInternal      = 6,
    
} API_AVAILABLE(macos(26.0), ios(26.0));

API_AVAILABLE(macos(26.0), ios(26.0))
MTL_EXTERN NSErrorDomain const MTL4CommandQueueErrorDomain;

/// Represents options to configure a commit operation on a command queue.
///
/// You pass these options as a parameter when you call ``MTL4CommandQueue/commit:count:options:``.
///
/// - Note Instances of this class are not thread-safe. If your app modifies a shared commit options instance from
/// multiple threads simultaneously, you are responsible for providing external synchronization.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CommitOptions : NSObject

/// Registers a commit feedback handler that Metal calls with feedback data when available.
///
/// - Parameter block: ``MTL4CommitFeedbackHandler`` that Metal invokes.
- (void)addFeedbackHandler:(MTL4CommitFeedbackHandler)block;

@end

/// Groups together parameters for the creation of a new command queue.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CommandQueueDescriptor : NSObject <NSCopying>

/// Assigns an optional label to the command queue instance for debugging purposes.
@property (nullable, copy, nonatomic) NSString *label;

/// Assigns a dispatch queue to which Metal submits feedback notification blocks.
///
/// When you assign a dispatch queue via this method, Metal requires that the queue parameter you provide is a serial queue.
///
/// If you set the value of property to `nil`, the default, Metal allocates an internal dispatch queue to service feedback
/// notifications.
@property (nullable, nonatomic, assign) dispatch_queue_t feedbackQueue;

@end

/// Groups together arguments for an operation to update a sparse texture mapping.
///
/// When performing a sparse mapping update, you are responsible for issuing a barrier against stage `MTLStageResourceState`.
///
/// You can determine the sparse texture tier by calling ``MTLTexture/sparseTextureTier``.
typedef struct
{
    /// The mode of the mapping operation to perform.
    ///
    /// When mode is ``MTLSparseTextureMappingMode/MTLSparseTextureMappingModeMap``,
    /// Metal walks the tiles in the region in X, Y, then Z order, assigning the next
    /// tile from the heap in increasing order, starting at ``heapOffset``.
    ///
    /// When mode is ``MTLSparseTextureMappingMode/MTLSparseTextureMappingModeUnmap``,
    /// Metal unmaps the tiles in the region, ignoring the contents of member ``heapOffset``.
    MTLSparseTextureMappingMode mode;
    
    /// The region in the texture to update, in tiles.
    ///
    /// When ``textureLevel`` is equal to the texture's ``MTLTexture/firstMipmapInTail``,
    /// set `origin.y` to `0` and `size.height` to `1`.
    MTLRegion textureRegion;
    
    /// The index of the mipmap level in the texture to update.
    ///
    /// Provide a value between `0` and the texture's ``MTLTexture/firstMipmapInTail``.
    NSUInteger textureLevel;
    
    /// The index of the array slice in the texture to update.
    ///
    /// Provide `0` in this member if the texture type is not an array.
    NSUInteger textureSlice;
    
    /// The starting offset in the heap, in tiles.
    NSUInteger heapOffset;

} MTL4UpdateSparseTextureMappingOperation API_AVAILABLE(macos(26.0), ios(26.0));

/// Groups together arguments for an operation to copy a sparse texture mapping.
typedef struct
{
    /// The region in the source texture, in tiles.
    ///
    /// The tiles remain mapped in the source texture.
    ///
    /// When ``sourceLevel`` is equal to the source texture's ``MTLTexture/firstMipmapInTail``,
    /// set `origin.y` to `0` and `size.height` to `1`.
    MTLRegion sourceRegion;
    
    /// The index of the mipmap level in the source texture.
    ///
    /// Provide a value between `0` and the source texture's ``MTLTexture/firstMipmapInTail``.
    ///
    /// When ``sourceLevel`` is equal to the source texture's ``MTLTexture/firstMipmapInTail``,
    /// set ``destinationLevel`` to the destination texture's ``MTLTexture/firstMipmapInTail``.
    NSUInteger sourceLevel;
    
    /// The index of the array slice in the texture source of the copy operation.
    ///
    /// Provide `0` in this member if the texture type is not an array.
    NSUInteger sourceSlice;
    
    /// The origin in the destination texture to copy into, in tiles.
    ///
    /// The X, Y and Z coordinates of the tiles relative to the origin match the same
    /// coordinates in the source region.
    ///
    /// When ``destinationLevel`` is equal to the destination texture's ``MTLTexture/firstMipmapInTail``,
    /// set `destinationOrigin.y` to `0`.
    MTLOrigin destinationOrigin;
    
    /// The index of the mipmap level in the destination texture.
    ///
    /// Provide a value between `0` and the destination texture's ``MTLTexture/firstMipmapInTail``.
    ///
    /// When ``sourceLevel`` is equal to the source texture's ``MTLTexture/firstMipmapInTail``,
    /// set ``destinationLevel`` to the destination texture's ``MTLTexture/firstMipmapInTail``.
    NSUInteger destinationLevel;
    
    /// The index of the array slice in the destination texture to copy into.
    ///
    /// Provide `0` in this member if the texture type is not an array.
    NSUInteger destinationSlice;

} MTL4CopySparseTextureMappingOperation API_AVAILABLE(macos(26.0), ios(26.0));

/// Groups together arguments for an operation to update a sparse buffer mapping.
typedef struct
{
    /// The mode of the mapping operation to perform.
    ///
    /// When mode is ``MTLSparseTextureMappingMode/MTLSparseTextureMappingModeMap``,
    /// Metal walks the tiles in the range in buffer offset order, assigning the
    /// next tile from the heap in increasing order, starting at ``heapOffset``.
    ///
    /// When mode is ``MTLSparseTextureMappingMode/MTLSparseTextureMappingModeUnmap``,
    /// Metal unmaps the tiles in the range, and ignores the value of member ``heapOffset``.
    MTLSparseTextureMappingMode mode;
    
    /// The range in the buffer, in tiles.
    NSRange bufferRange;
    
    /// The starting offset in the heap, in tiles.
    NSUInteger heapOffset;

} MTL4UpdateSparseBufferMappingOperation API_AVAILABLE(macos(26.0), ios(26.0));

/// Groups together arguments for an operation to copy a sparse buffer mapping.
typedef struct
{
    /// The range in the source buffer, in tiles.
    ///
    /// The tiles remain mapped in the source buffer.
    NSRange sourceRange;
    
    /// The origin in the destination buffer, in tiles.
    NSUInteger destinationOffset;
} MTL4CopySparseBufferMappingOperation API_AVAILABLE(macos(26.0), ios(26.0));

/// An abstraction representing a command queue that you use commit and synchronize command buffers and to
/// perform other GPU operations.
API_AVAILABLE(macos(26.0), ios(26.0)) NS_SWIFT_SENDABLE
@protocol MTL4CommandQueue <NSObject>

/// Returns the GPU device that the command queue belongs to.
@property (readonly) id<MTLDevice> device;

/// Obtains this queue's optional label for debugging purposes.
@property (readonly, nullable) NSString* label;

/// Enqueues an array of command buffers for execution.
///
/// The order in which you sort the command buffers in the array is meaningful, especially when it contains suspending/resuming
/// render passes. A suspending/resuming render pass is a render pass you create by calling
/// ``MTL4CommandBuffer/renderCommandEncoderWithDescriptor:options:``,
/// and provide `MTL4RenderEncoderOptionSuspending` or `MTL4RenderEncoderOptionResuming` for the `options` parameter.
///
/// If your command buffers contain suspend/resume render passes, ensure that the first command buffer only suspends,
/// and the last one only resumes. Additionally, make sure that all intermediate command buffers are both suspending
/// and resuming.
///
/// - Parameters:
///   - commandBuffers: an array of ``MTL4CommandBuffer``.
///   - count: the number of ``MTL4CommandBuffer`` instances in the `commandBuffers` array.
- (void)commit:(const id<MTL4CommandBuffer> _Nonnull[_Nonnull])commandBuffers
         count:(NSUInteger)count;

/// Enqueues an array of command buffer instances for execution with a set of options.
///
/// Provide an ``MTL4CommitOptions`` instance to configure the commit operation.
///
/// The order in which you sort the command buffers in the array is meaningful, especially when it contains suspending/resuming
/// render passes. A suspending/resuming render pass is a render pass you create by calling
/// ``MTL4CommandBuffer/renderCommandEncoderWithDescriptor:options:``,
/// and provide `MTL4RenderEncoderOptionSuspending` or `MTL4RenderEncoderOptionResuming` for the `options` parameter.
///
/// If your command buffers contain suspend/resume render passes, ensure that the first command buffer only suspends,
/// and the last one only resumes. Additionally, make sure that all intermediate command buffers are both suspending
/// and resuming.
///
/// When you commit work from multiple threads, modifying and reusing the same options instance,
/// you are responsible for externally synchronizing access to it.
///
/// - Parameters:
///   - commandBuffers: an array of ``MTL4CommandBuffer``.
///   - count: the number of ``MTL4CommandBuffer`` instances in the `commandBuffers` array.
///   - options: an instance of ``MTL4CommitOptions`` that configures the commit operation.
- (void)commit:(const id<MTL4CommandBuffer> _Nonnull[_Nonnull])commandBuffers
         count:(NSUInteger)count
       options:(MTL4CommitOptions *)options;

/// Schedules an operation to signal a GPU event with a specific value after all GPU work prior to this point is complete.
///
/// - Parameters:
///   - event: ``MTLEvent`` to signal.
///   - value: the value to signal the ``MTLEvent`` with.
- (void)signalEvent:(id<MTLEvent>)event
              value:(uint64_t)value;

/// Schedules an operation to wait for a GPU event of a specific value before continuing to execute any future GPU work.
///
/// - Parameters:
///   - event: ``MTLEvent`` to wait on.
///   - value: the specific value to wait for.
- (void)waitForEvent:(id<MTLEvent>)event
               value:(uint64_t)value;

/// Schedules a signal operation on the command queue to indicate when rendering to a Metal drawable is complete.
///
/// Signaling when rendering to a ``MTLDrawable`` instance is complete indicates that it's safe to present it to the
/// display.
///
/// You are responsible for calling this method after committing all command buffers that contain commands targeting
/// this drawable, and before calling ``MTLDrawable/present``, ``MTLDrawable/presentAtTime:``, or
/// ``MTLDrawable/presentAfterMinimumDuration:``.
///
/// - Note: This method doesn't trigger the presentation of the drawable, and fails if you call it after any of the
/// present methods, or if you call it multiple times.
///
/// Metal doesn't guarantee that command buffers you commit to the command queue after calling this method execute
/// before presentation.
///
/// - Parameters:
///   - drawable: ``MTLDrawable`` instance to signal.
- (void)signalDrawable:(id<MTLDrawable>)drawable;

/// Schedules a wait operation on the command queue to ensure the display is no longer using a specific Metal drawable.
///
/// Use this method to ensure the display is no longer using a ``MTLDrawable`` instance before executing any subsequent
/// commands.
///
/// This method returns immediately and doesn't perform any synchronization on the current thread. You are responsible
/// for calling this method before committing any command buffers containing commands that target this drawable.
///
/// Call this method multiple times if you commit your command buffers to multiple command queues.
///
/// - Parameters:
///   - drawable: ``MTLDrawable`` instance to signal.
- (void)waitForDrawable:(id<MTLDrawable>)drawable;

/// Marks a residency set as part of this command queue.
///
/// Ensures that Metal makes the residency set resident during the execution of all command buffers you commit to this
/// command queue.
///
/// Each command queue supports up to 32 unique residency set instances.
///
/// - Parameter residencySet: ``MTLResidencySet`` to add to the command queue.
- (void)addResidencySet:(id<MTLResidencySet>)residencySet;

/// Marks an array of residency sets as part of this command queue.
///
/// Ensures that Metal makes the residency set resident during the execution of all command buffers you commit to this
/// command queue.
///
/// Each command queue supports up to 32 unique residency set instances.
///
/// - Parameters:
///   - residencySets: Array of ``MTLResidencySet`` instances to add to the command queue.
///   - count: Number of ``MTLResidencySet`` instances in the array.
- (void)addResidencySets:(const id<MTLResidencySet> _Nonnull[_Nonnull])residencySets
                   count:(NSUInteger)count;

/// Removes a residency set from the command queue.
///
/// After calling this method ensures only the remaining residency sets remain resident during the execution of the
/// command buffers you commit this command queue.
///
/// - Parameter residencySet: ``MTLResidencySet`` instance to remove from the command queue.
- (void)removeResidencySet:(id<MTLResidencySet>)residencySet;

/// Removes multiple residency sets from the command queue.
///
/// After calling this method ensures only the remaining residency sets remain resident during the execution of the
/// command buffers you commit this command queue.
///
/// - Parameters:
///   - residencySets: Array of ``MTLResidencySet`` instances to remove from the command queue.
///   - count: Number of ``MTLResidencySet`` instances in the array.
- (void)removeResidencySets:(const id<MTLResidencySet> _Nonnull[_Nonnull])residencySets
                      count:(NSUInteger)count;

/// Updates multiple regions within a placement sparse texture to alias specific tiles of a Metal heap.
///
/// You can provide a `nil` parameter to the `heap` argument only if when you perform unmap operations. Otherwise, you are
/// responsible for ensuring the heap is non-nil and has a
/// ``MTLHeapDescriptor/maxCompatiblePlacementSparsePageSize`` of at least the texture's
/// ``MTLTextureDescriptor/placementSparsePageSize``.
///
/// When performing a sparse mapping update, you are responsible for issuing a barrier against stage `MTLStageResourceState`.
///
/// You can determine the sparse texture tier by calling `MTLTexture/sparseTextureTier`.
///
/// - Parameters:
///   - texture: A placement sparse ``MTLTexture``.
///   - heap: ``MTLHeap`` you allocate with type ``MTLHeapType/MTLHeapTypePlacement``.
///   - operations: An array of ``MTL4UpdateSparseTextureMappingOperation`` instances to perform.
///   - count: Number of operations to perform.
- (void)updateTextureMappings:(id<MTLTexture>)texture
                         heap:(nullable id<MTLHeap>)heap
                   operations:(const MTL4UpdateSparseTextureMappingOperation [_Nonnull])operations
                        count:(NSUInteger)count;

/// Copies multiple regions within a source placement sparse texture to a destination placement sparse texture.
///
/// You are responsible for ensuring the source and destination textures have the same
/// ``MTLTextureDescriptor/placementSparsePageSize``.
///
/// Additionally, you are responsible for ensuring that the source and destination textures don't use the same aliased tiles
/// at the same time.
///
/// - Note: If a sparse texture and a sparse buffer share the same backing tiles, these don't provide you
/// you with meaningful views of the other resource’s data.
///
/// - Parameters:
///   - sourceTexture: The source placement sparse ``MTLTexture``.
///   - destinationTexture: The destination placement sparse ``MTLTexture``.
///   - operations: An array of ``MTL4CopySparseTextureMappingOperation`` instances to perform.
///   - count: Number of operations to perform.
- (void)copyTextureMappingsFromTexture:(id<MTLTexture>)sourceTexture
                             toTexture:(id<MTLTexture>)destinationTexture
                            operations:(const MTL4CopySparseTextureMappingOperation [_Nonnull]) operations
                                 count:(NSUInteger)count;

/// Updates multiple regions within a placement sparse buffer to alias specific tiles from a Metal heap.
///
/// You can provide a `nil` parameter to the `heap` argument only when you perform unmap operations. Otherwise, you are
/// responsible for ensuring parameter `heap` references an ``MTLHeap`` that has a ``MTLHeapDescriptor/maxCompatiblePlacementSparsePageSize``
/// of at least the buffer's `placementSparsePageSize` you assign when creating the sparse buffer via
/// ``MTLDevice/newBufferWithLength:options:placementSparsePageSize:``.
///
/// - Parameters:
///   - buffer: A placement sparse ``MTLBuffer``.
///   - heap: An ``MTLHeap`` you allocate with type ``MTLHeapType/MTLHeapTypePlacement``.
///   - operations: An array of ``MTL4UpdateSparseBufferMappingOperation`` instances to perform.
///   - count: Number of operations to perform.
- (void)updateBufferMappings:(id<MTLBuffer>)buffer
                        heap:(nullable id<MTLHeap>)heap
                  operations:(const MTL4UpdateSparseBufferMappingOperation [_Nonnull])operations
                       count:(NSUInteger)count;

/// Copies multiple offsets within a source placement sparse buffer to a destination placement sparse buffer.
///
/// You are responsible for ensuring the source destination sparse buffers have the same `placementSparsePageSize` when
/// you create them via ``MTLDevice/newBufferWithLength:options:placementSparsePageSize:``.
///
/// Additionally, you are responsible for ensuring both the source and destination sparse buffers don't use the same aliased
/// tiles at the same time.
///
/// - Note: If a sparse texture and a sparse buffer share the same backing tiles, these don't provide you
///         with meaningful views of the other resource’s data.
///
/// - Parameters:
///   - sourceBuffer: The source placement sparse ``MTLBuffer``.
///   - destinationBuffer: The destination placement sparse ``MTLBuffer``.
///   - operations: An array of ``MTL4CopySparseBufferMappingOperation`` instances to perform.
///   - count: Number of operations to perform.
- (void)copyBufferMappingsFromBuffer:(id<MTLBuffer>)sourceBuffer
                            toBuffer:(id<MTLBuffer>)destinationBuffer
                          operations:(const MTL4CopySparseBufferMappingOperation [_Nonnull])operations
                               count:(NSUInteger)count;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4CommandQueue_h
