// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Output buffer for WOFF2 decompression.

#include "./woff2_out.h"

namespace woff2 {

WOFF2StringOut::WOFF2StringOut(string* buf)
  : buf_(buf),
    max_size_(kDefaultMaxSize),
    offset_(0) {}

bool WOFF2StringOut::Write(const void *buf, size_t n) {
  return Write(buf, offset_, n);
}

bool WOFF2StringOut::Write(const void *buf, size_t offset, size_t n) {
  if (offset > max_size_ || n > max_size_ - offset) {
    return false;
  }
  if (offset == buf_->size()) {
    buf_->append(static_cast<const char*>(buf), n);
  } else {
    if (offset + n > buf_->size()) {
      buf_->append(offset + n - buf_->size(), 0);
    }
    buf_->replace(offset, n, static_cast<const char*>(buf), n);
  }
  offset_ = std::max(offset_, offset + n);

  return true;
}

void WOFF2StringOut::SetMaxSize(size_t max_size) {
  max_size_ = max_size;
  if (offset_ > max_size_) {
    offset_ = max_size_;
  }
}

WOFF2MemoryOut::WOFF2MemoryOut(uint8_t* buf, size_t buf_size)
  : buf_(buf),
    buf_size_(buf_size),
    offset_(0) {}

bool WOFF2MemoryOut::Write(const void *buf, size_t n) {
  return Write(buf, offset_, n);
}

bool WOFF2MemoryOut::Write(const void *buf, size_t offset, size_t n) {
  if (offset > buf_size_ || n > buf_size_ - offset) {
    return false;
  }
  std::memcpy(buf_ + offset, buf, n);
  offset_ = std::max(offset_, offset + n);

  return true;
}

} // namespace woff2
