#ifndef CIFILTERBUILTINS_H
#define CIFILTERBUILTINS_H

#ifdef __OBJC__

#import <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH

#import <CoreImage/CIFilter.h>
@class CIVector;
@class CIColor;
@class CIBarcodeDescriptor;
@class MLModel;

NS_ASSUME_NONNULL_BEGIN

// CICategoryGradient

/// The protocol for the Distance Gradient From Red Mask filter.
///
/// Produces an infinite image where the red channel contains the distance in pixels from each pixel to the mask.
@protocol CIDistanceGradientFromRedMask <CIFilter>
  /// The input image whose red channel defines a mask. If the red channel pixel value is greater than 0.5 then the point is considered in the mask and output pixel will be zero. Otherwise the output pixel will be a value between zero and one.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Determines the maximum distance to the mask that can be measured. Distances between zero and the maximum will be normalized to zero and one.
  @property (nonatomic) float maximumDistance;
@end

/// The protocol for the Gaussian Gradient filter.
///
/// Generates a gradient that varies from one color to another using a Gaussian distribution.
@protocol CIGaussianGradient <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The first color to use in the gradient.
  @property (nonatomic, retain) CIColor *color0;
  /// The second color to use in the gradient.
  @property (nonatomic, retain) CIColor *color1;
  /// The radius of the Gaussian distribution.
  @property (nonatomic) float radius;
@end

/// The protocol for the Hue/Saturation/Value Gradient filter.
///
/// Generates a color wheel that shows hues and saturations for a specified value.
@protocol CIHueSaturationValueGradient <CIFilter>
  /// The color value used to generate the color wheel.
  @property (nonatomic) float value;
  /// The radius of the color wheel in pixels.
  @property (nonatomic) float radius;
  /// The amount to smooth the hue angle to make it appear more continuous.
  @property (nonatomic) float softness;
  /// The amount of dithered noise to add to the color wheel to avoid banding artifacts.
  @property (nonatomic) float dither;
  /// The CGColorSpaceRef that the color wheel should be generated in.
  @property (nonatomic, nullable) CGColorSpaceRef colorSpace;
@end

/// The protocol for the Linear Gradient filter.
///
/// Generates a gradient that varies along a linear axis between two defined endpoints.
@protocol CILinearGradient <CIFilter>
  /// The starting position of the gradient -- where the first color begins.
  @property (nonatomic) CGPoint point0;
  /// The ending position of the gradient -- where the second color begins.
  @property (nonatomic) CGPoint point1;
  /// The first color to use in the gradient.
  @property (nonatomic, retain) CIColor *color0;
  /// The second color to use in the gradient.
  @property (nonatomic, retain) CIColor *color1;
@end

/// The protocol for the Radial Gradient filter.
///
/// Generates a gradient that varies radially between two circles having the same center. It is valid for one of the two circles to have a radius of 0.
@protocol CIRadialGradient <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius of the starting circle to use in the gradient.
  @property (nonatomic) float radius0;
  /// The radius of the ending circle to use in the gradient.
  @property (nonatomic) float radius1;
  /// The first color to use in the gradient.
  @property (nonatomic, retain) CIColor *color0;
  /// The second color to use in the gradient.
  @property (nonatomic, retain) CIColor *color1;
@end

/// The protocol for the Signed Distance Gradient From Red Mask filter.
///
/// Produces an infinite image where the red channel contains the distance in pixels from each pixel to the mask.
@protocol CISignedDistanceGradientFromRedMask <CIFilter>
  /// The input image whose red channel defines a mask. If the red channel pixel value is greater than 0.5 then the point is considered in the mask and output pixel will be a value between zero and negative one. Otherwise the output pixel will be a value between zero and one.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Determines the maximum distance to the mask that can be measured. Distances between zero and the maximum will be normalized to negative one and one.
  @property (nonatomic) float maximumDistance;
@end

/// The protocol for the Smooth Linear Gradient filter.
///
/// Generates a gradient that varies along a linear axis between two defined endpoints.
@protocol CISmoothLinearGradient <CIFilter>
  /// The starting position of the gradient -- where the first color begins.
  @property (nonatomic) CGPoint point0;
  /// The ending position of the gradient -- where the second color begins.
  @property (nonatomic) CGPoint point1;
  /// The first color to use in the gradient.
  @property (nonatomic, retain) CIColor *color0;
  /// The second color to use in the gradient.
  @property (nonatomic, retain) CIColor *color1;
@end

// CICategorySharpen

/// The protocol for the Sharpen Luminance filter.
///
/// Increases image detail by sharpening. It operates on the luminance of the image; the chrominance of the pixels remains unaffected.
@protocol CISharpenLuminance <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount of sharpening to apply. Larger values are sharper.
  @property (nonatomic) float sharpness;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
@end

/// The protocol for the Unsharp Mask filter.
///
/// Increases the contrast of the edges between pixels of different colors in an image.
@protocol CIUnsharpMask <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius around a given pixel to apply the unsharp mask. The larger the radius, the more of the image is affected.
  @property (nonatomic) float radius;
  /// The intensity of the effect. The larger the value, the more contrast in the affected area.
  @property (nonatomic) float intensity;
@end

// CICategoryHalftoneEffect

/// The protocol for the Circular Screen filter.
///
/// Simulates a circular-shaped halftone screen.
@protocol CICircularScreen <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The distance between each circle in the pattern.
  @property (nonatomic) float width;
  /// The sharpness of the circles. The larger the value, the sharper the circles.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the CMYK Halftone filter.
///
/// Creates a color, halftoned rendition of the source image, using cyan, magenta, yellow, and black inks over a white page.
@protocol CICMYKHalftone <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The distance between dots in the pattern.
  @property (nonatomic) float width;
  /// The angle in radians of the pattern.
  @property (nonatomic) float angle;
  /// The sharpness of the pattern. The larger the value, the sharper the pattern.
  @property (nonatomic) float sharpness;
  /// The gray component replacement value. The value can vary from 0.0 (none) to 1.0.
  @property (nonatomic) float grayComponentReplacement;
  /// The under color removal value. The value can vary from 0.0 to 1.0. 
  @property (nonatomic) float underColorRemoval;
@end

/// The protocol for the Dot Screen filter.
///
/// Simulates the dot patterns of a halftone screen.
@protocol CIDotScreen <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the pattern.
  @property (nonatomic) float angle;
  /// The distance between dots in the pattern.
  @property (nonatomic) float width;
  /// The sharpness of the pattern. The larger the value, the sharper the pattern.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the Hatched Screen filter.
///
/// Simulates the hatched pattern of a halftone screen.
@protocol CIHatchedScreen <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the pattern.
  @property (nonatomic) float angle;
  /// The distance between lines in the pattern.
  @property (nonatomic) float width;
  /// The amount of sharpening to apply.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the Line Screen filter.
///
/// Simulates the line pattern of a halftone screen.
@protocol CILineScreen <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the pattern.
  @property (nonatomic) float angle;
  /// The distance between lines in the pattern.
  @property (nonatomic) float width;
  /// The sharpness of the pattern. The larger the value, the sharper the pattern.
  @property (nonatomic) float sharpness;
@end

// CICategoryGeometryAdjustment

/// A protocol for filters that adjust the geometry of an image
/// given four control points in the coordinate space of the image.
@protocol CIFourCoordinateGeometryFilter <CIFilter>
  /// The image to use as the input for the geometry adjustment filter.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The top left point in the coordinate space of the image.
  @property (nonatomic) CGPoint topLeft;
  /// The top right point in the coordinate space of the image.
  @property (nonatomic) CGPoint topRight;
  /// The bottom right point in the coordinate space of the image.
  @property (nonatomic) CGPoint bottomRight;
  /// The bottom left point in the coordinate space of the image.
  @property (nonatomic) CGPoint bottomLeft;
@end

/// The protocol for the Bicubic Scale Transform filter.
///
/// Produces a high-quality, scaled version of a source image. The parameters of B and C for this filter determine the sharpness or softness of the resampling. The most commonly used B and C values are 0.0 and 0.75, respectively.
@protocol CIBicubicScaleTransform <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The scaling factor to use on the image. Values less than 1.0 scale down the images. Values greater than 1.0 scale up the image.
  @property (nonatomic) float scale;
  /// The additional horizontal scaling factor to use on the image.
  @property (nonatomic) float aspectRatio;
  /// Specifies the value of B to use for the cubic resampling function.
  @property (nonatomic) float parameterB;
  /// Specifies the value of C to use for the cubic resampling function.
  @property (nonatomic) float parameterC;
@end

/// The protocol for the Edge Preserve Upsample Filter filter.
///
/// Upsamples a small image to the size of the input image using the luminance of the input image as a guide to preserve detail.
@protocol CIEdgePreserveUpsample <CIFilter>
  /// The image to use as a guide to upsample the small image.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image that the filter upsamples.
  @property (nonatomic, retain, nullable) CIImage *smallImage;
  /// A value that specifies the influence of the input image’s spatial information on the upsampling operation.
  @property (nonatomic) float spatialSigma;
  /// A value that specifies the influence of the input image’s luma information on the upsampling operation.
  @property (nonatomic) float lumaSigma;
@end

/// The protocol for the Combined Keystone Correction filter.
///
/// Apply keystone correction to an image with combined horizontal and vertical guides.
@protocol CIKeystoneCorrectionCombined <CIFourCoordinateGeometryFilter>
  /// 35mm equivalent focal length of the input image.
  @property (nonatomic) float focalLength;
@end

/// The protocol for the Horizontal Keystone Correction filter.
///
/// Apply horizontal keystone correction to an image with guides.
@protocol CIKeystoneCorrectionHorizontal <CIFourCoordinateGeometryFilter>
  /// 35mm equivalent focal length of the input image.
  @property (nonatomic) float focalLength;
@end

/// The protocol for the Vertical Keystone Correction filter.
///
/// Apply vertical keystone correction to an image with guides.
@protocol CIKeystoneCorrectionVertical <CIFourCoordinateGeometryFilter>
  /// 35mm equivalent focal length of the input image.
  @property (nonatomic) float focalLength;
@end

/// The protocol for the Lanczos Scale Transform filter.
///
/// Produces a high-quality, scaled version of a source image. You typically use this filter to scale down an image.
@protocol CILanczosScaleTransform <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The scaling factor to use on the image. Values less than 1.0 scale down the images. Values greater than 1.0 scale up the image.
  @property (nonatomic) float scale;
  /// The additional horizontal scaling factor to use on the image.
  @property (nonatomic) float aspectRatio;
@end

/// The protocol for the Maximum Scale Transform filter.
///
/// Produces a scaled version of a source image that uses the maximum of neighboring pixels instead of linear averaging.
@protocol CIMaximumScaleTransform <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The scaling factor to use on the image. Values less than 1.0 scale down the images. Values greater than 1.0 scale up the image.
  @property (nonatomic) float scale;
  /// The additional horizontal scaling factor to use on the image.
  @property (nonatomic) float aspectRatio;
@end

/// The protocol for the Perspective Correction filter.
///
/// Apply a perspective correction to an image. This geometry maps four control points in the input image to a rectangle.
@protocol CIPerspectiveCorrection <CIFourCoordinateGeometryFilter>
  /// If true then the filter crops the output image to the rectangle that the points map to. If false, then pixels from outside the four control points remain in the output image
  @property (nonatomic) bool crop;
@end

/// The protocol for the Perspective Rotate filter.
///
/// Apply a homogenous rotation transform to an image.
@protocol CIPerspectiveRotate <CIFilter>
  /// The image to process.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// 35mm equivalent focal length of the input image.
  @property (nonatomic) float focalLength;
  /// Pitch angle in radians.
  @property (nonatomic) float pitch;
  /// Yaw angle in radians.
  @property (nonatomic) float yaw;
  /// Roll angle in radians.
  @property (nonatomic) float roll;
@end

/// The protocol for the Perspective Transform filter.
///
/// Alters the geometry of an image to simulate the observer changing viewing position. You can use the perspective filter to skew an image.
@protocol CIPerspectiveTransform <CIFourCoordinateGeometryFilter>
@end

/// The protocol for the Perspective Transform with Extent filter.
///
/// Alters the geometry of an image to simulate the observer changing viewing position. You can use the perspective filter to skew an image.
@protocol CIPerspectiveTransformWithExtent <CIFourCoordinateGeometryFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
@end

/// The protocol for the Straighten filter.
///
/// Rotates a source image by the specified angle in radians. The image is then scaled and cropped so that the rotated image fits the extent of the input image.
@protocol CIStraighten <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The angle in radians of the effect.
  @property (nonatomic) float angle;
@end

// CICategoryTransition

/// A protocol for filters that perform an animatable transition between two images.
@protocol CITransitionFilter <CIFilter>
  /// The image to use at the start of the transition.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image to use at the end of the transition.
  @property (nonatomic, retain, nullable) CIImage *targetImage;
  /// The parametric time of the transition between `0` and `1`.
  @property (nonatomic) float time;
@end

/// The protocol for the Accordion Fold Transition filter.
///
/// Transitions from one image to another of a differing dimensions by unfolding.
@protocol CIAccordionFoldTransition <CITransitionFilter>
  /// The height in pixels from the bottom of the image to the bottom of the folded part of the transition.
  @property (nonatomic) float bottomHeight;
  /// The number of folds used in the transition.
  @property (nonatomic) float numberOfFolds;
  /// A value that specifies the intensity of the shadow in the transition.
  @property (nonatomic) float foldShadowAmount;
@end

/// The protocol for the Bars Swipe Transition filter.
///
/// Transitions from one image to another by swiping rectangular portions of the foreground image to disclose the target image.
@protocol CIBarsSwipeTransition <CITransitionFilter>
  /// The angle in radians of the bars.
  @property (nonatomic) float angle;
  /// The width of each bar.
  @property (nonatomic) float width;
  /// The offset of one bar with respect to another.
  @property (nonatomic) float barOffset;
@end

/// The protocol for the Copy Machine filter.
///
/// Transitions from one image to another by simulating the effect of a copy machine.
@protocol CICopyMachineTransition <CITransitionFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The color of the copier light.
  @property (nonatomic, retain) CIColor *color;
  /// The angle in radians of the copier light.
  @property (nonatomic) float angle;
  /// The width of the copier light. 
  @property (nonatomic) float width;
  /// The opacity of the copier light. A value of 0.0 is transparent. A value of 1.0 is opaque.
  @property (nonatomic) float opacity;
@end

/// The protocol for the Disintegrate With Mask filter.
///
/// Transitions from one image to another using the shape defined by a mask.
@protocol CIDisintegrateWithMaskTransition <CITransitionFilter>
  /// An image that defines the shape to use when disintegrating from the source to the target image.
  @property (nonatomic, retain, nullable) CIImage *maskImage;
  /// The radius of the shadow created by the mask.
  @property (nonatomic) float shadowRadius;
  /// The density of the shadow created by the mask.
  @property (nonatomic) float shadowDensity;
  /// The offset of the shadow created by the mask.
  @property (nonatomic) CGPoint shadowOffset;
@end

/// The protocol for the Dissolve filter.
///
/// Uses a dissolve to transition from one image to another.
@protocol CIDissolveTransition <CITransitionFilter>
@end

/// The protocol for the Flash filter.
///
/// Transitions from one image to another by creating a flash. The flash originates from a point you specify. Small at first, it rapidly expands until the image frame is completely filled with the flash color. As the color fades, the target image begins to appear.
@protocol CIFlashTransition <CITransitionFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The extent of the flash.
  @property (nonatomic) CGRect extent;
  /// The color of the light rays emanating from the flash.
  @property (nonatomic, retain) CIColor *color;
  /// The radius of the light rays emanating from the flash.
  @property (nonatomic) float maxStriationRadius;
  /// The strength of the light rays emanating from the flash.
  @property (nonatomic) float striationStrength;
  /// The contrast of the light rays emanating from the flash.
  @property (nonatomic) float striationContrast;
  /// The amount of fade between the flash and the target image. The higher the value, the more flash time and the less fade time.
  @property (nonatomic) float fadeThreshold;
@end

/// The protocol for the Mod filter.
///
/// Transitions from one image to another by revealing the target image through irregularly shaped holes.
@protocol CIModTransition <CITransitionFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the mod hole pattern.
  @property (nonatomic) float angle;
  /// The radius of the undistorted holes in the pattern.
  @property (nonatomic) float radius;
  /// The amount of stretching applied to the mod hole pattern. Holes in the center are not distorted as much as those at the edge of the image.
  @property (nonatomic) float compression;
@end

/// The protocol for the Page Curl filter.
///
/// Transitions from one image to another by simulating a curling page, revealing the new image as the page curls.
@protocol CIPageCurlTransition <CITransitionFilter>
  /// The image that appears on the back of the source image, as the page curls to reveal the target image.
  @property (nonatomic, retain, nullable) CIImage *backsideImage;
  /// An image that looks like a shaded sphere enclosed in a square image.
  @property (nonatomic, retain, nullable) CIImage *shadingImage;
  /// The extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The angle in radians of the curling page.
  @property (nonatomic) float angle;
  /// The radius of the curl.
  @property (nonatomic) float radius;
@end

/// The protocol for the Page Curl With Shadow filter.
///
/// Transitions from one image to another by simulating a curling page, revealing the new image as the page curls.
@protocol CIPageCurlWithShadowTransition <CITransitionFilter>
  /// The image that appears on the back of the source image, as the page curls to reveal the target image.
  @property (nonatomic, retain, nullable) CIImage *backsideImage;
  /// The extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The angle in radians of the curling page.
  @property (nonatomic) float angle;
  /// The radius of the curl.
  @property (nonatomic) float radius;
  /// The maximum size in pixels of the shadow.
  @property (nonatomic) float shadowSize;
  /// The strength of the shadow.
  @property (nonatomic) float shadowAmount;
  /// The rectangular portion of input image that will cast a shadow.
  @property (nonatomic) CGRect shadowExtent;
@end

/// The protocol for the Ripple filter.
///
/// Transitions from one image to another by creating a circular wave that expands from the center point, revealing the new image in the wake of the wave.
@protocol CIRippleTransition <CITransitionFilter>
  /// An image that looks like a shaded sphere enclosed in a square image.
  @property (nonatomic, retain, nullable) CIImage *shadingImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The width of the ripple.
  @property (nonatomic) float width;
  /// A value that determines whether the ripple starts as a bulge (higher value) or a dimple (lower value).
  @property (nonatomic) float scale;
@end

/// The protocol for the Swipe filter.
///
/// Transitions from one image to another by simulating a swiping action.
@protocol CISwipeTransition <CITransitionFilter>
  /// The extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The color of the swipe.
  @property (nonatomic, retain) CIColor *color;
  /// The angle in radians of the swipe.
  @property (nonatomic) float angle;
  /// The width of the swipe.
  @property (nonatomic) float width;
  /// The opacity of the swipe.
  @property (nonatomic) float opacity;
@end

// CICategoryCompositeOperation

/// The protocol for the Addition filter.
///
/// Adds color components to achieve a brightening effect. This filter is typically used to add highlights and lens flare effects.
@protocol CICompositeOperation <CIFilter>
  /// The image to use as a foreground image.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image to use as a background image.
  @property (nonatomic, retain, nullable) CIImage *backgroundImage;
@end

// CICategoryColorAdjustment

/// The protocol for the Color Absolute Difference filter.
///
/// Produces an image that is the absolute value of the color difference between two images. The alpha channel of the result will be the product of the two image alpha channels.
@protocol CIColorAbsoluteDifference <CIFilter>
  /// The first input image for differencing.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The second input image for differencing.
  @property (nonatomic, retain, nullable) CIImage *inputImage2;
@end

/// The protocol for the Color Clamp filter.
///
/// Clamp color to a certain range.
@protocol CIColorClamp <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Lower clamping values.
  @property (nonatomic, retain) CIVector *minComponents;
  /// Higher clamping values.
  @property (nonatomic, retain) CIVector *maxComponents;
@end

/// The protocol for the Color Controls filter.
///
/// Adjusts saturation, brightness, and contrast values.
@protocol CIColorControls <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount of saturation to apply. The larger the value, the more saturated the result.
  @property (nonatomic) float saturation;
  /// The amount of brightness to apply. The larger the value, the brighter the result.
  @property (nonatomic) float brightness;
  /// The amount of contrast to apply. The larger the value, the more contrast in the resulting image.
  @property (nonatomic) float contrast;
@end

/// The protocol for the Color Matrix filter.
///
/// Multiplies source color values and adds a bias factor to each color component.
@protocol CIColorMatrix <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount of red to multiply the source color values by.
  @property (nonatomic, retain) CIVector *RVector;
  /// The amount of green to multiply the source color values by.
  @property (nonatomic, retain) CIVector *GVector;
  /// The amount of blue to multiply the source color values by.
  @property (nonatomic, retain) CIVector *BVector;
  /// The amount of alpha to multiply the source color values by.
  @property (nonatomic, retain) CIVector *AVector;
  /// A vector that’s added to each color component.
  @property (nonatomic, retain) CIVector *biasVector;
@end

/// The protocol for the Color Polynomial filter.
///
/// Adjusts the color of an image with polynomials.
@protocol CIColorPolynomial <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Polynomial coefficients for red channel.
  @property (nonatomic, retain) CIVector *redCoefficients;
  /// Polynomial coefficients for green channel.
  @property (nonatomic, retain) CIVector *greenCoefficients;
  /// Polynomial coefficients for blue channel.
  @property (nonatomic, retain) CIVector *blueCoefficients;
  /// Polynomial coefficients for alpha channel.
  @property (nonatomic, retain) CIVector *alphaCoefficients;
@end

/// The protocol for the Color Threshold filter.
///
/// Produces a binarized image from an image and a threshold value. The red, green and blue channels of the resulting image will be one if its value is greater than the threshold and zero otherwise.
@protocol CIColorThreshold <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The threshold value that governs if the RGB channels of the resulting image will be zero or one.
  @property (nonatomic) float threshold;
@end

/// The protocol for the Color Threshold Otsu filter.
///
/// Produces a binarized image from an image with finite extent. The threshold is calculated from the image histogram using Otsu’s method. The red, green and blue channels of the resulting image will be one if its value is greater than the threshold and zero otherwise.
@protocol CIColorThresholdOtsu <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Depth To Disparity filter.
///
/// Convert a depth data image to disparity data.
@protocol CIDepthToDisparity <CIFilter>
  /// The input depth data image to convert to disparity data.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Disparity To Depth filter.
///
/// Convert a disparity data image to depth data.
@protocol CIDisparityToDepth <CIFilter>
  /// The input disparity data image to convert to depth data.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Exposure Adjust filter.
///
/// Adjusts the exposure setting for an image similar to the way you control exposure for a camera when you change the F-stop.
@protocol CIExposureAdjust <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount to adjust the exposure of the image by. The larger the value, the brighter the exposure.
  @property (nonatomic) float EV;
@end

/// The protocol for the Gamma Adjust filter.
///
/// Adjusts midtone brightness. This filter is typically used to compensate for nonlinear effects of displays. Adjusting the gamma effectively changes the slope of the transition between black and white.
@protocol CIGammaAdjust <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A gamma value to use to correct image brightness. The larger the value, the darker the result.
  @property (nonatomic) float power;
@end

/// The protocol for the Hue Adjust filter.
///
/// Changes the overall hue, or tint, of the source pixels.
@protocol CIHueAdjust <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// An angle in radians to use to correct the hue of an image.
  @property (nonatomic) float angle;
@end

/// The protocol for the Linear to sRGB Tone Curve filter.
///
/// Converts an image in linear space to sRGB space.
@protocol CILinearToSRGBToneCurve <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the sRGB Tone Curve to Linear filter.
///
/// Converts an image in sRGB space to linear space.
@protocol CISRGBToneCurveToLinear <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the System Tone Map filter.
///
/// Apply a global tone curve to an image that reduces colors of the input image to a desired dynamic range consistent with other frameworks.
@protocol CISystemToneMap <CIFilter>
  /// Specifies input image with content headroom and average light level properties.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Specifies the current headroom of the intended display.
  @property (nonatomic) float displayHeadroom;
  /// Specifies the preferred dynamic range behavior of the tone mapping. The value should be kCIDynamicRangeStandard, kCIDynamicRangeConstrainedHigh, kCIDynamicRangeHigh or nil.  If nil then it will behave as kCIDynamicRangeHigh.
  @property (nonatomic, retain, nullable) CIDynamicRangeOption preferredDynamicRange;
@end

/// The protocol for the Temperature and Tint filter.
///
/// Adapt the reference white point for an image.
@protocol CITemperatureAndTint <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A vector containing the source white point defined by color temperature and tint or chromaticity (x,y).
  @property (nonatomic, retain) CIVector *neutral;
  /// A vector containing the desired white point defined by color temperature and tint or chromaticity (x,y).
  @property (nonatomic, retain) CIVector *targetNeutral;
@end

/// The protocol for the Tone Curve filter.
///
/// Adjusts tone response of the R, G, and B channels of an image. The input points are five x,y values that are interpolated using a spline curve. The curve is applied in a perceptual (gamma 2) version of the working space.
@protocol CIToneCurve <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A vector containing the position of the first point of the tone curve.
  @property (nonatomic) CGPoint point0;
  /// A vector containing the position of the second point of the tone curve.
  @property (nonatomic) CGPoint point1;
  /// A vector containing the position of the third point of the tone curve.
  @property (nonatomic) CGPoint point2;
  /// A vector containing the position of the fourth point of the tone curve.
  @property (nonatomic) CGPoint point3;
  /// A vector containing the position of the fifth point of the tone curve.
  @property (nonatomic) CGPoint point4;
  /// If true, then the color effect will be extrapolated if the input image contains RGB component values outside the range 0.0 to 1.0.
  @property (nonatomic) bool extrapolate NS_AVAILABLE(26_0, 26_0);
@end

/// The protocol for the Tone Map Headroom filter.
///
/// Apply a global tone curve to an image that reduces colors from a source headroom value to a target headroom value.
@protocol CIToneMapHeadroom <CIFilter>
  /// Specifies input image with an optional content headroom property.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// If non-nil, this overrides the headroom property of the input image.
  @property (nonatomic) float sourceHeadroom;
  /// Specifies the target headroom of the output image.
  @property (nonatomic) float targetHeadroom;
@end

/// The protocol for the Vibrance filter.
///
/// Adjusts the saturation of an image while keeping pleasing skin tones.
@protocol CIVibrance <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount to adjust the saturation.
  @property (nonatomic) float amount;
@end

/// The protocol for the White Point Adjust filter.
///
/// Adjusts the reference white point for an image and maps all colors in the source using the new reference.
@protocol CIWhitePointAdjust <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A color to use as the white point.
  @property (nonatomic, retain) CIColor *color;
@end

// CICategoryColorEffect

/// The protocol for the Color Cross Polynomial filter.
///
/// Adjusts the color of an image with polynomials.
@protocol CIColorCrossPolynomial <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Polynomial coefficients for red channel.
  @property (nonatomic, retain) CIVector *redCoefficients;
  /// Polynomial coefficients for green channel.
  @property (nonatomic, retain) CIVector *greenCoefficients;
  /// Polynomial coefficients for blue channel.
  @property (nonatomic, retain) CIVector *blueCoefficients;
@end

/// The protocol for the Color Cube filter.
///
/// Uses a three-dimensional color table to transform the source image pixels.
@protocol CIColorCube <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The dimension of the color cube.
  @property (nonatomic) float cubeDimension;
  /// Data containing a 3-dimensional color table of floating-point premultiplied RGBA values. The cells are organized in a standard ordering. The columns and rows of the data are indexed by red and green, respectively. Each data plane is followed by the next higher plane in the data, with planes indexed by blue.
  @property (nonatomic, retain) NSData *cubeData;
  /// If true, then the color cube will be extrapolated if the input image contains RGB component values outside the range 0.0 to 1.0.
  @property (nonatomic) bool extrapolate NS_AVAILABLE(13_0, 16_0);
@end

/// The protocol for the Color Cubes Mixed With Mask filter.
///
/// Uses two three-dimensional color tables in a specified colorspace to transform the source image pixels. The mask image is used as an interpolant to mix the output of the two cubes.
@protocol CIColorCubesMixedWithMask <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A masking image.
  @property (nonatomic, retain, nullable) CIImage *maskImage;
  /// The dimension of the color cubes.
  @property (nonatomic) float cubeDimension;
  /// Data containing a 3-dimensional color table of floating-point premultiplied RGBA values. The cells are organized in a standard ordering. The columns and rows of the data are indexed by red and green, respectively. Each data plane is followed by the next higher plane in the data, with planes indexed by blue.
  @property (nonatomic, retain) NSData *cube0Data;
  /// Data containing a 3-dimensional color table of floating-point premultiplied RGBA values. The cells are organized in a standard ordering. The columns and rows of the data are indexed by red and green, respectively. Each data plane is followed by the next higher plane in the data, with planes indexed by blue.
  @property (nonatomic, retain) NSData *cube1Data;
  /// The CGColorSpace that defines the RGB values in the color table.
  @property (nonatomic, nullable) CGColorSpaceRef colorSpace;
  /// If true, then the color cube will be extrapolated if the input image contains RGB component values outside the range 0 to 1.
  @property (nonatomic) bool extrapolate NS_AVAILABLE(13_0, 16_0);
@end

/// The protocol for the Color Cube with ColorSpace filter.
///
/// Uses a three-dimensional color table in a specified colorspace to transform the source image pixels.
@protocol CIColorCubeWithColorSpace <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The dimension of the color cube.
  @property (nonatomic) float cubeDimension;
  /// Data containing a 3-dimensional color table of floating-point premultiplied RGBA values. The cells are organized in a standard ordering. The columns and rows of the data are indexed by red and green, respectively. Each data plane is followed by the next higher plane in the data, with planes indexed by blue.
  @property (nonatomic, retain) NSData *cubeData;
  /// If true, then the color cube will be extrapolated if the input image contains RGB component values outside the range 0.0 to 1.0.
  @property (nonatomic) bool extrapolate NS_AVAILABLE(13_0, 16_0);
  /// The CGColorSpace that defines the RGB values in the color table.
  @property (nonatomic, nullable) CGColorSpaceRef colorSpace;
@end

/// The protocol for the Color Curves filter.
///
/// Uses a three-channel one-dimensional color table to transform the source image pixels.
@protocol CIColorCurves <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Data containing a color table of floating-point RGB values.
  @property (nonatomic, retain) NSData *curvesData;
  /// A two-element vector that defines the minimum and maximum RGB component values that are used to look up result values from the color table.
  @property (nonatomic, retain) CIVector *curvesDomain;
  /// The CGColorSpace that defines the RGB values in the color table.
  @property (nonatomic, nullable) CGColorSpaceRef colorSpace;
@end

/// The protocol for the Color Invert filter.
///
/// Inverts the colors in an image.
@protocol CIColorInvert <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Color Map filter.
///
/// Performs a nonlinear transformation of source color values using mapping values provided in a table.
@protocol CIColorMap <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image data from this image transforms the source image values.
  @property (nonatomic, retain, nullable) CIImage *gradientImage;
@end

/// The protocol for the Color Monochrome filter.
///
/// Remaps colors so they fall within shades of a single color.
@protocol CIColorMonochrome <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The monochrome color to apply to the image.
  @property (nonatomic, retain) CIColor *color;
  /// The intensity of the monochrome effect. A value of 1.0 creates a monochrome image using the supplied color. A value of 0.0 has no effect on the image.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Color Posterize filter.
///
/// Remaps red, green, and blue color components to the number of brightness values you specify for each color component. This filter flattens colors to achieve a look similar to that of a silk-screened poster.
@protocol CIColorPosterize <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The number of brightness levels to use for each color component. Lower values result in a more extreme poster effect.
  @property (nonatomic) float levels;
@end

/// A protocol for filters that convert an image between CIELAB color space and the Core Image RGB working space.
@protocol CIConvertLab <CIFilter>
  /// The image to convert.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The normalize Boolean defines the range of values in CIELAB space. 
  /// * false: L is in the range `0...100` and A,B are in the range `-128...128`. 
  /// * true:  L,A,B are in the range `0...1`.
  @property (nonatomic) bool normalize;
@end

/// The protocol for the Dither filter.
///
/// Apply dithering to an image. This operation is usually performed in a perceptual color space.
@protocol CIDither <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The intensity of the effect.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Document Enhancer filter.
///
/// Enhance a document image by removing unwanted shadows, whitening the background, and enhancing contrast.
@protocol CIDocumentEnhancer <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount of enhancement.
  @property (nonatomic) float amount;
@end

/// The protocol for the False Color filter.
///
/// Maps luminance to a color ramp of two colors. False color is often used to process astronomical and other scientific data, such as ultraviolet and X-ray images.
@protocol CIFalseColor <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The first color to use for the color ramp.
  @property (nonatomic, retain) CIColor *color0;
  /// The second color to use for the color ramp.
  @property (nonatomic, retain) CIColor *color1;
@end

/// The protocol for the Lab ∆E filter.
///
/// Produces an image with the Lab ∆E difference values between two images. The result image will contain ∆E 1994 values between 0.0 and 100.0 where 2.0 is considered a just noticeable difference.
@protocol CILabDeltaE <CIFilter>
  /// The first input image for comparison.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The second input image for comparison.
  @property (nonatomic, retain, nullable) CIImage *image2;
@end

/// The protocol for the Mask to Alpha filter.
///
/// Converts a grayscale image to a white image that is masked by alpha. The white values from the source image produce the inside of the mask; the black values become completely transparent.
@protocol CIMaskToAlpha <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Maximum Component filter.
///
/// Converts an image to grayscale using the maximum of the three color components.
@protocol CIMaximumComponent <CIFilter>
  /// The image to process.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Minimum Component filter.
///
/// Converts an image to grayscale using the minimum of the three color components.
@protocol CIMinimumComponent <CIFilter>
  /// The image to process.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Palette Centroid filter.
///
/// Calculate the mean (x,y) image coordinates of a color palette.
@protocol CIPaletteCentroid <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The input color palette, obtained using “CIKMeans“ filter.
  @property (nonatomic, retain, nullable) CIImage *paletteImage;
  /// Specifies whether the color palette should be applied in a perceptual color space.
  @property (nonatomic) bool perceptual;
@end

/// The protocol for the Palettize filter.
///
/// Paint an image from a color palette obtained using “CIKMeans“.
@protocol CIPalettize <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The input color palette, obtained using “CIKMeans“ filter.
  @property (nonatomic, retain, nullable) CIImage *paletteImage;
  /// Specifies whether the color palette should be applied in a perceptual color space.
  @property (nonatomic) bool perceptual;
@end

/// The protocol for the Photo Effect Chrome filter.
///
/// Apply a “Chrome” style effect to an image.
@protocol CIPhotoEffect <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// If true, then the color effect will be extrapolated if the input image contains RGB component values outside the range 0.0 to 1.0.
  @property (nonatomic) bool extrapolate NS_AVAILABLE(14_0, 17_0);
@end

/// The protocol for the Sepia Tone filter.
///
/// Maps the colors of an image to various shades of brown.
@protocol CISepiaTone <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The intensity of the sepia effect. A value of 1.0 creates a monochrome sepia image. A value of 0.0 has no effect on the image.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Thermal filter.
///
/// Apply a “Thermal” style effect to an image.
@protocol CIThermal <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Vignette filter.
///
/// Applies a vignette shading to the corners of an image.
@protocol CIVignette <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The intensity of the effect.
  @property (nonatomic) float intensity;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
@end

/// The protocol for the Vignette Effect filter.
///
/// Applies a vignette shading to the corners of an image.
@protocol CIVignetteEffect <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
  /// The intensity of the effect.
  @property (nonatomic) float intensity;
  /// The falloff of the effect.
  @property (nonatomic) float falloff;
@end

/// The protocol for the X-Ray filter.
///
/// Apply an “XRay” style effect to an image.
@protocol CIXRay <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

// CICategoryDistortionEffect

/// The protocol for the Bump Distortion filter.
///
/// Creates a concave or convex bump that originates at a specified point in the image.
@protocol CIBumpDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The scale of the effect determines the curvature of the bump. A value of 0.0 has no effect. Positive values create an outward bump; negative values create an inward bump.
  @property (nonatomic) float scale;
@end

/// The protocol for the Bump Distortion Linear filter.
///
/// Creates a bump that originates from a linear portion of the image.
@protocol CIBumpDistortionLinear <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The angle in radians of the line around which the distortion occurs.
  @property (nonatomic) float angle;
  /// The scale of the effect.
  @property (nonatomic) float scale;
@end

/// The protocol for the Circle Splash Distortion filter.
///
/// Distorts the pixels starting at the circumference of a circle and emanating outward.
@protocol CICircleSplashDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
@end

/// The protocol for the Circular Wrap Distortion filter.
///
/// Wraps an image around a transparent circle. The distortion of the image increases with the distance from the center of the circle.
@protocol CICircularWrap <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The angle in radians of the effect.
  @property (nonatomic) float angle;
@end

/// The protocol for the Displacement Distortion filter.
///
/// Applies the grayscale values of the second image to the first image. The output image has a texture defined by the grayscale values.
@protocol CIDisplacementDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// An image whose grayscale values will be applied to the source image.
  @property (nonatomic, retain, nullable) CIImage *displacementImage;
  /// The amount of texturing of the resulting image. The larger the value, the greater the texturing.
  @property (nonatomic) float scale;
@end

/// The protocol for the Droste filter.
///
/// The Droste effect produces an infinite image by distorting an image into a spiral of the image within itself.
@protocol CIDroste <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The first of two points that define the rectangular frame of the effect.
  @property (nonatomic) CGPoint insetPoint0;
  /// The second of two points that define the rectangular frame of the effect.
  @property (nonatomic) CGPoint insetPoint1;
  /// An integer number representing the amount of strands in the effect.
  @property (nonatomic) float strands;
  /// An integer number representing the amount of intervals in the effect.
  @property (nonatomic) float periodicity;
  /// A float number representing the angle of the rotation in radians.
  @property (nonatomic) float rotation;
  /// A float number representing the zoom of the effect.
  @property (nonatomic) float zoom;
@end

/// The protocol for the Glass Distortion filter.
///
/// Distorts an image by applying a glass-like texture. The raised portions of the output image are the result of applying a texture map.
@protocol CIGlassDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A texture to apply to the source image.
  @property (nonatomic, retain, nullable) CIImage *textureImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The amount of texturing of the resulting image. The larger the value, the greater the texturing.
  @property (nonatomic) float scale;
@end

/// The protocol for the Glass Lozenge filter.
///
/// Creates a lozenge-shaped lens and distorts the portion of the image over which the lens is placed.
@protocol CIGlassLozenge <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The x and y position that defines the center of the circle at one end of the lozenge.
  @property (nonatomic) CGPoint point0;
  /// The x and y position that defines the center of the circle at the other end of the lozenge.
  @property (nonatomic) CGPoint point1;
  /// The radius of the lozenge. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The refraction of the glass.
  @property (nonatomic) float refraction;
@end

/// The protocol for the Hole Distortion filter.
///
/// Creates a circular area that pushes the image pixels outward, distorting those pixels closest to the circle the most.
@protocol CIHoleDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
@end

/// The protocol for the Light Tunnel Distortion filter.
///
/// Light tunnel distortion.
@protocol CILightTunnel <CIFilter>
  /// The image to process.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// Rotation angle in radians of the light tunnel.
  @property (nonatomic) float rotation;
  /// Center radius of the light tunnel.
  @property (nonatomic) float radius;
@end

/// The protocol for the Nine Part Stretched filter.
///
/// Distorts an image by stretching an image based on two input breakpoints.
@protocol CINinePartStretched <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Lower left corner of image to retain before stretching begins.
  @property (nonatomic) CGPoint breakpoint0;
  /// Upper right corner of image to retain after stretching ends.
  @property (nonatomic) CGPoint breakpoint1;
  /// Vector indicating how much image should grow in pixels in both dimensions.
  @property (nonatomic) CGPoint growAmount;
@end

/// The protocol for the Nine Part Tiled filter.
///
/// Distorts an image by tiling an image based on two input breakpoints.
@protocol CINinePartTiled <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Lower left corner of image to retain before tiling begins.
  @property (nonatomic) CGPoint breakpoint0;
  /// Upper right corner of image to retain after tiling ends.
  @property (nonatomic) CGPoint breakpoint1;
  /// Vector indicating how much image should grow in pixels in both dimensions.
  @property (nonatomic) CGPoint growAmount;
  /// Indicates that Y-Axis flip should occur.
  @property (nonatomic) bool flipYTiles;
@end

/// The protocol for the Pinch Distortion filter.
///
/// Creates a rectangular-shaped area that pinches source pixels inward, distorting those pixels closest to the rectangle the most.
@protocol CIPinchDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The amount of pinching. A value of 0.0 has no effect. A value of 1.0 is the maximum pinch.
  @property (nonatomic) float scale;
@end

/// The protocol for the Stretch Crop filter.
///
/// Distorts an image by stretching and or cropping to fit a target size.
@protocol CIStretchCrop <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The size in pixels of the output image.
  @property (nonatomic) CGPoint size;
  /// Determines if and how much cropping should be used to achieve the target size. If value is 0 then only stretching is used. If 1 then only cropping is used.
  @property (nonatomic) float cropAmount;
  /// Determine how much the center of the image is stretched if stretching is used. If value is 0 then the center of the image maintains the original aspect ratio. If 1 then the image is stretched uniformly.
  @property (nonatomic) float centerStretchAmount;
@end

/// The protocol for the Torus Lens Distortion filter.
///
/// Creates a torus-shaped lens and distorts the portion of the image over which the lens is placed.
@protocol CITorusLensDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The outer radius of the torus.
  @property (nonatomic) float radius;
  /// The width of the ring.
  @property (nonatomic) float width;
  /// The refraction of the glass.
  @property (nonatomic) float refraction;
@end

/// The protocol for the Twirl Distortion filter.
///
/// Rotates pixels around a point to give a twirling effect. You can specify the number of rotations as well as the center and radius of the effect.
@protocol CITwirlDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The angle in radians of the twirl. Values can be positive or negative.
  @property (nonatomic) float angle;
@end

/// The protocol for the Vortex Distortion filter.
///
/// Rotates pixels around a point to simulate a vortex. You can specify the number of rotations as well the center and radius of the effect. 
@protocol CIVortexDistortion <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The radius determines how many pixels are used to create the distortion. The larger the radius, the wider the extent of the distortion.
  @property (nonatomic) float radius;
  /// The angle in radians of the effect.
  @property (nonatomic) float angle;
@end

// CICategoryTileEffect

/// The protocol for the Affine Clamp filter.
///
/// Performs an affine transformation on a source image and then clamps the pixels at the edge of the transformed image, extending them outwards. This filter performs similarly to the “Affine Transform” filter except that it produces an image with infinite extent. You can use this filter when you need to blur an image but you want to avoid a soft, black fringe along the edges.
@protocol CIAffineClamp <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The transform to apply to the image.
  @property (nonatomic) CGAffineTransform transform;
@end

/// The protocol for the Affine Tile filter.
///
/// Applies an affine transformation to an image and then tiles the transformed image.
@protocol CIAffineTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The transform to apply to the image.
  @property (nonatomic) CGAffineTransform transform;
@end

/// The protocol for the Eightfold Reflected Tile filter.
///
/// Produces a tiled image from a source image by applying an 8-way reflected symmetry.
@protocol CIEightfoldReflectedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Fourfold Reflected Tile filter.
///
/// Produces a tiled image from a source image by applying a 4-way reflected symmetry.
@protocol CIFourfoldReflectedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
  /// The primary angle for the repeating reflected tile. Small values create thin diamond tiles, and higher values create fatter reflected tiles.
  @property (nonatomic) float acuteAngle;
@end

/// The protocol for the Fourfold Rotated Tile filter.
///
/// Produces a tiled image from a source image by rotating the source at increments of 90 degrees.
@protocol CIFourfoldRotatedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Fourfold Translated Tile filter.
///
/// Produces a tiled image from a source image by applying 4 translation operations.
@protocol CIFourfoldTranslatedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
  /// The primary angle for the repeating translated tile. Small values create thin diamond tiles, and higher values create fatter translated tiles.
  @property (nonatomic) float acuteAngle;
@end

/// The protocol for the Glide Reflected Tile filter.
///
/// Produces a tiled image from a source image by translating and smearing the image.
@protocol CIGlideReflectedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Kaleidoscope filter.
///
/// Produces a kaleidoscopic image from a source image by applying 12-way symmetry.
@protocol CIKaleidoscope <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The number of reflections in the pattern.
  @property (nonatomic) NSInteger count;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of reflection.
  @property (nonatomic) float angle;
@end

/// The protocol for the Op Tile filter.
///
/// Segments an image, applying any specified scaling and rotation, and then assembles the image again to give an op art appearance.
@protocol CIOpTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The scale determines the number of tiles in the effect.
  @property (nonatomic) float scale;
  /// The angle in radians of a tile.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Parallelogram Tile filter.
///
/// Warps an image by reflecting it in a parallelogram, and then tiles the result.
@protocol CIParallelogramTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The primary angle for the repeating parallelogram tile. Small values create thin diamond tiles, and higher values create fatter parallelogram tiles.
  @property (nonatomic) float acuteAngle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Perspective Tile filter.
///
/// Applies a perspective transform to an image and then tiles the result.
@protocol CIPerspectiveTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The top left coordinate of a tile.
  @property (nonatomic) CGPoint topLeft;
  /// The top right coordinate of a tile.
  @property (nonatomic) CGPoint topRight;
  /// The bottom right coordinate of a tile.
  @property (nonatomic) CGPoint bottomRight;
  /// The bottom left coordinate of a tile.
  @property (nonatomic) CGPoint bottomLeft;
@end

/// The protocol for the Sixfold Reflected Tile filter.
///
/// Produces a tiled image from a source image by applying a 6-way reflected symmetry.
@protocol CISixfoldReflectedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Sixfold Rotated Tile filter.
///
/// Produces a tiled image from a source image by rotating the source at increments of 60 degrees.
@protocol CISixfoldRotatedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Triangle Kaleidoscope filter.
///
/// Maps a triangular portion of image to a triangular area and then generates a kaleidoscope effect.
@protocol CITriangleKaleidoscope <CIFilter>
  /// Input image to generate kaleidoscope effect from.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The x and y position to use as the center of the triangular area in the input image.
  @property (nonatomic) CGPoint point;
  /// The size in pixels of the triangle.
  @property (nonatomic) float size;
  /// Rotation angle in radians of the triangle.
  @property (nonatomic) float rotation;
  /// The decay determines how fast the color fades from the center triangle.
  @property (nonatomic) float decay;
@end

/// The protocol for the Triangle Tile filter.
///
/// Maps a triangular portion of image to a triangular area and then tiles the result.
@protocol CITriangleTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

/// The protocol for the Twelvefold Reflected Tile filter.
///
/// Produces a tiled image from a source image by applying a 12-way reflected symmetry.
@protocol CITwelvefoldReflectedTile <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The angle in radians of the tiled pattern.
  @property (nonatomic) float angle;
  /// The width of a tile.
  @property (nonatomic) float width;
@end

// CICategoryGenerator

/// The protocol for the Attributed Text Image Generator filter.
///
/// Generate an image attributed string.
@protocol CIAttributedTextImageGenerator <CIFilter>
  /// The attributed text to render.
  @property (nonatomic, retain) NSAttributedString *text;
  /// The scale of the font to use for the generated text.
  @property (nonatomic) float scaleFactor;
  /// A value for an additional number of pixels to pad around the text’s bounding box.
  @property (nonatomic) float padding NS_AVAILABLE(13_0, 16_0);
@end

/// The protocol for the Aztec Code Generator filter.
///
/// Generate an Aztec barcode image for message data.
@protocol CIAztecCodeGenerator <CIFilter>
  /// The message to encode in the Aztec Barcode
  @property (nonatomic, retain) NSData *message;
  /// Aztec error correction value between 5 and 95
  @property (nonatomic) float correctionLevel;
  /// Aztec layers value between 1 and 32. Set to nil for automatic.
  @property (nonatomic) float layers;
  /// Force a compact style Aztec code to true or false. Set to nil for automatic.
  @property (nonatomic) float compactStyle;
@end

/// The protocol for the Barcode Generator filter.
///
/// Generate a barcode image from a CIBarcodeDescriptor.
@protocol CIBarcodeGenerator <CIFilter>
  /// The CIBarcodeDescription object to generate an image for.
  @property (nonatomic, retain) CIBarcodeDescriptor *barcodeDescriptor;
@end

/// The protocol for the Blurred Rectangle Generator filter.
///
/// Generates a blurred rectangle image with the specified extent, blur sigma, and color.
@protocol CIBlurredRectangleGenerator <CIFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The sigma for a gaussian blur.
  @property (nonatomic) float sigma;
  /// A color.
  @property (nonatomic, retain) CIColor *color;
@end

/// The protocol for the Blurred Rounded Rectangle Generator filter.
///
/// Generates a blurred rounded rectangle image with the specified extent, corner radius, blur sigma, and color.
@protocol CIBlurredRoundedRectangleGenerator <CIFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
  /// A value to control the smoothness of the transition between the curved and linear edges of the shape.
  @property (nonatomic) float smoothness;
  /// The sigma for a gaussian blur.
  @property (nonatomic) float sigma;
  /// A color.
  @property (nonatomic, retain) CIColor *color;
@end

/// The protocol for the Checkerboard filter.
///
/// Generates a pattern of squares of alternating colors. You can specify the size, colors, and the sharpness of the pattern.
@protocol CICheckerboardGenerator <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// A color to use for the first set of squares.
  @property (nonatomic, retain) CIColor *color0;
  /// A color to use for the second set of squares.
  @property (nonatomic, retain) CIColor *color1;
  /// The width of the squares in the pattern.
  @property (nonatomic) float width;
  /// The sharpness of the edges in pattern. The smaller the value, the more blurry the pattern. Values range from 0.0 to 1.0.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the Code 128 Barcode Generator filter.
///
/// Generate a Code 128 barcode image for message data.
@protocol CICode128BarcodeGenerator <CIFilter>
  /// The message to encode in the Code 128 Barcode
  @property (nonatomic, retain) NSData *message;
  /// The number of empty white pixels that should surround the barcode.
  @property (nonatomic) float quietSpace;
  /// The height of the generated barcode in pixels.
  @property (nonatomic) float barcodeHeight;
@end

/// The protocol for the Lenticular Halo filter.
///
/// Simulates a halo that is generated by the diffraction associated with the spread of a lens. This filter is typically applied to another image to simulate lens flares and similar effects.
@protocol CILenticularHaloGenerator <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// A color.
  @property (nonatomic, retain) CIColor *color;
  /// The radius of the halo.
  @property (nonatomic) float haloRadius;
  /// The width of the halo, from its inner radius to its outer radius.
  @property (nonatomic) float haloWidth;
  @property (nonatomic) float haloOverlap;
  /// The intensity of the halo colors. Larger values are more intense.
  @property (nonatomic) float striationStrength;
  /// The contrast of the halo colors. Larger values are higher contrast.
  @property (nonatomic) float striationContrast;
  /// The duration of the effect.
  @property (nonatomic) float time;
@end

/// The protocol for the Mesh Generator filter.
///
/// Generates a mesh from an array of line segments.
@protocol CIMeshGenerator <CIFilter>
  /// The width in pixels of the effect.
  @property (nonatomic) float width;
  /// A color.
  @property (nonatomic, retain) CIColor *color;
  /// An array of line segments stored as an array of CIVectors each containing a start point and end point.
  @property (nonatomic, retain) NSArray *mesh;
@end

/// The protocol for the PDF417 Barcode Generator filter.
///
/// Generate a PDF417 barcode image for message data.
@protocol CIPDF417BarcodeGenerator <CIFilter>
  /// The message to encode in the PDF417 Barcode
  @property (nonatomic, retain) NSData *message;
  /// The minimum width of the generated barcode in pixels.
  @property (nonatomic) float minWidth;
  /// The maximum width of the generated barcode in pixels.
  @property (nonatomic) float maxWidth;
  /// The minimum height of the generated barcode in pixels.
  @property (nonatomic) float minHeight;
  /// The maximum height of the generated barcode in pixels.
  @property (nonatomic) float maxHeight;
  /// The number of data columns in the generated barcode
  @property (nonatomic) float dataColumns;
  /// The number of rows in the generated barcode
  @property (nonatomic) float rows;
  /// The preferred aspect ratio of the generated barcode
  @property (nonatomic) float preferredAspectRatio;
  /// The compaction mode of the generated barcode.
  @property (nonatomic) float compactionMode;
  /// Force a compact style Aztec code to true or false. Set to nil for automatic.
  @property (nonatomic) float compactStyle;
  /// The correction level ratio of the generated barcode
  @property (nonatomic) float correctionLevel;
  /// Force compaction style to true or false. Set to nil for automatic.
  @property (nonatomic) float alwaysSpecifyCompaction;
@end

/// The protocol for the QR Code Generator filter.
///
/// Generate a QR Code image for message data.
@protocol CIQRCodeGenerator <CIFilter>
  /// The message to encode in the QR Code
  @property (nonatomic, retain) NSData *message;
  /// QR Code correction level L, M, Q, or H.
  @property (nonatomic, retain) NSString *correctionLevel;
@end

/// The protocol for the Random Generator filter.
///
/// Generates an image of infinite extent whose pixel values are made up of four independent, uniformly-distributed random numbers in the 0 to 1 range.
@protocol CIRandomGenerator <CIFilter>
@end

/// The protocol for the Rounded QR Code Generator filter.
///
/// Generate a QR Code image for message data.
@protocol CIRoundedQRCodeGenerator <CIFilter>
  /// The message to encode in the QR Code
  @property (nonatomic, retain) NSData *message;
  /// QR Code correction level L, M, Q, or H.
  @property (nonatomic, retain) NSString *correctionLevel;
  /// The scale factor to enlarge the QRCode by.
  @property (nonatomic) float scale;
  /// If 1, then the Finder Patterns in the QRCode should have a rounded appearance. If 2, then the Alignment Patterns will also be rounded
  @property (nonatomic) NSInteger roundedMarkers;
  /// If true then the data points in the QRCode should have a rounded appearance.
  @property (nonatomic) bool roundedData;
  /// The fraction of the center space of the QRCode to fill with Color 1. If the size is 0.0 or the Correction Level is L or M, the center of the QRCode will be unaltered. The size will be limited to 0.25 if the Correction Level is Q. The size will be limited to 0.33 if the Correction Level is H.
  @property (nonatomic) float centerSpaceSize;
  /// The background color for the QRCode
  @property (nonatomic, retain) CIColor *color0;
  /// The foreground color for the QRCode
  @property (nonatomic, retain) CIColor *color1;
@end

/// The protocol for the Rounded Rectangle Generator filter.
///
/// Generates a rounded rectangle image with the specified extent, corner radius, and color.
@protocol CIRoundedRectangleGenerator <CIFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
  /// A value to control the smoothness of the transition between the curved and linear edges of the shape.
  @property (nonatomic) float smoothness NS_AVAILABLE(26_0, 26_0);
  /// A color.
  @property (nonatomic, retain) CIColor *color;
@end

/// The protocol for the Rounded Rectangle Stroke Generator filter.
///
/// Generates a rounded rectangle stroke image with the specified extent, corner radius, stroke width, and color.
@protocol CIRoundedRectangleStrokeGenerator <CIFilter>
  /// A rectangle that defines the extent of the effect.
  @property (nonatomic) CGRect extent;
  /// The distance from the center of the effect.
  @property (nonatomic) float radius;
  /// A value to control the smoothness of the transition between the curved and linear edges of the shape.
  @property (nonatomic) float smoothness NS_AVAILABLE(26_0, 26_0);
  /// A color.
  @property (nonatomic, retain) CIColor *color;
  /// The width in pixels of the effect.
  @property (nonatomic) float width;
@end

/// The protocol for the Star Shine filter.
///
/// Generates a starburst pattern. The output image is typically used as input to another filter.
@protocol CIStarShineGenerator <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The color to use for the outer shell of the circular star.
  @property (nonatomic, retain) CIColor *color;
  /// The radius of the star.
  @property (nonatomic) float radius;
  /// The size of the cross pattern.
  @property (nonatomic) float crossScale;
  /// The angle in radians of the cross pattern.
  @property (nonatomic) float crossAngle;
  /// The opacity of the cross pattern.
  @property (nonatomic) float crossOpacity;
  /// The width of the cross pattern.
  @property (nonatomic) float crossWidth;
  /// The length of the cross spikes.
  @property (nonatomic) float epsilon;
@end

/// The protocol for the Stripes filter.
///
/// Generates a stripe pattern. You can control the color of the stripes, the spacing, and the contrast.
@protocol CIStripesGenerator <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// A color to use for the odd stripes.
  @property (nonatomic, retain) CIColor *color0;
  /// A color to use for the even stripes.
  @property (nonatomic, retain) CIColor *color1;
  /// The width of a stripe.
  @property (nonatomic) float width;
  /// The sharpness of the stripe pattern. The smaller the value, the more blurry the pattern. Values range from 0.0 to 1.0.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the Sunbeams filter.
///
/// Generates a sun effect. You typically use the output of the sunbeams filter as input to a composite filter.
@protocol CISunbeamsGenerator <CIFilter>
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The color of the sun.
  @property (nonatomic, retain) CIColor *color;
  /// The radius of the sun.
  @property (nonatomic) float sunRadius;
  /// The radius of the sunbeams.
  @property (nonatomic) float maxStriationRadius;
  /// The intensity of the sunbeams. Higher values result in more intensity.
  @property (nonatomic) float striationStrength;
  /// The contrast of the sunbeams. Higher values result in more contrast.
  @property (nonatomic) float striationContrast;
  /// The duration of the effect.
  @property (nonatomic) float time;
@end

/// The protocol for the Text Image Generator filter.
///
/// Generate an image from a string and font information.
@protocol CITextImageGenerator <CIFilter>
  /// The text to render.
  @property (nonatomic, retain) NSString *text;
  /// The name of the font to use for the generated text.
  @property (nonatomic, retain) NSString *fontName;
  /// The size of the font to use for the generated text.
  @property (nonatomic) float fontSize;
  /// The scale of the font to use for the generated text.
  @property (nonatomic) float scaleFactor;
  /// The number of additional pixels to pad around the text’s bounding box.
  @property (nonatomic) float padding NS_AVAILABLE(13_0, 16_0);
@end

// CICategoryStylize

/// The protocol for the Blend With Alpha Mask filter.
///
/// Uses values from a mask image to interpolate between an image and the background. When a mask alpha value is 0.0, the result is the background. When the mask alpha value is 1.0, the result is the image.
@protocol CIBlendWithMask <CIFilter>
  /// The image to use as a foreground image.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image to use as a background image.
  @property (nonatomic, retain, nullable) CIImage *backgroundImage;
  /// A masking image.
  @property (nonatomic, retain, nullable) CIImage *maskImage;
@end

/// The protocol for the Bloom filter.
///
/// Softens edges and applies a pleasant glow to an image.
@protocol CIBloom <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the effect. The larger the radius, the greater the effect.
  @property (nonatomic) float radius;
  /// The intensity of the effect. A value of 0.0 is no effect. A value of 1.0 is the maximum effect.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Canny Edge Detector filter.
///
/// Applies the Canny Edge Detection algorithm to an image.
@protocol CICannyEdgeDetector <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The gaussian sigma of blur to apply to the image to reduce high-frequency noise.
  @property (nonatomic) float gaussianSigma;
  /// Specifies whether the edge thresholds should be computed in a perceptual color space.
  @property (nonatomic) bool perceptual;
  /// The threshold that determines if gradient magnitude is a strong edge.
  @property (nonatomic) float thresholdHigh;
  /// The threshold that determines if gradient magnitude is a weak edge.
  @property (nonatomic) float thresholdLow;
  /// The number of hysteresis passes to apply to promote weak edge pixels.
  @property (nonatomic) NSInteger hysteresisPasses;
@end

/// The protocol for the Comic Effect filter.
///
/// Simulates a comic book drawing by outlining edges and applying a color halftone effect.
@protocol CIComicEffect <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the 3 by 3 Convolution filter.
///
/// Convolution with 3 by 3 matrix.
@protocol CIConvolution <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A vector containing the 9 weights of the convolution kernel.
  @property (nonatomic, retain) CIVector *weights;
  /// A value that is added to the RGBA components of the output pixel.
  @property (nonatomic) float bias;
@end

/// The protocol for the CoreML Model Filter filter.
///
/// Generates output image by applying input CoreML model to the input image.
@protocol CICoreMLModel <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The CoreML model to be used for applying effect on the image.
  @property (nonatomic, retain) MLModel *model;
  /// A number to specify which output of a multi-head CoreML model should be used for applying effect on the image.
  @property (nonatomic) float headIndex;
  /// A boolean value to specify that Softmax normalization should be applied to the output of the model.
  @property (nonatomic) bool softmaxNormalization;
@end

/// The protocol for the Crystallize filter.
///
/// Creates polygon-shaped color blocks by aggregating source pixel-color values.
@protocol CICrystallize <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the effect. The larger the radius, the larger the resulting crystals.
  @property (nonatomic) float radius;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
@end

/// The protocol for the Depth of Field filter.
///
/// Simulates miniaturization effect created by Tilt & Shift lens by performing depth of field effects.
@protocol CIDepthOfField <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The first of two points defining a line in the image that should remain in focus
  @property (nonatomic) CGPoint point0;
  /// The second of two points defining a line in the image that should remain in focus
  @property (nonatomic) CGPoint point1;
  /// The amount to adjust the saturation.
  @property (nonatomic) float saturation;
  /// A float number representing the radius of the unsharpened mask effect applied to the in-focus area of effect.
  @property (nonatomic) float unsharpMaskRadius;
  /// A float number representing the intensity of the unsharp mask effect applied to the in-focus area of effect.
  @property (nonatomic) float unsharpMaskIntensity;
  /// A float representing how much to blur the portion of the image that is not near the focus line.
  @property (nonatomic) float radius;
@end

/// The protocol for the Edges filter.
///
/// Finds all edges in an image and displays them in color.
@protocol CIEdges <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The intensity of the edges. The larger the value, the higher the intensity.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Edge Work filter.
///
/// Produces a stylized black-and-white rendition of an image that looks similar to a woodblock cutout.
@protocol CIEdgeWork <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The thickness of the edges. The larger the value, the thicker the edges.
  @property (nonatomic) float radius;
@end

/// The protocol for the Gabor Gradients filter.
///
/// Applies multichannel 5 by 5 Gabor gradient filter to an image. The resulting image has maximum horizontal gradient in the red channel and the maximum vertical gradient in the green channel. The gradient values can be positive or negative.
@protocol CIGaborGradients <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Gloom filter.
///
/// Dulls the highlights of an image.
@protocol CIGloom <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the effect. The larger the radius, the greater the effect.
  @property (nonatomic) float radius;
  /// The intensity of the effect. A value of 0.0 is no effect. A value of 1.0 is the maximum effect.
  @property (nonatomic) float intensity;
@end

/// The protocol for the Height Field From Mask filter.
///
/// Produces a continuous three-dimensional, loft-shaped height field from a grayscale mask. The white values of the mask define those pixels that are inside the height field while the black values define those pixels that are outside. The field varies smoothly and continuously inside the mask, reaching the value 0 at the edge of the mask. You can use this filter with the Shaded Material filter to produce extremely realistic shaded objects.
@protocol CIHeightFieldFromMask <CIFilter>
  /// The white values of the mask define those pixels that are inside the height field while the black values define those pixels that are outside. The field varies smoothly and continuously inside the mask, reaching the value 0 at the edge of the mask.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The distance from the edge of the mask for the smooth transition is proportional to the input radius. Larger values make the transition smoother and more pronounced. Smaller values make the transition approximate a fillet radius.
  @property (nonatomic) float radius;
@end

/// The protocol for the Hexagonal Pixelate filter.
///
/// Displays an image as colored hexagons whose color is an average of the pixels they replace.
@protocol CIHexagonalPixellate <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The scale determines the size of the hexagons. Larger values result in larger hexagons.
  @property (nonatomic) float scale;
@end

/// The protocol for the Highlight and Shadow Adjust filter.
///
/// Adjust the tonal mapping of an image while preserving spatial detail.
@protocol CIHighlightShadowAdjust <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Shadow Highlight Radius.
  @property (nonatomic) float radius;
  /// The amount of adjustment to the shadows of the image.
  @property (nonatomic) float shadowAmount;
  /// The amount of adjustment to the highlights of the image.
  @property (nonatomic) float highlightAmount;
@end

/// The protocol for the Line Overlay filter.
///
/// Creates a sketch that outlines the edges of an image in black, leaving the non-outlined portions of the image transparent. The result has alpha and is rendered in black, so it won’t look like much until you render it over another image using source over compositing.
@protocol CILineOverlay <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The noise level of the image (used with camera data) that gets removed before tracing the edges of the image. Increasing the noise level helps to clean up the traced edges of the image.
  @property (nonatomic) float NRNoiseLevel;
  /// The amount of sharpening done when removing noise in the image before tracing the edges of the image. This improves the edge acquisition.
  @property (nonatomic) float NRSharpness;
  /// The accentuation factor of the Sobel gradient information when tracing the edges of the image. Higher values find more edges, although typically a low value (such as 1.0) is used.
  @property (nonatomic) float edgeIntensity;
  /// This value determines edge visibility. Larger values thin out the edges.
  @property (nonatomic) float threshold;
  /// The amount of anti-aliasing to use on the edges produced by this filter. Higher values produce higher contrast edges (they are less anti-aliased).
  @property (nonatomic) float contrast;
@end

/// The protocol for the Mix filter.
///
/// Uses an amount parameter to interpolate between an image and a background image. When value is 0.0 or less, the result is the background image. When the value is 1.0 or more, the result is the image.
@protocol CIMix <CIFilter>
  /// The image to use as a foreground image.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image to use as a background image.
  @property (nonatomic, retain, nullable) CIImage *backgroundImage;
  /// The amount of the effect.
  @property (nonatomic) float amount;
@end

/// The protocol for the Person Segmentation filter.
///
/// Returns a segmentation mask that is red in the portions of an image that are likely to be persons. The returned image may have a different size and aspect ratio from the input image.
@protocol CIPersonSegmentation <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// Determines the size and quality of the resulting segmentation mask. The value can be a number where 0 is accurate, 1 is balanced, and 2 is fast.
  @property (nonatomic) NSUInteger qualityLevel;
@end

/// The protocol for the Pixelate filter.
///
/// Makes an image blocky.
@protocol CIPixellate <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The scale determines the size of the squares. Larger values result in larger squares.
  @property (nonatomic) float scale;
@end

/// The protocol for the Pointillize filter.
///
/// Renders the source image in a pointillistic style.
@protocol CIPointillize <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius of the circles in the resulting pattern.
  @property (nonatomic) float radius;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
@end

/// The protocol for the Saliency Map Filter filter.
///
/// Generates output image as a saliency map of the input image.
@protocol CISaliencyMap <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Shaded Material filter.
///
/// Produces a shaded image from a height field. The height field is defined to have greater heights with lighter shades, and lesser heights (lower areas) with darker shades. You can combine this filter with the “Height Field From Mask” filter to produce quick shadings of masks, such as text.
@protocol CIShadedMaterial <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The image to use as the height field. The resulting image has greater heights with lighter shades, and lesser heights (lower areas) with darker shades.
  @property (nonatomic, retain, nullable) CIImage *shadingImage;
  /// The scale of the effect. The higher the value, the more dramatic the effect.
  @property (nonatomic) float scale;
@end

/// The protocol for the Sobel Gradients filter.
///
/// Applies multichannel 3 by 3 Sobel gradient filter to an image. The resulting image has maximum horizontal gradient in the red channel and the maximum vertical gradient in the green channel. The gradient values can be positive or negative.
@protocol CISobelGradients <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Spot Color filter.
///
/// Replaces one or more color ranges with spot colors.
@protocol CISpotColor <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center value of the first color range to replace.
  @property (nonatomic, retain) CIColor *centerColor1;
  /// A replacement color for the first color range.
  @property (nonatomic, retain) CIColor *replacementColor1;
  /// A value that indicates how close the first color must match before it is replaced.
  @property (nonatomic) float closeness1;
  /// The contrast of the first replacement color.
  @property (nonatomic) float contrast1;
  /// The center value of the second color range to replace.
  @property (nonatomic, retain) CIColor *centerColor2;
  /// A replacement color for the second color range.
  @property (nonatomic, retain) CIColor *replacementColor2;
  /// A value that indicates how close the second color must match before it is replaced.
  @property (nonatomic) float closeness2;
  /// The contrast of the second replacement color.
  @property (nonatomic) float contrast2;
  /// The center value of the third color range to replace.
  @property (nonatomic, retain) CIColor *centerColor3;
  /// A replacement color for the third color range.
  @property (nonatomic, retain) CIColor *replacementColor3;
  /// A value that indicates how close the third color must match before it is replaced.
  @property (nonatomic) float closeness3;
  /// The contrast of the third replacement color.
  @property (nonatomic) float contrast3;
@end

/// The protocol for the Spot Light filter.
///
/// Applies a directional spotlight effect to an image.
@protocol CISpotLight <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The x and y position of the spotlight.
  @property (nonatomic, retain) CIVector *lightPosition;
  /// The x and y position that the spotlight points at.
  @property (nonatomic, retain) CIVector *lightPointsAt;
  /// The brightness of the spotlight.
  @property (nonatomic) float brightness;
  /// The spotlight size. The smaller the value, the more tightly focused the light beam.
  @property (nonatomic) float concentration;
  /// The color of the spotlight.
  @property (nonatomic, retain) CIColor *color;
@end

// CICategoryBlur

/// The protocol for the Bokeh Blur filter.
///
/// Smooths an image using a disc-shaped convolution kernel.
@protocol CIBokehBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the blur. The larger the radius, the blurrier the result.
  @property (nonatomic) float radius;
  /// The amount of extra emphasis at the ring of the bokeh.
  @property (nonatomic) float ringAmount;
  /// The size of extra emphasis at the ring of the bokeh.
  @property (nonatomic) float ringSize;
  /// The softness of the bokeh effect.
  @property (nonatomic) float softness;
@end

/// The protocol for the Box Blur filter.
///
/// Smooths or sharpens an image using a box-shaped convolution kernel.
@protocol CIBoxBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the blur. The larger the radius, the blurrier the result.
  @property (nonatomic) float radius;
@end

/// The protocol for the Disc Blur filter.
///
/// Smooths an image using a disc-shaped convolution kernel.
@protocol CIDiscBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the blur. The larger the radius, the blurrier the result.
  @property (nonatomic) float radius;
@end

/// The protocol for the Gaussian Blur filter.
///
/// Spreads source pixels by an amount specified by a Gaussian distribution.
@protocol CIGaussianBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the blur. The larger the radius, the blurrier the result.
  @property (nonatomic) float radius;
@end

/// The protocol for the Masked Variable Blur filter.
///
/// Blurs an image according to the brightness levels in a mask image.
@protocol CIMaskedVariableBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The mask image that determines how much to blur the image. The mask’s green channel value from 0.0 to 1.0 determines if the image is not blurred or blurred by the full radius.
  @property (nonatomic, retain, nullable) CIImage *mask;
  /// A value that governs the maximum blur radius to apply.
  @property (nonatomic) float radius;
@end

/// The protocol for the Median filter.
///
/// Computes the median value for a group of neighboring pixels and replaces each pixel value with the median. The effect is to reduce noise.
@protocol CIMedian <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
@end

/// The protocol for the Morphology Gradient filter.
///
/// Finds the edges of an image by returning the difference between the morphological minimum and maximum operations to the image.
@protocol CIMorphologyGradient <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The desired radius of the circular morphological operation to the image.
  @property (nonatomic) float radius;
@end

/// The protocol for the Morphology Maximum filter.
///
/// Lightens areas of an image by applying a circular morphological maximum operation to the image.
@protocol CIMorphologyMaximum <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The desired radius of the circular morphological operation to the image.
  @property (nonatomic) float radius;
@end

/// The protocol for the Morphology Minimum filter.
///
/// Darkens areas of an image by applying a circular morphological maximum operation to the image.
@protocol CIMorphologyMinimum <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The desired radius of the circular morphological operation to the image.
  @property (nonatomic) float radius;
@end

/// The protocol for the Morphology Rectangle Maximum filter.
///
/// Lightens areas of an image by applying a rectangular morphological maximum operation to the image.
@protocol CIMorphologyRectangleMaximum <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The width in pixels of the morphological operation. The value will be rounded to the nearest odd integer.
  @property (nonatomic) float width;
  /// The height in pixels of the morphological operation. The value will be rounded to the nearest odd integer.
  @property (nonatomic) float height;
@end

/// The protocol for the Morphology Rectangle Minimum filter.
///
/// Darkens areas of an image by applying a rectangular morphological maximum operation to the image.
@protocol CIMorphologyRectangleMinimum <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The width in pixels of the morphological operation. The value will be rounded to the nearest odd integer.
  @property (nonatomic) float width;
  /// The height in pixels of the morphological operation. The value will be rounded to the nearest odd integer.
  @property (nonatomic) float height;
@end

/// The protocol for the Motion Blur filter.
///
/// Blurs an image to simulate the effect of using a camera that moves a specified angle and distance while capturing the image.
@protocol CIMotionBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The radius determines how many pixels are used to create the blur. The larger the radius, the blurrier the result.
  @property (nonatomic) float radius;
  /// The angle in radians of the motion determines which direction the blur smears.
  @property (nonatomic) float angle;
@end

/// The protocol for the Noise Reduction filter.
///
/// Reduces noise using a threshold value to define what is considered noise. Small changes in luminance below that value are considered noise and get a noise reduction treatment, which is a local blur. Changes above the threshold value are considered edges, so they are sharpened.
@protocol CINoiseReduction <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The amount of noise reduction. The larger the value, the more noise reduction.
  @property (nonatomic) float noiseLevel;
  /// The sharpness of the final image. The larger the value, the sharper the result.
  @property (nonatomic) float sharpness;
@end

/// The protocol for the Zoom Blur filter.
///
/// Simulates the effect of zooming the camera while capturing the image.
@protocol CIZoomBlur <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The center of the effect as x and y pixel coordinates.
  @property (nonatomic) CGPoint center;
  /// The zoom-in amount. Larger values result in more zooming in.
  @property (nonatomic) float amount;
@end

// CICategoryReduction

/// A protocol for filters that perform a reduction calculation on an image.
@protocol CIAreaReductionFilter <CIFilter>
  /// The image to use as the input for the reduction filter.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// A rectangle that, when intersected with the input image extent, 
  /// defines the extent of the reduction calculation.
  @property (nonatomic) CGRect extent;
@end

/// The protocol for the Area Average filter.
///
/// Calculates the average color for the specified area in an image, returning the result in a pixel.
@protocol CIAreaAverage <CIAreaReductionFilter>
@end

/// The protocol for the Area Average and Maximum Red filter.
///
/// Calculates the average and maximum red component value for the specified area in an image. The result is returned in the red and green channels of a one pixel image.
@protocol CIAreaAverageMaximumRed <CIAreaReductionFilter>
@end

/// The protocol for the Area Bounds Red filter.
///
/// Calculates the approximate bounding box of pixels within the specified area of an image where the red component values are non-zero. The result is 1x1 pixel image where the RGBA values contain the normalized X,Y,W,H dimensions of the bounding box.
@protocol CIAreaBoundsRed <CIAreaReductionFilter>
@end

/// The protocol for the Area Histogram filter.
///
/// Calculates histograms of the R, G, B, and A channels of the specified area of an image. The output image is a one pixel tall image containing the histogram data for all four channels.
@protocol CIAreaHistogram <CIAreaReductionFilter>
  /// The scale value to use for the histogram values. If the scale is 1.0, then the bins in the resulting image will add up to 1.0.
  @property (nonatomic) float scale;
  /// The number of bins for the histogram. This value will determine the width of the output image.
  @property (nonatomic) NSInteger count;
@end

/// The protocol for the Area Logarithmic Histogram filter.
///
/// Calculates histogram of the R, G, B, and A channels of the specified area of an image. Before binning, the R, G, and B channel values are transformed by the log base two function. The output image is a one pixel tall image containing the histogram data for all four channels.
@protocol CIAreaLogarithmicHistogram <CIAreaReductionFilter>
  /// The amount of the effect.
  @property (nonatomic) float scale;
  /// The number of bins for the histogram. This value will determine the width of the output image.
  @property (nonatomic) NSInteger count;
  /// The minimum of the range of color channel values to be in the logarithmic histogram image.
  @property (nonatomic) float minimumStop;
  /// The maximum of the range of color channel values to be in the logarithmic histogram image.
  @property (nonatomic) float maximumStop;
@end

/// The protocol for the Area Maximum filter.
///
/// Calculates the maximum component values for the specified area in an image, returning the result in a pixel.
@protocol CIAreaMaximum <CIAreaReductionFilter>
@end

/// The protocol for the Area Maximum Alpha filter.
///
/// Finds and returns the pixel with the maximum alpha value.
@protocol CIAreaMaximumAlpha <CIAreaReductionFilter>
@end

/// The protocol for the Area Minimum filter.
///
/// Calculates the minimum component values for the specified area in an image, returning the result in a pixel.
@protocol CIAreaMinimum <CIAreaReductionFilter>
@end

/// The protocol for the Area Minimum Alpha filter.
///
/// Finds and returns the pixel with the minimum alpha value.
@protocol CIAreaMinimumAlpha <CIAreaReductionFilter>
@end

/// The protocol for the Area Min and Max filter.
///
/// Calculates the per-component minimum and maximum value for the specified area in an image. The result is returned in a 2x1 image where the component minimum values are stored in the pixel on the left.
@protocol CIAreaMinMax <CIAreaReductionFilter>
@end

/// The protocol for the Area Min and Max Red filter.
///
/// Calculates the minimum and maximum red component value for the specified area in an image. The result is returned in the red and green channels of a one pixel image.
@protocol CIAreaMinMaxRed <CIAreaReductionFilter>
@end

/// The protocol for the Column Average filter.
///
/// Calculates the average color for each column of the specified area in an image, returning the result in a 1D image.
@protocol CIColumnAverage <CIAreaReductionFilter>
@end

/// The protocol for the Histogram Display filter.
///
/// Generates a displayable histogram image from the output of the “Area Histogram” filter.
@protocol CIHistogramDisplay <CIFilter>
  /// The image to use as an input for the effect.
  @property (nonatomic, retain, nullable) CIImage *inputImage;
  /// The height of the displayable histogram image.
  @property (nonatomic) float height;
  /// The fraction of the right portion of the histogram image to make lighter.
  @property (nonatomic) float highLimit;
  /// The fraction of the left portion of the histogram image to make darker.
  @property (nonatomic) float lowLimit;
@end

/// The protocol for the KMeans filter.
///
/// Create a palette of the most common colors found in the image.
@protocol CIKMeans <CIAreaReductionFilter>
  /// Specifies the color seeds to use for k-means clustering, either passed as an image or an array of colors.
  @property (nonatomic, retain, nullable) CIImage *inputMeans;
  /// Specifies how many k-means color clusters should be used.
  @property (nonatomic) NSInteger count;
  /// Specifies how many k-means passes should be performed.
  @property (nonatomic) float passes;
  /// Specifies whether the k-means color palette should be computed in a perceptual color space.
  @property (nonatomic) bool perceptual;
@end

/// The protocol for the Row Average filter.
///
/// Calculates the average color for each row of the specified area in an image, returning the result in a 1D image.
@protocol CIRowAverage <CIAreaReductionFilter>
@end


NS_CLASS_AVAILABLE(10_15, 13_0)
@interface CIFilter (Builtins)

// CICategoryGradient
+ (CIFilter<CIDistanceGradientFromRedMask>*) distanceGradientFromRedMaskFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CIGaussianGradient>*) gaussianGradientFilter;
+ (CIFilter<CIHueSaturationValueGradient>*) hueSaturationValueGradientFilter;
+ (CIFilter<CILinearGradient>*) linearGradientFilter;
+ (CIFilter<CIRadialGradient>*) radialGradientFilter;
+ (CIFilter<CISignedDistanceGradientFromRedMask>*) signedDistanceGradientFromRedMaskFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CISmoothLinearGradient>*) smoothLinearGradientFilter;

// CICategorySharpen
+ (CIFilter<CISharpenLuminance>*) sharpenLuminanceFilter;
+ (CIFilter<CIUnsharpMask>*) unsharpMaskFilter;

// CICategoryHalftoneEffect
+ (CIFilter<CICircularScreen>*) circularScreenFilter;
+ (CIFilter<CICMYKHalftone>*) CMYKHalftone;
+ (CIFilter<CIDotScreen>*) dotScreenFilter;
+ (CIFilter<CIHatchedScreen>*) hatchedScreenFilter;
+ (CIFilter<CILineScreen>*) lineScreenFilter;

// CICategoryGeometryAdjustment
+ (CIFilter<CIBicubicScaleTransform>*) bicubicScaleTransformFilter;
+ (CIFilter<CIEdgePreserveUpsample>*) edgePreserveUpsampleFilter;
+ (CIFilter<CIKeystoneCorrectionCombined>*) keystoneCorrectionCombinedFilter;
+ (CIFilter<CIKeystoneCorrectionHorizontal>*) keystoneCorrectionHorizontalFilter;
+ (CIFilter<CIKeystoneCorrectionVertical>*) keystoneCorrectionVerticalFilter;
+ (CIFilter<CILanczosScaleTransform>*) lanczosScaleTransformFilter;
+ (CIFilter<CIMaximumScaleTransform>*) maximumScaleTransformFilter NS_AVAILABLE(15_0, 18_0);
+ (CIFilter<CIPerspectiveCorrection>*) perspectiveCorrectionFilter;
+ (CIFilter<CIPerspectiveRotate>*) perspectiveRotateFilter;
+ (CIFilter<CIPerspectiveTransform>*) perspectiveTransformFilter;
+ (CIFilter<CIPerspectiveTransformWithExtent>*) perspectiveTransformWithExtentFilter;
+ (CIFilter<CIStraighten>*) straightenFilter;

// CICategoryTransition
+ (CIFilter<CIAccordionFoldTransition>*) accordionFoldTransitionFilter;
+ (CIFilter<CIBarsSwipeTransition>*) barsSwipeTransitionFilter;
+ (CIFilter<CICopyMachineTransition>*) copyMachineTransitionFilter NS_RETURNS_NOT_RETAINED;
+ (CIFilter<CIDisintegrateWithMaskTransition>*) disintegrateWithMaskTransitionFilter;
+ (CIFilter<CIDissolveTransition>*) dissolveTransitionFilter;
+ (CIFilter<CIFlashTransition>*) flashTransitionFilter;
+ (CIFilter<CIModTransition>*) modTransitionFilter;
+ (CIFilter<CIPageCurlTransition>*) pageCurlTransitionFilter;
+ (CIFilter<CIPageCurlWithShadowTransition>*) pageCurlWithShadowTransitionFilter;
+ (CIFilter<CIRippleTransition>*) rippleTransitionFilter;
+ (CIFilter<CISwipeTransition>*) swipeTransitionFilter;

// CICategoryCompositeOperation
+ (CIFilter<CICompositeOperation>*) additionCompositingFilter;
+ (CIFilter<CICompositeOperation>*) colorBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) colorBurnBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) colorDodgeBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) darkenBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) differenceBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) divideBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) exclusionBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) hardLightBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) hueBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) lightenBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) linearBurnBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) linearDodgeBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) linearLightBlendModeFilter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CICompositeOperation>*) luminosityBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) maximumCompositingFilter;
+ (CIFilter<CICompositeOperation>*) minimumCompositingFilter;
+ (CIFilter<CICompositeOperation>*) multiplyBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) multiplyCompositingFilter;
+ (CIFilter<CICompositeOperation>*) overlayBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) pinLightBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) saturationBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) screenBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) softLightBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) sourceAtopCompositingFilter;
+ (CIFilter<CICompositeOperation>*) sourceInCompositingFilter;
+ (CIFilter<CICompositeOperation>*) sourceOutCompositingFilter;
+ (CIFilter<CICompositeOperation>*) sourceOverCompositingFilter;
+ (CIFilter<CICompositeOperation>*) subtractBlendModeFilter;
+ (CIFilter<CICompositeOperation>*) vividLightBlendModeFilter NS_AVAILABLE(12_0, 15_0);

// CICategoryColorAdjustment
+ (CIFilter<CIColorAbsoluteDifference>*) colorAbsoluteDifferenceFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIColorClamp>*) colorClampFilter;
+ (CIFilter<CIColorControls>*) colorControlsFilter;
+ (CIFilter<CIColorMatrix>*) colorMatrixFilter;
+ (CIFilter<CIColorPolynomial>*) colorPolynomialFilter;
+ (CIFilter<CIColorThreshold>*) colorThresholdFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIColorThresholdOtsu>*) colorThresholdOtsuFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIDepthToDisparity>*) depthToDisparityFilter;
+ (CIFilter<CIDisparityToDepth>*) disparityToDepthFilter;
+ (CIFilter<CIExposureAdjust>*) exposureAdjustFilter;
+ (CIFilter<CIGammaAdjust>*) gammaAdjustFilter;
+ (CIFilter<CIHueAdjust>*) hueAdjustFilter;
+ (CIFilter<CILinearToSRGBToneCurve>*) linearToSRGBToneCurveFilter;
+ (CIFilter<CISRGBToneCurveToLinear>*) sRGBToneCurveToLinearFilter;
+ (CIFilter<CISystemToneMap>*) systemToneMapFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CITemperatureAndTint>*) temperatureAndTintFilter;
+ (CIFilter<CIToneCurve>*) toneCurveFilter;
+ (CIFilter<CIToneMapHeadroom>*) toneMapHeadroomFilter NS_AVAILABLE(15_0, 18_0);
+ (CIFilter<CIVibrance>*) vibranceFilter;
+ (CIFilter<CIWhitePointAdjust>*) whitePointAdjustFilter;

// CICategoryColorEffect
+ (CIFilter<CIColorCrossPolynomial>*) colorCrossPolynomialFilter;
+ (CIFilter<CIColorCube>*) colorCubeFilter;
+ (CIFilter<CIColorCubesMixedWithMask>*) colorCubesMixedWithMaskFilter;
+ (CIFilter<CIColorCubeWithColorSpace>*) colorCubeWithColorSpaceFilter;
+ (CIFilter<CIColorCurves>*) colorCurvesFilter;
+ (CIFilter<CIColorInvert>*) colorInvertFilter;
+ (CIFilter<CIColorMap>*) colorMapFilter;
+ (CIFilter<CIColorMonochrome>*) colorMonochromeFilter;
+ (CIFilter<CIColorPosterize>*) colorPosterizeFilter;
+ (CIFilter<CIConvertLab>*) convertLabToRGBFilter NS_AVAILABLE(13_0, 16_0);
+ (CIFilter<CIConvertLab>*) convertRGBtoLabFilter NS_AVAILABLE(13_0, 16_0);
+ (CIFilter<CIDither>*) ditherFilter;
+ (CIFilter<CIDocumentEnhancer>*) documentEnhancerFilter;
+ (CIFilter<CIFalseColor>*) falseColorFilter;
+ (CIFilter<CILabDeltaE>*) LabDeltaE;
+ (CIFilter<CIMaskToAlpha>*) maskToAlphaFilter;
+ (CIFilter<CIMaximumComponent>*) maximumComponentFilter;
+ (CIFilter<CIMinimumComponent>*) minimumComponentFilter;
+ (CIFilter<CIPaletteCentroid>*) paletteCentroidFilter;
+ (CIFilter<CIPalettize>*) palettizeFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectChromeFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectFadeFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectInstantFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectMonoFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectNoirFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectProcessFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectTonalFilter;
+ (CIFilter<CIPhotoEffect>*) photoEffectTransferFilter;
+ (CIFilter<CISepiaTone>*) sepiaToneFilter;
+ (CIFilter<CIThermal>*) thermalFilter;
+ (CIFilter<CIVignette>*) vignetteFilter;
+ (CIFilter<CIVignetteEffect>*) vignetteEffectFilter;
+ (CIFilter<CIXRay>*) xRayFilter;

// CICategoryDistortionEffect
+ (CIFilter<CIBumpDistortion>*) bumpDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIBumpDistortionLinear>*) bumpDistortionLinearFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CICircleSplashDistortion>*) circleSplashDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CICircularWrap>*) circularWrapFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIDisplacementDistortion>*) displacementDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIDroste>*) drosteFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIGlassDistortion>*) glassDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIGlassLozenge>*) glassLozengeFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIHoleDistortion>*) holeDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CILightTunnel>*) lightTunnelFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CINinePartStretched>*) ninePartStretchedFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CINinePartTiled>*) ninePartTiledFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIPinchDistortion>*) pinchDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIStretchCrop>*) stretchCropFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CITorusLensDistortion>*) torusLensDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CITwirlDistortion>*) twirlDistortionFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIVortexDistortion>*) vortexDistortionFilter NS_AVAILABLE(11_0, 14_0);

// CICategoryTileEffect
+ (CIFilter<CIAffineClamp>*) affineClampFilter;
+ (CIFilter<CIAffineTile>*) affineTileFilter;
+ (CIFilter<CIEightfoldReflectedTile>*) eightfoldReflectedTileFilter;
+ (CIFilter<CIFourfoldReflectedTile>*) fourfoldReflectedTileFilter;
+ (CIFilter<CIFourfoldRotatedTile>*) fourfoldRotatedTileFilter;
+ (CIFilter<CIFourfoldTranslatedTile>*) fourfoldTranslatedTileFilter;
+ (CIFilter<CIGlideReflectedTile>*) glideReflectedTileFilter;
+ (CIFilter<CIKaleidoscope>*) kaleidoscopeFilter;
+ (CIFilter<CIOpTile>*) opTileFilter;
+ (CIFilter<CIParallelogramTile>*) parallelogramTileFilter;
+ (CIFilter<CIPerspectiveTile>*) perspectiveTileFilter;
+ (CIFilter<CISixfoldReflectedTile>*) sixfoldReflectedTileFilter;
+ (CIFilter<CISixfoldRotatedTile>*) sixfoldRotatedTileFilter;
+ (CIFilter<CITriangleKaleidoscope>*) triangleKaleidoscopeFilter;
+ (CIFilter<CITriangleTile>*) triangleTileFilter;
+ (CIFilter<CITwelvefoldReflectedTile>*) twelvefoldReflectedTileFilter;

// CICategoryGenerator
+ (CIFilter<CIAttributedTextImageGenerator>*) attributedTextImageGeneratorFilter;
+ (CIFilter<CIAztecCodeGenerator>*) aztecCodeGeneratorFilter;
+ (CIFilter<CIBarcodeGenerator>*) barcodeGeneratorFilter;
+ (CIFilter<CIBlurredRectangleGenerator>*) blurredRectangleGeneratorFilter NS_AVAILABLE(14_0, 17_0);
+ (CIFilter<CIBlurredRoundedRectangleGenerator>*) blurredRoundedRectangleGeneratorFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CICheckerboardGenerator>*) checkerboardGeneratorFilter;
+ (CIFilter<CICode128BarcodeGenerator>*) code128BarcodeGeneratorFilter;
+ (CIFilter<CILenticularHaloGenerator>*) lenticularHaloGeneratorFilter;
+ (CIFilter<CIMeshGenerator>*) meshGeneratorFilter;
+ (CIFilter<CIPDF417BarcodeGenerator>*) PDF417BarcodeGenerator;
+ (CIFilter<CIQRCodeGenerator>*) QRCodeGenerator;
+ (CIFilter<CIRandomGenerator>*) randomGeneratorFilter;
+ (CIFilter<CIRoundedQRCodeGenerator>*) roundedQRCodeGeneratorFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CIRoundedRectangleGenerator>*) roundedRectangleGeneratorFilter;
+ (CIFilter<CIRoundedRectangleStrokeGenerator>*) roundedRectangleStrokeGeneratorFilter NS_AVAILABLE(14_0, 17_0);
+ (CIFilter<CIStarShineGenerator>*) starShineGeneratorFilter;
+ (CIFilter<CIStripesGenerator>*) stripesGeneratorFilter;
+ (CIFilter<CISunbeamsGenerator>*) sunbeamsGeneratorFilter;
+ (CIFilter<CITextImageGenerator>*) textImageGeneratorFilter;

// CICategoryStylize
+ (CIFilter<CIBlendWithMask>*) blendWithAlphaMaskFilter;
+ (CIFilter<CIBlendWithMask>*) blendWithBlueMaskFilter;
+ (CIFilter<CIBlendWithMask>*) blendWithMaskFilter;
+ (CIFilter<CIBlendWithMask>*) blendWithRedMaskFilter;
+ (CIFilter<CIBloom>*) bloomFilter;
+ (CIFilter<CICannyEdgeDetector>*) cannyEdgeDetectorFilter NS_AVAILABLE(14_0, 17_0);
+ (CIFilter<CIComicEffect>*) comicEffectFilter;
+ (CIFilter<CIConvolution>*) convolution3X3Filter;
+ (CIFilter<CIConvolution>*) convolution5X5Filter;
+ (CIFilter<CIConvolution>*) convolution7X7Filter;
+ (CIFilter<CIConvolution>*) convolution9HorizontalFilter;
+ (CIFilter<CIConvolution>*) convolution9VerticalFilter;
+ (CIFilter<CIConvolution>*) convolutionRGB3X3Filter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CIConvolution>*) convolutionRGB5X5Filter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CIConvolution>*) convolutionRGB7X7Filter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CIConvolution>*) convolutionRGB9HorizontalFilter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CIConvolution>*) convolutionRGB9VerticalFilter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CICoreMLModel>*) coreMLModelFilter;
+ (CIFilter<CICrystallize>*) crystallizeFilter;
+ (CIFilter<CIDepthOfField>*) depthOfFieldFilter;
+ (CIFilter<CIEdges>*) edgesFilter;
+ (CIFilter<CIEdgeWork>*) edgeWorkFilter;
+ (CIFilter<CIGaborGradients>*) gaborGradientsFilter;
+ (CIFilter<CIGloom>*) gloomFilter;
+ (CIFilter<CIHeightFieldFromMask>*) heightFieldFromMaskFilter;
+ (CIFilter<CIHexagonalPixellate>*) hexagonalPixellateFilter;
+ (CIFilter<CIHighlightShadowAdjust>*) highlightShadowAdjustFilter;
+ (CIFilter<CILineOverlay>*) lineOverlayFilter;
+ (CIFilter<CIMix>*) mixFilter;
+ (CIFilter<CIPersonSegmentation>*) personSegmentationFilter NS_AVAILABLE(12_0, 15_0);
+ (CIFilter<CIPixellate>*) pixellateFilter;
+ (CIFilter<CIPointillize>*) pointillizeFilter;
+ (CIFilter<CISaliencyMap>*) saliencyMapFilter;
+ (CIFilter<CIShadedMaterial>*) shadedMaterialFilter;
+ (CIFilter<CISobelGradients>*) sobelGradientsFilter NS_AVAILABLE(14_0, 17_0);
+ (CIFilter<CISpotColor>*) spotColorFilter;
+ (CIFilter<CISpotLight>*) spotLightFilter;

// CICategoryBlur
+ (CIFilter<CIBokehBlur>*) bokehBlurFilter;
+ (CIFilter<CIBoxBlur>*) boxBlurFilter;
+ (CIFilter<CIDiscBlur>*) discBlurFilter;
+ (CIFilter<CIGaussianBlur>*) gaussianBlurFilter;
+ (CIFilter<CIMaskedVariableBlur>*) maskedVariableBlurFilter;
+ (CIFilter<CIMedian>*) medianFilter;
+ (CIFilter<CIMorphologyGradient>*) morphologyGradientFilter;
+ (CIFilter<CIMorphologyMaximum>*) morphologyMaximumFilter;
+ (CIFilter<CIMorphologyMinimum>*) morphologyMinimumFilter;
+ (CIFilter<CIMorphologyRectangleMaximum>*) morphologyRectangleMaximumFilter;
+ (CIFilter<CIMorphologyRectangleMinimum>*) morphologyRectangleMinimumFilter;
+ (CIFilter<CIMotionBlur>*) motionBlurFilter;
+ (CIFilter<CINoiseReduction>*) noiseReductionFilter;
+ (CIFilter<CIZoomBlur>*) zoomBlurFilter;

// CICategoryReduction
+ (CIFilter<CIAreaHistogram>*) areaAlphaWeightedHistogramFilter NS_AVAILABLE(15_0, 18_0);
+ (CIFilter<CIAreaAverage>*) areaAverageFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaAverageMaximumRed>*) areaAverageMaximumRedFilter NS_AVAILABLE(26_0, 26_0);
+ (CIFilter<CIAreaBoundsRed>*) areaBoundsRedFilter NS_AVAILABLE(15_0, 18_0);
+ (CIFilter<CIAreaHistogram>*) areaHistogramFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaLogarithmicHistogram>*) areaLogarithmicHistogramFilter NS_AVAILABLE(13_0, 16_0);
+ (CIFilter<CIAreaMaximum>*) areaMaximumFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaMaximumAlpha>*) areaMaximumAlphaFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaMinimum>*) areaMinimumFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaMinimumAlpha>*) areaMinimumAlphaFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaMinMax>*) areaMinMaxFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIAreaMinMaxRed>*) areaMinMaxRedFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIColumnAverage>*) columnAverageFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIHistogramDisplay>*) histogramDisplayFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIKMeans>*) KMeansFilter NS_AVAILABLE(11_0, 14_0);
+ (CIFilter<CIRowAverage>*) rowAverageFilter NS_AVAILABLE(11_0, 14_0);

@end

NS_ASSUME_NONNULL_END

#endif // TARGET_OS_OSX || TARGET_OS_IOS || TARGET_OS_TV

#endif /* __OBJC__ */

#endif /* CIFILTERBUILTINS_H */
