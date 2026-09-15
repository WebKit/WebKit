/* 
   CoreImage - CIColor.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved. 
*/

#ifndef CICOLOR_H
#define CICOLOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>
#import <CoreImage/CIVector.h>

NS_ASSUME_NONNULL_BEGIN

/// The Core Image class that defines a color object.
///
/// Use `CIColor` instances in conjunction with other Core Image classes, such as ``CIFilter-class`` and ``CIKernel``.
/// Many of the built-in Core Image filters have one or more `CIColor` inputs that you can set to affect the filter's
/// behavior.
/// 
/// ### Color Model
/// 
/// A color is defined as a N-dimensional model where each dimension's color component is represented
/// by intensity values. A color component may also be referred to as a color channel. An RGB color model, for example, 
/// is three-dimensional and the red, green, and blue component intensities define each unique color. 
/// 
/// ### Color Space
/// 
/// A color is also defined by a color space that locates the axes of N-dimensional model within the greater
/// volume of human perceivable colors.  Core Image uses `CGColorSpace` instances to specify a variety of different
/// color spaces such as sRGB, P3, BT.2020, etc. The `CGColorSpace` also defines if the color space is coded
/// linearly or in a non-linear perceptual curve.
/// (For more information on `CGColorSpace` see <doc://com.apple.documentation/documentation/coregraphics/cgcolorspace>)
/// 
/// ### Color Range
/// 
/// Standard dynamic range (SDR) color color component values range from `0.0` to `1.0`, with `0.0` 
/// representing an 0% of that component and `1.0` representing 100%. In contrast, high dynamic range (HDR) color values 
/// can be less than `0.0` (for more saturation) or greater than `1.0` (for more brightness).
/// 
/// ### Color Opacity
/// 
/// `CIColor` instances also have an alpha component, which represents the opacity of the color, with 0.0 meaning completely 
/// transparent and 1.0 meaning completely opaque. If a color does not have an explicit alpha component, Core Image 
/// assumes that the alpha component equals 1.0. With `CIColor` that color components values are not premultiplied. 
/// So for example, a semi-transparent pure red `CIColor` is represented by RGB `1.0,0.0,0.0` and A `0.5`.  In contrast 
/// color components values in ``CIImage`` buffers or read in ``CIKernel`` samplers are premultiplied by default.
/// 
NS_CLASS_AVAILABLE(10_4, 5_0) NS_SWIFT_SENDABLE
@interface CIColor : NSObject <NSSecureCoding, NSCopying>
{
    void *_priv;
    void *_pad[3];
}

/// Create a Core Image color object with a Core Graphics color object.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (instancetype)colorWithCGColor:(CGColorRef)color;

/// Create a Core Image color object in the sRGB color space 
/// with the specified red, green, blue, and alpha component values.
/// 
/// On macOS before 10.10, the CIColor's color space will be Generic RGB.
/// - Parameters:
///   - red: The color's unpremultiplied red component value between 0 and 1.
///   - green: The color's unpremultiplied green component value between 0 and 1.
///   - blue: The color's unpremultiplied blue component value between 0 and 1.
///   - alpha: The color's alpha (opacity) value between 0 and 1.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (instancetype)colorWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;

/// Create a Core Image color object in the sRGB color space 
/// with the specified red, green, and blue component values.
///  
/// On macOS before 10.10, the CIColor's color space will be Generic RGB. 
/// - Parameters:
///   - red: The color's unpremultiplied red component value between 0 and 1.
///   - green: The color's unpremultiplied green component value between 0 and 1.
///   - blue: The color's unpremultiplied blue component value between 0 and 1.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (instancetype)colorWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue;

/// Create a Core Image color object 
/// with the specified red, green, blue, and alpha component values
/// as measured in the specified color space.
/// 
/// This will return `null` if the `CGColorSpace` is not `kCGColorSpaceModelRGB`. 
/// 
/// The RGB values can be outside the `0...1` range if the `CGColorSpace` is unclamped.
/// - Parameters:
///   - red: The color's unpremultiplied red component value.
///   - green: The color's unpremultiplied green component value.
///   - blue: The color's unpremultiplied blue component value.
///   - alpha: The color's alpha (opacity) value between 0 and 1.
///   - colorSpace: The color's `CGColorSpace` which must have `kCGColorSpaceModelRGB`.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (nullable instancetype)colorWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha 
                           colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);

/// Create a Core Image color object 
/// with the specified red, green, and blue component values
/// as measured in the specified color space.
/// 
/// This will return `null` if the `CGColorSpace` is not `kCGColorSpaceModelRGB`. 
/// 
/// The RGB values can be outside the `0...1` range if the `CGColorSpace` is unclamped.
/// - Parameters:
///   - red: The color's unpremultiplied red component value.
///   - green: The color's unpremultiplied green component value.
///   - blue: The color's unpremultiplied blue component value.
///   - colorSpace: The color's `CGColorSpace` which must have `kCGColorSpaceModelRGB`.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (nullable instancetype)colorWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue 
                           colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);

/// Create a Core Image color object in the sRGB color space using a string containing the RGBA color component values.
/// 
/// On macOS before 10.10, the CIColor's color space will be Generic RGB. 
/// 
/// - Parameters:
///   - representation: A string that contains color and alpha float values. 
///   For example, the string: `"0.5 0.7 0.3 1.0"` indicates an RGB color whose components 
///   are 50% red, 70% green, 30% blue, and 100% opaque. 
///   If the string contains only 3 float values, the alpha component will be `1.0`
///   If the string contains no float values, then ``/CIColor/clearColor`` will be returned.
/// - Returns:
///    An autoreleased ``CIColor`` instance.
+ (instancetype)colorWithString:(NSString *)representation;


/// Create a Core Image color object with a Core Graphics color object.
/// - Returns:
///    An initialized ``CIColor`` instance.
- (instancetype)initWithCGColor:(CGColorRef)color NS_DESIGNATED_INITIALIZER;

/// Initialize a Core Image color object in the sRGB color space 
/// with the specified red, green, blue, and alpha component values.
/// 
/// On macOS before 10.10, the CIColor's color space will be Generic RGB. 
/// - Parameters:
///   - red: The color's unpremultiplied red component value between 0 and 1.
///   - green: The color's unpremultiplied green component value between 0 and 1.
///   - blue: The color's unpremultiplied blue component value between 0 and 1.
///   - alpha: The color's alpha (opacity) value between 0 and 1.
/// - Returns:
///    An initialized ``CIColor`` instance.
- (instancetype)initWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;

/// Initialize a Core Image color object in the sRGB color space 
/// with the specified red, green, and blue component values.
/// 
/// On macOS before 10.10, the CIColor's color space will be Generic RGB. 
/// - Parameters:
///   - red: The color's unpremultiplied red component value between 0 and 1.
///   - green: The color's unpremultiplied green component value between 0 and 1.
///   - blue: The color's unpremultiplied blue component value between 0 and 1.
/// - Returns:
///    An initialized ``CIColor`` instance.
- (instancetype)initWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue NS_AVAILABLE(10_11, 9_0);

/// Initialize a Core Image color object 
/// with the specified red, green, and blue component values
/// as measured in the specified color space.
/// 
/// This will return null if the `CGColorSpace` is not `kCGColorSpaceModelRGB`. 
/// The RGB values can be outside the `0...1` range if the `CGColorSpace` is unclamped.
/// - Parameters:
///   - red: The color's unpremultiplied red component value.
///   - green: The color's unpremultiplied green component value.
///   - blue: The color's unpremultiplied blue component value.
///   - alpha: The color's alpha (opacity) value between 0 and 1.
///   - colorSpace: The color's `CGColorSpace` which must have `kCGColorSpaceModelRGB`.
/// - Returns:
///    An initialized ``CIColor`` instance.
- (nullable instancetype)initWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue
                               alpha:(CGFloat)alpha 
                          colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);

/// Initialize a Core Image color object 
/// with the specified red, green, and blue component values
/// as measured in the specified color space.
/// 
/// This will return null if the `CGColorSpace` is not `kCGColorSpaceModelRGB`. 
/// The RGB values can be outside the `0...1` range if the `CGColorSpace` is unclamped.
/// - Parameters:
///   - red: The color's unpremultiplied red component value.
///   - green: The color's unpremultiplied green component value.
///   - blue: The color's unpremultiplied blue component value.
///   - colorSpace: The color's `CGColorSpace` which must have `kCGColorSpaceModelRGB`.
/// - Returns:
///    An initialized ``CIColor`` instance.
- (nullable instancetype)initWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue
                          colorSpace:(CGColorSpaceRef)colorSpace NS_AVAILABLE(10_12, 10_0);


/// Returns the color components of the color including alpha.
///   
/// This number includes the alpha component if the color contains one.
/// 
/// Typically this number will be `4` for red, green, blue, and alpha.
/// If the ``CIColor`` was initialized with a `CGColor` then the number 
/// will be the same as calling `CGColorGetNumberOfComponents()`
@property (readonly) size_t numberOfComponents;

/// Return a pointer to an array of `CGFloat` values including alpha.
/// 
/// Typically this array will contain `4` `CGFloat` values for red, green, blue, and alpha. 
/// If the ``CIColor`` was initialized with a `CGColor` then returned pointer 
/// will be the same as calling `CGColorGetComponents()`
@property (readonly) const CGFloat *components NS_RETURNS_INNER_POINTER;

/// Returns the alpha value of the color.
@property (readonly) CGFloat alpha;

/// Returns the `CGColorSpace` associated with the color
@property (readonly) CGColorSpaceRef colorSpace CF_RETURNS_NOT_RETAINED;

/// Returns the unpremultiplied red component of the color.
/// 
/// If the ``CIColor`` was initialized with a `CGColor` in a non-RGB `CGColorSpace`
/// then it will be converted to sRGB to get the red component.
@property (readonly) CGFloat red;

/// Returns the unpremultiplied green component of the color.
/// 
/// If the ``CIColor`` was initialized with a `CGColor` in a non-RGB `CGColorSpace`
/// then it will be converted to sRGB to get the green component.
@property (readonly) CGFloat green;

/// Returns the unpremultiplied blue component of the color.
/// 
/// If the ``CIColor`` was initialized with a `CGColor` in a non-RGB `CGColorSpace`
/// then it will be converted to sRGB to get the green component.
@property (readonly) CGFloat blue;

/// Returns a formatted string with the unpremultiplied color and alpha components of the color.
/// 
/// The string representation always has four components: red, green, blue, and alpha. 
/// 
/// Some example string representations of colors:
/// 
/// `CIColor`                                       | `stringRepresentation`
/// ----------------------------------------------- | --------------
/// `[CIColor colorWithRed:0.2 green:0.4 blue:0.6]` | `"0.2 0.4 0.6 1.0"`
/// ``/CIColor/yellowColor``                        | `"1.0 1.0 0.0 1.0"`
/// 
/// To create a ``CIColor`` instance from a string representation, use the ``colorWithString:`` method.
/// 
/// If the ``CIColor`` was initialized with a `CGColor` in a non-RGB `CGColorSpace`
/// then it will be converted to sRGB to get the red, green, and blue components.
/// 
/// This property is not KVO-safe because it returns a new `NSString` instance each time.
/// The value of the `NSString` will be the same each time it is called.
/// 
@property (readonly) NSString *stringRepresentation;


/* Convenience constant CIColors in the sRGB colorspace. */

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0,0,0` and alpha value `1`.
@property (class, strong, readonly) CIColor *blackColor   NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `1,1,1` and alpha value `1`.
@property (class, strong, readonly) CIColor *whiteColor   NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0.5,0.5,0.5` and alpha value `1`.
@property (class, strong, readonly) CIColor *grayColor    NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `1,0,0` and alpha value `1`.
@property (class, strong, readonly) CIColor *redColor     NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0,1,0` and alpha value `1`.
@property (class, strong, readonly) CIColor *greenColor   NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0,0,1` and alpha value `1`.
@property (class, strong, readonly) CIColor *blueColor    NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0,1,1` and alpha value `1`.
@property (class, strong, readonly) CIColor *cyanColor    NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `1,0,1` and alpha value `1`.
@property (class, strong, readonly) CIColor *magentaColor NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `1,1,0` and alpha value `1`.
@property (class, strong, readonly) CIColor *yellowColor  NS_AVAILABLE(10_12, 10_0);

/// Returns a singleton Core Image color instance in the sRGB color space with RGB values `0,0,0` and alpha value `0`.
@property (class, strong, readonly) CIColor *clearColor   NS_AVAILABLE(10_12, 10_0);

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CICOLOR_H */
