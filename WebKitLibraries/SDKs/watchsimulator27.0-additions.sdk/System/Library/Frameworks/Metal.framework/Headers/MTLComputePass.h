//
//  MTLComputePass.h
//  Metal
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>



#import <Metal/MTLTypes.h>
#import <Metal/MTLCommandBuffer.h>

#import <Metal/MTLCounters.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLComputePassSampleBufferAttachmentDescriptor : NSObject<NSCopying>
/*!
@property sampleBuffer
@abstract The sample buffer to store samples for the compute-pass defined samples.
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
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a compute pass will fail.
 */
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
/*!
 @property endOfEncoderSampleIndex
 @abstract The sample index to use to store the sample taken at the end of
 command encoder processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a compute pass will fail.
 */
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLComputePassSampleBufferAttachmentDescriptorArray : NSObject
/* Individual attachment state access */
- (MTLComputePassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLComputePassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

/*!
 @class MTLComputePassDescriptor
 @abstract MTLComputePassDescriptor represents a collection of attachments to be used to create a concrete compute command encoder
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLComputePassDescriptor : NSObject <NSCopying>

/*!
 @method computePassDescriptor
 @abstract Create an autoreleased default frame buffer descriptor
 */
+ (MTLComputePassDescriptor *)computePassDescriptor;

/*!
 @property dispatchType
 @abstract The dispatch type of the compute command encoder.
 */
@property (nonatomic) MTLDispatchType dispatchType;

/*!
 @property sampleBufferAttachments
 @abstract An array of sample buffers and associated sample indices.
 */
@property (readonly) MTLComputePassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

@end

NS_ASSUME_NONNULL_END

