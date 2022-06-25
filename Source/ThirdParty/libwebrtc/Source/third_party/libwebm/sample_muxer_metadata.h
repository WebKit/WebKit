// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef SAMPLE_MUXER_METADATA_H_  // NOLINT
#define SAMPLE_MUXER_METADATA_H_

#include <stdint.h>

#include <list>
#include <set>
#include <string>

#include "webvtt/webvttparser.h"

namespace mkvmuxer {
class Chapter;
class Frame;
class Segment;
class Track;
}  // namespace mkvmuxer

class SampleMuxerMetadata {
 public:
  enum Kind { kSubtitles, kCaptions, kDescriptions, kMetadata, kChapters };

  SampleMuxerMetadata();

  // Bind this metadata object to the muxer instance.  Returns false
  // if segment equals NULL, or Init has already been called.
  bool Init(mkvmuxer::Segment* segment);

  // Parse the WebVTT file |filename| having the indicated |kind|, and
  // create a corresponding track (or chapters element) in the
  // segment.  Returns false on error.
  bool Load(const char* filename, Kind kind);

  bool AddChapters();

  // Write any WebVTT cues whose time is less or equal to |time_ns| as
  // a metadata block in its corresponding track.  If |time_ns| is
  // negative, write all remaining cues. Returns false on error.
  bool Write(int64_t time_ns);

 private:
  typedef libwebvtt::Cue cue_t;

  // Used to sort cues as they are loaded.
  struct SortableCue {
    bool operator>(int64_t time_ns) const {
      // Cue start time expressed in milliseconds
      const int64_t start_ms = cue.start_time.presentation();

      // Cue start time expressed in nanoseconds (MKV time)
      const int64_t start_ns = start_ms * 1000000;

      return (start_ns > time_ns);
    }

    bool operator<(const SortableCue& rhs) const {
      if (cue.start_time < rhs.cue.start_time)
        return true;

      if (cue.start_time > rhs.cue.start_time)
        return false;

      return (track_num < rhs.track_num);
    }

    // Write this cue as a metablock to |segment|.  Returns false on
    // error.
    bool Write(mkvmuxer::Segment* segment) const;

    uint64_t track_num;
    cue_t cue;
  };

  typedef std::multiset<SortableCue> cues_set_t;
  typedef std::list<cue_t> cue_list_t;

  // Parse the WebVTT cues in the named |file|, returning false on
  // error.  We handle chapters as a special case, because they are
  // stored in their own, dedicated level-1 element.
  bool LoadChapters(const char* file);

  // Parse the WebVTT chapters in |file| to populate |cues|.  Returns
  // false on error.
  static bool ParseChapters(const char* file, cue_list_t* cues);

  // Adds WebVTT cue |chapter| to the chapters element of the output
  // file's segment element.  Returns false on error.
  bool AddChapter(const cue_t& chapter);

  // Add a metadata track to the segment having the indicated |kind|,
  // returning the |track_num| that has been chosen for this track.
  // Returns false on error.
  bool AddTrack(Kind kind, uint64_t* track_num);

  // Parse the WebVTT |file| having the indicated |kind| and
  // |track_num|, adding each parsed cue to cues set.  Returns false
  // on error.
  bool Parse(const char* file, Kind kind, uint64_t track_num);

  // Converts a WebVTT cue to a Matroska metadata block.
  static void MakeFrame(const cue_t& cue, std::string* frame);

  // Populate the cue identifier part of the metadata block.
  static void WriteCueIdentifier(const std::string& identifier,
                                 std::string* frame);

  // Populate the cue settings part of the metadata block.
  static void WriteCueSettings(const cue_t::settings_t& settings,
                               std::string* frame);

  // Populate the payload part of the metadata block.
  static void WriteCuePayload(const cue_t::payload_t& payload,
                              std::string* frame);

  mkvmuxer::Segment* segment_;

  // Set of cues ordered by time and then by track number.
  cues_set_t cues_set_;

  // The cues that will be used to populate the Chapters level-1
  // element of the output file.
  cue_list_t chapter_cues_;

  // Disable copy ctor and copy assign.
  SampleMuxerMetadata(const SampleMuxerMetadata&);
  SampleMuxerMetadata& operator=(const SampleMuxerMetadata&);
};

#endif  // SAMPLE_MUXER_METADATA_H_  // NOLINT
