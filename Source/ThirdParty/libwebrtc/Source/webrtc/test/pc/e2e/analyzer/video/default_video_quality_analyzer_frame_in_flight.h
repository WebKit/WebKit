/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAME_IN_FLIGHT_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAME_IN_FLIGHT_H_

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"

namespace webrtc {

struct ReceiverFrameStats {
  // Time when last packet of a frame was received.
  Timestamp received_time = Timestamp::MinusInfinity();
  Timestamp decode_start_time = Timestamp::MinusInfinity();
  Timestamp decode_end_time = Timestamp::MinusInfinity();
  Timestamp rendered_time = Timestamp::MinusInfinity();
  Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

  // Type and encoded size of received frame.
  VideoFrameType frame_type = VideoFrameType::kEmptyFrame;
  DataSize encoded_image_size = DataSize::Bytes(0);

  absl::optional<int> rendered_frame_width = absl::nullopt;
  absl::optional<int> rendered_frame_height = absl::nullopt;

  // Can be not set if frame was dropped in the network.
  absl::optional<StreamCodecInfo> used_decoder = absl::nullopt;

  bool dropped = false;
};

// Represents a frame which was sent by sender and is currently on the way to
// multiple receivers. Some receivers may receive this frame and some don't.
//
// Contains all statistic associated with the frame and gathered in multiple
// points of the video pipeline.
//
// Internally may store the copy of the source frame which was sent. In such
// case this frame is "alive".
class FrameInFlight {
 public:
  FrameInFlight(size_t stream,
                VideoFrame frame,
                Timestamp captured_time,
                std::set<size_t> expected_receivers);

  size_t stream() const { return stream_; }
  // Returns internal copy of source `VideoFrame` or `absl::nullopt` if it was
  // removed before.
  const absl::optional<VideoFrame>& frame() const { return frame_; }
  // Removes internal copy of the source `VideoFrame` to free up extra memory.
  // Returns was frame removed or not.
  bool RemoveFrame();
  void SetFrameId(uint16_t id);

  void AddExpectedReceiver(size_t peer) { expected_receivers_.insert(peer); }

  void RemoveExpectedReceiver(size_t peer) { expected_receivers_.erase(peer); }

  std::vector<size_t> GetPeersWhichDidntReceive() const;

  // Returns if all peers which were expected to receive this frame actually
  // received it or not.
  bool HaveAllPeersReceived() const;

  void SetPreEncodeTime(webrtc::Timestamp time) { pre_encode_time_ = time; }

  void OnFrameEncoded(webrtc::Timestamp time,
                      VideoFrameType frame_type,
                      DataSize encoded_image_size,
                      uint32_t target_encode_bitrate,
                      StreamCodecInfo used_encoder);

  bool HasEncodedTime() const { return encoded_time_.IsFinite(); }

  void OnFramePreDecode(size_t peer,
                        webrtc::Timestamp received_time,
                        webrtc::Timestamp decode_start_time,
                        VideoFrameType frame_type,
                        DataSize encoded_image_size);

  bool HasReceivedTime(size_t peer) const;

  void OnFrameDecoded(size_t peer,
                      webrtc::Timestamp time,
                      StreamCodecInfo used_decoder);

  bool HasDecodeEndTime(size_t peer) const;

  void OnFrameRendered(size_t peer,
                       webrtc::Timestamp time,
                       int width,
                       int height);

  bool HasRenderedTime(size_t peer) const;

  // Crash if rendered time is not set for specified `peer`.
  webrtc::Timestamp rendered_time(size_t peer) const {
    return receiver_stats_.at(peer).rendered_time;
  }

  // Marks that frame was dropped and wasn't seen by particular `peer`.
  void MarkDropped(size_t peer) { receiver_stats_[peer].dropped = true; }
  bool IsDropped(size_t peer) const;

  void SetPrevFrameRenderedTime(size_t peer, webrtc::Timestamp time) {
    receiver_stats_[peer].prev_frame_rendered_time = time;
  }

  FrameStats GetStatsForPeer(size_t peer) const;

 private:
  const size_t stream_;
  // Set of peer's indexes who are expected to receive this frame. This is not
  // the set of peer's indexes that received the frame. For example, if peer A
  // was among expected receivers, it received frame and then left the call, A
  // will be removed from this set, but the Stats for peer A still will be
  // preserved in the FrameInFlight.
  //
  // This set is used to determine if this frame is expected to be received by
  // any peer or can be safely deleted. It is responsibility of the user of this
  // object to decide when it should be deleted.
  std::set<size_t> expected_receivers_;
  absl::optional<VideoFrame> frame_;

  // Frame events timestamp.
  Timestamp captured_time_;
  Timestamp pre_encode_time_ = Timestamp::MinusInfinity();
  Timestamp encoded_time_ = Timestamp::MinusInfinity();
  // Type and encoded size of sent frame.
  VideoFrameType frame_type_ = VideoFrameType::kEmptyFrame;
  DataSize encoded_image_size_ = DataSize::Bytes(0);
  uint32_t target_encode_bitrate_ = 0;
  // Can be not set if frame was dropped by encoder.
  absl::optional<StreamCodecInfo> used_encoder_ = absl::nullopt;
  // Map from the receiver peer's index to frame stats for that peer.
  std::map<size_t, ReceiverFrameStats> receiver_stats_;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAME_IN_FLIGHT_H_
