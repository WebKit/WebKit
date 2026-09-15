//
//  MTLBlitPass.h
//  Metal
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>


#import <Metal/MTLTypes.h>

#import <Metal/MTLCounters.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLBlitPassSampleBufferAttachmentDescriptor : NSObject<NSCopying>
/*!
@property sampleBuffer
@abstract The sample buffer to store samples for the blit-pass defined samples.
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
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a blit pass will fail.
 */
@property (nonatomic) NSUInteger startOfEncoderSampleIndex;
/*!
 @property endOfEncoderSampleIndex
 @abstract The sample index to use to store the sample taken at the end of
 Command encoder processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a blit pass will fail.
 */
@property (nonatomic) NSUInteger endOfEncoderSampleIndex;
@end

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLBlitPassSampleBufferAttachmentDescriptorArray : NSObject
/* Individual attachment state access */
- (MTLBlitPassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLBlitPassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

/*!
 @class MTLBlitPassDescriptor
 @abstract MTLBlitPassDescriptor represents a collection of attachments to be used to create a concrete blit command encoder
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLBlitPassDescriptor : NSObject <NSCopying>

/*!
 @method blitPassDescriptor
 @abstract Create an autoreleased default frame buffer descriptor
 */
+ (MTLBlitPassDescriptor *)blitPassDescriptor;


/*!
 @property sampleBufferAttachments
 @abstract An array of sample buffers and associated sample indices.
 */
@property (readonly) MTLBlitPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments;

@end

NS_ASSUME_NONNULL_END
