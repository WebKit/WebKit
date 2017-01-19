/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_CALL_FLEXFEC_RECEIVE_STREAM_H_
#define WEBRTC_API_CALL_FLEXFEC_RECEIVE_STREAM_H_

#include <string>

#include "webrtc/config.h"

namespace webrtc {

// WORK IN PROGRESS!
// This class is under development and it is not yet intended for use outside
// of WebRTC.
//
// TODO(brandtr): Remove this comment when FlexFEC is ready for public use.

class FlexfecReceiveStream {
 public:
  struct Stats {
    std::string ToString(int64_t time_ms) const;

    // TODO(brandtr): Add appropriate stats here.
    int flexfec_bitrate_bps;
  };

  // TODO(brandtr): When we add multistream protection, and thus add a
  // FlexfecSendStream class, remove FlexfecConfig from config.h and add
  // the appropriate configs here and in FlexfecSendStream.
  using Config = FlexfecConfig;

  // Starts stream activity.
  // When a stream is active, it can receive and process packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive nor process packets.
  virtual void Stop() = 0;

  virtual Stats GetStats() const = 0;

 protected:
  virtual ~FlexfecReceiveStream() = default;
};

}  // namespace webrtc

#endif  // WEBRTC_API_CALL_FLEXFEC_RECEIVE_STREAM_H_
