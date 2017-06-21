/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_
#define WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_

#include "webrtc/call/call.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_network_pipe.h"

#include <map>

namespace webrtc {

namespace test {

class LayerFilteringTransport : public test::DirectTransport {
 public:
  LayerFilteringTransport(const FakeNetworkPipe::Config& config,
                          Call* send_call,
                          uint8_t vp8_video_payload_type,
                          uint8_t vp9_video_payload_type,
                          int selected_tl,
                          int selected_sl,
                          const std::map<uint8_t, MediaType>& payload_type_map);
  bool DiscardedLastPacket() const;
  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;

 private:
  // Used to distinguish between VP8 and VP9.
  const uint8_t vp8_video_payload_type_;
  const uint8_t vp9_video_payload_type_;
  // Discard or invalidate all temporal/spatial layers with id greater than the
  // selected one. -1 to disable filtering.
  const int selected_tl_;
  const int selected_sl_;
  bool discarded_last_packet_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_LAYER_FILTERING_TRANSPORT_H_
