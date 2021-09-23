/*
 * Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 * Copyright (C) 2020 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebMFileWriter.h"

#include <string>
#include "mkvmuxer/mkvmuxerutil.h"

namespace WebKit {

WebMFileWriter::WebMFileWriter(FILE* file, vpx_codec_enc_cfg_t* cfg)
    : m_cfg(cfg)
    , m_writer(new mkvmuxer::MkvWriter(file))
    , m_segment(new mkvmuxer::Segment()) {
  m_segment->Init(m_writer.get());
  m_segment->set_mode(mkvmuxer::Segment::kFile);
  m_segment->OutputCues(true);

  mkvmuxer::SegmentInfo* info = m_segment->GetSegmentInfo();
  std::string version = "Playwright " + std::string(vpx_codec_version_str());
  info->set_writing_app(version.c_str());

  // Add vp8 track.
  m_videoTrackId = m_segment->AddVideoTrack(
      static_cast<int>(m_cfg->g_w), static_cast<int>(m_cfg->g_h), 0);
  if (!m_videoTrackId) {
    fprintf(stderr, "Failed to add video track\n");
  }
}

WebMFileWriter::~WebMFileWriter() {}

bool WebMFileWriter::writeFrame(const vpx_codec_cx_pkt_t* pkt) {
  int64_t pts_ns = pkt->data.frame.pts * 1000000000ll * m_cfg->g_timebase.num /
                   m_cfg->g_timebase.den;
  return m_segment->AddFrame(static_cast<uint8_t*>(pkt->data.frame.buf),
                             pkt->data.frame.sz, m_videoTrackId, pts_ns,
                             pkt->data.frame.flags & VPX_FRAME_IS_KEY);
}

void WebMFileWriter::finish() {
  m_segment->Finalize();
}

} // namespace WebKit
