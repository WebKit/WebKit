//
//  MTLAccelerationStructureCommandEncoder.h
//  Metal
//
//  Copyright © 2019 Apple, Inc. All rights reserved.


#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLBuffer.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLAccelerationStructure.h>


NS_ASSUME_NONNULL_BEGIN

API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@protocol MTLAccelerationStructureCommandEncoder <MTLCommandEncoder>

/*!
 * @brief Encode an acceleration structure build into the command buffer. All bottom-level acceleration
 * structure builds must have completed before a top-level acceleration structure build may begin. The
 * resulting acceleration structure will not retain any references to the input vertex buffer, instance buffer, etc.
 *
 * The acceleration structure build will not be completed until the command buffer has been committed
 * and finished executing. However, it is safe to encode ray tracing work against the acceleration
 * structure as long as the command buffers are scheduled and synchronized such that the command buffer
 * will have completed by the time the ray tracing starts.
 *
 * The acceleration structure and scratch buffer must be at least the size returned by the
 * [MTLDevice accelerationStructureSizesWithDescriptor:] query.
 *
 * @param accelerationStructure Acceleration structure storage to build into
 * @param descriptor            Object describing the acceleration structure to build
 * @param scratchBuffer         Scratch buffer to use while building the acceleration structure. The
 *                              contents may be overwritten and are undefined after the build has
 *                              started/completed.
 * @param scratchBufferOffset   Offset into the scratch buffer
 */
- (void)buildAccelerationStructure:(id <MTLAccelerationStructure>)accelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                     scratchBuffer:(id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset;

/*!
 * @brief Encode an acceleration structure refit into the command buffer. Refitting can be used to
 * update the acceleration structure when geometry changes and is much faster than rebuilding from
 * scratch. However, the quality of the acceleration structure and the subsequent ray tracing
 * performance will degrade depending on how much the geometry changes.
 * 
 * Refitting can not be used after certain changes, such as adding or removing geometry. Acceleration
 * structures can be refit in place by specifying the same source and destination acceleration structures
 * or by providing a nil destination acceleration structure. If the source and destination acceleration
 * structures are not the same, they must not overlap in memory.
 *
 * The destination acceleration structure must be at least as large as the source acceleration structure,
 * unless the source acceleration structure has been compacted, in which case the destination acceleration
 * structure must be at least as large as the compacted size of the source acceleration structure.
 *
 * The scratch buffer must be at least the size returned by the accelerationStructureSizesWithDescriptor
 * method of the MTLDevice.
 *
 * @param descriptor                       Object describing the acceleration structure to build
 * @param sourceAccelerationStructure      Acceleration structure to copy from
 * @param destinationAccelerationStructure Acceleration structure to copy to
 * @param scratchBuffer                    Scratch buffer to use while refitting the acceleration
 *                                         structure. The contents may be overwritten and are undefined
 *                                         after the refit has started/completed.
 * @param scratchBufferOffset              Offset into the scratch buffer.
 */
- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(nullable id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset;

/*!
 * @brief Encode an acceleration structure refit into the command buffer. Refitting can be used to
 * update the acceleration structure when geometry changes and is much faster than rebuilding from
 * scratch. However, the quality of the acceleration structure and the subsequent ray tracing
 * performance will degrade depending on how much the geometry changes.
 *
 * Refitting can not be used after certain changes, such as adding or removing geometry. Acceleration
 * structures can be refit in place by specifying the same source and destination acceleration structures
 * or by providing a nil destination acceleration structure. If the source and destination acceleration
 * structures are not the same, they must not overlap in memory.
 *
 * The destination acceleration structure must be at least as large as the source acceleration structure,
 * unless the source acceleration structure has been compacted, in which case the destination acceleration
 * structure must be at least as large as the compacted size of the source acceleration structure.
 *
 * The scratch buffer must be at least the size returned by the accelerationStructureSizesWithDescriptor
 * method of the MTLDevice.
 *
 * @param descriptor                       Object describing the acceleration structure to build
 * @param sourceAccelerationStructure      Acceleration structure to copy from
 * @param destinationAccelerationStructure Acceleration structure to copy to
 * @param scratchBuffer                    Scratch buffer to use while refitting the acceleration
 *                                         structure. The contents may be overwritten and are undefined
 *                                         after the refit has started/completed.
 * @param scratchBufferOffset              Offset into the scratch buffer.
 * @param options                          Options specifying the elements of the acceleration structure to refit.
 */
- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTLAccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(nullable id <MTLBuffer>)scratchBuffer
               scratchBufferOffset:(NSUInteger)scratchBufferOffset
                           options:(MTLAccelerationStructureRefitOptions)options
                           API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 * @brief Copy an acceleration structure. The source and destination acceleration structures must not
 * overlap in memory. If this is a top level acceleration structure, references to bottom level
 * acceleration structures will be preserved.
 *
 * The destination acceleration structure must be at least as large as the source acceleration structure,
 * unless the source acceleration structure has been compacted, in which case the destination acceleration
 * structure must be at least as large as the compacted size of the source acceleration structure.
 *
 * @param sourceAccelerationStructure      Acceleration structure to copy from
 * @param destinationAccelerationStructure Acceleration structure to copy to
 */
- (void)copyAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
          toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

/*!
 * @brief Compute the compacted size for an acceleration structure and write it into a buffer.
 *
 * This size is potentially smaller than the source acceleration structure. To perform compaction,
 * read this size from the buffer once the command buffer has completed and use it to allocate a
 * smaller acceleration structure. Then create another encoder and call the 
 * copyAndCompactAccelerationStructure method.
 *
 * @param accelerationStructure Source acceleration structure
 * @param buffer                Destination size buffer. The compacted size will be written as a 32 bit
 *                              unsigned integer representing the compacted size in bytes.
 * @param offset                Offset into the size buffer
 */
- (void)writeCompactedAccelerationStructureSize:(id <MTLAccelerationStructure>)accelerationStructure
                                       toBuffer:(id <MTLBuffer>)buffer
                                         offset:(NSUInteger)offset;

/*!
 * @brief Compute the compacted size for an acceleration structure and write it into a buffer.
 *
 * This size is potentially smaller than the source acceleration structure. To perform compaction,
 * read this size from the buffer once the command buffer has completed and use it to allocate a
 * smaller acceleration structure. Then create another encoder and call the 
 * copyAndCompactAccelerationStructure method.
 *
 * @param accelerationStructure Source acceleration structure
 * @param buffer                Destination size buffer. The compacted size will be written as either
 *                              a 32 bit or 64 bit value depending on the sizeDataType argument
 *                              unsigned integer representing the compacted size in bytes.
 * @param offset                Offset into the size buffer
 * @param sizeDataType          Data type of the size to write into the buffer. Must be either
 *                              MTLDataTypeUInt (32 bit) or MTLDataTypeULong (64 bit)
 */
- (void)writeCompactedAccelerationStructureSize:(id <MTLAccelerationStructure>)accelerationStructure
                                       toBuffer:(id <MTLBuffer>)buffer
                                         offset:(NSUInteger)offset
                                   sizeDataType:(MTLDataType)sizeDataType API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 * @brief Copy and compact an acceleration structure. The source and destination acceleration structures
 * must not overlap in memory. If this is a top level acceleration structure, references to bottom level
 * acceleration structures will be preserved.
 *
 * The destination acceleration structure must be at least as large as the compacted size of the source
 * acceleration structure, which is computed by the writeCompactedAccelerationStructureSize method.
 *
 * @param sourceAccelerationStructure      Acceleration structure to copy and compact
 * @param destinationAccelerationStructure Acceleration structure to copy to
 */
- (void)copyAndCompactAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                    toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

 /*!
 @method updateFence:
 @abstract Update the fence to capture all GPU work so far enqueued by this encoder.
 @discussion The fence is updated at build submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)updateFence:(id <MTLFence>)fence;

/*!
 @method waitForFence:
 @abstract Prevent further GPU work until the fence is reached.
 @discussion The fence is evaluated at build submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)waitForFence:(id <MTLFence>)fence;

/*!
 * @method useResource:usage:
 * @abstract Declare that a resource may be accessed by the command encoder through an argument buffer
 * 
 * @discussion For tracked MTLResources, this method protects against data hazards. This method must be called before encoding any acceleration structure commands which may access the resource through an argument buffer.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage;

/*!
 * @method useResources:count:usage:
 * @abstract Declare that an array of resources may be accessed through an argument buffer by the command encoder
 * @discussion For tracked MTL Resources, this method protects against data hazards. This method must be called before encoding any acceleration structure commands which may access the resources through an argument buffer.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage;

/*!
 * @method useHeap:
 * @abstract Declare that the resources allocated from a heap may be accessed as readonly by the encoder through an argument buffer
 * @discussion For tracked MTLHeaps, this method protects against data hazards. This method must be called before encoding any acceleration structure commands which may access the resources allocated from the heap through an argument buffer. This method may cause all of the color attachments allocated from the heap to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useHeap:(id <MTLHeap>)heap;

/*!
 * @method useHeaps:count:
 * @abstract Declare that the resources allocated from an array of heaps may be accessed as readonly by the encoder through an argument buffer
 * @discussion For tracked MTLHeaps, this method protects against data hazards. This method must be called before encoding any acceleration structure commands which may access the resources allocated from the heaps through an argument buffer. This method may cause all of the color attachments allocated from the heaps to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useHeaps:(const id <MTLHeap> __nonnull[__nonnull])heaps count:(NSUInteger)count;

/*!
 @method sampleCountersInBuffer:atSampleIndex:withBarrier:
 @abstract Sample hardware counters at this point in the acceleration structure
 encoder and store the counter sample into the sample buffer at the specified index.
 @param sampleBuffer The sample buffer to sample into
 @param sampleIndex The index into the counter buffer to write the sample
 @param barrier Insert a barrier before taking the sample.  Passing
 YES will ensure that all work encoded before this operation in the encoder is
 complete but does not isolate the work with respect to other encoders.  Passing
 NO will allow the sample to be taken concurrently with other operations in this
 encoder.
 In general, passing YES will lead to more repeatable counter results but
 may negatively impact performance.  Passing NO will generally be higher performance
 but counter results may not be repeatable.
 */
-(void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                atSampleIndex:(NSUInteger)sampleIndex
                  withBarrier:(BOOL)barrier API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

@end


MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@interface MTLAccelerationStructurePassSampleBufferAttachmentDescriptor : NSObject<NSCopying>
/*!
@property sampleBuffer
@abstract The sample buffer to store samples for the acceleration structure pass defined samples.
If sampleBuffer is non-nil, the sample indices will be used to store samples into
the sample buffer.  If no sample buffer is provided, no samples will be taken.
If any of the sample indices are specified as MTLCounterDontSample, no sample
will be taken for that action.
*/
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer;

/*!
 @property startOfEncoderSampleIndex
 @abstract The sample index to use to store the sample taken at the start of
 command encoder processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of an
 acceleration structure pass will fail.
 */
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
/*!
 @property endOfEncoderSampleIndex
 @abstract The sample index to use to store the sample taken at the end of
 command encoder processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of an
 acceleration structure pass will fail.
 */
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@interface MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray : NSObject

/* Individual attachment state access */
- (MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLAccelerationStructurePassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

/*!
 @class MTLAccelerationStructurePassDescriptor
 @abstract MTLAccelerationStructurePassDescriptor represents a collection of attachments to be used to create a concrete acceleration structure encoder.
 */
MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@interface MTLAccelerationStructurePassDescriptor : NSObject <NSCopying>

/*!
 @method accelerationStructurePassDescriptor
 @abstract Create an autoreleased default acceleration structure pass descriptor
 */
+ (MTLAccelerationStructurePassDescriptor *)accelerationStructurePassDescriptor;

/*!
 @property sampleBufferAttachments
 @abstract An array of sample buffers and associated sample indices.
 */
@property (readonly) MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

@end


NS_ASSUME_NONNULL_END

