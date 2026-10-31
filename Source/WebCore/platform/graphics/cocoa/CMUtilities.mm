/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "CMUtilities.h"

#if PLATFORM(COCOA)

#import "CAAudioStreamDescription.h"
#import "CDMFairPlayStreaming.h"
#import "FormatDescriptionUtilities.h"
#import "ISOTrackEncryptionBox.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "MediaSamplesBlock.h"
#import "MediaUtilities.h"
#import "SharedBuffer.h"
#import "WebMAudioUtilitiesCocoa.h"
#import <CoreMedia/CMFormatDescription.h>
#import <cmath>
#import <numeric>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/cf/CoreAudioExtras.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/FlipBytes.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PacketDurationParser);

#if ENABLE(VORBIS) && !defined(WEBCORE_kAudioFormatVorbis_DEFINED)
#define WEBCORE_kAudioFormatVorbis_DEFINED
constexpr uint32_t kAudioFormatVorbis = 'vorb';
#endif

CAAudioStreamDescription audioStreamDescriptionFromAudioInfo(const AudioInfo& info)
{
    ASSERT(info.codecName().value != kAudioFormatLinearPCM);
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName().value;
    std::span<const uint8_t> cookieDataSpan { };
    RefPtr cookieData = info.cookieData();
    bool filled = false;
    if (cookieData && cookieData->size()) {
        cookieDataSpan = cookieData->span();
        UInt32 size = sizeof(asbd);
        if (auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, cookieDataSpan.size(), cookieDataSpan.data(), &size, &asbd))
            RELEASE_LOG_DEBUG(Media, "kAudioFormatProperty_FormatInfo failed with error %d (%.4s)", static_cast<int>(error), (char *)&error);
        else
            filled = true;
    }
    if (!filled) {
        asbd.mSampleRate = info.rate();
        asbd.mFramesPerPacket = info.framesPerPacket();
        asbd.mChannelsPerFrame = info.channels();
        asbd.mBitsPerChannel = info.bitDepth();
    }
    return asbd;
}

static FourCC cfStringToFourCC(CFStringRef string)
{
    ASSERT(CFStringGetLength(string) >= 4);
    return char(CFStringGetCharacterAtIndex(string, 0)) << 24 | char(CFStringGetCharacterAtIndex(string, 1)) << 16 | char(CFStringGetCharacterAtIndex(string, 2)) << 8 | char(CFStringGetCharacterAtIndex(string, 3));
}

static RetainPtr<CFStringRef> cfStringFromFourCC(FourCC fourCC)
{
    auto string = fourCC.string();
    return adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, string.begin(), kCFStringEncodingASCII));
}

static Vector<std::pair<FourCC, Ref<SharedBuffer>>> parseExtensionAtomsDictionary(CFDictionaryRef atomDictionary, size_t indexStart = 0)
{
    CFIndex extensionCount = CFDictionaryGetCount(atomDictionary);
    if (CFIndex(indexStart) >= extensionCount)
        return { };

    Vector<const void*, 1> keys(extensionCount);
    Vector<const void*, 1> values(extensionCount);
    CFDictionaryGetKeysAndValues(atomDictionary, keys.mutableSpan().data(), values.mutableSpan().data());
    Vector<std::pair<FourCC, Ref<SharedBuffer>>> result;
    result.reserveInitialCapacity(extensionCount - indexStart);
    // Per CMFormatDescription.h, each value is either a CFData payload or a CFArray
    // of CFData (multiple atoms of the same FourCC). Expand arrays into multiple
    // entries sharing a FourCC.
    for (CFIndex index = indexStart; index < extensionCount; ++index) {
        auto fourCC = cfStringToFourCC(protect(checked_cf_cast<CFStringRef>(keys[index])));
        RetainPtr value = static_cast<CFTypeRef>(values[index]);
        if (RetainPtr data = dynamic_cf_cast<CFDataRef>(value))
            result.append({ fourCC, SharedBuffer::create(data.get()) });
        else if (RetainPtr array = dynamic_cf_cast<CFArrayRef>(value)) {
            CFIndex arrayCount = CFArrayGetCount(array.get());
            for (CFIndex arrayIndex = 0; arrayIndex < arrayCount; ++arrayIndex) {
                if (RetainPtr entry = dynamic_cf_cast<CFDataRef>(CFArrayGetValueAtIndex(array.get(), arrayIndex)))
                    result.append({ fourCC, SharedBuffer::create(entry.get()) });
            }
        } else
            RELEASE_LOG_ERROR(Media, "Unexpected value type in SampleDescriptionExtensionAtoms for FourCC %s", fourCC.string().begin());
    }
    return result;
}

static RetainPtr<CFDictionaryRef> createExtensionAtomsDictionary(const Vector<std::pair<FourCC, Ref<SharedBuffer>>>& configurations)
{
    Vector<FourCC> orderedKeys;
    Vector<Vector<RetainPtr<CFDataRef>>> groupedDatas;
    orderedKeys.reserveInitialCapacity(configurations.size());
    groupedDatas.reserveInitialCapacity(configurations.size());
    for (auto& [fourCC, buffer] : configurations) {
        size_t existingIndex = notFound;
        for (size_t i = 0; i < orderedKeys.size(); ++i) {
            if (orderedKeys[i] == fourCC) {
                existingIndex = i;
                break;
            }
        }
        if (existingIndex == notFound) {
            orderedKeys.append(fourCC);
            groupedDatas.append({ buffer->createCFData() });
        } else
            groupedDatas[existingIndex].append(buffer->createCFData());
    }

    Vector<RetainPtr<CFTypeRef>> retainedKeys = { orderedKeys.size(), [&](auto index) -> RetainPtr<CFTypeRef> {
        return cfStringFromFourCC(orderedKeys[index]);
    } };
    Vector<RetainPtr<CFTypeRef>> retainedValues = { groupedDatas.size(), [&](auto index) -> RetainPtr<CFTypeRef> {
        auto& datas = groupedDatas[index];
        if (datas.size() == 1)
            return datas[0];
        Vector<CFTypeRef> rawDatas(datas.size(), [&](auto j) {
            return datas[j].get();
        });
        return adoptCF(CFArrayCreate(kCFAllocatorDefault, rawDatas.begin(), rawDatas.size(), &kCFTypeArrayCallBacks));
    } };
    Vector<CFTypeRef> rawConfigurationKeys(retainedKeys.size(), [&](auto index) {
        return retainedKeys[index].get();
    });
    Vector<CFTypeRef> rawConfigurationValues(retainedValues.size(), [&](auto index) {
        return retainedValues[index].get();
    });
    ASSERT(rawConfigurationKeys.size() == rawConfigurationValues.size());

    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, rawConfigurationKeys.begin(), rawConfigurationValues.begin(), rawConfigurationKeys.size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
static std::optional<EncryptionDataCollection> getEncryptionDataCollection(CMFormatDescriptionRef description)
{
    RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(description);
    std::optional<TrackInfoEncryptionData> encryptionData = [](CMFormatDescriptionRef description) -> std::optional<TrackInfoEncryptionData> {
        if (RetainPtr trackEncryptionData = dynamic_cf_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("CommonEncryptionTrackEncryptionBox"))))
            return TrackInfoEncryptionData { EncryptionBoxType::CommonEncryptionTrackEncryptionBox, SharedBuffer::create(trackEncryptionData.get()) };
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
        if (RetainPtr trackEncryptionData = dynamic_cf_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("TransportStreamEncryptionInitData"))))
            return TrackInfoEncryptionData { EncryptionBoxType::TransportStreamEncryptionInitData, SharedBuffer::create(trackEncryptionData.get()) };
#endif
        return std::nullopt;
    }(description);

    if (!encryptionData)
        return { };

    std::optional<FourCC> encryptionOriginalFormat;
    RetainPtr cfEncryptionOriginalFormat = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("CommonEncryptionOriginalFormat")));
    if (cfEncryptionOriginalFormat) {
        uint32_t plainTextCodecType = 0;
        CFNumberGetValue(cfEncryptionOriginalFormat.get(), kCFNumberSInt32Type, &plainTextCodecType);
        encryptionOriginalFormat = plainTextCodecType;
    }

    // For video, every atom in this dictionary (including any codec configuration atom) is
    // already captured in full by VideoInfo::extensionAtoms(); encryptionInitDatas is left
    // empty here for video so createExtensionsDictionary() sources these atoms from a single place.
    if (PAL::CMFormatDescriptionGetMediaType(description) == kCMMediaType_Video) {
        return EncryptionDataCollection {
            .encryptionData = WTF::move(*encryptionData),
            .encryptionOriginalFormat = encryptionOriginalFormat
        };
    }

    RetainPtr extensionAtoms = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
    if (!extensionAtoms) {
        return EncryptionDataCollection {
            .encryptionData = WTF::move(*encryptionData),
            .encryptionOriginalFormat = encryptionOriginalFormat
        };
    }

    auto encryptionInitDatas = parseExtensionAtomsDictionary(extensionAtoms.get());

    return EncryptionDataCollection {
        .encryptionData = WTF::move(*encryptionData),
        .encryptionOriginalFormat = encryptionOriginalFormat,
        .encryptionInitDatas = WTF::move(encryptionInitDatas)
    };
}
#endif

static RetainPtr<CFMutableDictionaryRef> createExtensionsDictionary(const TrackInfo& info)
{
    if (info.isAudio()) {
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (!info.encryptionDataCollection())
            return nullptr;
#else
        return nullptr;
#endif
    }

    // A sizing hint only; exceeding it costs a rehash. The video counts include the keys
    // createFormatDescriptionFromTrackInfo() goes on to add: FullRangeVideo, BitsPerComponent,
    // ColorPrimaries, TransferFunction, GammaLevel, YCbCrMatrix, both ChromaLocation fields,
    // FieldCount, FieldDetail and PixelAspectRatio.
    size_t maxNumberOfElements = [&] {
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        return info.isAudio() ? 5 : 15;
#else
        return info.isAudio() ? 5 : 12;
#endif
    }();
    RetainPtr extensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, maxNumberOfElements, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!extensions)
        return nullptr;

    Vector<std::pair<FourCC, Ref<SharedBuffer>>> configurations;
    CFIndex configurationsNumKey = 0;
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (auto& encryptionCollection = info.encryptionDataCollection()) {
        CFDictionaryAddValue(extensions.get(), CFSTR("CommonEncryptionProtected"), kCFBooleanTrue);
        RetainPtr data = Ref { encryptionCollection->encryptionData.second }->createCFData();
        switch (encryptionCollection->encryptionData.first) {
        case EncryptionBoxType::CommonEncryptionTrackEncryptionBox:
            CFDictionaryAddValue(extensions.get(), CFSTR("CommonEncryptionTrackEncryptionBox"), data.get());
            break;
        case EncryptionBoxType::TransportStreamEncryptionInitData:
            CFDictionaryAddValue(extensions.get(), CFSTR("TransportStreamEncryptionInitData"), data.get());
            break;
        }
        if (encryptionCollection->encryptionOriginalFormat) {
            RetainPtr originalFormat = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &encryptionCollection->encryptionOriginalFormat->value));
            CFDictionaryAddValue(extensions.get(), CFSTR("CommonEncryptionOriginalFormat"), originalFormat.get());
        }
        configurationsNumKey += encryptionCollection->encryptionInitDatas.size();
    }
#endif

    if (info.isVideo())
        configurationsNumKey += downcast<const VideoInfo>(info).extensionAtoms().size();

    configurations.reserveInitialCapacity(configurationsNumKey);

#if ENABLE(ENCRYPTED_MEDIA)
    if (auto& encryptionCollection = info.encryptionDataCollection())
        configurations.appendVector(encryptionCollection->encryptionInitDatas);
#endif

    if (info.isVideo())
        configurations.appendVector(downcast<const VideoInfo>(info).extensionAtoms());

    if (configurations.size())
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, createExtensionAtomsDictionary(configurations).get());

    return extensions;
}

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioInfo& info)
{
    auto streamDescription = audioStreamDescriptionFromAudioInfo(info);
    std::span<const uint8_t> cookie;
    RefPtr cookieData = info.cookieData();
    if (cookieData)
        cookie = cookieData->span();

    RetainPtr<CFDictionaryRef> extensions = createExtensionsDictionary(info);

    auto basicDescription = std::get<const AudioStreamBasicDescription*>(streamDescription.platformDescription().description);
    CMFormatDescriptionRef format = nullptr;
    auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, basicDescription, 0, nullptr, cookie.size(), cookie.data(), extensions.get(), &format);
    if (error) {
        LOG_ERROR("createAudioFormatDescription failed with %d", static_cast<int>(error));
        return nullptr;
    }
    return adoptCF(format);
}

CFStringRef convertToCMColorPrimaries(PlatformVideoColorPrimaries primaries)
{
    switch (primaries) {
    case PlatformVideoColorPrimaries::Bt709:
        return kCVImageBufferColorPrimaries_ITU_R_709_2;
    case PlatformVideoColorPrimaries::JedecP22Phosphors:
        return kCVImageBufferColorPrimaries_EBU_3213;
    case PlatformVideoColorPrimaries::Smpte170m:
    case PlatformVideoColorPrimaries::Smpte240m:
        return kCVImageBufferColorPrimaries_SMPTE_C;
    case PlatformVideoColorPrimaries::SmpteRp431:
        return PAL::kCMFormatDescriptionColorPrimaries_DCI_P3;
    case PlatformVideoColorPrimaries::SmpteEg432:
        return PAL::kCMFormatDescriptionColorPrimaries_P3_D65;
    case PlatformVideoColorPrimaries::Bt2020:
        return PAL::kCMFormatDescriptionColorPrimaries_ITU_R_2020;
    default:
        return nullptr;
    }
}

CFStringRef convertToCMTransferFunction(PlatformVideoTransferCharacteristics characteristics)
{
    switch (characteristics) {
    case PlatformVideoTransferCharacteristics::Smpte170m:
    case PlatformVideoTransferCharacteristics::Bt709:
        return kCVImageBufferTransferFunction_ITU_R_709_2;
    case PlatformVideoTransferCharacteristics::Smpte240m:
        return kCVImageBufferTransferFunction_SMPTE_240M_1995;
    case PlatformVideoTransferCharacteristics::SmpteSt2084:
        return PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;
    case PlatformVideoTransferCharacteristics::Bt2020_10bit:
    case PlatformVideoTransferCharacteristics::Bt2020_12bit:
        return PAL::kCMFormatDescriptionTransferFunction_ITU_R_2020;
    case PlatformVideoTransferCharacteristics::SmpteSt4281:
        return PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;
    case PlatformVideoTransferCharacteristics::AribStdB67Hlg:
        return PAL::kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;
    case PlatformVideoTransferCharacteristics::Iec6196621:
        return PAL::kCMFormatDescriptionTransferFunction_sRGB;
    case PlatformVideoTransferCharacteristics::Gamma22curve:
    case PlatformVideoTransferCharacteristics::Gamma28curve:
        return kCVImageBufferTransferFunction_UseGamma;
    case PlatformVideoTransferCharacteristics::Linear:
        return PAL::kCMFormatDescriptionTransferFunction_Linear;
    default:
        return nullptr;
    }
}

CFStringRef convertToCMYCbCRMatrix(PlatformVideoMatrixCoefficients coefficients)
{
    switch (coefficients) {
    case PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance:
        return PAL::kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;
    case PlatformVideoMatrixCoefficients::Bt470bg:
    case PlatformVideoMatrixCoefficients::Smpte170m:
        return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    case PlatformVideoMatrixCoefficients::Bt709:
        return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
    case PlatformVideoMatrixCoefficients::Smpte240m:
        return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
    default:
        return nullptr;
    }
}

static CFStringRef convertToCMChromaLocation(PlatformVideoChromaLocation chromaLocation)
{
    switch (chromaLocation) {
    case PlatformVideoChromaLocation::Left:
        return kCVImageBufferChromaLocation_Left;
    case PlatformVideoChromaLocation::Center:
        return kCVImageBufferChromaLocation_Center;
    case PlatformVideoChromaLocation::TopLeft:
        return kCVImageBufferChromaLocation_TopLeft;
    case PlatformVideoChromaLocation::Top:
        return kCVImageBufferChromaLocation_Top;
    case PlatformVideoChromaLocation::BottomLeft:
        return kCVImageBufferChromaLocation_BottomLeft;
    case PlatformVideoChromaLocation::Bottom:
        return kCVImageBufferChromaLocation_Bottom;
    case PlatformVideoChromaLocation::Dv420:
        return kCVImageBufferChromaLocation_DV420;
    default:
        return nullptr;
    }
}

static CFStringRef convertToCMFieldDetail(PlatformVideoFieldDetail fieldDetail)
{
    switch (fieldDetail) {
    case PlatformVideoFieldDetail::TemporalTopFirst:
        return kCVImageBufferFieldDetailTemporalTopFirst;
    case PlatformVideoFieldDetail::TemporalBottomFirst:
        return kCVImageBufferFieldDetailTemporalBottomFirst;
    case PlatformVideoFieldDetail::SpatialFirstLineEarly:
        return kCVImageBufferFieldDetailSpatialFirstLineEarly;
    case PlatformVideoFieldDetail::SpatialFirstLineLate:
        return kCVImageBufferFieldDetailSpatialFirstLineLate;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static std::optional<OSType> LEGACY_NODELETE pixelFormatTypeForDecodedVideo(const VideoInfo& info)
{
    if (!info.pixelFormat())
        return { };

    // The 8-bit planar formats are repacked into a biplanar buffer when the CVPixelBuffer is created.
    switch (*info.pixelFormat()) {
    case VideoPixelFormat::I420:
        return info.colorSpace().fullRange.value_or(false) ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    default:
        return { };
    }
}

// Calls add() with every property that can be expressed both as a CMFormatDescription extension and
// as a CVImageBuffer attachment.
template<typename Add>
static void addImageBufferProperties(const VideoInfo& videoInfo, Add&& add)
{
    if (videoInfo.colorSpace().primaries) {
        if (RetainPtr cmColorPrimaries = convertToCMColorPrimaries(*videoInfo.colorSpace().primaries))
            add(kCVImageBufferColorPrimariesKey, cmColorPrimaries.get());
    }
    if (videoInfo.colorSpace().transfer) {
        if (RetainPtr cmTransferFunction = convertToCMTransferFunction(*videoInfo.colorSpace().transfer))
            add(kCVImageBufferTransferFunctionKey, cmTransferFunction.get());
        if (*videoInfo.colorSpace().transfer == PlatformVideoTransferCharacteristics::Gamma22curve)
            add(kCVImageBufferGammaLevelKey, (__bridge CFTypeRef)@(2.2));
        else if (*videoInfo.colorSpace().transfer == PlatformVideoTransferCharacteristics::Gamma28curve)
            add(kCVImageBufferGammaLevelKey, (__bridge CFTypeRef)@(2.8));
    }

    if (videoInfo.colorSpace().matrix) {
        if (RetainPtr cmMatrix = convertToCMYCbCRMatrix(*videoInfo.colorSpace().matrix))
            add(kCVImageBufferYCbCrMatrixKey, cmMatrix.get());
    }

    if (videoInfo.colorSpace().chromaLocation) {
        if (RetainPtr cmChromaLocation = convertToCMChromaLocation(*videoInfo.colorSpace().chromaLocation)) {
            add(kCVImageBufferChromaLocationTopFieldKey, cmChromaLocation.get());
            add(kCVImageBufferChromaLocationBottomFieldKey, cmChromaLocation.get());
        }
    }

    if (videoInfo.fieldCount())
        add(kCVImageBufferFieldCountKey, (__bridge CFTypeRef)@(std::to_underlying(*videoInfo.fieldCount())));

    if (videoInfo.fieldDetail()) {
        if (RetainPtr cmFieldDetail = convertToCMFieldDetail(*videoInfo.fieldDetail()))
            add(kCVImageBufferFieldDetailKey, cmFieldDetail.get());
    }

    if (!videoInfo.size().isEmpty() && !videoInfo.displaySize().isEmpty() && videoInfo.size() != videoInfo.displaySize()) {
        // The two spacings are the numerator and denominator of a single fraction, as stored in
        // the `pasp` box. Emit them as an exact integer pair rather than as the two quotients
        // displaySize / size, which only approximate the ratio.
        auto horizontalSpacing = std::llround(videoInfo.displaySize().width() * videoInfo.size().height());
        auto verticalSpacing = std::llround(videoInfo.displaySize().height() * videoInfo.size().width());
        if (horizontalSpacing > 0 && verticalSpacing > 0) {
            auto divisor = std::gcd(horizontalSpacing, verticalSpacing);
            horizontalSpacing /= divisor;
            verticalSpacing /= divisor;
            add(kCVImageBufferPixelAspectRatioKey, @{
                (__bridge NSString*)kCVImageBufferPixelAspectRatioHorizontalSpacingKey : @(horizontalSpacing),
                (__bridge NSString*)kCVImageBufferPixelAspectRatioVerticalSpacingKey : @(verticalSpacing)
            });
        }
    }
}

// FIXME: only 32-bit floating point is recognised for 32-bit PCM (the only producer is the WebM parser), and PCM is assumed little-endian.
static std::optional<AudioStreamDescription::PCMFormat> encodedPCMFormat(const AudioInfo& info)
{
    switch (info.bitDepth()) {
    case 8:
        return AudioStreamDescription::Uint8;
    case 16:
        return AudioStreamDescription::Int16;
    case 24:
        return AudioStreamDescription::Int24;
    case 32:
        return AudioStreamDescription::Float32;
    default:
        return { };
    }
}

RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromTrackInfo(const TrackInfo& info)
{
    ASSERT(info.isVideo() || info.isAudio());

    if (RefPtr audioInfo = dynamicDowncast<AudioInfo>(info)) {
        switch (audioInfo->codecName().value) {
#if ENABLE(OPUS)
        case kAudioFormatOpus:
            if (!isOpusDecoderAvailable() || (!audioInfo->cookieData() || !audioInfo->cookieData()->size()))
                return nullptr;
            return createAudioFormatDescription(*audioInfo);
#endif
#if ENABLE(VORBIS)
        case kAudioFormatVorbis:
            if (!isVorbisDecoderAvailable() || (!audioInfo->cookieData() || !audioInfo->cookieData()->size()))
                return nullptr;
            return createAudioFormatDescription(*audioInfo);
#endif
        case kAudioFormatFLAC:
            if (!audioInfo->cookieData() || !audioInfo->cookieData()->size())
                return nullptr;
            return createAudioFormatDescription(*audioInfo);
        case kAudioFormatLinearPCM: {
            std::optional<AudioStreamDescription::PCMFormat> format;
            if (audioInfo->isDecoded()) {
                if (audioInfo->sampleFormat() != AudioSampleFormat::F32)
                    return nullptr;
                format = AudioStreamDescription::Float32;
            } else
                format = encodedPCMFormat(*audioInfo);
            if (!format)
                return nullptr;

            auto absd = CAAudioStreamDescription { static_cast<double>(audioInfo->rate()), audioInfo->channels(), *format, CAAudioStreamDescription::IsInterleaved::Yes }.streamDescription();

            AudioChannelLayoutWithSize channelLayout;
            if (auto layout = audioInfo->channelLayout()) {
                channelLayout = channelLayoutFromMediaChannelLayout(*layout, audioInfo->channels());
                if (!channelLayout.first) {
                    RELEASE_LOG_ERROR(MediaStream, "createFormatDescriptionFromTrackInfo: invalid channel layout");
                    return nullptr;
                }
            }

            RetainPtr extensions = createExtensionsDictionary(info);
            CMFormatDescriptionRef newFormat = nullptr;
            if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &absd, channelLayout.second, channelLayout.first.get(), 0, nullptr, extensions.get(), &newFormat)) {
                RELEASE_LOG_ERROR(MediaStream, "createFormatDescriptionFromTrackInfo: CMAudioFormatDescriptionCreate failed with error %d", (int)error);
                return nullptr;
            }
            return adoptCF(newFormat);
        }
        default:
            return createAudioFormatDescription(*audioInfo);
        }
    }

    auto& videoInfo = downcast<const VideoInfo>(info);
    RetainPtr extensions = createExtensionsDictionary(info);

    OSType codecType = videoInfo.codecName().value;
    if (videoInfo.isDecoded()) {
        auto pixelFormatType = pixelFormatTypeForDecodedVideo(videoInfo);
        if (!pixelFormatType)
            return nullptr;
        codecType = *pixelFormatType;
    }

    if (videoInfo.colorSpace().fullRange.value_or(false))
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_FullRangeVideo, kCFBooleanTrue);

    int bitDepth = videoInfo.bitDepth();
    RetainPtr bitsPerComponent = adoptCF(CFNumberCreate(nullptr, kCFNumberIntType, &bitDepth));
    CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_BitsPerComponent, bitsPerComponent.get());

    addImageBufferProperties(videoInfo, [&](CFStringRef key, CFTypeRef value) {
        CFDictionaryAddValue(extensions.get(), key, value);
    });

#if PLATFORM(VISION)
    if (videoInfo.immersiveVideoMetadata()) {
        if (RetainPtr dictionary = formatDescriptionDictionaryFromImmersiveVideoMetadata(*videoInfo.immersiveVideoMetadata())) {
            CFDictionaryApplyFunction(dictionary.get(), [](CFTypeRef key, CFTypeRef value, void* context) {
                CFMutableDictionaryRef dict = static_cast<CFMutableDictionaryRef>(context);
                CFDictionarySetValue(dict, key, value);
            }, extensions.get());
        }
    }
#endif

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto error = PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, codecType, videoInfo.size().width(), videoInfo.size().height(), extensions.get(), &formatDescription);
    if (error != noErr) {
        RELEASE_LOG_ERROR(Media, "CMVideoFormatDescriptionCreate failed with error %d (%.4s)", (int)error, (char*)&error);
        return nullptr;
    }

    return adoptCF(formatDescription);
}

RefPtr<AudioInfo> createAudioInfoFromFormatDescription(CMFormatDescriptionRef description)
{
    // This method currently only works for compressed content.
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(description);
    if (mediaType != kCMMediaType_Audio)
        return nullptr;
    const AudioStreamBasicDescription* asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(description);
    ASSERT(asbd);
    if (!asbd)
        return nullptr;
    size_t cookieSize = 0;
    const void* cookie = PAL::CMAudioFormatDescriptionGetMagicCookie(description, &cookieSize);
    RefPtr cookieData = cookieSize ? RefPtr { SharedBuffer::create(unsafeMakeSpan(static_cast<const uint8_t*>(cookie), cookieSize)) } : nullptr;

    return AudioInfo::create({
        {
            .codecName = asbd->mFormatID,
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
            .encryptionData = getEncryptionDataCollection(description)
#endif
        }, {
            .rate = static_cast<uint32_t>(asbd->mSampleRate),
            .channels = asbd->mChannelsPerFrame,
            .framesPerPacket = asbd->mFramesPerPacket,
            .bitDepth = static_cast<uint8_t>(asbd->mBitsPerChannel),
            .cookieData = WTF::move(cookieData),
        }
    });
}

RefPtr<VideoInfo> createVideoInfoFromFormatDescription(CMFormatDescriptionRef description, std::optional<uint8_t> reorderQueueMaxSize)
{
    // This method currently only works for compressed content.
    auto mediaType = PAL::CMFormatDescriptionGetMediaType(description);
    if (mediaType != kCMMediaType_Video)
        return nullptr;

    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(description);

    int bitDepth = 8;
    if (RetainPtr bitsPerComponent = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_BitsPerComponent)))
        CFNumberGetValue(bitsPerComponent.get(), kCFNumberIntType, &bitDepth);

    RetainPtr cmExtensionAtoms = PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    Vector<TrackInfo::AtomData> extensionAtoms;
    if (RetainPtr atomDictionary = dynamic_cf_cast<CFDictionaryRef>(cmExtensionAtoms.get())) {
        CFIndex extensionCount = CFDictionaryGetCount(atomDictionary.get());
        if (!extensionCount)
            RELEASE_LOG_INFO(Media, "kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms having %ld keys keys expected at least 1", extensionCount);
        else
            extensionAtoms = parseExtensionAtomsDictionary(atomDictionary.get());
    } else
        RELEASE_LOG_ERROR(Media, "Couldn't retrieve extensionAtoms from CMFormatDescription");

    return VideoInfo::create({
        {
            .codecName = PAL::CMFormatDescriptionGetMediaSubType(description),
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
            .encryptionData = getEncryptionDataCollection(description)
#endif
        }, {
            .size = IntSize { dimensions.width, dimensions.height },
            .displaySize = presentationSizeFromFormatDescription(description),
            .bitDepth = static_cast<uint8_t>(bitDepth),
            .colorSpace = colorSpaceFromFormatDescription(description).value_or(PlatformVideoColorSpace { }),
            .fieldCount = fieldCountFromFormatDescription(description),
            .fieldDetail = fieldDetailFromFormatDescription(description),
            .extensionAtoms = WTF::move(extensionAtoms),
            .reorderQueueMaxSize = reorderQueueMaxSize,
#if PLATFORM(VISION)
            .immersiveVideoMetadata = immersiveVideoMetadataFromFormatDescription(description)
#endif
        }
    });
}

static bool isInterleavedFloat32Format(CMFormatDescriptionRef format, const AudioInfo& info)
{
    const AudioStreamBasicDescription* description = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(format);
    return description
        && description->mFormatID == kAudioFormatLinearPCM
        && (description->mFormatFlags & kAudioFormatFlagIsFloat)
        && !(description->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
        && description->mBitsPerChannel == 32
        && description->mChannelsPerFrame == info.channels()
        && description->mSampleRate == static_cast<double>(info.rate());
}

// Decoded audio is interleaved F32, so the format is constant bitrate: the sample count is a frame count
// and the timing entry holds the duration of one frame.
static std::expected<RetainPtr<CMSampleBufferRef>, ASCIILiteral> decodedAudioToCMSampleBuffer(const MediaSamplesBlock& samples, const AudioInfo& info, CMFormatDescriptionRef formatDescription)
{
    if (samples.isEmpty())
        return makeUnexpected("No decoded audio sample"_s);

    if (info.sampleFormat() != AudioSampleFormat::F32)
        return makeUnexpected("Unsupported decoded audio sample format"_s);

    size_t bytesPerFrame = info.channels() * sizeof(float);
    if (!bytesPerFrame || !info.rate())
        return makeUnexpected("Invalid decoded audio format"_s);

    RetainPtr format = formatDescription ? retainPtr(formatDescription) : createFormatDescriptionFromTrackInfo(info);
    if (!format)
        return makeUnexpected("No CMFormatDescription available"_s);
    if (!isInterleavedFloat32Format(format.get(), info))
        return makeUnexpected("CMFormatDescription doesn't match the decoded audio format"_s);

    RetainPtr<CMBlockBufferRef> completeBlockBuffers;
    if (samples.size() > 1) {
        CMBlockBufferRef rawBlockBuffer = nullptr;
        auto err = PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, samples.size(), 0, &rawBlockBuffer);
        if (err != kCMBlockBufferNoErr || !rawBlockBuffer)
            return makeUnexpected("CMBlockBufferCreateEmpty failed"_s);
        completeBlockBuffers = adoptCF(rawBlockBuffer);
    }

    size_t totalBytes = 0;
    for (auto& sample : samples) {
        RefPtr sampleData = sample.data;
        if (!sampleData || !sampleData->size() || sampleData->size() % bytesPerFrame)
            return makeUnexpected("Invalid decoded audio data"_s);

        auto blockBuffer = sampleData->createCMBlockBuffer();
        if (!blockBuffer)
            return makeUnexpected("Couldn't create CMBlockBuffer"_s);

        if (!completeBlockBuffers)
            completeBlockBuffers = WTF::move(blockBuffer);
        else {
            auto err = PAL::CMBlockBufferAppendBufferReference(completeBlockBuffers.get(), blockBuffer.get(), 0, 0, 0);
            if (err != kCMBlockBufferNoErr)
                return makeUnexpected("CMBlockBufferAppendBufferReference failed"_s);
        }
        totalBytes += sampleData->size();
    }

    // The items of a block are contiguous, so the first presentation time and a per-frame duration describe all of them.
    CMSampleTimingInfo timing { PAL::CMTimeMake(1, info.rate()), PAL::toCMTime(samples.first().presentationTime), PAL::kCMTimeInvalid };
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, completeBlockBuffers.get(), format.get(), totalBytes / bytesPerFrame, 1, &timing, 0, nullptr, &rawSampleBuffer))
        return makeUnexpected("CMSampleBufferCreateReady failed: OOM"_s);
    RetainPtr sampleBuffer = adoptCF(rawSampleBuffer);

    if (samples.discontinuity())
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, *samples.discontinuity() ? kCFBooleanTrue : kCFBooleanFalse, kCMAttachmentMode_ShouldPropagate);

    return sampleBuffer;
}

static bool copyI420ToBiplanarPixelBuffer(std::span<const uint8_t> source, CVPixelBufferRef pixelBuffer, size_t width, size_t height)
{
    size_t chromaWidth = (width + 1) / 2;
    size_t chromaHeight = (height + 1) / 2;
    size_t lumaSize = width * height;
    size_t chromaSize = chromaWidth * chromaHeight;
    if (source.size() != lumaSize + 2 * chromaSize)
        return false;

    if (CVPixelBufferGetPlaneCount(pixelBuffer) != 2
        || CVPixelBufferGetWidthOfPlane(pixelBuffer, 0) != width
        || CVPixelBufferGetHeightOfPlane(pixelBuffer, 0) != height
        || CVPixelBufferGetWidthOfPlane(pixelBuffer, 1) != chromaWidth
        || CVPixelBufferGetHeightOfPlane(pixelBuffer, 1) != chromaHeight)
        return false;

    if (CVPixelBufferLockBaseAddress(pixelBuffer, 0) != kCVReturnSuccess)
        return false;
    auto unlock = makeScopeExit([pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    });

    auto lumaSource = source.first(lumaSize);
    auto uSource = source.subspan(lumaSize, chromaSize);
    auto vSource = source.subspan(lumaSize + chromaSize, chromaSize);

    auto lumaDestination = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    size_t lumaStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    auto chromaDestination = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
    size_t chromaStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
    if (lumaStride < width || chromaStride < 2 * chromaWidth || lumaDestination.size() < lumaStride * height || chromaDestination.size() < chromaStride * chromaHeight)
        return false;

    for (size_t y = 0; y < height; ++y)
        memcpySpan(lumaDestination.subspan(y * lumaStride, width), lumaSource.subspan(y * width, width));

    for (size_t y = 0; y < chromaHeight; ++y) {
        auto destination = chromaDestination.subspan(y * chromaStride, 2 * chromaWidth);
        auto u = uSource.subspan(y * chromaWidth, chromaWidth);
        auto v = vSource.subspan(y * chromaWidth, chromaWidth);
        for (size_t x = 0; x < chromaWidth; ++x) {
            destination[2 * x] = u[x];
            destination[2 * x + 1] = v[x];
        }
    }
    return true;
}

// A CMSampleBuffer holds a single image buffer, so the block must hold exactly one frame.
// The format description is always derived from the image buffer.
static std::expected<RetainPtr<CMSampleBufferRef>, ASCIILiteral> decodedVideoToCMSampleBuffer(const MediaSamplesBlock& samples, const VideoInfo& info)
{
    if (samples.size() != 1)
        return makeUnexpected("A decoded video block must hold exactly one frame"_s);

    auto pixelFormatType = pixelFormatTypeForDecodedVideo(info);
    if (!pixelFormatType)
        return makeUnexpected("Unsupported decoded video pixel format"_s);

    auto& sample = samples.first();
    RefPtr sampleData = sample.data;
    if (!sampleData)
        return makeUnexpected("No decoded video data"_s);

    int width = info.size().width();
    int height = info.size().height();
    if (width <= 0 || height <= 0)
        return makeUnexpected("Invalid decoded video size"_s);

    RetainPtr attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr surfaceProperties = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(attributes.get(), kCVPixelBufferIOSurfacePropertiesKey, surfaceProperties.get());

    CVPixelBufferRef rawPixelBuffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, *pixelFormatType, attributes.get(), &rawPixelBuffer) != kCVReturnSuccess || !rawPixelBuffer)
        return makeUnexpected("CVPixelBufferCreate failed"_s);
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    Ref contiguousData = sampleData->makeContiguous();
    if (!copyI420ToBiplanarPixelBuffer(contiguousData->span(), pixelBuffer.get(), width, height))
        return makeUnexpected("Invalid decoded video data"_s);

    addImageBufferProperties(info, [&](CFStringRef key, CFTypeRef value) {
        CVBufferSetAttachment(pixelBuffer.get(), key, value, kCVAttachmentMode_ShouldPropagate);
    });

    CMVideoFormatDescriptionRef rawFormatDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer.get(), &rawFormatDescription) != noErr || !rawFormatDescription)
        return makeUnexpected("CMVideoFormatDescriptionCreateForImageBuffer failed"_s);
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMSampleTimingInfo timing { PAL::toCMTime(sample.duration), PAL::toCMTime(sample.presentationTime), PAL::toCMTime(sample.decodeTime) };
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, pixelBuffer.get(), formatDescription.get(), &timing, &rawSampleBuffer) != noErr || !rawSampleBuffer)
        return makeUnexpected("CMSampleBufferCreateReadyWithImageBuffer failed"_s);
    RetainPtr sampleBuffer = adoptCF(rawSampleBuffer);

    if (sample.flags & MediaSample::SampleFlags::IsNonDisplaying) {
        if (RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true)) {
            RetainPtr attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), 0));
            CFDictionarySetValue(attachments.get(), PAL::kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);
        }
    }

    return sampleBuffer;
}

std::expected<RetainPtr<CMSampleBufferRef>, ASCIILiteral> toCMSampleBuffer(const MediaSamplesBlock& samples, CMFormatDescriptionRef formatDescription)
{
    if (!samples.info())
        return makeUnexpected("No TrackInfo found"_s);

    if (RefPtr audioInfo = dynamicDowncast<AudioInfo>(samples.info()); audioInfo && audioInfo->isDecoded())
        return decodedAudioToCMSampleBuffer(samples, *audioInfo, formatDescription);
    if (RefPtr videoInfo = dynamicDowncast<VideoInfo>(samples.info()); videoInfo && videoInfo->isDecoded())
        return decodedVideoToCMSampleBuffer(samples, *videoInfo);

    RetainPtr format = formatDescription ? retainPtr(formatDescription) : createFormatDescriptionFromTrackInfo(*protect(samples.info()));
    if (!format)
        return makeUnexpected("No CMFormatDescription available"_s);

    RetainPtr<CMBlockBufferRef> completeBlockBuffers;
    if (samples.size() > 1) {
        // Optimisation so that we allocate the entire CMBlockBuffer at once if we have more than one to return.
        CMBlockBufferRef rawBlockBuffer = nullptr;
        auto err = PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, samples.size(), 0, &rawBlockBuffer);
        if (err != kCMBlockBufferNoErr || !rawBlockBuffer)
            return makeUnexpected("CMBlockBufferCreateEmpty failed"_s);
        completeBlockBuffers = adoptCF(rawBlockBuffer);
    }

    Vector<CMSampleTimingInfo> packetTimings;
    packetTimings.reserveInitialCapacity(samples.size());
    Vector<size_t> packetSizes;
    packetSizes.reserveInitialCapacity(samples.size());
    auto cumulativeTrimDurationAtStart = MediaTime::zeroTime();
    auto cumulativeTrimDurationAtEnd = MediaTime::zeroTime();
    for (auto& sample : samples) {
        RefPtr sampleData = sample.data;
        auto blockBuffer = sampleData->createCMBlockBuffer();
        if (!blockBuffer)
            return makeUnexpected("Couldn't create CMBlockBuffer"_s);

        if (!completeBlockBuffers)
            completeBlockBuffers = WTF::move(blockBuffer);
        else {
            auto err = PAL::CMBlockBufferAppendBufferReference(completeBlockBuffers.get(), blockBuffer.get(), 0, 0, 0);
            if (err != kCMBlockBufferNoErr)
                return makeUnexpected("CMBlockBufferAppendBufferReference failed"_s);
        }
        auto [trimAtStart, trimAtEnd] = sample.trimInterval;
        if (!trimAtStart.isFinite() || !trimAtEnd.isFinite() || trimAtStart < MediaTime::zeroTime() || trimAtEnd < MediaTime::zeroTime() || sample.duration < MediaTime::zeroTime())
            return makeUnexpected("Invalid trim interval"_s);
        packetTimings.append({ PAL::toCMTime(sample.duration + trimAtStart + trimAtEnd), PAL::toCMTime(sample.presentationTime - trimAtStart), PAL::toCMTime(sample.decodeTime - trimAtStart) });
        packetSizes.append(sampleData->size());
        cumulativeTrimDurationAtStart += trimAtStart;
        cumulativeTrimDurationAtEnd += trimAtEnd;
    }

    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, completeBlockBuffers.get(), format.get(), packetSizes.size(), packetTimings.size(), packetTimings.span().data(), packetSizes.size(), packetSizes.span().data(), &rawSampleBuffer))
        return makeUnexpected("CMSampleBufferCreateReady failed: OOM"_s);

    if (samples.isVideo() && samples.size()) {
        RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
        ASSERT(attachmentsArray);
        if (!attachmentsArray)
            return makeUnexpected("No sample attachment found"_s);
        ASSERT(size_t(CFArrayGetCount(attachmentsArray)) == samples.size());
        for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
            RetainPtr attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
            if (!(samples[i].flags & MediaSample::SampleFlags::IsSync))
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);

            if (samples[i].flags & MediaSample::SampleFlags::IsNonDisplaying)
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);

            // Attach HDR10+ (aka SMPTE ST 2094-40) metadata, if present:
            if (samples[i].hdrMetadataType == PlatformMediaCapabilitiesHdrMetadataType::SmpteSt209440 && samples[i].hdrMetadata)
                CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_HDR10PlusPerFrameData, Ref { *samples[i].hdrMetadata }->createCFData().get());
        }
    } else if (samples.isAudio() && samples.discontinuity())
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, *samples.discontinuity() ? kCFBooleanTrue : kCFBooleanFalse, kCMAttachmentMode_ShouldPropagate);

    auto setTrimDurationAttachment = [&](CFStringRef key, const MediaTime& trimDuration) {
        if (trimDuration <= MediaTime::zeroTime())
            return;
        RetainPtr trimDurationDict = adoptCF(PAL::CMTimeCopyAsDictionary(PAL::toCMTime(trimDuration), kCFAllocatorDefault));
        PAL::CMSetAttachment(rawSampleBuffer, key, trimDurationDict.get(), kCMAttachmentMode_ShouldPropagate);
    };
    setTrimDurationAttachment(PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart, cumulativeTrimDurationAtStart);
    setTrimDurationAttachment(PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd, cumulativeTrimDurationAtEnd);

#if ENABLE(ENCRYPTED_MEDIA)
    if (!samples.info() || !samples.info()->encryptionDataCollection())
        return adoptCF(rawSampleBuffer);

    RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
    ASSERT(attachmentsArray);
    if (!attachmentsArray)
        return makeUnexpected("No sample attachment found"_s);
    if (static_cast<size_t>(CFArrayGetCount(attachmentsArray.get())) < samples.size()) {
        RELEASE_LOG_DEBUG(Media, "Encrypted sample doesn't contain sufficient attachments: %u (expected:%u)", static_cast<unsigned>(CFArrayGetCount(attachmentsArray.get())), static_cast<unsigned>(samples.size()));
        return adoptCF(rawSampleBuffer);
    }

    for (size_t index = 0; index < samples.size(); index++) {
        RetainPtr attachmentsDictionary = dynamic_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), index));
        ASSERT(attachmentsDictionary);
        if (!attachmentsDictionary)
            continue;

        auto& sample = samples[index];
        RetainPtr value = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &sample.bytesOfClearDataCount));
        CFDictionarySetValue(attachmentsDictionary.get(), CFSTR("BytesOfClearDataCount") /* PAL::kCMSampleAttachmentKey_BytesOfClearDataCount */, value.get());
        if (RefPtr cryptorIV = sample.cryptorIV)
            CFDictionarySetValue(attachmentsDictionary.get(), CFSTR("CryptorIV") /* PAL::kCMSampleAttachmentKey_CryptorInitializationVector */, cryptorIV->createCFData().get());
        if (RefPtr cryptorSubsampleAuxiliaryData = sample.cryptorSubsampleAuxiliaryData)
            CFDictionarySetValue(attachmentsDictionary.get(), PAL::kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData, cryptorSubsampleAuxiliaryData->createCFData().get());
    }
#endif
    return adoptCF(rawSampleBuffer);
}

UniqueRef<MediaSamplesBlock> samplesBlockFromCMSampleBuffer(CMSampleBufferRef cmSample, const TrackInfo* trackInfo)
{
    ASSERT(cmSample);
    RefPtr info = trackInfo;
    if (!trackInfo) {
        if (RetainPtr description = PAL::CMSampleBufferGetFormatDescription(cmSample)) {
            if (PAL::CMFormatDescriptionGetMediaType(description.get()) == kCMMediaType_Audio)
                info = createAudioInfoFromFormatDescription(description.get());
            else {
                ASSERT(PAL::CMFormatDescriptionGetMediaType(description.get()) == kCMMediaType_Video);
                info = createVideoInfoFromFormatDescription(description.get());
            }
        }
    }

    auto trimDurationAttachment = [&](CFStringRef key) {
        if (RetainPtr trimDurationDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMGetAttachment(cmSample, key, nullptr)))
            return PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimDurationDict.get()));
        return MediaTime::zeroTime();
    };
    auto trimDurationAtStart = trimDurationAttachment(PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart);
    auto trimDurationAtEnd = trimDurationAttachment(PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd);

    // Non-zero when the output presentation timestamp was set explicitly.
    auto outputTimeOffset = MediaTime::zeroTime();
    if (auto outputPresentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(cmSample); CMTIME_IS_VALID(outputPresentationTime))
        outputTimeOffset = PAL::toMediaTime(outputPresentationTime) - PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(cmSample)) - trimDurationAtStart;

    // The item's times are those of the trimmed output, as MediaSample reports them.
    auto mediaSampleItemForSample = [outputTimeOffset](auto&& sample, const MediaTime& trimAtStart, const MediaTime& trimAtEnd) {
        RetainPtr sampleBuffer = sample->sampleBuffer();
        RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
#if ENABLE(ENCRYPTED_MEDIA)
        SInt32 bytesOfClearDataCount = 0;
        RefPtr<SharedBuffer> cryptorIV;
        RefPtr<SharedBuffer> cryptorSubsampleAuxiliaryData;

        RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
        if (attachmentsArray && CFArrayGetCount(attachmentsArray.get()) > 0) {
            if (RetainPtr attachmentsDictionary = dynamic_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), 0))) {
                if (RetainPtr number = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(attachmentsDictionary.get(), CFSTR("BytesOfClearDataCount") /* PAL::kCMSampleAttachmentKey_BytesOfClearDataCount */)))
                    CFNumberGetValue(number.get(), kCFNumberSInt32Type, &bytesOfClearDataCount);
                if (RetainPtr data = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(attachmentsDictionary.get(), CFSTR("CryptorIV") /* PAL::kCMSampleAttachmentKey_CryptorInitializationVector */)))
                    cryptorIV = SharedBuffer::create(data.get());
                if (RetainPtr data = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(attachmentsDictionary.get(), PAL::kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData)))
                    cryptorSubsampleAuxiliaryData = SharedBuffer::create(data.get());
            }
        }
#endif
        auto presentationTime = PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer.get());
        auto decodeTime = PAL::CMSampleBufferGetDecodeTimeStamp(sampleBuffer.get());
        if (CMTIME_IS_INVALID(decodeTime))
            decodeTime = presentationTime;
        auto outputTime = [&](const CMTime& time) {
            return PAL::toMediaTime(time) + trimAtStart + outputTimeOffset;
        };
        return MediaSamplesBlock::MediaSampleItem {
            .presentationTime = outputTime(presentationTime),
            .decodeTime = outputTime(decodeTime),
            .duration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(sampleBuffer.get())) - trimAtStart - trimAtEnd,
            .trimInterval = { trimAtStart, trimAtEnd },
            .data = sharedBufferFromCMBlockBuffer(blockBuffer.get()),
            .flags = sample->flags(),
#if ENABLE(ENCRYPTED_MEDIA)
            .bytesOfClearDataCount = bytesOfClearDataCount,
            .cryptorIV = WTF::move(cryptorIV),
            .cryptorSubsampleAuxiliaryData = WTF::move(cryptorSubsampleAuxiliaryData),
#endif
        };
    };

    if (info && info->codecName() == kAudioFormatLinearPCM) {
        MediaSamplesBlock::SamplesVector sample;
        sample.reserveInitialCapacity(1);
        sample.append(mediaSampleItemForSample(MediaSampleAVFObjC::create(cmSample, info->trackID()), trimDurationAtStart, trimDurationAtEnd));
        return makeUniqueRef<MediaSamplesBlock>(info.get(), WTF::move(sample));
    }

    // Dividing drops the trim attachments, so the leading samples take the start trim and the trailing ones the end trim.
    auto subSamples = MediaSampleAVFObjC::create(cmSample, info ? info->trackID() : 0)->divide();
    Vector<std::pair<MediaTime, MediaTime>> trims;
    if (trimDurationAtStart || trimDurationAtEnd)
        trims.grow(subSamples.size());
    auto fullDuration = [&](size_t index) {
        return PAL::toMediaTime(PAL::CMSampleBufferGetDuration(protect(subSamples[index]->sampleBuffer())));
    };
    for (size_t index = 0; index < subSamples.size() && trimDurationAtStart > MediaTime::zeroTime(); ++index) {
        trims[index].first = std::min(trimDurationAtStart, fullDuration(index));
        trimDurationAtStart -= trims[index].first;
    }
    for (size_t index = subSamples.size(); index-- && trimDurationAtEnd > MediaTime::zeroTime();) {
        trims[index].second = std::min(trimDurationAtEnd, fullDuration(index) - trims[index].first);
        trimDurationAtEnd -= trims[index].second;
    }
    MediaSamplesBlock::SamplesVector samples(subSamples.size(), [&](auto index) {
        if (trims.isEmpty())
            return mediaSampleItemForSample(subSamples[index], MediaTime::zeroTime(), MediaTime::zeroTime());
        return mediaSampleItemForSample(subSamples[index], trims[index].first, trims[index].second);
    });
    return makeUniqueRef<MediaSamplesBlock>(info.get(), WTF::move(samples));
}

void attachColorSpaceToPixelBuffer(const PlatformVideoColorSpace& colorSpace, CVPixelBufferRef pixelBuffer)
{
    ASSERT(pixelBuffer);
    if (!pixelBuffer)
        return;

    CVBufferRemoveAttachment(pixelBuffer, kCVImageBufferCGColorSpaceKey);
    if (colorSpace.primaries)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, protect(convertToCMColorPrimaries(*colorSpace.primaries)), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.transfer) {
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey, protect(convertToCMTransferFunction(*colorSpace.transfer)), kCVAttachmentMode_ShouldPropagate);
        if (*colorSpace.transfer == PlatformVideoTransferCharacteristics::Gamma22curve)
            CVBufferSetAttachment(pixelBuffer, kCVImageBufferGammaLevelKey, (__bridge CFTypeRef)@(2.2), kCVAttachmentMode_ShouldPropagate);
        else if (*colorSpace.transfer == PlatformVideoTransferCharacteristics::Gamma28curve)
            CVBufferSetAttachment(pixelBuffer, kCVImageBufferGammaLevelKey, (__bridge CFTypeRef)@(2.8), kCVAttachmentMode_ShouldPropagate);
    }
    if (colorSpace.matrix)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, protect(convertToCMYCbCRMatrix(*colorSpace.matrix)), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.chromaLocation) {
        if (RetainPtr cmChromaLocation = convertToCMChromaLocation(*colorSpace.chromaLocation)) {
            CVBufferSetAttachment(pixelBuffer, kCVImageBufferChromaLocationTopFieldKey, cmChromaLocation.get(), kCVAttachmentMode_ShouldPropagate);
            CVBufferSetAttachment(pixelBuffer, kCVImageBufferChromaLocationBottomFieldKey, cmChromaLocation.get(), kCVAttachmentMode_ShouldPropagate);
        }
    }
}

PlatformVideoColorSpace computeVideoFrameColorSpace(CVPixelBufferRef pixelBuffer)
{
    ASSERT(pixelBuffer);
    if (!pixelBuffer)
        return { };

    std::optional<PlatformVideoColorPrimaries> primaries;
    auto pixelPrimaries = CVBufferGetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, nil);
    if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_ITU_R_709_2))
        primaries = PlatformVideoColorPrimaries::Bt709;
    else if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_EBU_3213))
        primaries = PlatformVideoColorPrimaries::JedecP22Phosphors;
    else if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_P22))
        primaries = PlatformVideoColorPrimaries::JedecP22Phosphors;
    else if (safeCFEqual(pixelPrimaries, kCVImageBufferColorPrimaries_SMPTE_C))
        primaries = PlatformVideoColorPrimaries::Smpte170m;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_DCI_P3))
        primaries = PlatformVideoColorPrimaries::SmpteRp431;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_P3_D65))
        primaries = PlatformVideoColorPrimaries::SmpteEg432;
    else if (safeCFEqual(pixelPrimaries, PAL::kCMFormatDescriptionColorPrimaries_ITU_R_2020))
        primaries = PlatformVideoColorPrimaries::Bt2020;

    std::optional<PlatformVideoTransferCharacteristics> transfer;
    auto pixelTransfer = CVBufferGetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey, nil);
    if (safeCFEqual(pixelTransfer, kCVImageBufferTransferFunction_ITU_R_709_2))
        transfer = PlatformVideoTransferCharacteristics::Bt709;
    else if (safeCFEqual(pixelTransfer, kCVImageBufferTransferFunction_SMPTE_240M_1995))
        transfer = PlatformVideoTransferCharacteristics::Smpte240m;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ))
        transfer = PlatformVideoTransferCharacteristics::SmpteSt2084;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1))
        transfer = PlatformVideoTransferCharacteristics::SmpteSt4281;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG))
        transfer = PlatformVideoTransferCharacteristics::AribStdB67Hlg;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_Linear))
        transfer = PlatformVideoTransferCharacteristics::Linear;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_sRGB))
        transfer = PlatformVideoTransferCharacteristics::Iec6196621;
    else if (safeCFEqual(pixelTransfer, PAL::kCMFormatDescriptionTransferFunction_ITU_R_2020))
        transfer = PlatformVideoTransferCharacteristics::Bt2020_10bit;
    else if (safeCFEqual(pixelTransfer, kCVImageBufferTransferFunction_UseGamma)) {
        if (auto gammaLevel = CVBufferGetAttachment(pixelBuffer, kCVImageBufferGammaLevelKey, nil)) {
            double gamma = 0;
            CFNumberGetValue(static_cast<CFNumberRef>(gammaLevel), kCFNumberFloat64Type, &gamma);
            transfer = gamma < 2.5 ? PlatformVideoTransferCharacteristics::Gamma22curve : PlatformVideoTransferCharacteristics::Gamma28curve;
        }
    }

    std::optional<PlatformVideoMatrixCoefficients> matrix;
    auto pixelMatrix = CVBufferGetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, nil);
    if (safeCFEqual(pixelMatrix, PAL::kCMFormatDescriptionYCbCrMatrix_ITU_R_2020))
        matrix = PlatformVideoMatrixCoefficients::Bt2020NonconstantLuminance;
    else if (safeCFEqual(pixelMatrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
        matrix = PlatformVideoMatrixCoefficients::Bt709;
    else if (safeCFEqual(pixelMatrix, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995))
        matrix = PlatformVideoMatrixCoefficients::Smpte240m;
    else if (safeCFEqual(pixelMatrix, kCVImageBufferYCbCrMatrix_ITU_R_601_4))
        matrix = PlatformVideoMatrixCoefficients::Bt470bg;

    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    // FIXME: We should do a more comprehensive check.
    bool isFullRange = pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange && pixelFormat != kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;

    std::optional<PlatformVideoChromaLocation> chromaLocation;
    RetainPtr pixelChromaLocation = CVBufferGetAttachment(pixelBuffer, kCVImageBufferChromaLocationTopFieldKey, nil);
    if (!pixelChromaLocation)
        pixelChromaLocation = CVBufferGetAttachment(pixelBuffer, kCVImageBufferChromaLocationBottomFieldKey, nil);
    if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_Left))
        chromaLocation = PlatformVideoChromaLocation::Left;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_Center))
        chromaLocation = PlatformVideoChromaLocation::Center;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_TopLeft))
        chromaLocation = PlatformVideoChromaLocation::TopLeft;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_Top))
        chromaLocation = PlatformVideoChromaLocation::Top;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_BottomLeft))
        chromaLocation = PlatformVideoChromaLocation::BottomLeft;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_Bottom))
        chromaLocation = PlatformVideoChromaLocation::Bottom;
    else if (safeCFEqual(pixelChromaLocation.get(), kCVImageBufferChromaLocation_DV420))
        chromaLocation = PlatformVideoChromaLocation::Dv420;

    return { .primaries = primaries, .transfer = transfer, .matrix = matrix, .fullRange = isFullRange, .chromaLocation = chromaLocation };
}

PacketDurationParser::PacketDurationParser(const AudioInfo& info)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName().value;
    UInt32 size = sizeof(asbd);
    RefPtr cookieData = info.cookieData();
    auto cookieDataSpan = cookieData->span();
    auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, cookieDataSpan.size(), cookieDataSpan.data(), &size, &asbd);
    if (error || !info.rate()) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescription failed with error %d (%.4s)", (int)error, (char*)&error);
        return;
    }
    m_audioFormatID = asbd.mFormatID;
    m_sampleRate = info.rate();
    m_constantFramesPerPacket = asbd.mFramesPerPacket;
#if HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    switch (m_audioFormatID) {
#if ENABLE(VORBIS)
    case kAudioFormatVorbis: {
        AudioFormatInfo formatInfo = { asbd, cookieDataSpan.data(), (UInt32)cookieDataSpan.size() };
        UInt32 propertySize = sizeof(AudioFormatVorbisModeInfo);
        m_vorbisModeInfo = std::make_unique<AudioFormatVorbisModeInfo>();
        if (PAL::AudioFormatGetProperty(kAudioFormatProperty_VorbisModeInfo, sizeof(formatInfo), &formatInfo, &propertySize, m_vorbisModeInfo.get()) != noErr || !m_vorbisModeInfo->mModeCount) {
            m_vorbisModeInfo.reset();
            // No mode info or invalid mode info.
            return;
        }

        auto ilog = [] (uint32_t v) {
            int ret = 0;
            while (v) {
                ret++;
                v >>= 1;
            }
            return ret;
        };

        uint32_t modeBitCount = ilog(m_vorbisModeInfo->mModeCount - 1);
        for (uint32_t thisModeBit = 0; thisModeBit < modeBitCount; ++thisModeBit)
            m_vorbisModeMask |= 1 << thisModeBit;
        }
        break;
#endif
    default:
        // No need to examine the magic cookie.
        break;
    }
#endif
    m_isValid = true;
}

size_t PacketDurationParser::framesInPacket(std::span<const uint8_t> packet)
{
#if !HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    UNUSED_PARAM(packet);
    return m_constantFramesPerPacket;
#else
    if (m_constantFramesPerPacket)
        return m_constantFramesPerPacket;

    if (packet.empty())
        return 0;

    switch (m_audioFormatID) {
#if ENABLE(OPUS)
    case kAudioFormatOpus: {
        OpusCookieContents cookie;
        if (!parseOpusTOCData(packet, cookie))
            return 0;
        return cookie.framesPerPacket * (cookie.frameDuration.seconds() * m_sampleRate);
        }
#endif
#if ENABLE(VORBIS)
    case kAudioFormatVorbis: {
        // The following calculation corresponds to the duration of the "finished audiodata"
        // produced by the decoder from the current packet in its position within
        // the stream, as documented by Xiph in the Vorbis I specification.
        // It also corresponds to the delta in granule position of the packet within
        // the same sequence of packets in an Ogg file, with the possible exception of
        // the ultimate packet, which may be assigned a smaller delta for the purpose
        // of trimming.
        constexpr uint8_t kVorbisPacketTypeFlag = 0b00000001;

        auto leadingByte = packet[0];
        if (leadingByte & kVorbisPacketTypeFlag)
            return 0; // Not an audio packet.

        uint32_t modeIndex = (leadingByte >> 1) & m_vorbisModeMask;
        if (modeIndex >= m_vorbisModeInfo->mModeCount)
            return 0; // Invalid mode.

        uint32_t blockSize = 0;
        if (!(m_vorbisModeInfo->mModeFlags & (1ULL << modeIndex)))
            blockSize = m_vorbisModeInfo->mShortBlockSize;
        else
            blockSize = m_vorbisModeInfo->mLongBlockSize;
        // The first vorbis packet decoded doesn't output audible content, and should be undetermined.
        // However as content could be fed in any order, we must assume that previous content could be available at some stage.
        size_t framesOfOutput = (blockSize + m_lastVorbisBlockSize) / 4;
        m_lastVorbisBlockSize = blockSize;

        return framesOfOutput;
        }
#endif
    default:
        return m_constantFramesPerPacket;
    }
#endif
}

void PacketDurationParser::reset()
{
#if ENABLE(VORBIS)
    if (m_audioFormatID == kAudioFormatVorbis)
        m_lastVorbisBlockSize = 0;
#endif
}

PacketDurationParser::~PacketDurationParser() = default;

Vector<AudioStreamPacketDescription> getPacketDescriptions(CMSampleBufferRef sampleBuffer)
{
    size_t packetDescriptionsSize;
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, 0, nullptr, &packetDescriptionsSize) != noErr) {
        RELEASE_LOG_FAULT(Media, "Unable to get packet description list size");
        return { };
    }
    size_t numDescriptions = packetDescriptionsSize / sizeof(AudioStreamPacketDescription);
    if (!numDescriptions) {
        RELEASE_LOG_DEBUG(Media, "No packet description found.");
        return { };
    }
    Vector<AudioStreamPacketDescription> descriptions(numDescriptions);
    if (PAL::CMSampleBufferGetAudioStreamPacketDescriptions(sampleBuffer, packetDescriptionsSize, descriptions.mutableSpan().data(), nullptr) != noErr) {
        RELEASE_LOG_FAULT(Media, "Unable to get packet description list");
        return { };
    }
    auto numPackets = PAL::CMSampleBufferGetNumSamples(sampleBuffer);
    if (numDescriptions != size_t(numPackets)) {
        RELEASE_LOG_FAULT(Media, "Unhandled CMSampleBuffer structure");
        return { };
    }
    return descriptions;
}

Vector<CMSampleTimingInfo> getSampleTimingInfoArray(CMSampleBufferRef sampleBuffer)
{
    CMItemCount entriesNeeded = 0;
    if (PAL::CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, 0, nullptr, &entriesNeeded) != noErr || !entriesNeeded)
        return { };
    Vector<CMSampleTimingInfo> timings(entriesNeeded);
    if (PAL::CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, entriesNeeded, timings.mutableSpan().data(), nullptr) != noErr)
        return { };
    return timings;
}

static bool sampleBufferHasNoSampleSizes(CMSampleBufferRef sampleBuffer)
{
    return PAL::CMSampleBufferGetSampleSizeArray(sampleBuffer, 0, nullptr, nullptr) == kCMSampleBufferError_BufferHasNoSampleSizes;
}

// Synthesize a single-packet CMSampleBuffer that references a slice of a source
// block buffer. Used when the source CMSampleBuffer carries per-packet sizes in
// the audio stream packet description array (kCMSampleBufferError_BufferHasNoSampleSizes
// shape) and CoreMedia APIs that walk the sample-size array cannot be used.
static RetainPtr<CMSampleBufferRef> createSinglePacketSampleBuffer(CMBlockBufferRef sourceBlockBuffer, CMFormatDescriptionRef formatDescription, const AudioStreamPacketDescription& desc, const CMSampleTimingInfo& timing)
{
    if (desc.mStartOffset < 0)
        return nullptr;

    CMBlockBufferRef rawSubBlock = nullptr;
    if (PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, 1, 0, &rawSubBlock) != kCMBlockBufferNoErr || !rawSubBlock)
        return nullptr;
    RetainPtr subBlock = adoptCF(rawSubBlock);
    if (PAL::CMBlockBufferAppendBufferReference(subBlock.get(), sourceBlockBuffer, static_cast<size_t>(desc.mStartOffset), desc.mDataByteSize, 0) != kCMBlockBufferNoErr)
        return nullptr;

    size_t sampleSize = desc.mDataByteSize;
    CMSampleBufferRef rawSample = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, subBlock.get(), formatDescription, 1, 1, &timing, 1, &sampleSize, &rawSample) != noErr || !rawSample)
        return nullptr;
    return adoptCF(rawSample);
}

OSStatus forEachSample(CMSampleBufferRef sampleBuffer, NOESCAPE const Function<OSStatus(CMSampleBufferRef, CMItemCount)>& callback)
{
    if (!sampleBufferHasNoSampleSizes(sampleBuffer)) {
        return PAL::CMSampleBufferCallBlockForEachSample(sampleBuffer, [&](CMSampleBufferRef subSample, CMItemCount index) -> OSStatus {
            return callback(subSample, index);
        });
    }

    auto numSamples = PAL::CMSampleBufferGetNumSamples(sampleBuffer);
    auto packetDescs = getPacketDescriptions(sampleBuffer);
    if (packetDescs.size() != static_cast<size_t>(numSamples))
        return kCMSampleBufferError_CannotSubdivide;

    auto timings = getSampleTimingInfoArray(sampleBuffer);
    if (timings.isEmpty())
        return kCMSampleBufferError_CannotSubdivide;

    const bool uniformTiming = timings.size() == 1;
    const CMSampleTimingInfo& baseTiming = timings[0];
    CMTime runningPTS = baseTiming.presentationTimeStamp;
    CMTime runningDTS = baseTiming.decodeTimeStamp;

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
    RetainPtr sourceBlockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!formatDescription || !sourceBlockBuffer)
        return kCMSampleBufferError_CannotSubdivide;

    for (CMItemCount i = 0; i < numSamples; ++i) {
        CMSampleTimingInfo timing = uniformTiming
            ? CMSampleTimingInfo { baseTiming.duration, runningPTS, runningDTS }
            : timings[i];
        if (uniformTiming && CMTIME_IS_VALID(baseTiming.duration)) {
            if (CMTIME_IS_VALID(runningPTS))
                runningPTS = PAL::CMTimeAdd(runningPTS, baseTiming.duration);
            if (CMTIME_IS_VALID(runningDTS))
                runningDTS = PAL::CMTimeAdd(runningDTS, baseTiming.duration);
        }

        RetainPtr subSample = createSinglePacketSampleBuffer(sourceBlockBuffer.get(), formatDescription.get(), packetDescs[i], timing);
        if (!subSample)
            return kCMSampleBufferError_CannotSubdivide;

        if (auto status = callback(subSample.get(), i))
            return status;
    }
    return noErr;
}

RetainPtr<CMSampleBufferRef> copySampleBufferForRange(CMSampleBufferRef sampleBuffer, CFRange range)
{
    if (!sampleBufferHasNoSampleSizes(sampleBuffer)) {
        CMSampleBufferRef rawCopy = nullptr;
        if (PAL::CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, sampleBuffer, range, &rawCopy) != noErr || !rawCopy)
            return nullptr;
        return adoptCF(rawCopy);
    }

    auto numSamples = PAL::CMSampleBufferGetNumSamples(sampleBuffer);
    if (range.location < 0 || range.length <= 0 || range.location + range.length > numSamples)
        return nullptr;

    auto packetDescs = getPacketDescriptions(sampleBuffer);
    if (packetDescs.size() != static_cast<size_t>(numSamples))
        return nullptr;

    auto timings = getSampleTimingInfoArray(sampleBuffer);
    if (timings.isEmpty())
        return nullptr;

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
    RetainPtr sourceBlockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!formatDescription || !sourceBlockBuffer)
        return nullptr;

    CMBlockBufferRef rawSubBlock = nullptr;
    if (PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, range.length, 0, &rawSubBlock) != kCMBlockBufferNoErr || !rawSubBlock)
        return nullptr;
    RetainPtr subBlock = adoptCF(rawSubBlock);

    Vector<size_t> sampleSizes;
    sampleSizes.reserveInitialCapacity(static_cast<size_t>(range.length));
    for (CFIndex i = 0; i < range.length; ++i) {
        const auto& desc = packetDescs[range.location + i];
        if (desc.mStartOffset < 0)
            return nullptr;
        if (PAL::CMBlockBufferAppendBufferReference(subBlock.get(), sourceBlockBuffer.get(), static_cast<size_t>(desc.mStartOffset), desc.mDataByteSize, 0) != kCMBlockBufferNoErr)
            return nullptr;
        sampleSizes.append(desc.mDataByteSize);
    }

    Vector<CMSampleTimingInfo> rangeTimings;
    CMItemCount timingEntries;
    const CMSampleTimingInfo* timingArray = nullptr;
    CMSampleTimingInfo uniformTimingForRange;
    if (timings.size() == 1) {
        const CMSampleTimingInfo& baseTiming = timings[0];
        uniformTimingForRange = baseTiming;
        if (CMTIME_IS_VALID(baseTiming.duration)) {
            for (CFIndex i = 0; i < range.location; ++i) {
                if (CMTIME_IS_VALID(uniformTimingForRange.presentationTimeStamp))
                    uniformTimingForRange.presentationTimeStamp = PAL::CMTimeAdd(uniformTimingForRange.presentationTimeStamp, baseTiming.duration);
                if (CMTIME_IS_VALID(uniformTimingForRange.decodeTimeStamp))
                    uniformTimingForRange.decodeTimeStamp = PAL::CMTimeAdd(uniformTimingForRange.decodeTimeStamp, baseTiming.duration);
            }
        }
        timingArray = &uniformTimingForRange;
        timingEntries = 1;
    } else {
        rangeTimings = timings.subspan(range.location, range.length);
        timingArray = rangeTimings.span().data();
        timingEntries = rangeTimings.size();
    }

    CMSampleBufferRef rawSample = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, subBlock.get(), formatDescription.get(), range.length, timingEntries, timingArray, sampleSizes.size(), sampleSizes.span().data(), &rawSample) != noErr || !rawSample)
        return nullptr;
    return adoptCF(rawSample);
}

RetainPtr<CMBlockBufferRef> ensureContiguousBlockBuffer(CMBlockBufferRef rawBlockBuffer)
{
    if (PAL::CMBlockBufferIsRangeContiguous(rawBlockBuffer, 0, 0))
        return rawBlockBuffer;
    // This is an out-parameter which is immediately adopted below.
    SUPPRESS_UNRETAINED_LOCAL CMBlockBufferRef contiguousBuffer;
    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBlockBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer)) {
        RELEASE_LOG_FAULT(Media, "Failed to create contiguous blockBuffer with error:%d", static_cast<int>(status));
        return nullptr;
    }
    return adoptCF(contiguousBuffer);
}

Vector<uint8_t> convertParameterSetsCMSampleBufferToAnnexB(CMSampleBufferRef sampleBuffer, bool isKeyframe, CMVideoFormatDescriptionGetParameterSetAtIndexFunction getParameterSetAtIndex)
{
    static constexpr uint8_t annexBHeaderBytes[] = { 0, 0, 0, 1 };
    static constexpr size_t avccHeaderByteSize = sizeof(uint32_t);

    Vector<uint8_t> annexBBuffer;

    RetainPtr description = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!description) {
        RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB no description");
        return annexBBuffer;
    }

    int naluHeaderSize = 0;
    size_t paramSetCount = 0;
    if (getParameterSetAtIndex(description, 0, nullptr, nullptr, &paramSetCount, &naluHeaderSize) != noErr)
        return annexBBuffer;
    if (naluHeaderSize != avccHeaderByteSize) {
        RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB unexpected nalu header size");
        return annexBBuffer;
    }

    if (isKeyframe) {
        for (size_t i = 0; i < paramSetCount; ++i) {
            const uint8_t* paramSet = nullptr;
            size_t paramSetSize = 0;
            if (getParameterSetAtIndex(description, i, &paramSet, &paramSetSize, nullptr, nullptr) != noErr || !paramSet)
                return { };
            annexBBuffer.append(std::span { annexBHeaderBytes });
            annexBBuffer.append(unsafeMakeSpan(paramSet, paramSetSize));
        }
    }

    RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB no block buffer");
        return { };
    }

    RetainPtr contiguousBuffer = ensureContiguousBlockBuffer(blockBuffer);
    if (!contiguousBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB unable to create a contiguous block buffer");
        return { };
    }

    auto dataSpan = [&contiguousBuffer] -> std::optional<std::span<const uint8_t>> {
        char* dataPtr = nullptr;
        size_t blockBufferSize = PAL::CMBlockBufferGetDataLength(contiguousBuffer.get());
        if (PAL::CMBlockBufferGetDataPointer(contiguousBuffer.get(), 0, nullptr, nullptr, &dataPtr) != noErr)
            return { };
        return unsafeMakeSpan(byteCast<uint8_t>(dataPtr), blockBufferSize);
    }();
    if (!dataSpan) {
        RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB unable to get block buffer data");
        return { };
    }

    auto data = *dataSpan;
    while (data.size() > 0) {
        if (data.size() < avccHeaderByteSize) {
            RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB missing data");
            return { };
        }
        uint32_t packetSize;
        memcpySpan(asMutableByteSpan(packetSize), data.first(avccHeaderByteSize));
        packetSize = flipBytes(packetSize);
        size_t bytesWritten = packetSize + avccHeaderByteSize;
        if (bytesWritten > data.size()) {
            RELEASE_LOG_ERROR(WebRTC, "convertParameterSetsCMSampleBufferToAnnexB missing data");
            return { };
        }
        annexBBuffer.append(std::span { annexBHeaderBytes });
        annexBBuffer.append(data.subspan(avccHeaderByteSize, packetSize));
        data = data.subspan(bytesWritten);
    }

    return annexBBuffer;
}

Ref<SharedBuffer> sharedBufferFromCMBlockBuffer(CMBlockBufferRef blockBuffer)
{
    return SharedBuffer::create(DataSegment::Provider {
        [blockBuffer = ensureContiguousBlockBuffer(blockBuffer)]() -> std::span<const uint8_t> {
            if (!blockBuffer)
                return { };
            return PAL::CMBlockBufferGetDataSpan(blockBuffer.get());
        }
    });
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
Vector<Ref<SharedBuffer>> getKeyIDs(CMFormatDescriptionRef description)
{
    if (!description)
        return { };
    if (RetainPtr trackEncryptionData = static_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("CommonEncryptionTrackEncryptionBox")))) {
        // AVStreamDataParser will attach the 'tenc' box to each sample, not including the leading
        // size and boxType data. Extract the 'tenc' box and use that box to derive the sample's
        // keyID.
        ISOTrackEncryptionBox trackEncryptionBox;
        if (!trackEncryptionBox.parseWithoutTypeAndSize(span(trackEncryptionData.get())))
            return { };
        return { SharedBuffer::create(trackEncryptionBox.defaultKID()) };
    }

#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    if (static_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("TransportStreamEncryptionInitData")))) {
        // AVStreamDataParser will attach a JSON transport stream encryption
        // description object to each sample. Use a static keyID in this case
        // as MPEG2-TS encryption does not specify a particular keyID in the
        // stream.
        return CDMPrivateFairPlayStreaming::mptsKeyIDs();
    }
#endif

    return { };
}
#endif

FourCC computeBoxType(FourCC codecType)
{
    switch (codecType.value) {
    case kCMVideoCodecType_VP9:
    case 'vp08':
        return 'vpcC';
    case kCMVideoCodecType_H264:
        return 'avcC';
    case kCMVideoCodecType_HEVC:
        return 'hvcC';
    case kCMVideoCodecType_AV1:
        return 'av1C';
    default:
        ASSERT_NOT_REACHED();
        return 'baad';
    }
}

AudioChannelLayoutWithSize channelLayoutFromChannelLayoutTag(UInt32 channelLayoutTag)
{
    if (channelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions || channelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
        ASSERT_NOT_REACHED();
        return { nullptr, 0 };
    }

    auto channelLayout = PAL::createAudioChannelLayout(1, PAL::ShouldZeroMemory::Yes);
    channelLayout->mChannelLayoutTag = channelLayoutTag;
    auto channelLayoutSize = PAL::allocationSize(*channelLayout);
    return { WTF::move(channelLayout), channelLayoutSize };
}

AudioChannelLayoutWithSize channelLayoutFromChannelLabels(std::span<const AudioChannelLabel> channelLabels)
{
    auto channelLayout = PAL::createAudioChannelLayout(static_cast<uint32_t>(channelLabels.size()), PAL::ShouldZeroMemory::Yes);
    channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    auto descriptions = unsafeMakeSpan(channelLayout->mChannelDescriptions, channelLayout->mNumberChannelDescriptions);
    for (size_t index = 0; index < channelLabels.size(); ++index)
        descriptions[index].mChannelLabel = channelLabels[index];
    auto channelLayoutSize = PAL::allocationSize(*channelLayout);
    return { WTF::move(channelLayout), channelLayoutSize };
}

static std::optional<WebAudioChannel> webAudioChannel(AudioChannelLabel label, bool surroundsAreSides)
{
    switch (label) {
    case kAudioChannelLabel_Left:
        return WebAudioChannel::FrontLeft;
    case kAudioChannelLabel_Right:
        return WebAudioChannel::FrontRight;
    case kAudioChannelLabel_Center:
    case kAudioChannelLabel_Mono:
        return WebAudioChannel::FrontCenter;
    case kAudioChannelLabel_LFEScreen:
        return WebAudioChannel::LowFrequency;
    case kAudioChannelLabel_LeftSurround:
        return surroundsAreSides ? WebAudioChannel::SideLeft : WebAudioChannel::BackLeft;
    case kAudioChannelLabel_RightSurround:
        return surroundsAreSides ? WebAudioChannel::SideRight : WebAudioChannel::BackRight;
    case kAudioChannelLabel_RearSurroundLeft:
    case kAudioChannelLabel_LeftBackSurround:
        return WebAudioChannel::BackLeft;
    case kAudioChannelLabel_RearSurroundRight:
    case kAudioChannelLabel_RightBackSurround:
        return WebAudioChannel::BackRight;
    case kAudioChannelLabel_LeftCenter:
        return WebAudioChannel::FrontLeftOfCenter;
    case kAudioChannelLabel_RightCenter:
        return WebAudioChannel::FrontRightOfCenter;
    case kAudioChannelLabel_CenterSurround:
        return WebAudioChannel::BackCenter;
    case kAudioChannelLabel_LeftSurroundDirect:
    case kAudioChannelLabel_LeftSideSurround:
        return WebAudioChannel::SideLeft;
    case kAudioChannelLabel_RightSurroundDirect:
    case kAudioChannelLabel_RightSideSurround:
        return WebAudioChannel::SideRight;
    case kAudioChannelLabel_TopCenterSurround:
        return WebAudioChannel::TopCenter;
    case kAudioChannelLabel_VerticalHeightLeft:
        return WebAudioChannel::TopFrontLeft;
    case kAudioChannelLabel_VerticalHeightCenter:
        return WebAudioChannel::TopFrontCenter;
    case kAudioChannelLabel_VerticalHeightRight:
        return WebAudioChannel::TopFrontRight;
    case kAudioChannelLabel_TopBackLeft:
    case kAudioChannelLabel_LeftTopRear:
        return WebAudioChannel::TopBackLeft;
    case kAudioChannelLabel_TopBackCenter:
    case kAudioChannelLabel_CenterTopRear:
        return WebAudioChannel::TopBackCenter;
    case kAudioChannelLabel_TopBackRight:
    case kAudioChannelLabel_RightTopRear:
        return WebAudioChannel::TopBackRight;
    default:
        return { };
    }
}

std::optional<Vector<WebAudioChannel>> webAudioChannelsFromChannelDescriptions(std::span<const AudioChannelDescription> descriptions)
{
    // Surround channels are at the back, unless the layout also has back or back center channels; they are then at the side.
    bool surroundsAreSides = std::ranges::any_of(descriptions, [](auto& description) {
        switch (description.mChannelLabel) {
        case kAudioChannelLabel_RearSurroundLeft:
        case kAudioChannelLabel_RearSurroundRight:
        case kAudioChannelLabel_LeftBackSurround:
        case kAudioChannelLabel_RightBackSurround:
        case kAudioChannelLabel_CenterSurround:
            return true;
        default:
            return false;
        }
    });

    Vector<WebAudioChannel> channels;
    channels.reserveInitialCapacity(descriptions.size());
    for (auto& description : descriptions) {
        auto channel = webAudioChannel(description.mChannelLabel, surroundsAreSides);
        if (!channel)
            return { };
        channels.append(*channel);
    }
    return channels;
}

// The inverse of webAudioChannelsFromChannelDescriptions(): surround channels are labelled as such when they are
// the only channels at the back or at the side; otherwise the back channels are rear surround channels.
static Vector<AudioChannelLabel> channelLabelsFromWebAudioChannelMask(uint32_t mask)
{
    auto has = [&](WebAudioChannel channel) {
        return !!(mask & webAudioChannelBit(channel));
    };
    bool hasSides = has(WebAudioChannel::SideLeft) || has(WebAudioChannel::SideRight);
    bool hasBacks = has(WebAudioChannel::BackLeft) || has(WebAudioChannel::BackRight) || has(WebAudioChannel::BackCenter);

    Vector<AudioChannelLabel> labels;
    for (uint8_t index = 0; index < numberOfWebAudioChannels; ++index) {
        auto channel = static_cast<WebAudioChannel>(index);
        if (!has(channel))
            continue;
        labels.append([&] -> AudioChannelLabel {
            switch (channel) {
            case WebAudioChannel::FrontLeft:
                return kAudioChannelLabel_Left;
            case WebAudioChannel::FrontRight:
                return kAudioChannelLabel_Right;
            case WebAudioChannel::FrontCenter:
                return kAudioChannelLabel_Center;
            case WebAudioChannel::LowFrequency:
                return kAudioChannelLabel_LFEScreen;
            case WebAudioChannel::BackLeft:
                return hasSides || has(WebAudioChannel::BackCenter) ? kAudioChannelLabel_RearSurroundLeft : kAudioChannelLabel_LeftSurround;
            case WebAudioChannel::BackRight:
                return hasSides || has(WebAudioChannel::BackCenter) ? kAudioChannelLabel_RearSurroundRight : kAudioChannelLabel_RightSurround;
            case WebAudioChannel::FrontLeftOfCenter:
                return kAudioChannelLabel_LeftCenter;
            case WebAudioChannel::FrontRightOfCenter:
                return kAudioChannelLabel_RightCenter;
            case WebAudioChannel::BackCenter:
                return kAudioChannelLabel_CenterSurround;
            case WebAudioChannel::SideLeft:
                return hasBacks ? kAudioChannelLabel_LeftSurround : kAudioChannelLabel_LeftSideSurround;
            case WebAudioChannel::SideRight:
                return hasBacks ? kAudioChannelLabel_RightSurround : kAudioChannelLabel_RightSideSurround;
            case WebAudioChannel::TopCenter:
                return kAudioChannelLabel_TopCenterSurround;
            case WebAudioChannel::TopFrontLeft:
                return kAudioChannelLabel_VerticalHeightLeft;
            case WebAudioChannel::TopFrontCenter:
                return kAudioChannelLabel_VerticalHeightCenter;
            case WebAudioChannel::TopFrontRight:
                return kAudioChannelLabel_VerticalHeightRight;
            case WebAudioChannel::TopBackLeft:
                return kAudioChannelLabel_TopBackLeft;
            case WebAudioChannel::TopBackCenter:
                return kAudioChannelLabel_TopBackCenter;
            case WebAudioChannel::TopBackRight:
                return kAudioChannelLabel_TopBackRight;
            }
            ASSERT_NOT_REACHED();
            return kAudioChannelLabel_Unknown;
        }());
    }
    return labels;
}

AudioChannelLayoutWithSize channelLayoutFromMediaChannelLayout(const MediaChannelLayout& layout, uint32_t numberOfChannels)
{
    if (!layout.isValid(numberOfChannels) || numberOfChannels > std::numeric_limits<uint16_t>::max())
        return { nullptr, 0 };

    // The labels are the ones of CoreAudio's own layouts, so that the speakers are the same as with a layout tag.
    if (layout.kind == MediaChannelLayout::Kind::Positional)
        return channelLayoutFromChannelLabels(channelLabelsFromWebAudioChannelMask(layout.mask).span());

    if (layout.kind == MediaChannelLayout::Kind::Ambisonic && layout.mask) {
        // A tag can't describe non-diegetic channels, so the channels are described one by one.
        uint32_t components = numberOfChannels - std::popcount(layout.mask);
        Vector<AudioChannelLabel> labels(components, [](size_t index) -> AudioChannelLabel {
            return kAudioChannelLabel_HOA_ACN_0 + static_cast<AudioChannelLabel>(index);
        });
        labels.append(kAudioChannelLabel_Left);
        labels.append(kAudioChannelLabel_Right);
        return channelLayoutFromChannelLabels(labels.span());
    }

    auto channelLayout = PAL::createAudioChannelLayout(0, PAL::ShouldZeroMemory::Yes);
    switch (layout.kind) {
    case MediaChannelLayout::Kind::Positional:
        ASSERT_NOT_REACHED();
        return { nullptr, 0 };
    case MediaChannelLayout::Kind::Discrete:
        // A single channel without a position is mono.
        channelLayout->mChannelLayoutTag = numberOfChannels == 1 ? kAudioChannelLayoutTag_Mono : kAudioChannelLayoutTag_DiscreteInOrder | numberOfChannels;
        break;
    case MediaChannelLayout::Kind::Ambisonic:
        channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_HOA_ACN_SN3D | numberOfChannels;
        break;
    }
    auto channelLayoutSize = PAL::allocationSize(*channelLayout);
    return { WTF::move(channelLayout), channelLayoutSize };
}

std::optional<MediaChannelLayout> mediaChannelLayoutFromChannelLayout(const AudioChannelLayout& channelLayout)
{
    if (channelLayout.mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
        uint32_t mask = channelLayout.mChannelBitmap;
        if (!mask || mask >= (1U << numberOfWebAudioChannels))
            return { };
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Positional, .mask = mask };
    }
    if (channelLayout.mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions) {
        // Ambisonic components HOA_ACN_0 to HOA_ACN_n, optionally followed by a non-diegetic stereo pair.
        auto descriptions = PAL::span(channelLayout);
        size_t components = 0;
        while (components < descriptions.size() && descriptions[components].mChannelLabel == kAudioChannelLabel_HOA_ACN_0 + components)
            ++components;
        if (components) {
            if (components == descriptions.size())
                return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Ambisonic };
            if (descriptions.size() == components + 2 && descriptions[components].mChannelLabel == kAudioChannelLabel_Left && descriptions[components + 1].mChannelLabel == kAudioChannelLabel_Right)
                return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Ambisonic, .mask = MediaChannelLayout::nonDiegeticStereoMask };
            return { };
        }

        // A single channel without a position is mono.
        if (descriptions.size() == 1 && descriptions[0].mChannelLabel == kAudioChannelLabel_Mono)
            return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };

        // Channels at distinct Web Audio positions, in Web Audio order.
        auto channels = webAudioChannelsFromChannelDescriptions(descriptions);
        if (!channels || channels->isEmpty() || !std::ranges::is_sorted(*channels))
            return { };
        uint32_t mask = 0;
        for (auto channel : *channels)
            mask |= webAudioChannelBit(channel);
        if (static_cast<size_t>(std::popcount(mask)) != channels->size())
            return { };
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Positional, .mask = mask };
    }
    switch (channelLayout.mChannelLayoutTag & 0xFFFF0000) {
    case kAudioChannelLayoutTag_Mono & 0xFFFF0000:
    case kAudioChannelLayoutTag_DiscreteInOrder:
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };
    case kAudioChannelLayoutTag_HOA_ACN_SN3D:
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Ambisonic };
    default:
        return { };
    }
}

String channelLayoutDescription(UInt32 channelLayoutTag)
{
    auto channelLayout = channelLayoutFromChannelLayoutTag(channelLayoutTag);
    if (!channelLayout.first)
        return { };
    // This is an out-parameter which is immediately adopted below.
    SUPPRESS_UNRETAINED_LOCAL CFStringRef channelLayoutName;
    UInt32 stringSize = sizeof(channelLayoutName);
    if (PAL::AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, channelLayout.second, channelLayout.first.get(), &stringSize, &channelLayoutName))
        return { };
    SUPPRESS_RETAINPTR_CTOR_ADOPT return adoptCF(channelLayoutName).get(); // The caller is responsible for releasing the returned string.
}

RetainPtr<CMSampleBufferRef> sampleBufferFromVideoData(std::span<const uint8_t> buffer, CMVideoFormatDescriptionRef videoFormat)
{
    // This is an out-parameter which is immediately adopted below.
    SUPPRESS_UNRETAINED_LOCAL CMBlockBufferRef newVlockBuffer;
    if (auto error = PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer.size(), kCFAllocatorDefault, NULL, 0, buffer.size(), kCMBlockBufferAssureMemoryNowFlag, &newVlockBuffer)) {
        RELEASE_LOG_ERROR(Media, "CMBlockBufferCreateWithMemoryBlock failed with: %d", (int)error);
        return nullptr;
    }
    auto blockBuffer = adoptCF(newVlockBuffer);

    if (auto error = PAL::CMBlockBufferReplaceDataBytes(buffer.data(), blockBuffer.get(), 0, buffer.size())) {
        RELEASE_LOG_ERROR(Media, "CMBlockBufferReplaceDataBytes failed with: %d", (int)error);
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer = nullptr;
    if (auto error = PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, videoFormat, 1, 0, nullptr, 0, nullptr, &sampleBuffer)) {
        RELEASE_LOG_ERROR(Media, "CMSampleBufferCreate failed with: %d", (int)error);
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

bool isCMSampleBufferRandomAccess(CMSampleBufferRef sample)
{
    RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments || CFArrayGetCount(attachments.get()) < 1)
        return true;
    RetainPtr firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments.get(), 0));
    const void* notSync = nullptr;
    if (!CFDictionaryGetValueIfPresent(firstAttachment.get(), PAL::kCMSampleAttachmentKey_NotSync, &notSync))
        return true;

    return !notSync || !CFBooleanGetValue(static_cast<CFBooleanRef>(notSync));
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
