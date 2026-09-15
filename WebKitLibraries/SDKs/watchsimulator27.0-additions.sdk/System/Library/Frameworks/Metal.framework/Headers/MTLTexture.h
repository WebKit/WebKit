//
//  MTLTexture.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLBuffer.h>
#import <Metal/MTLTypes.h>


#import <IOSurface/IOSurfaceRef.h>


NS_ASSUME_NONNULL_BEGIN
/*!
 @enum MTLTextureType
 @abstract MTLTextureType describes the dimensionality of each image, and if multiple images are arranged into an array or cube.
 */
typedef NS_ENUM(NSUInteger, MTLTextureType)
{
    MTLTextureType1D = 0,
    MTLTextureType1DArray = 1,
    MTLTextureType2D = 2,
    MTLTextureType2DArray = 3,
    MTLTextureType2DMultisample = 4,
    MTLTextureTypeCube = 5,
    MTLTextureTypeCubeArray API_AVAILABLE(macos(10.11), ios(11.0)) = 6,
    MTLTextureType3D = 7,
    MTLTextureType2DMultisampleArray API_AVAILABLE(macos(10.14), ios(14.0), tvos(16.0)) = 8,
    MTLTextureTypeTextureBuffer API_AVAILABLE(macos(10.14), ios(12.0)) = 9
} API_AVAILABLE(macos(10.11), ios(8.0));


typedef NS_ENUM(uint8_t, MTLTextureSwizzle) {
    MTLTextureSwizzleZero = 0,
    MTLTextureSwizzleOne = 1,
    MTLTextureSwizzleRed = 2,
    MTLTextureSwizzleGreen = 3,
    MTLTextureSwizzleBlue = 4,
    MTLTextureSwizzleAlpha = 5,
} API_AVAILABLE(macos(10.15), ios(13.0));

typedef struct
{
    MTLTextureSwizzle red;
    MTLTextureSwizzle green;
    MTLTextureSwizzle blue;
    MTLTextureSwizzle alpha;
} MTLTextureSwizzleChannels API_AVAILABLE(macos(10.15), ios(13.0));

API_AVAILABLE(macos(10.15), ios(13.0)) NS_SWIFT_UNAVAILABLE("Use MTLTextureSwizzleChannels.init instead")
MTL_INLINE MTLTextureSwizzleChannels MTLTextureSwizzleChannelsMake(MTLTextureSwizzle r, MTLTextureSwizzle g, MTLTextureSwizzle b, MTLTextureSwizzle a)
{
    MTLTextureSwizzleChannels swizzle;
    swizzle.red = r;
    swizzle.green = g;
    swizzle.blue = b;
    swizzle.alpha = a;
    return swizzle;
}

#define MTLTextureSwizzleChannelsDefault (MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha))




MTL_EXPORT API_AVAILABLE(macos(10.14), ios(13.0))
@interface MTLSharedTextureHandle : NSObject <NSSecureCoding>
{
    struct MTLSharedTextureHandlePrivate *_priv;
}

/*!
 @property device
 @abstract The device this texture was created against.
 @discussion This shared texture handle can only be used with this device.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property label
 @abstract A copy of the original texture's label property, if any
*/
@property (readonly, nullable) NSString *label;

@end

/*!
 @enum MTLTextureUsage
 @abstract MTLTextureUsage declares how the texture will be used over its lifetime (bitwise OR for multiple uses).
 @discussion This information may be used by the driver to make optimization decisions.
*/
typedef NS_OPTIONS(NSUInteger, MTLTextureUsage)
{
    MTLTextureUsageUnknown         = 0x0000,
    MTLTextureUsageShaderRead      = 0x0001,
    MTLTextureUsageShaderWrite     = 0x0002,
    MTLTextureUsageRenderTarget    = 0x0004,
    MTLTextureUsagePixelFormatView = 0x0010,
    MTLTextureUsageShaderAtomic API_AVAILABLE(macos(14.0), ios(17.0)) = 0x0020,
} API_AVAILABLE(macos(10.11), ios(9.0));

typedef NS_ENUM(NSInteger, MTLTextureCompressionType)
{
    MTLTextureCompressionTypeLossless = 0,
    MTLTextureCompressionTypeLossy = 1,
} API_AVAILABLE(macos(12.5), ios(15.0), tvos(16.0));

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLTextureDescriptor : NSObject <NSCopying>

/*!
 @method texture2DDescriptorWithPixelFormat:width:height:mipmapped:
 @abstract Create a TextureDescriptor for a common 2D texture.
 */
+ (MTLTextureDescriptor*)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat width:(NSUInteger)width height:(NSUInteger)height mipmapped:(BOOL)mipmapped;

/*!
 @method textureCubeDescriptorWithPixelFormat:size:mipmapped:
 @abstract Create a TextureDescriptor for a common Cube texture.
 */
+ (MTLTextureDescriptor*)textureCubeDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat size:(NSUInteger)size mipmapped:(BOOL)mipmapped;

/*!
 @method textureBufferDescriptorWithPixelFormat:width:resourceOptions:usage:
 @abstract Create a TextureDescriptor for a common texture buffer.
 */
+ (MTLTextureDescriptor*)textureBufferDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat
                                                          width:(NSUInteger)width
                                                resourceOptions:(MTLResourceOptions)resourceOptions
                                                          usage:(MTLTextureUsage)usage API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property type
 @abstract The overall type of the texture to be created. The default value is MTLTextureType2D.
 */
@property (readwrite, nonatomic) MTLTextureType textureType;

/*!
 @property pixelFormat
 @abstract The pixel format to use when allocating this texture. This is also the pixel format that will be used to when the caller writes or reads pixels from this texture. The default value is MTLPixelFormatRGBA8Unorm.
 */
@property (readwrite, nonatomic) MTLPixelFormat pixelFormat;

/*!
 @property width
 @abstract The width of the texture to create. The default value is 1.
 */
@property (readwrite, nonatomic) NSUInteger width;

/*!
 @property height
 @abstract The height of the texture to create. The default value is 1.
 @discussion height If allocating a 1D texture, height must be 1.
 */
@property (readwrite, nonatomic) NSUInteger height;

/*!
 @property depth
 @abstract The depth of the texture to create. The default value is 1.
 @discussion depth When allocating any texture types other than 3D, depth must be 1.
 */
@property (readwrite, nonatomic) NSUInteger depth;

/*!
 @property mipmapLevelCount
 @abstract The number of mipmap levels to allocate. The default value is 1.
 @discussion When creating Buffer and Multisample textures, mipmapLevelCount must be 1.
 */
@property (readwrite, nonatomic) NSUInteger mipmapLevelCount;

/*!
 @property sampleCount
 @abstract The number of samples in the texture to create. The default value is 1.
 @discussion When creating Buffer textures sampleCount must be 1. Implementations may round sample counts up to the next supported value.
 */
@property (readwrite, nonatomic) NSUInteger sampleCount;

/*!
 @property arrayLength
 @abstract The number of array elements to allocate. The default value is 1.
 @discussion When allocating any non-Array texture type, arrayLength has to be 1. Otherwise it must be set to something greater than 1 and less than 2048.
 */
@property (readwrite, nonatomic) NSUInteger arrayLength;

/*!
 @property resourceOptions
 @abstract Options to control memory allocation parameters, etc.
 @discussion Contains a packed set of the storageMode, cpuCacheMode and hazardTrackingMode properties.
 */
@property (readwrite, nonatomic) MTLResourceOptions resourceOptions;

/*!
 @property cpuCacheMode
 @abstract Options to specify CPU cache mode of texture resource.
 */
@property (readwrite, nonatomic) MTLCPUCacheMode cpuCacheMode API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property storageMode
 @abstract To specify storage mode of texture resource.
 */
@property (readwrite, nonatomic) MTLStorageMode storageMode API_AVAILABLE(macos(10.11), ios(9.0));


/*!
 @property hazardTrackingMode
 @abstract Set hazard tracking mode for the texture. The default value is MTLHazardTrackingModeDefault.
 @discussion
 For resources created from the device, MTLHazardTrackingModeDefault is treated as MTLHazardTrackingModeTracked.
 For resources created on a heap, MTLHazardTrackingModeDefault is treated as the hazardTrackingMode of the heap itself.
 In either case, it is possible to opt-out of hazard tracking by setting MTLHazardTrackingModeUntracked.
 It is not possible to opt-in to hazard tracking on a heap that itself is not hazard tracked.
 For optimal performance, perform hazard tracking manually through MTLFence or MTLEvent instead.
 */
@property (readwrite, nonatomic) MTLHazardTrackingMode hazardTrackingMode API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @property usage
 @abstract Description of texture usage
 */
@property (readwrite, nonatomic) MTLTextureUsage usage API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property allowGPUOptimizedContents
 @abstract Allow GPU-optimization for the contents of this texture. The default value is true.
 @discussion Useful for opting-out of GPU-optimization when implicit optimization (e.g. RT writes) is regressing CPU-read-back performance. See the documentation for optimizeContentsForGPUAccess: and optimizeContentsForCPUAccess: APIs.
 */
@property (readwrite, nonatomic) BOOL allowGPUOptimizedContents API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property compressionType
 @abstract Controls how the texture contents will be compressed when written to by the GPU. Compression can be used to reduce the bandwidth usage and storage requirements of a texture.
 @discussion The default compression type is lossless, meaning that no loss of precision will occur when the texture content is modified.
 Losslessly compressed textures may benefit from reduced bandwidth usage when regions of correlated color values are written, but do not benefit from reduced storage requirements.
 Enabling lossy compression for textures that can tolerate some precision loss will guarantee both reduced bandwidth usage and reduced storage requirements.
 The amount of precision loss depends on the color values stored; regions with correlated color values can be represented with limited to no precision loss, whereas regions with unrelated color values suffer more precision loss.
 Enabling lossy compression requires both storageMode == MTLStorageModePrivate, allowGPUOptimizedContents == YES, and cannot be combined with either MTLTextureUsagePixelFormatView, MTLTextureUsageShaderWrite, MTLTextureUsageShaderAtomic, MTLTextureType1D(Array) or MTLTextureTypeTextureBuffer.
 Moreover, not all MTLPixelFormat are supported with lossy compression, verify that the MTLDevice's GPU family supports the lossy compression feature for the pixelFormat requested.
 Set allowGPUOptimizedContents to NO to opt out of both lossless and lossy compression; such textures do not benefit from either reduced bandwidth usage or reduced storage requirements, but have predictable CPU readback performance.
 */
@property (readwrite, nonatomic) MTLTextureCompressionType compressionType API_AVAILABLE(macos(12.5), ios(15.0), tvos(16.0));

/*!
 @property swizzle
 @abstract Channel swizzle to use when reading or sampling from the texture, the default value is MTLTextureSwizzleChannelsDefault.
 */
@property (readwrite, nonatomic) MTLTextureSwizzleChannels swizzle API_AVAILABLE(macos(10.15), ios(13.0));

/// Determines the page size for a placement sparse texture.
///
/// Set this property to a non-zero value to create a *placement sparse texture*.
///
/// Placement sparse textures are instances of ``MTLTexture`` that you assign memory to using a ``MTLHeap`` instance
/// of type ``MTLHeapType/MTLHeapTypePlacement`` and a ``MTLHeapDescriptor/maxCompatiblePlacementSparsePageSize``
/// at least as large as the ``MTLSparsePageSize`` value you assign to this property.
///
/// This value is 0 by default.
@property (readwrite, nonatomic) MTLSparsePageSize placementSparsePageSize API_AVAILABLE(macos(26.0), ios(26.0));

@end


MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLTextureViewDescriptor : NSObject <NSCopying>

/*!
 @property pixelFormat
 @abstract A desired pixel format of a texture view.
 */
@property (readwrite, nonatomic) MTLPixelFormat pixelFormat;

/*!
 @property textureType
 @abstract A desired texture view of a texture view.
 */
@property (readwrite, nonatomic) MTLTextureType textureType;

/*!
 @property levelRange
 @abstract A desired range of mip levels of a texture view.
 */
@property (readwrite, nonatomic) NSRange levelRange;

/*!
 @property sliceRange
 @abstract A desired range of slices of a texture view.
 */
@property (readwrite, nonatomic) NSRange sliceRange;

/*!
 @property swizzle
 @abstract A desired swizzle format of a texture view.
 */
@property (readwrite, nonatomic) MTLTextureSwizzleChannels swizzle;
 

@end


/*!
 @protocol MTLTexture
 @abstract MTLTexture represents a collection of 1D, 2D, or 3D images.
 @discussion
 Each image in a texture is a 1D, 2D, 2DMultisample, or 3D image. The texture contains one or more images arranged in a mipmap stack. If there are multiple mipmap stacks, each one is referred to as a slice of the texture. 1D, 2D, 2DMultisample, and 3D textures have a single slice. In 1DArray and 2DArray textures, every slice is an array element. A Cube texture always has 6 slices, one for each face. In a CubeArray texture, each set of six slices is one element in the array.
 
 Most APIs that operate on individual images in a texture address those images via a tuple of a Slice, and Mipmap Level within that slice.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLTexture <MTLResource>

/*!
 @property rootResource
 @abstract The resource this texture was created from. It may be a texture or a buffer. If this texture is not reusing storage of another MTLResource, then nil is returned.
 */
@property (nullable, readonly) id <MTLResource> rootResource API_DEPRECATED("Use parentTexture or buffer instead", macos(10.11, 10.12), ios(8.0, 10.0));

/*!
 @property parentTexture
 @abstract The texture this texture view was created from, or nil if this is not a texture view or it was not created from a texture.
 */
@property (nullable, readonly) id <MTLTexture> parentTexture API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property parentRelativeLevel
 @abstract The base level of the texture this texture view was created from, or 0 if this is not a texture view.
 */
@property (readonly) NSUInteger parentRelativeLevel API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property parentRelativeSlice
 @abstract The base slice of the texture this texture view was created from, or 0 if this is not a texture view.
 */
@property (readonly) NSUInteger parentRelativeSlice API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property buffer
 @abstract The buffer this texture view was created from, or nil if this is not a texture view or it was not created from a buffer.
 */
@property (nullable, readonly) id <MTLBuffer> buffer API_AVAILABLE(macos(10.12), ios(9.0));

/*!
 @property bufferOffset
 @abstract The offset of the buffer this texture view was created from, or 0 if this is not a texture view.
 */
@property (readonly) NSUInteger bufferOffset API_AVAILABLE(macos(10.12), ios(9.0));

/*!
 @property bufferBytesPerRow
 @abstract The bytesPerRow of the buffer this texture view was created from, or 0 if this is not a texture view.
 */
@property (readonly) NSUInteger bufferBytesPerRow API_AVAILABLE(macos(10.12), ios(9.0));


/*!
 @property iosurface
 @abstract If this texture was created from an IOSurface, this returns a reference to that IOSurface. iosurface is nil if this texture was not created from an IOSurface.
 */
@property (nullable, readonly) IOSurfaceRef iosurface API_AVAILABLE(macos(10.11), ios(11.0));

/*!
 @property iosurfacePlane
 @abstract If this texture was created from an IOSurface, this returns the plane of the IOSurface from which the texture was created. iosurfacePlane is 0 if this texture was not created from an IOSurface.
 */
@property (readonly) NSUInteger iosurfacePlane API_AVAILABLE(macos(10.11), ios(11.0));

/*!
 @property type
 @abstract The type of this texture.
 */
@property (readonly) MTLTextureType textureType;

/*!
 @property pixelFormat
 @abstract The MTLPixelFormat that is used to interpret this texture's contents.
 */
@property (readonly) MTLPixelFormat pixelFormat;

/*!
 @property width
 @abstract The width of the MTLTexture instance in pixels.
 */
@property (readonly) NSUInteger width;

/*!
 @property height
 @abstract The height of the MTLTexture instance in pixels.
 @discussion. height is 1 if the texture is 1D.
 */
@property (readonly) NSUInteger height;

/*!
 @property depth
 @abstract The depth of this MTLTexture instance in pixels.
 @discussion If this MTLTexture is not a 3D texture, the depth is 1
 */
@property (readonly) NSUInteger depth;

/*!
 @property mipmapLevelCount
 @abstract The number of mipmap levels in each slice of this MTLTexture.
 */
@property (readonly) NSUInteger mipmapLevelCount;

/*!
 @property sampleCount
 @abstract The number of samples in each pixel of this MTLTexture.
 @discussion If this texture is any type other than 2DMultisample, samples is 1.
 */
@property (readonly) NSUInteger sampleCount;

/*!
 @property arrayLength
 @abstract The number of array elements in this MTLTexture.
 @discussion For non-Array texture types, arrayLength is 1.
 */
@property (readonly) NSUInteger arrayLength;

/*!
 @property usage
 @abstract Description of texture usage.
 */
@property (readonly) MTLTextureUsage usage;

/*!
 @property shareable
 @abstract If YES, this texture can be shared with other processes.
 @discussion Texture can be shared across process addres space boundaries through use of sharedTextureHandle and XPC.
 */
@property (readonly, getter = isShareable) BOOL shareable API_AVAILABLE(macos(10.14), ios(13.0));

/*!
 @property framebufferOnly
 @abstract If YES, this texture can only be used with a MTLAttachmentDescriptor, and cannot be used as a texture argument for MTLRenderCommandEncoder, MTLBlitCommandEncoder, or MTLComputeCommandEncoder. Furthermore, when this property's value is YES, readPixels/writePixels may not be used with this texture.
 @discussion Textures obtained from CAMetalDrawables may have this property set to YES, depending on the value of frameBufferOnly passed to their parent CAMetalLayer. Textures created directly by the application will not have any restrictions.
 */
@property (readonly, getter = isFramebufferOnly) BOOL framebufferOnly;

/*!
 @property firstMipmapInTail
 @abstract For sparse textures this property returns index of first mipmap that is packed in tail.
 Mapping this mipmap level will map all subsequent mipmap levels.
 */
@property (readonly) NSUInteger firstMipmapInTail API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @property tailSizeInBytes
 @abstract Amount of memory in bytes required to map sparse texture tail.
 */
@property (readonly) NSUInteger tailSizeInBytes API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

@property (readonly) BOOL isSparse API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @property allowGPUOptimizedContents
 @abstract Allow GPU-optimization for the contents texture. The default value is true.
 @discussion Useful for opting-out of GPU-optimization when implicit optimization (e.g. RT writes) is regressing CPU-read-back performance. See the documentation for optimizeContentsForGPUAccess: and optimizeContentsForCPUAccess: APIs.
 */
@property (readonly) BOOL allowGPUOptimizedContents API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property compressionType
 @abstract Returns the compression type of the texture
 @discussion See the compressionType property on MTLTextureDescriptor
 */
@property (readonly) MTLTextureCompressionType compressionType API_AVAILABLE(macos(12.5), ios(15.0), tvos(16.0));


/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method getBytes:bytesPerRow:bytesPerImage:fromRegion:mipmapLevel:slice:
 @abstract Copies a block of pixels from a texture slice into the application's memory.
 */
- (void)getBytes:(void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow bytesPerImage:(NSUInteger)bytesPerImage fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice;

/*!
 @method replaceRegion:mipmapLevel:slice:withBytes:bytesPerRow:bytesPerImage:
 @abstract Copy a block of pixel data from the caller's pointer into a texture slice.
 */
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice withBytes:(const void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow bytesPerImage:(NSUInteger)bytesPerImage;

/*!
 @method getBytes:bytesPerRow:fromRegion:mipmapLevel:
 @abstract Convenience for getBytes:bytesPerRow:bytesPerImage:fromRegion:mipmapLevel:slice: that doesn't require slice related arguments
 */
- (void)getBytes:(void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level;

/*!
 @method replaceRegion:mipmapLevel:withBytes:bytesPerRow:
 @abstract Convenience for replaceRegion:mipmapLevel:slice:withBytes:bytesPerRow:bytesPerImage: that doesn't require slice related arguments
 */
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level withBytes:(const void *)pixelBytes bytesPerRow:(NSUInteger)bytesPerRow;

/*!
 @method newTextureViewWithPixelFormat:
 @abstract Create a new texture which shares the same storage as the source texture, but with a different (but compatible) pixel format.
 */
- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat;

/*!
 @method newTextureViewWithPixelFormat:textureType:levels:slices:
 @abstract Create a new texture which shares the same storage as the source texture, but with a different (but compatible) pixel format, texture type, levels and slices.
 */
- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat textureType:(MTLTextureType)textureType levels:(NSRange)levelRange slices:(NSRange)sliceRange API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method newSharedTextureHandle
 @abstract Create a new texture handle, that can be shared across process addres space boundaries.
 */
- (nullable MTLSharedTextureHandle *)newSharedTextureHandle API_AVAILABLE(macos(10.14), ios(13.0));

/*!
 @method newTextureViewWithDescriptor:
 @abstract Create a new texture which shares the same storage as the source texture, but with different (but compatible) properties specified by the descriptor
 */
- (nullable id<MTLTexture>)newTextureViewWithDescriptor:(MTLTextureViewDescriptor *)descriptor API_AVAILABLE(macos(26.0), ios(26.0));



/*!
 @property swizzle
 @abstract The channel swizzle used when reading or sampling from this texture
 */
@property (readonly, nonatomic) MTLTextureSwizzleChannels swizzle API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @method newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:
 @abstract Create a new texture which shares the same storage as the source texture, but with a different (but compatible) pixel format, texture type, levels, slices and swizzle. 
 */
- (nullable id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)pixelFormat textureType:(MTLTextureType)textureType levels:(NSRange)levelRange slices:(NSRange)sliceRange swizzle:(MTLTextureSwizzleChannels)swizzle API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @property sparseTextureTier
 @abstract Query support tier for sparse textures.
 */
@property (readonly) MTLTextureSparseTier sparseTextureTier API_AVAILABLE(macos(26.0), ios(26.0));


@end
NS_ASSUME_NONNULL_END
