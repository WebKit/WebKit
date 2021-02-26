/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SourceBufferParserWebM.h"

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivateWebM.h"
#include "ContentType.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSampleAVFObjC.h"
#include "NotImplemented.h"
#include "PlatformMediaSessionManager.h"
#include "SharedBuffer.h"
#include "VP9UtilitiesCocoa.h"
#include "VideoTrackPrivateWebM.h"
#include "WebMAudioUtilitiesCocoa.h"
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <webm/webm_parser.h>
#include <wtf/Algorithms.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StdList.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/darwin/WeakLinking.h>
#include <wtf/spi/darwin/OSVariantSPI.h>

#include "CoreVideoSoftLink.h"
#include "VideoToolboxSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

WTF_WEAK_LINK_FORCE_IMPORT(webm::swap);

namespace WTF {

template<typename> struct LogArgument;

template<> struct LogArgument<webm::TrackType> {
    static String toString(webm::TrackType type)
    {
        switch (type) {
        case webm::TrackType::kVideo: return "Video"_s;
        case webm::TrackType::kAudio: return "Audio"_s;
        case webm::TrackType::kComplex: return "Complex"_s;
        case webm::TrackType::kLogo: return "Logo"_s;
        case webm::TrackType::kSubtitle: return "Subtitle"_s;
        case webm::TrackType::kButtons: return "Buttons"_s;
        case webm::TrackType::kControl: return "Control"_s;
        }
        return "Unknown"_s;
    }
};

template<> struct LogArgument<webm::Id> {
    static String toString(webm::Id id)
    {
        switch (id) {
        case webm::Id::kEbml: return "Ebml"_s;
        case webm::Id::kEbmlVersion: return "EbmlVersion"_s;
        case webm::Id::kEbmlReadVersion: return "EbmlReadVersion"_s;
        case webm::Id::kEbmlMaxIdLength: return "EbmlMaxIdLength"_s;
        case webm::Id::kEbmlMaxSizeLength: return "EbmlMaxSizeLength"_s;
        case webm::Id::kDocType: return "DocType"_s;
        case webm::Id::kDocTypeVersion: return "DocTypeVersion"_s;
        case webm::Id::kDocTypeReadVersion: return "DocTypeReadVersion"_s;
        case webm::Id::kVoid: return "Void"_s;
        case webm::Id::kSegment: return "Segment"_s;
        case webm::Id::kSeekHead: return "SeekHead"_s;
        case webm::Id::kSeek: return "Seek"_s;
        case webm::Id::kSeekId: return "SeekId"_s;
        case webm::Id::kSeekPosition: return "SeekPosition"_s;
        case webm::Id::kInfo: return "Info"_s;
        case webm::Id::kTimecodeScale: return "TimecodeScale"_s;
        case webm::Id::kDuration: return "Duration"_s;
        case webm::Id::kDateUtc: return "DateUtc"_s;
        case webm::Id::kTitle: return "Title"_s;
        case webm::Id::kMuxingApp: return "MuxingApp"_s;
        case webm::Id::kWritingApp: return "WritingApp"_s;
        case webm::Id::kCluster: return "Cluster"_s;
        case webm::Id::kTimecode: return "Timecode"_s;
        case webm::Id::kPrevSize: return "PrevSize"_s;
        case webm::Id::kSimpleBlock: return "SimpleBlock"_s;
        case webm::Id::kBlockGroup: return "BlockGroup"_s;
        case webm::Id::kBlock: return "Block"_s;
        case webm::Id::kBlockVirtual: return "BlockVirtual"_s;
        case webm::Id::kBlockAdditions: return "BlockAdditions"_s;
        case webm::Id::kBlockMore: return "BlockMore"_s;
        case webm::Id::kBlockAddId: return "BlockAddId"_s;
        case webm::Id::kBlockAdditional: return "BlockAdditional"_s;
        case webm::Id::kBlockDuration: return "BlockDuration"_s;
        case webm::Id::kReferenceBlock: return "ReferenceBlock"_s;
        case webm::Id::kDiscardPadding: return "DiscardPadding"_s;
        case webm::Id::kSlices: return "Slices"_s;
        case webm::Id::kTimeSlice: return "TimeSlice"_s;
        case webm::Id::kLaceNumber: return "LaceNumber"_s;
        case webm::Id::kTracks: return "Tracks"_s;
        case webm::Id::kTrackEntry: return "TrackEntry"_s;
        case webm::Id::kTrackNumber: return "TrackNumber"_s;
        case webm::Id::kTrackUid: return "TrackUid"_s;
        case webm::Id::kTrackType: return "TrackType"_s;
        case webm::Id::kFlagEnabled: return "FlagEnabled"_s;
        case webm::Id::kFlagDefault: return "FlagDefault"_s;
        case webm::Id::kFlagForced: return "FlagForced"_s;
        case webm::Id::kFlagLacing: return "FlagLacing"_s;
        case webm::Id::kDefaultDuration: return "DefaultDuration"_s;
        case webm::Id::kName: return "Name"_s;
        case webm::Id::kLanguage: return "Language"_s;
        case webm::Id::kCodecId: return "CodecId"_s;
        case webm::Id::kCodecPrivate: return "CodecPrivate"_s;
        case webm::Id::kCodecName: return "CodecName"_s;
        case webm::Id::kCodecDelay: return "CodecDelay"_s;
        case webm::Id::kSeekPreRoll: return "SeekPreRoll"_s;
        case webm::Id::kVideo: return "Video"_s;
        case webm::Id::kFlagInterlaced: return "FlagInterlaced"_s;
        case webm::Id::kStereoMode: return "StereoMode"_s;
        case webm::Id::kAlphaMode: return "AlphaMode"_s;
        case webm::Id::kPixelWidth: return "PixelWidth"_s;
        case webm::Id::kPixelHeight: return "PixelHeight"_s;
        case webm::Id::kPixelCropBottom: return "PixelCropBottom"_s;
        case webm::Id::kPixelCropTop: return "PixelCropTop"_s;
        case webm::Id::kPixelCropLeft: return "PixelCropLeft"_s;
        case webm::Id::kPixelCropRight: return "PixelCropRight"_s;
        case webm::Id::kDisplayWidth: return "DisplayWidth"_s;
        case webm::Id::kDisplayHeight: return "DisplayHeight"_s;
        case webm::Id::kDisplayUnit: return "DisplayUnit"_s;
        case webm::Id::kAspectRatioType: return "AspectRatioType"_s;
        case webm::Id::kFrameRate: return "FrameRate"_s;
        case webm::Id::kColour: return "Colour"_s;
        case webm::Id::kMatrixCoefficients: return "MatrixCoefficients"_s;
        case webm::Id::kBitsPerChannel: return "BitsPerChannel"_s;
        case webm::Id::kChromaSubsamplingHorz: return "ChromaSubsamplingHorz"_s;
        case webm::Id::kChromaSubsamplingVert: return "ChromaSubsamplingVert"_s;
        case webm::Id::kCbSubsamplingHorz: return "CbSubsamplingHorz"_s;
        case webm::Id::kCbSubsamplingVert: return "CbSubsamplingVert"_s;
        case webm::Id::kChromaSitingHorz: return "ChromaSitingHorz"_s;
        case webm::Id::kChromaSitingVert: return "ChromaSitingVert"_s;
        case webm::Id::kRange: return "Range"_s;
        case webm::Id::kTransferCharacteristics: return "TransferCharacteristics"_s;
        case webm::Id::kPrimaries: return "Primaries"_s;
        case webm::Id::kMaxCll: return "MaxCll"_s;
        case webm::Id::kMaxFall: return "MaxFall"_s;
        case webm::Id::kMasteringMetadata: return "MasteringMetadata"_s;
        case webm::Id::kPrimaryRChromaticityX: return "PrimaryRChromaticityX"_s;
        case webm::Id::kPrimaryRChromaticityY: return "PrimaryRChromaticityY"_s;
        case webm::Id::kPrimaryGChromaticityX: return "PrimaryGChromaticityX"_s;
        case webm::Id::kPrimaryGChromaticityY: return "PrimaryGChromaticityY"_s;
        case webm::Id::kPrimaryBChromaticityX: return "PrimaryBChromaticityX"_s;
        case webm::Id::kPrimaryBChromaticityY: return "PrimaryBChromaticityY"_s;
        case webm::Id::kWhitePointChromaticityX: return "WhitePointChromaticityX"_s;
        case webm::Id::kWhitePointChromaticityY: return "WhitePointChromaticityY"_s;
        case webm::Id::kLuminanceMax: return "LuminanceMax"_s;
        case webm::Id::kLuminanceMin: return "LuminanceMin"_s;
        case webm::Id::kProjection: return "Projection"_s;
        case webm::Id::kProjectionType: return "ProjectionType"_s;
        case webm::Id::kProjectionPrivate: return "ProjectionPrivate"_s;
        case webm::Id::kProjectionPoseYaw: return "ProjectionPoseYaw"_s;
        case webm::Id::kProjectionPosePitch: return "ProjectionPosePitch"_s;
        case webm::Id::kProjectionPoseRoll: return "ProjectionPoseRoll"_s;
        case webm::Id::kAudio: return "Audio"_s;
        case webm::Id::kSamplingFrequency: return "SamplingFrequency"_s;
        case webm::Id::kOutputSamplingFrequency: return "OutputSamplingFrequency"_s;
        case webm::Id::kChannels: return "Channels"_s;
        case webm::Id::kBitDepth: return "BitDepth"_s;
        case webm::Id::kContentEncodings: return "ContentEncodings"_s;
        case webm::Id::kContentEncoding: return "ContentEncoding"_s;
        case webm::Id::kContentEncodingOrder: return "ContentEncodingOrder"_s;
        case webm::Id::kContentEncodingScope: return "ContentEncodingScope"_s;
        case webm::Id::kContentEncodingType: return "ContentEncodingType"_s;
        case webm::Id::kContentEncryption: return "ContentEncryption"_s;
        case webm::Id::kContentEncAlgo: return "ContentEncAlgo"_s;
        case webm::Id::kContentEncKeyId: return "ContentEncKeyId"_s;
        case webm::Id::kContentEncAesSettings: return "ContentEncAesSettings"_s;
        case webm::Id::kAesSettingsCipherMode: return "AesSettingsCipherMode"_s;
        case webm::Id::kCues: return "Cues"_s;
        case webm::Id::kCuePoint: return "CuePoint"_s;
        case webm::Id::kCueTime: return "CueTime"_s;
        case webm::Id::kCueTrackPositions: return "CueTrackPositions"_s;
        case webm::Id::kCueTrack: return "CueTrack"_s;
        case webm::Id::kCueClusterPosition: return "CueClusterPosition"_s;
        case webm::Id::kCueRelativePosition: return "CueRelativePosition"_s;
        case webm::Id::kCueDuration: return "CueDuration"_s;
        case webm::Id::kCueBlockNumber: return "CueBlockNumber"_s;
        case webm::Id::kChapters: return "Chapters"_s;
        case webm::Id::kEditionEntry: return "EditionEntry"_s;
        case webm::Id::kChapterAtom: return "ChapterAtom"_s;
        case webm::Id::kChapterUid: return "ChapterUid"_s;
        case webm::Id::kChapterStringUid: return "ChapterStringUid"_s;
        case webm::Id::kChapterTimeStart: return "ChapterTimeStart"_s;
        case webm::Id::kChapterTimeEnd: return "ChapterTimeEnd"_s;
        case webm::Id::kChapterDisplay: return "ChapterDisplay"_s;
        case webm::Id::kChapString: return "ChapString"_s;
        case webm::Id::kChapLanguage: return "ChapLanguage"_s;
        case webm::Id::kChapCountry: return "ChapCountry"_s;
        case webm::Id::kTags: return "Tags"_s;
        case webm::Id::kTag: return "Tag"_s;
        case webm::Id::kTargets: return "Targets"_s;
        case webm::Id::kTargetTypeValue: return "TargetTypeValue"_s;
        case webm::Id::kTargetType: return "TargetType"_s;
        case webm::Id::kTagTrackUid: return "TagTrackUid"_s;
        case webm::Id::kSimpleTag: return "SimpleTag"_s;
        case webm::Id::kTagName: return "TagName"_s;
        case webm::Id::kTagLanguage: return "TagLanguage"_s;
        case webm::Id::kTagDefault: return "TagDefault"_s;
        case webm::Id::kTagString: return "TagString"_s;
        case webm::Id::kTagBinary: return "TagBinary"_s;
        }
        return "Unknown"_s;
    }
};

template<> struct LogArgument<WebCore::SourceBufferParserWebM::State> {
    static String toString(WebCore::SourceBufferParserWebM::State state)
    {
        switch (state) {
        case WebCore::SourceBufferParserWebM::State::None: return "None"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingEbml: return "ReadingEbml"_s;
        case WebCore::SourceBufferParserWebM::State::ReadEbml: return "ReadEbml"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingSegment: return "ReadingSegment"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingInfo: return "ReadingInfo"_s;
        case WebCore::SourceBufferParserWebM::State::ReadInfo: return "ReadInfo"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingTracks: return "ReadingTracks"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingTrack: return "ReadingTrack"_s;
        case WebCore::SourceBufferParserWebM::State::ReadTrack: return "ReadTrack"_s;
        case WebCore::SourceBufferParserWebM::State::ReadingCluster: return "ReadingCluster"_s;
        }
        return "Unknown"_s;
    }
};

} // namespace WTF

namespace WebCore {

using namespace PAL;

static WTFLogChannel& logChannel() { return LogMedia; }
static const char* logClassName() { return "SourceBufferParserWebM"; }

// FIXME: Remove this once kCMVideoCodecType_VP9 is added to CMFormatDescription.h
constexpr CMVideoCodecType kCMVideoCodecType_VP9 { 'vp09' };

static bool isWebmParserAvailable()
{
    return !!webm::swap;
}

using namespace webm;

class SourceBufferParserWebM::StreamingVectorReader final : public webm::Reader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void appendSegment(Segment&& segment)
    {
        m_data.push_back(WTFMove(segment));
        if (m_currentSegment == m_data.end())
            --m_currentSegment;
    }

    Status Read(std::size_t numToRead, uint8_t* outputBuffer, uint64_t* numActuallyRead) final
    {
        ASSERT(outputBuffer && numActuallyRead);
        if (!outputBuffer || !numActuallyRead)
            return Status(Status::kNotEnoughMemory);

        *numActuallyRead = 0;
        while (numToRead && m_currentSegment != m_data.end()) {
            auto& currentSegment = *m_currentSegment;

            if (m_positionWithinSegment >= currentSegment.size()) {
                advanceToNextSegment();
                continue;
            }

            *numActuallyRead = currentSegment.read(m_positionWithinSegment, numToRead, outputBuffer);
            m_position += *numActuallyRead;
            m_positionWithinSegment += *numActuallyRead;
            numToRead -= *numActuallyRead;
            if (m_positionWithinSegment == currentSegment.size())
                advanceToNextSegment();
        }
        if (!numToRead)
            return Status(Status::kOkCompleted);
        if (*numActuallyRead)
            return Status(Status::kOkPartial);
        return Status(Status::kWouldBlock);
    }

    Status Skip(uint64_t numToSkip, uint64_t* numActuallySkipped) final
    {
        ASSERT(numActuallySkipped);
        if (!numActuallySkipped)
            return Status(Status::kNotEnoughMemory);

        *numActuallySkipped = 0;
        while (m_currentSegment != m_data.end()) {
            auto& currentSegment = *m_currentSegment;

            if (m_positionWithinSegment >= currentSegment.size()) {
                advanceToNextSegment();
                continue;
            }

            size_t numAvailable = currentSegment.size() - m_positionWithinSegment;

            if (numToSkip < numAvailable) {
                *numActuallySkipped += numToSkip;
                m_position += numToSkip;
                m_positionWithinSegment += numToSkip;
                return Status(Status::kOkCompleted);
            }

            *numActuallySkipped += numAvailable;
            m_position += numAvailable;
            m_positionWithinSegment += numAvailable;
            numToSkip -= numAvailable;
            advanceToNextSegment();
            continue;
        }
        if (!numToSkip)
            return Status(Status::kOkCompleted);
        if (*numActuallySkipped)
            return Status(Status::kOkPartial);
        return Status(Status::kWouldBlock);
    }

    uint64_t Position() const final { return m_position; }

    void reset()
    {
        m_position = 0;
        m_data.clear();
        m_currentSegment = m_data.end();
    }

    bool rewindTo(uint64_t rewindToPosition)
    {
        ASSERT(rewindToPosition <= m_position);
        if (rewindToPosition > m_position)
            return false;

        auto rewindAmount = m_position - rewindToPosition;
        while (rewindAmount) {
            if (rewindAmount <= m_positionWithinSegment) {
                m_positionWithinSegment -= rewindAmount;
                return true;
            }

            if (m_currentSegment == m_data.begin())
                return false;

            rewindAmount -= m_positionWithinSegment;
            --m_currentSegment;
            m_positionWithinSegment = m_currentSegment->size();
        }
        return true;
    }

    void reclaimSegments()
    {
        m_data.erase(m_data.begin(), m_currentSegment);
    }

private:
    void advanceToNextSegment()
    {
        ASSERT(m_currentSegment != m_data.end());
        if (m_currentSegment == m_data.end())
            return;

        ASSERT(m_positionWithinSegment == m_currentSegment->size());

        ++m_currentSegment;
        m_positionWithinSegment = 0;
    }

    StdList<Segment> m_data;
    StdList<Segment>::iterator m_currentSegment { m_data.end() };
    size_t m_position { 0 };
    size_t m_positionWithinSegment { 0 };
};

class MediaDescriptionWebM final : public MediaDescription {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MediaDescriptionWebM> create(webm::TrackEntry&& track)
    {
        ASSERT(isWebmParserAvailable());
        return adoptRef(*new MediaDescriptionWebM(WTFMove(track)));
    }

    MediaDescriptionWebM(webm::TrackEntry&& track)
        : m_track { WTFMove(track) }
    {
    }

    AtomString codec() const final
    {
        if (m_codec)
            return *m_codec;

        // From: https://www.matroska.org/technical/codec_specs.html
        // "Each Codec ID MUST include a Major Codec ID immediately following the Codec ID Prefix.
        // A Major Codec ID MAY be followed by an OPTIONAL Codec ID Suffix to communicate a refinement
        // of the Major Codec ID. If a Codec ID Suffix is used, then the Codec ID MUST include a forward
        // slash (“/”) as a separator between the Major Codec ID and the Codec ID Suffix. The Major Codec
        // ID MUST be composed of only capital letters (A-Z) and numbers (0-9). The Codec ID Suffix MUST
        // be composed of only capital letters (A-Z), numbers (0-9), underscore (“_”), and forward slash (“/”)."

        if (!m_track.codec_id.is_present()) {
            m_codec = emptyAtom();
            return *m_codec;
        }

        StringView codecID { m_track.codec_id.value().data(), (unsigned)m_track.codec_id.value().length() };
        if (!codecID.startsWith("V_") && !codecID.startsWith("A_") && !codecID.startsWith("S_")) {
            m_codec = emptyAtom();
            return *m_codec;
        }

        auto slashLocation = codecID.find('/');
        auto length = slashLocation == notFound ? codecID.length() - 2 : slashLocation - 2;
        m_codec = AtomString { codecID.substring(2, length).convertToASCIILowercase() };
        return *m_codec;
    }
    bool isVideo() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kVideo; }
    bool isAudio() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kAudio; }
    bool isText() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kSubtitle; }

    const webm::TrackEntry& track() { return m_track; }

private:
    mutable Optional<AtomString> m_codec;
    webm::TrackEntry m_track;
};

const HashSet<String, ASCIICaseInsensitiveHash>& SourceBufferParserWebM::webmMIMETypes()
{
    static auto types = makeNeverDestroyed([] {

        HashSet<String, ASCIICaseInsensitiveHash> types;

#if ENABLE(VP9)
        types.add("video/webm");
#endif
#if ENABLE(VORBIS) || ENABLE(OPUS)
        types.add("audio/webm");
#endif

        return types;
    }());

    return types;
}

static bool canLoadFormatReader()
{
#if !ENABLE(WEBM_FORMAT_READER)
    return false;
#elif USE(APPLE_INTERNAL_SDK)
    return true;
#else
    // FIXME (rdar://72320419): If WebKit was built with ad-hoc code-signing,
    // CoreMedia will only load the format reader plug-in when a user default
    // is set on Apple internal OSs. That means we cannot currently support WebM
    // in public SDK builds on customer OSs.
    static bool allowsInternalSecurityPolicies = os_variant_allows_internal_security_policies("com.apple.WebKit");
    return allowsInternalSecurityPolicies;
#endif // !USE(APPLE_INTERNAL_SDK)
}

bool SourceBufferParserWebM::isWebMFormatReaderAvailable()
{
    return PlatformMediaSessionManager::webMFormatReaderEnabled() && canLoadFormatReader() && isWebmParserAvailable();
}

MediaPlayerEnums::SupportsType SourceBufferParserWebM::isContentTypeSupported(const ContentType& type)
{
#if ENABLE(VP9) || ENABLE(VORBIS) || ENABLE(OPUS)
    if (!isWebmParserAvailable())
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    auto containerType = type.containerType();
    bool isAudioContainerType = WTF::equalIgnoringASCIICase(containerType, "audio/webm");
    bool isVideoContainerType = WTF::equalIgnoringASCIICase(containerType, "video/webm");
    if (!isAudioContainerType && !isVideoContainerType)
        return MediaPlayerEnums::SupportsType::IsNotSupported;
    
    bool isAnyAudioCodecAvailable = false;
#if ENABLE(VORBIS)
    isAnyAudioCodecAvailable |= isVorbisDecoderAvailable();
#endif
#if ENABLE(OPUS)
    isAnyAudioCodecAvailable |= isOpusDecoderAvailable();
#endif

    if (isAudioContainerType && !isAnyAudioCodecAvailable)
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    bool isAnyCodecAvailable = isAnyAudioCodecAvailable;
#if ENABLE(VP9)
    isAnyCodecAvailable |= isVP9DecoderAvailable();
#endif

    if (!isAnyCodecAvailable)
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    auto codecs = type.codecs();
    if (codecs.isEmpty())
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    for (auto& codec : codecs) {
#if ENABLE(VP9)
        if (codec.startsWith("vp09") || codec.startsWith("vp08") || equal(codec, "vp8") || equal(codec, "vp9")) {

            if (!isVP9DecoderAvailable())
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            auto codecParameters = parseVPCodecParameters(codec);
            if (!codecParameters)
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            if (!isVPCodecConfigurationRecordSupported(*codecParameters))
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            continue;
        }
#endif // ENABLE(VP9)

#if ENABLE(VORBIS)
        if (codec == "vorbis") {
            if (!isVorbisDecoderAvailable())
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            continue;
        }
#endif // ENABLE(VORBIS)

#if ENABLE(OPUS)
        if (codec == "opus") {
            if (!isOpusDecoderAvailable())
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            continue;
        }
#endif // ENABLE(OPUS)

        return MediaPlayerEnums::SupportsType::IsNotSupported;
    }

    return MediaPlayerEnums::SupportsType::IsSupported;

#else
    UNUSED_PARAM(type);

    return MediaPlayerEnums::SupportsType::IsNotSupported;
#endif // ENABLE(VP9) || ENABLE(VORBIS) || ENABLE(OPUS)
}

RefPtr<SourceBufferParserWebM> SourceBufferParserWebM::create(const ContentType& type)
{
    if (isContentTypeSupported(type) != MediaPlayerEnums::SupportsType::IsNotSupported)
        return adoptRef(new SourceBufferParserWebM());
    return nullptr;
}

static SourceBufferParserWebM::CallOnClientThreadCallback callOnMainThreadCallback()
{
    return [](Function<void()>&& function) {
        callOnMainThread(WTFMove(function));
    };
}

SourceBufferParserWebM::SourceBufferParserWebM()
    : m_reader(WTF::makeUniqueRef<StreamingVectorReader>())
    , m_callOnClientThreadCallback(callOnMainThreadCallback())
{
    if (isWebmParserAvailable())
        m_parser = WTF::makeUniqueWithoutFastMallocCheck<WebmParser>();
}

SourceBufferParserWebM::~SourceBufferParserWebM()
{
}

void SourceBufferParserWebM::appendData(Segment&& segment, CompletionHandler<void()>&& completionHandler, AppendFlags appendFlags)
{
    if (!m_parser) {
        completionHandler();
        return;
    }

    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "flags(", appendFlags == AppendFlags::Discontinuity ? "Discontinuity" : "", "), size(", segment.size(), ")");

    if (appendFlags == AppendFlags::Discontinuity) {
        m_reader->reset();
        m_parser->DidSeek();
    }
    m_reader->appendSegment(WTFMove(segment));

    while (true) {
        m_status = m_parser->Feed(this, &m_reader);
        if (m_status.ok() || m_status.code == Status::kEndOfFile || m_status.code == Status::kWouldBlock) {
            m_reader->reclaimSegments();
            completionHandler();
            return;
        }

        if (m_status.code != static_cast<int32_t>(ErrorCode::ReceivedEbmlInsideSegment))
            break;

        // The WebM Byte Stream Format <https://w3c.github.io/media-source/webm-byte-stream-format.html>
        // states that an "Initialization Segment" starts with an Ebml Element followed by a Segment Element,
        // and that a "Media Segment" is a single Cluster Element. However, since Cluster Elements are contained
        // within a Segment Element, this means that a new Ebml Element can be appended at any time while the
        // parser is still parsing children of a Segment Element. In this scenario, "rewind" the reader to the
        // position of the incoming Ebml Element, and reset the parser, which will cause the Ebml element to be
        // parsed as a top-level element, rather than as a child of the Segment.
        if (!m_reader->rewindTo(*m_rewindToPosition)) {
            ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "failed to rewind reader");
            break;
        }

        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "received Ebml element while parsing Segment element. Rewound reader and reset parser, retrying");
        m_rewindToPosition = WTF::nullopt;
        m_parser->DidSeek();
        m_state = State::None;
        continue;
    }

    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "status.code(", m_status.code, ")");
    m_callOnClientThreadCallback([this, protectedThis = makeRef(*this), code = m_status.code] {
        if (m_didEncounterErrorDuringParsingCallback)
            m_didEncounterErrorDuringParsingCallback(code);
    });

    completionHandler();
}

void SourceBufferParserWebM::flushPendingMediaData()
{
}

void SourceBufferParserWebM::setShouldProvideMediaDataForTrackID(bool, uint64_t)
{
    notImplemented();
}

bool SourceBufferParserWebM::shouldProvideMediadataForTrackID(uint64_t)
{
    notImplemented();
    return false;
}

void SourceBufferParserWebM::resetParserState()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    flushPendingAudioBuffers();
    if (m_parser)
        m_parser->DidSeek();
    m_state = State::None;
    m_tracks.clear();
    m_initializationSegment = nullptr;
    m_initializationSegmentEncountered = false;
    m_currentBlock.reset();
}

void SourceBufferParserWebM::invalidate()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_parser = nullptr;
    m_tracks.clear();
    m_initializationSegment = nullptr;
    m_currentBlock.reset();
}

void SourceBufferParserWebM::setLogger(const Logger& logger, const void* logIdentifier)
{
    m_logger = makeRefPtr(logger);
    m_logIdentifier = logIdentifier;
}

auto SourceBufferParserWebM::trackDataForTrackNumber(uint64_t trackNumber) -> TrackData*
{
    for (auto& track : m_tracks) {
        if (track->track().track_number.is_present() && track->track().track_number.value() == trackNumber)
            return &track;
    }
    return nullptr;
}

Status SourceBufferParserWebM::OnElementBegin(const ElementMetadata& metadata, Action* action)
{
    UNUSED_PARAM(metadata);
    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    if (m_state == State::ReadingSegment && metadata.id == Id::kEbml) {
        m_rewindToPosition = metadata.position;
        return Status(Status::Code(ErrorCode::ReceivedEbmlInsideSegment));
    }

    if ((m_state == State::None && metadata.id != Id::kEbml && metadata.id != Id::kSegment)
        || (m_state == State::ReadingSegment && metadata.id != Id::kInfo && metadata.id != Id::kTracks && metadata.id != Id::kCluster)) {
        INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "state(", m_state, "), id(", metadata.id, "), position(", metadata.position, "), headerSize(", metadata.header_size, "), size(", metadata.size, "), skipping");

        *action = Action::kSkip;
        return Status(Status::kOkCompleted);
    }

    auto oldState = m_state;

    if (metadata.id == Id::kEbml)
        m_state = State::ReadingEbml;
    else if (metadata.id == Id::kSegment)
        m_state = State::ReadingSegment;
    else if (metadata.id == Id::kInfo)
        m_state = State::ReadingInfo;
    else if (metadata.id == Id::kTracks)
        m_state = State::ReadingTracks;
    else if (metadata.id == Id::kTrackEntry)
        m_state = State::ReadingTrack;
    else if (metadata.id == Id::kCluster)
        m_state = State::ReadingCluster;

    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "state(", oldState, "->", m_state, "), id(", metadata.id, "), position(", metadata.position, "), headerSize(", metadata.header_size, "), size(", metadata.size, ")");

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnElementEnd(const ElementMetadata& metadata)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    auto oldState = m_state;

    if (metadata.id == Id::kEbml || metadata.id == Id::kSegment)
        m_state = State::None;
    else if (metadata.id == Id::kInfo || metadata.id == Id::kTracks || metadata.id == Id::kCluster)
        m_state = State::ReadingSegment;
    else if (metadata.id == Id::kTrackEntry)
        m_state = State::ReadingTracks;

    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "state(", oldState, "->", m_state, "), id(", metadata.id, "), size(", metadata.size, ")");

    if (metadata.id == Id::kTracks) {
        if (!m_keyIds.isEmpty() && !m_didProvideContentKeyRequestInitializationDataForTrackIDCallback) {
            ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered encrypted content without an key request callback");
            return Status(Status::Code(ErrorCode::ContentEncrypted));
        }

        if (m_initializationSegmentEncountered && m_didParseInitializationDataCallback) {
            m_callOnClientThreadCallback([this, protectedThis = makeRef(*this), initializationSegment = WTFMove(*m_initializationSegment)]() mutable {
                m_didParseInitializationDataCallback(WTFMove(initializationSegment));
            });
        }
        m_initializationSegmentEncountered = false;
        m_initializationSegment = nullptr;

        if (!m_keyIds.isEmpty()) {
            for (auto& keyIdPair : m_keyIds)
                m_didProvideContentKeyRequestInitializationDataForTrackIDCallback(WTFMove(keyIdPair.second), keyIdPair.first);
        }
        m_keyIds.clear();
    }

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnEbml(const ElementMetadata& metadata, const Ebml& ebml)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (ebml.doc_type.is_present() && ebml.doc_type.value().compare("webm"))
        return Status(Status::Code(ErrorCode::InvalidDocType));

    m_initializationSegmentEncountered = true;
    m_initializationSegment = WTF::makeUniqueWithoutFastMallocCheck<InitializationSegment>();

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnSegmentBegin(const ElementMetadata& metadata, Action* action)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (!m_initializationSegmentEncountered) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered Segment before Embl");
        return Status(Status::Code(ErrorCode::InvalidInitSegment));
    }

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);
    *action = Action::kRead;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnInfo(const ElementMetadata& metadata, const Info& info)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (!m_initializationSegmentEncountered || !m_initializationSegment) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered Info outside Segment");
        return Status(Status::Code(ErrorCode::InvalidInitSegment));
    }

    auto timecodeScale = info.timecode_scale.is_present() ? info.timecode_scale.value() : 1000000;
    m_timescale = 1000000000 / timecodeScale;
    m_initializationSegment->duration = info.duration.is_present() ? MediaTime(info.duration.value(), m_timescale) : MediaTime::indefiniteTime();

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnClusterBegin(const ElementMetadata& metadata, const Cluster& cluster, Action* action)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    if (cluster.timecode.is_present())
        m_currentTimecode = cluster.timecode.value();

    *action = Action::kRead;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnTrackEntry(const ElementMetadata& metadata, const TrackEntry& trackEntry)
{
    UNUSED_PARAM(metadata);
    if (!trackEntry.track_type.is_present() || !trackEntry.codec_id.is_present())
        return Status(Status::kOkCompleted);

    auto trackType = trackEntry.track_type.value();
    String codecId { trackEntry.codec_id.value().data(), (unsigned)trackEntry.codec_id.value().length() };

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, trackType, ", codec ", codecId);

    if (trackType == TrackType::kVideo && !supportedVideoCodecs().contains(codecId)) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered unsupported video codec ID ", codecId);
        return Status(Status::Code(ErrorCode::UnsupportedVideoCodec));
    }

    if (trackType == TrackType::kAudio && !supportedAudioCodecs().contains(codecId)) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered unsupported audio codec ID ", codecId);
        return Status(Status::Code(ErrorCode::UnsupportedAudioCodec));
    }

    if (trackType == TrackType::kVideo) {
        auto track = VideoTrackPrivateWebM::create(TrackEntry(trackEntry));
        if (m_logger)
            track->setLogger(*m_logger, LoggerHelper::childLogIdentifier(m_logIdentifier, ++m_nextChildIdentifier));
        m_initializationSegment->videoTracks.append({ MediaDescriptionWebM::create(TrackEntry(trackEntry)), WTFMove(track) });
    } else if (trackType == TrackType::kAudio) {
        auto track = AudioTrackPrivateWebM::create(TrackEntry(trackEntry));
        if (m_logger)
            track->setLogger(*m_logger, LoggerHelper::childLogIdentifier(m_logIdentifier, ++m_nextChildIdentifier));
        m_initializationSegment->audioTracks.append({ MediaDescriptionWebM::create(TrackEntry(trackEntry)), WTFMove(track) });
    }

    if (trackEntry.content_encodings.is_present() && !trackEntry.content_encodings.value().encodings.empty()) {
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "content_encodings detected:");
        for (auto& encoding : trackEntry.content_encodings.value().encodings) {
            if (!encoding.is_present())
                continue;

            auto& encryption = encoding.value().encryption;
            if (!encryption.is_present())
                continue;

            auto& keyIdElement = encryption.value().key_id;
            if (!keyIdElement.is_present())
                continue;

            auto& keyId = keyIdElement.value();
            m_keyIds.append(std::make_pair(trackEntry.track_uid.value(), Uint8Array::create(keyId.data(), keyId.size())));
        }
    }

    StringView codecString { trackEntry.codec_id.value().data(), (unsigned)trackEntry.codec_id.value().length() };
#if ENABLE(VP9)
    if (codecString == "V_VP9" && isVP9DecoderAvailable()) {
        m_tracks.append(VideoTrackData::create(CodecType::VP9, trackEntry, *this));
        return Status(Status::kOkCompleted);
    }
    if (codecString == "V_VP8" && isVP8DecoderAvailable()) {
        m_tracks.append(VideoTrackData::create(CodecType::VP8, trackEntry, *this));
        return Status(Status::kOkCompleted);
    }
#endif

#if ENABLE(VORBIS)
    if (codecString == "A_VORBIS" && isVorbisDecoderAvailable()) {
        m_tracks.append(AudioTrackData::create(CodecType::Vorbis, trackEntry, *this, m_minimumAudioSampleDuration));
        return Status(Status::kOkCompleted);
    }
#endif

#if ENABLE(OPUS)
    if (codecString == "A_OPUS" && isOpusDecoderAvailable()) {
        m_tracks.append(AudioTrackData::create(CodecType::Opus, trackEntry, *this, m_minimumAudioSampleDuration));
        return Status(Status::kOkCompleted);
    }
#endif

    m_tracks.append(TrackData::create(CodecType::Unsupported, trackEntry, *this));
    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockBegin(const ElementMetadata& metadata, const Block& block, Action* action)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;

    m_currentBlock = makeOptional<BlockVariant>(Block(block));

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockEnd(const ElementMetadata& metadata, const Block& block)
{
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(block);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_currentBlock = WTF::nullopt;

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnSimpleBlockBegin(const ElementMetadata& metadata, const SimpleBlock& block, Action* action)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;

    m_currentBlock = makeOptional<BlockVariant>(SimpleBlock(block));

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnSimpleBlockEnd(const ElementMetadata& metadata, const SimpleBlock& block)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    UNUSED_PARAM(block);

    m_currentBlock = WTF::nullopt;

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockGroupBegin(const webm::ElementMetadata& metadata, webm::Action* action)
{
    UNUSED_PARAM(metadata);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;
    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockGroupEnd(const webm::ElementMetadata& metadata, const webm::BlockGroup& blockGroup)
{
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(blockGroup);
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnFrame(const FrameMetadata& metadata, Reader* reader, uint64_t* bytesRemaining)
{
    ASSERT(reader);
    if (!reader)
        return Status(Status::kNotEnoughMemory);

    if (!m_currentBlock) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "no current block!");
        return Status(Status::kInvalidElementId);
    }

    auto* block = WTF::switchOn(*m_currentBlock, [](Block& block) {
        return &block;
    }, [](SimpleBlock& block) -> Block* {
        return &block;
    });
    if (!block)
        return Status(Status::kInvalidElementId);

    auto trackNumber = block->track_number;
    auto* trackData = trackDataForTrackNumber(trackNumber);
    if (!trackData) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Ignoring unknown track number ", trackNumber);
        return Status(Status::kInvalidElementId);
    }

    switch (trackData->codec()) {
    case CodecType::VP8:
    case CodecType::VP9:
    case CodecType::Vorbis:
    case CodecType::Opus:
        break;

    case CodecType::Unsupported:
        return Skip(reader, bytesRemaining);
    }

    return trackData->consumeFrameData(*reader, metadata, bytesRemaining, CMTimeMake(block->timecode + m_currentTimecode, m_timescale), block->num_frames);
}

void SourceBufferParserWebM::provideMediaData(RetainPtr<CMSampleBufferRef> sampleBuffer, uint64_t trackID, Optional<size_t> byteRangeOffset)
{
    m_callOnClientThreadCallback([this, protectedThis = makeRef(*this), sampleBuffer = WTFMove(sampleBuffer), trackID, byteRangeOffset] () mutable {
        if (!m_didProvideMediaDataCallback)
            return;

        auto mediaSample = MediaSampleAVFObjC::create(sampleBuffer.get(), trackID);
        if (byteRangeOffset)
            mediaSample->setByteRangeOffset(*byteRangeOffset);
        m_didProvideMediaDataCallback(WTFMove(mediaSample), trackID, emptyString());
    });
}

#define PARSER_LOG_ERROR_IF_POSSIBLE(...) if (parser().loggerPtr()) parser().loggerPtr()->error(logChannel(), WTF::Logger::LogSiteIdentifier(logClassName(), __func__, parser().logIdentifier()), __VA_ARGS__)

webm::Status SourceBufferParserWebM::VideoTrackData::consumeFrameData(webm::Reader& reader, const FrameMetadata& metadata, uint64_t* bytesRemaining, const CMTime& presentationTime, int sampleCount)
{
#if ENABLE(VP9)
    CMBlockBufferRef rawBlockBuffer = nullptr;

    if (!m_currentBlockBuffer) {
        auto err = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, metadata.size, kCFAllocatorDefault, nullptr, 0, metadata.size, 0, &rawBlockBuffer);
        if (err) {
            PARSER_LOG_ERROR_IF_POSSIBLE("CMBlockBufferCreateWithMemoryBlock failed with error", err);
            return Skip(&reader, bytesRemaining);
        }

        m_currentBlockBuffer = adoptCF(rawBlockBuffer);
        m_currentBlockBufferPosition = 0;

        err = CMBlockBufferAssureBlockMemory(m_currentBlockBuffer.get());
        if (err) {
            PARSER_LOG_ERROR_IF_POSSIBLE("CMAudioSampleBufferCreateWithPacketDescriptions failed with error", err);
            return Skip(&reader, bytesRemaining);
        }
    }

    size_t bytesToRead = metadata.size - m_currentBlockBufferPosition;
    while (bytesToRead) {
        size_t segmentSizeAtPosition = 0;
        uint8_t* blockBufferData = nullptr;
        auto err = CMBlockBufferGetDataPointer(m_currentBlockBuffer.get(), m_currentBlockBufferPosition, &segmentSizeAtPosition, nullptr, (char**)&blockBufferData);
        if (err) {
            PARSER_LOG_ERROR_IF_POSSIBLE("CMBlockBufferGetDataPointer failed with error", err);
            return Skip(&reader, bytesRemaining);
        }

        size_t bytesToReadFromThisSegment = std::min(bytesToRead, segmentSizeAtPosition);
        uint64_t bytesRead;

        auto status = reader.Read(bytesToReadFromThisSegment, (uint8_t*)blockBufferData, &bytesRead);
        bytesToRead -= bytesRead;
        *bytesRemaining -= bytesRead;
        m_currentBlockBufferPosition += bytesRead;

        // FIXME: We can't yet handle parsing a Frame that doesn't have all its memory available.
        if (status.code == webm::Status::kOkPartial || status.code == webm::Status::kWouldBlock)
            return status;
    }

    createSampleBuffer(presentationTime, sampleCount, metadata);
#else
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(presentationTime);
    UNUSED_PARAM(sampleCount);
#endif

    return Skip(&reader, bytesRemaining);
}

void SourceBufferParserWebM::VideoTrackData::createSampleBuffer(const CMTime& presentationTime, int sampleCount, const webm::FrameMetadata& metadata)
{
#if ENABLE(VP9)
    uint8_t* blockBufferData = nullptr;
    size_t segmentSizeAtPosition = 0;
    auto err = CMBlockBufferGetDataPointer(m_currentBlockBuffer.get(), 0, &segmentSizeAtPosition, nullptr, (char**)&blockBufferData);
    if (err) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMBlockBufferGetDataPointer failed with error", err);
        return;
    }

    bool isKey = false;
    RetainPtr<CMFormatDescriptionRef> formatDescription;
    if (codec() == CodecType::VP9) {
        if (!m_headerParser.ParseUncompressedHeader(blockBufferData, segmentSizeAtPosition))
            return;

        if (m_headerParser.key()) {
            isKey = true;
            auto formatDescription = createFormatDescriptionFromVP9HeaderParser(m_headerParser, track().video.value().colour);
            if (!formatDescription) {
                PARSER_LOG_ERROR_IF_POSSIBLE("failed to create format description from VPX header");
                return;
            }
            setFormatDescription(WTFMove(formatDescription));
        }
    } else if (codec() == CodecType::VP8) {
        auto header = parseVP8FrameHeader(blockBufferData, segmentSizeAtPosition);
        if (header && header->keyframe) {
            isKey = true;
            auto formatDescription = createFormatDescriptionFromVP8Header(*header, track().video.value().colour);
            if (!formatDescription) {
                PARSER_LOG_ERROR_IF_POSSIBLE("failed to create format description from VPX header");
                return;
            }
            setFormatDescription(WTFMove(formatDescription));
        }
    }

    auto track = this->track();
    // FIXME: A block might contain more than one frame, but only this frame has been read into `currentBlockBuffer`.
    // Below we create sample buffers for each frame, each with the block's timecode and `num_frames` value.
    // Shouldn't we create just one sample buffer once all the block's frames have been read into `currentBlockBuffer`?

    uint64_t duration = 0;
    if (track.default_duration.is_present())
        duration = track.default_duration.value() * presentationTime.timescale / 1000000000;

    CMSampleBufferRef rawSampleBuffer = nullptr;
    size_t frameSize = CMBlockBufferGetDataLength(m_currentBlockBuffer.get());
    CMSampleTimingInfo timing = { CMTimeMake(duration, presentationTime.timescale), presentationTime, presentationTime };
    err = CMSampleBufferCreateReady(kCFAllocatorDefault, m_currentBlockBuffer.get(), this->formatDescription().get(), sampleCount, 1, &timing, 1, &frameSize, &rawSampleBuffer);
    if (err) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMSampleBufferCreateReady failed with error", err);
        return;
    }
    auto sampleBuffer = adoptCF(rawSampleBuffer);

    m_currentBlockBuffer = nullptr;
    m_currentBlockBufferPosition = 0;

    auto attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), true);
    ASSERT(attachmentsArray);
    if (!attachmentsArray) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMSampleBufferGetSampleAttachmentsArray returned NULL");
        return;
    }

    if (!isKey) {
        for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
            CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
            CFDictionarySetValue(attachments, kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
        }
    }

    auto trackID = track.track_uid.value();
    parser().provideMediaData(WTFMove(sampleBuffer), trackID, metadata.position);
#else
    UNUSED_PARAM(presentationTime);
    UNUSED_PARAM(sampleCount);
    UNUSED_PARAM(metadata);
#endif // ENABLE(VP9)
}

webm::Status SourceBufferParserWebM::AudioTrackData::consumeFrameData(webm::Reader& reader, const FrameMetadata& metadata, uint64_t* bytesRemaining, const CMTime& presentationTime, int sampleCount)
{
    ASSERT(sampleCount);

    if (!formatDescription()) {
        if (!track().codec_private.is_present()) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Audio track missing magic cookie");
            return Skip(&reader, bytesRemaining);
        }

        RetainPtr<CMFormatDescriptionRef> formatDescription;
        auto& privateData = track().codec_private.value();
        if (codec() == CodecType::Vorbis)
            formatDescription = createVorbisAudioFormatDescription(privateData.size(), privateData.data());
        else if (codec() == CodecType::Opus)
            formatDescription = createOpusAudioFormatDescription(privateData.size(), privateData.data());

        if (!formatDescription) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Failed to create format description from audio track header");
            return Skip(&reader, bytesRemaining);
        }

        auto streamDescription = CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
        if (!streamDescription) {
            PARSER_LOG_ERROR_IF_POSSIBLE("CMAudioFormatDescriptionGetStreamBasicDescription failed");
            return Skip(&reader, bytesRemaining);
        }
        m_packetDuration = CMTimeMake(streamDescription->mFramesPerPacket, streamDescription->mSampleRate);

        setFormatDescription(WTFMove(formatDescription));
    }

    if (m_packetDescriptions.isEmpty()) {
        m_packetBytesRead = 0;
        m_byteOffset = metadata.position;
        m_samplePresentationTime = presentationTime;
    }

    m_packetData.resize(m_packetBytesRead + metadata.size);
    size_t packetDataOffset = m_packetBytesRead;
    size_t bytesToRead = metadata.size;
    while (bytesToRead) {
        uint64_t bytesRead;
        auto status = reader.Read(bytesToRead, m_packetData.data() + m_packetBytesRead, &bytesRead);
        bytesToRead -= bytesRead;
        *bytesRemaining -= bytesRead;
        m_packetBytesRead += bytesRead;

        // FIXME: We can't yet handle parsing a Frame that doesn't have all its memory available.
        if (status.code == webm::Status::kOkPartial || status.code == webm::Status::kWouldBlock)
            return status;
    }

    m_packetDescriptions.append({ static_cast<int64_t>(packetDataOffset), 0, static_cast<UInt32>(metadata.size) });
    auto sampleDuration = CMTimeGetSeconds(CMTimeSubtract(presentationTime, m_samplePresentationTime)) + CMTimeGetSeconds(m_packetDuration) * sampleCount;
    if (sampleDuration >= m_minimumSampleDuration)
        createSampleBuffer(metadata.position);

    return Skip(&reader, bytesRemaining);
}

void SourceBufferParserWebM::AudioTrackData::createSampleBuffer(Optional<size_t> latestByteRangeOffset)
{
    if (m_packetDescriptions.isEmpty())
        return;

    ASSERT(!m_packetData.isEmpty());

    CMBlockBufferRef blockBuffer = nullptr;
    auto err = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, m_packetData.sizeInBytes(), kCFAllocatorDefault, nullptr, 0, m_packetData.sizeInBytes(), kCMBlockBufferAssureMemoryNowFlag, &blockBuffer);
    if (err) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMBlockBufferCreateWithMemoryBlock failed with %d", err);
        return;
    }
    auto buffer = adoptCF(blockBuffer);

    err = CMBlockBufferReplaceDataBytes(m_packetData.data(), buffer.get(), 0, m_packetData.sizeInBytes());
    if (err) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMBlockBufferReplaceDataBytes failed with %d", err);
        return;
    }

    CMSampleBufferRef rawSampleBuffer = nullptr;
    err = CMAudioSampleBufferCreateReadyWithPacketDescriptions(kCFAllocatorDefault, buffer.get(), formatDescription().get(), m_packetDescriptions.size(), m_samplePresentationTime, m_packetDescriptions.data(), &rawSampleBuffer);
    if (err) {
        PARSER_LOG_ERROR_IF_POSSIBLE("CMAudioSampleBufferCreateWithPacketDescriptions failed with %d", err);
        return;
    }
    auto sampleBuffer = adoptCF(rawSampleBuffer);

    m_packetData.clear();
    m_packetDescriptions.clear();

    auto trackID = track().track_uid.value();
    parser().provideMediaData(WTFMove(sampleBuffer), trackID, latestByteRangeOffset);
}

void SourceBufferParserWebM::flushPendingAudioBuffers()
{
    for (auto& track : m_tracks) {
        if (track->trackType() == SourceBufferParserWebM::TrackData::Type::Audio)
            downcast<AudioTrackData>(track.get()).createSampleBuffer();
    }
}

void SourceBufferParserWebM::setMinimumAudioSampleDuration(float duration)
{
    m_minimumAudioSampleDuration = duration;
}

void SourceBufferParserWebM::setCallOnClientThreadCallback(CallOnClientThreadCallback&& callback)
{
    ASSERT(callback);
    m_callOnClientThreadCallback = WTFMove(callback);
}

const HashSet<String>& SourceBufferParserWebM::supportedVideoCodecs()
{
    static auto codecs = makeNeverDestroyed<HashSet<String>>({ "V_VP8", "V_VP9" });
    return codecs;
}

const HashSet<String>& SourceBufferParserWebM::supportedAudioCodecs()
{
    static auto codecs = makeNeverDestroyed<HashSet<String>>({ "A_VORBIS", "A_OPUS" });
    return codecs;
}

}

#endif // ENABLE(MEDIA_SOURCE)
