/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import <AVFoundation/AVAssetTrack.h>
#import <CoreMedia/CMFormatDescription.h>
#import <WebCore/FormatDescriptionUtilities.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SourceBufferParser.h>
#import <WebKit/WKRetainPtr.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#if HAVE(IMMERSIVE_VIDEO_METADATA_SUPPORT)

namespace TestWebKitAPI {

static bool compareCameraCalibrationDictionary(CFDictionaryRef original, CFDictionaryRef copy)
{
    static std::array<CFStringRef, 13> compulsoryKeys = {
        PAL::kCMFormatDescriptionCameraCalibration_LensAlgorithmKind,
        PAL::kCMFormatDescriptionCameraCalibration_LensDomain,
        PAL::kCMFormatDescriptionCameraCalibration_LensIdentifier,
        PAL::kCMFormatDescriptionCameraCalibration_LensRole,
        PAL::kCMFormatDescriptionCameraCalibration_LensDistortions,
        PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialX,
        PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialY,
        PAL::kCMFormatDescriptionCameraCalibration_RadialAngleLimit,
        PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrix,
        PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixProjectionOffset,
        PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixReferenceDimensions,
        PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOriginSource,
        PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOrientationQuaternion
    };
    for (auto key : compulsoryKeys) {
        RetainPtr originalValue = CFDictionaryGetValue(original, key);
        RetainPtr copyValue = CFDictionaryGetValue(copy, key);
        bool result = CFEqual(originalValue.get(), copyValue.get());
        EXPECT_TRUE(result);
        if (!result)
            return false;
    }
    return true;
}

static bool compareCameraCalibrationArray(CFArrayRef original, CFArrayRef copy)
{
    int size = CFArrayGetCount(original);
    if (size != CFArrayGetCount(copy))
        return false;
    for (int index = 0; index < size; index++) {
        RetainPtr originalEntry = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(original, index));
        RetainPtr copyEntry = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(copy, index));
        EXPECT_TRUE(!!originalEntry);
        EXPECT_TRUE(!!copyEntry);
        if (!originalEntry || !copyEntry)
            return false;
        if (!compareCameraCalibrationDictionary(originalEntry.get(), copyEntry.get()))
            return false;
    }
    return true;
}

TEST(ImmersiveVideoMetadata, Spatial)
{
    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:@"spatial" withExtension:@"mp4"];
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:nil]);
    RetainPtr<NSArray> tracks = [asset tracks];
    EXPECT_GT([tracks count], 0UL);

    RetainPtr<NSArray<AVAssetTrack *>> videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    EXPECT_GT([videoTracks count], 0UL);
    RetainPtr<AVAssetTrack> track = [videoTracks firstObject];
    RetainPtr<NSArray> descriptions = [track formatDescriptions];
    EXPECT_GT([descriptions count], 0UL);

    RetainPtr description = (CMFormatDescriptionRef)[descriptions firstObject];
    EXPECT_TRUE(!!description);

    auto immersiveVideoMetadata = WebCore::immersiveVideoMetadataFromFormatDescription(description.get());
    EXPECT_TRUE(immersiveVideoMetadata.has_value());
    EXPECT_TRUE(immersiveVideoMetadata->isSpatial());
    EXPECT_EQ(immersiveVideoMetadata->horizontalFieldOfView, 63400);
    EXPECT_EQ(immersiveVideoMetadata->stereoCameraBaseline, 19274u);
    EXPECT_EQ(immersiveVideoMetadata->horizontalDisparityAdjustment, 200);

    RetainPtr dictionary = WebCore::formatDescriptionDictionaryFromImmersiveVideoMetadata(*immersiveVideoMetadata);
    EXPECT_TRUE(!!dictionary);
    EXPECT_EQ(CFDictionaryGetCount(dictionary.get()), 7L);
}

TEST(ImmersiveVideoMetadata, ImmersiveMono)
{
    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:@"immersivemono" withExtension:@"mp4"];
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:nil]);
    RetainPtr<NSArray> tracks = [asset tracks];
    EXPECT_GT([tracks count], 0UL);

    RetainPtr<NSArray<AVAssetTrack *>> videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    EXPECT_GT([videoTracks count], 0UL);
    RetainPtr<AVAssetTrack> track = [videoTracks firstObject];
    RetainPtr<NSArray> descriptions = [track formatDescriptions];
    EXPECT_GT([descriptions count], 0UL);

    RetainPtr description = (CMFormatDescriptionRef)[descriptions firstObject];
    RetainPtr originalDictionary = PAL::CMFormatDescriptionGetExtensions(description.get());
    EXPECT_TRUE(!!description);
    RetainPtr originalCameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(PAL::CMFormatDescriptionGetExtension(description.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(!!originalCameraCalibrationArray);

    auto immersiveVideoMetadata = WebCore::immersiveVideoMetadataFromFormatDescription(description.get());
    EXPECT_TRUE(immersiveVideoMetadata.has_value());
    EXPECT_EQ(immersiveVideoMetadata->kind, WebCore::VideoProjectionMetadataKind::Parametric);
    EXPECT_FALSE(immersiveVideoMetadata->hasLeftStereoEyeView.has_value());
    EXPECT_FALSE(immersiveVideoMetadata->hasRightStereoEyeView.has_value());
    EXPECT_FALSE(immersiveVideoMetadata->heroEye.has_value());
    EXPECT_EQ(immersiveVideoMetadata->cameraCalibrationDataLensCollection.size(), 1UL);
    EXPECT_EQ(immersiveVideoMetadata->cameraCalibrationDataLensCollection[0].lensRole, WebCore::LensRole::Mono);

    RetainPtr dictionary = WebCore::formatDescriptionDictionaryFromImmersiveVideoMetadata(*immersiveVideoMetadata);
    EXPECT_TRUE(!!dictionary);
    EXPECT_EQ(CFDictionaryGetCount(dictionary.get()), 2L);
    RetainPtr cameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(compareCameraCalibrationArray(originalCameraCalibrationArray.get(), cameraCalibrationArray.get()));
}

TEST(ImmersiveVideoMetadata, ImmersiveStereo_SideBySide)
{
    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:@"immersivestereo-sbs" withExtension:@"mp4"];
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:nil]);
    RetainPtr<NSArray> tracks = [asset tracks];
    EXPECT_GT([tracks count], 0UL);

    RetainPtr<NSArray<AVAssetTrack *>> videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    EXPECT_GT([videoTracks count], 0UL);
    RetainPtr<AVAssetTrack> track = [videoTracks firstObject];
    RetainPtr<NSArray> descriptions = [track formatDescriptions];
    EXPECT_GT([descriptions count], 0UL);

    RetainPtr description = (CMFormatDescriptionRef)[descriptions firstObject];
    RetainPtr originalDictionary = PAL::CMFormatDescriptionGetExtensions(description.get());
    EXPECT_TRUE(!!description);
    RetainPtr originalCameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(PAL::CMFormatDescriptionGetExtension(description.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(!!originalCameraCalibrationArray);

    auto immersiveVideoMetadata = WebCore::immersiveVideoMetadataFromFormatDescription(description.get());
    EXPECT_TRUE(immersiveVideoMetadata.has_value());
    EXPECT_EQ(immersiveVideoMetadata->kind, WebCore::VideoProjectionMetadataKind::Parametric);
    EXPECT_TRUE(immersiveVideoMetadata->hasLeftStereoEyeView.has_value());
    EXPECT_TRUE(*immersiveVideoMetadata->hasLeftStereoEyeView);
    EXPECT_TRUE(immersiveVideoMetadata->hasRightStereoEyeView.has_value());
    EXPECT_TRUE(*immersiveVideoMetadata->hasRightStereoEyeView);
    EXPECT_FALSE(immersiveVideoMetadata->heroEye.has_value());
    EXPECT_TRUE(immersiveVideoMetadata->viewPackingKind.has_value());
    EXPECT_EQ(*immersiveVideoMetadata->viewPackingKind, WebCore::ViewPackingKind::SideBySide);
    EXPECT_EQ(immersiveVideoMetadata->cameraCalibrationDataLensCollection.size(), 2UL);

    RetainPtr dictionary = WebCore::formatDescriptionDictionaryFromImmersiveVideoMetadata(*immersiveVideoMetadata);
    EXPECT_TRUE(!!dictionary);
    EXPECT_EQ(CFDictionaryGetCount(dictionary.get()), 5L);
    RetainPtr cameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(compareCameraCalibrationArray(originalCameraCalibrationArray.get(), cameraCalibrationArray.get()));
}

TEST(ImmersiveVideoMetadata, ImmersiveStereo_OverUnder)
{
    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:@"immersivestereo-overunder" withExtension:@"mp4"];
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:nil]);
    RetainPtr<NSArray> tracks = [asset tracks];
    EXPECT_GT([tracks count], 0UL);

    RetainPtr<NSArray<AVAssetTrack *>> videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    EXPECT_GT([videoTracks count], 0UL);
    RetainPtr<AVAssetTrack> track = [videoTracks firstObject];
    RetainPtr<NSArray> descriptions = [track formatDescriptions];
    EXPECT_GT([descriptions count], 0UL);

    RetainPtr description = (CMFormatDescriptionRef)[descriptions firstObject];
    RetainPtr originalDictionary = PAL::CMFormatDescriptionGetExtensions(description.get());
    EXPECT_TRUE(!!description);
    RetainPtr originalCameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(PAL::CMFormatDescriptionGetExtension(description.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(!!originalCameraCalibrationArray);

    auto immersiveVideoMetadata = WebCore::immersiveVideoMetadataFromFormatDescription(description.get());
    EXPECT_TRUE(immersiveVideoMetadata.has_value());
    EXPECT_EQ(immersiveVideoMetadata->kind, WebCore::VideoProjectionMetadataKind::Parametric);
    EXPECT_TRUE(immersiveVideoMetadata->hasLeftStereoEyeView.has_value());
    EXPECT_TRUE(*immersiveVideoMetadata->hasLeftStereoEyeView);
    EXPECT_TRUE(immersiveVideoMetadata->hasRightStereoEyeView.has_value());
    EXPECT_TRUE(*immersiveVideoMetadata->hasRightStereoEyeView);
    EXPECT_FALSE(immersiveVideoMetadata->heroEye.has_value());
    EXPECT_EQ(*immersiveVideoMetadata->viewPackingKind, WebCore::ViewPackingKind::OverUnder);
    EXPECT_EQ(immersiveVideoMetadata->cameraCalibrationDataLensCollection.size(), 2UL);

    RetainPtr dictionary = WebCore::formatDescriptionDictionaryFromImmersiveVideoMetadata(*immersiveVideoMetadata);
    EXPECT_TRUE(!!dictionary);
    EXPECT_EQ(CFDictionaryGetCount(dictionary.get()), 5L);
    RetainPtr cameraCalibrationArray = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection));
    EXPECT_TRUE(compareCameraCalibrationArray(originalCameraCalibrationArray.get(), cameraCalibrationArray.get()));
}

}
#endif
