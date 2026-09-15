/*
   CoreImage - CIFilter.h

   Copyright (c) 2015 Apple, Inc.
   All rights reserved.
*/

#ifndef CIFILTER_H
#define CIFILTER_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImageDefines.h>
#import <CoreImage/CIFilterConstructor.h>

NS_ASSUME_NONNULL_BEGIN

/* Filter attributes keys */

/** Name of the filter */
CORE_IMAGE_EXPORT NSString * const kCIAttributeFilterName;

/** Name of the filter intended for UI display (eg. localized) */
CORE_IMAGE_EXPORT NSString * const kCIAttributeFilterDisplayName;

/** Description of the filter intended for UI display (eg. localized) */
CORE_IMAGE_EXPORT NSString * const kCIAttributeDescription NS_AVAILABLE(10_5, 9_0);

/** The version of OS X and iOS a filter was first available in. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeFilterAvailable_Mac NS_AVAILABLE(10_11, 9_0);
CORE_IMAGE_EXPORT NSString * const kCIAttributeFilterAvailable_iOS NS_AVAILABLE(10_11, 9_0);

/** URL for the reference documentation of the filter. See localizedReferenceDocumentationForFilterName. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeReferenceDocumentation NS_AVAILABLE(10_5, 9_0);

/** Array of filter category names (see below) */
CORE_IMAGE_EXPORT NSString * const kCIAttributeFilterCategories;

/** Class name of the filter. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeClass;

/** The type of the attribute e.g. scalar, time, distance, etc. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeType;

/** Minimum value for the attribute. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeMin;

/** Maximum value for the attribute. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeMax;

/** Minimum value for the slider. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeSliderMin;

/** Maximum value for the slider. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeSliderMax;

/** Default value for the slider. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeDefault;

/** The identity value is the value at which the filter has no effect. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeIdentity;

/** The non-localized name of the attribute. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeName;

/** The localized name of the attribute to be used for display to the user. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeDisplayName;


/** Key to request the desired set of controls in a filter UIView. The defined values for this key are:
    CIUISetBasic, CIUISetIntermediate, CIUISetAdvanced and CIUISetDevelopment. */
CORE_IMAGE_EXPORT NSString * const kCIUIParameterSet NS_AVAILABLE(10_5, 9_0);

/** Constant for requesting controls that are appropriate in a basic user scenario, meaning the bare minimum of settings to control the filter. */
CORE_IMAGE_EXPORT NSString * const kCIUISetBasic NS_AVAILABLE(10_5, 9_0);

/** Constant for requesting controls that are appropriate in an intermediate user scenario. */
CORE_IMAGE_EXPORT NSString * const kCIUISetIntermediate NS_AVAILABLE(10_5, 9_0);

/** Constant for requesting controls that are appropriate in an advanced user scenario. */
CORE_IMAGE_EXPORT NSString * const kCIUISetAdvanced NS_AVAILABLE(10_5, 9_0);

/** Constant for requesting controls that should only be visible for development purposes. */
CORE_IMAGE_EXPORT NSString * const kCIUISetDevelopment NS_AVAILABLE(10_5, 9_0);


/* Types for numbers */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeTime;
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeScalar;
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeDistance;
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeAngle;
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeBoolean;

/** Indicates that the key uses integer values. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeInteger NS_AVAILABLE(10_5, 5_0);

/** Indicates that the key uses non negative integer values. */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeCount NS_AVAILABLE(10_5, 5_0);

/* Types for 2-element vectors */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypePosition;
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeOffset;

/* Types for 3-element vectors */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypePosition3;

/* Types for 4-element vectors */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeRectangle;

/* Types for colors */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeOpaqueColor NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeColor NS_AVAILABLE(10_11, 5_0);

/* Types for images */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeGradient NS_AVAILABLE(10_4, 9_0);
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeImage NS_AVAILABLE(10_11, 5_0);

/* Types for NSValue of CGAffineTransform */
CORE_IMAGE_EXPORT NSString * const kCIAttributeTypeTransform NS_AVAILABLE(10_11, 5_0);


/* Categories */
CORE_IMAGE_EXPORT NSString * const kCICategoryDistortionEffect;
CORE_IMAGE_EXPORT NSString * const kCICategoryGeometryAdjustment;
CORE_IMAGE_EXPORT NSString * const kCICategoryCompositeOperation;
CORE_IMAGE_EXPORT NSString * const kCICategoryHalftoneEffect;
CORE_IMAGE_EXPORT NSString * const kCICategoryColorAdjustment;
CORE_IMAGE_EXPORT NSString * const kCICategoryColorEffect;
CORE_IMAGE_EXPORT NSString * const kCICategoryTransition;
CORE_IMAGE_EXPORT NSString * const kCICategoryTileEffect;
CORE_IMAGE_EXPORT NSString * const kCICategoryGenerator;
CORE_IMAGE_EXPORT NSString * const kCICategoryReduction NS_AVAILABLE(10_5, 5_0);
CORE_IMAGE_EXPORT NSString * const kCICategoryGradient;
CORE_IMAGE_EXPORT NSString * const kCICategoryStylize;
CORE_IMAGE_EXPORT NSString * const kCICategorySharpen;
CORE_IMAGE_EXPORT NSString * const kCICategoryBlur;
CORE_IMAGE_EXPORT NSString * const kCICategoryVideo;
CORE_IMAGE_EXPORT NSString * const kCICategoryStillImage;
CORE_IMAGE_EXPORT NSString * const kCICategoryInterlaced;
CORE_IMAGE_EXPORT NSString * const kCICategoryNonSquarePixels;
CORE_IMAGE_EXPORT NSString * const kCICategoryHighDynamicRange;
CORE_IMAGE_EXPORT NSString * const kCICategoryBuiltIn;
CORE_IMAGE_EXPORT NSString * const kCICategoryFilterGenerator NS_AVAILABLE(10_5, 9_0);


/* Options keys for [CIFilter apply:arguments:options:] and [CIFilter apply:...] methods. */

CORE_IMAGE_EXPORT NSString * const kCIApplyOptionExtent NS_AVAILABLE_MAC(10_4);
CORE_IMAGE_EXPORT NSString * const kCIApplyOptionDefinition NS_AVAILABLE_MAC(10_4);
CORE_IMAGE_EXPORT NSString * const kCIApplyOptionUserInfo NS_AVAILABLE_MAC(10_4);

/* If used, the value of the kCIApplyOptionColorSpace key be must be an RGB CGColorSpaceRef.
 * Using this option specifies that the output of the kernel is in this color space.
 * If not specified, the output of the kernel is in the working color space of the rendering CIContext. */
CORE_IMAGE_EXPORT NSString * const kCIApplyOptionColorSpace NS_AVAILABLE_MAC(10_4);


/* common filter parameter keys */

/// A key to get  the output image of a Core Image filter. 
/// 
/// The value for this key will be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIOutputImageKey NS_AVAILABLE(10_5, 5_0);

/// A key to get or set the background image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputBackgroundImageKey NS_AVAILABLE(10_5, 5_0);

/// A key to get or set the input image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
/// For filters that also use a background image, this key refers to the foreground image.
CORE_IMAGE_EXPORT NSString * const kCIInputImageKey NS_AVAILABLE(10_5, 5_0);

/// A key to get or set the depth map image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputDepthImageKey NS_AVAILABLE(10_13, 11_0);

/// A key to get or set the disparity map image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputDisparityImageKey NS_AVAILABLE(10_13, 11_0);

/// A key to get or set the scalar amount value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputAmountKey NS_AVAILABLE(10_14, 12_0);

/// A key to get or set the scalar count value of a Core Image filter. 
/// 
/// The value for this key needs to be an integer  `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputCountKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the scalar threshold value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputThresholdKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the scalar time value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputTimeKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the geometric 2x3 matrix transform value of a Core Image filter. 
/// The value for this key needs to be an `NSAffineTransformStruct` or an `NSValue` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputTransformKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the geometric scale value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputScaleKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the geometric aspect ratio value of a Core Image filter. 
/// The value for this key needs to be an `NSNumber` instance containing the `horizontal/vertical` scale ratio .
CORE_IMAGE_EXPORT NSString * const kCIInputAspectRatioKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the center value of a Core Image filter.  
/// The value for this key needs to be a ``CIVector`` instance containing the `x,y` coordinate.
CORE_IMAGE_EXPORT NSString * const kCIInputCenterKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the geometric radius value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputRadiusKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the geometric radius value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputRadius0Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the geometric radius value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputRadius1Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the geometric angle value of a Core Image filter.  Typically the angle is in radians.
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputAngleKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar optical refraction value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputRefractionKey NS_AVAILABLE(10_5, 9_0);

/// A key to get or set the geometric width value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputWidthKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar sharpness value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputSharpnessKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar intensity value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputIntensityKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar EV value of a Core Image filter that specifies how many F-stops brighter or darker to make the image. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputEVKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar saturation value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputSaturationKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the color value of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIColor`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputColorKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set a color value of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIColor`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputColor0Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set a color value of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIColor`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputColor1Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set a color space value of a Core Image filter. 
/// 
/// The value for this key needs to be a `CGColorSpace` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputColorSpaceKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the scalar brightness value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputBrightnessKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the scalar contrast value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputContrastKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the boolean behavior of a Core Image filter that specifies if the filter should extrapolate a table beyond the defined range. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputExtrapolateKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the boolean behavior of a Core Image filter that specifies if the filter should operate in linear or perceptual colors. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputPerceptualKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the scalar bias value of a Core Image filter. 
/// 
/// The value for this key needs to be an `NSNumber` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputBiasKey NS_AVAILABLE(10_5, 9_0);

/// A key to get or set the vector bias value of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIVector`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputBiasVectorKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the vector weights value of a convolution Core Image filter. 
/// 
/// The value for this key needs to be a ``CIVector`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputWeightsKey NS_AVAILABLE(10_11, 9_0);

/// A key to get or set the gradient map image of a Core Image filter that maps luminance to a color with alpha. 
/// 
/// The value for this key needs to be a 1 pixel tall ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputGradientImageKey NS_AVAILABLE(10_5, 9_0);

/// A key to get or set the mask image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputMaskImageKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the matte image of a Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputMatteImageKey NS_AVAILABLE(10_14, 12_0);

/// A key to get or set the environment map image of a Core Image filter that maps normal directions to a color with alpha. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputShadingImageKey NS_AVAILABLE(10_5, 9_0);

/// A key to get or set the target image for a transition Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputTargetImageKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the backside image for a transition Core Image filter. 
/// 
/// The value for this key needs to be a ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputBacksideImageKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the palette image for a  Core Image filter. 
/// 
/// The value for this key needs to be a  1 pixel tall ``CIImage`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputPaletteImageKey NS_AVAILABLE(26_0, 26_0);

/// A key to get or set the vector extent value of a Core Image filterthat defines the extent of the effect. 
/// The value for this key needs to be a ``CIVector`` instance.
CORE_IMAGE_EXPORT NSString * const kCIInputExtentKey NS_AVAILABLE(10_5, 7_0);

/// A key to get or set the coordinate value of a Core Image filter.  
/// The value for this key needs to be a ``CIVector`` instance containing the `x,y` coordinate.
CORE_IMAGE_EXPORT NSString * const kCIInputPoint0Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set a coordinate value of a Core Image filter.  
/// The value for this key needs to be a ``CIVector`` instance containing the `x,y` coordinate.
CORE_IMAGE_EXPORT NSString * const kCIInputPoint1Key NS_AVAILABLE(26_0, 26_0);

/// A key to get or set a coordinate value of a Core Image filter.  
/// The value for this key needs to be a ``CIVector`` instance containing the `x,y` coordinate.
CORE_IMAGE_EXPORT NSString * const kCIInputVersionKey NS_AVAILABLE(10_11, 6_0);

/// An enum string type that your code can use to select different System Tone Mapping modes.
/// 
/// These options are consistent with the analogous options available in Core Graphics,
/// Core Animation, AppKit, UIKit, and SwiftUI, 
/// In Core Image, this option can be set on the `CISystemToneMap` filter.
typedef NSString * CIDynamicRangeOption NS_TYPED_ENUM;

/// Use Standard dynamic range. 
/// 
/// Images with `contentHeadroom` metadata will be tone mapped to a maximum pixel value of 1.0.
CORE_IMAGE_EXPORT CIDynamicRangeOption const kCIDynamicRangeStandard NS_AVAILABLE(26_0, 26_0);

/// Use extended dynamic range, but brightness is modulated to optimize for
/// co-existence with other composited content. 
/// 
/// For best results, images should contain `contentAverageLightLevel` metadata.
CORE_IMAGE_EXPORT CIDynamicRangeOption const kCIDynamicRangeConstrainedHigh NS_AVAILABLE(26_0, 26_0);

/// Use High dynamic range. 
/// 
/// The provides the best HDR quality and needs to be reserved
/// for situations where the user is focused on the media, such as larger views in
/// an image editing/viewing app, or annotating/drawing with HDR colors
CORE_IMAGE_EXPORT CIDynamicRangeOption const kCIDynamicRangeHigh NS_AVAILABLE(26_0, 26_0);


@class CIKernel, CIImage;
@protocol CIFilterConstructor;

/** CIFilter are filter objects for Core Image that encapsulate the filter with its attributes

 The CIFilter class produces a CIImage object as output. Typically, a filter takes one or more images as input. Some filters, however, generate an image based on other types of input parameters. The parameters of a CIFilter object are set and retrieved through the use of key-value pairs. You use the CIFilter object in conjunction with the CIImage, CIContext, CIVector, CIImageAccumulator, and CIColor objects to take advantage of the built-in Core Image filters when processing images. CIFilter objects are also used along with CIKernel, CISampler, and CIFilterShape objects to create custom filters. */

NS_CLASS_AVAILABLE(10_4, 5_0)
@interface CIFilter : NSObject <NSSecureCoding, NSCopying>
{
    void *_priv[8];
}

@property (readonly, nonatomic, nullable) CIImage *outputImage NS_AVAILABLE(10_10, 5_0);

/* The name of the filter. On OSX and iOS 10, this property is read-write.
 * This can be useful when using CIFilters with CoreAnimation or SceneKit.
 * For example, to set an attribute of a filter attached to a layer,
 * a unique path such as "filters.myExposureFilter.inputEV" could be used.
 * CALayer animations may also access filter attributes via key-paths. */
@property (nonatomic, copy) NSString *name;
- (NSString *)name NS_AVAILABLE(10_5,5_0);
- (void)setName:(NSString *)aString NS_AVAILABLE(10_5,10_0);

/* The 'enabled' property is used only by CoreAnimation and is animatable.
 * In Core Animation, a CIFilter only applied to its input when this
 * property is set to true. */
@property (getter=isEnabled) BOOL enabled NS_AVAILABLE_MAC(10_5);


/** Returns an array containing the names of all inputs in the filter. */
@property (nonatomic, readonly) NSArray<NSString *> *inputKeys;

/** Returns an array containing the names of all outputs in the filter. */
@property (nonatomic, readonly) NSArray<NSString *> *outputKeys;

/** Sets all inputs to their default values (where default values are defined, other inputs are left as-is). */
- (void)setDefaults;

/** Returns a dictionary containing key/value pairs describing the filter. (see description of keys below) */
@property (nonatomic, readonly) NSDictionary<NSString *,id> *attributes;


/** Used by CIFilter subclasses to apply the array of argument values 'args' to the kernel function 'k'. The supplied arguments must be type-compatible with the function signature of the kernel.

 The key-value pairs defined by 'dict' (if non-nil) are used to control exactly how the kernel is evaluated. Valid keys include:
 kCIApplyOptionExtent: the size of the produced image. Value is a four element NSArray [X Y WIDTH HEIGHT].
 kCIApplyOptionDefinition: the Domain of Definition of the produced image. Value is either a CIFilterShape object, or a four element NSArray defining a rectangle.
 @param  k         CIKernel of the filter
 @param  args      Array of arguments that are applied to the kernel
 @param  dict      Array of additional options
*/
- (nullable CIImage *)apply:(CIKernel *)k
				  arguments:(nullable NSArray *)args
			        options:(nullable NSDictionary<NSString *,id> *)dict NS_AVAILABLE_MAC(10_4);

/** Similar to above except that all argument values and option key-value are specified inline. The list of key-value pairs must be terminated by the 'nil' object. */
- (nullable CIImage *)apply:(CIKernel *)k, ... NS_REQUIRES_NIL_TERMINATION NS_AVAILABLE_MAC(10_4) NS_SWIFT_UNAVAILABLE("");

@end


@protocol CIFilter

    @property (readonly, nonatomic, nullable) CIImage *outputImage;

    @optional

    // CIFilter subclasses can implement 'customAttributes' to return a dictionary
    // containing key/value pairs describing the filter. (see description of keys below)
    + (nullable NSDictionary<NSString *,id>*) customAttributes;

@end


/** Methods to register a filter and get access to the list of registered filters
 Use these methods to create filters and find filters. */
@interface CIFilter (CIFilterRegistry)

/** Creates a new filter of type 'name'.
 On OSX, all input values will be undefined.
 On iOS, all input values will be set to default values. */
+ (nullable CIFilter *) filterWithName:(NSString *) name;

/** Creates a new filter of type 'name'.
 The filter's input parameters are set from the list of key-value pairs which must be nil-terminated.
 On OSX, any of the filter input parameters not specified in the list will be undefined.
 On iOS, any of the filter input parameters not specified in the list will be set to default values. */
+ (nullable CIFilter *)filterWithName:(NSString *)name
                        keysAndValues:key0, ... NS_REQUIRES_NIL_TERMINATION NS_SWIFT_UNAVAILABLE("");

/** Creates a new filter of type 'name'.
 The filter's input parameters are set from the dictionary of key-value pairs.
 On OSX, any of the filter input parameters not specified in the dictionary will be undefined.
 On iOS, any of the filter input parameters not specified in the dictionary will be set to default values. */
+ (nullable CIFilter *)filterWithName:(NSString *)name
                  withInputParameters:(nullable NSDictionary<NSString *,id> *)params NS_AVAILABLE(10_10, 8_0);

/** Returns an array containing all published filter names in a category. */
+ (NSArray<NSString *> *)filterNamesInCategory:(nullable NSString *)category;

/** Returns an array containing all published filter names that belong to all listed categories. */
+ (NSArray<NSString *> *)filterNamesInCategories:(nullable NSArray<NSString *> *)categories;


/** Publishes a new filter called 'name'.

 The constructor object 'anObject' should implement the filterWithName: method.
 That method will be invoked with the name of the filter to create.
 The class attributes must have a kCIAttributeFilterCategories key associated with a set of categories.
 @param   attributes    Dictionary of the registration attributes of the filter. See below for attribute keys.
*/
+ (void)registerFilterName:(NSString *)name
               constructor:(id<CIFilterConstructor>)anObject
           classAttributes:(NSDictionary<NSString *,id> *)attributes NS_AVAILABLE(10_4, 9_0);

/** Returns the localized name of a filter for display in the UI. */
+ (nullable NSString *)localizedNameForFilterName:(NSString *)filterName NS_AVAILABLE(10_4, 9_0);

/** Returns the localized name of a category for display in the UI. */
+ (NSString *)localizedNameForCategory:(NSString *)category NS_AVAILABLE(10_4, 9_0);

/** Returns the localized description of a filter for display in the UI. */
+ (nullable NSString *)localizedDescriptionForFilterName:(NSString *)filterName NS_AVAILABLE(10_4, 9_0);

/** Returns the URL to the localized reference documentation describing the filter.

 The URL can be a local file or a remote document on a webserver. It is possible, that this method returns nil (like filters that predate this feature). A client of this API has to handle this case gracefully. */
+ (nullable NSURL *)localizedReferenceDocumentationForFilterName:(NSString *)filterName NS_AVAILABLE(10_4, 9_0);

@end


/** Methods to serialize arrays of filters to xmp. */
@interface CIFilter (CIFilterXMPSerialization)

/* Given an array of filters, serialize the filters' parameters
   into XMP form that is suitable for embedding in to an image.
   At this time the only filters classes that are serialized
   are, CIAffineTransform, CICrop, and the filters returned by
   [CIImage autoAdjustmentFilters].
   The parameters of other filter classes will not be serialized.
   The return value will be null if none of the filters can be serialized.
 */
+ (nullable NSData*)serializedXMPFromFilters:(NSArray<CIFilter *> *)filters
                            inputImageExtent:(CGRect)extent
    NS_DEPRECATED(10_9,14_0, 6_0,17_0);

/* Return an array of CIFilters de-serialized from XMP data.
 */
+ (NSArray<CIFilter *> *)filterArrayFromSerializedXMP:(NSData *)xmpData
                                     inputImageExtent:(CGRect)extent
                                                error:(NSError **)outError
    NS_DEPRECATED(10_9,14_0, 6_0,17_0);

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIFILTER_H */
