/* 
   CoreImage - CIVector.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved.
*/

#ifndef CIVECTOR_H
#define CIVECTOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The Core Image class that defines a vector object.
///
/// A `CIVector` can store one or more `CGFloat` in one object. They can store a group of float values
/// for a variety of different uses such as coordinate points, direction vectors, geometric rectangles, 
/// transform matrices, convolution weights, or just a list a parameter values. 
///
/// You use `CIVector` objects in conjunction with other Core Image classes, such as ``CIFilter-class`` 
/// and ``CIKernel``.  Many of the built-in Core Image filters have one or more `CIVector` inputs that 
/// you can set to affect the filter's behavior.
///
NS_CLASS_AVAILABLE(10_4, 5_0) NS_SWIFT_SENDABLE
@interface CIVector : NSObject <NSCopying, NSSecureCoding>
{
    size_t _count;
    
    union {
        CGFloat vec[4];
        CGFloat *ptr;
    }
    _u;
}

/* Create a new vector object. */

/// Create a Core Image vector object that is initialized with the specified values.
/// - Parameters:
///   - values: The pointer `CGFloat` values to initialize the vector with.
///   - count: The number of `CGFloats` specified by the `values` parameter.
/// - Returns:
///    An autoreleased ``CIVector`` object of length `count`.
+ (instancetype)vectorWithValues:(const CGFloat *)values count:(size_t)count;

/// Create a Core Image vector object that is initialized with one value.
/// - Parameters:
///   - x: The value for the first position in the vector.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 1.
+ (instancetype)vectorWithX:(CGFloat)x;

/// Create a Core Image vector object that is initialized with two values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 2.
+ (instancetype)vectorWithX:(CGFloat)x Y:(CGFloat)y;

/// Create a Core Image vector object that is initialized with three values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
///   - z: The value for the third position in the vector.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 3.
+ (instancetype)vectorWithX:(CGFloat)x Y:(CGFloat)y Z:(CGFloat)z;

/// Create a Core Image vector object that is initialized with four values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
///   - z: The value for the third position in the vector.
///   - w: The value for the forth position in the vector.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 4.
+ (instancetype)vectorWithX:(CGFloat)x Y:(CGFloat)y Z:(CGFloat)z W:(CGFloat)w;

/// Create a Core Image vector object that is initialized with two values provided by a `CGPoint` structure.
/// 
/// The `CGRect` structure’s `y` and `y` values 
/// are stored in the vector’s two values.
/// - Parameters:
///   - p: The `CGPoint` structure.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 2.
+ (instancetype)vectorWithCGPoint:(CGPoint)p NS_AVAILABLE(10_9, 5_0);

/// Create a Core Image vector object that is initialized with four values provided by a `CGRect` structure.
/// 
/// The `CGRect` structure’s `x`, `y`, `height` and `width` values 
/// are stored in the vector’s four values.
/// - Parameters:
///   - r: The `CGRect` structure.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 4.
+ (instancetype)vectorWithCGRect:(CGRect)r NS_AVAILABLE(10_9, 5_0);

/// Create a Core Image vector object that is initialized with six values provided by a `CGAffineTransform` structure.
/// 
/// The `CGAffineTransform` structure’s `a`, `b`, `c`, `d`, `tx` and `ty` values 
/// are stored in the vector’s six values.
/// - Parameters:
///   - t: The `CGAffineTransform` structure.
/// - Returns:
///    An autoreleased ``CIVector`` object of length 6.
+ (instancetype)vectorWithCGAffineTransform:(CGAffineTransform)t NS_AVAILABLE(10_9, 5_0);

/// Create a Core Image vector object with values provided in a string representation.
/// - Parameters:
///   - representation: A string that is in one of the formats returned by the `stringRepresentation` method.
/// - Returns:
///    An autoreleased ``CIVector`` object.
+ (instancetype)vectorWithString:(NSString *)representation;

/* Initializers. */

/// Initialize a Core Image vector object with the specified the values.
/// - Parameters:
///   - values: A pointer `CGFloat` values for vector.
///   - count: The number of `CGFloats` specified by the `values` parameter.
/// - Returns:
///    An initialized ``CIVector`` object of length `count`.
- (instancetype)initWithValues:(const CGFloat *)values count:(size_t)count NS_DESIGNATED_INITIALIZER;

/// Initialize a Core Image vector object with one value.
/// - Parameters:
///   - x: The value for the first position in the vector.
/// - Returns:
///    An initialized ``CIVector`` object of length 1.
- (instancetype)initWithX:(CGFloat)x;

/// Initialize a Core Image vector object with two values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
/// - Returns:
///    An initialized ``CIVector`` object of length 2.
- (instancetype)initWithX:(CGFloat)x Y:(CGFloat)y;

/// Initialize a Core Image vector object with three values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
///   - z: The value for the third position in the vector.
/// - Returns:
///    An initialized ``CIVector`` object of length 3.
- (instancetype)initWithX:(CGFloat)x Y:(CGFloat)y Z:(CGFloat)z;

/// Initialize a Core Image vector object with four values.
/// - Parameters:
///   - x: The value for the first position in the vector.
///   - y: The value for the second position in the vector.
///   - z: The value for the third position in the vector.
///   - w: The value for the forth position in the vector.
/// - Returns:
///    An initialized ``CIVector`` object of length 4.
- (instancetype)initWithX:(CGFloat)x Y:(CGFloat)y Z:(CGFloat)z W:(CGFloat)w;

/// Initialize a Core Image vector object with two values provided by a `CGPoint` structure.
/// 
/// The `CGRect` structure’s `y` and `y` values 
/// are stored in the vector’s two values.
/// - Parameters:
///   - p: The `CGPoint` structure.
/// - Returns:
///    An initialized ``CIVector`` object of length 2.
- (instancetype)initWithCGPoint:(CGPoint)p NS_AVAILABLE(10_9, 5_0);

/// Initialize a Core Image vector object with four values provided by a `CGRect` structure.
/// 
/// The `CGRect` structure’s `x`, `y`, `height` and `width` values 
/// are stored in the vector’s four values.
/// - Parameters:
///   - r: The `CGRect` structure.
/// - Returns:
///    An initialized ``CIVector`` object of length 4.
- (instancetype)initWithCGRect:(CGRect)r NS_AVAILABLE(10_9, 5_0);

/// Initialize a Core Image vector object with six values provided by a `CGAffineTransform` structure.
/// 
/// The `CGAffineTransform` structure’s `a`, `b`, `c`, `c`, `tx` and `ty` values 
/// are stored in the vector’s six values.
/// - Parameters:
///   - t: The `CGAffineTransform` structure.
/// - Returns:
///    An initialized ``CIVector`` object of length 6.
- (instancetype)initWithCGAffineTransform:(CGAffineTransform)t NS_AVAILABLE(10_9, 5_0);

/// Initialize a Core Image vector object with values provided in a string representation.
/// - Parameters:
///   - representation: A string that is in one of the formats returned by the `stringRepresentation` method.
/// - Returns:
///    An initialized ``CIVector`` object.
- (instancetype)initWithString:(NSString *)representation;

/// Returns a value from a specific position in the vector.
/// 
/// The numbering of elements in a vector begins with zero.
/// - Parameters:
///   - index: The position in the vector of the value that you want to retrieve.
/// - Returns:
///    The value retrieved from the vector or `0` if the position is undefined.
- (CGFloat)valueAtIndex:(size_t)index;

/// The number of items in the vector.
@property (readonly) size_t count;

/* Properties. */

/// The value located in the first position in the vector.
@property (readonly) CGFloat X;

/// The value located in the second position in the vector.
@property (readonly) CGFloat Y;

/// The value located in the third position in the vector.
@property (readonly) CGFloat Z;

/// The value located in the forth position in the vector.
@property (readonly) CGFloat W;

/// Returns the values in the vector as a `CGPoint` structure.
/// - Returns:
///    Reading this property returns a `CGPoint` structure 
///    from the `X` and `Y` values from the vector.
@property (readonly) CGPoint CGPointValue NS_AVAILABLE(10_9, 5_0);

/// Returns the values in the vector as a `CGRect` structure.
/// - Returns:
///    Reading this property creates a `CGRect` structure 
///    whose origin is the `X`, `Y`, `Z` and `W` values from the vector.
@property (readonly) CGRect CGRectValue NS_AVAILABLE(10_9, 5_0);

/// Returns the values in the vector as a `CGAffineTransformValue` structure.
/// - Returns:
///    Reading this property creates a `CGAffineTransformValue` structure 
///    from the first six values in the vector.
@property (readonly) CGAffineTransform CGAffineTransformValue NS_AVAILABLE(10_9, 5_0);

/// Returns a formatted string with all the values of a `CIVector`.
///
/// Some example string representations of vectors:
/// 
/// `CIVector`                               | `stringRepresentation`
/// ---------------------------------------- | --------------
/// `[CIVector vectorWithX:1.0 Y:0.5 Z:0.3]` | `"[1.0 0.5 0.3]"`
/// `[CIVector vectorWithX:10.0 Y:23.0]`     | `"[10.0 23.0]"`
/// 
/// To create a ``CIVector`` object from a string representation, use the ``vectorWithString:`` method.
/// 
@property (readonly) NSString *stringRepresentation;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIVECTOR_H */
