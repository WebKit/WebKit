/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstddef>

#include "net/dcsctp/fuzzers/dcsctp_fuzzers.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  dcsctp::dcsctp_fuzzers::FuzzerCallbacks cb;
  dcsctp::DcSctpOptions options;
  options.disable_checksum_verification = true;
  dcsctp::DcSctpSocket socket("A", cb, nullptr, options);

  dcsctp::dcsctp_fuzzers::FuzzSocket(socket, cb, fuzz_data.ReadRemaining());
}
}  // namespace webrtc
