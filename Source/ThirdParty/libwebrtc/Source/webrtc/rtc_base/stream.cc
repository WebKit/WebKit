/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/stream.h"

#include <string.h>

#include <cstdint>

#include "api/array_view.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// StreamInterface
///////////////////////////////////////////////////////////////////////////////

StreamResult StreamInterface::WriteAll(ArrayView<const uint8_t> data,
                                       size_t& written,
                                       int& error) {
  StreamResult result = SR_SUCCESS;
  size_t total_written = 0, current_written;
  while (total_written < data.size()) {
    rtc::ArrayView<const uint8_t> this_slice =
        data.subview(total_written, data.size() - total_written);
    result = Write(this_slice, current_written, error);
    if (result != SR_SUCCESS)
      break;
    total_written += current_written;
  }
  written = total_written;
  return result;
}

bool StreamInterface::Flush() {
  return false;
}

StreamInterface::StreamInterface() {}

}  // namespace rtc
