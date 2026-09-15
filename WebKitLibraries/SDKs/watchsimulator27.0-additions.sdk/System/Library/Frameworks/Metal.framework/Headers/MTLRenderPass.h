//
//  MTLRenderPass.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>


typedef struct
{
    double red;
    double green;
    double blue;
    double alpha;
} MTLClearColor;

MTL_INLINE MTLClearColor MTLClearColorMake(double red, double green, double blue, double alpha);


@protocol MTLRasterizationRateMap;

#import <Metal/MTLTypes.h>

#import <Metal/MTLCounters.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;

typedef NS_ENUM(NSUInteger, MTLLoadAction) {
    MTLLoadActionDontCare = 0,
    MTLLoadActionLoad = 1,
    MTLLoadActionClear = 2,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLStoreAction) {
    MTLStoreActionDontCare = 0,
    MTLStoreActionStore = 1,
    MTLStoreActionMultisampleResolve = 2,
    MTLStoreActionStoreAndMultisampleResolve API_AVAILABLE(macos(10.12), ios(10.0)) = 3,
    MTLStoreActionUnknown API_AVAILABLE(macos(10.12), ios(10.0)) = 4,
    MTLStoreActionCustomSampleDepthStore API_AVAILABLE(macos(10.13), ios(11.0)) = 5,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_OPTIONS(NSUInteger, MTLStoreActionOptions) {
    MTLStoreActionOptionNone                  = 0,
    MTLStoreActionOptionCustomSamplePositions = 1 << 0,
} API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/// This enumeration controls if Metal accumulates visibility results between render encoders or resets them.
///
/// You can specify this property for ``MTLRenderCommandEncoders`` and for ``MTL4RenderCommandEncoders`` through
/// their descriptors' ``MTLRenderCommandEncoder/visibilityResultType`` and ``MTL4RenderCommandEncoder/visibilityResultType``
/// methods.
typedef NS_ENUM(NSInteger, MTLVisibilityResultType)
{
    /// Reset visibility result data when you create a render command encoder.
    MTLVisibilityResultTypeReset = 0,
    
    /// Accumulate visibility results data across multiple render passes.
    MTLVisibilityResultTypeAccumulate = 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

@protocol MTLTexture;
@protocol MTLBuffer;

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassAttachmentDescriptor : NSObject <NSCopying>

/*!
 @property texture
 @abstract The MTLTexture object for this attachment.
 */
@property (nullable, nonatomic, strong) id <MTLTexture> texture;

/*!
 @property level
 @abstract The mipmap level of the texture to be used for rendering.  Default is zero.
 */
@property (nonatomic) NSUInteger level;

/*!
 @property slice
 @abstract The slice of the texture to be used for rendering.  Default is zero.
 */
@property (nonatomic) NSUInteger slice;

/*!
 @property depthPlane
 @abstract The depth plane of the texture to be used for rendering.  Default is zero.
 */
@property (nonatomic) NSUInteger depthPlane;

/*!
 @property resolveTexture
 @abstract The texture used for multisample resolve operations.  Only used (and required)
 if the store action is set to MTLStoreActionMultisampleResolve.
 */
@property (nullable, nonatomic, strong) id <MTLTexture> resolveTexture;

/*!
 @property resolveLevel
 @abstract The mipmap level of the resolve texture to be used for multisample resolve.  Defaults to zero.
 */
@property (nonatomic) NSUInteger resolveLevel;

/*!
 @property resolveLevel
 @abstract The texture slice of the resolve texture to be used for multisample resolve.  Defaults to zero.
 */
@property (nonatomic) NSUInteger resolveSlice;

/*!
 @property resolveDepthPlane
 @abstract The texture depth plane of the resolve texture to be used for multisample resolve.  Defaults to zero.
 */
@property (nonatomic) NSUInteger resolveDepthPlane;

/*!
 @property loadAction
 @abstract The action to be performed with this attachment at the beginning of a render pass.  Default is
 MTLLoadActionDontCare unless specified by a creation or init method.
 */
@property (nonatomic) MTLLoadAction loadAction;

/*!
 @property storeAction
 @abstract The action to be performed with this attachment at the end of a render pass.  Default is
 MTLStoreActionDontCare unless specified by a creation or init method.
 */
@property (nonatomic) MTLStoreAction storeAction;

/*!
 @property storeActionOptions
 @abstract Optional configuration for the store action performed with this attachment at the end of a render pass.  Default is
 MTLStoreActionOptionNone.
 */
@property (nonatomic) MTLStoreActionOptions storeActionOptions API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor

/*!
 @property clearColor
 @abstract The clear color to be used if the loadAction property is MTLLoadActionClear
 */
@property (nonatomic) MTLClearColor clearColor;

@end

/*!
 @enum MTLMultisampleDepthResolveFilter
 @abstract Controls the MSAA depth resolve operation. Supported on iOS GPU Family 3 and later.
 */
typedef NS_ENUM(NSUInteger, MTLMultisampleDepthResolveFilter)
{
    MTLMultisampleDepthResolveFilterSample0 = 0,
    MTLMultisampleDepthResolveFilterMin = 1,
    MTLMultisampleDepthResolveFilterMax = 2,
} API_AVAILABLE(macos(10.14), ios(9.0));

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor

/*!
 @property clearDepth
 @abstract The clear depth value to be used if the loadAction property is MTLLoadActionClear
 */
@property (nonatomic) double clearDepth;

/*!
 @property resolveFilter
 @abstract The filter to be used for depth multisample resolve.  Defaults to MTLMultisampleDepthResolveFilterSample0.
 */
@property (nonatomic) MTLMultisampleDepthResolveFilter depthResolveFilter API_AVAILABLE(macos(10.14), ios(9.0));

@end



/*!
 @enum MTLMultisampleStencilResolveFilter
 @abstract Controls the MSAA stencil resolve operation.
 */
typedef NS_ENUM(NSUInteger, MTLMultisampleStencilResolveFilter)
{
    /*!
     @constant MTLMultisampleStencilResolveFilterSample0
     @abstract The stencil sample corresponding to sample 0. This is the default behavior.
     */
    MTLMultisampleStencilResolveFilterSample0               = 0,

    /*!
     @constant MTLMultisampleStencilResolveFilterDepthResolvedSample
     @abstract The stencil sample corresponding to whichever depth sample is selected by the depth resolve filter. If depth resolve is not enabled, the stencil sample is chosen based on what a depth resolve filter would have selected.
     */
    MTLMultisampleStencilResolveFilterDepthResolvedSample   = 1,
} API_AVAILABLE(macos(10.14), ios(12.0), tvos(14.5));



MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor

/*!
 @property clearStencil
 @abstract The clear stencil value to be used if the loadAction property is MTLLoadActionClear
 */
@property (nonatomic) uint32_t clearStencil;

/*!
 @property stencilResolveFilter
 @abstract The filter to be used for stencil multisample resolve. Defaults to MTLMultisampleStencilResolveFilterSample0.
 */
@property (nonatomic) MTLMultisampleStencilResolveFilter stencilResolveFilter API_AVAILABLE(macos(10.14), ios(12.0), tvos(14.5));

@end



MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassColorAttachmentDescriptorArray : NSObject
/* Individual attachment state access */
- (MTLRenderPassColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLRenderPassColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLRenderPassSampleBufferAttachmentDescriptor : NSObject<NSCopying>
/*!
@property sampleBuffer
@abstract The sample buffer to store samples for the render-pass defined samples.
If sampleBuffer is non-nil, the sample indices will be used to store samples into
the sample buffer.  If no sample buffer is provided, no samples will be taken.
If any of the sample indices are specified as MTLCounterDontSample, no sample
will be taken for that action.
*/
@property (nullable, nonatomic, retain) id<MTLCounterSampleBuffer> sampleBuffer API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property startOfVertexSampleIndex
 @abstract The sample index to use to store the sample taken at the start of
 vertex processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a render pass will fail.
 */
@property (nonatomic) NSUInteger startOfVertexSampleIndex API_AVAILABLE(macos(11.0), ios(14.0));
/*!
 @property endOfVertexSampleIndex
 @abstract The sample index to use to store the sample taken at the end of
 vertex processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a render pass will fail.
 */
@property (nonatomic) NSUInteger endOfVertexSampleIndex API_AVAILABLE(macos(11.0), ios(14.0));
/*!
 @property startOfFragmentSampleIndex
 @abstract The sample index to use to store the sample taken at the start of
 fragment processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a render pass will fail.
 */
@property (nonatomic) NSUInteger startOfFragmentSampleIndex API_AVAILABLE(macos(11.0), ios(14.0));
/*!
 @property endOfFragmentSampleIndex
 @abstract The sample index to use to store the sample taken at the end of
 fragment processing.  Setting the value to MTLCounterDontSample will cause
 this sample to be omitted.
 @discussion On devices where MTLCounterSamplingPointAtStageBoundary is unsupported,
 this sample index is invalid and must be set to MTLCounterDontSample or creation of a render pass will fail.
 */
@property (nonatomic) NSUInteger endOfFragmentSampleIndex API_AVAILABLE(macos(11.0), ios(14.0));
@end

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLRenderPassSampleBufferAttachmentDescriptorArray : NSObject
/* Individual attachment state access */
- (MTLRenderPassSampleBufferAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLRenderPassSampleBufferAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

/*!
 @class MTLRenderPassDescriptor
 @abstract MTLRenderPassDescriptor represents a collection of attachments to be used to create a concrete render command encoder
 */
MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPassDescriptor : NSObject <NSCopying>

/*!
 @method renderPassDescriptor
 @abstract Create an autoreleased default frame buffer descriptor
 */
+ (MTLRenderPassDescriptor *)renderPassDescriptor;

@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;

@property (copy, nonatomic, null_resettable) MTLRenderPassDepthAttachmentDescriptor *depthAttachment;

@property (copy, nonatomic, null_resettable) MTLRenderPassStencilAttachmentDescriptor *stencilAttachment;

/*!
 @property visibilityResultBuffer:
 @abstract Buffer into which samples passing the depth and stencil tests are counted.
 */
@property (nullable, nonatomic, strong) id <MTLBuffer> visibilityResultBuffer;

/*!
 @property renderTargetArrayLength:
 @abstract The number of active layers
 */
@property (nonatomic) NSUInteger renderTargetArrayLength API_AVAILABLE(macos(10.11), ios(12.0), tvos(14.5));


/*!
 @property imageblockSampleLength:
 @abstract The per sample size in bytes of the largest explicit imageblock layout in the renderPass.
 */
@property (nonatomic) NSUInteger imageblockSampleLength API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @property threadgroupMemoryLength:
 @abstract The per tile size in bytes of the persistent threadgroup memory allocation.
 */
@property (nonatomic) NSUInteger threadgroupMemoryLength API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @property tileWidth:
 @abstract The width in pixels of the tile.
 @discussion Defaults to 0. Zero means Metal chooses a width that fits within the local memory.
 */
@property (nonatomic) NSUInteger tileWidth API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @property tileHeight:
 @abstract The height in pixels of the tile.
 @discussion Defaults to 0. Zero means Metal chooses a height that fits within the local memory.
 */
@property (nonatomic) NSUInteger tileHeight API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @property defaultRasterSampleCount:
 @abstract The raster sample count for the render pass when no attachments are given.
 */
@property (nonatomic) NSUInteger defaultRasterSampleCount API_AVAILABLE(ios(11.0), tvos(14.5), macos(10.15), macCatalyst(13.0));

/*!
 @property renderTargetWidth:
 @abstract The width in pixels to constrain the render target to.
 @discussion Defaults to 0. If non-zero the value must be smaller than or equal to the minimum width of all attachments.
 */
@property (nonatomic) NSUInteger renderTargetWidth API_AVAILABLE(ios(11.0), tvos(14.5), macos(10.15), macCatalyst(13.0));

/*!
 @property renderTargetHeight:
 @abstract The height in pixels to constrain the render target to.
 @discussion Defaults to 0. If non-zero the value must be smaller than or equal to the minimum height of all attachments.
 */
@property (nonatomic) NSUInteger renderTargetHeight API_AVAILABLE(ios(11.0), tvos(14.5), macos(10.15), macCatalyst(13.0));

/*!
 @method setSamplePositions:count:
 @abstract Configure the custom sample positions, to be used in MSAA rendering (i.e. when sample count > 1).
 @param positions The source array for custom sample position data.
 @param count Specifies the length of the positions array, and must be a valid sample count or 0 (to disable custom sample positions).
 */
- (void)setSamplePositions:(const MTLSamplePosition * _Nullable)positions count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @method getSamplePositions:count:
 @abstract Retrieve the previously configured custom sample positions. The positions input array will only be modified when count specifies a length sufficient for the number of previously configured positions.
 @param positions The destination array for custom sample position data.
 @param count Specifies the length of the positions array, which must be large enough to hold all configured sample positions.
 @return The number of previously configured custom sample positions.
 */
- (NSUInteger)getSamplePositions:(MTLSamplePosition * _Nullable)positions count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0));


/*!
 @property rasterizationRateMap
 @abstract The variable rasterization rate map to use when rendering this pass, or nil to not use variable rasterization rate.
 @discussion The default value is nil. Enabling variable rasterization rate allows for decreasing the rasterization rate in unimportant regions of screen space.
 */
@property (nullable, nonatomic, strong) id<MTLRasterizationRateMap> rasterizationRateMap API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 @property sampleBufferAttachments
 @abstract An array of sample buffers and associated sample indices.
 */
@property (readonly) MTLRenderPassSampleBufferAttachmentDescriptorArray * sampleBufferAttachments API_AVAILABLE(macos(11.0), ios(14.0));

/// Specifies if Metal accumulates visibility results between render encoders or resets them.
@property (nonatomic) MTLVisibilityResultType visibilityResultType API_AVAILABLE(macos(26.0), ios(26.0));

/// Specifies if the render pass should support color attachment mapping.
@property (nonatomic) BOOL supportColorAttachmentMapping API_AVAILABLE(macos(26.0), ios(26.0));

@end

// Inline definitions
MTL_INLINE MTLClearColor MTLClearColorMake(double red, double green, double blue, double alpha)
{
    MTLClearColor result;
    result.red = red;
    result.green = green;
    result.blue = blue;
    result.alpha = alpha;
    return result;
}

NS_ASSUME_NONNULL_END
