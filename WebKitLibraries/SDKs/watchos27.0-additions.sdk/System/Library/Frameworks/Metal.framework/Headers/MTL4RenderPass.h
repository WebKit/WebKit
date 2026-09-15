//
//  MTL4RenderPass.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4RenderPass_h
#define MTL4RenderPass_h

#import <Foundation/Foundation.h>

#import <Metal/MTLDefines.h>
#import <Metal/MTLRenderPass.h>


NS_ASSUME_NONNULL_BEGIN

/// Describes a render pass.
///
/// You use render pass descriptors to create instances of ``MTL4RenderCommandEncoder`` and encode draw
/// commands into instances of ``MTL4CommandBuffer``.
///
/// To create render command encoders, you typically call ``MTL4CommandBuffer/renderCommandEncoderWithDescriptor:``.
/// The ``MTL4CommandBuffer/renderCommandEncoderWithDescriptor:options:`` variant of this method allows you to specify
/// additional options to encode a render pass in parallel from multiple CPU cores by creating *suspending* and *resuming*
/// render passes.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPassDescriptor : NSObject <NSCopying>

/// Accesses the array of state information for render attachments that store color data.
@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;

/// Accesses state information for a render attachment that stores depth data.
@property (copy, nonatomic, null_resettable) MTLRenderPassDepthAttachmentDescriptor *depthAttachment;

/// Accesses state information for a render attachment that stores stencil data.
@property (copy, nonatomic, null_resettable) MTLRenderPassStencilAttachmentDescriptor *stencilAttachment;

/// Assigns the number of layers that all attachments this descriptor references have.
@property (nonatomic) NSUInteger renderTargetArrayLength;

/// Assigns the per-sample size, in bytes, of the largest explicit imageblock layout in the render pass.
@property (nonatomic) NSUInteger imageblockSampleLength;

/// Assigns the per-tile size, in bytes, of the persistent threadgroup memory allocation of this render pass.
@property (nonatomic) NSUInteger threadgroupMemoryLength;

/// The width of the tiles, in pixels, a render pass you create with this descriptor applies to its attachments.
///
/// For tile-based rendering, Metal divides each render attachment into smaller regions, or _tiles_.
/// The property's default is `0`, which tells Metal to select a size that fits in tile memory.
///
/// See <doc:tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering>
/// for more information about tiles, tile memory, and deferred rendering.
@property (nonatomic) NSUInteger tileWidth;

/// The height of the tiles, in pixels, a render pass you create with this descriptor applies to its attachments.
///
/// For tile-based rendering, Metal divides each render attachment into smaller regions, or _tiles_.
/// The property's default is `0`, which tells Metal to select a size that fits in tile memory.
///
/// See <doc:tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering>
/// for more information about tiles, tile memory, and deferred rendering.
@property (nonatomic) NSUInteger tileHeight;

/// Sets the default raster sample count for the render pass when it references no attachments.
@property (nonatomic) NSUInteger defaultRasterSampleCount;

/// Sets the width, in pixels, to which Metal constrains the render target.
///
/// When this value is non-zero, you need to assign it to be smaller than or equal to the minimum width of all attachments.
///
/// The default value of this property is `0`.
///
@property (nonatomic) NSUInteger renderTargetWidth;

/// Sets the height, in pixels, to which Metal constrains the render target.
///
/// When this value is non-zero, you need to assign it to be smaller than or equal to the minimum height of all attachments.
///
/// The default value of this property is `0`.
///
@property (nonatomic) NSUInteger renderTargetHeight;

/// Assigns an optional variable rasterization rate map that Metal uses in the render pass.
///
/// Enabling variable rasterization rate allows Metal to decrease the rasterization rate, typically in unimportant
/// regions of color attachments, to accelerate processing.
///
/// When set to `nil`, the default, Metal doesn't use variable rasterization rate.
///
@property (nullable, nonatomic, strong) id<MTLRasterizationRateMap> rasterizationRateMap;

/// Configures a buffer into which Metal writes counts of fragments (pixels) passing the depth and stencil tests.
@property (nullable, nonatomic, strong) id <MTLBuffer> visibilityResultBuffer;

/// Determines if Metal accumulates visibility results between render encoders or resets them.
@property (nonatomic) MTLVisibilityResultType visibilityResultType;

/// Configures the custom sample positions to use in MSAA rendering.
///
/// - Parameters:
///   - positions: Array of ``MTLSamplePosition`` instances.
///   - count:     Number of ``MTLSamplePosition`` instances in the array. This value
///                needs to be a valid sample count, or `0` to disable custom sample positions.
- (void)setSamplePositions:(const MTLSamplePosition * _Nullable)positions
                     count:(NSUInteger)count;

/// Retrieves the previously-configured custom sample positions.
///
/// This method stores the app's last set custom sample positions into an output array. Metal only modifies the array
/// when the `count` parameter consists of a length sufficient to store the number of sample positions.
///
/// - Parameters:
///   - positions: The destination array where Metal stores ``MTLSamplePosition`` instances.
///   - count:     Number of ``MTLSamplePosition`` instances in the array. This array
///                needs to be large enough to store all sample positions.
///
/// - Returns: The number of previously-configured custom sample positions.
- (NSUInteger)getSamplePositions:(MTLSamplePosition * _Nullable)positions
                           count:(NSUInteger)count;

/// Controls if the render pass supports color attachment mapping.
@property (nonatomic) BOOL supportColorAttachmentMapping;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4RenderPass_h
