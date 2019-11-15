/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/logging/log_writer.h"

namespace webrtc {

LogWriterFactoryAddPrefix::LogWriterFactoryAddPrefix(
    LogWriterFactoryInterface* base,
    std::string prefix)
    : base_factory_(base), prefix_(prefix) {}

std::unique_ptr<RtcEventLogOutput> LogWriterFactoryAddPrefix::Create(
    std::string filename) {
  return base_factory_->Create(prefix_ + filename);
}

}  // namespace webrtc
