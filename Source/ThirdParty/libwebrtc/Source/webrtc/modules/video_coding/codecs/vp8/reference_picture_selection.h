/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file defines classes for doing reference picture selection, primarily
 * with VP8.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_REFERENCE_PICTURE_SELECTION_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_REFERENCE_PICTURE_SELECTION_H_

#include "webrtc/typedefs.h"

namespace webrtc {

class ReferencePictureSelection {
 public:
  ReferencePictureSelection();
  void Init();

  // Report a received reference picture selection indication. This will
  // introduce a new established reference if the received RPSI isn't too late.
  void ReceivedRPSI(int rpsi_picture_id);

  // Report a received slice loss indication. Returns true if a refresh frame
  // must be sent to the receiver, which is accomplished by only predicting
  // from the established reference.
  // |now_ts| is the RTP timestamp corresponding to the current time. Typically
  // the capture timestamp of the frame currently being processed.
  // Returns true if it's time to encode a decoder refresh, otherwise false.
  bool ReceivedSLI(uint32_t now_ts);

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  // |picture_id| picture id of the frame to be encoded.
  // |send_refresh| should be set to true if a decoder refresh should be
  // encoded, otherwise false.
  // |now_ts| is the RTP timestamp corresponding to the current time. Typically
  // the capture timestamp of the frame currently being processed.
  // Returns the flags to be given to the libvpx encoder when encoding the next
  // frame.
  int EncodeFlags(int picture_id, bool send_refresh, uint32_t now_ts);

  // Notify the RPS that the frame with picture id |picture_id| was encoded as
  // a key frame, effectively updating all reference buffers.
  void EncodedKeyFrame(int picture_id);

  // Set the round-trip time between the sender and the receiver to |rtt|
  // milliseconds.
  void SetRtt(int64_t rtt);

 private:
  static int64_t TimestampDiff(uint32_t new_ts, uint32_t old_ts);

  const double kRttConfidence;

  bool update_golden_next_;
  bool established_golden_;
  bool received_ack_;
  int last_sent_ref_picture_id_;
  uint32_t last_sent_ref_update_time_;
  int established_ref_picture_id_;
  uint32_t last_refresh_time_;
  int64_t rtt_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_REFERENCE_PICTURE_SELECTION_H_
