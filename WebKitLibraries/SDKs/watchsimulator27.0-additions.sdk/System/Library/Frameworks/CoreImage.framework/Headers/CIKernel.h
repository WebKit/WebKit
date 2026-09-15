/* CoreImage - CIKernel.h

   Copyright (c) 2014 Apple, Inc.
   All rights reserved. */

#ifndef CIKERNEL_H
#define CIKERNEL_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <CoreImage/CIImage.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* Block callback used by Core Image to ask what rectangles of a kernel's input images
 * are needed to produce a desired rectangle of the kernel's output image.
 *
 * 'index' is the 0-based index specifying which of the kernel's input images is being queried.
 * 'destRect' is the extent rectangle of kernel's output image being queried.
 *
 * Returns the rectangle of the index'th input image that is needed to produce destRect.
 * Returning CGRectNull indicates that the index'th input image is not needed to produce destRect.
 * The returned rectangle need not be contained by the extent of the index'th input image.
 */
typedef CGRect (^CIKernelROICallback)(int index, CGRect destRect);


@class CIImage;


/*
 * CIKernel is an object that encapsulates a Core Image Kernel Language
 * routine that generates a new images based on input images and agruments.
 *
 * General kernel functions are declared akin to this example:
 *   kernel vec4 myColorKernel (sampler fore, sampler back, vec4 params)
 *
 * The function must take a sampler argument for each input image.
 * Additional arguments can be of type float, vec2, vec3, vec4, or __color.
 * The destination pixel location is obtained by calling destCoord().
 * The kernel should call sample() with coordinates based on either
 * samplerCoord() or samplerTransform() to read pixel values from input images.
 * The function must return a vec4 pixel color.
 */
NS_CLASS_AVAILABLE(10_4, 8_0) NS_SWIFT_SENDABLE
@interface CIKernel : NSObject
{
    void *_priv;
}

/* The string argument should contain a program in the Core Image Kernel Language.
 * All the kernel functions in the program are converted to instances of a CIKernel objects
 * and returned in an array.
 * On OSX 10.10 and before, the array will contain instances of CIKernel class.
 * On OSX after 10.10, the array will contain instances of CIKernel, CIColorKernel or CIWarpKernel classes.
 * On iOS, the array will contain instances of CIKernel, CIColorKernel or CIWarpKernel classes.
 */
+ (nullable NSArray<CIKernel *> *)kernelsWithString:(NSString *)string  CIKL_DEPRECATED(10_4,10_14, 8_0,12_0);

/* The string argument should contain a program in the Metal Language.
 * CIKernel objects will be returned for each valid Metal function.
 * To be valid, the Metal function must be stitchable (i.e. attributed using [[stitchable]]) and
 * conform to the expected calling conventions for a CIKernel, CIColorKernel, CIBlendKernel, or CIWarpKernel.
 * The array will contain instances of CIKernel, CIColorKernel or CIWarpKernel classes.
 * The kernels will only be usable on Metal-backed CIContext on a device that 'supportsDynamicLibraries'
 */
+ (nullable NSArray<CIKernel *> *)kernelsWithMetalString:(NSString *)source error:(NSError **)error NS_AVAILABLE(12_0, 15_0);

/* The string argument should contain a program with one kernel.
 * On OSX 10.10 and before, this returns a CIKernel object.
 * On OSX after 10.10, this returns a CIKernel, CIColorKernel, or CIWarpKernel object.
 * On iOS this returns a CIKernel, CIColorKernel, or CIWarpKernel object.
 */
+ (nullable instancetype)kernelWithString:(NSString *)string  CIKL_DEPRECATED(10_10,10_14, 8_0,12_0);

/* The data argument should represent a metallib file compiled with the Core Image Standard Library
 * and contain the given function written in the Metal Shading Language.
 *
 * An optional output pixel format can be specified, and would be used if the output of the kernel
 * needs to be written to an intermediate texture.
 */
+ (nullable instancetype)kernelWithFunctionName:(NSString *)name
                           fromMetalLibraryData:(NSData *)data
                                          error:(NSError **)error NS_AVAILABLE(10_13, 11_0);

+ (nullable instancetype)kernelWithFunctionName:(NSString *)name
                           fromMetalLibraryData:(NSData *)data
                              outputPixelFormat:(CIFormat)format
                                          error:(NSError **)error NS_AVAILABLE(10_13, 11_0);

// This method will return an array of strings corresponding to names of all of the kernels
// contained within the underlying Metal library in the associated NSData.
+ (NSArray<NSString *> *)kernelNamesFromMetalLibraryData:(NSData *)data NS_AVAILABLE(11_0,14_0);


/* The name of the kernel. */
@property (atomic, readonly) NSString *name  NS_AVAILABLE(10_4, 8_0);

/* Sets the selector used by Core Image to ask what rectangles of a kernel's input images
 * are needed to produce a desired rectangle of the kernel's output image.
 *
 * Using setROISelector: is suppoted but not recommended.
 * The selector is only used if one the [CIFilter apply:...] methods is used.
 * Instead, use one of the [CIKernel applyWithExtent:roiCallback:...] methods.
 *
 * The method should have one of the following signatures:
 *  - (CGRect)regionOf:(int)samplerIndex destRect:(CGRect)r userInfo:obj;
 *  - (CGRect)regionOf:(int)samplerIndex destRect:(CGRect)r;
 *
 * 'samplerIndex' is the 0-based index specifying which of the kernel's input images is being queried.
 * 'destRect' is the extent rectangle of kernel's output image being queried.
 * 'userInfo' is the object associated with the kCIApplyOptionUserInfo when the kernel was applied.
 *
 * The method should return the rectangle of the index'th input image that is needed to produce destRect.
 * Returning CGRectNull indicates that the index'th input image is not needed to produce destRect.
 */
- (void)setROISelector:(SEL)method  NS_AVAILABLE(10_4, 9_0);

/* Apply the receiver CIKernel to produce a new CIImage object.
 *
 * The 'extent' is the bounding box of all non-clear pixels produced by the kernel.
 *
 * The 'callback' is a block that should return the rectangle of each input image
 * that is needed to produce a given rectangle in the coordinate space of the
 * new image.
 *
 * The 'args' is an array of parameters needed to describe the new image.
 * The object types of the items in the array correspond to the argument types of the
 * kernel function.  For example, if the first argument in the kernel is a sampler,
 * then the first object in the array must be a CIImage.
 */
- (nullable CIImage *)applyWithExtent:(CGRect)extent
                          roiCallback:(CIKernelROICallback)callback
                            arguments:(nullable NSArray<id> *)args  NS_AVAILABLE(10_11, 8_0);
@end



/*
 * CIColorKernel is an object that encapsulates a Core Image Kernel Language
 * routine that processes only the color information in images.
 *
 * Color kernel functions are declared akin to this example:
 *   kernel vec4 myColorKernel (__sample fore, __sample back, vec4 params)
 *
 * The function must take a __sample argument for each input image.
 * Additional arguments can be of type float, vec2, vec3, vec4, or __color.
 * The destination pixel location is obtained by calling destCoord().
 * The kernel should not call sample(), samplerCoord(), or samplerTransform().
 * The function must return a vec4 pixel color.
 */
NS_CLASS_AVAILABLE(10_11, 8_0) NS_SWIFT_SENDABLE
@interface CIColorKernel : CIKernel
{
}

/* The string argument should contain a program with one color kernel.
 * On iOS8 [CIColorKernel kernelWithString:] can return a CIKernel, CIColorKernel, or CIWarpKernel object.
 * On iOS9 [CIColorKernel kernelWithString:] will return a CIColorKernel object or nil.
 * On OS X [CIColorKernel kernelWithString:] will return a CIColorKernel object or nil.
 */
+ (nullable instancetype)kernelWithString:(NSString *)string  CIKL_DEPRECATED(10_10,10_14, 8_0,12_0);

/* Apply the receiver CIColorKernel to produce a new CIImage object.
 *
 * The 'extent' is the bounding box of all non-clear pixels produced by the kernel.
 *
 * The 'args' is an array of parameters needed to describe the new image.
 * The object types of the items in the array correspond to the argument types of the
 * kernel function.  For example, if the first argument in the kernel is a __sample,
 * then the first object in the array must be a CIImage.
 */
- (nullable CIImage *)applyWithExtent:(CGRect)extent
                            arguments:(nullable NSArray<id> *)args;
@end


/*
 * CIWarpKernel is an object that encapsulates a Core Image Kernel Language
 * function that processes only the geometry of an image.
 *
 * Warp kernel functions are declared akin to this example:
 *   kernel vec2 myWarpKernel (vec4 params)
 *
 * Additional arguments can be of type float, vec2, vec3, vec4.
 * The destination pixel location is obtained by calling destCoord().
 * The kernel should not call sample(), samplerCoord(), or samplerTransform().
 * The function must return a vec2 source location.
 */
NS_CLASS_AVAILABLE(10_11, 8_0) NS_SWIFT_SENDABLE
@interface CIWarpKernel : CIKernel
{
}

/* The string argument should contain a program with one warp kernel.
 * On iOS8 [CIWarpKernel kernelWithString:] can return a CIKernel, CIColorKernel, or CIWarpKernel object.
 * On iOS9 [CIWarpKernel kernelWithString:] will return a CIWarpKernel object or nil.
 * On OS X [CIWarpKernel kernelWithString:] will return a CIWarpKernel object or nil.
 */
+ (nullable instancetype)kernelWithString:(NSString *)string  CIKL_DEPRECATED(10_10,10_14, 8_0,12_0);

/* Apply the receiver CIWarpKernel to produce a new CIImage object.
 *
 * The 'extent' is the bounding box of all pixel coordinates that are warped by the
 * kernel to fall within the extent of 'image'.
 *
 * The 'image' is the input image that will be warped into a new image.
 *
 * The 'callback' is a block that should return the rectangle of the input image
 * that is needed to produce a given rectangle in the coordinate space of the
 * new image.
 *
 * The 'args' is an array of parameters needed to describe the warping function.
 * The object types of the items in the array correspond to the argument types of the
 * kernel function.  For example, if the first argument in the kernel is a vec3,
 * then the first object in the array must be a CIVector of count 3.
 */
- (nullable CIImage *)applyWithExtent:(CGRect)extent
                          roiCallback:(CIKernelROICallback)callback
                           inputImage:(CIImage*)image
                            arguments:(nullable NSArray<id> *)args;
@end


/* CIBlendKernel is a special type of color kernel that blends two images. 
 *
 * Blend kernel functions are declared akin to this example:
 *   kernel vec4 myBlendKernel (__sample fore, __sample back)
 *
 * A blend kernel function must have exactly two arguments of type __sample.
 * The first argument represents the value of the source pixel and the second 
 * represents that of the old destination. The vec4 returned by the kernel will 
 * be the new destination color.
 * The kernel should not call sample(), samplerCoord(), or samplerTransform().
 * The function must return a vec4 pixel color.
 */
NS_CLASS_AVAILABLE(10_13, 11_0) NS_SWIFT_SENDABLE
@interface CIBlendKernel : CIColorKernel
{
}

/* The string argument should contain a program with one custom blend kernel. */
+ (nullable instancetype)kernelWithString:(NSString *)string  CIKL_DEPRECATED(10_10,10_14, 8_0,12_0);

/* Apply the receiver CIBlendKernel to produce a new CIImage object
 * by blending a foreground and background images.
 *
 * The 'extent' of the result image will be determined by the reciver and
 * the extent of the forground and background images.  For most of the
 * builtin blend kernels (as well as custom blend kernels) the result image
 * extent will be the union of the forground and background image
 * extents.
 */
- (nullable CIImage *)applyWithForeground:(CIImage*)foreground
                               background:(CIImage*)background;

/* Apply the receiver CIBlendKernel to produce a new CIImage object
 * by blending a foreground and background images in the specifid colorspace.
 *
 * The 'extent' of the result image will be determined by the reciver and
 * the extent of the forground and background images.  For most of the
 * builtin blend kernels (as well as custom blend kernels) the result image
 * extent will be the union of the forground and background image
 * extents.
 */
- (nullable CIImage *)applyWithForeground:(CIImage*)foreground
                               background:(CIImage*)background
                               colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_15, 13_0);

@end

@interface CIBlendKernel (BuiltIn)

/* Core Image builtin blend kernels */

/* Component-wise operators */

@property (class, strong, readonly) CIBlendKernel *componentAdd;
@property (class, strong, readonly) CIBlendKernel *componentMultiply;
@property (class, strong, readonly) CIBlendKernel *componentMin;
@property (class, strong, readonly) CIBlendKernel *componentMax;

/* Porter Duff
 * http://dl.acm.org/citation.cfm?id=808606
 */
@property (class, strong, readonly) CIBlendKernel *clear;
@property (class, strong, readonly) CIBlendKernel *source;
@property (class, strong, readonly) CIBlendKernel *destination;
@property (class, strong, readonly) CIBlendKernel *sourceOver;
@property (class, strong, readonly) CIBlendKernel *destinationOver;
@property (class, strong, readonly) CIBlendKernel *sourceIn;
@property (class, strong, readonly) CIBlendKernel *destinationIn;
@property (class, strong, readonly) CIBlendKernel *sourceOut;
@property (class, strong, readonly) CIBlendKernel *destinationOut;
@property (class, strong, readonly) CIBlendKernel *sourceAtop;
@property (class, strong, readonly) CIBlendKernel *destinationAtop;
@property (class, strong, readonly) CIBlendKernel *exclusiveOr;

/* PDF 1.7 blend modes
 * https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf
 */

/* Standard separable blend modes */
@property (class, strong, readonly) CIBlendKernel *multiply;
@property (class, strong, readonly) CIBlendKernel *screen;
@property (class, strong, readonly) CIBlendKernel *overlay;
@property (class, strong, readonly) CIBlendKernel *darken;
@property (class, strong, readonly) CIBlendKernel *lighten;
@property (class, strong, readonly) CIBlendKernel *colorDodge;
@property (class, strong, readonly) CIBlendKernel *colorBurn;
@property (class, strong, readonly) CIBlendKernel *hardLight;
@property (class, strong, readonly) CIBlendKernel *softLight;
@property (class, strong, readonly) CIBlendKernel *difference;
@property (class, strong, readonly) CIBlendKernel *exclusion;

/* Standard nonseparable blend modes */
@property (class, strong, readonly) CIBlendKernel *hue;
@property (class, strong, readonly) CIBlendKernel *saturation;
@property (class, strong, readonly) CIBlendKernel *color;
@property (class, strong, readonly) CIBlendKernel *luminosity;

/* Additional blend modes */
@property (class, strong, readonly) CIBlendKernel *subtract;
@property (class, strong, readonly) CIBlendKernel *divide;
@property (class, strong, readonly) CIBlendKernel *linearBurn;
@property (class, strong, readonly) CIBlendKernel *linearDodge;
@property (class, strong, readonly) CIBlendKernel *vividLight;
@property (class, strong, readonly) CIBlendKernel *linearLight;
@property (class, strong, readonly) CIBlendKernel *pinLight;
@property (class, strong, readonly) CIBlendKernel *hardMix;
@property (class, strong, readonly) CIBlendKernel *darkerColor;
@property (class, strong, readonly) CIBlendKernel *lighterColor;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIKERNEL_H */
