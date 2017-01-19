/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/reference_picture_selection.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "webrtc/typedefs.h"

namespace webrtc {

ReferencePictureSelection::ReferencePictureSelection()
    : kRttConfidence(1.33),
      update_golden_next_(true),
      established_golden_(false),
      received_ack_(false),
      last_sent_ref_picture_id_(0),
      last_sent_ref_update_time_(0),
      established_ref_picture_id_(0),
      last_refresh_time_(0),
      rtt_(0) {}

void ReferencePictureSelection::Init() {
  update_golden_next_ = true;
  established_golden_ = false;
  received_ack_ = false;
  last_sent_ref_picture_id_ = 0;
  last_sent_ref_update_time_ = 0;
  established_ref_picture_id_ = 0;
  last_refresh_time_ = 0;
  rtt_ = 0;
}

void ReferencePictureSelection::ReceivedRPSI(int rpsi_picture_id) {
  // Assume RPSI is signaled with 14 bits.
  if ((rpsi_picture_id & 0x3fff) == (last_sent_ref_picture_id_ & 0x3fff)) {
    // Remote peer has received our last reference frame, switch frame type.
    received_ack_ = true;
    established_golden_ = update_golden_next_;
    update_golden_next_ = !update_golden_next_;
    established_ref_picture_id_ = last_sent_ref_picture_id_;
  }
}

bool ReferencePictureSelection::ReceivedSLI(uint32_t now_ts) {
  bool send_refresh = false;
  // Don't send a refresh more than once per round-trip time.
  // This is to avoid too frequent refreshes, since the receiver
  // will signal an SLI for every corrupt frame.
  if (TimestampDiff(now_ts, last_refresh_time_) > rtt_) {
    send_refresh = true;
    last_refresh_time_ = now_ts;
  }
  return send_refresh;
}

int ReferencePictureSelection::EncodeFlags(int picture_id,
                                           bool send_refresh,
                                           uint32_t now_ts) {
  int flags = 0;
  // We can't refresh the decoder until we have established the key frame.
  if (send_refresh && received_ack_) {
    flags |= VP8_EFLAG_NO_REF_LAST;  // Don't reference the last frame
    if (established_golden_)
      flags |= VP8_EFLAG_NO_REF_ARF;  // Don't reference the alt-ref frame.
    else
      flags |= VP8_EFLAG_NO_REF_GF;  // Don't reference the golden frame
  }

  // Make sure we don't update the reference frames too often. We must wait long
  // enough for an RPSI to arrive after the decoder decoded the reference frame.
  // Ideally that should happen after one round-trip time.
  // Add a margin defined by |kRttConfidence|.
  int64_t update_interval = static_cast<int64_t>(kRttConfidence * rtt_);
  const int64_t kMinUpdateInterval = 90 * 10;  // Timestamp frequency
  if (update_interval < kMinUpdateInterval)
    update_interval = kMinUpdateInterval;
  // Don't send reference frame updates until we have an established reference.
  if (TimestampDiff(now_ts, last_sent_ref_update_time_) > update_interval &&
      received_ack_) {
    flags |= VP8_EFLAG_NO_REF_LAST;  // Don't reference the last frame.
    if (update_golden_next_) {
      flags |= VP8_EFLAG_FORCE_GF;    // Update the golden reference.
      flags |= VP8_EFLAG_NO_UPD_ARF;  // Don't update alt-ref.
      flags |= VP8_EFLAG_NO_REF_GF;   // Don't reference the golden frame.
    } else {
      flags |= VP8_EFLAG_FORCE_ARF;   // Update the alt-ref reference.
      flags |= VP8_EFLAG_NO_UPD_GF;   // Don't update the golden frame.
      flags |= VP8_EFLAG_NO_REF_ARF;  // Don't reference the alt-ref frame.
    }
    last_sent_ref_picture_id_ = picture_id;
    last_sent_ref_update_time_ = now_ts;
  } else {
    // No update of golden or alt-ref. We can therefore freely reference the
    // established reference frame and the last frame.
    if (established_golden_)
      flags |= VP8_EFLAG_NO_REF_ARF;  // Don't reference the alt-ref frame.
    else
      flags |= VP8_EFLAG_NO_REF_GF;  // Don't reference the golden frame.
    flags |= VP8_EFLAG_NO_UPD_GF;    // Don't update the golden frame.
    flags |= VP8_EFLAG_NO_UPD_ARF;   // Don't update the alt-ref frame.
  }
  return flags;
}

void ReferencePictureSelection::EncodedKeyFrame(int picture_id) {
  last_sent_ref_picture_id_ = picture_id;
  received_ack_ = false;
}

void ReferencePictureSelection::SetRtt(int64_t rtt) {
  // Convert from milliseconds to timestamp frequency.
  rtt_ = 90 * rtt;
}

int64_t ReferencePictureSelection::TimestampDiff(uint32_t new_ts,
                                                 uint32_t old_ts) {
  if (old_ts > new_ts) {
    // Assuming this is a wrap, doing a compensated subtraction.
    return (new_ts + (static_cast<int64_t>(1) << 32)) - old_ts;
  }
  return new_ts - old_ts;
}

}  // namespace webrtc
