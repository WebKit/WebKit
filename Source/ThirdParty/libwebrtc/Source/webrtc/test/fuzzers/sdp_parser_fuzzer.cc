/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "api/jsep.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 16384) {
    return;
  }
  std::string message(fuzz_data.ReadString());
  webrtc::SdpParseError error;

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      CreateSessionDescription(SdpType::kOffer, message, &error);
}

}  // namespace webrtc
