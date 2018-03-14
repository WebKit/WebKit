/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep.h"

namespace webrtc {

std::string IceCandidateInterface::server_url() const {
  return "";
}

size_t SessionDescriptionInterface::RemoveCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  return 0;
}

}  // namespace webrtc
