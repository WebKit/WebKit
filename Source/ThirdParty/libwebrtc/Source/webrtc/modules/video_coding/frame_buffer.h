/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_BUFFER_H_
#define MODULES_VIDEO_CODING_FRAME_BUFFER_H_

#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/jitter_buffer_common.h"
#include "modules/video_coding/session_info.h"

namespace webrtc {

class VCMFrameBuffer : public VCMEncodedFrame {
 public:
  VCMFrameBuffer();
  virtual ~VCMFrameBuffer();

  virtual void Reset();

  VCMFrameBufferEnum InsertPacket(const VCMPacket& packet,
                                  int64_t timeInMs,
                                  VCMDecodeErrorMode decode_error_mode,
                                  const FrameData& frame_data);

  // State
  // Get current state of frame
  VCMFrameBufferStateEnum GetState() const;
  void PrepareForDecode(bool continuous);

  bool IsSessionComplete() const;
  bool HaveFirstPacket() const;
  int NumPackets() const;

  // Sequence numbers
  // Get lowest packet sequence number in frame
  int32_t GetLowSeqNum() const;
  // Get highest packet sequence number in frame
  int32_t GetHighSeqNum() const;

  int PictureId() const;
  int TemporalId() const;
  bool LayerSync() const;
  int Tl0PicId() const;

  std::vector<NaluInfo> GetNaluInfos() const;

  void SetGofInfo(const GofInfoVP9& gof_info, size_t idx);

  // Increments a counter to keep track of the number of packets of this frame
  // which were NACKed before they arrived.
  void IncrementNackCount();
  // Returns the number of packets of this frame which were NACKed before they
  // arrived.
  int16_t GetNackCount() const;

  int64_t LatestPacketTimeMs() const;

  webrtc::FrameType FrameType() const;

 private:
  void SetState(VCMFrameBufferStateEnum state);  // Set state of frame

  VCMFrameBufferStateEnum _state;  // Current state of the frame
  VCMSessionInfo _sessionInfo;
  uint16_t _nackCount;
  int64_t _latestPacketTimeMs;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_BUFFER_H_
