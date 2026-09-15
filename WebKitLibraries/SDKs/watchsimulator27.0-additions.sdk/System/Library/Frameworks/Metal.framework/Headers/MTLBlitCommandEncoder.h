//
//  MTLBlitCommandEncoder.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLResourceStateCommandEncoder.h>
#import <Metal/MTLBuffer.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLRenderPass.h>
#import <Metal/MTLBlitPass.h>
#import <Metal/MTLCounters.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 @enum MTLBlitOption
 @abstract Controls the blit operation
 */
typedef NS_OPTIONS(NSUInteger, MTLBlitOption)
{
    MTLBlitOptionNone                       = 0,
    MTLBlitOptionDepthFromDepthStencil      = 1 << 0,
    MTLBlitOptionStencilFromDepthStencil    = 1 << 1,
    MTLBlitOptionRowLinearPVRTC API_AVAILABLE(ios(9.0), macos(11.0), macCatalyst(14.0)) = 1 << 2,
} API_AVAILABLE(macos(10.11), ios(9.0));


/*!
 @protocol MTLBlitCommandEncoder
 @abstract A command encoder that performs basic copies and blits between buffers and textures.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLBlitCommandEncoder <MTLCommandEncoder>

/*!
 @method synchronizeResource:
 @abstract Flush any copy of this resource from the device's caches, and invalidate any CPU caches if needed.
 @param resource The resource to page off.
 @discussion When the device writes to a resource with a storage mode of MTLResourceStorageModeManaged, those writes may be cached (for example, in VRAM or on chip renderer cache),
 making any CPU access (either MTLBuffer.contents or -[MTLTexture getBytes:...] and -[MTLTexture replaceRegion:]) produce undefined results.  To allow the CPU to see what the device
 has written, a CommandBuffer containing this synchronization must be executed.  After completion of the CommandBuffer, the CPU can access the contents of the resource safely.
 */
- (void)synchronizeResource:(id<MTLResource>)resource API_DEPRECATED("Managed storage has no effect on Apple Silicon, use Shared storage instead", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @method synchronizeTexture:slice:mipmapLevel:
 @abstract Flush any copy of this image from the device's caches, and invalidate CPU caches if needed.
 @param texture The texture to page off.
 @param slice The slice of the texture to page off.
 @param level The mipmap level of the texture to flush.
 @discussion
 See the discussion of -synchronizeResource.   -synchronizeTexture:slice:mipmapLevel performs the same role, except it may flush only a subset of the texture storage, rather than the entire texture.
 */
- (void)synchronizeTexture:(id<MTLTexture>)texture slice:(NSUInteger)slice level:(NSUInteger)level API_DEPRECATED("Managed storage has no effect on Apple Silicon, use Shared storage instead", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @method copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:
 @abstract Copy a rectangle of pixels between textures.
 */
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin;

/*!
 @method copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:
 @abstract Copy an image from a buffer into a texture.
 */
- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin;

/*!
 @method copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:options:
 @abstract Copy an image from a buffer into a texture.
 */
- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin options:(MTLBlitOption)options API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:
 @abstract Copy an image from a texture into a buffer.
 */
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage;

/*!
 @method copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:sourceOptions:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:options:
 @abstract Copy an image from a texture into a buffer.
 */
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage options:(MTLBlitOption)options API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method generateMipmapsForTexture:
 @abstract Generate mipmaps for a texture from the base level up to the max level.
 */
- (void)generateMipmapsForTexture:(id<MTLTexture>)texture;

/*!
 @method fillBuffer:range:value:
 @abstract Fill a buffer with a fixed value in each byte.
 */
- (void)fillBuffer:(id<MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value;

/*!
 @method copyFromTexture:sourceSlice:sourceLevel:toTexture:destinationSlice:destinationLevel:sliceCount:levelCount:
 @abstract Copy whole surfaces between textures.
 @discussion Convenience function to copy sliceCount * levelCount whole surfaces between textures
 The source and destination pixel format must be identical.
 The source and destination sample count must be identical.
 The sourceLevel mip in sourceTexture must have the same dimension as the destinationLevel mip in destinationTexture.
 The sourceTexture must have at least sourceLevel + levelCount mips
 The destinationTexture must have at least destinationLevel + levelCount mips
 The sourceTexture must have at least sourceSlice + sliceCount array slices
 The destinationTexture must have at least destinationSlice + sliceCount array slices
 */
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel
              toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel
             sliceCount:(NSUInteger)sliceCount levelCount:(NSUInteger)levelCount API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @method copyFromTexture:toTexture:
 @abstract Copy as many whole surfaces as possible between textures.
 @discussion Convenience function that calls copyFromTexture:sourceSlice:sourceLevel:toTexture:destinationSlice:destinationLevel:sliceCount:levelCount:
 The source and destination pixel format must be identical.
 The source and destination sample count must be identical.
 Either:
 - sourceTexture must have a mip M with identical dimensions as the first mip of destinationTexture: sourceLevel = M, destinationLevel = 0
 - destinationTexture must have a mip M with identical dimensions as the first mip of sourceTexture: sourceLevel = 0, destinationLevel = M
 Computes: levelCount = min(sourceTexture.mipmapLevelCount - sourceLevel, destinationTexture.mipmapLevelCount - destinationLevel)
           sliceCount = min(sourceTexture.arrayLength, destinationTexture.arrayLength)
 Then invokes the method above using the computed parameters.
 */
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture toTexture:(id<MTLTexture>)destinationTexture API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @method copyFromBuffer:sourceOffset:toBuffer:destinationOffset:size:
 @abstract Basic memory copy between buffers.
 */
- (void)copyFromBuffer:(id <MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset toBuffer:(id <MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset size:(NSUInteger)size;

/*!
 @method updateFence:
 @abstract Update the fence to capture all GPU work so far enqueued by this encoder.
 @discussion The fence is updated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)updateFence:(id <MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method waitForFence:
 @abstract Prevent further GPU work until the fence is reached.
 @discussion The fence is evaluated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)waitForFence:(id <MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0));


    /*!
     @method getTextureAccessCounters:region:mipLevel:slice:type:resetCounters:countersBuffer:countersBufferOffset
     @abstract Copies tile access counters within specified region into provided buffer
     */
    -(void) getTextureAccessCounters:(id<MTLTexture>)texture
                              region:(MTLRegion)region
                            mipLevel:(NSUInteger)mipLevel
                               slice:(NSUInteger)slice
                       resetCounters:(BOOL)resetCounters
                      countersBuffer:(id<MTLBuffer>)countersBuffer
                countersBufferOffset:(NSUInteger)countersBufferOffset API_DEPRECATED("Access counters are no longer supported in Metal", macos(11.0, 26.4), macCatalyst(14.0, 26.4), ios(13.0, 26.4), tvos(16.0, 26.4));

    /*!
     @method resetTextureAccessCounters:region:mipLevel:slice:type:
     @abstract Resets tile access counters within specified region
     */
    -(void) resetTextureAccessCounters:(id<MTLTexture>)texture
                                region:(MTLRegion)region
                              mipLevel:(NSUInteger)mipLevel
                                 slice:(NSUInteger)slice API_DEPRECATED("Access counters are no longer supported in Metal", macos(11.0, 26.4), macCatalyst(14.0, 26.4), ios(13.0, 26.4), tvos(16.0, 26.4));



/*!
 @method optimizeContentsForGPUAccess:
 @abstract Optimizes the texture data to ensure the best possible performance when accessing content on the GPU at the expense of CPU-access performance.
 */
- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method optimizeContentsForGPUAccess:slice:level:
 @abstract Optimizes a subset of the texture data to ensure the best possible performance when accessing content on the GPU at the expense of CPU-access performance.
 */
- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture slice:(NSUInteger)slice level:(NSUInteger)level API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method optimizeContentsForCPUAccess:
 @abstract Optimizes the texture data to ensure the best possible performance when accessing content on the CPU at the expense of GPU-access performance.
 */
- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method optimizeContentsForCPUAccess:slice:level:
 @abstract Optimizes a subset of the texture data to ensure the best possible performance when accessing content on the CPU at the expense of GPU-access performance.
 */
- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture slice:(NSUInteger)slice level:(NSUInteger)level API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @method resetCommandsInBuffer:withRange:
 @abstract reset commands in a indirect command buffer using the GPU
 */
- (void) resetCommandsInBuffer:(id<MTLIndirectCommandBuffer>)buffer withRange:(NSRange)range API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method copyIndirectCommandBuffer:source:sourceRange:destination:destinationIndex
 @abstract copy a region of a buffer into a destination buffer starting at destinationIndex using the GPU
 */
- (void)copyIndirectCommandBuffer:(id <MTLIndirectCommandBuffer>)source sourceRange:(NSRange)sourceRange
                      destination:(id <MTLIndirectCommandBuffer>)destination destinationIndex:(NSUInteger)destinationIndex API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method optimizeIndirectCommandBuffer:withRange:
 @abstract Encodes a command that can improve the performance of a range of commands within an indirect command buffer.
 */
- (void)optimizeIndirectCommandBuffer:(id <MTLIndirectCommandBuffer>)indirectCommandBuffer withRange:(NSRange)range API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @method sampleCountersInBuffer:atSampleIndex:withBarrier:
 @abstract Sample hardware counters at this point in the blit encoder and
 store the counter sample into the sample buffer at the specified index.
 @param sampleBuffer The sample buffer to sample into
 @param sampleIndex The index into the counter buffer to write the sample.
 @param barrier Insert a barrier before taking the sample.  Passing
 YES will ensure that all work encoded before this operation in the encoder is
 complete but does not isolate the work with respect to other encoders.  Passing
 NO will allow the sample to be taken concurrently with other operations in this
 encoder.
 In general, passing YES will lead to more repeatable counter results but
 may negatively impact performance.  Passing NO will generally be higher performance
 but counter results may not be repeatable.
 @discussion On devices where MTLCounterSamplingPointAtBlitBoundary is unsupported,
 this method is not available and will generate an error if called.
 */
-(void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                atSampleIndex:(NSUInteger)sampleIndex
                  withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @method resolveCounters:inRange:destinationBuffer:destinationOffset:
 @param sampleBuffer The sample buffer to resolve.
 @param range The range of indices to resolve.
 @param destinationBuffer The buffer to resolve values into.
 @param destinationOffset The offset to begin writing values out to.  This must be a multiple of
 the minimum constant buffer alignment.
 @abstract Resolve the counters from the raw buffer to a processed buffer.
 @discussion Samples that encountered an error during resolve will be set to
 MTLCounterErrorValue.
 */
-(void)resolveCounters:(id<MTLCounterSampleBuffer>)sampleBuffer
               inRange:(NSRange)range
     destinationBuffer:(id<MTLBuffer>)destinationBuffer
     destinationOffset:(NSUInteger)destinationOffset API_AVAILABLE(macos(10.15), ios(14.0));



/// Encodes a command to copy data from a slice of the data plane of a tensor into a slice of the data plane of
/// another tensor.
///
/// If `sourceTensor` and `destinationTensor` are not aliasable, this command applies a reshape operation.
///
/// Ensure the first dimension of `sourceOrigin`, `sourceDimensions`, `destinationOrigin`,
/// and `destinationDimensions` is byte aligned.
///
/// - Parameters:
///   - sourceTensor: A tensor instance the method copies data from.
///   - sourceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in `sourceTensor`. Each element in this array corresponds to the dimension at the
///     same index in `sourceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sourceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from `sourceTensor`. Each element in this array corresponds to the dimension
///     at the same index in `sourceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sourceOrigin`.
///   - destinationTensor: A tensor instance the method copies data to.
///   - destinationOrigin: An array of per-dimension offsets that together locate the first element
///     to write in `destinationTensor`. Each element in this array corresponds to the dimension at
///     the same index in `destinationDimensions`. Each offset value represents the number of elements
///     from the start of that dimension.
///   - destinationDimensions: An array of per-dimension sizes that together define the extent of
///     the slice to write in `destinationTensor`. Each element in this array corresponds to the
///     dimension at the same index in `destinationOrigin`. Each size value represents the number of
///     elements to include along that dimension, starting from the corresponding offset in
///     `destinationOrigin`.
- (void)copyFromTensor:(id<MTLTensor>)sourceTensor
          sourceOrigin:(MTLTensorExtents *)sourceOrigin
      sourceDimensions:(MTLTensorExtents *)sourceDimensions
              toTensor:(id<MTLTensor>)destinationTensor
     destinationOrigin:(MTLTensorExtents *)destinationOrigin
 destinationDimensions:(MTLTensorExtents *)destinationDimensions API_AVAILABLE(macos(26.0), ios(26.0));




/// Encodes a command to copy data from a slice of a plane of a tensor into a slice of a plane of
/// another tensor.
///
/// If `sourceTensor` and `destinationTensor` are not aliasable, this command applies a reshape operation.
/// For auxiliary planes, specify origin and dimensions in plane coordinates by applying the corresponding auxiliary plane's block
/// factors.
///
/// Ensure the first dimension of `sourceOrigin`, `sourceDimensions`, `destinationOrigin`,
/// and `destinationDimensions` is byte aligned.
///
/// - Parameters:
///   - sourceTensor: A tensor instance the method copies data from.
///   - sourceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in `sourceTensor`. Each element in this array corresponds to the dimension at the
///     same index in `sourceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sourceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from `sourceTensor`. Each element in this array corresponds to the dimension
///     at the same index in `sourceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sourceOrigin`.
///   - sourcePlane: The plane the method copies data from.
///   - destinationTensor: A tensor instance the method copies data to.
///   - destinationOrigin: An array of per-dimension offsets that together locate the first element
///     to write in `destinationTensor`. Each element in this array corresponds to the dimension at
///     the same index in `destinationDimensions`. Each offset value represents the number of elements
///     from the start of that dimension.
///   - destinationDimensions: An array of per-dimension sizes that together define the extent of
///     the slice to write in `destinationTensor`. Each element in this array corresponds to the
///     dimension at the same index in `destinationOrigin`. Each size value represents the number of
///     elements to include along that dimension, starting from the corresponding offset in
///     `destinationOrigin`.
///   - destinationPlane: The plane the method copies data to.
- (void)copyFromTensor:(id<MTLTensor>)sourceTensor
          sourceOrigin:(MTLTensorExtents *)sourceOrigin
      sourceDimensions:(MTLTensorExtents *)sourceDimensions
           sourcePlane:(MTLTensorPlaneType)sourcePlane
              toTensor:(id<MTLTensor>)destinationTensor
     destinationOrigin:(MTLTensorExtents *)destinationOrigin
 destinationDimensions:(MTLTensorExtents *)destinationDimensions
      destinationPlane:(MTLTensorPlaneType)destinationPlane API_AVAILABLE(macos(27.0), ios(27.0));


@end
NS_ASSUME_NONNULL_END
