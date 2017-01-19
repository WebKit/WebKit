/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/include/i420_buffer_pool.h"

#include "webrtc/base/checks.h"

namespace webrtc {

I420BufferPool::I420BufferPool(bool zero_initialize,
                               size_t max_number_of_buffers)
    : zero_initialize_(zero_initialize),
      max_number_of_buffers_(max_number_of_buffers) {}

void I420BufferPool::Release() {
  buffers_.clear();
}

rtc::scoped_refptr<I420Buffer> I420BufferPool::CreateBuffer(int width,
                                                            int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  // Release buffers with wrong resolution.
  for (auto it = buffers_.begin(); it != buffers_.end();) {
    if ((*it)->width() != width || (*it)->height() != height)
      it = buffers_.erase(it);
    else
      ++it;
  }
  // Look for a free buffer.
  for (const rtc::scoped_refptr<PooledI420Buffer>& buffer : buffers_) {
    // If the buffer is in use, the ref count will be >= 2, one from the list we
    // are looping over and one from the application. If the ref count is 1,
    // then the list we are looping over holds the only reference and it's safe
    // to reuse.
    if (buffer->HasOneRef())
      return buffer;
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;
  // Allocate new buffer.
  rtc::scoped_refptr<PooledI420Buffer> buffer =
      new PooledI420Buffer(width, height);
  if (zero_initialize_)
    buffer->InitializeData();
  buffers_.push_back(buffer);
  return buffer;
}

}  // namespace webrtc
