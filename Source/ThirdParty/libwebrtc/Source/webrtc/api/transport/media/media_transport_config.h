/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_CONFIG_H_
#define API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"

namespace webrtc {

// Media transport config is made available to both transport and audio / video
// layers, but access to individual interfaces should not be open without
// necessity.
struct MediaTransportConfig {
  // Default constructor for no-media transport scenarios.
  MediaTransportConfig() = default;

  // Constructor for datagram transport scenarios.
  explicit MediaTransportConfig(size_t rtp_max_packet_size);

  std::string DebugString() const;

  // If provided, limits RTP packet size (excludes ICE, IP or network overhead).
  absl::optional<size_t> rtp_max_packet_size;
};

}  // namespace webrtc

#endif  // API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_CONFIG_H_
