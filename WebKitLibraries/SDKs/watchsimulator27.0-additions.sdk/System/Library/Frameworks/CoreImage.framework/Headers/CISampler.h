/* 
   CoreImage - CISampler.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved.
*/

#ifndef CISAMPLER_H
#define CISAMPLER_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CIFilterShape, CIImage;

NS_CLASS_AVAILABLE(10_4, 9_0)
@interface CISampler : NSObject <NSCopying>
{
    void *_priv;
}

/* Creates a new sampler object refencing 'im'. */
+ (instancetype)samplerWithImage:(CIImage *)im;

/* Creates a new sampler object from 'im' specifying key/value option
 * pairs. Each key is an NSString. Supported keys include:
 *
 * kCISamplerAffineMatrix: An NSArray [a b c d tx ty] defining the
 * transformation to be applied to the sampler.
 *
 * kCISamplerWrapMode: An NSString defining how pixels outside the
 * sampler's extent are produced. Options include kCISamplerWrapBlack
 * (pixels are transparent black, the default) and kCISamplerWrapClamp
 * (coordinates are clamped to the extent).
 *
 * kCISamplerFilterMode: An NSString defining the filter to use when
 * sampling the image. One of kCISamplerFilterNearest (point sampling)
 * or kCISamplerFilterLinear (bilinear interpolation, the default). 
 */
+ (instancetype)samplerWithImage:(CIImage *)im keysAndValues:key0, ... NS_REQUIRES_NIL_TERMINATION;
+ (instancetype)samplerWithImage:(CIImage *)im options:(nullable NSDictionary *)dict;

/* Initializers. */

- (instancetype)initWithImage:(CIImage *)im;
- (instancetype)initWithImage:(CIImage *)im keysAndValues:key0, ...;
- (instancetype)initWithImage:(CIImage *)im options:(nullable NSDictionary *)dict NS_DESIGNATED_INITIALIZER;

/* Returns the shape containing the Domain Of Definition (DOD) of the
 * sampler. The DOD is defined such that it contains all non-transparent
 * pixels produced by referencing the sampler. 
 * This property is KVO-safe 
 */
@property (readonly) CIFilterShape * definition;

/* Returns the extent of the sampler. Sampling outside the extent will
 * bring the sampler's wrap mode into action. 
 */
@property (readonly) CGRect extent;

@end


/* Sampler options. */

CORE_IMAGE_EXPORT NSString *const kCISamplerAffineMatrix NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString *const kCISamplerWrapMode NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString *const kCISamplerFilterMode NS_AVAILABLE(10_4, 9_0);

CORE_IMAGE_EXPORT NSString *const kCISamplerWrapBlack NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString *const kCISamplerWrapClamp NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString *const kCISamplerFilterNearest NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString *const kCISamplerFilterLinear NS_AVAILABLE(10_4, 9_0);

/* If used, the value of the kCISamplerColorSpace key be must be an RGB CGColorSpaceRef.
 * Using this option specifies that samples should be converted to this color space before being passed to a kernel. 
 * If not specified, samples will be passed to the kernel in the working color space of the rendering CIContext. */
CORE_IMAGE_EXPORT NSString *const kCISamplerColorSpace NS_AVAILABLE(10_4, 9_0);

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CISAMPLER_H */
