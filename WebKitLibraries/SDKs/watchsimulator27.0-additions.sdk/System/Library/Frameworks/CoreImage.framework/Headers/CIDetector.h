/* CoreImage - CIDetector.h

   Copyright (c) 2011 Apple, Inc.
   All rights reserved. */

#ifndef CIDETECTOR_H
#define CIDETECTOR_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CIImage;
@class CIContext;
@class CIFeature;

/** Detects features in images.

 This class potentially holds onto a lot of state. Hence it may be beneficial from a performance perspective to re-use the same CIDetector instance. Specifying a CIContext when creating a detector may have an impact on performance since this context may be used when analyzing an image.
 */
NS_CLASS_AVAILABLE(10_7, 5_0)
@interface CIDetector : NSObject {}

/** Returns a new detector instance of the given type.

 The type is used to specify the detection intent.
 This will return value if the detector type is not supported.

 The context argument specifies the CIContext to be used to operate on the image. May be nil.

 If the input image to -featuresInImage: is the output of a CoreImage operation, it may improve performance to specify the same context that was used to operate on that image.

 The detector may do image processing in this context and if the image is on the GPU and the specified context is a GPU context this may avoid additional upload to / download from the GPU. If the input image is on the CPU (or the output from a CPU based context) specifying a GPU based context (or vice versa) may reduce performance.

//  The options parameter lets you optionally specify a accuracy / performance tradeoff. Can be nil or an empty dictionary. */
+ (nullable CIDetector *)detectorOfType:(NSString*)type
                                context:(nullable CIContext *)context
                                options:(nullable NSDictionary<NSString *,id> *)options
    NS_AVAILABLE(10_7, 5_0);

/** Returns an array of CIFeature instances in the given image.
 The array is sorted by confidence, highest confidence first. */
- (NSArray<CIFeature *> *)featuresInImage:(CIImage *)image
    NS_AVAILABLE(10_7, 5_0);

/** Returns an array of CIFeature instances in the given image.
 The array is sorted by confidence, highest confidence first.
 The options dictionary can contain a CIDetectorImageOrientation key value. */
- (NSArray<CIFeature *> *)featuresInImage:(CIImage *)image
                                  options:(nullable NSDictionary<NSString *,id> *)options
    NS_AVAILABLE(10_8, 5_0);

@end


// Types to be used for +[CIDetector detectorOfType:context:options:]

/* Specifies a detector type for face recognition. */
CORE_IMAGE_EXPORT NSString* const CIDetectorTypeFace NS_AVAILABLE(10_7, 5_0);

/* Specifies a detector type for rectangle detection. */
CORE_IMAGE_EXPORT NSString* const CIDetectorTypeRectangle NS_AVAILABLE(10_10, 8_0);

/* Specifies a detector type for barcode detection. */
CORE_IMAGE_EXPORT NSString* const CIDetectorTypeQRCode NS_AVAILABLE(10_10, 8_0);

/* Specifies a detector type for text detection. */
CORE_IMAGE_EXPORT NSString* const CIDetectorTypeText NS_AVAILABLE(10_11, 9_0);

// Options that can be used with +[CIDetector detectorOfType:context:options:]

/* The key in the options dictionary used to specify a accuracy / performance tradeoff to be used. */
CORE_IMAGE_EXPORT NSString* const CIDetectorAccuracy NS_AVAILABLE(10_7, 5_0);

/* These are values to be used with the CIDetectorAccuracy key when created a CIDetector.
 There is a performance / accuracy tradeoff to be made. The default value will work well for most
 situations, but using these the detector will favor performance over accuracy or
 accuracy over performance. */
CORE_IMAGE_EXPORT NSString* const CIDetectorAccuracyLow NS_AVAILABLE(10_7, 5_0); /* Lower accuracy, higher performance */
CORE_IMAGE_EXPORT NSString* const CIDetectorAccuracyHigh NS_AVAILABLE(10_7, 5_0); /* Lower performance, higher accuracy */

/*The key in the options dictionary used to specify that feature tracking should be used. */
CORE_IMAGE_EXPORT NSString* const CIDetectorTracking NS_AVAILABLE(10_8, 6_0);

/* The key in the options dictionary used to specify the minimum size that the
 detector will recognize as a feature. */

/* For face detector, the value for this key is an float NSNumber
 from 0.0 ... 1.0 that represents a percentage of shorter edge of an input image.
 valid values range: 0.01 <= CIDetectorMinFeatureSize <= 0.5.
 Setting a higher value for this parameter is used for performance gain only. The default value is 0.15. */

/* For rectangle detector, the value for this key is an float NSNumber
 from 0.0 ... 1.0 that represents a percentage of shorter edge of an input image.
 valid values range: 0.2 <= CIDetectorMinFeatureSize <= 1.0 The default value is 0.2. */

/* For text detector, the value for this key is an float NSNumber
 from 0.0 ... 1.0 that represents a percentage of height of an input image.
 valid values range: 0.0 <= CIDetectorMinFeatureSize <= 1.0. The default value is 10/(height of input image). */
CORE_IMAGE_EXPORT NSString* const CIDetectorMinFeatureSize NS_AVAILABLE(10_8, 6_0);

/* For rectangle detector, the value for this key is an integer NSNumber
 from 1 ... 256 that represents the maximum number of features to return. 
 valid value range: 1 <= CIDetectorMaxFeatureCount <= 256. The default value is 1.
 */
CORE_IMAGE_EXPORT NSString* const CIDetectorMaxFeatureCount NS_AVAILABLE(10_12, 10_0);

/* The key in the options dictionary used to specify number of angles, the value for this key is one of 1, 3, 5, 7, 9, 11.*/
CORE_IMAGE_EXPORT NSString* const CIDetectorNumberOfAngles NS_AVAILABLE(10_11, 9_0);


// Options that can be used with -[CIDetector featuresInImage:options:]

/// A dictionary key that configures a Core Image feature detection operation 
/// to account for the orientation the image.
/// 
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// The value of this key is an number object whose value is an integer between 1 and 8. 
/// The TIFF and EXIF specifications define the orientation values that describe how the image should be displayed. 
/// The default value is 1. For further details, see `CGImagePropertyOrientation`.
/// 
/// The ``CIDetectorTypeFace`` and ``CIDetectorTypeText`` can use this option to correctly find faces or text.
/// 
/// Regardless of the orientation values the ``/CIFeature/bounds-property`` which is always measured in 
/// the cartesean coordinates system of the image that you pass to the detector.
/// 
CORE_IMAGE_EXPORT NSString *const CIDetectorImageOrientation NS_AVAILABLE(10_8, 5_0);

/// A dictionary key that configures a Core Image face feature detection operation 
/// to perform additional processing to recognize closed eyes in detected faces.
///
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// If the value of the key is true, then facial expressions such as blinking and smiles are extracted.
/// This is needed for the ``/CIFaceFeature/leftEyeClosed-property`` and ``/CIFaceFeature/rightEyeClosed-property`` to function.
/// 
CORE_IMAGE_EXPORT NSString *const CIDetectorEyeBlink NS_AVAILABLE(10_9, 7_0);

/// A dictionary key that configures a Core Image face feature detection operation 
/// to perform additional processing to recognize smiles in detected faces.
///
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// If the value of the key is true, then facial expressions such as blinking and smiles eyes are extracted.
/// This is needed for the ``/CIFaceFeature/hasSmile-property`` to function.
/// 
CORE_IMAGE_EXPORT NSString *const CIDetectorSmile NS_AVAILABLE(10_9, 7_0);

/// A dictionary key that configures a Core Image rectangle feature detection operation 
/// to account for the focal length of the camera used for the image.
/// 
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// The value of this key is an NSNumber object whose value is a floating-point number. Use this option with the CIDetectorTypeRectangle 
/// detector type to control the effect of the CIDetectorAspectRatio option on feature detection.
/// 
/// This option’s value can be 0.0, -1.0, or any positive value:
/// * The special value of -1.0 (the default) disables the aspect ratio test for the returned rectangle.
/// * The special value of 0.0 enables a less precise test of aspect ratio that approximates an orthographic (non-perspective) projection. 
///   Use this value if you want to specify the aspect ratio of the rectangle via the CIDetectorAspectRatio option, but have no means of 
///   determining the value for the focal length in pixels. See below for a method to compute an approximate value for the focal length in pixels.
/// * Any other value specifies the camera focal length, in pixels, allowing the aspect ratio specification to account for perspective distortion 
///   of rectangles in the input image. 
///   
/// If you know the diagonal field of view of the camera (the scene angle subtended by the diagonal corners of an image), you can use the 
/// following formula to compute an approximate focal length in pixels:
/// 
/// `focal_length_pixels = (image_diagonal_pixels/2)/tan(FOV/2)`
/// 
/// In this formula, `image_diagonal_pixels` is the length (in pixels) of the image diagonal of the maximum resolution of the camera sensor. 
/// For example, this value is:
/// * `4080` pixels for a `3264 x 2448` (8 megapixel) sensor
/// * `5000` pixels for a `4096 x 3024` (12 megapixel) sensor.
///
/// To measure diagonal field of view, put the camera on a tripod so that it is perpendicular to a surface and the center of the image is 
/// oriented on a mark on the surface. Measure the distance from the mark to one of the corner points of the image (Y). Measure the distance 
/// from the camera to the surface (Z). The field of view is then `2*arctan(Y/Z)`.
/// 
/// You must specify this value in terms of the maximum sensor resolution. If the supplied CIImage has been scaled relative relative to the 
/// maximum sensor resolution, the supplied focal length must also be similarly scaled.
/// 
CORE_IMAGE_EXPORT NSString* const CIDetectorFocalLength NS_AVAILABLE(10_10, 8_0);

/// A dictionary key that configures a Core Image rectangle feature detection operation 
/// to search for a rectangle of a desired aspect ratio (width divided by height).
/// 
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// The value for this key needs to be is a positive float number. 
/// Use this option with a ``CIDetectorTypeRectangle`` detector to fine-tune the accuracy of the detector. 
/// 
/// For example, to more accurately find a business card (3.5 x 2 inches) in an image, specify an aspect ratio of 1.75.
/// 
/// If this key is not specified, the a default value of 1.6 is used.
///
CORE_IMAGE_EXPORT NSString* const CIDetectorAspectRatio NS_AVAILABLE(10_10, 8_0);

/// A dictionary key that configures a Core Image text feature detection operation 
/// to return feature information for components of detected features.
///
/// This option is used with ``/CIDetector/featuresInImage:options:``
/// 
/// If the value for this option configures the ``CIDetectorTypeText`` detector as follows:
/// * False: detect only in regions likely to contain text.
/// * True: detect in regions likely to contain individual characters.
/// 
/// If this key is not specified, the a default is False.
///
CORE_IMAGE_EXPORT NSString* const CIDetectorReturnSubFeatures __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIDETECTOR_H */
