/*
   CoreImage - CIImageProvider.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved.
*/

#ifndef CIIMAGEPROVIDER_H
#define CIIMAGEPROVIDER_H

#ifdef __OBJC__

#import <CoreImage/CIImage.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLTexture, MTLCommandBuffer;

@interface CIImage (CIImageProvider)

/// Create an image object based on pixels from an image provider object.
///
/// Core Image retains the provider object until the image is deallocated.
/// The image provider object will not be called until the image is rendered.
/// 
/// - Parameters:
///    - provider: An object that implements the `CIImageProvider` protocol. 
///    - width: The width of the image.
///    - height: The height of the image.
///    - format: The ``CIFormat`` of the provided pixels.
///    - colorSpace: The color space that the image is defined in. 
///          If `nil`, then the pixels will not be is not color matched to the Core Image working color space. 
///    - options: A dictionary that contains various ``CIImageOption`` keys that affect the resulting ``CIImage``.  
///          The option ``kCIImageProviderTileSize`` controls if and how the provider object is called in tiles. 
///          The option ``kCIImageProviderUserInfo`` allows additional state to be passed to the provider object.
/// - Returns:
///    An autoreleased ``CIImage`` object based on the data provider.
///
+ (CIImage *)imageWithImageProvider:(id)provider
							   size:(size_t)width
                                   :(size_t)height
							 format:(CIFormat)format
						 colorSpace:(nullable CGColorSpaceRef)colorSpace
                            options:(nullable NSDictionary<CIImageOption,id> *)options
    NS_AVAILABLE(10_4, 9_0);

/// Initializes an image object based on pixels from an image provider object.
///
/// Core Image retains the provider object until the image is deallocated.
/// The image provider object will not be called until the image is rendered.
/// 
/// - Parameters:
///    - provider: An object that implements the `CIImageProvider` protocol. 
///    - width: The width of the image.
///    - height: The height of the image.
///    - format: The ``CIFormat`` of the provided pixels.
///    - colorSpace: The color space that the image is defined in. 
///          If `nil`, then the pixels will not be is not color matched to the Core Image working color space. 
///    - options: A dictionary that contains various ``CIImageOption`` keys that affect the resulting ``CIImage``.  
///          The option ``kCIImageProviderTileSize`` controls if and how the provider object is called in tiles. 
///          The option ``kCIImageProviderUserInfo`` allows additional state to be passed to the provider object.
/// - Returns:
///    An initialized ``CIImage`` object based on the data provider.
///
- (instancetype)initWithImageProvider:(id)provider
                                 size:(size_t)width
                                     :(size_t)height
                               format:(CIFormat)format
                           colorSpace:(nullable CGColorSpaceRef)colorSpace
                              options:(nullable NSDictionary<CIImageOption,id> *)options
    NS_AVAILABLE(10_4, 9_0);

@end

/** Protocol used to lazily supply image data. */

@interface NSObject (CIImageProvider)

/// The method that an image provider object must implement.  
/// This method provides pixel data when the image object is rendered.
/// 
/// The implementation should provide pixels for the requested sub-rect `x,y,width,height` of the image. 
/// The sub-rect is in defined in the image's local coordinate space, 
/// where the origin is relative to the top left corner of the image.
/// 
/// By default, this method will be called to request the full image
/// regardless of what sub-rect is needed for the current render.
/// In this case the requested `x,y,width,height` will be `0,0,imageWidth,imageHeight`
/// 
/// If the ``kCIImageProviderTileSize`` option is specified when the ``CIImage`` was created,
/// then this method may be called once for each tile that is needed for the current render.
/// 
/// - Parameters:
///   - data: A pointer into which the provider should copy the pixels for the requested sub-rect. 
///   - rowbytes: The number of bytes per row for the requested pixels.
///   - originx: The x origin of the requested sub-rect relative to the upper left corner of the image.
///   - originy: The y origin of the requested sub-rect relative to the upper left corner of the image.
///   - width: The width of the requested sub-rect.
///   - height: The height of the requested sub-rect.
///   - info: The value of the `kCIImageProviderTileSize`` option specified when calling:
///           * ``/CIImage/imageWithImageProvider:size::format:colorSpace:options:``
///           * ``/CIImage/initWithImageProvider:size::format:colorSpace:options:``
///
- (void)provideImageData:(void *)data
			 bytesPerRow:(size_t)rowbytes
				  origin:(size_t)originx
                        :(size_t)originy
					size:(size_t)width
                        :(size_t)height
				userInfo:(nullable id)info;


/// An optional method that an image provider object may implement.  
/// With this method, the provider object can use the Metal API to provide pixel  
/// data into a MTLTexture when the image object is rendered.
/// 
/// The implementation should provide pixels for the requested sub-rect `x,y,width,height` of the image. 
/// The sub-rect is in defined in the image's local coordinate space, 
/// where the origin is relative to the top left corner of the image.
/// 
/// The work to fill the `MTLTexture` should be encoded on the specified `commandBuffer`.  
/// If the implementation uses its own commandBuffer, 
/// then it should call `waitUntilCompleted` before returning. 
/// If the texture is surface-backed then you only need to
/// call `waitUntilScheduled` before returning.
/// 
/// By default, this method will be called to request the full image
/// regardless of what sub-rect is needed for the current render.
/// In this case the requested `x,y,width,height` will be `0,0,imageWidth,imageHeight`
/// 
/// If the ``kCIImageProviderTileSize`` option is specified when the ``CIImage`` was created,
/// then this method may be called once for each tile that is needed for the current render.
/// 
/// - Parameters:
///   - texture: The `<id>MTLTexture` into which the provider should copy the pixels for the requested sub-rect. 
///   - commandBuffer: The `<id>MTLCommandBuffer` that the provider should use encoded the copy.
///   - originx: The x origin of the requested sub-rect relative to the upper left corner of the image.
///   - originy: The y origin of the requested sub-rect relative to the upper left corner of the image.
///   - width: The width of the requested sub-rect.
///   - height: The height of the requested sub-rect.
///   - info: The value of the `kCIImageProviderTileSize`` option specified when calling:
///           * ``/CIImage/imageWithImageProvider:size::format:colorSpace:options:``
///           * ``/CIImage/initWithImageProvider:size::format:colorSpace:options:``
///
- (void)provideImageToMTLTexture:(id<MTLTexture>)texture
                   commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                         originx:(size_t)originx
                         originy:(size_t)originy
                           width:(size_t)width
                          height:(size_t)height
                        userInfo:(nullable id)info NS_AVAILABLE(26_0, 26_0);

@end

/// Specifies the tile size that the Provide Image Data method will be called for.
/// 
/// This key and its value may be passed to:
/// * ``/CIImage/imageWithImageProvider:size::format:colorSpace:options:``
/// * ``/CIImage/initWithImageProvider:size::format:colorSpace:options:``
/// 
/// If the value of this key is:
///   Value                      | Behavior of sub-rect passed to ``provideImageData:bytesPerRow:origin::size::userData:``
///   -------------------------- | ----------------------------
///   Not specified              |  the entire image
///   `NSNumber`                 |  square tiles of size x size
///   `NSArray` with 2 numbers   |  rectangular tiles of width x height.
///   ``CIVector`` with 2 values |  rectangular tiles of width x height.
///   `NSNull`                   |  can be called for any possible origin and size.
///
CORE_IMAGE_EXPORT CIImageOption const kCIImageProviderTileSize NS_AVAILABLE(10_4, 9_0);

/// A key for any data needed by the image provider object. 
/// The associated value is an object that contains the needed data.
/// 
/// This key and its value may be passed to:
/// * ``/CIImage/imageWithImageProvider:size::format:colorSpace:options:``
/// * ``/CIImage/initWithImageProvider:size::format:colorSpace:options:``
/// 
/// The value object is retained until the image is deallocated.
///
CORE_IMAGE_EXPORT CIImageOption const kCIImageProviderUserInfo NS_AVAILABLE(10_4, 9_0);

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIIMAGEPROVIDER_H */
