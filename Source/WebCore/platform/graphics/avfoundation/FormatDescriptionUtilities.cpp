/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "AV1Utilities.h"
#import "FloatSize.h"
#import "FourCC.h"
#import "HEVCUtilities.h"
#import "PlatformVideoColorSpace.h"
#import "SharedBuffer.h"
#import "SpatialVideoMetadata.h"
#import "VideoProjectionMetadata.h"
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {
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
    auto primaries = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_ColorPrimaries()));
    auto transfer = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_TransferFunction()));
    auto matrix = dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_YCbCrMatrix()));

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
        if (safeCFEqual(primaries, PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_ITU_R_709_2()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt709;
        else if (safeCFEqual(primaries, PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_EBU_3213()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Bt470bg;
        else if (safeCFEqual(primaries, PAL::get_CoreMedia_kCMFormatDescriptionColorPrimaries_SMPTE_C()))
            colorSpace.primaries = PlatformVideoColorPrimaries::Smpte170m;
    }

    if (transfer) {
        if (safeCFEqual(transfer, PAL::get_CoreMedia_kCMFormatDescriptionTransferFunction_ITU_R_709_2()))
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Bt709;
        else if (safeCFEqual(transfer, PAL::get_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB()))
            colorSpace.transfer = PlatformVideoTransferCharacteristics::Iec6196621;
    }

    if (matrix) {
        if (safeCFEqual(matrix, PAL::get_CoreMedia_kCVImageBufferYCbCrMatrix_ITU_R_709_2()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt709;
        else if (safeCFEqual(matrix, PAL::get_CoreMedia_kCVImageBufferYCbCrMatrix_ITU_R_601_4()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Bt470bg;
        else if (safeCFEqual(matrix, PAL::get_CoreMedia_kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995()))
            colorSpace.matrix = PlatformVideoMatrixCoefficients::Smpte170m;
    }

    if (auto fullRange = static_cast<CFBooleanRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_FullRangeVideo())))
        colorSpace.fullRange = CFBooleanGetValue(fullRange);

    return colorSpace;
}

String codecFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return emptyString();

    auto subType = PAL::softLink_CoreMedia_CMFormatDescriptionGetMediaSubType(formatDescription);
    switch (subType) {
    case kCMVideoCodecType_H264:
    case 'cavc':
        {
            auto sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()));
            if (!sampleExtensionsDict)
                return "avc1"_s;
            auto sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, CFSTR("avcC")));
            if (!sampleExtensions)
                return "avc1"_s;
            auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions);
            auto parameters = parseAVCDecoderConfigurationRecord(configurationRecordBuffer);
            if (!parameters)
                return "avc1"_s;
            return createAVCCodecParametersString(*parameters);
        }
    case kCMVideoCodecType_HEVC:
    case kCMVideoCodecType_HEVCWithAlpha:
    case 'chvc':
        {
            auto sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()));
            if (!sampleExtensionsDict)
                return "hvc1"_s;
            auto sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, CFSTR("hvcC")));
            if (!sampleExtensions)
                return "hvc1"_s;
            auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions);
            auto parameters = parseHEVCDecoderConfigurationRecord(kCMVideoCodecType_HEVC, configurationRecordBuffer);
            if (!parameters)
                return "hvc1"_s;
            return createHEVCCodecParametersString(*parameters);
        }
    case kCMVideoCodecType_DolbyVisionHEVC:
    case 'cdh1':
        {
            auto sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()));
            if (!sampleExtensionsDict)
                return "dvh1"_s;
            auto sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, CFSTR("dvcC")));
            if (!sampleExtensions)
                return "dvh1"_s;
            auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions);
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
    case kCMVideoCodecType_AV1:
        {
            auto sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()));
            if (!sampleExtensionsDict)
                return "av01"_s;
            auto sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, CFSTR("av1C")));
            if (!sampleExtensions)
                return "av01"_s;
            auto configurationRecordBuffer = SharedBuffer::create(sampleExtensions);
            auto parameters = parseAV1DecoderConfigurationRecord(configurationRecordBuffer);
            if (!parameters)
                return "av01"_s;
            return createAV1CodecParametersString(*parameters);
        }
#endif
    }

    return emptyString();
}

std::optional<SpatialVideoMetadata> spatialVideoMetadataFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return { };

    // Note: this assumes that the spatial metadata is in the first section of the format description.
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return { };

    auto horizontalFieldOfView = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView));
    if (!horizontalFieldOfView)
        return { };

    auto baselineField = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_StereoCameraBaseline));
    if (!baselineField)
        return { };

    auto disparityAdjustmentField = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment));
    if (!disparityAdjustmentField)
        return { };

    SInt32 value;
    SpatialVideoMetadata metadata;

    CFNumberGetValue(horizontalFieldOfView, kCFNumberSInt32Type, &value);
    metadata.horizontalFOVDegrees = value / 1000.0;

    CFNumberGetValue(baselineField, kCFNumberSInt32Type, &value);
    metadata.baseline = value;

    CFNumberGetValue(disparityAdjustmentField, kCFNumberSInt32Type, &value);
    metadata.disparityAdjustment = value / 10000.0;

    CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(formatDescription);
    metadata.size = { dimensions.width, dimensions.height };

    return metadata;
}

static std::optional<VideoProjectionMetadataKind> toVideoProjectionMetadataKind(CFStringRef kind)
{
    if (!kind)
        return { };

    if (PAL::canLoad_CoreMedia_kCMFormatDescriptionProjectionKind_Equirectangular()
        && CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_Equirectangular, 0) == kCFCompareEqualTo)
        return VideoProjectionMetadata::Kind::Equirectangular;

    if (PAL::canLoad_CoreMedia_kCMFormatDescriptionProjectionKind_HalfEquirectangular()
        && CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_HalfEquirectangular, 0) == kCFCompareEqualTo)
        return VideoProjectionMetadata::Kind::HalfEquirectangular;

    if (PAL::canLoad_CoreMedia_kCMFormatDescriptionProjectionKind_ParametricImmersive()
        && CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_ParametricImmersive, 0) == kCFCompareEqualTo)
        return VideoProjectionMetadata::Kind::Parametric;

    if (PAL::canLoad_CoreMedia_kCMFormatDescriptionProjectionKind_AppleImmersiveVideo()
        && CFStringCompare(kind, PAL::kCMFormatDescriptionProjectionKind_AppleImmersiveVideo, 0) == kCFCompareEqualTo)
        return VideoProjectionMetadata::Kind::AppleImmersiveVideo;

    return { };
}

std::optional<VideoProjectionMetadata> videoProjectionMetadataFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    if (!formatDescription)
        return { };

    // Note: this assumes that the spatial metadata is in the first section of the format description.
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return { };

    if (!PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProjectionKind())
        return { };

    auto projectionKind = toVideoProjectionMetadataKind(dynamic_cf_cast<CFStringRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_ProjectionKind)));
    if (!projectionKind)
        return { };

    if (projectionKind == VideoProjectionMetadata::Kind::Equirectangular
        || projectionKind == VideoProjectionMetadata::Kind::HalfEquirectangular
        || projectionKind == VideoProjectionMetadata::Kind::AppleImmersiveVideo)
        return VideoProjectionMetadata { *projectionKind, { } };

    if (projectionKind == VideoProjectionMetadata::Kind::Parametric) {
        // TODO: extract the camera lens parameters from the format description.
        return VideoProjectionMetadata { *projectionKind, { } };
    }

    return { };
}

} // namespace WebCore
