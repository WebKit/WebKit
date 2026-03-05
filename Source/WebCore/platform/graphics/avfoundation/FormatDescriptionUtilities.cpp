/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FormatDescriptionUtilities.h"

#include "AV1Utilities.h"
#include "FloatSize.h"
#include "FourCC.h"
#include "HEVCUtilities.h"
#include "ImmersiveVideoMetadata.h"
#include "Logging.h"
#include "PlatformVideoColorSpace.h"
#include "SharedBuffer.h"
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/cf/VectorCF.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/cf/VideoToolboxSoftLink.h>

#if HAVE(IMMERSIVE_VIDEO_METADATA_SUPPORT)
namespace WTF {

static std::optional<RetainPtr<CFDictionaryRef>> makeVectorElement(const RetainPtr<CFDictionaryRef>*, CFDictionaryRef dictionary)
{
    return dictionary;
}

}
#endif

namespace WebCore {

TrackInfoTrackType typeFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(formatDescription);
    switch (mediaType) {
    case kCMMediaType_Video:
        return TrackInfoTrackType::Video;
    case kCMMediaType_Audio:
        return TrackInfoTrackType::Audio;
    case kCMMediaType_Text:
    case kCMMediaType_ClosedCaption:
    case kCMMediaType_Subtitle:
        return TrackInfoTrackType::Text;
    default:
        return TrackInfoTrackType::Unknown;
    }
}

FloatSize presentationSizeFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return { };

    return FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
}

std::optional<PlatformVideoColorSpace> colorSpaceFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return std::nullopt;

    PlatformVideoColorSpace colorSpace;
    RetainPtr primaries = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_ColorPrimariesSingleton()));
    RetainPtr transfer = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_TransferFunctionSingleton()));
    RetainPtr matrix = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_YCbCrMatrixSingleton()));

    if (!primaries || !transfer || !matrix) {
        auto size = presentationSizeFromFormatDescription(formatDescription);
        auto codecType = PAL::CMFormatDescriptionGetMediaSubType(formatDescription);

        CFStringRef defaultPrimaries = nullptr;
        CFStringRef defaultTransfer = nullptr;
        CFStringRef defaultMatrix = nullptr;

        PAL::VTGetDefaultColorAttributesWithHints(codecType, nullptr, size.width(), size.height(), &defaultPrimaries, &defaultTransfer, &defaultMatrix);
        if (!primaries && defaultPrimaries)
            primaries = defaultPrimaries;
        if (!transfer && defaultTransfer)
            transfer = defaultTransfer;
        if (!matrix && defaultMatrix)
            matrix = defaultMatrix;
    }

    if (primaries) {
        if (safeCFEqual(primaries.get(), PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_ITU_R_709_2Singleton()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt709;
        else if (safeCFEqual(primaries.get(), PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_EBU_3213Singleton()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt470bg;
        else if (safeCFEqual(primaries.get(), PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_SMPTE_CSingleton()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Smpte170m;
    }

    if (transfer) {
        if (safeCFEqual(transfer.get(), PAL::get_CoreMedia_kCMFormatDescriptionTransferFunction_ITU_R_709_2Singleton()))
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt709;
        else if (safeCFEqual(transfer.get(), PAL::kCMFormatDescriptionTransferFunction_sRGB))
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Iec6196621;
    }

    if (matrix) {
        if (safeCFEqual(matrix.get(), PAL::get_CoreMedia_kCVImageBufferYCbCrMatrix_ITU_R_709_2Singleton()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt709;
        else if (safeCFEqual(matrix.get(), PAL::get_CoreMedia_kCVImageBufferYCbCrMatrix_ITU_R_601_4Singleton()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt470bg;
        else if (safeCFEqual(matrix.get(), PAL::get_CoreMedia_kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995Singleton()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Smpte170m;
    }

    if (RetainPtr fullRange = static_cast<CFBooleanRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_FullRangeVideo)))
        colorSpace.fullRange = CFBooleanGetValue(fullRange.get());

    return colorSpace;
}

String codecFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return emptyString();

    auto subType = PAL::softLink_CoreMedia_CMFormatDescriptionGetMediaSubType(formatDescription);
    CFStringRef originalFormatKey = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat() ? PAL::kCMFormatDescriptionExtension_ProtectedContentOriginalFormat : CFSTR("CommonEncryptionOriginalFormat");
    if (RetainPtr originalFormat = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, originalFormatKey)))
        CFNumberGetValue(originalFormat.get(), kCFNumberSInt32Type, &subType);

    switch (subType) {
    case kCMVideoCodecType_H264:
    case 'cavc': {
        RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
        if (!sampleExtensionsDict)
            return "avc1"_s;
        RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict.get(), CFSTR("avcC")));
        if (!sampleExtensions)
            return "avc1"_s;
        auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions.get());
        auto parameters = parseAVCDecoderConfigurationRecord(configurationRecordBuffer);
        if (!parameters)
            return "avc1"_s;
        return createAVCCodecParametersString(*parameters);
    }
    case kCMVideoCodecType_HEVC:
    case kCMVideoCodecType_HEVCWithAlpha:
    case 'chvc': {
        RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
        if (!sampleExtensionsDict)
            return "hvc1"_s;
        RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict.get(), CFSTR("hvcC")));
        if (!sampleExtensions)
            return "hvc1"_s;
        auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions.get());
        auto parameters = parseHEVCDecoderConfigurationRecord(kCMVideoCodecType_HEVC, configurationRecordBuffer);
        if (!parameters)
            return "hvc1"_s;
        return createHEVCCodecParametersString(*parameters);
    }
    case kCMVideoCodecType_DolbyVisionHEVC:
    case 'cdh1': {
        RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
        if (!sampleExtensionsDict)
            return "dvh1"_s;
        RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict.get(), CFSTR("dvcC")));
        if (!sampleExtensions)
            return "dvh1"_s;
        auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions.get());
        auto parameters = parseDoViDecoderConfigurationRecord(configurationRecordBuffer);
        if (!parameters)
            return "dvh1"_s;
        return createDoViCodecParametersString(*parameters);
    }
    case kCMVideoCodecType_MPEG4Video:
        return "mp4v"_s;
    case kCMVideoCodecType_VP9:
        return "vp09"_s;
    case kAudioFormatAC3:
        return "ac-3"_s;
    case kAudioFormatMPEG4AAC:
        return "mp4a.40.2"_s;
    case kAudioFormatMPEG4AAC_HE:
        return "mp4a.40.5"_s;
    case kAudioFormatMPEG4AAC_HE_V2:
        return "mp4a.40.29"_s;
    case kAudioFormatMPEG4AAC_LD:
        return "mp4a.40.23"_s;
    case kAudioFormatMPEG4AAC_ELD:
        return "mp4a.40.39"_s;
    case kAudioFormatFLAC:
        return "flac"_s;
    case kAudioFormatOpus:
        return "opus"_s;
    case kAudioFormatEnhancedAC3:
    case 'ec+3':
    case 'qec3':
    case 'ce-3':
        return "ec-3"_s;
    case 'dts ':
        return "dts"_s;
#if ENABLE(AV1)
    case kCMVideoCodecType_AV1: {
        RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
        if (!sampleExtensionsDict)
            return "av01"_s;
        RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict.get(), CFSTR("av1C")));
        if (!sampleExtensions)
            return "av01"_s;
        auto parameters = parseAV1DecoderConfigurationRecord(span(sampleExtensions.get()));
        if (!parameters)
            return "av01"_s;
        return createAV1CodecParametersString(*parameters);
    }
#endif
    }

    return emptyString();
}

bool formatDescriptionIsProtected(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return false;

    CFStringRef originalFormatKey = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat() ? PAL::kCMFormatDescriptionExtension_ProtectedContentOriginalFormat : CFSTR("CommonEncryptionOriginalFormat");

    // Note: this assumes only-and-all content which is protected will have the ProtectedContentOriginalFormat key.
    if (auto originalFormat = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, originalFormatKey))) {
        UNUSED_PARAM(originalFormat);
        return true;
    }

    return false;
}

#if HAVE(IMMERSIVE_VIDEO_METADATA_SUPPORT)
static std::optional<HeroEye> toHeroEye(CFStringRef eye)
{
    if (!eye)
        return { };

    if (CFStringCompare(eye, PAL::kCMFormatDescriptionHeroEye_Left, 0) == kCFCompareEqualTo)
        return HeroEye::Left;

    if (CFStringCompare(eye, PAL::kCMFormatDescriptionHeroEye_Right, 0) == kCFCompareEqualTo)
        return HeroEye::Right;

    return { };
}

static std::optional<ViewPackingKind> toViewPackingKind(CFStringRef kind)
{
    if (!kind)
        return { };

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionViewPackingKind_SideBySide, 0) == kCFCompareEqualTo)
        return ViewPackingKind::SideBySide;

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionViewPackingKind_OverUnder, 0) == kCFCompareEqualTo)
        return ViewPackingKind::OverUnder;

    return { };
}

static std::optional<LensAlgorithmKind> toLensAlgorithmKind(CFStringRef kind)
{
    if (!kind)
        return { };
    if (CFStringCompare(kind, PAL::kCMFormatDescriptionCameraCalibrationLensAlgorithmKind_ParametricLens, 0) == kCFCompareEqualTo)
        return LensAlgorithmKind::ParametricLens;
    return { };
}

static std::optional<LensDomain> toLensDomain(CFStringRef domain)
{
    if (!domain)
        return { };
    if (CFStringCompare(domain, PAL::kCMFormatDescriptionCameraCalibrationLensDomain_Color, 0) == kCFCompareEqualTo)
        return LensDomain::Color;
    return { };
}

static std::optional<LensRole> toLensRole(CFStringRef role)
{
    if (!role)
        return { };
    if (CFStringCompare(role, PAL::kCMFormatDescriptionCameraCalibrationLensRole_Mono, 0) == kCFCompareEqualTo)
        return LensRole::Mono;
    if (CFStringCompare(role, PAL::kCMFormatDescriptionCameraCalibrationLensRole_Left, 0) == kCFCompareEqualTo)
        return LensRole::Left;
    if (CFStringCompare(role, PAL::kCMFormatDescriptionCameraCalibrationLensRole_Right, 0) == kCFCompareEqualTo)
        return LensRole::Right;
    return { };
}

static std::optional<ExtrinsicOriginSource> toExtrinsicOriginSource(CFStringRef source)
{
    if (!source)
        return { };
    if (CFStringCompare(source, PAL::kCMFormatDescriptionCameraCalibrationExtrinsicOriginSource_StereoCameraSystemBaseline, 0) == kCFCompareEqualTo)
        return ExtrinsicOriginSource::StereoCameraSystemBaseline;
    return { };
}

static Vector<CameraCalibration> toCameraCalibrationDataLensCollection(CFArrayRef array)
{
    Vector<CameraCalibration> collection;
    collection.reserveInitialCapacity(CFArrayGetCount(array));

    for (RetainPtr dictionary : makeVector<RetainPtr<CFDictionaryRef>, CFDictionaryRef>(array)) {
        auto lensAlgorithmKind = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensAlgorithmKind));
        auto lensDomain = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensDomain));
        auto lensIdentifier = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensIdentifier));
        auto lensRole = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensRole));
        auto lensDistortions = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensDistortions));
        auto intrinsicMatrix = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrix));
        auto lensFrameAdjustmentsPolynomialX = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialX));
        auto lensFrameAdjustmentsPolynomialY = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialY));
        auto radialAngleLimit = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_RadialAngleLimit));
        auto intrinsicMatrixProjectionOffset = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixProjectionOffset));
        auto intrinsicMatrixReferenceDimensions = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixReferenceDimensions));
        auto extrinsicOriginSource = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOriginSource));
        auto extrinsicOrientationQuaternion = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOrientationQuaternion));

        if (!lensAlgorithmKind || !lensDomain || !lensIdentifier || !lensDistortions || !intrinsicMatrix || !intrinsicMatrixProjectionOffset || !intrinsicMatrixReferenceDimensions || !extrinsicOriginSource || !extrinsicOrientationQuaternion) {
            RELEASE_LOG_ERROR(Media, "Invalid CameraCalibrationDataLens, compulsory fields missing");
            return { };
        }
        if (!lensRole || !lensFrameAdjustmentsPolynomialX || !lensFrameAdjustmentsPolynomialY || !radialAngleLimit) {
            RELEASE_LOG_ERROR(Media, "Legacy APMP detected, failing");
            return { };
        }

        CameraCalibration cameraCalibration;
        if (auto kind = toLensAlgorithmKind(lensAlgorithmKind))
            cameraCalibration.lensAlgorithmKind = *kind;
        else
            return { };
        if (auto domain = toLensDomain(lensDomain))
            cameraCalibration.lensDomain = *domain;
        else
            return { };
        CFNumberGetValue(lensIdentifier, kCFNumberSInt32Type, &cameraCalibration.lensIdentifier);
        if (auto role = toLensRole(lensRole))
            cameraCalibration.lensRole = *role;
        else
            return { };

        cameraCalibration.lensDistortions = makeVector<float, CFNumberRef>(lensDistortions);
        cameraCalibration.lensFrameAdjustmentsPolynomialX = makeVector<float, CFNumberRef>(lensFrameAdjustmentsPolynomialX);
        cameraCalibration.lensFrameAdjustmentsPolynomialY = makeVector<float, CFNumberRef>(lensFrameAdjustmentsPolynomialY);

        CFNumberGetValue(radialAngleLimit, kCFNumberFloat32Type, &cameraCalibration.radialAngleLimit);

        if (CFDataGetLength(intrinsicMatrix) != sizeof(cameraCalibration.intrinsicMatrix))
            return { };
        CFDataGetBytes(intrinsicMatrix, CFRangeMake(0, sizeof(cameraCalibration.intrinsicMatrix)), reinterpret_cast<UInt8*>(&cameraCalibration.intrinsicMatrix));
        CFNumberGetValue(intrinsicMatrixProjectionOffset, kCFNumberFloat32Type, &cameraCalibration.intrinsicMatrixProjectionOffset);

        CGSize size = CGSizeZero;
        if (!CGSizeMakeWithDictionaryRepresentation(intrinsicMatrixReferenceDimensions, &size))
            return { };
        cameraCalibration.intrinsicMatrixReferenceDimensions = { size.width, size.height };

        if (auto source = toExtrinsicOriginSource(extrinsicOriginSource))
            cameraCalibration.extrinsicOriginSource = *source;
        else
            return { };

        cameraCalibration.extrinsicOrientationQuaternion = makeVector<float, CFNumberRef>(extrinsicOrientationQuaternion);

        collection.append(WTF::move(cameraCalibration));
    }
    return collection;
}

static std::optional<VideoProjectionMetadataKind> toVideoProjectionMetadataKind(CFStringRef kind)
{
    if (!kind)
        return { };

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_Rectilinear, 0) == kCFCompareEqualTo)
        return ImmersiveVideoMetadata::Kind::Rectilinear;

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_Equirectangular, 0) == kCFCompareEqualTo)
        return ImmersiveVideoMetadata::Kind::Equirectangular;

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_HalfEquirectangular, 0) == kCFCompareEqualTo)
        return ImmersiveVideoMetadata::Kind::HalfEquirectangular;

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_ParametricImmersive, 0) == kCFCompareEqualTo)
        return ImmersiveVideoMetadata::Kind::Parametric;

    if (CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_AppleImmersiveVideo, 0) == kCFCompareEqualTo)
        return ImmersiveVideoMetadata::Kind::AppleImmersiveVideo;

    return { };
}

#endif

std::optional<ImmersiveVideoMetadata> immersiveVideoMetadataFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return { };

#if HAVE(IMMERSIVE_VIDEO_METADATA_SUPPORT)
    // Note: this assumes that the spatial metadata is in the first section of the format description.
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return { };

    auto projectionKind = toVideoProjectionMetadataKind(dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_ProjectionKind)));
    if (!projectionKind)
        return { };

    ImmersiveVideoMetadata metadata;
    metadata.kind = *projectionKind;

    if (auto horizontalFieldOfView = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView))) {
        metadata.horizontalFieldOfView.emplace(0);
        CFNumberGetValue(horizontalFieldOfView, kCFNumberSInt32Type, &*metadata.horizontalFieldOfView);
    }
    if (auto baselineField = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_StereoCameraBaseline))) {
        metadata.stereoCameraBaseline.emplace(0);
        CFNumberGetValue(baselineField, kCFNumberSInt32Type, &*metadata.stereoCameraBaseline);
    }

    if (auto disparityAdjustmentField = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment))) {
        metadata.horizontalDisparityAdjustment.emplace(0);
        CFNumberGetValue(disparityAdjustmentField, kCFNumberSInt32Type, &*metadata.horizontalDisparityAdjustment);
    }

    CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(formatDescription);
    metadata.size = { dimensions.width, dimensions.height };

    if (auto hasLeftStereoEyeView = dynamic_cf_cast<CFBooleanRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HasLeftStereoEyeView)))
        metadata.hasLeftStereoEyeView = CFBooleanGetValue(hasLeftStereoEyeView);
    if (auto hasRightStereoEyeView = dynamic_cf_cast<CFBooleanRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HasRightStereoEyeView)))
        metadata.hasRightStereoEyeView = CFBooleanGetValue(hasRightStereoEyeView);
    if (auto heroEye = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HeroEye)))
        metadata.heroEye = toHeroEye(heroEye);
    if (auto viewPackingKind = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_ViewPackingKind)))
        metadata.viewPackingKind = toViewPackingKind(viewPackingKind);

    if (auto collection = dynamic_cf_cast<CFArrayRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection)))
        metadata.cameraCalibrationDataLensCollection = toCameraCalibrationDataLensCollection(collection);

    return metadata;
#else
    return { };
#endif
}

#if HAVE(IMMERSIVE_VIDEO_METADATA_SUPPORT)
RetainPtr<CFDictionaryRef> extractImmersiveVideoMetadata(CMFormatDescriptionRef description)
{
    static CFStringRef keys[] = {
        PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection,
        PAL::kCMFormatDescriptionExtension_HasLeftStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HasRightStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HeroEye,
        PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView,
        PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment,
        PAL::kCMFormatDescriptionExtension_StereoCameraBaseline,
        PAL::kCMFormatDescriptionExtension_ProjectionKind,
        PAL::kCMFormatDescriptionExtension_ViewPackingKind
    };

    static constexpr size_t numberOfKeys = sizeof(keys) / sizeof(keys[0]);
    auto keysSpan = unsafeMakeSpan(keys, numberOfKeys);
    size_t keysSet = 0;
    Vector<RetainPtr<CFPropertyListRef>, numberOfKeys> values(numberOfKeys, [&](size_t index) -> RetainPtr<CFPropertyListRef> {
        RetainPtr value = PAL::CMFormatDescriptionGetExtension(description, RetainPtr { keysSpan[index] }.get());
        if (!value)
            return nullptr;
        keysSet++;
        return value;
    });

    if (!keysSet)
        return nullptr;

    RetainPtr<CFMutableDictionaryRef> extensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, keysSet, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    for (size_t index = 0; index < numberOfKeys; index++) {
        if (values[index])
            CFDictionarySetValue(extensions.get(), keysSpan[index], values[index].get());
    }
    return extensions;
}

RetainPtr<CFDictionaryRef> formatDescriptionDictionaryFromImmersiveVideoMetadata(const ImmersiveVideoMetadata& metadata)
{
    RetainPtr<CFMutableDictionaryRef> extensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 9, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto kind = [](auto kind) -> CFStringRef {
        switch (kind) {
        case ImmersiveVideoMetadata::Kind::Rectilinear:
            return PAL::kCMFormatDescriptionProjectionKind_Rectilinear;
        case ImmersiveVideoMetadata::Kind::Equirectangular:
            return PAL::kCMFormatDescriptionProjectionKind_Equirectangular;
        case ImmersiveVideoMetadata::Kind::HalfEquirectangular:
            return PAL::kCMFormatDescriptionProjectionKind_HalfEquirectangular;
        case ImmersiveVideoMetadata::Kind::Parametric:
            return PAL::kCMFormatDescriptionProjectionKind_ParametricImmersive;
        case ImmersiveVideoMetadata::Kind::AppleImmersiveVideo:
            return PAL::kCMFormatDescriptionProjectionKind_AppleImmersiveVideo;
        default:
            return nil;
        }
    }(metadata.kind);
    if (kind)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_ProjectionKind, kind);

    if (metadata.horizontalFieldOfView)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &*metadata.horizontalFieldOfView)).get());
    if (metadata.stereoCameraBaseline)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_StereoCameraBaseline, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &*metadata.stereoCameraBaseline)).get());
    if (metadata.horizontalDisparityAdjustment)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &*metadata.horizontalDisparityAdjustment)).get());

    if (metadata.hasLeftStereoEyeView)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_HasLeftStereoEyeView, *metadata.hasLeftStereoEyeView ? kCFBooleanTrue : kCFBooleanFalse);
    if (metadata.hasRightStereoEyeView)
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_HasRightStereoEyeView, *metadata.hasRightStereoEyeView ? kCFBooleanTrue : kCFBooleanFalse);

    if (metadata.heroEye) {
        CFStringRef heroEye = [](auto eye) {
            switch (eye) {
            case HeroEye::Left:
                return PAL::kCMFormatDescriptionHeroEye_Left;
            case HeroEye::Right:
                return PAL::kCMFormatDescriptionHeroEye_Right;
            }
        }(*metadata.heroEye);
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_HeroEye, heroEye);
    }

    if (metadata.viewPackingKind) {
        CFStringRef viewPackingKind = [](auto viewPackingKind) {
            switch (viewPackingKind) {
            case ViewPackingKind::SideBySide:
                return PAL::kCMFormatDescriptionViewPackingKind_SideBySide;
            case ViewPackingKind::OverUnder:
                return PAL::kCMFormatDescriptionViewPackingKind_OverUnder;
            }
        }(*metadata.viewPackingKind);
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_ViewPackingKind, viewPackingKind);
    }

    RetainPtr array = adoptCF(CFArrayCreateMutable(nullptr, metadata.cameraCalibrationDataLensCollection.size(), &kCFTypeArrayCallBacks));
    for (auto& cameraCalibration : metadata.cameraCalibrationDataLensCollection) {
        RetainPtr dictionary = adoptCF(CFDictionaryCreateMutable(nullptr, 13, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        auto lensAlgorithmKind = [](auto lensAlgorithmKind) {
            switch (lensAlgorithmKind) {
            case LensAlgorithmKind::ParametricLens:
                return PAL::kCMFormatDescriptionCameraCalibrationLensAlgorithmKind_ParametricLens;
            }
        }(cameraCalibration.lensAlgorithmKind);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensAlgorithmKind, lensAlgorithmKind);

        auto lensDomain = [](auto lensDomain) {
            switch (lensDomain) {
            case LensDomain::Color:
                return PAL::kCMFormatDescriptionCameraCalibrationLensDomain_Color;
            }
        }(cameraCalibration.lensDomain);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensDomain, lensDomain);

        RetainPtr lensIdentifier = adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &cameraCalibration.lensIdentifier));
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensIdentifier, lensIdentifier.get());

        auto lensRole = [](auto lensRole) {
            switch (lensRole) {
            case LensRole::Mono:
                return PAL::kCMFormatDescriptionCameraCalibrationLensRole_Mono;
            case LensRole::Left:
                return PAL::kCMFormatDescriptionCameraCalibrationLensRole_Left;
            case LensRole::Right:
                return PAL::kCMFormatDescriptionCameraCalibrationLensRole_Right;
            }
        }(cameraCalibration.lensRole);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensRole, lensRole);

        RetainPtr lensDistortions = createCFArray(cameraCalibration.lensDistortions);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensDistortions, lensDistortions.get());

        RetainPtr lensFrameAdjustmentsPolynomialX = createCFArray(cameraCalibration.lensFrameAdjustmentsPolynomialX);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialX, lensFrameAdjustmentsPolynomialX.get());

        RetainPtr lensFrameAdjustmentsPolynomialY = createCFArray(cameraCalibration.lensFrameAdjustmentsPolynomialY);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_LensFrameAdjustmentsPolynomialY, lensFrameAdjustmentsPolynomialY.get());

        RetainPtr radialAngleLimit = adoptCF(CFNumberCreate(nullptr, kCFNumberFloat32Type, &cameraCalibration.radialAngleLimit));
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_RadialAngleLimit, radialAngleLimit.get());

        RetainPtr intrinsicMatrix = adoptCF(CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&cameraCalibration.intrinsicMatrix), sizeof(cameraCalibration.intrinsicMatrix)));
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrix, intrinsicMatrix.get());

        RetainPtr intrinsicMatrixProjectionOffset = adoptCF(CFNumberCreate(nullptr, kCFNumberFloat32Type, &cameraCalibration.intrinsicMatrixProjectionOffset));
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixProjectionOffset, intrinsicMatrixProjectionOffset.get());

        RetainPtr intrinsicMatrixReferenceDimensions = adoptCF(CGSizeCreateDictionaryRepresentation({
            .width = CGFloat(cameraCalibration.intrinsicMatrixReferenceDimensions.width()),
            .height = CGFloat(cameraCalibration.intrinsicMatrixReferenceDimensions.height())
        }));
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_IntrinsicMatrixReferenceDimensions, intrinsicMatrixReferenceDimensions.get());

        auto extrinsicOriginSource = [](auto extrinsicOriginSource) {
            switch (extrinsicOriginSource) {
            case ExtrinsicOriginSource::StereoCameraSystemBaseline:
                return PAL::kCMFormatDescriptionCameraCalibrationExtrinsicOriginSource_StereoCameraSystemBaseline;
            }
        }(cameraCalibration.extrinsicOriginSource);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOriginSource, extrinsicOriginSource);

        RetainPtr extrinsicOrientationQuaternion = createCFArray(cameraCalibration.extrinsicOrientationQuaternion);
        CFDictionarySetValue(dictionary.get(), PAL::kCMFormatDescriptionCameraCalibration_ExtrinsicOrientationQuaternion, extrinsicOrientationQuaternion.get());

        CFArrayAppendValue(array.get(), dictionary.get());
    }
    CFDictionarySetValue(extensions.get(), PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection, array.get());

    return extensions;
}

#endif

} // namespace WebCore
