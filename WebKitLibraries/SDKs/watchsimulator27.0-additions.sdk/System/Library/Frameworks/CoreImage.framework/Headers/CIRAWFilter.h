/* CoreImage - CIRAWFilter.h
 
 Copyright (c) 2006 Apple, Inc.
 All rights reserved. */

#ifndef CIRAWFILTER_H
#define CIRAWFILTER_H

#ifdef __OBJC__

#import <CoreImage/CIFilter.h>
#import <CoreImage/CIRAWFilter_Deprecated.h>
#import <CoreImage/CoreImageDefines.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/CGImageProperties.h>

@class NSURL;
@class NSDictionary;
@class NSData;
@class NSProgress;

NS_ASSUME_NONNULL_BEGIN

NS_CLASS_AVAILABLE(12_0, 15_0)
@interface CIRAWFilter : CIFilter

typedef NSString* CIRAWDecoderVersion NS_TYPED_ENUM;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersionNone;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion9;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion9DNG;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion8;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion8DNG;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion7;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion7DNG;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion6;
CORE_IMAGE_EXPORT CIRAWDecoderVersion const CIRAWDecoderVersion6DNG;

/* Returns a NSArray containing the names of all supported camera models. */
@property(class, readonly) NSArray<NSString*>* supportedCameraModels;

/* Returns a NSArray containing the names of all supported camera models that support the specified decoder version. */
+ (NSArray<NSString*>*) supportedCameraModelsWithVersion:(CIRAWDecoderVersion)version
    NS_SWIFT_NAME(supportedCameraModels(version:))
    API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0));

/* Downloads any on-demand resources required to decode this filter's image using its current decoderVersion, if they aren't already present.
   Returns an NSProgress that can be used to track or cancel the download.
   If 'timeout' elapses before the download finishes, completionHandler is called with a timeout error.
   completionHandler is called with a nil error on success (whether or not a download was actually needed), or a non-nil error if the download failed or timed out.
   The completion handler is called on a queue of CoreImage's choosing; dispatch to your own queue if you need one (e.g. the main queue for UI work). */
- (NSProgress *) downloadResourcesWithTimeout:(NSTimeInterval)timeout
                             completionHandler:(void (^)(NSError * _Nullable error))completionHandler
    NS_SWIFT_NAME(downloadResources(timeout:completionHandler:))
    API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0));

/* Array of all supported decoder versions for the given image type, sorted in increasingly newer order. All entries would represent a valid version identifier set to 'decoderVersion'.*/
@property(readonly) NSArray<CIRAWDecoderVersion>* supportedDecoderVersions;

/* The full native size of the unscaled image. This is not affected by changing orientation */
@property(readonly) CGSize nativeSize;

/* Metadata properties CIRAWFilter instance
   This is a dictionary with the same contents as CGImageSourceCopyProperties. */
@property(readonly) NSDictionary* properties;

/* Changing this value allows the user to change the orientation of the image.
   The valid values are in range 1...8 and follow the EXIF specification. */
@property(readwrite) CGImagePropertyOrientation orientation;

/* Setting Draft Mode to true can improve image decoding speed with minimal loss of quality. */
@property(readwrite, getter= isDraftModeEnabled) BOOL draftModeEnabled;

/* Version string representing the decoder version to be used. A newly initialized object defaults to the newest available decoder version for the given image type. User can request an alternative, older version in order to maintain compatibility with older releases. Must be one of supportedDecoderVersions (above), otherwise a nil output image will be generated. */
@property(readwrite, retain) CIRAWDecoderVersion decoderVersion;

/* The desired scale factor at which the image will be eventually drawn. Setting this value can greatly improve the drawing performance.
   A value of 1.0 results in a full size output image, values smaller than 1 will result in a smaller output image.
   The value should be the range of 0...1. Default value is 1 */
@property(readwrite) float scaleFactor;

/* A value controlling the amount of exposure to apply to the image. Default value is 0. */
@property(readwrite) float exposure;

/* A value for baseline exposure applied to the image. Default value can vary based on the image. */
@property(readwrite) float baselineExposure;

/* A value controlling an amount to subtract from shadows the image. Default value can vary based on the image. */
@property(readwrite) float shadowBias;

/* A value to control the amount of global tone curve to apply to the image.
   A value of 0 indicates no global tone curve, i.e. linear response.
   A value of 1 indicates full global tone curve.
   The value should be the range of 0...1.  The default value is 1. */
@property(readwrite) float boostAmount;

/* The amount to boost the shadow areas of the image. Can be used to lighten details in shadows. This has no effect if the boostAmount is 0.
   A value less than 1 will darken the shadows.  A value greater than 1 will lighten the shadows.
   The value should be the range of 0...2.  The default value is 1. */
@property(readwrite) float boostShadowAmount;

/* A boolean value to control if highlight recovery is enabled or not.
   The default value is true. */
@property(readonly, getter= isHighlightRecoverySupported) BOOL highlightRecoverySupported NS_AVAILABLE(26_0, 26_0);
@property(readwrite, getter= isHighlightRecoveryEnabled) BOOL highlightRecoveryEnabled NS_AVAILABLE(26_0, 26_0);

/* A boolean value to control if gamut mapping is enabled or not.
   The default value is true. */
@property(readwrite, getter= isGamutMappingEnabled) BOOL gamutMappingEnabled;

/* A boolean value to control if gamut mapping is enabled or not.
   The default value can vary based on the image.
   The 'isLensCorrectionSupported' property is false if the current image doesn't support this setting. */
@property(readonly, getter= isLensCorrectionSupported) BOOL lensCorrectionSupported;
@property(readwrite, getter= isLensCorrectionEnabled) BOOL lensCorrectionEnabled;

/* A value to control the amount of luminance noise reduction to apply to the image.
   A value of 0 indicates no luminance noise reduction.
   A value of 1 indicates maximum luminance noise reduction.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isLuminanceNoiseReductionSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isLuminanceNoiseReductionSupported) BOOL luminanceNoiseReductionSupported;
@property(readwrite) float luminanceNoiseReductionAmount;

/* A value to control the amount of chroma noise reduction to apply to the image.
   A value of 0 indicates no chroma noise reduction.
   A value of 1 indicates maximum chroma noise reduction.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isColorNoiseReductionSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isColorNoiseReductionSupported) BOOL colorNoiseReductionSupported;
@property(readwrite) float colorNoiseReductionAmount;

/* A value to control the amount of sharpness to apply to the edges of the image.
   A value of 0 indicates no sharpness applied.
   A value of 1 indicates maximum sharpness applied.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isSharpnessSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isSharpnessSupported) BOOL sharpnessSupported;
@property(readwrite) float sharpnessAmount;

/* A value to control the amount of local contrast to apply to the edges of the image.
   A value of 0 indicates no contrast applied.
   A value of 1 indicates maximum contrast applied.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isContrastSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isContrastSupported) BOOL contrastSupported;
@property(readwrite) float contrastAmount;

/* A value to control the amount of detail enhancement to apply to the edges of the image.
   A value of 0 indicates no detail enhancement.
   A value of 3 indicates maximum detail enhancement.
   The value should be the range of 0...3.  The default value will vary per image.
   The 'isDetailSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isDetailSupported) BOOL detailSupported;
@property(readwrite) float detailAmount;

/* A value to control the amount of moire artifact reduction to apply to high frequency areas of the image.
   A value of 0 indicates no moire reduction.
   A value of 3 indicates maximum moire reduction.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isMoireReductionSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isMoireReductionSupported) BOOL moireReductionSupported;
@property(readwrite) float moireReductionAmount;

/* A value to control the amount of despeckle correction to apply to the raw image.
   A value of 0 indicates no despeckle correction.
   A value of 1 indicates maximum despeckle correction.
   The value should be in the range of 0...1.  The default value will vary per image.
   The 'isDespeckleSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isDespeckleSupported) BOOL despeckleSupported;
@property(readwrite) float despeckleAmount;

/* A value to control the amount of local tone curve to apply to the image.
   A value of 0 indicates no local tone curve, i.e. linear response.
   A value of 1 indicates full global tone curve.
   The value should be the range of 0...1.  The default value will vary per image.
   The 'isLocalToneMapSupported' property is false if the current image doesn't support this adjustment. */
@property(readonly, getter= isLocalToneMapSupported) BOOL localToneMapSupported;
@property(readwrite) float localToneMapAmount;

/* Allows the output to have an Extended Dynamic Range with values greater than 1 possible.
   A value of 0 indicates no extended dynamic range output.
   A value of 1 indicates default extended dynamic range output.
   A value of 2 indicates maximum extended dynamic range output.
   The value should be the range of 0...2.  The default value is 0. */
@property(readwrite) float extendedDynamicRangeAmount;

/* These properties provide differnet was to query or modify the image white balance
   Use 'neutralChromaticity' to query or change white balance based on x,y chromaticity values in the range (0..1)
   Use 'neutralTemperature' and 'neutralTemperature' to query or change white balance based temp/tint values in the ranges (2000K..50000K, -150..150)
   Use 'neutralLocation' to change the white balance based on an x,y pixel coordiniates in the image.
 */
@property(readwrite) CGPoint neutralChromaticity;
@property(readwrite) CGPoint neutralLocation;
@property(readwrite) float neutralTemperature;
@property(readwrite) float neutralTint;

/* An optional CIFilter to be applied to the RAW image while it is in linear space. */
@property(readwrite, retain, nullable) CIFilter* linearSpaceFilter;

/* Properties for auxiliary images that may be present in the file */
@property(readonly, nullable) CIImage* previewImage;
@property(readonly, nullable) CIImage* portraitEffectsMatte;
@property(readonly, nullable) CIImage* semanticSegmentationSkinMatte;
@property(readonly, nullable) CIImage* semanticSegmentationHairMatte;
@property(readonly, nullable) CIImage* semanticSegmentationGlassesMatte;
@property(readonly, nullable) CIImage* semanticSegmentationSkyMatte;
@property(readonly, nullable) CIImage* semanticSegmentationTeethMatte;

+ (nullable instancetype)filterWithImageURL:(NSURL *)url;

+ (nullable instancetype)filterWithImageData:(NSData *)data
							  identifierHint:(nullable NSString *)identifierHint;

+ (nullable instancetype)filterWithCVPixelBuffer:(CVPixelBufferRef)buffer
									  properties:(NSDictionary *)properties;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIRAWFILTER_H */
