/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "GPUVideoEncoderVTBH265.h"

#if USE(AVFOUNDATION)

#import "HEVCUtilities.h"
#import <WebCore/HEVCUtilitiesCocoa.h>
#import <wtf/Borrow.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoEncoderVTBH265);

static std::optional<HEVCParameters> hevcParametersFromFormatParameters(const Vector<std::pair<String, String>>& parameters)
{
    // https://datatracker.ietf.org/doc/html/rfc7798#section-7.1
    std::optional<HEVCParameters> hevcParameters;
    String interopConstraints;
    for (auto& [name, value] : borrow(parameters).get()) {
        if (equalLettersIgnoringASCIICase(name, "profile-id"_s)) {
            auto profileIDC = parseInteger<uint8_t>(value);
            if (!profileIDC)
                return { };
            hevcParameters = HEVCParameters { };
            hevcParameters->generalProfileIDC = *profileIDC;
        } else if (equalLettersIgnoringASCIICase(name, "interop-constraints"_s))
            interopConstraints = value;
    }
    if (!hevcParameters)
        return { };

    auto& flags = hevcParameters->generalConstraintIndicatorFlags;
    if (interopConstraints.length() != 2 * flags.size())
        return hevcParameters;
    for (size_t i = 0; i < flags.size(); ++i) {
        auto flag = parseInteger<uint8_t>(StringView { interopConstraints }.substring(2 * i, 2), 16);
        if (!flag)
            return { };
        flags[i] = *flag;
    }
    return hevcParameters;
}

static RetainPtr<CFStringRef> computeHEVCVTProfileLevel(const Vector<std::pair<String, String>>& parameters)
{
    auto hevcParameters = hevcParametersFromFormatParameters(parameters);
    if (!hevcParameters)
        return { };

    switch (hevcParameters->generalProfileIDC) {
    case 1:
        return PAL::kVTProfileLevel_HEVC_Main_AutoLevel;
    case 2:
        return PAL::kVTProfileLevel_HEVC_Main10_AutoLevel;
    case 4:
        if (isHEVCMain42210Profile(*hevcParameters))
            return PAL::kVTProfileLevel_HEVC_Main42210_AutoLevel;
        return { };
    default:
        return { };
    }
}

bool GPUVideoEncoderVTBH265::isProfileLevelSupported(CFStringRef profileLevel, IntSize size)
{
    CFStringRef encoderID = nullptr;
    CFDictionaryRef supportedProperties = nullptr;
    if (PAL::VTCopySupportedPropertyDictionaryForEncoder(size.width(), size.height(), kCMVideoCodecType_HEVC, encoderSpecification(), &encoderID, &supportedProperties) != noErr)
        return false;
    RetainPtr adoptedEncoderID = adoptCF(encoderID);
    RetainPtr adoptedSupportedProperties = adoptCF(supportedProperties);

    RetainPtr profileLevelProperty = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(adoptedSupportedProperties, PAL::kVTCompressionPropertyKey_ProfileLevel));
    if (!profileLevelProperty)
        return false;
    RetainPtr supportedValues = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(profileLevelProperty, PAL::kVTPropertySupportedValueListKey));
    return supportedValues && CFArrayContainsValue(supportedValues, CFRangeMake(0, CFArrayGetCount(supportedValues)), profileLevel);
}

RefPtr<GPUVideoEncoderVTBH265> GPUVideoEncoderVTBH265::create(CreationInfo&& info, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
{
    RetainPtr profileLevel = computeHEVCVTProfileLevel(parameters);
    if (profileLevel && !info.size.isEmpty() && !isProfileLevelSupported(profileLevel, info.size))
        return nullptr;
    return adoptRef(*new GPUVideoEncoderVTBH265(WTF::move(info), WTF::move(profileLevel), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback)));
}

GPUVideoEncoderVTBH265::GPUVideoEncoderVTBH265(CreationInfo&& info, RetainPtr<CFStringRef>&& profileLevel, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    : GPUVideoEncoderVTB(WTF::move(info), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback))
    , m_profileLevel(WTF::move(profileLevel))
{
}

void GPUVideoEncoderVTBH265::configureAdditionalProperties()
{
    assertIsCurrent(queue());

    if (m_profileLevel)
        setProperty(PAL::kVTCompressionPropertyKey_ProfileLevel, m_profileLevel);
}

bool GPUVideoEncoderVTBH265::convertAndNotify(RetainPtr<CMSampleBufferRef>&& sampleBuffer, GPUVideoEncoderFrameInfo&& info, const PlatformVideoColorSpace& colorSpace)
{
    if (useAnnexB()) {
        auto annexBBuffer = convertHEVCCMSampleBufferToAnnexB(sampleBuffer, info.isKeyFrame);
        if (annexBBuffer.isEmpty())
            return false;

        notifyDescriptionIfNeeded(sampleBuffer, CFSTR("hvcC"), colorSpace);
        notifyEncodedFrame(annexBBuffer.span(), info);
        return true;
    }

    auto buffer = toVector(sampleBuffer);
    if (!buffer)
        return false;

    notifyDescriptionIfNeeded(sampleBuffer, CFSTR("hvcC"), colorSpace);
    notifyEncodedFrame(buffer->span(), info);
    return true;
}

}

#endif // USE(AVFOUNDATION)
