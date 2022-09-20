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

#include "absl/strings/string_view.h"

namespace webrtc {

LogWriterFactoryAddPrefix::LogWriterFactoryAddPrefix(
    LogWriterFactoryInterface* base,
    absl::string_view prefix)
    : base_factory_(base), prefix_(prefix) {}

std::unique_ptr<RtcEventLogOutput> LogWriterFactoryAddPrefix::Create(
    absl::string_view filename) {
  return base_factory_->Create(prefix_ + std::string(filename));
}

}  // namespace webrtc
