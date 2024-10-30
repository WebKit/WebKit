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
#include "CMUtilities.h"
#include "ContentType.h"
#include "InbandTextTrackPrivate.h"
#include "LibWebRTCMacros.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSampleAVFObjC.h"
#include "NotImplemented.h"
#include "PlatformMediaSessionManager.h"
#include "VP9UtilitiesCocoa.h"
#include "VideoTrackPrivateWebM.h"
#include "WebMAudioUtilitiesCocoa.h"
#include <webm/webm_parser.h>
#include <wtf/Algorithms.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StdList.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/darwin/WeakLinking.h>
#include <wtf/spi/darwin/OSVariantSPI.h>

#include "CoreVideoSoftLink.h"

WTF_WEAK_LINK_FORCE_IMPORT(webm::swap);

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

template<typename> struct LogArgument;

template<> struct LogArgument<webm::TrackType> {
    static ASCIILiteral toString(webm::TrackType type)
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
    static ASCIILiteral toString(webm::Id id)
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

} // namespace WTF

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebMParser);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WebMParserTrackData, WebMParser::TrackData);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WebMParserVideoTrackData, WebMParser::VideoTrackData);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WebMParserAudioTrackData, WebMParser::AudioTrackData);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SourceBufferParserWebM);

// FIXME: Remove this once kCMVideoCodecType_VP9 is added to CMFormatDescription.h
constexpr CMVideoCodecType kCMVideoCodecType_VP9 { 'vp09' };

constexpr uint32_t k_us_in_seconds = 1000000000;

static bool isWebmParserAvailable()
{
    return !!webm::swap;
}

using namespace webm;

static Status segmentReadErrorToWebmStatus(SourceBufferParser::Segment::ReadError error)
{
    switch (error) {
    case SourceBufferParser::Segment::ReadError::EndOfFile: return Status(Status::kEndOfFile);
    case SourceBufferParser::Segment::ReadError::FatalError: return Status(Status::Code(WebMParser::ErrorCode::ReaderFailed));
    }
}

typedef WebMParser::SegmentReader WebMParserSegmentReader;

class WebMParser::SegmentReader final : public webm::Reader {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebMParserSegmentReader);
public:
    void appendSegment(SourceBufferParser::Segment&& segment)
    {
        m_data.push_back(WTFMove(segment));
        if (m_currentSegment == m_data.end())
            --m_currentSegment;
    }

    Status Read(std::size_t numToRead, uint8_t* outputBuffer, uint64_t* numActuallyRead) final
    {
        ASSERT(outputBuffer && numActuallyRead);
        if (!numActuallyRead)
            return Status(Status::kNotEnoughMemory);

        *numActuallyRead = 0;
        if (!outputBuffer)
            return Status(Status::kNotEnoughMemory);

        while (numToRead && m_currentSegment != m_data.end()) {
            auto& currentSegment = *m_currentSegment;

            if (m_positionWithinSegment >= currentSegment.size()) {
                advanceToNextSegment();
                continue;
            }

            auto readResult = currentSegment.read({ outputBuffer, numToRead }, m_positionWithinSegment);
            if (!readResult.has_value())
                return segmentReadErrorToWebmStatus(readResult.error());
            auto lastRead = readResult.value();
            m_position += lastRead;
            *numActuallyRead += lastRead;
            m_positionWithinSegment += lastRead;
            numToRead -= lastRead;
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

    Status ReadInto(std::size_t numToRead, SharedBufferBuilder& outputBuffer, uint64_t* numActuallyRead)
    {
        ASSERT(numActuallyRead);
        if (!numActuallyRead)
            return Status(Status::kNotEnoughMemory);

        *numActuallyRead = 0;

        while (numToRead && m_currentSegment != m_data.end()) {
            auto& currentSegment = *m_currentSegment;

            if (m_positionWithinSegment >= currentSegment.size()) {
                advanceToNextSegment();
                continue;
            }
            auto rawBlockBuffer = currentSegment.getData(m_positionWithinSegment, numToRead);
            auto lastRead = rawBlockBuffer->size();
            outputBuffer.append(rawBlockBuffer);
            m_position += lastRead;
            *numActuallyRead += lastRead;
            m_positionWithinSegment += lastRead;
            numToRead -= lastRead;
            if (m_positionWithinSegment == currentSegment.size())
                advanceToNextSegment();
        }
        if (!numToRead)
            return Status(Status::kOkCompleted);
        if (*numActuallyRead)
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

    StdList<SourceBufferParser::Segment> m_data;
    StdList<SourceBufferParser::Segment>::iterator m_currentSegment { m_data.end() };
    size_t m_position { 0 };
    size_t m_positionWithinSegment { 0 };
};

class MediaDescriptionWebM final : public MediaDescription {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaDescriptionWebM);
public:
    static Ref<MediaDescriptionWebM> create(webm::TrackEntry&& track)
    {
        ASSERT(isWebmParserAvailable());
        return adoptRef(*new MediaDescriptionWebM(WTFMove(track)));
    }

    MediaDescriptionWebM(webm::TrackEntry&& track)
        : MediaDescription(extractCodec(track))
        , m_track { WTFMove(track) }
    {
    }

    bool isVideo() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kVideo; }
    bool isAudio() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kAudio; }
    bool isText() const final { return m_track.track_type.is_present() && m_track.track_type.value() == TrackType::kSubtitle; }

    const webm::TrackEntry& track() { return m_track; }

private:
    String extractCodec(const webm::TrackEntry& track) const
    {
        // From: https://www.matroska.org/technical/codec_specs.html
        // "Each Codec ID MUST include a Major Codec ID immediately following the Codec ID Prefix.
        // A Major Codec ID MAY be followed by an OPTIONAL Codec ID Suffix to communicate a refinement
        // of the Major Codec ID. If a Codec ID Suffix is used, then the Codec ID MUST include a forward
        // slash (“/”) as a separator between the Major Codec ID and the Codec ID Suffix. The Major Codec
        // ID MUST be composed of only capital letters (A-Z) and numbers (0-9). The Codec ID Suffix MUST
        // be composed of only capital letters (A-Z), numbers (0-9), underscore (“_”), and forward slash (“/”)."

        if (!track.codec_id.is_present())
            return emptyString();
        StringView codecID { std::span { track.codec_id.value() } };
        if (!codecID.startsWith("V_"_s) && !codecID.startsWith("A_"_s) && !codecID.startsWith("S_"_s))
            return emptyString();

        auto slashLocation = codecID.find('/');
        auto length = slashLocation == notFound ? codecID.length() - 2 : slashLocation - 2;
        return codecID.substring(2, length).convertToASCIILowercase();
    }

    const webm::TrackEntry m_track;
};

std::span<const ASCIILiteral> SourceBufferParserWebM::supportedMIMETypes()
{
#if !(ENABLE(VP9) || ENABLE(VORBIS) || ENABLE(OPUS))
    return { };
#else
    static constexpr std::array types {
#if ENABLE(VP9)
        "video/webm"_s,
#endif
#if ENABLE(VORBIS) || ENABLE(OPUS)
        "audio/webm"_s,
#endif
    };
    return types;
#endif
}

WebMParser::WebMParser(Callback& callback)
    : m_parser(makeUniqueWithoutFastMallocCheck<WebmParser>())
    , m_reader(makeUniqueRef<SegmentReader>())
    , m_callback(callback)
{
}

WebMParser::~WebMParser()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
}

void WebMParser::resetState()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_parser)
        m_parser->DidSeek();
    m_reader->reset();
    m_state = m_initializationSegmentProcessed ? State::ReadingSegment : State::None;
    m_initializationSegment = nullptr;
    m_initializationSegmentEncountered = false;
    m_currentBlock.reset();
    for (auto& track : m_tracks)
        track->reset();
}

void WebMParser::reset()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_reader->reset();
    m_parser->DidSeek();
}

void WebMParser::createByteRangeSamples()
{
    for (auto& track : m_tracks)
        track->createByteRangeSamples();
    m_createByteRangeSamples = true;
}

ExceptionOr<int> WebMParser::parse(SourceBufferParser::Segment&& segment)
{
    if (!m_parser)
        return Exception { ExceptionCode::InvalidStateError };

    m_reader->appendSegment(WTFMove(segment));

    while (true) {
        m_status = m_parser->Feed(this, &m_reader);
        if (m_status.ok() || m_status.code == Status::kEndOfFile || m_status.code == Status::kWouldBlock) {
            m_reader->reclaimSegments();

            // We always keep one sample queued in order to calculate the video sample's time, return it now.
            flushPendingVideoSamples();
            return 0;
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
        m_rewindToPosition = std::nullopt;
        m_parser->DidSeek();
        m_state = State::None;
        continue;
    }

    return m_status.code;
}

void WebMParser::setLogger(const Logger& newLogger, uint64_t newLogIdentifier)
{
    m_logger = &newLogger;
    m_logIdentifier = newLogIdentifier;
    ALWAYS_LOG(LOGIDENTIFIER);
}

WTFLogChannel& WebMParser::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}

void WebMParser::invalidate()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_parser = nullptr;
    m_tracks.clear();
    m_initializationSegment = nullptr;
    m_currentBlock.reset();
}

auto WebMParser::trackDataForTrackNumber(uint64_t trackNumber) -> TrackData*
{
    for (auto& track : m_tracks) {
        if (track->track().track_number.is_present() && track->track().track_number.value() == trackNumber)
            return &track;
    }
    return nullptr;
}

Status WebMParser::OnElementBegin(const ElementMetadata& metadata, Action* action)
{
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

    // Apply some sanity check; libwebm::StringParser will read the content into a std::string and ByteParser into a std::vector
    std::optional<size_t> maxElementSizeAllowed;
    switch (metadata.id) {
    case Id::kChapterStringUid:
    case Id::kChapString:
    case Id::kChapLanguage:
    case Id::kChapCountry:
    case Id::kDocType:
    case Id::kTitle:
    case Id::kMuxingApp:
    case Id::kWritingApp:
    case Id::kTagName:
    case Id::kTagLanguage:
    case Id::kTagString:
    case Id::kTargetType:
    case Id::kName:
    case Id::kLanguage:
    case Id::kCodecId:
    case Id::kCodecName:
        maxElementSizeAllowed = 1 * 1024 * 1024; // 1MiB
        break;
    case Id::kBlockAdditional:
    case Id::kContentEncKeyId:
    case Id::kProjectionPrivate:
    case Id::kTagBinary:
        maxElementSizeAllowed = 16 * 1024 * 1024; // 16MiB
        break;
    default:
        break;
    }
    if (maxElementSizeAllowed && metadata.size >= *maxElementSizeAllowed)
        return Status(Status::kNotEnoughMemory);

    return Status(Status::kOkCompleted);
}

Status WebMParser::OnElementEnd(const ElementMetadata& metadata)
{
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
        if (!m_keyIds.isEmpty() && !m_callback.canDecrypt()) {
            ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered encrypted content without an key request callback");
            return Status(Status::Code(ErrorCode::ContentEncrypted));
        }

        if (m_initializationSegmentEncountered)
            m_callback.parsedInitializationData(WTFMove(*m_initializationSegment));
        m_initializationSegmentEncountered = false;
        m_initializationSegment = nullptr;
        m_initializationSegmentProcessed = true;

        if (!m_keyIds.isEmpty()) {
            for (auto& keyIdPair : m_keyIds)
                m_callback.contentKeyRequestInitializationDataForTrackID(WTFMove(keyIdPair.second), keyIdPair.first);
        }
        m_keyIds.clear();
    }

    return Status(Status::kOkCompleted);
}

Status WebMParser::OnEbml(const ElementMetadata&, const Ebml& ebml)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (ebml.doc_type.is_present() && ebml.doc_type.value().compare("webm"))
        return Status(Status::Code(ErrorCode::InvalidDocType));

    m_initializationSegmentEncountered = true;
    m_initializationSegment = makeUniqueWithoutFastMallocCheck<SourceBufferParser::InitializationSegment>();
    // TODO: Setting this to false here, will prevent adding a new media segment should a
    // partial init segment be encountered after a call to sourceBuffer.abort().
    // It's probably fine as no-one in their right mind should send partial init segment only
    // to immediately abort it. We do it this way mostly to avoid getting into a rabbit hole
    // of ensuring that libwebm does something sane with rubbish input.
    m_initializationSegmentProcessed = false;

    // Reset the tracks as new tracks _must_ replace old ones. Otherwise, new samples may refer to old
    // track information.
    m_tracks.clear();

    return Status(Status::kOkCompleted);
}

Status WebMParser::OnSegmentBegin(const ElementMetadata&, Action* action)
{
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

Status WebMParser::OnInfo(const ElementMetadata&, const Info& info)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (!m_initializationSegmentEncountered || !m_initializationSegment) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered Info outside Segment");
        return Status(Status::Code(ErrorCode::InvalidInitSegment));
    }

    auto timecodeScale = info.timecode_scale.is_present() ? info.timecode_scale.value() : 1000000;
    m_timescale = k_us_in_seconds / timecodeScale;
    m_initializationSegment->duration = info.duration.is_present() ? MediaTime(info.duration.value(), m_timescale) : MediaTime::indefiniteTime();

    return Status(Status::kOkCompleted);
}

Status WebMParser::OnClusterBegin(const ElementMetadata&, const Cluster& cluster, Action* action)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    if (cluster.timecode.is_present())
        m_currentTimecode = cluster.timecode.value();

    *action = Action::kRead;

    return Status(Status::kOkCompleted);
}

Status WebMParser::OnTrackEntry(const ElementMetadata&, const TrackEntry& trackEntry)
{
    if (!trackEntry.track_type.is_present() || !trackEntry.codec_id.is_present())
        return Status(Status::kOkCompleted);

    auto trackType = trackEntry.track_type.value();
    String codecId = std::span { trackEntry.codec_id.value() };

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, trackType, ", codec ", codecId);

    if (trackType == TrackType::kVideo && !isSupportedVideoCodec(codecId)) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Encountered unsupported video codec ID ", codecId);
        return Status(Status::Code(ErrorCode::UnsupportedVideoCodec));
    }

    if (trackType == TrackType::kAudio && !isSupportedAudioCodec(codecId)) {
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
            m_keyIds.append(std::make_pair(trackEntry.track_uid.value(), SharedBuffer::create(std::span { keyId })));
        }
    }

    StringView codecString { std::span { trackEntry.codec_id.value() } };
    auto track = [&]() -> UniqueRef<TrackData> {
#if ENABLE(VP9)
        if (codecString == "V_VP9"_s && isVP9DecoderAvailable())
            return VideoTrackData::create(CodecType::VP9, trackEntry, *this);
        if (codecString == "V_VP8"_s && isVP8DecoderAvailable())
            return VideoTrackData::create(CodecType::VP8, trackEntry, *this);
#endif

#if ENABLE(VORBIS)
        if (codecString == "A_VORBIS"_s && isVorbisDecoderAvailable())
            return AudioTrackData::create(CodecType::Vorbis, trackEntry, *this);
#endif

#if ENABLE(OPUS)
        if (codecString == "A_OPUS"_s && isOpusDecoderAvailable())
            return AudioTrackData::create(CodecType::Opus, trackEntry, *this);
#endif
        return TrackData::create(CodecType::Unsupported, trackEntry, *this);
    }();

    if (m_createByteRangeSamples)
        track->createByteRangeSamples();

    m_tracks.append(WTFMove(track));
    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnBlockBegin(const ElementMetadata&, const Block& block, Action* action)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;

    m_currentBlock = std::make_optional<BlockVariant>(Block(block));

    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnBlockEnd(const ElementMetadata&, const Block&)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_currentBlock = std::nullopt;

    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnSimpleBlockBegin(const ElementMetadata&, const SimpleBlock& block, Action* action)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;

    m_currentBlock = std::make_optional<BlockVariant>(SimpleBlock(block));
    m_currentDuration = MediaTime::zeroTime();

    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnSimpleBlockEnd(const ElementMetadata&, const SimpleBlock&)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_currentBlock = std::nullopt;
    m_currentDuration = MediaTime::zeroTime();

    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnBlockGroupBegin(const webm::ElementMetadata&, webm::Action* action)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    *action = Action::kRead;
    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnBlockGroupEnd(const webm::ElementMetadata&, const webm::BlockGroup& blockGroup)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (blockGroup.block.is_present() && blockGroup.discard_padding.is_present()) {
        auto trackNumber = blockGroup.block.value().track_number;
        auto* trackData = trackDataForTrackNumber(trackNumber);
        if (!trackData) {
            ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Ignoring unknown track number ", trackNumber);
            return Status(Status::kOkCompleted);
        }
        if (trackData->track().track_uid.is_present() && blockGroup.discard_padding.value() > 0)
            m_callback.parsedTrimmingData(trackData->track().track_uid.value(), MediaTime(blockGroup.discard_padding.value(), k_us_in_seconds));
    }
    return Status(Status::kOkCompleted);
}

webm::Status WebMParser::OnFrame(const FrameMetadata& metadata, Reader* reader, uint64_t* bytesRemaining)
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

    auto result = trackData->consumeFrameData(*reader, metadata, bytesRemaining, MediaTime(block->timecode + m_currentTimecode, m_timescale) + m_currentDuration);
    if (std::holds_alternative<webm::Status>(result))
        return std::get<webm::Status>(result);
    m_currentDuration += std::get<MediaTime>(result);
    return Status(Status::kOkCompleted);
}


#define PARSER_LOG_ERROR_IF_POSSIBLE(...) if (parser().loggerPtr()) parser().loggerPtr()->error(logChannel(), Logger::LogSiteIdentifier(logClassName(), __func__, parser().logIdentifier()), __VA_ARGS__)

RefPtr<SharedBuffer> WebMParser::TrackData::contiguousCompleteBlockBuffer(size_t offset, size_t length) const
{
    if (offset + length > m_completeBlockBuffer->size())
        return nullptr;
    return m_completeBlockBuffer->getContiguousData(offset, length);
}

webm::Status WebMParser::TrackData::readFrameData(webm::Reader& reader, const webm::FrameMetadata& metadata, uint64_t* bytesRemaining)
{
    if (m_completePacketSize && *m_completePacketSize != metadata.size) {
        // The packet's metadata doesn't match the currently pending complete packet; restart.
        ASSERT_NOT_REACHED_WITH_MESSAGE("TrackData::readFrameData: webm in nonsensical state");
        reset();
    }

    if (!m_completePacketSize)
        m_completePacketSize = metadata.size;

    while (*bytesRemaining) {
        uint64_t bytesRead;
        auto status = static_cast<WebMParser::SegmentReader&>(reader).ReadInto(*bytesRemaining, m_currentBlockBuffer, &bytesRead);
        *bytesRemaining -= bytesRead;
        m_partialBytesRead += bytesRead;

        if (!status.completed_ok())
            return status;
    }

    ASSERT(m_partialBytesRead <= *m_completePacketSize);
    if (m_partialBytesRead < *m_completePacketSize)
        return webm::Status(webm::Status::kOkPartial);

    m_completeBlockBuffer = m_currentBlockBuffer.take();
    if (m_useByteRange)
        m_completeFrameData = MediaSample::ByteRange { metadata.position, metadata.size };
    else
        m_completeFrameData = Ref { *m_completeBlockBuffer };

    m_completePacketSize = std::nullopt;
    m_partialBytesRead = 0;

    return webm::Status(webm::Status::kOkCompleted);
}

void WebMParser::flushPendingVideoSamples()
{
    for (auto& track : m_tracks) {
        if (auto* videoTrack = dynamicDowncast<WebMParser::VideoTrackData>(track.get()))
            videoTrack->flushPendingSamples();
    }
}

void WebMParser::VideoTrackData::resetCompletedFramesState()
{
    ASSERT(!m_pendingMediaSamples.size());
    TrackData::resetCompletedFramesState();
}

WebMParser::ConsumeFrameDataResult WebMParser::VideoTrackData::consumeFrameData(webm::Reader& reader, const FrameMetadata& metadata, uint64_t* bytesRemaining, const MediaTime& presentationTime)
{
#if ENABLE(VP9)
    auto status = readFrameData(reader, metadata, bytesRemaining);
    if (!status.completed_ok())
        return status;

    constexpr size_t maxHeaderSize = 32; // The maximum length of a VP9 uncompressed header is 144 bits and 11 bytes for VP8. Round high.
    size_t segmentHeaderLength = std::min<size_t>(maxHeaderSize, metadata.size);
    auto contiguousBuffer = contiguousCompleteBlockBuffer(0, segmentHeaderLength);
    if (!contiguousBuffer) {
        PARSER_LOG_ERROR_IF_POSSIBLE("VideoTrackData::consumeFrameData failed to create contiguous data block");
        return Skip(&reader, bytesRemaining);
    }
    auto segmentHeaderData = contiguousBuffer->span().first(segmentHeaderLength);

    bool isKey = false;
    if (codec() == CodecType::VP9) {
        if (!m_headerParser.ParseUncompressedHeader(segmentHeaderData.data(), segmentHeaderData.size()))
            return Skip(&reader, bytesRemaining);

        if (m_headerParser.key()) {
            isKey = true;
            setFormatDescription(createVideoInfoFromVP9HeaderParser(m_headerParser, track().video.value()));
        }
    } else if (codec() == CodecType::VP8) {
        auto header = parseVP8FrameHeader(segmentHeaderData);
        if (header && header->keyframe) {
            isKey = true;
            setFormatDescription(createVideoInfoFromVP8Header(*header, track().video.value()));
        }
    }

    processPendingMediaSamples(presentationTime);

    if (formatDescription() && (!m_trackInfo || *formatDescription() != *m_trackInfo)) {
        m_trackInfo = formatDescription();
        m_processedMediaSamples.setInfo(formatDescription());
        parser().formatDescriptionChangedForTrackData(*this);
    }

    m_pendingMediaSamples.append({ presentationTime, presentationTime, MediaTime::indefiniteTime(), MediaTime::zeroTime(), WTFMove(m_completeFrameData), isKey ? MediaSample::SampleFlags::IsSync : MediaSample::SampleFlags::None });

    ASSERT(!*bytesRemaining);
    return webm::Status(webm::Status::kOkCompleted);
#else
    UNUSED_PARAM(presentationTime);
    UNUSED_PARAM(sampleCount);
    UNUSED_PARAM(metadata);
    return Skip(&reader, bytesRemaining);
#endif
}

void WebMParser::VideoTrackData::processPendingMediaSamples(const MediaTime& presentationTime)
{
    // WebM container doesn't contain information about duration; the end time of a frame is the start time of the next.
    // Some frames however may have a duration of 0 which typically indicates that they should be decoded but not displayed.
    // We group all the samples with the same presentation timestamp within the same final MediaSampleBlock.

    if (!m_pendingMediaSamples.size())
        return;
    auto& lastSample = m_pendingMediaSamples.last();
    lastSample.duration = presentationTime - lastSample.presentationTime;
    if (presentationTime == lastSample.presentationTime)
        return;

    MediaTime timeOffset;
    MediaTime durationOffset;
    while (m_pendingMediaSamples.size()) {
        auto sample = m_pendingMediaSamples.takeFirst();
        if (timeOffset) {
            sample.presentationTime += timeOffset;
            sample.decodeTime += timeOffset;
            auto usableOffset = std::min(durationOffset, sample.duration);
            sample.duration -= usableOffset;
            durationOffset -= usableOffset;
        }
        // The MediaFormatReader is unable to deal with samples having a duration of 0.
        // We instead set those samples to have a 1us duration and shift the presentation/decode time
        // of the following samples in the block by the same offset.
        if (!sample.duration) {
            sample.duration = MediaTime(1, k_us_in_seconds);
            timeOffset += sample.duration;
            durationOffset += sample.duration;
        }
        m_processedMediaSamples.append(WTFMove(sample));
    }
    m_lastDuration = m_processedMediaSamples.last().duration;
    m_lastPresentationTime = presentationTime;
    if (!m_processedMediaSamples.info())
        m_processedMediaSamples.setInfo(formatDescription());
    drainPendingSamples();
}

void WebMParser::VideoTrackData::flushPendingSamples()
{
    // We haven't been able to calculate the duration of the last sample as none will follow.
    // We set its duration to the track's default duration, or if not known the time of the last sample processed.
    if (!m_pendingMediaSamples.size())
        return;
    auto track = this->track();

    MediaTime presentationTime = m_lastPresentationTime ? *m_lastPresentationTime : m_pendingMediaSamples.first().presentationTime;
    MediaTime duration;
    if (track.default_duration.is_present())
        duration = MediaTime(track.default_duration.value() * presentationTime.timeScale() / k_us_in_seconds, presentationTime.timeScale());
    else if (m_lastDuration)
        duration = *m_lastDuration;
    processPendingMediaSamples(presentationTime + duration);
    m_lastPresentationTime.reset();
}

WebMParser::AudioTrackData::AudioTrackData(CodecType codecType, const webm::TrackEntry& trackEntry, WebMParser& parser)
    : TrackData { codecType, trackEntry, TrackInfo::TrackType::Audio, parser }
{
    // https://wiki.xiph.org/MatroskaOpus#Element_Additions
    // CodecDelay is a new unsigned integer element added to the TrackEntry element.
    // The value is the number of nanoseconds that must be discarded, for that stream, from the
    // start of that stream. The value is also the number of nanoseconds that all encoded
    // timestamps for that stream must be shifted to get the presentation timestamp.
    // (This will fix Vorbis encoding as well.)
    if (trackEntry.codec_delay.is_present()) {
        constexpr uint32_t k_us_in_seconds = 1000000000;
        m_remainingTrimDuration = MediaTime(trackEntry.codec_delay.value(), k_us_in_seconds);
        m_presentationTimeShift = -m_remainingTrimDuration;
    }
}

WebMParser::AudioTrackData::~AudioTrackData() = default;

void WebMParser::AudioTrackData::resetCompletedFramesState()
{
    mNumFramesInCompleteBlock = 0;
    TrackData::resetCompletedFramesState();
}

WebMParser::ConsumeFrameDataResult WebMParser::AudioTrackData::consumeFrameData(webm::Reader& reader, const FrameMetadata& metadata, uint64_t* bytesRemaining, const MediaTime& presentationTime)
{
    auto status = readFrameData(reader, metadata, bytesRemaining);
    if (!status.completed_ok())
        return status;

    if (!formatDescription()) {
        if (!track().codec_private.is_present()) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Audio track missing magic cookie");
            return Skip(&reader, bytesRemaining);
        }

        RefPtr<AudioInfo> formatDescription;
        auto& privateData = track().codec_private.value();
        if (codec() == CodecType::Vorbis)
            formatDescription = createVorbisAudioInfo(privateData);
        else if (codec() == CodecType::Opus) {
            auto contiguousBuffer = contiguousCompleteBlockBuffer(0, kOpusMinimumFrameDataSize);
            if (!contiguousBuffer) {
                PARSER_LOG_ERROR_IF_POSSIBLE("AudioTrackData::consumeFrameData: unable to create contiguous data block");
                return Skip(&reader, bytesRemaining);
            }
            OpusCookieContents cookieContents;
            if (!parseOpusPrivateData(std::span { privateData }, *contiguousBuffer, cookieContents)) {
                PARSER_LOG_ERROR_IF_POSSIBLE("Failed to parse Opus private data");
                return Skip(&reader, bytesRemaining);
            }
#if !HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
            if (!cookieContents.framesPerPacket) {
                PARSER_LOG_ERROR_IF_POSSIBLE("Opus private data indicates 0 frames per packet; bailing");
                return Skip(&reader, bytesRemaining);
            }
            m_framesPerPacket = cookieContents.framesPerPacket;
            m_frameDuration = cookieContents.frameDuration;
#endif
            formatDescription = createOpusAudioInfo(cookieContents);
        }

        if (!formatDescription) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Failed to create AudioInfo from audio track header");
            return Skip(&reader, bytesRemaining);
        }

        m_packetDurationParser = makeUnique<PacketDurationParser>(*formatDescription);
        if (!m_packetDurationParser->isValid()) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Failed to create PacketDurationParser from audio track header");
            return Skip(&reader, bytesRemaining);
        }
        setFormatDescription(formatDescription.releaseNonNull());
    }
#if !HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    else if (codec() == CodecType::Opus) {
        // Opus technically allows the frame duration and frames-per-packet values to change from packet to packet.
        // Prior rdar://71347713 CoreMedia opus decoder didn't support those, so throw an error when
        // that kind of variability is encountered.
        OpusCookieContents cookieContents;
        auto& privateData = track().codec_private.value();
        auto contiguousBuffer = contiguousCompleteBlockBuffer(0, kOpusMinimumFrameDataSize);
        if (!contiguousBuffer) {
            PARSER_LOG_ERROR_IF_POSSIBLE("AudioTrackData::consumeFrameData: unable to create contiguous data block");
            return Skip(&reader, bytesRemaining);
        }
        if (!parseOpusPrivateData(std::span { privateData }, *contiguousBuffer, cookieContents)
            || cookieContents.framesPerPacket != m_framesPerPacket
            || cookieContents.frameDuration != m_frameDuration) {
            PARSER_LOG_ERROR_IF_POSSIBLE("Opus frames-per-packet changed within a track; error");
            return Status(Status::Code(ErrorCode::VariableFrameDuration));
        }
    }
#endif

    auto contiguousBuffer = contiguousCompleteBlockBuffer(0, codec() == CodecType::Opus ? kOpusMinimumFrameDataSize : kVorbisMinimumFrameDataSize);
    if (!contiguousBuffer) {
        PARSER_LOG_ERROR_IF_POSSIBLE("AudioTrackData::consumeFrameData: unable to create contiguous data block");
        return Skip(&reader, bytesRemaining);
    }

    bool shouldDrain = !!m_processedMediaSamples.info();
    if (formatDescription() && (!m_trackInfo || *formatDescription() != *m_trackInfo)) {
        if (shouldDrain)
            drainPendingSamples();
        m_trackInfo = formatDescription();
        m_processedMediaSamples.setInfo(formatDescription());
        parser().formatDescriptionChangedForTrackData(*this);
    }

    MediaTime packetDuration = MediaTime(m_packetDurationParser->framesInPacket(*contiguousBuffer), downcast<AudioInfo>(formatDescription())->rate);
    auto trimDuration = MediaTime::zeroTime();
    MediaTime localPresentationTime = presentationTime;
    if (m_remainingTrimDuration.isFinite() && m_remainingTrimDuration > MediaTime::zeroTime()) {
        if (m_remainingTrimDuration < packetDuration)
            std::swap(trimDuration, m_remainingTrimDuration);
        else {
            m_remainingTrimDuration -= packetDuration;
            trimDuration = packetDuration;
        }
    }

    ASSERT(m_presentationTimeShift.isFinite());
    if (m_presentationTimeShift != MediaTime::zeroTime())
        localPresentationTime += m_presentationTimeShift;

    if (m_lastPresentationEndTime.isValid()) {
        MediaTime discontinuityGap = localPresentationTime - m_lastPresentationEndTime;
        // ATSC IS-191: Relative Timing of Sound and Vision for Broadcast Operations
        // "The sound program should never lead the video program by more than
        // 15 milliseconds, and should never lag the video program by more than
        // 45 milliseconds."
        // By collapsing gaps between samples, we effectively advance the timing
        // of the subsequent samples, which may result in the sound content leading
        // the video content. With a reasonable assumption that the media file starts
        // with A/V content in perfect sync, we can advance samples up to 15 ms.
        // Any gaps larger than that amount must have a discontiuity in order to bring
        // the audio and video presenatations into sync.
        constexpr MediaTime maximumAllowableDiscontinuity = MediaTime(15, 1000);
        if (discontinuityGap > MediaTime::zeroTime() && discontinuityGap < maximumAllowableDiscontinuity)
            localPresentationTime -= discontinuityGap;
    }

    m_lastPresentationEndTime = localPresentationTime + packetDuration;

    m_processedMediaSamples.append({ localPresentationTime, MediaTime::invalidTime(), packetDuration, trimDuration, WTFMove(m_completeFrameData), MediaSample::SampleFlags::IsSync });

    drainPendingSamples();

    ASSERT(!*bytesRemaining);
    return packetDuration;
}


bool WebMParser::isSupportedVideoCodec(StringView name)
{
    return name == "V_VP8"_s || name == "V_VP9"_s;
}

bool WebMParser::isSupportedAudioCodec(StringView name)
{
    return name == "A_VORBIS"_s || name == "A_OPUS"_s;
}

SourceBufferParserWebM::SourceBufferParserWebM()
    : m_parser(*this)
{
}

SourceBufferParserWebM::~SourceBufferParserWebM()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
}

MediaPlayerEnums::SupportsType SourceBufferParserWebM::isContentTypeSupported(const ContentType& type)
{
#if ENABLE(VP9) || ENABLE(VORBIS) || ENABLE(OPUS)
    if (!isWebmParserAvailable())
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    auto containerType = type.containerType();
    bool isAudioContainerType = equalLettersIgnoringASCIICase(containerType, "audio/webm"_s);
    bool isVideoContainerType = equalLettersIgnoringASCIICase(containerType, "video/webm"_s);
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
    isAnyCodecAvailable |= isVP8DecoderAvailable();
#endif

    if (!isAnyCodecAvailable)
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    auto codecs = type.codecs();
    if (codecs.isEmpty())
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    for (auto& codec : codecs) {
#if ENABLE(VP9)
        bool isVP9 = codec.startsWith("vp09"_s) || equal(codec, "vp9"_s) || equal(codec, "vp9.0"_s);
        bool isVP8 = codec.startsWith("vp08"_s) || equal(codec, "vp8"_s) || equal(codec, "vp8.0"_s);
        if (isVP9 || isVP8) {

            if (isVP9 && !isVP9DecoderAvailable())
                return MediaPlayerEnums::SupportsType::IsNotSupported;
            if (isVP8 && !isVP8DecoderAvailable())
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
        if (codec == "vorbis"_s) {
            if (!isVorbisDecoderAvailable())
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            continue;
        }
#endif // ENABLE(VORBIS)

#if ENABLE(OPUS)
        if (codec == "opus"_s) {
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

bool SourceBufferParserWebM::isAvailable()
{
    return isWebmParserAvailable();
}

RefPtr<SourceBufferParserWebM> SourceBufferParserWebM::create()
{
    return isAvailable() ? adoptRef(new SourceBufferParserWebM()) : nullptr;
}

void WebMParser::provideMediaData(MediaSamplesBlock&& samples)
{
    m_callback.parsedMediaData(WTFMove(samples));
}

void WebMParser::formatDescriptionChangedForTrackData(TrackData& trackData)
{
    if (auto formatDescription = trackData.formatDescription())
        m_callback.formatDescriptionChangedForTrackID(*formatDescription, trackData.track().track_uid.value());
}

void SourceBufferParserWebM::parsedInitializationData(InitializationSegment&& initializationSegment)
{
    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, initializationSegment = WTFMove(initializationSegment)]() mutable {
        if (m_didParseInitializationDataCallback)
            m_didParseInitializationDataCallback(WTFMove(initializationSegment));
    });
}

void SourceBufferParserWebM::parsedMediaData(MediaSamplesBlock&& samplesBlock)
{
    if (!samplesBlock.info()) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "No TrackInfo set");
        return;
    }

    RetainPtr<CMFormatDescriptionRef> formatDescription;
    if (samplesBlock.isVideo()) {
        if (m_videoInfo != samplesBlock.info()) {
            m_videoInfo = samplesBlock.info();
            m_videoFormatDescription = createFormatDescriptionFromTrackInfo(*samplesBlock.info());
        }
        formatDescription = m_videoFormatDescription;
    } else {
        if (m_audioInfo != samplesBlock.info()) {
            flushPendingAudioSamples();
            m_audioDiscontinuity = true;
            m_audioFormatDescription = createFormatDescriptionFromTrackInfo(*samplesBlock.info());
            m_audioInfo = samplesBlock.info();
        }
        formatDescription = m_audioFormatDescription;
    }

    if (samplesBlock.isVideo()) {
        returnSamples(WTFMove(samplesBlock), m_videoFormatDescription.get());
        return;
    }

    // Pack audio if needed.
    if (m_queuedAudioSamples.size()) {
        auto& lastSample = m_queuedAudioSamples.last();
        if (lastSample.duration + lastSample.presentationTime != samplesBlock.first().presentationTime)
            flushPendingAudioSamples();
    }
    for (auto& sample : samplesBlock)
        m_queuedAudioDuration += sample.duration;
    m_queuedAudioSamples.append(WTFMove(samplesBlock));
    if (m_queuedAudioDuration < m_minimumAudioSampleDuration)
        return;
    flushPendingAudioSamples();
}

void SourceBufferParserWebM::returnSamples(MediaSamplesBlock&& block, CMFormatDescriptionRef description)
{
    if (block.isEmpty())
        return;

    auto expectedBuffer = toCMSampleBuffer(WTFMove(block), description);
    if (!expectedBuffer) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "toCMSampleBuffer error:", expectedBuffer.error().data());
        return;
    }

    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, trackID = block.info()->trackID, sampleBuffer = WTFMove(expectedBuffer.value())] () mutable {
        if (!m_didProvideMediaDataCallback)
            return;

        auto mediaSample = MediaSampleAVFObjC::create(sampleBuffer.get(), trackID);

        m_didProvideMediaDataCallback(WTFMove(mediaSample), trackID, emptyString());
    });
}

void SourceBufferParserWebM::parsedTrimmingData(uint64_t trackID, const MediaTime& padding)
{
    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, trackID, padding] () {
        if (m_didParseTrimmingDataCallback)
            m_didParseTrimmingDataCallback(trackID, padding);
    });
}

void SourceBufferParserWebM::contentKeyRequestInitializationDataForTrackID(Ref<SharedBuffer>&& keyID, uint64_t trackID)
{
    if (m_didProvideContentKeyRequestInitializationDataForTrackIDCallback)
        m_didProvideContentKeyRequestInitializationDataForTrackIDCallback(WTFMove(keyID), trackID);
}

void SourceBufferParserWebM::formatDescriptionChangedForTrackID(Ref<TrackInfo>&& formatDescription, uint64_t trackID)
{
    m_callOnClientThreadCallback([this, protectedThis = Ref { *this }, formatDescription = WTFMove(formatDescription), trackID]() mutable {
        if (m_didUpdateFormatDescriptionForTrackIDCallback)
            m_didUpdateFormatDescriptionForTrackIDCallback(WTFMove(formatDescription), trackID);
    });
}

void SourceBufferParserWebM::flushPendingAudioSamples()
{
    if (!m_audioFormatDescription)
        return;
    ASSERT(m_audioInfo);
    m_queuedAudioSamples.setInfo(m_audioInfo.copyRef());
    m_queuedAudioSamples.setDiscontinuity(m_audioDiscontinuity);
    m_audioDiscontinuity = false;
    returnSamples(WTFMove(m_queuedAudioSamples), m_audioFormatDescription.get());

    m_queuedAudioSamples = { };
    m_queuedAudioDuration = { };
}

Expected<void, PlatformMediaError> SourceBufferParserWebM::appendData(Segment&& segment, AppendFlags appendFlags)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "flags(", appendFlags == AppendFlags::Discontinuity ? "Discontinuity" : "", "), size(", segment.size(), ")");

    if (appendFlags == AppendFlags::Discontinuity) {
        m_parser.reset();
        m_audioDiscontinuity = true;
    }

    auto result = m_parser.parse(WTFMove(segment));
    if (result.hasException() || result.returnValue())
        return makeUnexpected(PlatformMediaError::ParsingError);

    // Audio tracks are grouped into meta-samples of a duration no more than m_minimumSampleDuration.
    // But at the end of a file, no more audio data may be incoming, so flush and emit any pending
    // audio buffers.
    flushPendingAudioSamples();

    return { };
}

void SourceBufferParserWebM::flushPendingMediaData()
{
}

void SourceBufferParserWebM::invalidate()
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_parser.invalidate();
}

void SourceBufferParserWebM::setLogger(const Logger& newLogger, uint64_t newLogIdentifier)
{
    m_logger = &newLogger;
    m_logIdentifier = newLogIdentifier;
    ALWAYS_LOG(LOGIDENTIFIER);

    m_parser.setLogger(newLogger, newLogIdentifier);
}

WTFLogChannel& SourceBufferParserWebM::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}

void SourceBufferParserWebM::setMinimumAudioSampleDuration(float duration)
{
    INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, duration);
    m_minimumAudioSampleDuration = MediaTime::createWithFloat(duration);
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MEDIA_SOURCE)
