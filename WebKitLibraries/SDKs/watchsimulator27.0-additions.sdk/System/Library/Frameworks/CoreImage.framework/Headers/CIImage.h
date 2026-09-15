/*
   CoreImage - CIImage.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved.
*/

#ifndef CIIMAGE_H
#define CIIMAGE_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImageDefines.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>

#if TARGET_OS_OSX
#import <IOSurface/IOSurface.h>
#elif COREIMAGE_SUPPORTS_IOSURFACE
#import <IOSurface/IOSurfaceRef.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@class CIContext, CIFilterShape, CIColor, CIFilter;
@class AVDepthData;
@class AVPortraitEffectsMatte;
@class AVSemanticSegmentationMatte;

@protocol MTLTexture;

NS_CLASS_AVAILABLE(10_4, 5_0) NS_SWIFT_SENDABLE
@interface CIImage : NSObject <NSSecureCoding, NSCopying>
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    void *_state;
#endif
	void *_priv;
}

/* Pixel formats. */

typedef int CIFormat NS_TYPED_ENUM;

CORE_IMAGE_EXPORT const CIFormat kCIFormatARGB8 NS_AVAILABLE(10_4, 6_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatBGRA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBX8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatABGR8 NS_AVAILABLE(10_11, 9_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBAh NS_AVAILABLE(10_4, 6_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBA16 NS_AVAILABLE(10_4, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBAf NS_AVAILABLE(10_4, 7_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBX16 NS_AVAILABLE(11_0, 14_2);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBXh NS_AVAILABLE(14_0, 17_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBXf NS_AVAILABLE(14_0, 17_0);

// RGB101010 stored in little-endian 32bit int, 2 MSB are ignored, full-range (0-1024)
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGB10 NS_AVAILABLE(14_0, 17_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatA8 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatA16 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatAh NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatAf NS_AVAILABLE(10_11, 9_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatR8 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatR16 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRh NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRf NS_AVAILABLE(10_11, 9_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatRG8 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRG16 NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGh NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGf NS_AVAILABLE(10_11, 9_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatL8 NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatL16 NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatLh NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatLf NS_AVAILABLE(10_12, 10_0);

CORE_IMAGE_EXPORT const CIFormat kCIFormatLA8 NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatLA16 NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatLAh NS_AVAILABLE(10_12, 10_0);
CORE_IMAGE_EXPORT const CIFormat kCIFormatLAf NS_AVAILABLE(10_12, 10_0);


/* Image options dictionary keys.
   These keys can be passed with appropriate values to the methods:
     + [CIImage imageWith... options:]
     - [CIImage initWith... options:]
   to modify the default behavior of how CIImages are created.
*/

typedef NSString * CIImageOption NS_TYPED_ENUM;

/* A CGColorSpaceRef defining the color space of the image. This value
 * overrides the image's implicit color space.
 * If [NSNull null] then don't color manage the image. */
CORE_IMAGE_EXPORT CIImageOption const kCIImageColorSpace;

/// A Boolean value to control whether an image created with a CVPixelBuffer or an IOSurface
/// should be cropped and offset according clean aperture attachments.
///
/// For a `CVPixelBuffer` this will use `kCVImageBufferPreferredCleanApertureKey` 
/// or `kCVImageBufferCleanApertureKey`.
///
/// If the value for this option is:
/// * True: then image will be cropped and offset to the clean aperture.
/// * False: then the full image is returned.
/// * ``CIVector`` : then use it as a `CGRect` to crop and offset.
/// * Not specified : then it will behave as if False was specified.
///
CORE_IMAGE_EXPORT CIImageOption const kCIImageApplyCleanAperture NS_AVAILABLE(26_0, 26_0); 

/* A boolean value specifying whether an image returned by [CIImage with...] should have kernels
 * applied that will tone map to standard dynamic range (SDR).
 * The option will only have an effect if the image has a CGColorSpace that is high dynamic range (HDR).
 * This option can be useful if further usage of an image is not prepared for HDR values.
 *
 * If the value for this option is @YES, then the HDR input image will be tone mapped to working space SDR.
 * If the value for this option is @NO, then the HDR input image will be linearized to unclamped working space.
 * If this option is not specified, then it will behave as if @NO was specified.
 */
CORE_IMAGE_EXPORT CIImageOption const kCIImageToneMapHDRtoSDR NS_AVAILABLE(11_0, 14_1);

/* A boolean value specifying whether the image should be expanded to HDR if the image content support this.
 * This option is supported by:
 *     imageWithContentsOfURL:options:, initWithContentsOfURL:options,
 *     imageWithData:options:,          initWithData:options:,
 *
 * If this option value is @YES, the image will expanded to a HDR colorspace if supported.
 * If this option not present or the value is @NO, then the image will not be transformed.
 */
CORE_IMAGE_EXPORT CIImageOption const kCIImageExpandToHDR NS_AVAILABLE(14_0, 17_0);

/// A value for overriding the automatic behavior of the Content Headroom property 
/// when creating an image.
///
/// The value for this key should be an `NSNumber` instance.
CORE_IMAGE_EXPORT CIImageOption const kCIImageContentHeadroom NS_AVAILABLE(15_0, 18_0);

/// A value for overriding the automatic behavior of the Content Average Light Level property 
/// when creating an image.
///
/// The value for this key should be an `NSNumber` instance.
CORE_IMAGE_EXPORT CIImageOption const kCIImageContentAverageLightLevel NS_AVAILABLE(26_0, 26_0);


/// A Boolean value specifying how the image should be sampled.
///
/// If the value for this option is:
/// * True: The image will be sampled using nearest neighbor sampling. 
/// * False: The image will be sampled using bilinear interpolation. 
/// * Not specified: The default behavior is False. 
/// 
CORE_IMAGE_EXPORT CIImageOption const kCIImageNearestSampling NS_AVAILABLE(10_13, 11_0);


/// A Boolean value specifying when the image should be decoded.
///
/// If the value for this option is:
/// * True: The image will be decoded into a non-volatile cache at initialization time if possible. 
/// * False: The image will be decoded into a volatile cache at render time. 
/// * Not specified: The default behavior is True or False depening on the image size and available memeory.
/// 
/// This option is only supported by these APIs:
///  * ``/CIImage/imageWithContentsOfURL:options:`` 
///  * ``/CIImage/initWithContentsOfURL:options:``
///  * ``/CIImage/imageWithData:options:``
///  * ``/CIImage/initWithData:options:``
///  * ``/CIImage/imageWithCGImageSource:index:options:``
///  * ``/CIImage/initWithCGImageSource:index:options:``
///  
CORE_IMAGE_EXPORT CIImageOption const kCIImageCacheImmediately;


/* A NSDictionary of metadata properties to pass to CIImage initialization methods.
 * If this option is not specified, the properties will be set to CGImageSourceCopyPropertiesAtIndex.
 * If this option is [NSNull null], the properties will be set to nil.
 */
CORE_IMAGE_EXPORT CIImageOption const kCIImageProperties NS_AVAILABLE(10_8, 5_0);


/* A boolean value specifying that the image should transformed according to orientation metadata.
 * This option is supported by:
 *     imageWithContentsOfURL:options:, initWithContentsOfURL:options,
 *     imageWithData:options:,          initWithData:options:,
 *     imageWithCGImageSource:options:, initWithCGImageSource:options:
 * when the image data contains orientation metadata or by any initialization method
 * if the kCIImageProperties option is also provided.
 *
 * If this option value is @YES, the image will transformed according to the orientation metadata
 * and the orientation metadata will be be removed.
 * If this option value is @NO, then the image will not be transformed
 * and the orientation metadata will left unaltered.
 * If this option is not specified, then it will behave as if @NO was specified.
 */
CORE_IMAGE_EXPORT CIImageOption const kCIImageApplyOrientationProperty NS_AVAILABLE(10_13, 11_0);

CORE_IMAGE_EXPORT CIImageOption const kCIImageTextureTarget CI_GL_DEPRECATED_MAC(10_9,10_14);
CORE_IMAGE_EXPORT CIImageOption const kCIImageTextureFormat CI_GL_DEPRECATED_MAC(10_9,10_14);


/// The factor by which to scale down a returned images.
///
/// The value of this key should be an `NSNumber` containing the integer value 2, 4, or 8. 
/// It can be used to improve performance and reduce memory usage when working with large images.
/// 
/// When you specify this key, the retured image will be scaled down the image data by the specified numerical factor. 
/// If the image format doesn’t support the specified scale factor, a larger or full-size normal image is returned.
/// 
/// This option is only supported by JPEG, HEIF, TIFF, PNG and RAW images formats.
///
/// This option is only supported by these APIs:
///  * ``/CIImage/imageWithContentsOfURL:options:`` 
///  * ``/CIImage/initWithContentsOfURL:options:``
///  * ``/CIImage/imageWithData:options:``
///  * ``/CIImage/initWithData:options:``
///  * ``/CIImage/imageWithCGImageSource:index:options:``
///  * ``/CIImage/initWithCGImageSource:index:options:``
///  
/// > Note: the `kCGImageSourceSubsampleFactor` key can also be used for this purpose.
/// 
CORE_IMAGE_EXPORT CIImageOption const kCIImageSubsampleFactor
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos);


/// The uniform type identifier string to use in cases where a file's format cannot
/// be conclusively determined based solely on its contents.
///
/// The value of this key should be an `NSString` containing a hint. 
/// It is most commonly needed for some RAW file formats which can also be  
/// interpreted as TIFF files.
/// 
/// This option is only supported by these APIs:
///  * ``/CIImage/imageWithContentsOfURL:options:`` 
///  * ``/CIImage/initWithContentsOfURL:options:``
///  * ``/CIImage/imageWithData:options:``
///  * ``/CIImage/initWithData:options:``
///  
/// > Note: the key `kCGImageSourceTypeIdentifierHint` key can also be used for this purpose.
///
CORE_IMAGE_EXPORT CIImageOption const kCIImageTypeIdentifierHint
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos);

/// A Boolean value specifying that using hardware is preferred when decoding.
///
/// If the value for this option is:
/// * True: The image will be decoded using dedicated hardware if possible. 
/// * False: The image will be decoded using the CPU is possible. 
/// * Not specified: The default behavior is True.
/// 
/// This option is only supported by JPEG and HEIF images formats.
/// 
/// This option is only supported by these APIs:
///  * ``/CIImage/imageWithContentsOfURL:options:`` 
///  * ``/CIImage/initWithContentsOfURL:options:``
///  * ``/CIImage/imageWithData:options:``
///  * ``/CIImage/initWithData:options:``
///  * ``/CIImage/imageWithCGImageSource:index:options:``
///  * ``/CIImage/initWithCGImageSource:index:options:``
///  
/// > Note: the `kCGImageSourceUseHardwareAcceleration` key can also be used for this purpose.
///  
CORE_IMAGE_EXPORT CIImageOption const kCIImageUseHardwareAcceleration
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos);

/* The kCIImageAuxiliary keys specify that an auxiliary image be returned instead of the primary image.
 * These options are supported by:
 *     imageWithContentsOfURL:options:, initWithContentsOfURL:options,
 *     imageWithData:options:,          initWithData:options:,
 *     imageWithCGImageSource:options:, initWithCGImageSource:options:
 *
 * If the value of any of these keys is @YES, the auxiliary image be returned if present.
 * The returned image will be a monochrome image.
 *
 * The kCIImageAuxiliaryHDRGainMap option will return as a CIImage the auxiliary data returned
 * by either kCGImageAuxiliaryDataTypeHDRGainMap or kCGImageAuxiliaryDataTypeISOGainMap.
 * If the file contains both gain maps, then the kCGImageAuxiliaryDataTypeISOGainMap data is returned.
 */
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliaryDepth NS_AVAILABLE(10_13, 11_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliaryDisparity NS_AVAILABLE(10_13, 11_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliaryPortraitEffectsMatte NS_AVAILABLE(10_14, 12_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliarySemanticSegmentationSkinMatte NS_AVAILABLE(10_15, 13_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliarySemanticSegmentationHairMatte NS_AVAILABLE(10_15, 13_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliarySemanticSegmentationTeethMatte NS_AVAILABLE(10_15, 13_0);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliarySemanticSegmentationGlassesMatte NS_AVAILABLE(11_0, 14_1);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliarySemanticSegmentationSkyMatte NS_AVAILABLE(11_1, 14_3);
CORE_IMAGE_EXPORT CIImageOption const kCIImageAuxiliaryHDRGainMap NS_AVAILABLE(11_0, 14_1);


/* Creates a new image from the contents of 'image'. */
+ (CIImage *)imageWithCGImage:(CGImageRef)image;
+ (CIImage *)imageWithCGImage:(CGImageRef)image
                      options:(nullable NSDictionary<CIImageOption, id> *)options;

/* Creates a new image from the contents of 'source'. */
+ (CIImage *)imageWithCGImageSource:(CGImageSourceRef)source
                              index:(size_t)index
                            options:(nullable NSDictionary<CIImageOption, id> *)dict NS_AVAILABLE(10_15, 13_0);

/* Creates a new image from the contents of 'layer'. */
+ (CIImage *)imageWithCGLayer:(CGLayerRef)layer NS_DEPRECATED_MAC(10_4,10_11);
+ (CIImage *)imageWithCGLayer:(CGLayerRef)layer
                      options:(nullable NSDictionary<CIImageOption, id> *)options NS_DEPRECATED_MAC(10_4,10_11);

/* Creates a new image whose bitmap data is from 'data'. Each row contains 'bytesPerRow'
 * bytes. The dimensions of the image are defined by 'size'. 'format' defines
 * the format and size of each pixel. 'colorSpace' defines the color space
 * that the image is defined in, if nil, the image is not color matched. */
+ (CIImage *)imageWithBitmapData:(NSData *)data
                     bytesPerRow:(size_t)bytesPerRow
                            size:(CGSize)size
                          format:(CIFormat)format
                      colorSpace:(nullable CGColorSpaceRef)colorSpace;

/* Creates a new image referencing the contents of the GL texture object
 * with identifier 'name'. The texture should have dimensions as defined
 * by 'size'. If 'flipped' is true, then the contents of the texture are
 * flipped vertically when referenced. 'colorSpace' defines the color space
 * that the image is defined in, if nil, the texture is not color matched.*/
+ (CIImage *)imageWithTexture:(unsigned int)name
                         size:(CGSize)size
                      flipped:(BOOL)flipped
                   colorSpace:(nullable CGColorSpaceRef)colorSpace CI_GL_DEPRECATED(10_4,10_14, 6_0,12_0);

/* In the options dictionary you can specify the following:
 * - kCIImageColorSpace which should be a CGColorSpaceRef or [NSNull null]
 * - kCIImageTextureTarget which should be a NSNumber with either GL_TEXTURE_2D or GL_TEXTURE_RECTANGLE_ARB.
 * - kCIImageTextureFormat which should be a NSNumber with a CIFormat value
 */
+ (CIImage *)imageWithTexture:(unsigned int)name
                         size:(CGSize)size
                      flipped:(BOOL)flipped
                      options:(nullable NSDictionary<CIImageOption, id> *)options CI_GL_DEPRECATED_MAC(10_9,10_14);

/* Creates a new image referencing the contents of the Metal texture object.
 * The texture type must be MTLTextureType2D and the texture format must be unsigned normalized or floating-point.
 * When rendering a CIImage referencing this Metal texture, there should not be any uncommitted Metal comand buffers writing to the texture. */
+ (nullable CIImage *)imageWithMTLTexture:(id<MTLTexture>)texture
                                  options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_11, 9_0);

+ (nullable CIImage *)imageWithContentsOfURL:(NSURL *)url;
+ (nullable CIImage *)imageWithContentsOfURL:(NSURL *)url
                                     options:(nullable NSDictionary<CIImageOption, id> *)options;

+ (nullable CIImage *)imageWithData:(NSData *)data;
+ (nullable CIImage *)imageWithData:(NSData *)data
                            options:(nullable NSDictionary<CIImageOption, id> *)options;

/* Creates a new image whose data is from the contents of a CVImageBuffer. */
+ (CIImage *)imageWithCVImageBuffer:(CVImageBufferRef)imageBuffer NS_AVAILABLE(10_4, 9_0);
+ (CIImage *)imageWithCVImageBuffer:(CVImageBufferRef)imageBuffer
                            options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_4, 9_0);

/* Creates a new image whose data is from the contents of a CVPixelBufferRef. */
+ (CIImage *)imageWithCVPixelBuffer:(CVPixelBufferRef)pixelBuffer NS_AVAILABLE(10_11, 5_0);
+ (CIImage *)imageWithCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                            options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_11, 5_0);

#if COREIMAGE_SUPPORTS_IOSURFACE
/* Creates a new image from the contents of an IOSurface. */
+ (CIImage *)imageWithIOSurface:(IOSurfaceRef)surface NS_AVAILABLE(10_6, 5_0);
+ (CIImage *)imageWithIOSurface:(IOSurfaceRef)surface
                        options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_6, 5_0);
#endif

/* Return or initialize a new image with an infinite amount of the color
 * 'color'. */
+ (CIImage *)imageWithColor:(CIColor *)color;

/* Create an empty Image. */
+ (CIImage *)emptyImage;

/* Convenience constant color CIImages in the sRGB colorspace. */
@property (class, strong, readonly) CIImage *blackImage   NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *whiteImage   NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *grayImage    NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *redImage     NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *greenImage   NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *blueImage    NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *cyanImage    NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *magentaImage NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *yellowImage  NS_AVAILABLE(10_15, 13_0);
@property (class, strong, readonly) CIImage *clearImage   NS_AVAILABLE(10_15, 13_0);

/* Initializers. */

- (instancetype)initWithCGImage:(CGImageRef)image;
- (instancetype)initWithCGImage:(CGImageRef)image
                        options:(nullable NSDictionary<CIImageOption, id> *)options;

- (instancetype) initWithCGImageSource:(CGImageSourceRef)source
                                 index:(size_t)index
                               options:(nullable NSDictionary<CIImageOption, id> *)dict NS_AVAILABLE(10_15, 13_0);

- (instancetype)initWithCGLayer:(CGLayerRef)layer
    NS_DEPRECATED_MAC(10_4,10_11,"Use initWithCGImage: instead.");
- (instancetype)initWithCGLayer:(CGLayerRef)layer
                        options:(nullable NSDictionary<CIImageOption, id> *)options
    NS_DEPRECATED_MAC(10_4,10_11,"Use initWithCGImage:options instead.");

- (nullable instancetype)initWithData:(NSData *)data;
- (nullable instancetype)initWithData:(NSData *)data
                              options:(nullable NSDictionary<CIImageOption, id> *)options;

- (instancetype)initWithBitmapData:(NSData *)data
                       bytesPerRow:(size_t)bytesPerRow
                              size:(CGSize)size
                            format:(CIFormat)format
                        colorSpace:(nullable CGColorSpaceRef)colorSpace;

- (instancetype)initWithTexture:(unsigned int)name
                           size:(CGSize)size
                        flipped:(BOOL)flipped
                     colorSpace:(nullable CGColorSpaceRef)colorSpace CI_GL_DEPRECATED(10_4,10_14, 6_0,12_0);

- (instancetype)initWithTexture:(unsigned int)name
                           size:(CGSize)size
                        flipped:(BOOL)flipped
                        options:(nullable NSDictionary<CIImageOption, id> *)options CI_GL_DEPRECATED_MAC(10_9,10_14);

// initWithMTLTexture will return nil if textureType is not MTLTextureType2D.
- (nullable instancetype)initWithMTLTexture:(id<MTLTexture>)texture
                                    options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_11, 9_0);

- (nullable instancetype)initWithContentsOfURL:(NSURL *)url;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url
                                       options:(nullable NSDictionary<CIImageOption, id> *)options;

#if COREIMAGE_SUPPORTS_IOSURFACE
- (instancetype)initWithIOSurface:(IOSurfaceRef)surface NS_AVAILABLE(10_6, 5_0);

- (instancetype)initWithIOSurface:(IOSurfaceRef)surface
                          options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_6, 5_0);
#endif

#if TARGET_OS_OSX
- (instancetype)initWithIOSurface:(IOSurfaceRef)surface
                            plane:(size_t)plane
                           format:(CIFormat)format
                          options:(nullable NSDictionary<CIImageOption, id> *)options NS_DEPRECATED_MAC(10_9,10_11);
#endif

- (instancetype)initWithCVImageBuffer:(CVImageBufferRef)imageBuffer NS_AVAILABLE(10_4, 9_0);
- (instancetype)initWithCVImageBuffer:(CVImageBufferRef)imageBuffer
                              options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_4, 9_0);

- (instancetype)initWithCVPixelBuffer:(CVPixelBufferRef)pixelBuffer NS_AVAILABLE(10_11, 5_0);
- (instancetype)initWithCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                              options:(nullable NSDictionary<CIImageOption, id> *)options NS_AVAILABLE(10_11, 5_0);

- (instancetype)initWithColor:(CIColor *)color;

/* Returns a new image representing the original image with the transform
 * 'matrix' appended to it. */
- (CIImage *)imageByApplyingTransform:(CGAffineTransform)matrix;

// specifying true or false here will override the context's kCIContextHighQualityDownsample setting.
- (CIImage *)imageByApplyingTransform:(CGAffineTransform)matrix
                highQualityDownsample:(BOOL)highQualityDownsample NS_AVAILABLE(10_12, 10_0);

/* Returns a new image representing the original image with a transform applied to it based on an orientation value.
 * CGImagePropertyOrientation enum values from 1 to 8 as defined in the TIFF spec are supported.
 * Returns original image if the image is of infinite extent. */
- (CIImage *)imageByApplyingOrientation:(int)orientation NS_AVAILABLE(10_10, 8_0);

/* Returns a CGAffineTransform for an orientation value that can be applied to an image.
 * CGImagePropertyOrientation enum values from 1 to 8 as defined in the TIFF spec are supported.
 * Returns CGAffineTransformIdentity if the image is of infinite extent.*/
- (CGAffineTransform)imageTransformForOrientation:(int)orientation NS_AVAILABLE(10_10, 8_0);

/* Returns a new image representing the original image transformeded for the given CGImagePropertyOrientation */
- (CIImage *)imageByApplyingCGOrientation:(CGImagePropertyOrientation)orientation NS_AVAILABLE(10_13, 11_0);

/* Returns a CGAffineTransform for the CGImagePropertyOrientation value that can be applied to an image.*/
- (CGAffineTransform)imageTransformForCGOrientation:(CGImagePropertyOrientation)orientation NS_AVAILABLE(10_13, 11_0);

/* Return a new image formed by compositing the receiver image over 'dest'.
 * This is equivalent to the CISourceOverCompositing filter. */
- (CIImage *)imageByCompositingOverImage:(CIImage *)dest NS_AVAILABLE(10_4, 8_0);

/* Return a new image cropped to a rectangle. */
- (CIImage *)imageByCroppingToRect:(CGRect)rect;

/* Return a new infinite image by replicating the edge pixels of the receiver image. */
- (CIImage *)imageByClampingToExtent NS_AVAILABLE(10_10, 8_0);

/* Return a new infinite image by replicating the edge pixels of a rectangle.
 * This is equivalent to the CICrop filter. */
- (CIImage *)imageByClampingToRect:(CGRect)rect NS_AVAILABLE(10_12, 10_0);

/* A convenience method for applying a filter to an image.
 * The method returns outputImage of the filter after setting the
 * filter's inputImage to the method receiver and other parameters
 * from the key/value pairs of 'params'. */
- (CIImage *)imageByApplyingFilter:(NSString *)filterName
               withInputParameters:(nullable NSDictionary<NSString *,id> *)params NS_AVAILABLE(10_10, 8_0);

/* A convenience method for applying a filter to an image.
 * The method returns outputImage of the filter after setting the
 * filter's inputImage to the method receiver and any other parameters
 * from the filter's defaults. */
- (CIImage *)imageByApplyingFilter:(NSString *)filterName NS_AVAILABLE(10_13, 11_0);


/* Return a new image by color matching from the colorSpace to the context's working space.
 * This method will return nil if the CGColorSpace is not kCGColorSpaceModelRGB or Monochrome. */
- (nullable CIImage *)imageByColorMatchingColorSpaceToWorkingSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);

/* Return a new image by color matching from the context's working space to the colorSpace.
 * This method will return nil if the CGColorSpace is not kCGColorSpaceModelRGB or Monochrome. */
- (nullable CIImage *)imageByColorMatchingWorkingSpaceToColorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);

/* Return a new image by multiplying the receiver's RGB values by its alpha. */
- (CIImage *)imageByPremultiplyingAlpha NS_AVAILABLE(10_12, 10_0);

/* Return a new image by dividing the receiver's RGB values by its alpha. */
- (CIImage *)imageByUnpremultiplyingAlpha NS_AVAILABLE(10_12, 10_0);

/* Return a new image with alpha set to 1 within the rectangle and 0 outside. */
- (CIImage *)imageBySettingAlphaOneInExtent:(CGRect)extent NS_AVAILABLE(10_12, 10_0);

/// Create an image by applying a gaussian blur to the receiver.
/// - Parameters:
///    - sigma: The sigma of the gaussian blur to apply to the receiver.
///             If the sigma is very small (less than `0.16`) then the receiver is returned.
/// - Returns: 
///    An autoreleased ``CIImage`` instance or the received image.
- (CIImage *)imageByApplyingGaussianBlurWithSigma:(double)sigma NS_AVAILABLE(10_12, 10_0);

/// Return a new image by changing the receiver's metadata properties.
/// 
/// When you create an image, Core Image sets an image’s properties to a metadata 
/// dictionary as described here: ``properties``.
/// Use this method to override an image’s metadata properties with new values.
/// 
/// - Parameters:
///    - properties: A dictionary of metadata properties akin to the `CGImageSourceCopyPropertiesAtIndex()` function.
/// - Returns: 
///    An autoreleased ``CIImage`` instance with a copy of the new properties.
- (CIImage *)imageBySettingProperties:(NSDictionary*)properties NS_AVAILABLE(10_12, 10_0);

/// Create an image by changing the receiver's sample mode to bilinear interpolation.
/// - Returns: 
///    An autoreleased ``CIImage`` instance with a bilinear sampling.
- (CIImage *)imageBySamplingLinear NS_AVAILABLE(10_13, 11_0);

/// Create an image by changing the receiver's sample mode to nearest neighbor.
/// - Returns: 
///    An autoreleased ``CIImage`` instance with a nearest sampling.
- (CIImage *)imageBySamplingNearest NS_AVAILABLE(10_13, 11_0);

/// Create an image that inserts a intermediate that is cacheable
/// 
/// This intermediate will be not be cached if ``kCIContextCacheIntermediates`` is false.
/// - Returns: 
///    An autoreleased ``CIImage``.
- (CIImage *)imageByInsertingIntermediate NS_AVAILABLE(10_14, 12_0);

/// Create an image that inserts a intermediate that is cacheable.
/// 
/// - Parameters:
///    - cache: Controls if Core Image caches the returned image.
///           * `YES` : This intermediate will be cacheable even if 
///              ``kCIContextCacheIntermediates`` is false. 
///           * `NO`  : the intermediate will be not be cached if 
///              ``kCIContextCacheIntermediates`` is false.
/// - Returns: 
///    An autoreleased ``CIImage``.
- (CIImage *)imageByInsertingIntermediate:(BOOL)cache NS_AVAILABLE(10_14, 12_0);

/// Create an image that inserts a intermediate that is cached in tiles
/// 
/// This intermediate will be cacheable even if ``kCIContextCacheIntermediates`` is false.
/// - Returns: 
///    An autoreleased ``CIImage``.
- (CIImage *)imageByInsertingTiledIntermediate NS_AVAILABLE(26_0, 26_0);

/// Create an image that applies a gain map Core Image image to the received Core Image image.
/// 
/// The gain map image can be obtained by creating a ``CIImage`` instance from `NSURL`/`NSData` 
/// and setting the ``kCIImageAuxiliaryHDRGainMap`` option set to `@YES`. 
/// 
/// If the gain map ``CIImage`` instance doesn't have the needed ``properties`` metadata, 
/// the received image will be returned as-is.
/// 
/// - Returns: 
///    An autoreleased ``CIImage`` instance or the received image.
- (CIImage*) imageByApplyingGainMap:(CIImage*)gainmap NS_AVAILABLE(15_0, 18_0);

/// Create an image that applies a gain map Core Image image with a specified headroom to the received Core Image image.
/// 
/// - Parameters:
///    - gainmap: The gain map ``CIImage`` instance to apply to the receiver.
///    - headroom: a float value that specify how much headroom the resulting image should have.
///                The headroom value will be limited to between 1.0 (i.e. SDR) and 
///                the full headroom allowed by the gain map.
/// - Returns: 
///    An autoreleased ``CIImage`` instance or the received image.
- (CIImage*) imageByApplyingGainMap:(CIImage*)gainmap headroom:(float)headroom NS_AVAILABLE(15_0, 18_0);

/// Create an image by changing the receiver's contentHeadroom property. 
/// 
/// Changing this value will alter the behavior of the `CIToneMapHeadroom` and `CISystemToneMap` filters.
/// * If the value is set to 0.0 then the returned image's headroom is unknown.
/// * If the value is set to 1.0 then the returned image is SDR.
/// * If the value is set to greater 1.0 then the returned image is HDR.
/// * Otherwise the returned image's headroom is unknown.
///
/// - Returns: 
///    An autoreleased ``CIImage``.
- (CIImage*) imageBySettingContentHeadroom:(float)headroom NS_AVAILABLE(26_0, 26_0);

/// Create an image by changing the receiver's contentAverageLightLevel property. 
/// 
/// Changing this value will alter the behavior of the `CIToneMapHeadroom` and `CISystemToneMap` filters.
/// * If the value is set to 0.0 or less then the returned image's ``contentAverageLightLevel`` is unknown.
///
/// - Returns: 
///    An autoreleased ``CIImage``.
- (CIImage*) imageBySettingContentAverageLightLevel:(float)average NS_AVAILABLE(26_0, 26_0);

/// Returns a rectangle the defines the bounds of non-(0,0,0,0) pixels in the image.
/// > Note: the ``extent`` of `CIImage`` may be infinite or have a non-zero origin. 
@property (NS_NONATOMIC_IOSONLY, readonly) CGRect extent;

/// Returns YES if the image is known to have and alpha value of `1.0` over the entire image extent.
@property (nonatomic, readonly, getter=isOpaque) BOOL opaque;

/// Returns the metadata properties dictionary of the image. 
/// 
/// If the ``CIImage`` was created from `NSURL` or `NSData` then this dictionary is determined by calling `CGImageSourceCopyPropertiesAtIndex()`.
/// 
/// If the ``CIImage`` was created with the ``kCIImageProperties`` option, then that dictionary is returned.
/// 
/// If the ``CIImage`` was created by applying ``CIFilter-class`` or ``CIKernel`` then the 
/// properties of the root inputImage will be returned.
/// 
@property (atomic, readonly) NSDictionary<NSString *,id> *properties NS_AVAILABLE(10_8, 5_0);

/* Return the Domain of Definition of the image. */
@property (atomic, readonly) CIFilterShape *definition NS_AVAILABLE_MAC(10_4);

/* Returns the URL of the image when the image was created using the imageWithContentsOfURL APIs.
 * This method will return nil, if the URL cannot be determined. */
@property (atomic, readonly, nullable) NSURL *url NS_AVAILABLE(10_4, 9_0);

/* Returns the color space of the image.
 * If this returns nil, the image should be assumed to be in the Core Image working colorspace.
 *
 * This method will return nil if image is the result of applying a CIFilter or CIKernel.
 * There are exceptions to this.  Applying CIWarpKernels or certain CIFilters (e.g. CIGaussianBlur,
 * CILanczosScaleTransform, CIAreaAverage and some others) to an image will result in a CIImage with
 * the same 'colorspace' property value.
 */
@property (atomic, readonly, nullable) CGColorSpaceRef colorSpace NS_AVAILABLE(10_4, 9_0) CF_RETURNS_NOT_RETAINED;

/// Returns the content headroom of the image.
///
/// If the image headroom is unknown, then the value 0.0 will be returned.
///
/// If the image headroom is known, then a value greater than or equal to 1.0 will be returned.
/// A value of 1.0 will be returned if the image is SDR.
/// A value greater than 1.0 will be returned if the image is HDR.
///
/// The image headroom may known when a CIImage is first initialized.
/// If the a CIImage is initialized using:
/// * `NSURL` or `NSData` : the headroom may be determined by associated metadata 
///                         or deduced from pixel format or colorSpace information.
/// * `CGImage` : headroom may be determined by `CGImageGetHeadroomInfo()` 
///               or deduced from pixel format or colorSpace information.
/// * `IOSurface` : then the headroom will be determined by `kIOSurfaceContentHeadroom`.
///               or deduced from pixel format or colorSpace information.
/// * `CVPixelBuffer` : then the headroom will be determined by `kCVImageBufferContentLightLevelInfoKey`.
///               or deduced from pixel format or colorSpace information.
/// * `BitmapData` : headroom may be deduced from pixel format or colorSpace information.
///
/// If the image is the result of applying a ``CIFilter-class`` or ``CIKernel``, this method will return `0.0`.
/// 
/// There are exceptions to this.  Applying a `CIWarpKernel`` or certain ``CIFilter-class`` 
/// (e.g. `CIGaussianBlur`, `CILanczosScaleTransform`, `CIAreaAverage` and some others) 
/// to an image will result in a ``CIImage`` instance with the same `contentHeadroom` property value.
///
@property (nonatomic, readonly) float contentHeadroom NS_AVAILABLE(15_0, 18_0);

/// Returns the content average light level of the image.
///
/// If the image average light level is unknown, then the value 0.0 will be returned.
///
/// If the image headroom is known, then a value greater than or equal to 0.0 will be returned.
///
/// The image average light level may known when a CIImage is first initialized.
/// If the a CIImage is initialized with a:
///  * `CGImage` : then the headroom will be determined by `CGImageGetContentAverageLightLevel()`.
///  * `CVPixelBuffer` : then the headroom will be determined by `kCVImageBufferContentLightLevelInfoKey`.
///  
/// If the image is the result of applying a ``CIFilter-class`` or ``CIKernel``, this property will return `0.0`.
/// 
/// There are exceptions to this.  Applying a ``CIWarpKernel`` or certain ``CIFilter-class`` 
/// (e.g. `CIGaussianBlur`, `CILanczosScaleTransform`, `CIAreaAverage` and some others) 
/// to an image will result in a ``CIImage`` instance with the same `contentAverageLightLevel` property value.
///
@property (nonatomic, readonly) float contentAverageLightLevel NS_AVAILABLE(26_0, 26_0);

/* Returns a CVPixelBufferRef if the CIImage was created with [CIImage imageWithCVPixelBuffer] and no options.
 * Otherwise this property will be nil and calling [CIContext render:toCVPixelBuffer:] is recommended.
 * Modifying the contents of this pixelBuffer will cause the CIImage to render with undefined results. */
@property (nonatomic, readonly, nullable) CVPixelBufferRef pixelBuffer NS_AVAILABLE(10_12, 10_0);

/* Returns a CGImageRef if the CIImage was created with [CIImage imageWithCGImage] or [CIImage imageWithContentsOfURL] and no options.
 * Otherwise this property will be nil and calling [CIContext createCGImage:fromRect:] is recommended. */
@property (nonatomic, readonly, nullable) CGImageRef CGImage NS_AVAILABLE(10_12,10_0);

/// Returns a Metal Texture if the Core Image image was created with a texture.
/// 
/// This will return non-nil if the image was created with ``/CIImage/imageWithMTLTexture:options:`` and no options.
/// Otherwise this property will be `nil` you should instead call
///  ``/CIContext/render:toMTLTexture:commandBuffer:bounds:colorSpace:``.
/// > Warning: Modifying the contents of this texture will cause the ``CIImage`` instance to render with incorrect results.
/// 
@property (nonatomic, readonly, nullable) id<MTLTexture> metalTexture NS_AVAILABLE(15_0, 18_0);

/* Returns the rectangle of 'image' that is required to render the
 * rectangle 'rect' of the receiver.  This may return a null rect. */
- (CGRect)regionOfInterestForImage:(CIImage *)image
                            inRect:(CGRect)rect NS_AVAILABLE(10_11, 6_0);

@end


@interface CIImage (AutoAdjustment)

/* Image auto adjustment keys. */

typedef NSString * CIImageAutoAdjustmentOption NS_TYPED_ENUM;

/* These are the options dictionary keys which can be specified when calling
 * the autoAdjustmentFiltersWithOptions: method.
 */

/* If CFBoolean value is false then dont attempt to apply enhancement filters.
 * If not specified, the option is assumed to be present and true.
 */
CORE_IMAGE_EXPORT CIImageAutoAdjustmentOption const kCIImageAutoAdjustEnhance NS_AVAILABLE(10_8, 5_0);

/* If CFBoolean value is false then dont attempt to apply red eye filter.
 * If not specified, the option is assumed to be present and true.
 */
CORE_IMAGE_EXPORT CIImageAutoAdjustmentOption const kCIImageAutoAdjustRedEye NS_AVAILABLE(10_8, 5_0);

/* If value is an array of detected CIFeatures, then use these features
 * to determine the AutoAdjustEnhance and or AutoAdjustRedEye filters.
 * If not specified, receiver will call CIDetector.
 */
CORE_IMAGE_EXPORT CIImageAutoAdjustmentOption const kCIImageAutoAdjustFeatures NS_AVAILABLE(10_8, 5_0);

/* If CFBoolean value is false then don't attempt to apply crop filter.
 * If not specified, the option is assumed to be present and false.
 */
CORE_IMAGE_EXPORT CIImageAutoAdjustmentOption const kCIImageAutoAdjustCrop NS_AVAILABLE(10_10, 8_0);

/* If CFBoolean value is false then don't attempt to apply auto-level.
 * If not specified, the option is assumed to be present and false.
 */
CORE_IMAGE_EXPORT CIImageAutoAdjustmentOption const kCIImageAutoAdjustLevel NS_AVAILABLE(10_10, 8_0);


/* Return an array of filters to apply to an image to improve its
 * skin tones, saturation, contrast, shadows and repair red-eyes or LED-eyes.
 *
 * The options dictionary can contain a CIDetectorImageOrientation key value.
 * The value for this key is an integer NSNumber from 1..8 such as that
 * found in kCGImagePropertyOrientation.  If present, the adjustment will be done
 * based on that orientation but any coordinates in the returned filters will
 * still be based on those of the sender image.
 */
- (NSArray<CIFilter *> *)autoAdjustmentFilters NS_AVAILABLE(10_8, 5_0);
- (NSArray<CIFilter *> *)autoAdjustmentFiltersWithOptions:(nullable NSDictionary<CIImageAutoAdjustmentOption, id> *)options
    NS_AVAILABLE(10_8, 5_0);

@end

@interface CIImage (LabConversion)

/* Converts the receiver from the Core Image RGB working space to La*b* space.
 * The L channel in the range of 0...100 is stored in the the R channel of the resulting CIImage.
 * The a*b* channels in the range of -128..128 are stored in the GB channels of the resulting CIImage.
 * The A channel of the resulting CIImage is the same as the receiver's */
-(CIImage*) imageByConvertingWorkingSpaceToLab NS_AVAILABLE(13_0,16_0);

/* Converts an image from La*b* to the Core Image RGB working space.
 * This is the inverse of imageByConvertingWorkingSpaceToLab. */
-(CIImage*) imageByConvertingLabToWorkingSpace NS_AVAILABLE(13_0,16_0);

@end

@interface CIImage (AVDepthData)

/* Returns a AVDepthData if the CIImage was created with [CIImage imageWithData] or [CIImage imageWithContentsOfURL] and.
 * one the options kCIImageAuxiliaryDepth or kCIImageAuxiliaryDisparity. */
@property (nonatomic, readonly, nullable) AVDepthData *depthData NS_AVAILABLE(10_13,11_0);

-(nullable instancetype) initWithDepthData:(AVDepthData *)data
                                   options:(nullable NSDictionary<NSString *,id> *)options NS_AVAILABLE(10_13, 11_0);

-(nullable instancetype)initWithDepthData:(AVDepthData *)data NS_AVAILABLE(10_13, 11_0);

+(nullable instancetype)imageWithDepthData:(AVDepthData *)data
                                   options:(nullable NSDictionary<NSString *,id> *)options NS_AVAILABLE(10_13, 11_0);

+(nullable instancetype)imageWithDepthData:(AVDepthData *)data NS_AVAILABLE(10_13, 11_0);

@end


@interface CIImage (AVPortraitEffectsMatte)

/* Returns a AVPortraitEffectsMatte if the CIImage was created with [CIImage imageWithData] or [CIImage imageWithContentsOfURL] and.
 * one the options kCIImageAuxiliaryPortraitEffectsMatte. */
@property (nonatomic, readonly, nullable) AVPortraitEffectsMatte *portraitEffectsMatte NS_AVAILABLE(10_14,12_0);

-(nullable instancetype) initWithPortaitEffectsMatte:(AVPortraitEffectsMatte *)matte
                                             options:(nullable NSDictionary<CIImageOption,id> *)options NS_AVAILABLE(10_14, 12_0);

-(nullable instancetype)initWithPortaitEffectsMatte:(AVPortraitEffectsMatte *)matte NS_AVAILABLE(10_13, 11_0);

+(nullable instancetype)imageWithPortaitEffectsMatte:(AVPortraitEffectsMatte *)matte
                                            options:(nullable NSDictionary<CIImageOption,id> *)options NS_AVAILABLE(10_14, 12_0);

+(nullable instancetype)imageWithPortaitEffectsMatte:(AVPortraitEffectsMatte *)matte NS_AVAILABLE(10_14, 12_0);

@end

@interface CIImage (AVSemanticSegmentationMatte)

/* Returns a AVSemanticSegmentationMatte if the CIImage was created with [CIImage imageWithData] or [CIImage imageWithContentsOfURL] and.
 * one the options like kCIImageAuxiliarySemanticSegmentationSkinMatte. */
@property (nonatomic, readonly, nullable) AVSemanticSegmentationMatte *semanticSegmentationMatte NS_AVAILABLE(10_15, 13_0);

-(nullable instancetype)initWithSemanticSegmentationMatte:(AVSemanticSegmentationMatte *)matte
                                                  options:(nullable NSDictionary<CIImageOption,id> *)options NS_AVAILABLE(10_15, 13_0);

-(nullable instancetype)initWithSemanticSegmentationMatte:(AVSemanticSegmentationMatte *)matte NS_AVAILABLE(10_15, 13_0);

+(nullable instancetype)imageWithSemanticSegmentationMatte:(AVSemanticSegmentationMatte *)matte
                                                   options:(nullable NSDictionary<CIImageOption,id> *)options NS_AVAILABLE(10_15, 13_0);

+(nullable instancetype)imageWithSemanticSegmentationMatte:(AVSemanticSegmentationMatte *)matte NS_AVAILABLE(10_15, 13_0);


@end


NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIIMAGE_H */
