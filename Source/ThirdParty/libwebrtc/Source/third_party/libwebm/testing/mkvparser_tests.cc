// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <string>

#include "common/hdr_util.h"
#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"
#include "testing/test_util.h"

using mkvparser::AudioTrack;
using mkvparser::Block;
using mkvparser::BlockEntry;
using mkvparser::BlockGroup;
using mkvparser::Cluster;
using mkvparser::CuePoint;
using mkvparser::Cues;
using mkvparser::MkvReader;
using mkvparser::Segment;
using mkvparser::SegmentInfo;
using mkvparser::Track;
using mkvparser::Tracks;
using mkvparser::VideoTrack;

namespace test {

// Base class containing boiler plate stuff.
class ParserTest : public testing::Test {
 public:
  ParserTest() : is_reader_open_(false), segment_(NULL) {
    memset(dummy_data_, -1, kFrameLength);
    memset(gold_frame_, 0, kFrameLength);
  }

  virtual ~ParserTest() {
    CloseReader();
    if (segment_ != NULL) {
      delete segment_;
      segment_ = NULL;
    }
  }

  void CloseReader() {
    if (is_reader_open_) {
      reader_.Close();
    }
    is_reader_open_ = false;
  }

  bool CreateAndLoadSegment(const std::string& filename,
                            int expected_doc_type_ver) {
    filename_ = GetTestFilePath(filename);
    if (reader_.Open(filename_.c_str())) {
      return false;
    }
    is_reader_open_ = true;
    pos_ = 0;
    mkvparser::EBMLHeader ebml_header;
    ebml_header.Parse(&reader_, pos_);
    EXPECT_EQ(1, ebml_header.m_version);
    EXPECT_EQ(1, ebml_header.m_readVersion);
    EXPECT_STREQ("webm", ebml_header.m_docType);
    EXPECT_EQ(expected_doc_type_ver, ebml_header.m_docTypeVersion);
    EXPECT_EQ(2, ebml_header.m_docTypeReadVersion);

    if (mkvparser::Segment::CreateInstance(&reader_, pos_, segment_)) {
      return false;
    }
    return !HasFailure() && segment_->Load() >= 0;
  }

  bool CreateAndLoadSegment(const std::string& filename) {
    return CreateAndLoadSegment(filename, 4);
  }

  void CreateSegmentNoHeaderChecks(const std::string& filename) {
    filename_ = GetTestFilePath(filename);
    ASSERT_NE(0u, filename_.length());
    ASSERT_EQ(0, reader_.Open(filename_.c_str()));
    mkvparser::EBMLHeader ebml_header;
    ASSERT_EQ(0, ebml_header.Parse(&reader_, pos_));
    ASSERT_EQ(0, mkvparser::Segment::CreateInstance(&reader_, pos_, segment_));
  }

  void CompareBlockContents(const Cluster* const cluster,
                            const Block* const block, std::uint64_t timestamp,
                            int track_number, bool is_key, int frame_count) {
    ASSERT_TRUE(block != NULL);
    EXPECT_EQ(track_number, block->GetTrackNumber());
    EXPECT_EQ(static_cast<long long>(timestamp), block->GetTime(cluster));
    EXPECT_EQ(is_key, block->IsKey());
    EXPECT_EQ(frame_count, block->GetFrameCount());
    const Block::Frame& frame = block->GetFrame(0);
    EXPECT_EQ(kFrameLength, frame.len);
    std::memset(dummy_data_, -1, kFrameLength);
    frame.Read(&reader_, dummy_data_);
    EXPECT_EQ(0, std::memcmp(gold_frame_, dummy_data_, kFrameLength));
  }

  void CompareCuePointContents(const Track* const track,
                               const CuePoint* const cue_point,
                               std::uint64_t timestamp, int track_number,
                               std::uint64_t pos) {
    ASSERT_TRUE(cue_point != NULL);
    EXPECT_EQ(static_cast<long long>(timestamp), cue_point->GetTime(segment_));
    const CuePoint::TrackPosition* const track_position =
        cue_point->Find(track);
    EXPECT_EQ(track_number, track_position->m_track);
    EXPECT_EQ(static_cast<long long>(pos), track_position->m_pos);
  }

 protected:
  MkvReader reader_;
  bool is_reader_open_;
  Segment* segment_;
  std::string filename_;
  long long pos_;
  std::uint8_t dummy_data_[kFrameLength];
  std::uint8_t gold_frame_[kFrameLength];
};

TEST_F(ParserTest, SegmentInfo) {
  ASSERT_TRUE(CreateAndLoadSegment("segment_info.webm"));
  const SegmentInfo* const info = segment_->GetInfo();
  EXPECT_EQ(kTimeCodeScale, info->GetTimeCodeScale());
  EXPECT_STREQ(kAppString, info->GetMuxingAppAsUTF8());
  EXPECT_STREQ(kAppString, info->GetWritingAppAsUTF8());
}

TEST_F(ParserTest, TrackEntries) {
  ASSERT_TRUE(CreateAndLoadSegment("tracks.webm"));
  const Tracks* const tracks = segment_->GetTracks();
  const unsigned int kTracksCount = 2;
  EXPECT_EQ(kTracksCount, tracks->GetTracksCount());
  for (int i = 0; i < 2; ++i) {
    const Track* const track = tracks->GetTrackByIndex(i);
    ASSERT_TRUE(track != NULL);
    EXPECT_STREQ(kTrackName, track->GetNameAsUTF8());
    if (track->GetType() == Track::kVideo) {
      const VideoTrack* const video_track =
          dynamic_cast<const VideoTrack*>(track);
      EXPECT_EQ(kWidth, static_cast<int>(video_track->GetWidth()));
      EXPECT_EQ(kHeight, static_cast<int>(video_track->GetHeight()));
      EXPECT_STREQ(kVP8CodecId, video_track->GetCodecId());
      EXPECT_DOUBLE_EQ(kVideoFrameRate, video_track->GetFrameRate());
      const unsigned int kTrackUid = 1;
      EXPECT_EQ(kTrackUid, video_track->GetUid());
    } else if (track->GetType() == Track::kAudio) {
      const AudioTrack* const audio_track =
          dynamic_cast<const AudioTrack*>(track);
      EXPECT_EQ(kSampleRate, audio_track->GetSamplingRate());
      EXPECT_EQ(kChannels, audio_track->GetChannels());
      EXPECT_EQ(kBitDepth, audio_track->GetBitDepth());
      EXPECT_STREQ(kVorbisCodecId, audio_track->GetCodecId());
      const unsigned int kTrackUid = 2;
      EXPECT_EQ(kTrackUid, audio_track->GetUid());
    }
  }
}

TEST_F(ParserTest, SimpleBlock) {
  ASSERT_TRUE(CreateAndLoadSegment("simple_block.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // Get the cluster
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  // Get the first block
  const BlockEntry* block_entry;
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 0, kVideoTrackNumber,
                       false, 1);

  // Get the second block
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 2000000,
                       kVideoTrackNumber, false, 1);

  // End of Stream
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  ASSERT_EQ(NULL, block_entry);
  cluster = segment_->GetNext(cluster);
  EXPECT_TRUE(cluster->EOS());
}

TEST_F(ParserTest, MultipleClusters) {
  ASSERT_TRUE(CreateAndLoadSegment("force_new_cluster.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // Get the first cluster
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  // Get the first block
  const BlockEntry* block_entry;
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 0, kVideoTrackNumber,
                       false, 1);

  // Get the second cluster
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  EXPECT_EQ(NULL, block_entry);
  cluster = segment_->GetNext(cluster);
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  // Get the second block
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 2000000,
                       kVideoTrackNumber, false, 1);

  // Get the third block
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 4000000,
                       kVideoTrackNumber, false, 1);

  // Get the third cluster
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  EXPECT_EQ(NULL, block_entry);
  cluster = segment_->GetNext(cluster);
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  // Get the fourth block
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  CompareBlockContents(cluster, block_entry->GetBlock(), 6000000,
                       kVideoTrackNumber, false, 1);

  // End of Stream
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  EXPECT_EQ(NULL, block_entry);
  cluster = segment_->GetNext(cluster);
  EXPECT_TRUE(cluster->EOS());
}

TEST_F(ParserTest, BlockGroup) {
  ASSERT_TRUE(CreateAndLoadSegment("metadata_block.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // Get the cluster
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  // Get the first block
  const BlockEntry* block_entry;
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  EXPECT_EQ(BlockEntry::Kind::kBlockGroup, block_entry->GetKind());
  const BlockGroup* block_group = static_cast<const BlockGroup*>(block_entry);
  EXPECT_EQ(2, block_group->GetDurationTimeCode());
  CompareBlockContents(cluster, block_group->GetBlock(), 0,
                       kMetadataTrackNumber, true, 1);

  // Get the second block
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());
  EXPECT_EQ(BlockEntry::Kind::kBlockGroup, block_entry->GetKind());
  block_group = static_cast<const BlockGroup*>(block_entry);
  EXPECT_EQ(6, block_group->GetDurationTimeCode());
  CompareBlockContents(cluster, block_group->GetBlock(), 2000000,
                       kMetadataTrackNumber, true, 1);

  // End of Stream
  EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  EXPECT_EQ(NULL, block_entry);
  cluster = segment_->GetNext(cluster);
  EXPECT_TRUE(cluster->EOS());
}

TEST_F(ParserTest, Cues) {
  ASSERT_TRUE(CreateAndLoadSegment("output_cues.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  const Track* const track = segment_->GetTracks()->GetTrackByIndex(0);
  const Cues* const cues = segment_->GetCues();
  ASSERT_TRUE(cues != NULL);
  while (!cues->DoneParsing()) {
    cues->LoadCuePoint();
  }
  EXPECT_EQ(3, cues->GetCount());

  // Get first Cue Point
  const CuePoint* cue_point = cues->GetFirst();
  CompareCuePointContents(track, cue_point, 0, kVideoTrackNumber, 206);

  // Get second Cue Point
  cue_point = cues->GetNext(cue_point);
  CompareCuePointContents(track, cue_point, 6000000, kVideoTrackNumber, 269);

  // Get third (also last) Cue Point
  cue_point = cues->GetNext(cue_point);
  const CuePoint* last_cue_point = cues->GetLast();
  EXPECT_TRUE(cue_point == last_cue_point);
  CompareCuePointContents(track, cue_point, 4000000, kVideoTrackNumber, 269);

  EXPECT_TRUE(ValidateCues(segment_, &reader_));
}

TEST_F(ParserTest, CuesBeforeClusters) {
  ASSERT_TRUE(CreateAndLoadSegment("cues_before_clusters.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  const Track* const track = segment_->GetTracks()->GetTrackByIndex(0);
  const Cues* const cues = segment_->GetCues();
  ASSERT_TRUE(cues != NULL);
  while (!cues->DoneParsing()) {
    cues->LoadCuePoint();
  }
  EXPECT_EQ(2, cues->GetCount());

  // Get first Cue Point
  const CuePoint* cue_point = cues->GetFirst();
  CompareCuePointContents(track, cue_point, 0, kVideoTrackNumber, 238);

  // Get second (also last) Cue Point
  cue_point = cues->GetNext(cue_point);
  const CuePoint* last_cue_point = cues->GetLast();
  EXPECT_TRUE(cue_point == last_cue_point);
  CompareCuePointContents(track, cue_point, 6000000, kVideoTrackNumber, 301);

  EXPECT_TRUE(ValidateCues(segment_, &reader_));
}

TEST_F(ParserTest, CuesTrackNumber) {
  ASSERT_TRUE(CreateAndLoadSegment("set_cues_track_number.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  const Track* const track = segment_->GetTracks()->GetTrackByIndex(0);
  const Cues* const cues = segment_->GetCues();
  ASSERT_TRUE(cues != NULL);
  while (!cues->DoneParsing()) {
    cues->LoadCuePoint();
  }
  EXPECT_EQ(2, cues->GetCount());

  // Get first Cue Point
  const CuePoint* cue_point = cues->GetFirst();
  CompareCuePointContents(track, cue_point, 0, 10, 206);

  // Get second (also last) Cue Point
  cue_point = cues->GetNext(cue_point);
  const CuePoint* last_cue_point = cues->GetLast();
  EXPECT_TRUE(cue_point == last_cue_point);
  CompareCuePointContents(track, cue_point, 6000000, 10, 269);

  EXPECT_TRUE(ValidateCues(segment_, &reader_));
}

TEST_F(ParserTest, Opus) {
  ASSERT_TRUE(CreateAndLoadSegment("bbb_480p_vp9_opus_1second.webm", 4));
  const unsigned int kTracksCount = 2;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // --------------------------------------------------------------------------
  // Track Header validation.
  const Tracks* const tracks = segment_->GetTracks();
  EXPECT_EQ(kTracksCount, tracks->GetTracksCount());
  for (int i = 0; i < 2; ++i) {
    const Track* const track = tracks->GetTrackByIndex(i);
    ASSERT_TRUE(track != NULL);

    EXPECT_EQ(NULL, track->GetNameAsUTF8());
    EXPECT_STREQ("und", track->GetLanguage());
    EXPECT_EQ(i + 1, track->GetNumber());
    EXPECT_FALSE(track->GetLacing());

    if (track->GetType() == Track::kVideo) {
      const VideoTrack* const video_track =
          dynamic_cast<const VideoTrack*>(track);
      EXPECT_EQ(854, static_cast<int>(video_track->GetWidth()));
      EXPECT_EQ(480, static_cast<int>(video_track->GetHeight()));
      EXPECT_STREQ(kVP9CodecId, video_track->GetCodecId());
      EXPECT_DOUBLE_EQ(0., video_track->GetFrameRate());
      EXPECT_EQ(41666666,
                static_cast<int>(video_track->GetDefaultDuration()));  // 24.000
      const unsigned int kVideoUid = kVideoTrackNumber;
      EXPECT_EQ(kVideoUid, video_track->GetUid());
      const unsigned int kCodecDelay = 0;
      EXPECT_EQ(kCodecDelay, video_track->GetCodecDelay());
      const unsigned int kSeekPreRoll = 0;
      EXPECT_EQ(kSeekPreRoll, video_track->GetSeekPreRoll());

      size_t video_codec_private_size;
      EXPECT_EQ(NULL, video_track->GetCodecPrivate(video_codec_private_size));
      const unsigned int kPrivateSize = 0;
      EXPECT_EQ(kPrivateSize, video_codec_private_size);
    } else if (track->GetType() == Track::kAudio) {
      const AudioTrack* const audio_track =
          dynamic_cast<const AudioTrack*>(track);
      EXPECT_EQ(48000, audio_track->GetSamplingRate());
      EXPECT_EQ(6, audio_track->GetChannels());
      EXPECT_EQ(32, audio_track->GetBitDepth());
      EXPECT_STREQ(kOpusCodecId, audio_track->GetCodecId());
      EXPECT_EQ(kAudioTrackNumber, static_cast<int>(audio_track->GetUid()));
      const unsigned int kDefaultDuration = 0;
      EXPECT_EQ(kDefaultDuration, audio_track->GetDefaultDuration());
      EXPECT_EQ(kOpusCodecDelay, audio_track->GetCodecDelay());
      EXPECT_EQ(kOpusSeekPreroll, audio_track->GetSeekPreRoll());

      size_t audio_codec_private_size;
      EXPECT_TRUE(audio_track->GetCodecPrivate(audio_codec_private_size) !=
                  NULL);
      EXPECT_GE(audio_codec_private_size, kOpusPrivateDataSizeMinimum);
    }
  }

  // --------------------------------------------------------------------------
  // Parse the file to do block-level validation.
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());

  for (; cluster != NULL && !cluster->EOS();
       cluster = segment_->GetNext(cluster)) {
    // Get the first block
    const BlockEntry* block_entry;
    EXPECT_EQ(0, cluster->GetFirst(block_entry));
    ASSERT_TRUE(block_entry != NULL);
    EXPECT_FALSE(block_entry->EOS());

    while (block_entry != NULL && !block_entry->EOS()) {
      const Block* const block = block_entry->GetBlock();
      ASSERT_TRUE(block != NULL);
      EXPECT_FALSE(block->IsInvisible());
      EXPECT_EQ(Block::kLacingNone, block->GetLacing());

      const std::uint32_t track_number =
          static_cast<std::uint32_t>(block->GetTrackNumber());
      const Track* const track = tracks->GetTrackByNumber(track_number);
      ASSERT_TRUE(track != NULL);
      EXPECT_EQ(track->GetNumber(), block->GetTrackNumber());
      const unsigned int kContentEncodingCount = 0;
      EXPECT_EQ(kContentEncodingCount,
                track->GetContentEncodingCount());  // no encryption

      const std::int64_t track_type = track->GetType();
      EXPECT_TRUE(track_type == Track::kVideo || track_type == Track::kAudio);
      if (track_type == Track::kVideo) {
        EXPECT_EQ(BlockEntry::kBlockSimple, block_entry->GetKind());
        EXPECT_EQ(0, block->GetDiscardPadding());
      } else {
        EXPECT_TRUE(block->IsKey());
        const std::int64_t kLastAudioTimecode = 1001;
        const std::int64_t timecode = block->GetTimeCode(cluster);
        // Only the final Opus block should have discard padding.
        if (timecode == kLastAudioTimecode) {
          EXPECT_EQ(BlockEntry::kBlockGroup, block_entry->GetKind());
          EXPECT_EQ(13500000, block->GetDiscardPadding());
        } else {
          EXPECT_EQ(BlockEntry::kBlockSimple, block_entry->GetKind());
          EXPECT_EQ(0, block->GetDiscardPadding());
        }
      }

      const int frame_count = block->GetFrameCount();
      const Block::Frame& frame = block->GetFrame(0);
      EXPECT_EQ(1, frame_count);
      EXPECT_GT(frame.len, 0);

      EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
    }
  }

  ASSERT_TRUE(cluster != NULL);
  EXPECT_TRUE(cluster->EOS());
}

TEST_F(ParserTest, DiscardPadding) {
  // Test an artificial file with some extreme DiscardPadding values.
  const std::string file = "discard_padding.webm";
  ASSERT_TRUE(CreateAndLoadSegment(file, 4));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  // --------------------------------------------------------------------------
  // Track Header validation.
  const Tracks* const tracks = segment_->GetTracks();
  EXPECT_EQ(kTracksCount, tracks->GetTracksCount());
  const Track* const track = tracks->GetTrackByIndex(0);
  ASSERT_TRUE(track != NULL);

  EXPECT_STREQ(NULL, track->GetNameAsUTF8());
  EXPECT_EQ(NULL, track->GetLanguage());
  EXPECT_EQ(kAudioTrackNumber, track->GetNumber());
  EXPECT_TRUE(track->GetLacing());

  EXPECT_EQ(Track::kAudio, track->GetType());
  const AudioTrack* const audio_track = dynamic_cast<const AudioTrack*>(track);
  EXPECT_EQ(30, audio_track->GetSamplingRate());
  EXPECT_EQ(2, audio_track->GetChannels());
  EXPECT_STREQ(kOpusCodecId, audio_track->GetCodecId());
  EXPECT_EQ(kAudioTrackNumber, static_cast<int>(audio_track->GetUid()));
  const unsigned int kDefaultDuration = 0;
  EXPECT_EQ(kDefaultDuration, audio_track->GetDefaultDuration());
  const unsigned int kCodecDelay = 0;
  EXPECT_EQ(kCodecDelay, audio_track->GetCodecDelay());
  const unsigned int kSeekPreRoll = 0;
  EXPECT_EQ(kSeekPreRoll, audio_track->GetSeekPreRoll());

  size_t audio_codec_private_size;
  EXPECT_EQ(NULL, audio_track->GetCodecPrivate(audio_codec_private_size));
  const unsigned int kPrivateSize = 0;
  EXPECT_EQ(kPrivateSize, audio_codec_private_size);

  // --------------------------------------------------------------------------
  // Parse the file to do block-level validation.
  const Cluster* cluster = segment_->GetFirst();
  ASSERT_TRUE(cluster != NULL);
  EXPECT_FALSE(cluster->EOS());
  const unsigned int kSegmentCount = 1;
  EXPECT_EQ(kSegmentCount, segment_->GetCount());

  // Get the first block
  const BlockEntry* block_entry;
  EXPECT_EQ(0, cluster->GetFirst(block_entry));
  ASSERT_TRUE(block_entry != NULL);
  EXPECT_FALSE(block_entry->EOS());

  const std::array<int, 3> discard_padding = {{12810000, 127, -128}};
  int index = 0;
  while (block_entry != NULL && !block_entry->EOS()) {
    const Block* const block = block_entry->GetBlock();
    ASSERT_TRUE(block != NULL);
    EXPECT_FALSE(block->IsInvisible());
    EXPECT_EQ(Block::kLacingNone, block->GetLacing());

    const std::uint32_t track_number =
        static_cast<std::uint32_t>(block->GetTrackNumber());
    const Track* const track = tracks->GetTrackByNumber(track_number);
    ASSERT_TRUE(track != NULL);
    EXPECT_EQ(track->GetNumber(), block->GetTrackNumber());
    const unsigned int kContentEncodingCount = 0;
    EXPECT_EQ(kContentEncodingCount,
              track->GetContentEncodingCount());  // no encryption

    const std::int64_t track_type = track->GetType();
    EXPECT_EQ(Track::kAudio, track_type);
    EXPECT_TRUE(block->IsKey());

    // All blocks have DiscardPadding.
    EXPECT_EQ(BlockEntry::kBlockGroup, block_entry->GetKind());
    ASSERT_LT(index, static_cast<int>(discard_padding.size()));
    EXPECT_EQ(discard_padding[index], block->GetDiscardPadding());
    ++index;

    const int frame_count = block->GetFrameCount();
    const Block::Frame& frame = block->GetFrame(0);
    EXPECT_EQ(1, frame_count);
    EXPECT_GT(frame.len, 0);

    EXPECT_EQ(0, cluster->GetNext(block_entry, block_entry));
  }

  cluster = segment_->GetNext(cluster);
  ASSERT_TRUE(cluster != NULL);
  EXPECT_TRUE(cluster->EOS());
}

TEST_F(ParserTest, StereoModeParsedCorrectly) {
  ASSERT_TRUE(CreateAndLoadSegment("test_stereo_left_right.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());

  const VideoTrack* const video_track = dynamic_cast<const VideoTrack*>(
      segment_->GetTracks()->GetTrackByIndex(0));

  EXPECT_EQ(1, video_track->GetStereoMode());
  EXPECT_EQ(256, video_track->GetWidth());
  EXPECT_EQ(144, video_track->GetHeight());
  EXPECT_EQ(128, video_track->GetDisplayWidth());
  EXPECT_EQ(144, video_track->GetDisplayHeight());
}

TEST_F(ParserTest, CanParseColour) {
  ASSERT_TRUE(CreateAndLoadSegment("colour.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());
  const VideoTrack* const video_track = dynamic_cast<const VideoTrack*>(
      segment_->GetTracks()->GetTrackByIndex(0));

  const mkvparser::Colour* const colour = video_track->GetColour();
  ASSERT_TRUE(colour != nullptr);
  EXPECT_EQ(0u, colour->matrix_coefficients);
  EXPECT_EQ(1u, colour->bits_per_channel);
  EXPECT_EQ(2u, colour->chroma_subsampling_horz);
  EXPECT_EQ(3u, colour->chroma_subsampling_vert);
  EXPECT_EQ(4u, colour->cb_subsampling_horz);
  EXPECT_EQ(5u, colour->cb_subsampling_vert);
  EXPECT_EQ(1u, colour->chroma_siting_horz);
  EXPECT_EQ(1u, colour->chroma_siting_vert);
  EXPECT_EQ(2u, colour->range);
  EXPECT_EQ(9u, colour->transfer_characteristics);
  EXPECT_EQ(10u, colour->primaries);
  EXPECT_EQ(11u, colour->max_cll);
  EXPECT_EQ(12u, colour->max_fall);

  const mkvparser::MasteringMetadata* const mm =
      video_track->GetColour()->mastering_metadata;
  ASSERT_TRUE(mm != nullptr);
  ASSERT_TRUE(mm->r != nullptr);
  ASSERT_TRUE(mm->g != nullptr);
  ASSERT_TRUE(mm->b != nullptr);
  ASSERT_TRUE(mm->white_point != nullptr);
  EXPECT_FLOAT_EQ(.1, mm->r->x);
  EXPECT_FLOAT_EQ(.2, mm->r->y);
  EXPECT_FLOAT_EQ(.1, mm->g->x);
  EXPECT_FLOAT_EQ(.2, mm->g->y);
  EXPECT_FLOAT_EQ(.1, mm->b->x);
  EXPECT_FLOAT_EQ(.2, mm->b->y);
  EXPECT_FLOAT_EQ(.1, mm->white_point->x);
  EXPECT_FLOAT_EQ(.2, mm->white_point->y);
  EXPECT_FLOAT_EQ(30.0, mm->luminance_min);
  EXPECT_FLOAT_EQ(40.0, mm->luminance_max);
}

TEST_F(ParserTest, CanParseProjection) {
  ASSERT_TRUE(CreateAndLoadSegment("projection.webm"));
  const unsigned int kTracksCount = 1;
  EXPECT_EQ(kTracksCount, segment_->GetTracks()->GetTracksCount());
  const VideoTrack* const video_track =
      static_cast<const VideoTrack*>(segment_->GetTracks()->GetTrackByIndex(0));

  const mkvparser::Projection* const projection = video_track->GetProjection();
  ASSERT_TRUE(projection != nullptr);
  EXPECT_EQ(mkvparser::Projection::kRectangular, projection->type);
  EXPECT_FLOAT_EQ(1, projection->pose_yaw);
  EXPECT_FLOAT_EQ(2, projection->pose_pitch);
  EXPECT_FLOAT_EQ(3, projection->pose_roll);
  EXPECT_EQ(1u, projection->private_data_length);
  ASSERT_TRUE(projection->private_data != nullptr);
  EXPECT_EQ(4u, projection->private_data[0]);
}

TEST_F(ParserTest, Vp9CodecLevelTest) {
  const int kCodecPrivateLength = 3;
  const uint8_t good_codec_private_level[kCodecPrivateLength] = {2, 1, 11};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&good_codec_private_level[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.profile);
  EXPECT_EQ(11, features.level);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.bit_depth);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent,
            features.chroma_subsampling);
}

TEST_F(ParserTest, Vp9CodecProfileTest) {
  const int kCodecPrivateLength = 3;
  const uint8_t good_codec_private_profile[kCodecPrivateLength] = {1, 1, 1};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&good_codec_private_profile[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(1, features.profile);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.level);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.bit_depth);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent,
            features.chroma_subsampling);
}

TEST_F(ParserTest, Vp9CodecBitDepthTest) {
  const int kCodecPrivateLength = 3;
  const uint8_t good_codec_private_profile[kCodecPrivateLength] = {3, 1, 8};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&good_codec_private_profile[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.profile);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.level);
  EXPECT_EQ(8, features.bit_depth);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent,
            features.chroma_subsampling);
}

TEST_F(ParserTest, Vp9CodecChromaSubsamplingTest) {
  const int kCodecPrivateLength = 3;
  const uint8_t good_codec_private_profile[kCodecPrivateLength] = {4, 1, 0};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&good_codec_private_profile[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.profile);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.level);
  EXPECT_EQ(libwebm::Vp9CodecFeatures::kValueNotPresent, features.bit_depth);
  EXPECT_EQ(0, features.chroma_subsampling);
}

TEST_F(ParserTest, Vp9CodecProfileLevelTest) {
  const int kCodecPrivateLength = 6;
  const uint8_t codec_private[kCodecPrivateLength] = {1, 1, 1, 2, 1, 11};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&codec_private[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(1, features.profile);
  EXPECT_EQ(11, features.level);
}

TEST_F(ParserTest, Vp9CodecAllTest) {
  const int kCodecPrivateLength = 12;
  const uint8_t codec_private[kCodecPrivateLength] = {1, 1, 1, 2, 1, 11,
                                                      3, 1, 8, 4, 1, 0};
  libwebm::Vp9CodecFeatures features;
  EXPECT_TRUE(libwebm::ParseVpxCodecPrivate(&codec_private[0],
                                            kCodecPrivateLength, &features));
  EXPECT_EQ(1, features.profile);
  EXPECT_EQ(11, features.level);
  EXPECT_EQ(8, features.bit_depth);
  EXPECT_EQ(0, features.chroma_subsampling);
}

TEST_F(ParserTest, Vp9CodecPrivateBadTest) {
  const int kCodecPrivateLength = 3;
  libwebm::Vp9CodecFeatures features;
  // Test invalid codec private data; all of these should return false.
  const uint8_t bad_codec_private[kCodecPrivateLength] = {0, 0, 0};
  EXPECT_FALSE(
      libwebm::ParseVpxCodecPrivate(NULL, kCodecPrivateLength, &features));
  EXPECT_FALSE(
      libwebm::ParseVpxCodecPrivate(&bad_codec_private[0], 0, &features));
  EXPECT_FALSE(libwebm::ParseVpxCodecPrivate(&bad_codec_private[0],
                                             kCodecPrivateLength, &features));
  const uint8_t good_codec_private_level[kCodecPrivateLength] = {2, 1, 11};

  // Test parse of codec private chunks, but lie about length.
  EXPECT_FALSE(
      libwebm::ParseVpxCodecPrivate(&bad_codec_private[0], 0, &features));
  EXPECT_FALSE(libwebm::ParseVpxCodecPrivate(&good_codec_private_level[0], 0,
                                             &features));
  EXPECT_FALSE(libwebm::ParseVpxCodecPrivate(&good_codec_private_level[0],
                                             kCodecPrivateLength, NULL));
}

TEST_F(ParserTest, InvalidTruncatedChapterString) {
  ASSERT_NO_FATAL_FAILURE(CreateSegmentNoHeaderChecks(
      "invalid/chapters_truncated_chapter_string.mkv"));
  EXPECT_EQ(mkvparser::E_PARSE_FAILED, segment_->Load());
}

TEST_F(ParserTest, InvalidTruncatedChapterString2) {
  ASSERT_NO_FATAL_FAILURE(CreateSegmentNoHeaderChecks(
      "invalid/chapters_truncated_chapter_string_2.mkv"));
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID, segment_->Load());
}

TEST_F(ParserTest, InvalidFixedLacingSize) {
  ASSERT_NO_FATAL_FAILURE(
      CreateSegmentNoHeaderChecks("invalid/fixed_lacing_bad_lace_size.mkv"));
  ASSERT_EQ(0, segment_->Load());
  const mkvparser::BlockEntry* block_entry = NULL;
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID,
            segment_->GetFirst()->GetFirst(block_entry));
}

TEST_F(ParserTest, InvalidBlockEndsBeyondCluster) {
  ASSERT_NO_FATAL_FAILURE(
      CreateSegmentNoHeaderChecks("invalid/block_ends_beyond_cluster.mkv"));
  ASSERT_EQ(0, segment_->Load());
  const mkvparser::BlockEntry* block_entry = NULL;
  EXPECT_EQ(0, segment_->GetFirst()->GetFirst(block_entry));
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID,
            segment_->GetFirst()->GetNext(block_entry, block_entry));
}

TEST_F(ParserTest, InvalidBlockGroupBlockEndsBlockGroup) {
  ASSERT_NO_FATAL_FAILURE(CreateSegmentNoHeaderChecks(
      "invalid/blockgroup_block_ends_beyond_blockgroup.mkv"));
  ASSERT_EQ(0, segment_->Load());
  const mkvparser::BlockEntry* block_entry = NULL;
  EXPECT_EQ(0, segment_->GetFirst()->GetFirst(block_entry));
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID,
            segment_->GetFirst()->GetNext(block_entry, block_entry));
}

TEST_F(ParserTest, InvalidProjectionFloatOverflow) {
  ASSERT_NO_FATAL_FAILURE(
      CreateSegmentNoHeaderChecks("invalid/projection_float_overflow.webm"));
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID, segment_->Load());
}

TEST_F(ParserTest, InvalidPrimaryChromaticityParseFail) {
  ASSERT_NO_FATAL_FAILURE(CreateSegmentNoHeaderChecks(
      "invalid/primarychromaticity_fieldtoolarge.webm"));
  EXPECT_EQ(mkvparser::E_FILE_FORMAT_INVALID, segment_->Load());
}

}  // namespace test

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
