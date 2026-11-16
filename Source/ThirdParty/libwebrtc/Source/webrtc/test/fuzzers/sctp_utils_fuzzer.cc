/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "api/data_channel_interface.h"
#include "pc/sctp_utils.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  CopyOnWriteBuffer payload(fuzz_data.ReadRemaining());
  std::string label;
  DataChannelInit config;
  IsOpenMessage(payload);
  ParseDataChannelOpenMessage(payload, &label, &config);
  ParseDataChannelOpenAckMessage(payload);
}

}  // namespace webrtc
