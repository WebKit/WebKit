/* CoreImage - CIFeature.h

   Copyright (c) 2011 Apple, Inc.
   All rights reserved. */

#ifndef CIFEATURE_H
#define CIFEATURE_H

#ifdef __OBJC__

#import <CoreImage/CoreImageDefines.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The abstract superclass for objects representing notable features detected in an image.
///
/// > Note: In macOS 10.13, iOS 11, and tvOS 11 or later, the Vision framework replaces these classes 
/// for identifying and analyzing image features.
/// See <doc://com.apple.documentation/documentation/vision/vnobservation>)
/// 
/// A `CIFeature` object represents a portion of an image that a detector believes matches its criteria. 
/// Subclasses of CIFeature holds additional information specific to the detector that discovered the feature.
///
NS_CLASS_AVAILABLE(10_7, 5_0)
@interface CIFeature : NSObject {}


/// The type of feature that was discovered.
/// 
/// The type can be one of:
/// * ``CIFeatureTypeFace``
/// * ``CIFeatureTypeRectangle``
/// * ``CIFeatureTypeQRCode``
/// * ``CIFeatureTypeText``
@property (readonly, retain) NSString *type;

/// The rectangle that bounds the location of discovered feature.
///
/// The rectangle is in the cartesian coordinate system of the image.
@property (readonly, assign) CGRect bounds;

@end

/** Specifies the type of a feature that is a face. */

/// A Core Image feature type for person’s face.
///
/// To detect faces in an image or video, pass this to ``/CIDetector/detectorOfType:context:options:``
/// 
/// Use the ``CIFaceFeature`` class to find more information about the detected face.
CORE_IMAGE_EXPORT NSString* const CIFeatureTypeFace;

/// A Core Image feature type for rectangular object.
///
/// To detect rectangles in an image or video, pass this to ``/CIDetector/detectorOfType:context:options:``
/// 
/// Use the ``CIRectangleFeature`` class to find more information about the detected rectangle.
CORE_IMAGE_EXPORT NSString* const CIFeatureTypeRectangle;

/// A Core Image feature type for QR code object.
///
/// To detect QR codes in an image or video, pass this to ``/CIDetector/detectorOfType:context:options:``
/// 
/// Use the ``CIQRCodeFeature`` class to find more information about the detected QR code.
CORE_IMAGE_EXPORT NSString* const CIFeatureTypeQRCode;

/// A Core Image feature type for text.
///
/// To detect text in an image or video, pass this to ``/CIDetector/detectorOfType:context:options:``
/// 
/// Use the ``CITextFeature`` class to find more information about the detected text.
CORE_IMAGE_EXPORT NSString* const CIFeatureTypeText;


/// Information about a face detected in a still or video image.
/// 
/// > Note: In macOS 10.13, iOS 11, and tvOS 11 or later, the Vision framework replaces this 
/// class for identifying and analyzing image features. See `VNDetectFaceRectanglesRequest`.
/// See <doc://com.apple.documentation/documentation/vision/vndetectfacerectanglesrequest>)
///
/// The properties of a `CIFaceFeature` object provide information about the face’s eyes and mouth. 
/// A face object in a video can also have properties that track its location over time, tracking ID and frame count.
///
NS_CLASS_AVAILABLE(10_7, 5_0)
@interface CIFaceFeature : CIFeature
{
    CGRect bounds;
    BOOL hasLeftEyePosition;
    CGPoint leftEyePosition;
    BOOL hasRightEyePosition;
    CGPoint rightEyePosition;
    BOOL hasMouthPosition;
    CGPoint mouthPosition;
    
    BOOL hasTrackingID;
    int trackingID;
    BOOL hasTrackingFrameCount;
    int trackingFrameCount;
    
    BOOL hasFaceAngle;
    float faceAngle;
    
    BOOL hasSmile;
    BOOL leftEyeClosed;
    BOOL rightEyeClosed;
}

/// A rectangle indicating the position and extent of the face feature in image coordinates.
@property (readonly, assign) CGRect bounds;

/// A Boolean value that indicates whether the detector found the face’s left eye.
@property (readonly, assign) BOOL hasLeftEyePosition;

/// The image coordinate of the center of the left eye.
/// 
/// > Note: The left eye is on the left side of the face from the observer's perspective. 
///   It is not the left eye from the subject's perspective.
///   
@property (readonly, assign) CGPoint leftEyePosition;

/// A Boolean value that indicates whether the detector found the face’s right eye.
@property (readonly, assign) BOOL hasRightEyePosition;

/// The image coordinate of the center of the right eye.
/// 
/// > Note: The right eye is on the right side of the face from the observer's perspective. 
///   It is not the right eye from the subject's perspective.
///   
@property (readonly, assign) CGPoint rightEyePosition;

/// A Boolean value that indicates whether the detector found the face’s mouth.
@property (readonly, assign) BOOL hasMouthPosition;

/// The image coordinate of the center of the mouth.
@property (readonly, assign) CGPoint mouthPosition;

/// A Boolean value that indicates whether the face object has a tracking ID.
@property (readonly, assign) BOOL hasTrackingID;

/// The tracking identifier of the face object.
///
/// Core Image provides a tracking identifier for faces it detects in a video stream, which you can 
/// use to identify when a CIFaceFeature objects detected in one video frame is the same face detected 
/// in a previous video frame.
/// 
/// This identifier persists only as long as a face is in the frame and is not associated with a specific 
/// face. In other words, if a face moves out of the video frame and comes back into the frame later, 
/// another ID is assigned. (Core Image detects faces, but does not recognize specific faces.)
///
@property (readonly, assign) int trackingID;

/// A Boolean value that indicates the face object has a tracking frame count.
@property (readonly, assign) BOOL hasTrackingFrameCount;

/// The tracking frame count of the face.
@property (readonly, assign) int trackingFrameCount;

/// A Boolean value that indicates whether information about face rotation is available.
@property (readonly, assign) BOOL hasFaceAngle;

/// The rotation of the face.
///
/// Rotation is measured counterclockwise in degrees, with zero indicating that a line drawn between 
/// the eyes is horizontal relative to the image orientation.
///
@property (readonly, assign) float faceAngle;

/// A Boolean value that indicates whether a smile is detected in the face.
///
/// To detect smiles, ``/CIDetector/featuresInImage:options:`` needs to be called with the ``CIDetectorSmile`` option set to true.
@property (readonly, assign) BOOL hasSmile;

/// A Boolean value that indicates whether a closed left eye is detected in the face.
///
/// To detect closed eyes, ``/CIDetector/featuresInImage:options:`` needs to be called with the ``CIDetectorEyeBlink`` option set to true.
@property (readonly, assign) BOOL leftEyeClosed;

/// A Boolean value that indicates whether a closed right eye is detected in the face.
///
/// To detect closed eyes, ``/CIDetector/featuresInImage:options:`` needs to be called with the ``CIDetectorEyeBlink`` option set to true.
@property (readonly, assign) BOOL rightEyeClosed;

@end

/// Information about a rectangular region detected in a still or video image.
///
/// > Note: In macOS 10.13, iOS 11, and tvOS 11 or later, the Vision framework replaces these classes 
/// for identifying and analyzing image features. 
/// See <doc://com.apple.documentation/documentation/vision/vndetectfacerectanglesrequest>)
///
/// A detected rectangle feature is not necessarily rectangular in the plane of the image; rather, the 
/// feature identifies a shape that may be rectangular in space (for example a book on a desk) but which 
/// appears as a four-sided polygon in the image. The properties of a `CIRectangleFeature` object 
/// identify its four corners in image coordinates.
/// 
/// You can use rectangle feature detection together with the `CIPerspectiveCorrection` filter 
/// to transform the feature to a normal orientation.
/// 
/// To detect rectangles in an image or video, choose ``CIDetectorTypeRectangle`` when initializing a 
/// ``CIDetector`` object, and use the `CIDetectorAspectRatio` and `CIDetectorFocalLength` options to 
/// specify the approximate shape of rectangular features to search for. The detector returns at 
/// most one rectangle feature, the most prominent found in the image.
///
NS_CLASS_AVAILABLE(10_10, 8_0)
@interface CIRectangleFeature : CIFeature
{
    CGRect bounds;
    CGPoint topLeft;
    CGPoint topRight;
    CGPoint bottomLeft;
    CGPoint bottomRight;
}

/// A rectangle that indicates the position and extent of the rectangle feature in image coordinates.
///
/// This property identifies the rectangular region of the image containing the detected rectangle, 
/// not necessarily the shape of the rectangle. A detected feature is rectangular in space, but may 
/// appear as a four-sided polygon in the image. Use the properties listed in `CIRectangleFeature` to find the 
/// corners of the rectangle as it appears in perspective.
@property (readonly) CGRect bounds;

/// The image coordinate of the upper-left corner of the detected rectangle.
@property (readonly) CGPoint topLeft;

/// The image coordinate of the upper-right corner of the detected rectangle.
@property (readonly) CGPoint topRight;

/// The image coordinate of the lower-left corner of the detected rectangle.
@property (readonly) CGPoint bottomLeft;

/// The image coordinate of the lower-right corner of the detected rectangle.
@property (readonly) CGPoint bottomRight;

@end

/** A QR code feature found by a CIDetector
 All positions are relative to the original image. */

@class CIQRCodeDescriptor;

/// Information about a Quick Response code detected in a still or video image.
///
/// > Note: In macOS 10.13, iOS 11, and tvOS 11 or later, the Vision framework replaces these classes 
/// for identifying and analyzing image features. 
/// See <doc://com.apple.documentation/documentation/vision/vndetectbarcodesrequest>)
/// 
/// A QR code is a two-dimensional barcode using the ISO/IEC 18004:2006 standard. The properties of 
/// a CIQRCodeFeature object identify the corners of the barcode in the image perspective and provide 
/// the decoded message.
/// 
/// To detect QR codes in an image or video, choose ``CIDetectorTypeQRCode`` type when initializing a
/// ``CIDetector`` object.
///
NS_CLASS_AVAILABLE(10_10, 8_0)
@interface CIQRCodeFeature : CIFeature <NSSecureCoding, NSCopying>
{
    CGRect bounds;
    CGPoint topLeft;
    CGPoint topRight;
    CGPoint bottomLeft;
    CGPoint bottomRight;
    CIQRCodeDescriptor *symbolDescriptor;
}

/// A rectangle that indicates the position and extent of the QR code feature in image coordinates.
///
/// This property identifies the rectangular region of the image containing the detected QR code, 
/// not necessarily the shape of the QR code. A detected feature is square in space, but may 
/// appear as a four-sided polygon in the image. Use the properties listed in `CIQRCodeFeature` to find the 
/// corners of the QR code as it appears in perspective.
@property (readonly) CGRect bounds;

/// The image coordinate of the upper-left corner of the detected QR code.
@property (readonly) CGPoint topLeft;

/// The image coordinate of the upper-right corner of the detected QR code.
@property (readonly) CGPoint topRight;

/// The image coordinate of the lower-left corner of the detected QR code.
@property (readonly) CGPoint bottomLeft;

/// The image coordinate of the lower-right corner of the detected QR code.
@property (readonly) CGPoint bottomRight;

/// The string decoded from the detected barcode.
@property (nullable, readonly) NSString* messageString;

/// An abstract representation of a QR Code symbol.
/// 
/// The property is a ``CIQRCodeDescriptor`` instance that contains the payload, symbol version, 
/// mask pattern, and error correction level, so the QR Code can be reproduced.
/// 
@property (nullable, readonly) CIQRCodeDescriptor *symbolDescriptor NS_AVAILABLE(10_13, 11_0);

@end

/// Information about a text that was detected in a still or video image.
///
/// > Note: In macOS 10.13, iOS 11, and tvOS 11 or later, the Vision framework replaces these classes 
/// for identifying and analyzing image features. 
/// See <doc://com.apple.documentation/documentation/vision/vnrecognizetextrequest>)
/// 
/// A detected text feature is not necessarily rectangular in the plane of the image; rather, the 
/// feature identifies a shape that may be rectangular in space (for example a text on a sign) but which 
/// appears as a four-sided polygon in the image. The properties of a `CITextFeature` object 
/// identify its four corners in image coordinates.
/// 
/// To detect text in an image or video, choose the ``CIDetectorTypeText`` type when initializing a 
/// ``CIDetector`` object, and use the `CIDetectorImageOrientation` option to specify the desired 
/// orientation for finding upright text.
/// 
NS_CLASS_AVAILABLE(10_11, 9_0)
@interface CITextFeature : CIFeature
{
}

/// A rectangle that indicates the position and extent of the text feature in image coordinates.
///
/// This property identifies the rectangular region of the image containing the detected text, 
/// not necessarily the shape of the text box. A detected feature is rectangular in space, but may 
/// appear as a four-sided polygon in the image. Use the properties listed in `CITextFeature` to find the 
/// corners of the rectangle as it appears in perspective.
@property (readonly) CGRect bounds;

/// The image coordinate of the upper-left corner of the detected text.
@property (readonly) CGPoint topLeft;

/// The image coordinate of the upper-right corner of the detected text.
@property (readonly) CGPoint topRight;

/// The image coordinate of the lower-left corner of the detected text.
@property (readonly) CGPoint bottomLeft;

/// The image coordinate of the lower-right corner of the detected text.
@property (readonly) CGPoint bottomRight;

/// An array containing additional features detected within the feature.
///
/// A text detector can identify both a major region that is likely to contain text as well
/// as the areas within that region that likely to contain individual text features. Such 
/// features might be single characters, groups of closely-packed characters, or entire words.
/// 
/// To detect sub-features, ``/CIDetector/featuresInImage:options:`` needs to be called with 
/// the ``CIDetectorReturnSubFeatures`` option set to true.
/// 
@property (nullable, readonly) NSArray *subFeatures;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIFEATURE_H */
