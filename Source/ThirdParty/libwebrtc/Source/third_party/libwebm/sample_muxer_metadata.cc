// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This sample application demonstrates how to use the matroska parser
// library, which allows clients to handle a matroska format file.

#include "sample_muxer_metadata.h"

#include <cstdio>
#include <string>

#include "mkvmuxer/mkvmuxer.h"
#include "webvtt/vttreader.h"

SampleMuxerMetadata::SampleMuxerMetadata() : segment_(NULL) {}

bool SampleMuxerMetadata::Init(mkvmuxer::Segment* segment) {
  if (segment == NULL || segment_ != NULL)
    return false;

  segment_ = segment;
  return true;
}

bool SampleMuxerMetadata::Load(const char* file, Kind kind) {
  if (kind == kChapters)
    return LoadChapters(file);

  uint64_t track_num;

  if (!AddTrack(kind, &track_num)) {
    printf("Unable to add track for WebVTT file \"%s\"\n", file);
    return false;
  }

  return Parse(file, kind, track_num);
}

bool SampleMuxerMetadata::AddChapters() {
  typedef cue_list_t::const_iterator iter_t;
  iter_t i = chapter_cues_.begin();
  const iter_t j = chapter_cues_.end();

  while (i != j) {
    const cue_t& chapter = *i++;

    if (!AddChapter(chapter))
      return false;
  }

  return true;
}

bool SampleMuxerMetadata::Write(int64_t time_ns) {
  typedef cues_set_t::iterator iter_t;

  iter_t i = cues_set_.begin();
  const iter_t j = cues_set_.end();

  while (i != j) {
    const cues_set_t::value_type& v = *i;

    if (time_ns >= 0 && v > time_ns)
      return true;  // nothing else to do just yet

    if (!v.Write(segment_)) {
      printf("\nCould not add metadata.\n");
      return false;  // error
    }

    cues_set_.erase(i++);
  }

  return true;
}

bool SampleMuxerMetadata::LoadChapters(const char* file) {
  if (!chapter_cues_.empty()) {
    printf("Support for more than one chapters file is not yet implemented\n");
    return false;
  }

  cue_list_t cues;

  if (!ParseChapters(file, &cues))
    return false;

  // TODO(matthewjheaney): support more than one chapters file
  chapter_cues_.swap(cues);

  return true;
}

bool SampleMuxerMetadata::ParseChapters(const char* file,
                                        cue_list_t* cues_ptr) {
  cue_list_t& cues = *cues_ptr;
  cues.clear();

  libwebvtt::VttReader r;
  int e = r.Open(file);

  if (e) {
    printf("Unable to open WebVTT file: \"%s\"\n", file);
    return false;
  }

  libwebvtt::Parser p(&r);
  e = p.Init();

  if (e < 0) {  // error
    printf("Error parsing WebVTT file: \"%s\"\n", file);
    return false;
  }

  libwebvtt::Time t;
  t.hours = -1;

  for (;;) {
    cue_t c;
    e = p.Parse(&c);

    if (e < 0) {  // error
      printf("Error parsing WebVTT file: \"%s\"\n", file);
      return false;
    }

    if (e > 0)  // EOF
      return true;

    if (c.start_time < t) {
      printf("bad WebVTT cue timestamp (out-of-order)\n");
      return false;
    }

    if (c.stop_time < c.start_time) {
      printf("bad WebVTT cue timestamp (stop < start)\n");
      return false;
    }

    t = c.start_time;
    cues.push_back(c);
  }
}

bool SampleMuxerMetadata::AddChapter(const cue_t& cue) {
  // TODO(matthewjheaney): support language and country

  mkvmuxer::Chapter* const chapter = segment_->AddChapter();

  if (chapter == NULL) {
    printf("Unable to add chapter\n");
    return false;
  }

  if (cue.identifier.empty()) {
    chapter->set_id(NULL);
  } else {
    const char* const id = cue.identifier.c_str();
    if (!chapter->set_id(id)) {
      printf("Unable to set chapter id\n");
      return false;
    }
  }

  typedef libwebvtt::presentation_t time_ms_t;
  const time_ms_t start_time_ms = cue.start_time.presentation();
  const time_ms_t stop_time_ms = cue.stop_time.presentation();

  enum { kNsPerMs = 1000000 };
  const uint64_t start_time_ns = start_time_ms * kNsPerMs;
  const uint64_t stop_time_ns = stop_time_ms * kNsPerMs;

  chapter->set_time(*segment_, start_time_ns, stop_time_ns);

  typedef libwebvtt::Cue::payload_t::const_iterator iter_t;
  iter_t i = cue.payload.begin();
  const iter_t j = cue.payload.end();

  std::string title;

  for (;;) {
    title += *i++;

    if (i == j)
      break;

    enum { kLF = '\x0A' };
    title += kLF;
  }

  if (!chapter->add_string(title.c_str(), NULL, NULL)) {
    printf("Unable to set chapter title\n");
    return false;
  }

  return true;
}

bool SampleMuxerMetadata::AddTrack(Kind kind, uint64_t* track_num) {
  *track_num = 0;

  // Track number value 0 means "let muxer choose track number"
  mkvmuxer::Track* const track = segment_->AddTrack(0);

  if (track == NULL)  // error
    return false;

  // Return the track number value chosen by the muxer
  *track_num = track->number();

  int type;
  const char* codec_id;

  switch (kind) {
    case kSubtitles:
      type = 0x11;
      codec_id = "D_WEBVTT/SUBTITLES";
      break;

    case kCaptions:
      type = 0x11;
      codec_id = "D_WEBVTT/CAPTIONS";
      break;

    case kDescriptions:
      type = 0x21;
      codec_id = "D_WEBVTT/DESCRIPTIONS";
      break;

    case kMetadata:
      type = 0x21;
      codec_id = "D_WEBVTT/METADATA";
      break;

    default:
      return false;
  }

  track->set_type(type);
  track->set_codec_id(codec_id);

  // TODO(matthewjheaney): set name and language

  return true;
}

bool SampleMuxerMetadata::Parse(const char* file, Kind /* kind */,
                                uint64_t track_num) {
  libwebvtt::VttReader r;
  int e = r.Open(file);

  if (e) {
    printf("Unable to open WebVTT file: \"%s\"\n", file);
    return false;
  }

  libwebvtt::Parser p(&r);

  e = p.Init();

  if (e < 0) {  // error
    printf("Error parsing WebVTT file: \"%s\"\n", file);
    return false;
  }

  SortableCue cue;
  cue.track_num = track_num;

  libwebvtt::Time t;
  t.hours = -1;

  for (;;) {
    cue_t& c = cue.cue;
    e = p.Parse(&c);

    if (e < 0) {  // error
      printf("Error parsing WebVTT file: \"%s\"\n", file);
      return false;
    }

    if (e > 0)  // EOF
      return true;

    if (c.start_time >= t) {
      t = c.start_time;
    } else {
      printf("bad WebVTT cue timestamp (out-of-order)\n");
      return false;
    }

    if (c.stop_time < c.start_time) {
      printf("bad WebVTT cue timestamp (stop < start)\n");
      return false;
    }

    cues_set_.insert(cue);
  }
}

void SampleMuxerMetadata::MakeFrame(const cue_t& c, std::string* pf) {
  pf->clear();
  WriteCueIdentifier(c.identifier, pf);
  WriteCueSettings(c.settings, pf);
  WriteCuePayload(c.payload, pf);
}

void SampleMuxerMetadata::WriteCueIdentifier(const std::string& identifier,
                                             std::string* pf) {
  pf->append(identifier);
  pf->push_back('\x0A');  // LF
}

void SampleMuxerMetadata::WriteCueSettings(const cue_t::settings_t& settings,
                                           std::string* pf) {
  if (settings.empty()) {
    pf->push_back('\x0A');  // LF
    return;
  }

  typedef cue_t::settings_t::const_iterator iter_t;

  iter_t i = settings.begin();
  const iter_t j = settings.end();

  for (;;) {
    const libwebvtt::Setting& setting = *i++;

    pf->append(setting.name);
    pf->push_back(':');
    pf->append(setting.value);

    if (i == j)
      break;

    pf->push_back(' ');  // separate settings with whitespace
  }

  pf->push_back('\x0A');  // LF
}

void SampleMuxerMetadata::WriteCuePayload(const cue_t::payload_t& payload,
                                          std::string* pf) {
  typedef cue_t::payload_t::const_iterator iter_t;

  iter_t i = payload.begin();
  const iter_t j = payload.end();

  while (i != j) {
    const std::string& line = *i++;
    pf->append(line);
    pf->push_back('\x0A');  // LF
  }
}

bool SampleMuxerMetadata::SortableCue::Write(mkvmuxer::Segment* segment) const {
  // Cue start time expressed in milliseconds
  const int64_t start_ms = cue.start_time.presentation();

  // Cue start time expressed in nanoseconds (MKV time)
  const int64_t start_ns = start_ms * 1000000;

  // Cue stop time expressed in milliseconds
  const int64_t stop_ms = cue.stop_time.presentation();

  // Cue stop time expressed in nanonseconds
  const int64_t stop_ns = stop_ms * 1000000;

  // Metadata blocks always specify the block duration.
  const int64_t duration_ns = stop_ns - start_ns;

  std::string frame;
  MakeFrame(cue, &frame);

  typedef const uint8_t* data_t;
  const data_t buf = reinterpret_cast<data_t>(frame.data());
  const uint64_t len = frame.length();

  mkvmuxer::Frame muxer_frame;
  if (!muxer_frame.Init(buf, len))
    return 0;
  muxer_frame.set_track_number(track_num);
  muxer_frame.set_timestamp(start_ns);
  muxer_frame.set_duration(duration_ns);
  muxer_frame.set_is_key(true);  // All metadata frames are keyframes.
  return segment->AddGenericFrame(&muxer_frame);
}