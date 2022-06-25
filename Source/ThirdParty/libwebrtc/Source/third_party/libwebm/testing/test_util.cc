// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "testing/test_util.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <string>

#include "common/libwebm_util.h"
#include "common/webmids.h"

#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"

namespace test {

std::string GetTestDataDir() {
  const char* test_data_path = std::getenv("LIBWEBM_TEST_DATA_PATH");
  return test_data_path ? std::string(test_data_path) : std::string();
}

std::string GetTestFilePath(const std::string& name) {
  const std::string libwebm_testdata_dir = GetTestDataDir();
  return libwebm_testdata_dir + "/" + name;
}

bool CompareFiles(const std::string& file1, const std::string& file2) {
  const std::size_t kBlockSize = 4096;
  std::uint8_t buf1[kBlockSize] = {0};
  std::uint8_t buf2[kBlockSize] = {0};

  libwebm::FilePtr f1 =
      libwebm::FilePtr(std::fopen(file1.c_str(), "rb"), libwebm::FILEDeleter());
  libwebm::FilePtr f2 =
      libwebm::FilePtr(std::fopen(file2.c_str(), "rb"), libwebm::FILEDeleter());

  if (!f1.get() || !f2.get()) {
    // Files cannot match if one or both couldn't be opened.
    return false;
  }

  do {
    const std::size_t r1 = std::fread(buf1, 1, kBlockSize, f1.get());
    const std::size_t r2 = std::fread(buf2, 1, kBlockSize, f2.get());

    // TODO(fgalligan): Add output of which byte differs.
    if (r1 != r2 || std::memcmp(buf1, buf2, r1)) {
      return 0;  // Files are not equal
    }
  } while (!std::feof(f1.get()) && !std::feof(f2.get()));

  return std::feof(f1.get()) && std::feof(f2.get());
}

bool HasCuePoints(const mkvparser::Segment* segment,
                  std::int64_t* cues_offset) {
  if (!segment || !cues_offset) {
    return false;
  }
  using mkvparser::SeekHead;
  const SeekHead* const seek_head = segment->GetSeekHead();
  if (!seek_head) {
    return false;
  }

  std::int64_t offset = 0;
  for (int i = 0; i < seek_head->GetCount(); ++i) {
    const SeekHead::Entry* const entry = seek_head->GetEntry(i);
    if (entry->id == libwebm::kMkvCues) {
      offset = entry->pos;
    }
  }

  if (offset <= 0) {
    // No Cues found.
    return false;
  }

  *cues_offset = offset;
  return true;
}

bool ValidateCues(mkvparser::Segment* segment, mkvparser::IMkvReader* reader) {
  if (!segment) {
    return false;
  }

  std::int64_t cues_offset = 0;
  if (!HasCuePoints(segment, &cues_offset)) {
    // No cues to validate, everything is OK.
    return true;
  }

  // Parse Cues.
  long long cues_pos = 0;  // NOLINT
  long cues_len = 0;  // NOLINT
  if (segment->ParseCues(cues_offset, cues_pos, cues_len)) {
    return false;
  }

  // Get a pointer to the video track if it exists. Otherwise, we assume
  // that Cues are based on the first track (which is true for all our test
  // files).
  const mkvparser::Tracks* const tracks = segment->GetTracks();
  const mkvparser::Track* cues_track = tracks->GetTrackByIndex(0);
  for (int i = 1; i < static_cast<int>(tracks->GetTracksCount()); ++i) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i);
    if (track->GetType() == mkvparser::Track::kVideo) {
      cues_track = track;
      break;
    }
  }

  // Iterate through Cues and verify if they are pointing to the correct
  // Cluster position.
  const mkvparser::Cues* const cues = segment->GetCues();
  const mkvparser::CuePoint* cue_point = NULL;
  while (cues->LoadCuePoint()) {
    if (!cue_point) {
      cue_point = cues->GetFirst();
    } else {
      cue_point = cues->GetNext(cue_point);
    }
    const mkvparser::CuePoint::TrackPosition* const track_position =
        cue_point->Find(cues_track);
    const long long cluster_pos = track_position->m_pos +  // NOLINT
                                  segment->m_start;

    // If a cluster does not begin at |cluster_pos|, then the file is
    // incorrect.
    long length;  // NOLINT
    const std::int64_t id = mkvparser::ReadID(reader, cluster_pos, length);
    if (id != libwebm::kMkvCluster) {
      return false;
    }
  }
  return true;
}

MkvParser::~MkvParser() {
  delete segment;
  delete reader;
}

bool ParseMkvFileReleaseParser(const std::string& webm_file,
                               MkvParser* parser_out) {
  parser_out->reader = new (std::nothrow) mkvparser::MkvReader;
  mkvparser::MkvReader& reader = *parser_out->reader;
  if (!parser_out->reader || reader.Open(webm_file.c_str()) < 0) {
    return false;
  }

  long long pos = 0;  // NOLINT
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(&reader, pos)) {
    return false;
  }

  using mkvparser::Segment;
  Segment* segment_ptr = nullptr;
  if (Segment::CreateInstance(&reader, pos, segment_ptr)) {
    return false;
  }

  std::unique_ptr<Segment> segment(segment_ptr);
  long result;
  if ((result = segment->Load()) < 0) {
    return false;
  }

  const mkvparser::Cluster* cluster = segment->GetFirst();
  if (!cluster || cluster->EOS()) {
    return false;
  }

  while (cluster && cluster->EOS() == false) {
    if (cluster->GetTimeCode() < 0) {
      return false;
    }

    const mkvparser::BlockEntry* block = nullptr;
    if (cluster->GetFirst(block) < 0) {
      return false;
    }

    while (block != NULL && block->EOS() == false) {
      if (cluster->GetNext(block, block) < 0) {
        return false;
      }
    }

    cluster = segment->GetNext(cluster);
  }

  parser_out->segment = segment.release();
  return true;
}

bool ParseMkvFile(const std::string& webm_file) {
  MkvParser parser;
  const bool result = ParseMkvFileReleaseParser(webm_file, &parser);
  delete parser.segment;
  delete parser.reader;
  return result;
}

}  // namespace test
