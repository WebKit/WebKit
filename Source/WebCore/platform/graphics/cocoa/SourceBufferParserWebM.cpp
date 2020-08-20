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
#include "CoreVideoSoftLink.h"
#include "InbandTextTrackPrivate.h"
#include "MediaDescription.h"
#include "MediaSampleAVFObjC.h"
#include "NotImplemented.h"
#include "RuntimeEnabledFeatures.h"
#include "SharedBuffer.h"
#include "VP9UtilitiesCocoa.h"
#include "VideoTrackPrivateWebM.h"
#include "VideoToolboxSoftLink.h"
#include <JavaScriptCore/DataView.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <webm/webm_parser.h>
#include <wtf/Algorithms.h>
#include <wtf/Deque.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(webm::swap);

namespace WebCore {

// FIXME: Remove this once kCMVideoCodecType_VP9 is added to CMFormatDescription.h
constexpr CMVideoCodecType kCMVideoCodecType_VP9 { 'vp09' };

using namespace PAL;

static bool isWebmParserAvailable()
{
    return !!webm::swap && RuntimeEnabledFeatures::sharedFeatures().webMParserEnabled();
}

using namespace webm;

class SourceBufferParserWebM::StreamingVectorReader final : public webm::Reader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Segment = Vector<unsigned char>;
    void appendSegment(Segment&& segment)
    {
        m_data.append(WTFMove(segment));
    }

    Status Read(std::size_t numToRead, uint8_t* outputBuffer, uint64_t* numActuallyRead) final
    {
        ASSERT(outputBuffer && numActuallyRead);
        if (!outputBuffer || !numActuallyRead)
            return Status(Status::kNotEnoughMemory);

        *numActuallyRead = 0;
        while (!m_data.isEmpty()) {
            auto& currentSegment = m_data.first();

            if (m_positionWithinSegment >= currentSegment.size()) {
                advanceToNextSegment();
                continue;
            }

            size_t numAvailable = currentSegment.size() - m_positionWithinSegment;
            if (numToRead < numAvailable) {
                memcpy(outputBuffer, currentSegment.data() + m_positionWithinSegment, numToRead);
                *numActuallyRead += numToRead;
                m_position += numToRead;
                m_positionWithinSegment += numToRead;
                return Status(Status::kOkCompleted);
            }

            memcpy(outputBuffer, currentSegment.data() + m_positionWithinSegment, numAvailable);
            *numActuallyRead += numAvailable;
            m_position += numAvailable;
            m_positionWithinSegment += numAvailable;
            numToRead -= numAvailable;
            advanceToNextSegment();
            continue;
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
        while (!m_data.isEmpty()) {
            auto& currentSegment = m_data.first();

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
    }

private:
    void advanceToNextSegment()
    {
        ASSERT(!m_data.isEmpty());
        if (m_data.isEmpty())
            return;

        ASSERT(m_positionWithinSegment == m_data.first().size());

        m_data.removeFirst();
        m_positionWithinSegment = 0;
    }

    Deque<Segment> m_data;
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

MediaPlayerEnums::SupportsType SourceBufferParserWebM::isContentTypeSupported(const ContentType& type)
{
    if (!isWebmParserAvailable())
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    if (!WTF::equalIgnoringASCIICase(type.containerType(), "audio/webm")
        && !WTF::equalIgnoringASCIICase(type.containerType(), "video/webm"))
        return MediaPlayerEnums::SupportsType::IsNotSupported;

    String codecsParameter = type.parameter(ContentType::codecsParameter());
    if (!codecsParameter)
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    auto splitResults = StringView(codecsParameter).split(',');
    for (auto split : splitResults) {
        if (split.startsWith("vp09")) {
            auto codecParameters = parseVPCodecParameters(split);
            if (!codecParameters)
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            if (!isVPCodecConfigurationRecordSupported(*codecParameters))
                return MediaPlayerEnums::SupportsType::IsNotSupported;

            continue;
        }
        // FIXME: Add Opus Support
        // FIXME: Add Vorbis Support
        return MediaPlayerEnums::SupportsType::IsNotSupported;
    }
    return MediaPlayerEnums::SupportsType::IsSupported;
}

SourceBufferParserWebM::SourceBufferParserWebM()
    : m_reader(WTF::makeUniqueRef<StreamingVectorReader>())
{
    if (isWebmParserAvailable())
        m_parser = WTF::makeUniqueWithoutFastMallocCheck<WebmParser>();
}

SourceBufferParserWebM::~SourceBufferParserWebM() = default;

void SourceBufferParserWebM::appendData(Vector<unsigned char>&& buffer, AppendFlags appendFlags)
{
    if (!m_parser)
        return;

    if (appendFlags == AppendFlags::Discontinuity) {
        m_reader->reset();
        m_parser->DidSeek();
    }
    m_reader->appendSegment(WTFMove(buffer));
    m_status = m_parser->Feed(this, &m_reader);
    if (m_status.ok() || m_status.code == Status::kEndOfFile || m_status.code == Status::kWouldBlock)
        return;

    callOnMainThread([this, protectedThis = makeRef(*this), code = m_status.code] {
        if (m_didEncounterErrorDuringParsingCallback)
            m_didEncounterErrorDuringParsingCallback(code);
    });
}

void SourceBufferParserWebM::flushPendingMediaData()
{
    notImplemented();
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
    m_parser->DidSeek();
    m_state = State::None;
    m_tracks.clear();
    m_initializationSegment = nullptr;
    m_initializationSegmentEncountered = false;
    m_currentBlock.reset();
}

void SourceBufferParserWebM::invalidate()
{
    m_parser = nullptr;
    m_tracks.clear();
    m_initializationSegment = nullptr;
    m_currentBlock.reset();
}

auto SourceBufferParserWebM::trackDataForTrackNumber(uint64_t trackNumber) -> TrackData*
{
    for (auto& track : m_tracks) {
        if (track.track.track_number.is_present() && track.track.track_number.value() == trackNumber)
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

    if ((m_state == State::None && metadata.id != Id::kEbml)
        || (m_state == State::ReadEbml && metadata.id != Id::kSegment)
        || (m_state == State::ReadingSegment && metadata.id != Id::kInfo)
        || (m_state == State::ReadInfo && metadata.id != Id::kTracks)
        || (m_state == State::ReadTrack && metadata.id != Id::kTrackEntry && metadata.id != Id::kCluster)
        || (metadata.id == Id::kCues || metadata.id == Id::kChapters)) {
        *action = Action::kSkip;
        return Status(Status::kOkCompleted);
    }

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

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnEbml(const ElementMetadata& metadata, const Ebml& ebml)
{
    UNUSED_PARAM(metadata);
    if (ebml.doc_type.is_present() && ebml.doc_type.value().compare("webm"))
        return Status(Status::Code(ErrorCode::InvalidDocType));

    m_initializationSegmentEncountered = true;
    m_initializationSegment = WTF::makeUniqueWithoutFastMallocCheck<InitializationSegment>();
    m_state = State::ReadEbml;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnSegmentBegin(const ElementMetadata& metadata, Action* action)
{
    UNUSED_PARAM(metadata);
    if (!m_initializationSegmentEncountered)
        return Status(Status::Code(ErrorCode::InvalidInitSegment));

    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);
    *action = Action::kRead;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnInfo(const ElementMetadata& metadata, const Info& info)
{
    UNUSED_PARAM(metadata);
    if (!m_initializationSegmentEncountered || !m_initializationSegment)
        return Status(Status::Code(ErrorCode::InvalidInitSegment));

    auto timecodeScale = info.timecode_scale.is_present() ? info.timecode_scale.value() : 1000000;
    m_timescale = 1000000000 / timecodeScale;
    m_initializationSegment->duration = info.duration.is_present() ? MediaTime(info.duration.value(), m_timescale) : MediaTime::indefiniteTime();
    m_state = State::ReadInfo;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnClusterBegin(const ElementMetadata& metadata, const Cluster& cluster, Action* action)
{
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(cluster);
    ASSERT(action);
    if (!action)
        return Status(Status::kNotEnoughMemory);

    if (m_initializationSegmentEncountered && m_didParseInitializationDataCallback) {
        callOnMainThread([this, protectedThis = makeRef(*this), initializationSegment = WTFMove(*m_initializationSegment)] () mutable {
            m_didParseInitializationDataCallback(WTFMove(initializationSegment));
        });
    }
    m_initializationSegmentEncountered = false;
    m_initializationSegment = nullptr;

    if (cluster.timecode.is_present())
        m_currentTimecode = cluster.timecode.value();

    *action = Action::kRead;

    return Status(Status::kOkCompleted);
}

Status SourceBufferParserWebM::OnTrackEntry(const ElementMetadata& metadata, const TrackEntry& trackEntry)
{
    UNUSED_PARAM(metadata);
    m_state = State::ReadTrack;
    if (!trackEntry.track_type.is_present())
        return Status(Status::kOkCompleted);

    auto trackType = trackEntry.track_type.value();
    if (trackType == TrackType::kVideo)
        m_initializationSegment->videoTracks.append({ MediaDescriptionWebM::create(TrackEntry(trackEntry)), VideoTrackPrivateWebM::create(TrackEntry(trackEntry)) });
    else if (trackType == TrackType::kAudio)
        m_initializationSegment->audioTracks.append({ MediaDescriptionWebM::create(TrackEntry(trackEntry)), AudioTrackPrivateWebM::create(TrackEntry(trackEntry)) });
    m_tracks.append({ TrackEntry(trackEntry), { }, nullptr, nullptr, 0 });

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockBegin(const ElementMetadata& metadata, const Block& block, Action* action)
{
    UNUSED_PARAM(metadata);
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

    m_currentBlock = WTF::nullopt;

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnSimpleBlockBegin(const ElementMetadata& metadata, const SimpleBlock& block, Action* action)
{
    UNUSED_PARAM(metadata);
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
    UNUSED_PARAM(block);

    m_currentBlock = WTF::nullopt;

    return Status(Status::kOkCompleted);
}

webm::Status SourceBufferParserWebM::OnBlockGroupBegin(const webm::ElementMetadata& metadata, webm::Action* action)
{
    UNUSED_PARAM(metadata);
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
    return Status(Status::kOkCompleted);
}

static uint8_t convertToColorPrimaries(const Primaries& coefficients)
{
    switch (coefficients) {
    case Primaries::kBt709:
        return VPConfigurationColorPrimaries::BT_709_6;
    case Primaries::kUnspecified:
        return VPConfigurationColorPrimaries::Unspecified;
    case Primaries::kBt470M:
        return VPConfigurationColorPrimaries::BT_470_6_M;
    case Primaries::kBt470Bg:
        return VPConfigurationColorPrimaries::BT_470_7_BG;
    case Primaries::kSmpte170M:
        return VPConfigurationColorPrimaries::BT_601_7;
    case Primaries::kSmpte240M:
        return VPConfigurationColorPrimaries::SMPTE_ST_240;
    case Primaries::kFilm:
        return VPConfigurationColorPrimaries::Film;
    case Primaries::kBt2020:
        return VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance;
    case Primaries::kSmpteSt4281:
        return VPConfigurationColorPrimaries::SMPTE_ST_428_1;
    case Primaries::kSmpteRp431:
        return VPConfigurationColorPrimaries::SMPTE_RP_431_2;
    case Primaries::kSmpteEg432:
        return VPConfigurationColorPrimaries::SMPTE_EG_432_1;
    case Primaries::kJedecP22Phosphors:
        return VPConfigurationColorPrimaries::EBU_Tech_3213_E;
    }
}

static CFStringRef convertToCMColorPrimaries(uint8_t primaries)
{
    switch (primaries) {
    case VPConfigurationColorPrimaries::BT_709_6:
        return kCVImageBufferColorPrimaries_ITU_R_709_2;
    case VPConfigurationColorPrimaries::EBU_Tech_3213_E:
        return kCVImageBufferColorPrimaries_EBU_3213;
    case VPConfigurationColorPrimaries::BT_601_7:
    case VPConfigurationColorPrimaries::SMPTE_ST_240:
        return kCVImageBufferColorPrimaries_SMPTE_C;
    case VPConfigurationColorPrimaries::SMPTE_RP_431_2:
        return kCMFormatDescriptionColorPrimaries_DCI_P3;
    case VPConfigurationColorPrimaries::SMPTE_EG_432_1:
        return kCMFormatDescriptionColorPrimaries_P3_D65;
    case VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance:
        return kCMFormatDescriptionColorPrimaries_ITU_R_2020;
    }

    return nullptr;
}

static uint8_t convertToTransferCharacteristics(const TransferCharacteristics& characteristics)
{
    switch (characteristics) {
    case TransferCharacteristics::kBt709:
        return VPConfigurationTransferCharacteristics::BT_709_6;
    case TransferCharacteristics::kUnspecified:
        return VPConfigurationTransferCharacteristics::Unspecified;
    case TransferCharacteristics::kGamma22curve:
        return VPConfigurationTransferCharacteristics::BT_470_6_M;
    case TransferCharacteristics::kGamma28curve:
        return VPConfigurationTransferCharacteristics::BT_470_7_BG;
    case TransferCharacteristics::kSmpte170M:
        return VPConfigurationTransferCharacteristics::BT_601_7;
    case TransferCharacteristics::kSmpte240M:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_240;
    case TransferCharacteristics::kLinear:
        return VPConfigurationTransferCharacteristics::Linear;
    case TransferCharacteristics::kLog:
        return VPConfigurationTransferCharacteristics::Logrithmic;
    case TransferCharacteristics::kLogSqrt:
        return VPConfigurationTransferCharacteristics::Logrithmic_Sqrt;
    case TransferCharacteristics::kIec6196624:
        return VPConfigurationTransferCharacteristics::IEC_61966_2_4;
    case TransferCharacteristics::kBt1361ExtendedColourGamut:
        return VPConfigurationTransferCharacteristics::BT_1361_0;
    case TransferCharacteristics::kIec6196621:
        return VPConfigurationTransferCharacteristics::IEC_61966_2_1;
    case TransferCharacteristics::k10BitBt2020:
        return VPConfigurationTransferCharacteristics::BT_2020_10bit;
    case TransferCharacteristics::k12BitBt2020:
        return VPConfigurationTransferCharacteristics::BT_2020_12bit;
    case TransferCharacteristics::kSmpteSt2084:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_2084;
    case TransferCharacteristics::kSmpteSt4281:
        return VPConfigurationTransferCharacteristics::SMPTE_ST_428_1;
    case TransferCharacteristics::kAribStdB67Hlg:
        return VPConfigurationTransferCharacteristics::BT_2100_HLG;
    }
}

static CFStringRef convertToCMTransferFunction(uint8_t characteristics)
{
    switch (characteristics) {
    case VPConfigurationTransferCharacteristics::BT_709_6:
        return kCVImageBufferTransferFunction_ITU_R_709_2;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_240:
        return kCVImageBufferTransferFunction_SMPTE_240M_1995;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_2084:
        return kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;
    case VPConfigurationTransferCharacteristics::BT_2020_10bit:
    case VPConfigurationTransferCharacteristics::BT_2020_12bit:
        return kCMFormatDescriptionTransferFunction_ITU_R_2020;
    case VPConfigurationTransferCharacteristics::SMPTE_ST_428_1:
        return kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;
    case VPConfigurationTransferCharacteristics::BT_2100_HLG:
        return kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;
    case VPConfigurationTransferCharacteristics::IEC_61966_2_1:
        return PAL::canLoad_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() ? get_CoreMedia_kCMFormatDescriptionTransferFunction_sRGB() : nullptr;
    case VPConfigurationTransferCharacteristics::Linear:
        return kCMFormatDescriptionTransferFunction_Linear;
    }

    return nullptr;
}

static uint8_t convertToMatrixCoefficients(const MatrixCoefficients& coefficients)
{
    switch (coefficients) {
    case MatrixCoefficients::kRgb:
        return VPConfigurationMatrixCoefficients::Identity;
    case MatrixCoefficients::kBt709:
        return VPConfigurationMatrixCoefficients::BT_709_6;
    case MatrixCoefficients::kUnspecified:
        return VPConfigurationMatrixCoefficients::Unspecified;
    case MatrixCoefficients::kFcc:
        return VPConfigurationMatrixCoefficients::FCC;
    case MatrixCoefficients::kBt470Bg:
        return VPConfigurationMatrixCoefficients::BT_470_7_BG;
    case MatrixCoefficients::kSmpte170M:
        return VPConfigurationMatrixCoefficients::BT_601_7;
    case MatrixCoefficients::kSmpte240M:
        return VPConfigurationMatrixCoefficients::SMPTE_ST_240;
    case MatrixCoefficients::kYCgCo:
        return VPConfigurationMatrixCoefficients::YCgCo;
    case MatrixCoefficients::kBt2020NonconstantLuminance:
        return VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance;
    case MatrixCoefficients::kBt2020ConstantLuminance:
        return VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance;
    }
}

static CFStringRef convertToCMYCbCRMatrix(uint8_t coefficients)
{
    switch (coefficients) {
    case VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance:
        return kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;
    case VPConfigurationMatrixCoefficients::BT_470_7_BG:
    case VPConfigurationMatrixCoefficients::BT_601_7:
        return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    case VPConfigurationMatrixCoefficients::BT_709_6:
        return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
    case VPConfigurationMatrixCoefficients::SMPTE_ST_240:
        return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
    }

    return nullptr;
}

static uint8_t convertSubsamplingXYToChromaSubsampling(uint64_t x, uint64_t y)
{
    if (x & y)
        return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
    if (x & !y)
        return VPConfigurationChromaSubsampling::Subsampling_422;
    if (!x & !y)
        return VPConfigurationChromaSubsampling::Subsampling_444;
    // This indicates 4:4:0 subsampling, which is not expressable in the 'vpcC' box. Default to 4:2:0.
    return VPConfigurationChromaSubsampling::Subsampling_420_Colocated;
}

static RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromVP9HeaderParser(const vp9_parser::Vp9HeaderParser& parser, const webm::Element<Colour>& color)
{
    // Ref: "VP Codec ISO Media File Format Binding, v1.0, 2017-03-31"
    // <https://www.webmproject.org/vp9/mp4/>
    //
    // class VPCodecConfigurationBox extends FullBox('vpcC', version = 1, 0)
    // {
    //     VPCodecConfigurationRecord() vpcConfig;
    // }
    //
    // aligned (8) class VPCodecConfigurationRecord {
    //     unsigned int (8)     profile;
    //     unsigned int (8)     level;
    //     unsigned int (4)     bitDepth;
    //     unsigned int (3)     chromaSubsampling;
    //     unsigned int (1)     videoFullRangeFlag;
    //     unsigned int (8)     colourPrimaries;
    //     unsigned int (8)     transferCharacteristics;
    //     unsigned int (8)     matrixCoefficients;
    //     unsigned int (16)    codecIntializationDataSize;
    //     unsigned int (8)[]   codecIntializationData;
    // }
    //
    // codecIntializationDataSize​For VP8 and VP9 this field must be 0.
    // codecIntializationData​binary codec initialization data. Not used for VP8 and VP9.
    //
    // FIXME: Convert existing struct to an ISOBox and replace the writing code below
    // with a subclass of ISOFullBox.

    VPCodecConfigurationRecord record;

    record.profile = parser.profile();
    // CoreMedia does nat care about the VP9 codec level; hard-code to Level 1.0 here:
    record.level = 10;
    record.bitDepth = parser.bit_depth();
    record.videoFullRangeFlag = parser.color_range() ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
    record.chromaSubsampling = convertSubsamplingXYToChromaSubsampling(parser.subsampling_x(), parser.subsampling_y());
    record.colorPrimaries = VPConfigurationColorPrimaries::Unspecified;
    record.transferCharacteristics = VPConfigurationTransferCharacteristics::Unspecified;
    record.matrixCoefficients = VPConfigurationMatrixCoefficients::Unspecified;

    // Container color values can override per-sample ones:
    if (color.is_present()) {
        auto& colorValue = color.value();
        if (colorValue.chroma_subsampling_x.is_present() && colorValue.chroma_subsampling_y.is_present())
            record.chromaSubsampling = convertSubsamplingXYToChromaSubsampling(colorValue.chroma_subsampling_x.value(), colorValue.chroma_subsampling_y.value());
        if (colorValue.range.is_present() && colorValue.range.value() != Range::kUnspecified)
            record.videoFullRangeFlag = colorValue.range.value() == Range::kFull ? VPConfigurationRange::FullRange : VPConfigurationRange::VideoRange;
        if (colorValue.bits_per_channel.is_present())
            record.bitDepth = colorValue.bits_per_channel.value();
        if (colorValue.transfer_characteristics.is_present())
            record.transferCharacteristics = convertToTransferCharacteristics(colorValue.transfer_characteristics.value());
        if (colorValue.matrix_coefficients.is_present())
            record.matrixCoefficients = convertToMatrixCoefficients(colorValue.matrix_coefficients.value());
        if (colorValue.primaries.is_present())
            record.colorPrimaries = convertToColorPrimaries(colorValue.primaries.value());
    }

    constexpr size_t VPCodecConfigurationContentsSize = 12;

    uint32_t versionAndFlags = 1 << 24;
    uint8_t bitDepthChromaAndRange = (0xF & record.bitDepth) << 4 | (0x7 & record.chromaSubsampling) << 1 | (0x1 & record.videoFullRangeFlag);
    uint16_t codecIntializationDataSize = 0;

    auto view = JSC::DataView::create(ArrayBuffer::create(VPCodecConfigurationContentsSize, 1), 0, VPCodecConfigurationContentsSize);
    view->set(0, versionAndFlags, false);
    view->set(4, record.profile, false);
    view->set(5, record.level, false);
    view->set(6, bitDepthChromaAndRange, false);
    view->set(7, record.colorPrimaries, false);
    view->set(8, record.transferCharacteristics, false);
    view->set(9, record.matrixCoefficients, false);
    view->set(10, codecIntializationDataSize, false);

    auto data = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)view->data(), view->byteLength()));

    CFTypeRef configurationKeys[] = { CFSTR("vpcC") };
    CFTypeRef configurationValues[] = { data.get() };
    auto configurationDict = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, configurationKeys, configurationValues, WTF_ARRAY_LENGTH(configurationKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    Vector<CFTypeRef> extensionsKeys { kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms };
    Vector<CFTypeRef> extensionsValues = { configurationDict.get() };

    if (record.videoFullRangeFlag == VPConfigurationRange::FullRange) {
        extensionsKeys.append(kCMFormatDescriptionExtension_FullRangeVideo);
        extensionsValues.append(kCFBooleanTrue);
    }

    if (auto cmColorPrimaries = convertToCMColorPrimaries(record.colorPrimaries)) {
        extensionsKeys.append(kCVImageBufferColorPrimariesKey);
        extensionsValues.append(cmColorPrimaries);
    }

    if (auto cmTransferFunction = convertToCMTransferFunction(record.transferCharacteristics)) {
        extensionsKeys.append(kCVImageBufferTransferFunctionKey);
        extensionsValues.append(cmTransferFunction);
    }

    if (auto cmMatrix = convertToCMYCbCRMatrix(record.matrixCoefficients)) {
        extensionsKeys.append(kCVImageBufferYCbCrMatrixKey);
        extensionsValues.append(cmMatrix);
    }

    auto extensions = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, extensionsKeys.data(), extensionsValues.data(), extensionsKeys.size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    if (noErr != CMVideoFormatDescriptionCreate(kCFAllocatorDefault, kCMVideoCodecType_VP9, parser.width(), parser.height(), extensions.get(), &formatDescription))
        return nullptr;
    return adoptCF(formatDescription);
}

webm::Status SourceBufferParserWebM::OnFrame(const FrameMetadata& metadata, Reader* reader, uint64_t* bytesRemaining)
{
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(bytesRemaining);

    ASSERT(reader);
    if (!reader)
        return Status(Status::kNotEnoughMemory);

    if (!m_currentBlock)
        return Status(Status::kInvalidElementId);

    auto* block = WTF::switchOn(*m_currentBlock, [](Block& block) {
        return &block;
    }, [](SimpleBlock& block) -> Block* {
        return &block;
    });
    if (!block)
        return Status(Status::kInvalidElementId);

    auto trackNumber = block->track_number;

    auto* trackData = trackDataForTrackNumber(trackNumber);
    if (!trackData)
        return Status(Status::kInvalidElementId);
    auto& track = trackData->track;
    auto& headerParser = trackData->headerParser;

    uint64_t duration = 0;
    if (track.default_duration.is_present())
        duration = track.default_duration.value() * m_timescale / 1000000000;

    if (track.codec_id.is_present() && track.codec_id.value() == "V_VP9") {
        if (!trackData->currentBlockBuffer) {
            CMBlockBufferRef rawBlockBuffer = nullptr;
            if (noErr != CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, metadata.size, kCFAllocatorDefault, nullptr, 0, metadata.size, 0, &rawBlockBuffer))
                return Skip(reader, bytesRemaining);
            trackData->currentBlockBuffer = adoptCF(rawBlockBuffer);
            trackData->currentBlockBufferPosition = 0;

            if (noErr != CMBlockBufferAssureBlockMemory(trackData->currentBlockBuffer.get()))
                return Skip(reader, bytesRemaining);
        }

        size_t numToRead = metadata.size - trackData->currentBlockBufferPosition;
        size_t segmentSizeAtPosition = 0;
        uint8_t* blockBufferData = nullptr;

        while (numToRead) {
            if (noErr != CMBlockBufferGetDataPointer(trackData->currentBlockBuffer.get(), trackData->currentBlockBufferPosition, &segmentSizeAtPosition, nullptr, (char**)&blockBufferData))
                return Skip(reader, bytesRemaining);

            size_t numToReadFromThisSegment = std::min(numToRead, segmentSizeAtPosition);
            uint64_t numActuallyWrittenToThisSegment;

            auto status = m_reader->Read(numToReadFromThisSegment, (uint8_t*)blockBufferData, &numActuallyWrittenToThisSegment);
            numToRead -= numActuallyWrittenToThisSegment;
            *bytesRemaining -= numActuallyWrittenToThisSegment;
            trackData->currentBlockBufferPosition += numActuallyWrittenToThisSegment;

            // FIXME: We can't yet handle parsing a Frame that doesn't have all its memory available.
            if (status.code == webm::Status::kOkPartial || status.code == webm::Status::kWouldBlock)
                return status;
        }

        if (noErr != CMBlockBufferGetDataPointer(trackData->currentBlockBuffer.get(), 0, &segmentSizeAtPosition, nullptr, (char**)&blockBufferData))
            return Skip(reader, bytesRemaining);

        if (!headerParser.ParseUncompressedHeader(blockBufferData, segmentSizeAtPosition))
            return Skip(reader, bytesRemaining);

        // Only update the format description when the header indicates that the sample is
        // a key-frame, otherwise color information will not be parsed.
        if (headerParser.key())
            trackData->formatDescription = createFormatDescriptionFromVP9HeaderParser(headerParser, track.video.value().colour);
        if (!trackData->formatDescription)
            return Skip(reader, bytesRemaining);

        auto pts = CMTimeMake(block->timecode + m_currentTimecode, m_timescale);
        CMSampleTimingInfo timing = { CMTimeMake(duration, m_timescale), pts, pts };
        size_t size = metadata.size;

        auto sampleCount = block->num_frames;
        CMSampleBufferRef rawSampleBuffer = nullptr;
        if (noErr != CMSampleBufferCreateReady(kCFAllocatorDefault, trackData->currentBlockBuffer.get(), trackData->formatDescription.get(), sampleCount, 1, &timing, 1, &size, &rawSampleBuffer))
            return Skip(reader, bytesRemaining);
        auto sampleBuffer = adoptCF(rawSampleBuffer);

        trackData->currentBlockBuffer = nullptr;
        trackData->currentBlockBufferPosition = 0;

        if (!headerParser.key()) {
            auto attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), true);
            ASSERT(attachmentsArray);
            if (!attachmentsArray)
                return Skip(reader, bytesRemaining);
            for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
                CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
                CFDictionarySetValue(attachments, kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
            }
        }

        auto trackID = track.track_uid.value();

        callOnMainThread([this, protectedThis = makeRef(*this), sampleBuffer = WTFMove(sampleBuffer), trackID] () mutable {
            if (!m_didProvideMediaDataCallback)
                return;
            auto mediaSample = MediaSampleAVFObjC::create(sampleBuffer.get(), trackID);
            m_didProvideMediaDataCallback(WTFMove(mediaSample), trackID, emptyString());
        });

    }

    return Skip(reader, bytesRemaining);
}

}

#endif // ENABLE(MEDIA_SOURCE)
