/* CoreImage - CIContext.h

   Copyright (c) 2014 Apple, Inc.
   All rights reserved. */

#ifndef CICONTEXT_H
#define CICONTEXT_H

#ifdef __OBJC__

#import <CoreImage/CIImage.h>
#import <CoreImage/CoreImageDefines.h>
#import <CoreVideo/CoreVideo.h>

#if TARGET_OS_IPHONE && !TARGET_OS_MACCATALYST
  #define COREIMAGE_SUPPORTS_OPENGLES 1
#else
  #define COREIMAGE_SUPPORTS_OPENGLES 0
#endif 

#if COREIMAGE_SUPPORTS_OPENGLES
 #import <OpenGLES/EAGL.h>
#endif 
#if TARGET_OS_OSX
 #import <OpenGL/CGLTypes.h>
#endif 

@class CIFilter;

@protocol MTLDevice, MTLTexture, MTLCommandBuffer, MTLCommandQueue, MTLBuffer;

NS_ASSUME_NONNULL_BEGIN

/// The Core Image context class provides an evaluation context for Core Image processing with Metal, OpenGL, or OpenCL.
/// 
/// You use a `CIContext` instance to render a ``CIImage`` instance which represents a graph of image processing operations
/// which are built using other Core Image classes, such as ``CIFilter-class``, ``CIKernel``, ``CIColor`` and ``CIImage``. 
/// You can also use a `CIContext` with the ``CIDetector`` class to analyze images — for example, to detect faces 
/// or barcodes.
/// 
/// Contexts support automatic color management by performing all processing operations in a working color space.
/// This means that unless told otherwise:
/// * All input images are color matched from the input's color space to the working space.
/// * All renders are color matched from the working space to the destination space.
/// (For more information on `CGColorSpace` see <doc://com.apple.documentation/documentation/coregraphics/cgcolorspace>)
/// 
/// `CIContext` and ``CIImage`` instances are immutable, so multiple threads can use the same ``CIContext`` instance 
/// to render ``CIImage`` instances. However, ``CIFilter-class`` instances are mutable and thus cannot be shared safely among 
/// threads. Each thread must take case not to access or modify a ``CIFilter-class`` instance while it is being used by 
/// another thread.
/// 
/// The `CIContext` manages various internal state such as `MTLCommandQueue` and caches for compiled kernels
/// and intermediate buffers.  For this reason it is not recommended to create many `CIContext` instances.  As a rule,
/// it recommended that you create one `CIContext` instance for each view that renders ``CIImage`` or each background task.
///
NS_CLASS_AVAILABLE(10_4, 5_0) NS_SWIFT_SENDABLE
@interface CIContext : NSObject
{
    void *_priv;
}


/// An enum string type that your code can use to select different options when creating a Core Image context.
/// 
/// These option keys can be passed to `CIContext` creation APIs such as:
/// * ``/CIContext/contextWithOptions:``
/// * ``/CIContext/contextWithMTLDevice:options:``
/// 
typedef NSString * CIContextOption NS_TYPED_ENUM;

/// A Core Image context option key to specify the default destination color space for rendering.
/// 
/// This option only affects how Core Image renders using the following methods:
/// * ``/CIContext/createCGImage:fromRect:``
/// * ``/CIContext/drawImage:atPoint:fromRect:``
/// * ``/CIContext/drawImage:inRect:fromRect:``
/// 
/// With all other render methods, the destination color space is either specified as a parameter
/// or can be determined from the object being rendered to.
///
/// The value of this option can be either:
/// * A `CGColorSpace` instance with an RGB or monochrome color model that supports output.
/// * An `NSNull` instance to indicate that the context should not match from the working space to the destination.
///
/// If this option is not specified, then the default output space is sRGB.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextOutputColorSpace;

/// A Core Image context option key to specify the working color space for rendering.
/// 
/// Contexts support automatic color management by performing all processing operations 
/// in a working color space. This means that unless told otherwise:
/// * All input images are color matched from the input's color space to the working space.
/// * All renders are color matched from the working space to the destination's color space.
/// 
/// The default working space is the extended sRGB color space with linear gamma.
/// On macOS before 10.10, the default is extended Generic RGB with linear gamma. 
/// 
/// The value of this option can be either:
/// * A `CGColorSpace` instance with an RGB color model that supports output.
/// * An `NSNull` instance to request that Core Image perform no color management.
/// 
/// If this option is not specified, then the default working space is used.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextWorkingColorSpace;

/// A Core Image context option key to specify the pixel format to for intermediate results when rendering.
/// 
/// The value for this key is an `NSNumber` instance containing a ``CIFormat`` value. 
/// 
/// The supported values for the working pixel format are:
/// ``CIFormat``        | Notes
/// ------------------- | --------------
/// ``kCIFormatRGBA8``  | Uses 4 bytes per pixel. Only supporrts SDR and has less precision.
/// ``kCIFormatRGBAh``  | Uses 8 bytes per pixel. Supports HDR.
/// ``kCIFormatRGBAf``  | Uses 16 bytes per pixel. Only available on macOS
/// 
/// If this option is not specified, then the default is ``kCIFormatRGBAh``.
/// 
/// (The default is ``kCIFormatRGBA8`` if your if app is linked against iOS 12 SDK or earlier.)
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextWorkingFormat NS_AVAILABLE(10_4,8_0);

/// A Boolean value to control the quality of image downsampling operations performed by the 
/// Core Image context.
/// 
/// The higher quality behavior performs downsampling operations in multiple passes
/// in order to reduce aliasing artifacts.
/// 
/// The lower quality behavior performs downsampling operations a single pass
/// in order to improve performance.
///
/// If the value for this option is:
/// * True: The higher quality behavior will be used. 
/// * False: The lower quality behavior will be used. 
/// * Not specified: the default behavior is True on macOS and False on other platforms. 
/// 
/// > Note: 
/// > * This option does affect how ``/CIImage/imageByApplyingTransform:`` operations are performed by the context.
/// > * This option does not affect how ``/CIImage/imageByApplyingTransform:highQualityDownsample:`` behaves.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextHighQualityDownsample NS_AVAILABLE(10_11,9_0);

/// A Boolean value to control how a Core Image context render produces alpha-premultiplied pixels.
///
/// This option only affects how a context is rendered when using methods where the destination's
/// alpha mode cannot be determined such as:
/// *  ``/CIContext/render:toBitmap:rowBytes:bounds:format:colorSpace:``
/// *  ``/CIContext/render:toCVPixelBuffer:``
/// *  ``/CIContext/render:toIOSurface:bounds:colorSpace:``
/// *  ``/CIContext/render:toMTLTexture:commandBuffer:bounds:colorSpace:``
/// *  ``/CIContext/createCGImage:fromRect:``
/// 
/// If the value for this option is:
/// * True: The output will produce alpha-premultiplied pixels. 
/// * False: The output will produce un-premultiplied pixels. 
/// * Not specified: the default behavior True. 
/// 
/// This option does not affect how a context is rendered to a ``CIRenderDestination`` because
/// that API allows you to set or override the alpha behavior using ``/CIRenderDestination/alphaMode``.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextOutputPremultiplied NS_AVAILABLE(10_4,7_0);

/// A Boolean value to control how a Core Image context caches the contents of any intermediate image buffers it uses during rendering.
/// 
/// If a context caches intermediate buffers, then subsequent renders of a similar image using the same context
/// may be able to render faster. If a context does not cache intermediate buffers, then it may use less memory.
/// 
/// If the value for this option is:
/// * True: The context will cache intermediate results for future renders using the same context. 
/// * False: The context will not cache intermediate results. 
/// * Not specified: the default behavior True. 
/// 
/// > Note: 
/// > * This option does affect how ``/CIImage/imageByInsertingIntermediate`` behaves.
/// > * This option does not affect how ``/CIImage/imageByInsertingIntermediate:`` behaves.
///
CORE_IMAGE_EXPORT CIContextOption const kCIContextCacheIntermediates NS_AVAILABLE(10_12,10_0);

/// A Boolean value to control if a Core Image context will use a software renderer.
/// 
/// > Note: This option has no effect if the platform does not support OpenCL.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextUseSoftwareRenderer;

/// A Boolean value to control the priority Core Image context renders.
/// 
/// If this value is True, then rendering with the context from a background thread takes lower priority 
/// than other GPU usage from the main thread. This allows your app to perform Core Image rendering without 
/// disturbing the frame rate of UI animations.
///
CORE_IMAGE_EXPORT CIContextOption const kCIContextPriorityRequestLow NS_AVAILABLE(10_12, 8_0);

/// A Boolean value to control the power level of Core Image context renders.
/// 
/// This option only affects certain macOS devices with more than one available GPU device.
/// 
/// If this value is True, then rendering with the context will use a use allow power GPU device
/// if available and the high power device is not already in use.
/// 
/// Otherwise, the context will use the highest power/performance GPU device.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextAllowLowPower NS_AVAILABLE(10_12, 13_0);

/// A Boolean value to specify a client-provided name for a context.
/// 
/// This name will be used in QuickLook graphs and the output of CI_PRINT_TREE.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextName NS_AVAILABLE(10_14,12_0);

/// A number value to control the maximum memory in megabytes that the context allocates for render tasks.  
/// 
/// Larger values could increase memory  footprint while smaller values could reduce performance.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextMemoryLimit NS_AVAILABLE(14_0, 17_0);

/// A Core Video Metal texture cache object to improve the performance of Core Image context
/// renders that use Core Video pixel buffers.
/// 
/// Creating a Core Image context with this optional `CVMetalTextureCache` can improve the 
/// performance of creating a Metal texture from a `CVPixelBuffer`. It is recommended 
/// to specify this option if the context renders to or from pixel buffers that come 
/// from a `CVPixelBufferPool`.
/// 
/// It is the client's responsibility to flush the cache when appropriate.
/// 
CORE_IMAGE_EXPORT CIContextOption const kCIContextCVMetalTextureCache NS_AVAILABLE(26_0,26_0);

#pragma mark - contextWithCGLContext

/* Create a new CoreImage context object, all output will be drawn
 * into the surface attached to the OpenGL context 'cglctx'. If 'pixelFormat' is
 * non-null it should be the pixel format object used to create 'cglctx';
 * it's required to be valid for the lifetime of the CIContext.
 * The colorspace should be set to the colorspace of your target otherwise
 * CI will take the colorspace from the CGLContext if available. */
#if TARGET_OS_OSX
+ (CIContext *)contextWithCGLContext:(CGLContextObj)cglctx
						 pixelFormat:(nullable CGLPixelFormatObj)pixelFormat
						  colorSpace:(nullable CGColorSpaceRef)colorSpace
							 options:(nullable NSDictionary<CIContextOption, id> *)options
    CI_GL_DEPRECATED_MAC(10_6,10_14);
#endif

/* DEPRECATED, please use the method above or if you need this
* for backward capability, make sure that you specify the colorspace
* in the options dictionary */
#if TARGET_OS_OSX
+ (CIContext *)contextWithCGLContext:(CGLContextObj)cglctx
						 pixelFormat:(nullable CGLPixelFormatObj)pixelFormat
							 options:(nullable NSDictionary<CIContextOption, id> *)options
    CI_GL_DEPRECATED_MAC(10_4,10_6);
#endif


#pragma mark - contextWithCGContext

/* Create a context specifying a destination CGContext.
 *
 * Core Image will use an internal destination context when methods such
 * as [context render:to...] or [context createCGImage:...] are called.
 *
 * The [context drawImage:...] render methods will render to the CGContext.
 */
+ (CIContext *)contextWithCGContext:(CGContextRef)cgctx
                            options:(nullable NSDictionary<CIContextOption, id> *)options
    NS_AVAILABLE(10_4,9_0);


#pragma mark - context without specifying a destination

/* Create a context without specifying a destination CG/GL/Metal context.
 *
 * Core Image will use an internal destination context when methods such
 * as [context render:to...] or [context createCGImage:...] are called.
 *
 * The [context drawImage:...] render methods will not operate on this type
 * of context.
 */

+ (CIContext *)contextWithOptions:(nullable NSDictionary<CIContextOption, id> *)options
    NS_AVAILABLE(10_4,5_0);

+ (CIContext *)context NS_AVAILABLE(10_4,5_0);

- (instancetype)initWithOptions:(nullable NSDictionary<CIContextOption, id> *)options
NS_AVAILABLE(10_4,5_0);

- (instancetype)init NS_AVAILABLE(10_4,5_0);


#pragma mark - contextWithEAGLContext

/* Create a context specifying a destination EAGLContext.
 *
 * Core Image will use an internal destination context when methods such
 * as [context render:to...] or [context createCGImage:...] are called.
 *
 * The [context drawImage:...] render methods will render to the EAGLContext.
 */
#if COREIMAGE_SUPPORTS_OPENGLES
+ (CIContext *)contextWithEAGLContext:(EAGLContext *)eaglContext
    CI_GL_DEPRECATED_IOS(5_0,12_0);

+ (CIContext *)contextWithEAGLContext:(EAGLContext *)eaglContext
                              options:(nullable NSDictionary<CIContextOption, id> *)options
    CI_GL_DEPRECATED_IOS(5_0,12_0);
#endif


#pragma mark - contextWithMTLDevice

/* If a system has more than one MTLDevice, then you can create a CIContext
 * that uses a specific device. If a client wishes to use the default MTLDevice
 * then call [CIContext contextWithOptions:] instead. */
+ (CIContext *)contextWithMTLDevice:(id<MTLDevice>)device NS_AVAILABLE(10_11,9_0);

+ (CIContext *)contextWithMTLDevice:(id<MTLDevice>)device
                            options:(nullable NSDictionary<CIContextOption, id> *)options
    NS_AVAILABLE(10_11,9_0);

#pragma mark - contextWithMTLCommandQueue

/* Create a CIContext that commits commands to a specific queue.*/
+ (CIContext *)contextWithMTLCommandQueue:(id<MTLCommandQueue>)commandQueue NS_AVAILABLE(10_15,13_0);

+ (CIContext *)contextWithMTLCommandQueue:(id<MTLCommandQueue>)commandQueue
                            options:(nullable NSDictionary<CIContextOption, id> *)options
NS_AVAILABLE(10_15,13_0);


#pragma mark - properties

/// The working color space of the CIContext.
/// 
/// The working color space determines the color space used when executing filter kernels. 
/// You specify a working color space using the ``kCIContextWorkingColorSpace`` option when creating a ``CIContext``.
/// * All input images are color matched from the input's color space to the working space.
/// * All renders are color matched from the working space to the destination space.
///
/// The property will be `null` if the context was created with color management disabled.
///
@property (nullable, nonatomic, readonly) CGColorSpaceRef workingColorSpace NS_AVAILABLE(10_11,9_0);

/// The working pixel format that the CIContext uses for intermediate buffers.
/// 
/// The working format determines the pixel format that Core Image uses to create intermediate buffers for rendering images. 
/// You specify a working pixel format using the ``kCIContextWorkingFormat`` option when creating a ``CIContext``.
///
@property (nonatomic, readonly) CIFormat workingFormat NS_AVAILABLE(10_11,9_0);

#pragma mark - render methods

/* DEPRECATED, please use drawImage:inRect:fromRect: instead.
 * Render the subregion 'fromRect' of 'image' to point 'atPoint' in the context's destination. */
- (void)drawImage:(CIImage *)image
          atPoint:(CGPoint)atPoint
         fromRect:(CGRect)fromRect NS_DEPRECATED(10_4,10_8, 5_0,6_0);

/* Render the rectangle 'fromRect' of 'image' to the rectangle 'inRect' in the
 * context's destination. */
- (void)drawImage:(CIImage *)image
           inRect:(CGRect)inRect
         fromRect:(CGRect)fromRect;

/* Create a CoreGraphics layer object suitable for creating content for
 * subsequently rendering into this CI context. The 'info' parameter is
 * passed into CGLayerCreate () as the auxiliaryInfo dictionary.
 * This will return null if size is empty or infinite. */
- (nullable CGLayerRef)createCGLayerWithSize:(CGSize)size
                                        info:(nullable CFDictionaryRef)info
CF_RETURNS_RETAINED NS_DEPRECATED_MAC(10_4,10_11);

/* Render 'image' to the given bitmap.
 * The 'data' parameter must point to at least rowBytes*floor(bounds.size.height) bytes.
 * The 'bounds' parameter has the following behavior:
 *    The 'bounds' parameter acts to specify the region of 'image' to render.
 *    This region (regardless of its origin) is rendered at upper-left corner of 'data'.
 * Passing a 'colorSpace' value of null means:
 *   Disable output color management if app is linked against iOS SDK
 *   Disable output color management if app is linked against OSX 10.11 SDK or later
 *   Match to context's output color space if app is linked against OSX 10.10 SDK or earlier
 */
- (void)render:(CIImage *)image
	  toBitmap:(void *)data
	  rowBytes:(ptrdiff_t)rowBytes
		bounds:(CGRect)bounds
		format:(CIFormat)format
	colorSpace:(nullable CGColorSpaceRef)colorSpace;

#if COREIMAGE_SUPPORTS_IOSURFACE
/* Render 'image' to the given IOSurface.
 * Point (0,0) in the image coordinate system will align to the lower left corner of 'surface'.
 * The 'bounds' parameter acts as a clip rect to limit what region of 'surface' is modified.
 * If 'colorSpace' is nil, CI will not color match to the destination.
 */
- (void)render:(CIImage *)image
   toIOSurface:(IOSurfaceRef)surface
		bounds:(CGRect)bounds
	colorSpace:(nullable CGColorSpaceRef)colorSpace NS_AVAILABLE(10_6,5_0);
#endif

/* Render 'image' into the given CVPixelBuffer. 
 * In OS X 10.11.3 and iOS 9.3 and later
 *   CI will color match to the colorspace of the buffer.
 * otherwise
 *   CI will color match to the context's output colorspace.
 */
- (void)render:(CIImage *)image 
toCVPixelBuffer:(CVPixelBufferRef)buffer NS_AVAILABLE(10_11,5_0);

/* Render 'image' to the given CVPixelBufferRef.
 * Point (0,0) in the image coordinate system will align to the lower left corner of 'buffer'.
 * The 'bounds' parameter acts as a clip rect to limit what region of 'buffer' is modified.
 * If 'colorSpace' is nil, CI will not color match to the destination.
 */
- (void)render:(CIImage *)image
toCVPixelBuffer:(CVPixelBufferRef)buffer
        bounds:(CGRect)bounds
    colorSpace:(nullable CGColorSpaceRef)colorSpace NS_AVAILABLE(10_11,5_0);

/* Render 'bounds' of 'image' to a Metal texture, optionally specifying what command buffer to use.
 * Texture type must be MTLTexture2D.
 * NOTE: Rendering to a texture initialized with a commandBuffer requires encoding all the commands to render an image into the specified buffer.
 * This may impact system responsiveness and may result in higher memory usage if the image requires many passes to render.
 * To avoid this impact, it is recommended to create a context using [CIContext contextWithMTLCommandQueue:].
 */
- (void)render:(CIImage *)image
  toMTLTexture:(id<MTLTexture>)texture
 commandBuffer:(nullable id<MTLCommandBuffer>)commandBuffer
        bounds:(CGRect)bounds
    colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_11,9_0);


#pragma mark -

/* Runs the context's garbage collector to reclaim any resources that
 * are no longer required (e.g. removes textures from the texture cache
 * that reference deleted images.) This method is called automatically
 * after every rendering operation. */
- (void)reclaimResources NS_AVAILABLE_MAC(10_4);

/* Frees any cached data (such as temporary images) associated with the
 * context. This also runs the garbage collector. */
- (void)clearCaches NS_AVAILABLE(10_4,10_0);

/* Returns the maximum dimension for input images that can be processed 
 * on the context. */
- (CGSize)inputImageMaximumSize NS_AVAILABLE_IOS(5_0);

/* Returns the maximum dimension for image that can be rendered 
 * on the context. */
- (CGSize)outputImageMaximumSize NS_AVAILABLE_IOS(5_0);

@end


@interface CIContext (createCGImage)

/// Creates a Core Graphics image from a region of a Core Image image instance.
/// 
/// The color space of the created `CGImage` will be sRGB unless the receiving ``CIContext``
/// was created with a `kCIContextOutputColorSpace` option.
/// 
/// Normally the pixel format of the created CGImage will be 8 bits-per-component.
/// It will be 16 bits-per-component float if the above color space is HDR.
///
/// - Parameters:
///    - image: A ``CIImage`` image instance for which to create a `CGImage`.
///    - fromRect: The `CGRect` region of the `image` to use. 
///        This region relative to the cartesean coordinate system of `image`.
///        This region will be intersected with integralized and intersected with `image.extent`.
///
/// - Returns:
///    Returns a new `CGImage` instance. 
///    You are responsible for releasing the returned image when you no longer need it. 
///    The returned value will be `null` if the extent is empty or too big.
///
- (nullable CGImageRef)createCGImage:(CIImage *)image
                            fromRect:(CGRect)fromRect
CF_RETURNS_RETAINED;

/// Creates a Core Graphics image from a region of a Core Image image instance
/// with an option for controlling the pixel format and color space of the `CGImage`.
///
/// - Parameters:
///    - image: A ``CIImage`` image instance for which to create a `CGImage`.
///    - fromRect: The `CGRect` region of the `image` to use. 
///        This region relative to the cartesean coordinate system of `image`.
///        This region will be intersected with integralized and intersected with `image.extent`.
///    - format: A ``CIFormat`` to specify the pixel format of the created `CGImage`.
///        For example, if `kCIFormatRGBX16` is specified, then the created `CGImage` will 
///        be 16 bits-per-component and opaque.
///    - colorSpace: The `CGColorSpace` for the output image. 
///        This color space must have either `CGColorSpaceModel.rgb` or `CGColorSpaceModel.monochrome` 
///        and be compatible with the specified pixel format.
///
/// - Returns:
///    Returns a new `CGImage` instance. 
///    You are responsible for releasing the returned image when you no longer need it. 
///    The returned value will be `null` if the extent is empty or too big.
///
- (nullable CGImageRef)createCGImage:(CIImage *)image
                            fromRect:(CGRect)fromRect
                              format:(CIFormat)format
                          colorSpace:(nullable CGColorSpaceRef)colorSpace
CF_RETURNS_RETAINED;

/// Creates a Core Graphics image from a region of a Core Image image instance
/// with an option for controlling when the image is rendered.
///
/// - Parameters:
///    - image: A ``CIImage`` image instance for which to create a `CGImage`.
///    - fromRect: The `CGRect` region of the `image` to use. 
///        This region relative to the cartesean coordinate system of `image`.
///        This region will be intersected with integralized and intersected with `image.extent`.
///    - format: A ``CIFormat`` to specify the pixel format of the created `CGImage`.
///        For example, if `kCIFormatRGBX16` is specified, then the created `CGImage` will 
///        be 16 bits-per-component and opaque.
///    - colorSpace: The `CGColorSpace` for the output image. 
///        This color space must have either `CGColorSpaceModel.rgb` or `CGColorSpaceModel.monochrome` 
///        and be compatible with the specified pixel format.
///    - deferred: Controls when Core Image renders `image`.
///        * True: rendering of `image` is deferred until the created `CGImage` rendered. 
///        * False: the `image` is rendered immediately.
///
/// - Returns:
///    Returns a new `CGImage` instance. 
///    You are responsible for releasing the returned image when you no longer need it. 
///    The returned value will be `null` if the extent is empty or too big.
///
- (nullable CGImageRef)createCGImage:(CIImage *)image
                            fromRect:(CGRect)fromRect
                              format:(CIFormat)format
                          colorSpace:(nullable CGColorSpaceRef)colorSpace
                            deferred:(BOOL)deferred
CF_RETURNS_RETAINED NS_AVAILABLE(10_12,10_0);

/// Creates a Core Graphics image from a region of a Core Image image instance
/// with an option for calculating HDR statistics.
///
/// - Parameters:
///    - image: A ``CIImage`` image instance for which to create a `CGImage`.
///    - fromRect: The `CGRect` region of the `image` to use. 
///        This region relative to the cartesean coordinate system of `image`.
///        This region will be intersected with integralized and intersected with `image.extent`.
///    - format: A ``CIFormat`` to specify the pixel format of the created `CGImage`.
///        For example, if `kCIFormatRGBX16` is specified, then the created `CGImage` will 
///        be 16 bits-per-component and opaque.
///    - colorSpace: The `CGColorSpace` for the output image. 
///        This color space must have either `CGColorSpaceModel.rgb` or `CGColorSpaceModel.monochrome` 
///        and be compatible with the specified pixel format.
///    - deferred: Controls when Core Image renders `image`.
///        * True: rendering of `image` is deferred until the created `CGImage` rendered. 
///        * False: the `image` is rendered immediately.
///    - calculateHDRStats: Controls if Core Image calculates HDR statistics.
///        * True: Core Image will immediately render `image`, calculate the HDR statistics
///        and create a `CGImage` that has the calculated values.
///        * False:  the created `CGImage` will not have any HDR statistics.
///
/// - Returns:
///    Returns a new `CGImage` instance. 
///    You are responsible for releasing the returned image when you no longer need it. 
///    The returned value will be `null` if the extent is empty or too big.
///
- (nullable CGImageRef)createCGImage:(CIImage *)image
                            fromRect:(CGRect)fromRect
                              format:(CIFormat)format
                          colorSpace:(nullable CGColorSpaceRef)colorSpace
                            deferred:(BOOL)deferred
                   calculateHDRStats:(BOOL)calculateHDRStats
CF_RETURNS_RETAINED NS_AVAILABLE(26_0,26_0);

@end


@interface CIContext (CalculateHDRStats)

/// Given an IOSurface, use the receiving Core Image context to calculate its 
/// HDR statistics (content headroom and content average light level)
/// and then update the surface's attachments to store the values.
/// 
/// If the `IOSurface` has a Clean Aperture rectangle then only pixels within
/// that rectangle are considered.
///
/// - Parameters:
///    - surface: A mutable `IOSurfaceRef` for which to calculate and attach statistics.
///    
- (void) calculateHDRStatsForIOSurface:(IOSurfaceRef)surface
NS_AVAILABLE(26_0,26_0);

/// Given a CVPixelBuffer, use the receiving Core Image context to calculate its 
/// HDR statistics (content headroom and content average light level)
/// and then update the buffer's attachments to store the values.
/// 
/// If the `CVPixelBuffer` has a Clean Aperture rectangle then only pixels within
/// that rectangle are considered.
///
/// - Parameters:
///    - buffer: A mutable `CVPixelBuffer` for which to calculate and attach statistics.
///    
- (void) calculateHDRStatsForCVPixelBuffer:(CVPixelBufferRef)buffer
NS_AVAILABLE(26_0,26_0);

/// Given a Core Graphics image, use the receiving Core Image context to calculate its 
/// HDR statistics (content headroom and content average light level)
/// and then return a new Core Graphics image that has the calculated values.
///
/// - Parameters:
///    - cgimage: An immutable `CGImage` for which to calculate statistics.
/// - Returns:
///    Returns a new `CGImage` instance that has the calculated statistics attached.
///
- (CGImageRef) calculateHDRStatsForCGImage:(CGImageRef)cgimage
CF_RETURNS_RETAINED NS_AVAILABLE(26_0,26_0);

/// Given a Core Image image, use the receiving Core Image context to calculate its 
/// HDR statistics (content headroom and content average light level)
/// and then return a new Core Image image that has the calculated values.
///
/// If the image extent is not finite, then nil will be returned.
///
/// - Parameters:
///    - image: An immutable ``CIImage`` for which to calculate statistics.
/// - Returns:
///    Returns a new ``CIImage`` instance that has the calculated statistics attached.
///    
- (nullable CIImage*) calculateHDRStatsForImage:(CIImage*)image
NS_AVAILABLE(26_0,26_0);

@end


@interface CIContext (OfflineGPUSupport)

/* Not all GPUs will be driving a display. If they are offline we can still use them
 * to do work with Core Image. This method returns the number of offline GPUs which
 * can be used for this purpose */
+(unsigned int)offlineGPUCount NS_AVAILABLE_MAC(10_10);

/* These two methods lets you create a CIContext based on an offline gpu index.
 * The first method takes only the GPU index as a parameter, the second, takes
 * an optional colorspace, options dictionary and a CGLContext which can be
 * shared with other GL resources.  The return value will be null if index is 
 * out of range (e.g. if the device has no offline GPUs).
 */
#if TARGET_OS_OSX
+ (nullable CIContext *)contextForOfflineGPUAtIndex:(unsigned int)index CI_GL_DEPRECATED_MAC(10_10,10_14);
+ (nullable CIContext *)contextForOfflineGPUAtIndex:(unsigned int)index
                                         colorSpace:(nullable CGColorSpaceRef)colorSpace
                                            options:(nullable NSDictionary<CIContextOption, id> *)options
                                      sharedContext:(nullable CGLContextObj)sharedContext CI_GL_DEPRECATED_MAC(10_10,10_14);
#endif


@end

/// An enum string type that your code can use to select different options when saving to image representations such as JPEG and HEIF.
/// 
/// Some of the methods that support these options are:
/// * ``/CIContext/JPEGRepresentationOfImage:colorSpace:options:``
/// * ``/CIContext/HEIFRepresentationOfImage:format:colorSpace:options:``
/// 
typedef NSString * CIImageRepresentationOption NS_TYPED_ENUM; 

@interface CIContext (ImageRepresentation)

/// An optional key and value to save additional depth channel information to a JPEG or HEIF representations.
/// 
/// The value for this key needs to be an `AVDepthData` instance.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationAVDepthData NS_AVAILABLE(10_13,11_0);

/// An optional key and value to save additional depth channel information to a JPEG or HEIF.
/// 
/// The value for this key needs to be a monochrome depth ``CIImage`` instance.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationDepthImage NS_AVAILABLE(10_13,11_0);

/// An optional key and value to save additional depth channel information to a JPEG or HEIF.
/// 
/// The value for this key needs to be a monochrome disparity ``CIImage`` instance.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationDisparityImage NS_AVAILABLE(10_13,11_0);


/// An optional key and value to save a portrait matte channel information to a JPEG or HEIF.
/// 
/// The value for this key needs to be a an `AVPortraitEffectsMatte` instance.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationAVPortraitEffectsMatte NS_AVAILABLE(10_14,12_0);

/// An optional key and value to save a portrait matte channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a portrait matte ``CIImage`` instance where black pixels
/// represent the background region and white pixels represent the primary people in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationPortraitEffectsMatteImage NS_AVAILABLE(10_14,12_0);


/// An optional key and value to save one or more segmentation matte channels to a JPEG or HEIF.
/// 
/// The value for this key needs to be an array of AVSemanticSegmentationMatte instances.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationAVSemanticSegmentationMattes NS_AVAILABLE(10_15, 13_0);

/// An optional key and value to save a skin segmentation channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a ``CIImage`` instance where white pixels 
/// represent the areas of person's skin are found in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationSemanticSegmentationSkinMatteImage NS_AVAILABLE(10_15, 13_0);

/// An optional key and value to save a skin segmentation channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a ``CIImage`` instance where white pixels
/// represent the areas of person's head and facial hair are found in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationSemanticSegmentationHairMatteImage NS_AVAILABLE(10_15, 13_0);

/// An optional key and value to save a skin segmentation channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a ``CIImage`` instance where white pixels
/// represent the areas where a person's teeth are found in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationSemanticSegmentationTeethMatteImage NS_AVAILABLE(10_15, 13_0);

/// An optional key and value to save a skin segmentation channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a ``CIImage`` instance where white pixels 
/// represent the areas where a person's glasses are found in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationSemanticSegmentationGlassesMatteImage NS_AVAILABLE(11_0, 14_1);


/// An optional key and value to save a skin segmentation channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a ``CIImage`` instance where white pixels
/// represent the areas where a person's skin are found in the image.
/// The image will be converted to monochrome before it is saved to the JPEG or HEIF.
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationSemanticSegmentationSkyMatteImage NS_AVAILABLE(11_1, 14_3);

/// An optional key and value to save a HDR image using the gain map channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a HDR CIImage instance.
///
/// When provided, Core Image will calculate a gain map auxiliary image 
/// from the ratio of the HDR image to the primary SDR image.
///
/// If the the HDR ``CIImage`` instance has a ``/CIImage/contentHeadroom`` property, 
/// then that will be used when calculating the HDRGainMap image and metadata.
///
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationHDRImage NS_AVAILABLE(15_0, 18_0);

/// An optional key and value to save a gain map channel to a JPEG or HEIF.
/// 
/// The value for this key needs to be a monochrome ``CIImage`` instance.
/// 
/// If the ``kCIImageRepresentationHDRGainMapAsRGB`` option it true, then it needs to
/// be an RGB ``CIImage`` instance.
/// 
/// The ``/CIImage/properties`` should contain metadata information equivalent to what is returned when 
/// initializing an image using ``kCIImageAuxiliaryHDRGainMap``.
///
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationHDRGainMapImage NS_AVAILABLE(11_0, 14_1);

/// An optional key and value to request the gain map channel to be color instead of monochrome.
/// 
/// This key affects how the gain map image is calculated from the SDR receiver and
/// the ``kCIImageRepresentationHDRImage`` image value.
/// 
/// The value for this is a Boolean where:
///  * True: the gain map is created as a color ratio between the HDR and SDR images. 
///  * False: the gain map is created as a brightness ratio between the HDR and SDR images.
///  * Not specified: the default behavior False. 
/// 
CORE_IMAGE_EXPORT CIImageRepresentationOption const kCIImageRepresentationHDRGainMapAsRGB NS_AVAILABLE(15_0, 18_0);


/* Render a CIImage to TIFF data. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* No options keys are supported at this time. */
- (nullable NSData*) TIFFRepresentationOfImage:(CIImage*)image
                                        format:(CIFormat)format
                                    colorSpace:(CGColorSpaceRef)colorSpace
                                       options:(NSDictionary<CIImageRepresentationOption, id>*)options NS_AVAILABLE(10_12,10_0);

/* Render a CIImage to JPEG data. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* kCGImageDestinationEmbedThumbnail, and the depth, disparity, and matte options. */
- (nullable NSData*) JPEGRepresentationOfImage:(CIImage*)image
                                    colorSpace:(CGColorSpaceRef)colorSpace
                                       options:(NSDictionary<CIImageRepresentationOption, id>*)options NS_AVAILABLE(10_12,10_0);

/* Render a CIImage to HEIF data. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* kCGImageDestinationEmbedThumbnail,and the depth, disparity, and matte options. */
- (nullable NSData*) HEIFRepresentationOfImage:(CIImage*)image
                                        format:(CIFormat)format
                                    colorSpace:(CGColorSpaceRef)colorSpace
                                       options:(NSDictionary<CIImageRepresentationOption, id>*)options NS_AVAILABLE(10_13_4,11_0);

/* Render a CIImage to HEIF data. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* and the depth, disparity, and matte options. */
- (nullable NSData*) HEIF10RepresentationOfImage:(CIImage*)image
                                      colorSpace:(CGColorSpaceRef)colorSpace
                                         options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                           error:(NSError **)errorPtr NS_AVAILABLE(12_0,15_0);

/* Render a CIImage to PNG data. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* No options keys are supported at this time. */
- (nullable NSData*) PNGRepresentationOfImage:(CIImage*)image
                                       format:(CIFormat)format
                                   colorSpace:(CGColorSpaceRef)colorSpace
                                      options:(NSDictionary<CIImageRepresentationOption, id>*)options NS_AVAILABLE(10_13,11_0);

/* Render a CIImage to OpenEXR data. Image must have a finite non-empty extent. */
/* No options keys are supported at this time. */
- (nullable NSData*) OpenEXRRepresentationOfImage:(CIImage*)image
                                          options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                            error:(NSError **)errorPtr NS_AVAILABLE(14_0, 17_0);

/* Render a CIImage to TIFF file. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* No options keys are supported at this time. */
- (BOOL) writeTIFFRepresentationOfImage:(CIImage*)image
                                  toURL:(NSURL*)url
                                 format:(CIFormat)format
                             colorSpace:(CGColorSpaceRef)colorSpace 
                                options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                  error:(NSError **)errorPtr NS_AVAILABLE(10_12,10_0);

/* Render a CIImage to PNG file. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* No options keys are supported at this time. */
- (BOOL) writePNGRepresentationOfImage:(CIImage*)image
                                 toURL:(NSURL*)url
                                format:(CIFormat)format
                            colorSpace:(CGColorSpaceRef)colorSpace
                               options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                 error:(NSError **)errorPtr NS_AVAILABLE(10_13,11_0);

/* Render a CIImage to JPEG file. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* kCGImageDestinationEmbedThumbnail, and the depth, disparity, and matte options. */
- (BOOL) writeJPEGRepresentationOfImage:(CIImage*)image
                                  toURL:(NSURL*)url
                             colorSpace:(CGColorSpaceRef)colorSpace
                                options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                  error:(NSError **)errorPtr NS_AVAILABLE(10_12,10_0);

/* Render a CIImage to HEIF file. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome */
/* and must match the specified CIFormat. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* kCGImageDestinationEmbedThumbnail, and the depth, disparity, and matte options. */
- (BOOL) writeHEIFRepresentationOfImage:(CIImage*)image
                                  toURL:(NSURL*)url
                                 format:(CIFormat)format
                             colorSpace:(CGColorSpaceRef)colorSpace
                                options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                  error:(NSError **)errorPtr NS_AVAILABLE(10_13_4,11_0);

/* Render a CIImage to 10-bit deep HEIF file. Image must have a finite non-empty extent. */
/* The CGColorSpace must be kCGColorSpaceModelRGB or kCGColorSpaceModelMonochrome. */
/* Supported options keys are kCGImageDestinationLossyCompressionQuality, */
/* and the depth, disparity, and matte options. */
- (BOOL) writeHEIF10RepresentationOfImage:(CIImage*)image
                                    toURL:(NSURL*)url
                               colorSpace:(CGColorSpaceRef)colorSpace
                                  options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                    error:(NSError **)errorPtr NS_AVAILABLE(12_0,15_0);

/* Render a CIImage to OpenEXR file. Image must have a finite non-empty extent. */
/* No options keys are supported at this time. */
- (BOOL) writeOpenEXRRepresentationOfImage:(CIImage*)image
                                     toURL:(NSURL*)url
                                   options:(NSDictionary<CIImageRepresentationOption, id>*)options
                                     error:(NSError **)errorPtr NS_AVAILABLE(14_0, 17_0);

@end

@interface CIContext (CIDepthBlurEffect)

/* Create and CIFilter instance for 'image' that can be used to apply the CIDepthBlurEffect.
 *
 * The receiver context is user to render the image in order to get the facial landmarks
 *
 * The 'options' parameter is a key value/pair reserved for future use.
 *
 */

- (nullable CIFilter *) depthBlurEffectFilterForImageURL:(NSURL *)url options:(nullable NSDictionary *)options NS_AVAILABLE(10_14,12_0);

/* This is the same as the method above expect it uses NSData to instantiate the image data
 * instead of the contents of a NSURL.
 *
 */

- (nullable CIFilter *) depthBlurEffectFilterForImageData:(NSData *)data options:(nullable NSDictionary *)options NS_AVAILABLE(10_14,12_0);


/* Create and CIFilter instance for 'image' that can be used to apply the CIDepthBlurEffect.
 *
 * The receiver context is user to render the image in order to get the facial landmarks
 *
 * The 'orientation' parameter should be CGImagePropertyOrientation enum value
 * as defined in the TIFF spec.
 *
 * The 'options' parameter is a key value/pair reserved for future use.
 *
 */

- (nullable CIFilter *) depthBlurEffectFilterForImage:(CIImage *)image
                                       disparityImage:(CIImage *)disparityImage
                                 portraitEffectsMatte:(nullable CIImage *)portraitEffectsMatte
                                          orientation:(CGImagePropertyOrientation)orientation
                                              options:(nullable NSDictionary *)options NS_AVAILABLE(10_14,12_0);

- (nullable CIFilter *) depthBlurEffectFilterForImage:(CIImage *)image
                                       disparityImage:(CIImage *)disparityImage
                                 portraitEffectsMatte:(nullable CIImage *)portraitEffectsMatte
                             hairSemanticSegmentation:(nullable CIImage *)hairSemanticSegmentation
                                          orientation:(CGImagePropertyOrientation)orientation
                                              options:(nullable NSDictionary *)options NS_AVAILABLE(10_15,13_0);

- (nullable CIFilter *) depthBlurEffectFilterForImage:(CIImage *)image
                                       disparityImage:(CIImage *)disparityImage
                                 portraitEffectsMatte:(nullable CIImage *)portraitEffectsMatte
                             hairSemanticSegmentation:(nullable CIImage *)hairSemanticSegmentation
                                         glassesMatte:(nullable CIImage *)glassesMatte
                                              gainMap:(nullable CIImage *)gainMap
                                          orientation:(CGImagePropertyOrientation)orientation
                                              options:(nullable NSDictionary *)options NS_AVAILABLE(11_0,14_1);

@end


NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CICONTEXT_H */
