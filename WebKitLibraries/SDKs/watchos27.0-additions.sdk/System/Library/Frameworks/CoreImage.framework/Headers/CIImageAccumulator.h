/* CoreImage - CIImageAccumulator.h

   Copyright (c) 2004 Apple, Inc.
   All rights reserved. */

#ifndef CIIMAGEACCUMULATOR_H
#define CIIMAGEACCUMULATOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <CoreImage/CIImage.h>

NS_ASSUME_NONNULL_BEGIN

NS_CLASS_AVAILABLE(10_4, 9_0)
@interface CIImageAccumulator : NSObject
{
    void *_state;
}

/* Create a new accumulator object. 
   For pixel format options see CIImage.h.
   The specified color space is used to render the image. 
   If no color space is specified, no color matching is done. 
   The return values will be null if the format is unsupported or the extent is too big.
*/
+ (nullable instancetype)imageAccumulatorWithExtent:(CGRect)extent
                                             format:(CIFormat)format;

+ (nullable instancetype)imageAccumulatorWithExtent:(CGRect)extent
                                             format:(CIFormat)format
                                         colorSpace:(CGColorSpaceRef)colorSpace
NS_AVAILABLE(10_7, 9_0);

- (nullable instancetype)initWithExtent:(CGRect)extent
                                 format:(CIFormat)format;

- (nullable instancetype)initWithExtent:(CGRect)extent
                                 format:(CIFormat)format
                             colorSpace:(CGColorSpaceRef)colorSpace
NS_AVAILABLE(10_7, 9_0);

/* Return the extent of the accumulator. */
@property (readonly) CGRect extent;

/* Return the pixel format of the accumulator. */
@property (readonly) CIFormat format;

/* Return an image representing the current contents of the accumulator.
 * Rendering the image after subsequently calling setImage: has
 * undefined behavior. */
- (CIImage *)image;

/* Set the image 'im' as the current contents of the accumulator. */
- (void)setImage:(CIImage *)image;

/* Set the image 'im' as the accumulator's contents. The caller guarantees
 * that the new contents only differ from the old within the specified
 * region. */
- (void)setImage:(CIImage *)image dirtyRect:(CGRect)dirtyRect;

/* Reset the accumulator, discarding any pending updates and current content. */
- (void)clear;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIIMAGEACCUMULATOR_H */
