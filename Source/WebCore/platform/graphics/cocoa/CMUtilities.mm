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
#import "Logging.h"
#import "MediaSample.h"
#import "MediaUtilities.h"
#import "SharedBuffer.h"
#import "WebMAudioUtilitiesCocoa.h"
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/Scope.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

#if ENABLE(VORBIS)
constexpr uint32_t kAudioFormatVorbis = 'vorb';
#endif

namespace WebCore {

#if ENABLE(OPUS) || ENABLE(VORBIS)

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioInfo& info)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName.value;
    UInt32 size = sizeof(asbd);
    auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, info.cookieData->size(), info.cookieData->data(), &size, &asbd);
    if (error) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescription failed with error %d (%.4s)", error, (char *)&error);
        return nullptr;
    }

    return createAudioFormatDescription(CAAudioStreamDescription(asbd), info.cookieData->size(), info.cookieData->data());
}

#endif

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
        return PAL::canLoad_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() ? PAL::get_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() : nullptr;
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

    if (info.isAudio()) {
        auto& audioInfo = downcast<const AudioInfo>(info);
        if (!audioInfo.cookieData || !audioInfo.cookieData->size())
            return nullptr;

        switch (audioInfo.codecName.value) {
#if ENABLE(OPUS)
        case kAudioFormatOpus:
            if (!isOpusDecoderAvailable())
                return nullptr;
            return createAudioFormatDescription(audioInfo);
#endif
#if ENABLE(VORBIS)
        case kAudioFormatVorbis:
            if (!isVorbisDecoderAvailable())
                return nullptr;
            return createAudioFormatDescription(audioInfo);
#endif
        default:
            return nullptr;
        }
    }

    auto& videoInfo = downcast<const VideoInfo>(info);

    auto data = videoInfo.atomData->createCFData();
    ASSERT(videoInfo.codecName == 'vp09' || videoInfo.codecName == 'vp08');
    CFTypeRef configurationKeys[] = { CFSTR("vpcC") };
    CFTypeRef configurationValues[] = { data.get() };
    auto configurationDict = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, configurationKeys, configurationValues, std::size(configurationKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    Vector<CFTypeRef> extensionsKeys { PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms };
    Vector<CFTypeRef> extensionsValues = { configurationDict.get() };

    if (videoInfo.colorSpace.fullRange && *videoInfo.colorSpace.fullRange) {
        extensionsKeys.append(PAL::kCMFormatDescriptionExtension_FullRangeVideo);
        extensionsValues.append(kCFBooleanTrue);
    }

    if (videoInfo.colorSpace.primaries) {
        if (auto cmColorPrimaries = convertToCMColorPrimaries(*videoInfo.colorSpace.primaries)) {
            extensionsKeys.append(kCVImageBufferColorPrimariesKey);
            extensionsValues.append(cmColorPrimaries);
        }
    }
    if (videoInfo.colorSpace.transfer) {
        if (auto cmTransferFunction = convertToCMTransferFunction(*videoInfo.colorSpace.transfer)) {
            extensionsKeys.append(kCVImageBufferTransferFunctionKey);
            extensionsValues.append(cmTransferFunction);
        }
    }

    if (videoInfo.colorSpace.matrix) {
        if (auto cmMatrix = convertToCMYCbCRMatrix(*videoInfo.colorSpace.matrix)) {
            extensionsKeys.append(kCVImageBufferYCbCrMatrixKey);
            extensionsValues.append(cmMatrix);
        }
    }

    auto extensions = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, extensionsKeys.data(), extensionsValues.data(), extensionsKeys.size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    CMVideoCodecType codec = videoInfo.codecName == "vp09" ? kCMVideoCodecType_VP9 : 'vp08';
    if (noErr != PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, codec, videoInfo.size.width(), videoInfo.size.height(), extensions.get(), &formatDescription))
        return nullptr;

    return adoptCF(formatDescription);
}

Expected<RetainPtr<CMSampleBufferRef>, CString> toCMSampleBuffer(MediaSamplesBlock&& samples, CMFormatDescriptionRef formatDescription)
{
    if (!samples.info())
        return makeUnexpected("No TrackInfo found");

    auto scopeExit = makeScopeExit([&] {
        samples.clear();
    });

    auto format = formatDescription ? retainPtr(formatDescription) : createFormatDescriptionFromTrackInfo(*samples.info());
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
        if (!std::holds_alternative<Ref<const FragmentedSharedBuffer>>(sample.data))
            return makeUnexpected("Invalid MediaSamplesBlock type");
        auto& data = std::get<Ref<const FragmentedSharedBuffer>>(sample.data);
        auto blockBuffer = data->createCMBlockBuffer();
        if (!blockBuffer)
            return makeUnexpected("Couldn't create CMBlockBuffer");

        if (!completeBlockBuffers)
            completeBlockBuffers = WTFMove(blockBuffer);
        else {
            auto err = PAL::CMBlockBufferAppendBufferReference(completeBlockBuffers.get(), blockBuffer.get(), 0, 0, 0);
            if (err != kCMBlockBufferNoErr)
                return makeUnexpected("CMBlockBufferAppendBufferReference failed");
        }
        packetTimings.append({ PAL::toCMTime(sample.duration), PAL::toCMTime(sample.presentationTime), PAL::toCMTime(sample.decodeTime) });
        packetSizes.append(data->size());
        cumulativeTrimDuration += sample.trimDuration;
    }

    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (auto err = PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, completeBlockBuffers.get(), format.get(), packetSizes.size(), packetTimings.size(), packetTimings.data(), packetSizes.size(), packetSizes.data(), &rawSampleBuffer))
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
        }
    } else if (samples.isAudio() && samples.discontinuity())
        PAL::CMSetAttachment(rawSampleBuffer, PAL::kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, *samples.discontinuity() ? kCFBooleanTrue : kCFBooleanFalse, kCMAttachmentMode_ShouldPropagate);

    if (cumulativeTrimDuration > MediaTime::zeroTime()) {
        auto trimDurationDict = adoptCF(PAL::softLink_CoreMedia_CMTimeCopyAsDictionary(PAL::toCMTime(cumulativeTrimDuration), kCFAllocatorDefault));
        PAL::CMSetAttachment(rawSampleBuffer, PAL::get_CoreMedia_kCMSampleBufferAttachmentKey_TrimDurationAtStart(), trimDurationDict.get(), kCMAttachmentMode_ShouldPropagate);
    }

    return adoptCF(rawSampleBuffer);
}

PacketDurationParser::PacketDurationParser(const AudioInfo& info)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = info.codecName.value;
    UInt32 size = sizeof(asbd);
    auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, info.cookieData->size(), info.cookieData->data(), &size, &asbd);
    if (error || !info.rate) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescription failed with error %d (%.4s)", (int)error, (char*)&error);
        return;
    }
    m_audioFormatID = asbd.mFormatID;
    m_sampleRate = info.rate;
    m_constantFramesPerPacket = asbd.mFramesPerPacket;
#if HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    switch (m_audioFormatID) {
#if ENABLE(VORBIS)
    case kAudioFormatVorbis: {
        AudioFormatInfo formatInfo = { asbd, info.cookieData->data(), (UInt32)info.cookieData->size() };
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

size_t PacketDurationParser::framesInPacket(SharedBuffer& packet)
{
#if !HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    UNUSED_PARAM(packet);
    return m_constantFramesPerPacket;
#else
    if (m_constantFramesPerPacket)
        return m_constantFramesPerPacket;

    if (packet.isEmpty())
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
        if (!(m_vorbisModeInfo->mModeFlags & (1 << modeIndex)))
            blockSize = m_vorbisModeInfo->mShortBlockSize;
        else
            blockSize = m_vorbisModeInfo->mLongBlockSize;
        // The first vorbis packet decoded doesn't output audible content, and should be undetermined.
        // However as content could be fed in any order, we must assume that previous content could be available at some stage.
        size_t framesOfOutput = (blockSize + m_lastVorbisBlockSize) / 4;
        m_lastVorbisBlockSize = blockSize;

        return framesOfOutput;
        break;
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


} // namespace WebCore

#endif // PLATFORM(COCOA)
