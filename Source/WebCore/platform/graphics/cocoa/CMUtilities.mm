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
#import <JavaScriptCore/ArrayBuffer.h>
#import <JavaScriptCore/DataView.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/Expected.h>
#import <wtf/Scope.h>
#import <wtf/SharedTask.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PacketDurationParser);

#if ENABLE(VORBIS)
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

static RetainPtr<CFDictionaryRef> createExtensionAtomsDictionary(const Vector<std::pair<FourCC, Ref<SharedBuffer>>>& configurations)
{
    Vector<RetainPtr<CFTypeRef>> configurationCFStringKeys = { configurations.size(), [&](auto index) {
        return cfStringFromFourCC(configurations[index].first);
    } };
    Vector<RetainPtr<CFDataRef>> configurationValues = { configurations.size(), [&](auto index) {
        return configurations[index].second->createCFData();
    } };
    Vector<CFTypeRef> rawConfigurationKeys(configurationCFStringKeys.size(), [&](auto index) {
        return configurationCFStringKeys[index].get();
    });
    Vector<CFTypeRef> rawConfigurationValues(configurationValues.size(), [&](auto index) {
        return configurationValues[index].get();
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

    RetainPtr extensionAtoms = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(description, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
    if (!extensionAtoms) {
        return EncryptionDataCollection {
            .encryptionData = WTF::move(*encryptionData),
            .encryptionOriginalFormat = encryptionOriginalFormat
        };
    }

    // For video content, the first element of the dictionary is always the video's atomData.
    size_t indexStart = PAL::CMFormatDescriptionGetMediaType(description) == kCMMediaType_Video;
    size_t extensionsCount = CFDictionaryGetCount(extensionAtoms.get());
    if (extensionsCount <= indexStart) {
        return EncryptionDataCollection {
            .encryptionData = WTF::move(*encryptionData),
            .encryptionOriginalFormat = encryptionOriginalFormat
        };
    }

    Vector<const void*, 2> keys(extensionsCount);
    Vector<const void*, 2> values(extensionsCount);
    CFDictionaryGetKeysAndValues(extensionAtoms.get(), keys.mutableSpan().data(), values.mutableSpan().data());
    Vector<TrackInfoEncryptionInitData> encryptionInitDatas = { size_t(extensionsCount) - indexStart, [&](auto index) -> TrackInfoEncryptionInitData {
        return { cfStringToFourCC(static_cast<CFStringRef>(keys[index + indexStart])), SharedBuffer::create(static_cast<CFDataRef>(values[index + indexStart])) };
    } };

    return EncryptionDataCollection {
        .encryptionData = WTF::move(*encryptionData),
        .encryptionOriginalFormat = encryptionOriginalFormat,
        .encryptionInitDatas = WTF::move(encryptionInitDatas)
    };
}
#endif

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioInfo& info)
{
    auto streamDescription = audioStreamDescriptionFromAudioInfo(info);
    std::span<const uint8_t> cookie;
    RefPtr cookieData = info.cookieData();
    if (cookieData)
        cookie = cookieData->span();

    RetainPtr<CFDictionaryRef> extensions;
#if ENABLE(ENCRYPTED_MEDIA)
    if (info.encryptionDataCollection()) {
        RetainPtr dict = createExtensionAtomsDictionary(info.encryptionDataCollection()->encryptionInitDatas);
        CFTypeRef keys[] = { PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms };
        CFTypeRef values[] = { dict.get() };
        extensions = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }
#endif

    auto basicDescription = std::get<const AudioStreamBasicDescription*>(streamDescription.platformDescription().description);
    CMFormatDescriptionRef format = nullptr;
    auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, basicDescription, 0, nullptr, cookie.size(), cookie.data(), extensions.get(), &format);
    if (error) {
        LOG_ERROR("createAudioFormatDescription failed with %d", static_cast<int>(error));
        return nullptr;
    }
    return adoptCF(format);
}

static CFStringRef convertToCMColorPrimaries(PlatformVideoColorPrimaries primaries)
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

static CFStringRef convertToCMTransferFunction(PlatformVideoTransferCharacteristics characteristics)
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
        return PAL::canLoad_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() ? PAL::kCMFormatDescriptionTransferFunction_sRGB : nullptr;
    case PlatformVideoTransferCharacteristics::Linear:
        return PAL::kCMFormatDescriptionTransferFunction_Linear;
    default:
        return nullptr;
    }
}

static CFStringRef convertToCMYCbCRMatrix(PlatformVideoMatrixCoefficients coefficients)
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
        case kAudioFormatLinearPCM: {
            auto absd = CAAudioStreamDescription { static_cast<double>(audioInfo->rate()), audioInfo->channels(), AudioStreamDescription::Float32, CAAudioStreamDescription::IsInterleaved::Yes }.streamDescription();

            CMFormatDescriptionRef newFormat = nullptr;
            if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &absd, 0, nullptr, 0, nullptr, nullptr, &newFormat)) {
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

    size_t maxNumberOfElements = [] {
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        return 9;
#else
        return 5;
#endif
    }();
    RetainPtr extensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, maxNumberOfElements, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFIndex numKeys = videoInfo.extensionAtoms().size();
#if ENABLE(ENCRYPTED_MEDIA)
    if (videoInfo.encryptionDataCollection())
        numKeys += videoInfo.encryptionDataCollection()->encryptionInitDatas.size();
#endif
    Vector<std::pair<FourCC, Ref<SharedBuffer>>> configurations;
    configurations.reserveInitialCapacity(numKeys);
    configurations.appendVector(videoInfo.extensionAtoms());
#if ENABLE(ENCRYPTED_MEDIA)
    if (videoInfo.encryptionDataCollection())
        configurations.appendVector(videoInfo.encryptionDataCollection()->encryptionInitDatas);
#endif
    CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, createExtensionAtomsDictionary(configurations).get());

    if (videoInfo.colorSpace().fullRange.value_or(false))
        CFDictionaryAddValue(extensions.get(), PAL::kCMFormatDescriptionExtension_FullRangeVideo, kCFBooleanTrue);

    if (videoInfo.colorSpace().primaries) {
        if (RetainPtr cmColorPrimaries = convertToCMColorPrimaries(*videoInfo.colorSpace().primaries))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferColorPrimariesKey, cmColorPrimaries.get());
    }
    if (videoInfo.colorSpace().transfer) {
        if (RetainPtr cmTransferFunction = convertToCMTransferFunction(*videoInfo.colorSpace().transfer))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferTransferFunctionKey, cmTransferFunction.get());
    }

    if (videoInfo.colorSpace().matrix) {
        if (RetainPtr cmMatrix = convertToCMYCbCRMatrix(*videoInfo.colorSpace().matrix))
            CFDictionaryAddValue(extensions.get(), kCVImageBufferYCbCrMatrixKey, cmMatrix.get());
    }
    if (videoInfo.size() != videoInfo.displaySize()) {
        double horizontalRatio = videoInfo.displaySize().width() / videoInfo.size().width();
        double verticalRatio = videoInfo.displaySize().height() / videoInfo.size().height();
        CFDictionaryAddValue(extensions.get(), PAL::get_CoreMedia_kCMFormatDescriptionExtension_PixelAspectRatioSingleton(), @{
            (__bridge NSString*)PAL::get_CoreMedia_kCMFormatDescriptionKey_PixelAspectRatioHorizontalSpacingSingleton() : @(horizontalRatio),
            (__bridge NSString*)PAL::get_CoreMedia_kCMFormatDescriptionKey_PixelAspectRatioVerticalSpacingSingleton() : @(verticalRatio)
        });
    }

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (auto& encryptionCollection = videoInfo.encryptionDataCollection()) {
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
    }
#endif

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
    auto error = PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, videoInfo.codecName().value, videoInfo.size().width(), videoInfo.size().height(), extensions.get(), &formatDescription);
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

RefPtr<VideoInfo> createVideoInfoFromFormatDescription(CMFormatDescriptionRef description)
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
        else {
            Vector<const void*, 1> keys(extensionCount);
            Vector<const void*, 1> values(extensionCount);
            CFDictionaryGetKeysAndValues(atomDictionary.get(), keys.mutableSpan().data(), values.mutableSpan().data());
            extensionAtoms = { size_t(extensionCount), [&](auto index) -> TrackInfo::AtomData {
                return { cfStringToFourCC(checked_cf_cast<CFStringRef>(keys[index])), SharedBuffer::create(checked_cf_cast<CFDataRef>(values[index])) };
            } };
        }
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
            .extensionAtoms = WTF::move(extensionAtoms),
#if PLATFORM(VISION)
            .immersiveVideoMetadata = immersiveVideoMetadataFromFormatDescription(description)
#endif
        }
    });
}

Expected<RetainPtr<CMSampleBufferRef>, CString> toCMSampleBuffer(const MediaSamplesBlock& samples, CMFormatDescriptionRef formatDescription)
{
    if (!samples.info())
        return makeUnexpected("No TrackInfo found");

    RetainPtr format = formatDescription ? retainPtr(formatDescription) : createFormatDescriptionFromTrackInfo(*samples.protectedInfo());
    if (!format)
        return makeUnexpected("No CMFormatDescription available");

    RetainPtr<CMBlockBufferRef> completeBlockBuffers;
    if (samples.size() > 1) {
        // Optimisation so that we allocate the entire CMBlockBuffer at once if we have more than one to return.
        CMBlockBufferRef rawBlockBuffer = nullptr;
        auto err = PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, samples.size(), 0, &rawBlockBuffer);
        if (err != kCMBlockBufferNoErr || !rawBlockBuffer)
            return makeUnexpected("CMBlockBufferCreateEmpty failed");
        completeBlockBuffers = adoptCF(rawBlockBuffer);
    }

    Vector<CMSampleTimingInfo> packetTimings;
    packetTimings.reserveInitialCapacity(samples.size());
    Vector<size_t> packetSizes;
    packetSizes.reserveInitialCapacity(samples.size());
    auto cumulativeTrimDuration = MediaTime::zeroTime();
    for (auto& sample : samples) {
        RefPtr sampleData = sample.data;
        auto blockBuffer = sampleData->createCMBlockBuffer();
        if (!blockBuffer)
            return makeUnexpected("Couldn't create CMBlockBuffer");

        if (!completeBlockBuffers)
            completeBlockBuffers = WTF::move(blockBuffer);
        else {
            auto err = PAL::CMBlockBufferAppendBufferReference(completeBlockBuffers.get(), blockBuffer.get(), 0, 0, 0);
            if (err != kCMBlockBufferNoErr)
                return makeUnexpected("CMBlockBufferAppendBufferReference failed");
        }
        packetTimings.append({ PAL::toCMTime(sample.duration), PAL::toCMTime(sample.presentationTime), PAL::toCMTime(sample.decodeTime) });
        packetSizes.append(sampleData->size());
        cumulativeTrimDuration += sample.trimInterval.first;
    }

    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, completeBlockBuffers.get(), format.get(), packetSizes.size(), packetTimings.size(), packetTimings.span().data(), packetSizes.size(), packetSizes.span().data(), &rawSampleBuffer))
        return makeUnexpected("CMSampleBufferCreateReady failed: OOM");

    if (samples.isVideo() && samples.size()) {
        auto attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
        ASSERT(attachmentsArray);
        if (!attachmentsArray)
            return makeUnexpected("No sample attachment found");
        ASSERT(size_t(CFArrayGetCount(attachmentsArray)) == samples.size());
        for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
            CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
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

    if (cumulativeTrimDuration > MediaTime::zeroTime()) {
        auto trimDurationDict = adoptCF(PAL::softLink_CoreMedia_CMTimeCopyAsDictionary(PAL::toCMTime(cumulativeTrimDuration), kCFAllocatorDefault));
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart, trimDurationDict.get(), kCMAttachmentMode_ShouldPropagate);
    }

#if ENABLE(ENCRYPTED_MEDIA)
    if (!samples.info() || !samples.info()->encryptionDataCollection())
        return adoptCF(rawSampleBuffer);

    RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
    ASSERT(attachmentsArray);
    if (!attachmentsArray)
        return makeUnexpected("No sample attachment found");
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

    auto mediaSampleItemForSample = [](auto&& sample) {
        MediaTime duration = sample->duration();
        RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sample->sampleBuffer());
        auto trimDurationAtStart = MediaTime::zeroTime();
        if (RetainPtr trimDurationDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMGetAttachment(sample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart, nullptr)))
            trimDurationAtStart = PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimDurationDict.get()));
        auto trimDurationAtEnd = MediaTime::zeroTime();
        if (RetainPtr trimDurationDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMGetAttachment(sample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd, nullptr)))
            trimDurationAtEnd = PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimDurationDict.get()));
#if ENABLE(ENCRYPTED_MEDIA)
        SInt32 bytesOfClearDataCount = 0;
        RefPtr<SharedBuffer> cryptorIV;
        RefPtr<SharedBuffer> cryptorSubsampleAuxiliaryData;

        RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sample->sampleBuffer(), false);
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
        return MediaSamplesBlock::MediaSampleItem {
            .presentationTime = sample->presentationTime(),
            .decodeTime = sample->decodeTime(),
            .duration = sample->duration() + trimDurationAtStart + trimDurationAtEnd,
            .trimInterval = { trimDurationAtStart, trimDurationAtEnd },
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
        sample.append(mediaSampleItemForSample(MediaSampleAVFObjC::create(cmSample, info->trackID())));
        return makeUniqueRef<MediaSamplesBlock>(info.get(), WTF::move(sample));
    }

    auto subSamples = MediaSampleAVFObjC::create(cmSample, info ? info->trackID() : 0)->divide();
    MediaSamplesBlock::SamplesVector samples(subSamples.size(), [&](auto index) {
        return mediaSampleItemForSample(subSamples[index]);
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
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, convertToCMColorPrimaries(*colorSpace.primaries), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.transfer)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey, convertToCMTransferFunction(*colorSpace.transfer), kCVAttachmentMode_ShouldPropagate);
    if (colorSpace.matrix)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, convertToCMYCbCRMatrix(*colorSpace.matrix), kCVAttachmentMode_ShouldPropagate);
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

RetainPtr<CMBlockBufferRef> ensureContiguousBlockBuffer(CMBlockBufferRef rawBlockBuffer)
{
    if (PAL::CMBlockBufferIsRangeContiguous(rawBlockBuffer, 0, 0))
        return rawBlockBuffer;
    CMBlockBufferRef contiguousBuffer;
    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBlockBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer)) {
        RELEASE_LOG_FAULT(Media, "Failed to create contiguous blockBuffer with error:%d", static_cast<int>(status));
        return nullptr;
    }
    return adoptCF(contiguousBuffer);
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
        auto length = CFDataGetLength(trackEncryptionData.get());
        auto ptr = (void*)(CFDataGetBytePtr(trackEncryptionData.get()));
        Ref destructorFunction = createSharedTask<void(void*)>([data = WTF::move(trackEncryptionData)] (void*) { UNUSED_PARAM(data); });
        Ref trackEncryptionDataBuffer = ArrayBuffer::create(JSC::ArrayBufferContents(ptr, length, std::nullopt, WTF::move(destructorFunction)));

        ISOTrackEncryptionBox trackEncryptionBox;
        auto trackEncryptionView = JSC::DataView::create(WTF::move(trackEncryptionDataBuffer), 0, length);
        if (!trackEncryptionBox.parseWithoutTypeAndSize(trackEncryptionView))
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

} // namespace WebCore

#endif // PLATFORM(COCOA)
