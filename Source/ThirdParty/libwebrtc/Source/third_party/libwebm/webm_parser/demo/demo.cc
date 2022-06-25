// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "webm/callback.h"
#include "webm/file_reader.h"
#include "webm/status.h"
#include "webm/webm_parser.h"

// We use pretty much everything in the webm namespace. Just pull
// it all in.
using namespace webm;  // NOLINT

template <typename T>
std::ostream& PrintUnknownEnumValue(std::ostream& os, T value) {
  return os << std::to_string(static_cast<std::uint64_t>(value)) << " (?)";
}

// Overloads for operator<< for pretty printing enums.
std::ostream& operator<<(std::ostream& os, Id id) {
  switch (id) {
    case Id::kEbml:
      return os << "EBML";
    case Id::kEbmlVersion:
      return os << "EBMLVersion";
    case Id::kEbmlReadVersion:
      return os << "EBMLReadVersion";
    case Id::kEbmlMaxIdLength:
      return os << "EBMLMaxIDLength";
    case Id::kEbmlMaxSizeLength:
      return os << "EBMLMaxSizeLength";
    case Id::kDocType:
      return os << "DocType";
    case Id::kDocTypeVersion:
      return os << "DocTypeVersion";
    case Id::kDocTypeReadVersion:
      return os << "DocTypeReadVersion";
    case Id::kVoid:
      return os << "Void";
    case Id::kSegment:
      return os << "Segment";
    case Id::kSeekHead:
      return os << "SeekHead";
    case Id::kSeek:
      return os << "Seek";
    case Id::kSeekId:
      return os << "SeekID";
    case Id::kSeekPosition:
      return os << "SeekPosition";
    case Id::kInfo:
      return os << "Info";
    case Id::kTimecodeScale:
      return os << "TimecodeScale";
    case Id::kDuration:
      return os << "Duration";
    case Id::kDateUtc:
      return os << "DateUTC";
    case Id::kTitle:
      return os << "Title";
    case Id::kMuxingApp:
      return os << "MuxingApp";
    case Id::kWritingApp:
      return os << "WritingApp";
    case Id::kCluster:
      return os << "Cluster";
    case Id::kTimecode:
      return os << "Timecode";
    case Id::kPrevSize:
      return os << "PrevSize";
    case Id::kSimpleBlock:
      return os << "SimpleBlock";
    case Id::kBlockGroup:
      return os << "BlockGroup";
    case Id::kBlock:
      return os << "Block";
    case Id::kBlockVirtual:
      return os << "BlockVirtual";
    case Id::kBlockAdditions:
      return os << "BlockAdditions";
    case Id::kBlockMore:
      return os << "BlockMore";
    case Id::kBlockAddId:
      return os << "BlockAddID";
    case Id::kBlockAdditional:
      return os << "BlockAdditional";
    case Id::kBlockDuration:
      return os << "BlockDuration";
    case Id::kReferenceBlock:
      return os << "ReferenceBlock";
    case Id::kDiscardPadding:
      return os << "DiscardPadding";
    case Id::kSlices:
      return os << "Slices";
    case Id::kTimeSlice:
      return os << "TimeSlice";
    case Id::kLaceNumber:
      return os << "LaceNumber";
    case Id::kTracks:
      return os << "Tracks";
    case Id::kTrackEntry:
      return os << "TrackEntry";
    case Id::kTrackNumber:
      return os << "TrackNumber";
    case Id::kTrackUid:
      return os << "TrackUID";
    case Id::kTrackType:
      return os << "TrackType";
    case Id::kFlagEnabled:
      return os << "FlagEnabled";
    case Id::kFlagDefault:
      return os << "FlagDefault";
    case Id::kFlagForced:
      return os << "FlagForced";
    case Id::kFlagLacing:
      return os << "FlagLacing";
    case Id::kDefaultDuration:
      return os << "DefaultDuration";
    case Id::kName:
      return os << "Name";
    case Id::kLanguage:
      return os << "Language";
    case Id::kCodecId:
      return os << "CodecID";
    case Id::kCodecPrivate:
      return os << "CodecPrivate";
    case Id::kCodecName:
      return os << "CodecName";
    case Id::kCodecDelay:
      return os << "CodecDelay";
    case Id::kSeekPreRoll:
      return os << "SeekPreRoll";
    case Id::kVideo:
      return os << "Video";
    case Id::kFlagInterlaced:
      return os << "FlagInterlaced";
    case Id::kStereoMode:
      return os << "StereoMode";
    case Id::kAlphaMode:
      return os << "AlphaMode";
    case Id::kPixelWidth:
      return os << "PixelWidth";
    case Id::kPixelHeight:
      return os << "PixelHeight";
    case Id::kPixelCropBottom:
      return os << "PixelCropBottom";
    case Id::kPixelCropTop:
      return os << "PixelCropTop";
    case Id::kPixelCropLeft:
      return os << "PixelCropLeft";
    case Id::kPixelCropRight:
      return os << "PixelCropRight";
    case Id::kDisplayWidth:
      return os << "DisplayWidth";
    case Id::kDisplayHeight:
      return os << "DisplayHeight";
    case Id::kDisplayUnit:
      return os << "DisplayUnit";
    case Id::kAspectRatioType:
      return os << "AspectRatioType";
    case Id::kFrameRate:
      return os << "FrameRate";
    case Id::kColour:
      return os << "Colour";
    case Id::kMatrixCoefficients:
      return os << "MatrixCoefficients";
    case Id::kBitsPerChannel:
      return os << "BitsPerChannel";
    case Id::kChromaSubsamplingHorz:
      return os << "ChromaSubsamplingHorz";
    case Id::kChromaSubsamplingVert:
      return os << "ChromaSubsamplingVert";
    case Id::kCbSubsamplingHorz:
      return os << "CbSubsamplingHorz";
    case Id::kCbSubsamplingVert:
      return os << "CbSubsamplingVert";
    case Id::kChromaSitingHorz:
      return os << "ChromaSitingHorz";
    case Id::kChromaSitingVert:
      return os << "ChromaSitingVert";
    case Id::kRange:
      return os << "Range";
    case Id::kTransferCharacteristics:
      return os << "TransferCharacteristics";
    case Id::kPrimaries:
      return os << "Primaries";
    case Id::kMaxCll:
      return os << "MaxCLL";
    case Id::kMaxFall:
      return os << "MaxFALL";
    case Id::kMasteringMetadata:
      return os << "MasteringMetadata";
    case Id::kPrimaryRChromaticityX:
      return os << "PrimaryRChromaticityX";
    case Id::kPrimaryRChromaticityY:
      return os << "PrimaryRChromaticityY";
    case Id::kPrimaryGChromaticityX:
      return os << "PrimaryGChromaticityX";
    case Id::kPrimaryGChromaticityY:
      return os << "PrimaryGChromaticityY";
    case Id::kPrimaryBChromaticityX:
      return os << "PrimaryBChromaticityX";
    case Id::kPrimaryBChromaticityY:
      return os << "PrimaryBChromaticityY";
    case Id::kWhitePointChromaticityX:
      return os << "WhitePointChromaticityX";
    case Id::kWhitePointChromaticityY:
      return os << "WhitePointChromaticityY";
    case Id::kLuminanceMax:
      return os << "LuminanceMax";
    case Id::kLuminanceMin:
      return os << "LuminanceMin";
    case Id::kProjection:
      return os << "Projection";
    case Id::kProjectionType:
      return os << "kProjectionType";
    case Id::kProjectionPrivate:
      return os << "kProjectionPrivate";
    case Id::kProjectionPoseYaw:
      return os << "kProjectionPoseYaw";
    case Id::kProjectionPosePitch:
      return os << "kProjectionPosePitch";
    case Id::kProjectionPoseRoll:
      return os << "ProjectionPoseRoll";
    case Id::kAudio:
      return os << "Audio";
    case Id::kSamplingFrequency:
      return os << "SamplingFrequency";
    case Id::kOutputSamplingFrequency:
      return os << "OutputSamplingFrequency";
    case Id::kChannels:
      return os << "Channels";
    case Id::kBitDepth:
      return os << "BitDepth";
    case Id::kContentEncodings:
      return os << "ContentEncodings";
    case Id::kContentEncoding:
      return os << "ContentEncoding";
    case Id::kContentEncodingOrder:
      return os << "ContentEncodingOrder";
    case Id::kContentEncodingScope:
      return os << "ContentEncodingScope";
    case Id::kContentEncodingType:
      return os << "ContentEncodingType";
    case Id::kContentEncryption:
      return os << "ContentEncryption";
    case Id::kContentEncAlgo:
      return os << "ContentEncAlgo";
    case Id::kContentEncKeyId:
      return os << "ContentEncKeyID";
    case Id::kContentEncAesSettings:
      return os << "ContentEncAESSettings";
    case Id::kAesSettingsCipherMode:
      return os << "AESSettingsCipherMode";
    case Id::kCues:
      return os << "Cues";
    case Id::kCuePoint:
      return os << "CuePoint";
    case Id::kCueTime:
      return os << "CueTime";
    case Id::kCueTrackPositions:
      return os << "CueTrackPositions";
    case Id::kCueTrack:
      return os << "CueTrack";
    case Id::kCueClusterPosition:
      return os << "CueClusterPosition";
    case Id::kCueRelativePosition:
      return os << "CueRelativePosition";
    case Id::kCueDuration:
      return os << "CueDuration";
    case Id::kCueBlockNumber:
      return os << "CueBlockNumber";
    case Id::kChapters:
      return os << "Chapters";
    case Id::kEditionEntry:
      return os << "EditionEntry";
    case Id::kChapterAtom:
      return os << "ChapterAtom";
    case Id::kChapterUid:
      return os << "ChapterUID";
    case Id::kChapterStringUid:
      return os << "ChapterStringUID";
    case Id::kChapterTimeStart:
      return os << "ChapterTimeStart";
    case Id::kChapterTimeEnd:
      return os << "ChapterTimeEnd";
    case Id::kChapterDisplay:
      return os << "ChapterDisplay";
    case Id::kChapString:
      return os << "ChapString";
    case Id::kChapLanguage:
      return os << "ChapLanguage";
    case Id::kChapCountry:
      return os << "ChapCountry";
    case Id::kTags:
      return os << "Tags";
    case Id::kTag:
      return os << "Tag";
    case Id::kTargets:
      return os << "Targets";
    case Id::kTargetTypeValue:
      return os << "TargetTypeValue";
    case Id::kTargetType:
      return os << "TargetType";
    case Id::kTagTrackUid:
      return os << "TagTrackUID";
    case Id::kSimpleTag:
      return os << "SimpleTag";
    case Id::kTagName:
      return os << "TagName";
    case Id::kTagLanguage:
      return os << "TagLanguage";
    case Id::kTagDefault:
      return os << "TagDefault";
    case Id::kTagString:
      return os << "TagString";
    case Id::kTagBinary:
      return os << "TagBinary";
    default:
      return PrintUnknownEnumValue(os, id);
  }
}

std::ostream& operator<<(std::ostream& os, Lacing value) {
  switch (value) {
    case Lacing::kNone:
      return os << "0 (none)";
    case Lacing::kXiph:
      return os << "2 (Xiph)";
    case Lacing::kFixed:
      return os << "4 (fixed)";
    case Lacing::kEbml:
      return os << "6 (EBML)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, MatrixCoefficients value) {
  switch (value) {
    case MatrixCoefficients::kRgb:
      return os << "0 (identity, RGB/XYZ)";
    case MatrixCoefficients::kBt709:
      return os << "1 (Rec. ITU-R BT.709-5)";
    case MatrixCoefficients::kUnspecified:
      return os << "2 (unspecified)";
    case MatrixCoefficients::kFcc:
      return os << "4 (US FCC)";
    case MatrixCoefficients::kBt470Bg:
      return os << "5 (Rec. ITU-R BT.470-6 System B, G)";
    case MatrixCoefficients::kSmpte170M:
      return os << "6 (SMPTE 170M)";
    case MatrixCoefficients::kSmpte240M:
      return os << "7 (SMPTE 240M)";
    case MatrixCoefficients::kYCgCo:
      return os << "8 (YCgCo)";
    case MatrixCoefficients::kBt2020NonconstantLuminance:
      return os << "9 (Rec. ITU-R BT.2020, non-constant luma)";
    case MatrixCoefficients::kBt2020ConstantLuminance:
      return os << "10 (Rec. ITU-R BT.2020 , constant luma)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, Range value) {
  switch (value) {
    case Range::kUnspecified:
      return os << "0 (unspecified)";
    case Range::kBroadcast:
      return os << "1 (broadcast)";
    case Range::kFull:
      return os << "2 (full)";
    case Range::kDerived:
      return os << "3 (defined by MatrixCoefficients/TransferCharacteristics)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, TransferCharacteristics value) {
  switch (value) {
    case TransferCharacteristics::kBt709:
      return os << "1 (Rec. ITU-R BT.709-6)";
    case TransferCharacteristics::kUnspecified:
      return os << "2 (unspecified)";
    case TransferCharacteristics::kGamma22curve:
      return os << "4 (gamma 2.2, Rec. ITU‑R BT.470‑6 System M)";
    case TransferCharacteristics::kGamma28curve:
      return os << "5 (gamma 2.8, Rec. ITU‑R BT.470-6 System B, G)";
    case TransferCharacteristics::kSmpte170M:
      return os << "6 (SMPTE 170M)";
    case TransferCharacteristics::kSmpte240M:
      return os << "7 (SMPTE 240M)";
    case TransferCharacteristics::kLinear:
      return os << "8 (linear)";
    case TransferCharacteristics::kLog:
      return os << "9 (log, 100:1 range)";
    case TransferCharacteristics::kLogSqrt:
      return os << "10 (log, 316.2:1 range)";
    case TransferCharacteristics::kIec6196624:
      return os << "11 (IEC 61966-2-4)";
    case TransferCharacteristics::kBt1361ExtendedColourGamut:
      return os << "12 (Rec. ITU-R BT.1361, extended colour gamut)";
    case TransferCharacteristics::kIec6196621:
      return os << "13 (IEC 61966-2-1, sRGB or sYCC)";
    case TransferCharacteristics::k10BitBt2020:
      return os << "14 (Rec. ITU-R BT.2020-2, 10-bit)";
    case TransferCharacteristics::k12BitBt2020:
      return os << "15 (Rec. ITU-R BT.2020-2, 12-bit)";
    case TransferCharacteristics::kSmpteSt2084:
      return os << "16 (SMPTE ST 2084)";
    case TransferCharacteristics::kSmpteSt4281:
      return os << "17 (SMPTE ST 428-1)";
    case TransferCharacteristics::kAribStdB67Hlg:
      return os << "18 (ARIB STD-B67/Rec. ITU-R BT.[HDR-TV] HLG)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, Primaries value) {
  switch (value) {
    case Primaries::kBt709:
      return os << "1 (Rec. ITU‑R BT.709-6)";
    case Primaries::kUnspecified:
      return os << "2 (unspecified)";
    case Primaries::kBt470M:
      return os << "4 (Rec. ITU‑R BT.470‑6 System M)";
    case Primaries::kBt470Bg:
      return os << "5 (Rec. ITU‑R BT.470‑6 System B, G)";
    case Primaries::kSmpte170M:
      return os << "6 (SMPTE 170M)";
    case Primaries::kSmpte240M:
      return os << "7 (SMPTE 240M)";
    case Primaries::kFilm:
      return os << "8 (generic film)";
    case Primaries::kBt2020:
      return os << "9 (Rec. ITU-R BT.2020-2)";
    case Primaries::kSmpteSt4281:
      return os << "10 (SMPTE ST 428-1)";
    case Primaries::kJedecP22Phosphors:
      return os << "22 (EBU Tech. 3213-E/JEDEC P22 phosphors)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, ProjectionType value) {
  switch (value) {
    case ProjectionType::kRectangular:
      return os << "0 (rectangular)";
    case ProjectionType::kEquirectangular:
      return os << "1 (equirectangular)";
    case ProjectionType::kCubeMap:
      return os << "2 (cube map)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, FlagInterlaced value) {
  switch (value) {
    case FlagInterlaced::kUnspecified:
      return os << "0 (unspecified)";
    case FlagInterlaced::kInterlaced:
      return os << "1 (interlaced)";
    case FlagInterlaced::kProgressive:
      return os << "2 (progressive)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, StereoMode value) {
  switch (value) {
    case StereoMode::kMono:
      return os << "0 (mono)";
    case StereoMode::kSideBySideLeftFirst:
      return os << "1 (side-by-side, left eye first)";
    case StereoMode::kTopBottomRightFirst:
      return os << "2 (top-bottom, right eye first)";
    case StereoMode::kTopBottomLeftFirst:
      return os << "3 (top-bottom, left eye first)";
    case StereoMode::kCheckboardRightFirst:
      return os << "4 (checkboard, right eye first)";
    case StereoMode::kCheckboardLeftFirst:
      return os << "5 (checkboard, left eye first)";
    case StereoMode::kRowInterleavedRightFirst:
      return os << "6 (row interleaved, right eye first)";
    case StereoMode::kRowInterleavedLeftFirst:
      return os << "7 (row interleaved, left eye first)";
    case StereoMode::kColumnInterleavedRightFirst:
      return os << "8 (column interleaved, right eye first)";
    case StereoMode::kColumnInterleavedLeftFirst:
      return os << "9 (column interleaved, left eye first)";
    case StereoMode::kAnaglyphCyanRed:
      return os << "10 (anaglyph, cyan/red)";
    case StereoMode::kSideBySideRightFirst:
      return os << "11 (side-by-side, right eye first)";
    case StereoMode::kAnaglyphGreenMagenta:
      return os << "12 (anaglyph, green/magenta)";
    case StereoMode::kBlockLacedLeftFirst:
      return os << "13 (block laced, left eye first)";
    case StereoMode::kBlockLacedRightFirst:
      return os << "14 (block laced, right eye first)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, DisplayUnit value) {
  switch (value) {
    case DisplayUnit::kPixels:
      return os << "0 (pixels)";
    case DisplayUnit::kCentimeters:
      return os << "1 (centimeters)";
    case DisplayUnit::kInches:
      return os << "2 (inches)";
    case DisplayUnit::kDisplayAspectRatio:
      return os << "3 (display aspect ratio)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, AspectRatioType value) {
  switch (value) {
    case AspectRatioType::kFreeResizing:
      return os << "0 (free resizing)";
    case AspectRatioType::kKeep:
      return os << "1 (keep aspect ratio)";
    case AspectRatioType::kFixed:
      return os << "2 (fixed)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, AesSettingsCipherMode value) {
  switch (value) {
    case AesSettingsCipherMode::kCtr:
      return os << "1 (CTR)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, ContentEncAlgo value) {
  switch (value) {
    case ContentEncAlgo::kOnlySigned:
      return os << "0 (only signed, not encrypted)";
    case ContentEncAlgo::kDes:
      return os << "1 (DES)";
    case ContentEncAlgo::k3Des:
      return os << "2 (3DES)";
    case ContentEncAlgo::kTwofish:
      return os << "3 (Twofish)";
    case ContentEncAlgo::kBlowfish:
      return os << "4 (Blowfish)";
    case ContentEncAlgo::kAes:
      return os << "5 (AES)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, ContentEncodingType value) {
  switch (value) {
    case ContentEncodingType::kCompression:
      return os << "0 (compression)";
    case ContentEncodingType::kEncryption:
      return os << "1 (encryption)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

std::ostream& operator<<(std::ostream& os, TrackType value) {
  switch (value) {
    case TrackType::kVideo:
      return os << "1 (video)";
    case TrackType::kAudio:
      return os << "2 (audio)";
    case TrackType::kComplex:
      return os << "3 (complex)";
    case TrackType::kLogo:
      return os << "16 (logo)";
    case TrackType::kSubtitle:
      return os << "17 (subtitle)";
    case TrackType::kButtons:
      return os << "18 (buttons)";
    case TrackType::kControl:
      return os << "32 (control)";
    default:
      return PrintUnknownEnumValue(os, value);
  }
}

// For binary elements, just print out its size.
std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::uint8_t>& value) {
  return os << '<' << value.size() << " bytes>";
}

class DemoCallback : public Callback {
 public:
  int indent = 0;
  int spaces_per_indent = 2;

  void PrintElementMetadata(const std::string& name,
                            const ElementMetadata& metadata) {
    // Since we aren't doing any seeking in this demo, we don't have to worry
    // about kUnknownHeaderSize or kUnknownElementPosition when adding the
    // position and sizes.
    const std::uint64_t header_start = metadata.position;
    const std::uint64_t header_end = header_start + metadata.header_size;
    const std::uint64_t body_start = header_end;
    std::cout << std::string(indent * spaces_per_indent, ' ') << name;
    // The ContentEncAESSettings element has the longest name (out of all other
    // master elements) at 21 characters. It's also the deepest master element
    // at a level of 6. Insert enough whitespace so there's room for it.
    std::cout << std::string(21 + 6 * spaces_per_indent -
                                 indent * spaces_per_indent - name.size(),
                             ' ')
              << "    header: [" << header_start << ", " << header_end
              << ")  body: [" << body_start << ", ";
    if (metadata.size != kUnknownElementSize) {
      const std::uint64_t body_end = body_start + metadata.size;
      std::cout << body_end;
    } else {
      std::cout << '?';
    }
    std::cout << ")\n";
  }

  template <typename T>
  void PrintMandatoryElement(const std::string& name,
                             const Element<T>& element) {
    std::cout << std::string(indent * spaces_per_indent, ' ') << name;
    if (!element.is_present()) {
      std::cout << " (implicit)";
    }
    std::cout << ": " << element.value() << '\n';
  }

  template <typename T>
  void PrintMandatoryElement(const std::string& name,
                             const std::vector<Element<T>>& elements) {
    for (const Element<T>& element : elements) {
      PrintMandatoryElement(name, element);
    }
  }

  template <typename T>
  void PrintOptionalElement(const std::string& name,
                            const Element<T>& element) {
    if (element.is_present()) {
      std::cout << std::string(indent * spaces_per_indent, ' ') << name << ": "
                << element.value() << '\n';
    }
  }

  template <typename T>
  void PrintOptionalElement(const std::string& name,
                            const std::vector<Element<T>>& elements) {
    for (const Element<T>& element : elements) {
      PrintOptionalElement(name, element);
    }
  }

  void PrintMasterElement(const BlockAdditions& block_additions) {
    PrintMasterElement("BlockMore", block_additions.block_mores);
  }

  void PrintMasterElement(const BlockMore& block_more) {
    PrintMandatoryElement("BlockAddID", block_more.id);
    PrintMandatoryElement("BlockAdditional", block_more.data);
  }

  void PrintMasterElement(const Slices& slices) {
    PrintMasterElement("TimeSlice", slices.slices);
  }

  void PrintMasterElement(const TimeSlice& time_slice) {
    PrintOptionalElement("LaceNumber", time_slice.lace_number);
  }

  void PrintMasterElement(const Video& video) {
    PrintMandatoryElement("FlagInterlaced", video.interlaced);
    PrintOptionalElement("StereoMode", video.stereo_mode);
    PrintOptionalElement("AlphaMode", video.alpha_mode);
    PrintMandatoryElement("PixelWidth", video.pixel_width);
    PrintMandatoryElement("PixelHeight", video.pixel_height);
    PrintOptionalElement("PixelCropBottom", video.pixel_crop_bottom);
    PrintOptionalElement("PixelCropTop", video.pixel_crop_top);
    PrintOptionalElement("PixelCropLeft", video.pixel_crop_left);
    PrintOptionalElement("PixelCropRight", video.pixel_crop_right);
    PrintOptionalElement("DisplayWidth", video.display_width);
    PrintOptionalElement("DisplayHeight", video.display_height);
    PrintOptionalElement("DisplayUnit", video.display_unit);
    PrintOptionalElement("AspectRatioType", video.aspect_ratio_type);
    PrintOptionalElement("FrameRate", video.frame_rate);
    PrintMasterElement("Colour", video.colour);
    PrintMasterElement("Projection", video.projection);
  }

  void PrintMasterElement(const Colour& colour) {
    PrintOptionalElement("MatrixCoefficients", colour.matrix_coefficients);
    PrintOptionalElement("BitsPerChannel", colour.bits_per_channel);
    PrintOptionalElement("ChromaSubsamplingHorz", colour.chroma_subsampling_x);
    PrintOptionalElement("ChromaSubsamplingVert", colour.chroma_subsampling_y);
    PrintOptionalElement("CbSubsamplingHorz", colour.cb_subsampling_x);
    PrintOptionalElement("CbSubsamplingVert", colour.cb_subsampling_y);
    PrintOptionalElement("ChromaSitingHorz", colour.chroma_siting_x);
    PrintOptionalElement("ChromaSitingVert", colour.chroma_siting_y);
    PrintOptionalElement("Range", colour.range);
    PrintOptionalElement("TransferCharacteristics",
                         colour.transfer_characteristics);
    PrintOptionalElement("Primaries", colour.primaries);
    PrintOptionalElement("MaxCLL", colour.max_cll);
    PrintOptionalElement("MaxFALL", colour.max_fall);
    PrintMasterElement("MasteringMetadata", colour.mastering_metadata);
  }

  void PrintMasterElement(const MasteringMetadata& mastering_metadata) {
    PrintOptionalElement("PrimaryRChromaticityX",
                         mastering_metadata.primary_r_chromaticity_x);
    PrintOptionalElement("PrimaryRChromaticityY",
                         mastering_metadata.primary_r_chromaticity_y);
    PrintOptionalElement("PrimaryGChromaticityX",
                         mastering_metadata.primary_g_chromaticity_x);
    PrintOptionalElement("PrimaryGChromaticityY",
                         mastering_metadata.primary_g_chromaticity_y);
    PrintOptionalElement("PrimaryBChromaticityX",
                         mastering_metadata.primary_b_chromaticity_x);
    PrintOptionalElement("PrimaryBChromaticityY",
                         mastering_metadata.primary_b_chromaticity_y);
    PrintOptionalElement("WhitePointChromaticityX",
                         mastering_metadata.white_point_chromaticity_x);
    PrintOptionalElement("WhitePointChromaticityY",
                         mastering_metadata.white_point_chromaticity_y);
    PrintOptionalElement("LuminanceMax", mastering_metadata.luminance_max);
    PrintOptionalElement("LuminanceMin", mastering_metadata.luminance_min);
  }

  void PrintMasterElement(const Projection& projection) {
    PrintMandatoryElement("ProjectionType", projection.type);
    PrintOptionalElement("ProjectionPrivate", projection.projection_private);
    PrintMandatoryElement("ProjectionPoseYaw", projection.pose_yaw);
    PrintMandatoryElement("ProjectionPosePitch", projection.pose_pitch);
    PrintMandatoryElement("ProjectionPoseRoll", projection.pose_roll);
  }

  void PrintMasterElement(const Audio& audio) {
    PrintMandatoryElement("SamplingFrequency", audio.sampling_frequency);
    PrintOptionalElement("OutputSamplingFrequency", audio.output_frequency);
    PrintMandatoryElement("Channels", audio.channels);
    PrintOptionalElement("BitDepth", audio.bit_depth);
  }

  void PrintMasterElement(const ContentEncodings& content_encodings) {
    PrintMasterElement("ContentEncoding", content_encodings.encodings);
  }

  void PrintMasterElement(const ContentEncoding& content_encoding) {
    PrintMandatoryElement("ContentEncodingOrder", content_encoding.order);
    PrintMandatoryElement("ContentEncodingScope", content_encoding.scope);
    PrintMandatoryElement("ContentEncodingType", content_encoding.type);
    PrintMasterElement("ContentEncryption", content_encoding.encryption);
  }

  void PrintMasterElement(const ContentEncryption& content_encryption) {
    PrintOptionalElement("ContentEncAlgo", content_encryption.algorithm);
    PrintOptionalElement("ContentEncKeyID", content_encryption.key_id);
    PrintMasterElement("ContentEncAESSettings",
                       content_encryption.aes_settings);
  }

  void PrintMasterElement(
      const ContentEncAesSettings& content_enc_aes_settings) {
    PrintMandatoryElement("AESSettingsCipherMode",
                          content_enc_aes_settings.aes_settings_cipher_mode);
  }

  void PrintMasterElement(const CueTrackPositions& cue_track_positions) {
    PrintMandatoryElement("CueTrack", cue_track_positions.track);
    PrintMandatoryElement("CueClusterPosition",
                          cue_track_positions.cluster_position);
    PrintOptionalElement("CueRelativePosition",
                         cue_track_positions.relative_position);
    PrintOptionalElement("CueDuration", cue_track_positions.duration);
    PrintOptionalElement("CueBlockNumber", cue_track_positions.block_number);
  }

  void PrintMasterElement(const ChapterAtom& chapter_atom) {
    PrintMandatoryElement("ChapterUID", chapter_atom.uid);
    PrintOptionalElement("ChapterStringUID", chapter_atom.string_uid);
    PrintMandatoryElement("ChapterTimeStart", chapter_atom.time_start);
    PrintOptionalElement("ChapterTimeEnd", chapter_atom.time_end);
    PrintMasterElement("ChapterDisplay", chapter_atom.displays);
    PrintMasterElement("ChapterAtom", chapter_atom.atoms);
  }

  void PrintMasterElement(const ChapterDisplay& chapter_display) {
    PrintMandatoryElement("ChapString", chapter_display.string);
    PrintMandatoryElement("ChapLanguage", chapter_display.languages);
    PrintOptionalElement("ChapCountry", chapter_display.countries);
  }

  void PrintMasterElement(const Targets& targets) {
    PrintOptionalElement("TargetTypeValue", targets.type_value);
    PrintOptionalElement("TargetType", targets.type);
    PrintMandatoryElement("TagTrackUID", targets.track_uids);
  }

  void PrintMasterElement(const SimpleTag& simple_tag) {
    PrintMandatoryElement("TagName", simple_tag.name);
    PrintMandatoryElement("TagLanguage", simple_tag.language);
    PrintMandatoryElement("TagDefault", simple_tag.is_default);
    PrintOptionalElement("TagString", simple_tag.string);
    PrintOptionalElement("TagBinary", simple_tag.binary);
    PrintMasterElement("SimpleTag", simple_tag.tags);
  }

  // When printing a master element that's wrapped in Element<>, peel off the
  // Element<> wrapper and print the underlying master element if it's present.
  template <typename T>
  void PrintMasterElement(const std::string& name, const Element<T>& element) {
    if (element.is_present()) {
      std::cout << std::string(indent * spaces_per_indent, ' ') << name << "\n";
      ++indent;
      PrintMasterElement(element.value());
      --indent;
    }
  }

  template <typename T>
  void PrintMasterElement(const std::string& name,
                          const std::vector<Element<T>>& elements) {
    for (const Element<T>& element : elements) {
      PrintMasterElement(name, element);
    }
  }

  template <typename T>
  void PrintValue(const std::string& name, const T& value) {
    std::cout << std::string(indent * spaces_per_indent, ' ') << name << ": "
              << value << '\n';
  }

  Status OnElementBegin(const ElementMetadata& metadata,
                        Action* action) override {
    // Print out metadata for some level 1 elements that don't have explicit
    // callbacks.
    switch (metadata.id) {
      case Id::kSeekHead:
        indent = 1;
        PrintElementMetadata("SeekHead", metadata);
        break;
      case Id::kTracks:
        indent = 1;
        PrintElementMetadata("Tracks", metadata);
        break;
      case Id::kCues:
        indent = 1;
        PrintElementMetadata("Cues", metadata);
        break;
      case Id::kChapters:
        indent = 1;
        PrintElementMetadata("Chapters", metadata);
        break;
      case Id::kTags:
        indent = 1;
        PrintElementMetadata("Tags", metadata);
        break;
      default:
        break;
    }

    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnUnknownElement(const ElementMetadata& metadata, Reader* reader,
                          std::uint64_t* bytes_remaining) override {
    // Output unknown elements without any indentation because we aren't
    // tracking which element contains them.
    int original_indent = indent;
    indent = 0;
    PrintElementMetadata("UNKNOWN_ELEMENT!", metadata);
    indent = original_indent;
    // The base class's implementation will just skip the element via
    // Reader::Skip().
    return Callback::OnUnknownElement(metadata, reader, bytes_remaining);
  }

  Status OnEbml(const ElementMetadata& metadata, const Ebml& ebml) override {
    indent = 0;
    PrintElementMetadata("EBML", metadata);
    indent = 1;
    PrintMandatoryElement("EBMLVersion", ebml.ebml_version);
    PrintMandatoryElement("EBMLReadVersion", ebml.ebml_read_version);
    PrintMandatoryElement("EBMLMaxIDLength", ebml.ebml_max_id_length);
    PrintMandatoryElement("EBMLMaxSizeLength", ebml.ebml_max_size_length);
    PrintMandatoryElement("DocType", ebml.doc_type);
    PrintMandatoryElement("DocTypeVersion", ebml.doc_type_version);
    PrintMandatoryElement("DocTypeReadVersion", ebml.doc_type_read_version);
    return Status(Status::kOkCompleted);
  }

  Status OnVoid(const ElementMetadata& metadata, Reader* reader,
                std::uint64_t* bytes_remaining) override {
    // Output Void elements without any indentation because we aren't tracking
    // which element contains them.
    int original_indent = indent;
    indent = 0;
    PrintElementMetadata("Void", metadata);
    indent = original_indent;
    // The base class's implementation will just skip the element via
    // Reader::Skip().
    return Callback::OnVoid(metadata, reader, bytes_remaining);
  }

  Status OnSegmentBegin(const ElementMetadata& metadata,
                        Action* action) override {
    indent = 0;
    PrintElementMetadata("Segment", metadata);
    indent = 1;
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnSeek(const ElementMetadata& metadata, const Seek& seek) override {
    indent = 2;
    PrintElementMetadata("Seek", metadata);
    indent = 3;
    PrintMandatoryElement("SeekID", seek.id);
    PrintMandatoryElement("SeekPosition", seek.position);
    return Status(Status::kOkCompleted);
  }

  Status OnInfo(const ElementMetadata& metadata, const Info& info) override {
    indent = 1;
    PrintElementMetadata("Info", metadata);
    indent = 2;
    PrintMandatoryElement("TimecodeScale", info.timecode_scale);
    PrintOptionalElement("Duration", info.duration);
    PrintOptionalElement("DateUTC", info.date_utc);
    PrintOptionalElement("Title", info.title);
    PrintOptionalElement("MuxingApp", info.muxing_app);
    PrintOptionalElement("WritingApp", info.writing_app);
    return Status(Status::kOkCompleted);
  }

  Status OnClusterBegin(const ElementMetadata& metadata, const Cluster& cluster,
                        Action* action) override {
    indent = 1;
    PrintElementMetadata("Cluster", metadata);
    // A properly muxed file will have Timecode and PrevSize first before any
    // SimpleBlock or BlockGroups. The parser takes advantage of this and delays
    // calling OnClusterBegin() until it hits the first SimpleBlock or
    // BlockGroup child (or the Cluster ends if it's empty). It's possible for
    // an improperly muxed file to have Timecode or PrevSize after the first
    // block, in which case they'll be absent here and may be accessed in
    // OnClusterEnd() when the Cluster and all its children have been fully
    // parsed. In this demo we assume the file has been properly muxed.
    indent = 2;
    PrintMandatoryElement("Timecode", cluster.timecode);
    PrintOptionalElement("PrevSize", cluster.previous_size);
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnSimpleBlockBegin(const ElementMetadata& metadata,
                            const SimpleBlock& simple_block,
                            Action* action) override {
    indent = 2;
    PrintElementMetadata("SimpleBlock", metadata);
    indent = 3;
    PrintValue("track number", simple_block.track_number);
    PrintValue("frames", simple_block.num_frames);
    PrintValue("timecode", simple_block.timecode);
    PrintValue("lacing", simple_block.lacing);
    std::string flags = (simple_block.is_visible) ? "visible" : "invisible";
    if (simple_block.is_key_frame)
      flags += ", key frame";
    if (simple_block.is_discardable)
      flags += ", discardable";
    PrintValue("flags", flags);
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnSimpleBlockEnd(const ElementMetadata& /* metadata */,
                          const SimpleBlock& /* simple_block */) override {
    return Status(Status::kOkCompleted);
  }

  Status OnBlockGroupBegin(const ElementMetadata& metadata,
                           Action* action) override {
    indent = 2;
    PrintElementMetadata("BlockGroup", metadata);
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnBlockBegin(const ElementMetadata& metadata, const Block& block,
                      Action* action) override {
    indent = 3;
    PrintElementMetadata("Block", metadata);
    indent = 4;
    PrintValue("track number", block.track_number);
    PrintValue("frames", block.num_frames);
    PrintValue("timecode", block.timecode);
    PrintValue("lacing", block.lacing);
    PrintValue("flags", (block.is_visible) ? "visible" : "invisible");
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
  }

  Status OnBlockEnd(const ElementMetadata& /* metadata */,
                    const Block& /* block */) override {
    return Status(Status::kOkCompleted);
  }

  Status OnBlockGroupEnd(const ElementMetadata& /* metadata */,
                         const BlockGroup& block_group) override {
    if (block_group.virtual_block.is_present()) {
      std::cout << std::string(indent * spaces_per_indent, ' ')
                << "BlockVirtual\n";
      indent = 4;
      PrintValue("track number",
                 block_group.virtual_block.value().track_number);
      PrintValue("timecode", block_group.virtual_block.value().timecode);
    }
    indent = 3;
    PrintMasterElement("BlockAdditions", block_group.additions);
    PrintOptionalElement("BlockDuration", block_group.duration);
    PrintOptionalElement("ReferenceBlock", block_group.references);
    PrintOptionalElement("DiscardPadding", block_group.discard_padding);
    PrintMasterElement("Slices", block_group.slices);
    // BlockGroup::block has been set, but we've already printed it in
    // OnBlockBegin().
    return Status(Status::kOkCompleted);
  }

  Status OnFrame(const FrameMetadata& metadata, Reader* reader,
                 std::uint64_t* bytes_remaining) override {
    PrintValue("frame byte range",
               '[' + std::to_string(metadata.position) + ", " +
                   std::to_string(metadata.position + metadata.size) + ')');
    // The base class's implementation will just skip the frame via
    // Reader::Skip().
    return Callback::OnFrame(metadata, reader, bytes_remaining);
  }

  Status OnClusterEnd(const ElementMetadata& /* metadata */,
                      const Cluster& /* cluster */) override {
    // The Cluster and all its children have been fully parsed at this point. If
    // the file wasn't properly muxed and Timecode or PrevSize were missing in
    // OnClusterBegin(), they'll be set here (if the Cluster contained them). In
    // this demo we already handled them, though.
    return Status(Status::kOkCompleted);
  }

  Status OnTrackEntry(const ElementMetadata& metadata,
                      const TrackEntry& track_entry) override {
    indent = 2;
    PrintElementMetadata("TrackEntry", metadata);
    indent = 3;
    PrintMandatoryElement("TrackNumber", track_entry.track_number);
    PrintMandatoryElement("TrackUID", track_entry.track_uid);
    PrintMandatoryElement("TrackType", track_entry.track_type);
    PrintMandatoryElement("FlagEnabled", track_entry.is_enabled);
    PrintMandatoryElement("FlagDefault", track_entry.is_default);
    PrintMandatoryElement("FlagForced", track_entry.is_forced);
    PrintMandatoryElement("FlagLacing", track_entry.uses_lacing);
    PrintOptionalElement("DefaultDuration", track_entry.default_duration);
    PrintOptionalElement("Name", track_entry.name);
    PrintOptionalElement("Language", track_entry.language);
    PrintMandatoryElement("CodecID", track_entry.codec_id);
    PrintOptionalElement("CodecPrivate", track_entry.codec_private);
    PrintOptionalElement("CodecName", track_entry.codec_name);
    PrintOptionalElement("CodecDelay", track_entry.codec_delay);
    PrintMandatoryElement("SeekPreRoll", track_entry.seek_pre_roll);
    PrintMasterElement("Video", track_entry.video);
    PrintMasterElement("Audio", track_entry.audio);
    PrintMasterElement("ContentEncodings", track_entry.content_encodings);
    return Status(Status::kOkCompleted);
  }

  Status OnCuePoint(const ElementMetadata& metadata,
                    const CuePoint& cue_point) override {
    indent = 2;
    PrintElementMetadata("CuePoint", metadata);
    indent = 3;
    PrintMandatoryElement("CueTime", cue_point.time);
    PrintMasterElement("CueTrackPositions", cue_point.cue_track_positions);
    return Status(Status::kOkCompleted);
  }

  Status OnEditionEntry(const ElementMetadata& metadata,
                        const EditionEntry& edition_entry) override {
    indent = 2;
    PrintElementMetadata("EditionEntry", metadata);
    indent = 3;
    PrintMasterElement("ChapterAtom", edition_entry.atoms);
    return Status(Status::kOkCompleted);
  }

  Status OnTag(const ElementMetadata& metadata, const Tag& tag) override {
    indent = 2;
    PrintElementMetadata("Tag", metadata);
    indent = 3;
    PrintMasterElement("Targets", tag.targets);
    PrintMasterElement("SimpleTag", tag.tags);
    return Status(Status::kOkCompleted);
  }

  Status OnSegmentEnd(const ElementMetadata& /* metadata */) override {
    return Status(Status::kOkCompleted);
  }
};

int main(int argc, char* argv[]) {
  if ((argc != 1 && argc != 2) ||
      (argc == 2 && argv[1] == std::string("--help"))) {
    std::cerr << "Usage:\n"
              << argv[0] << " [path-to-webm-file]\n\n"
              << "Prints info for the WebM file specified in the command line. "
                 "If no file is\n"
              << "specified, stdin is used as input.\n";
    return EXIT_FAILURE;
  }

  FILE* file = (argc == 2) ? std::fopen(argv[1], "rb")
                           : std::freopen(nullptr, "rb", stdin);
  if (!file) {
    std::cerr << "File cannot be opened\n";
    return EXIT_FAILURE;
  }

  FileReader reader(file);
  DemoCallback callback;
  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  if (!status.completed_ok()) {
    std::cerr << "Parsing error; status code: " << status.code << '\n';
    return EXIT_FAILURE;
  }

  return 0;
}
