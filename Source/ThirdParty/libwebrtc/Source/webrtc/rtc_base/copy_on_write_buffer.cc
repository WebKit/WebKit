/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/copy_on_write_buffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

CopyOnWriteBuffer::RawBuffer::RawBuffer(size_t size)
    : size_(size), data_(std::make_unique_for_overwrite<uint8_t[]>(size)) {}

scoped_refptr<CopyOnWriteBuffer::RefCountedBuffer>
CopyOnWriteBuffer::CreateBuffer(size_t capacity) {
  if (capacity == 0) {
    return nullptr;
  }
  return make_ref_counted<RefCountedBuffer>(capacity);
}

CopyOnWriteBuffer::CopyOnWriteBuffer() : offset_(0), size_(0) {
  RTC_DCHECK(IsConsistent());
}

CopyOnWriteBuffer::CopyOnWriteBuffer(const CopyOnWriteBuffer& buf)
    : buffer_(buf.buffer_), offset_(buf.offset_), size_(buf.size_) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(CopyOnWriteBuffer&& buf) noexcept
    : buffer_(std::move(buf.buffer_)), offset_(buf.offset_), size_(buf.size_) {
  buf.offset_ = 0;
  buf.size_ = 0;
  RTC_DCHECK(IsConsistent());
}

CopyOnWriteBuffer::CopyOnWriteBuffer(absl::string_view s)
    : CopyOnWriteBuffer(s.data(), s.length()) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size)
    : buffer_(CreateBuffer(/*capacity=*/size)), offset_(0), size_(size) {
  // note - the RefCountedBuffer will be created uninitialized.
  RTC_DCHECK(IsConsistent());
}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size, size_t capacity)
    : buffer_(CreateBuffer(std::max(size, capacity))), offset_(0), size_(size) {
  RTC_DCHECK(IsConsistent());
}

CopyOnWriteBuffer::~CopyOnWriteBuffer() = default;

bool CopyOnWriteBuffer::operator==(const CopyOnWriteBuffer& buf) const {
  // Must either be the same view of the same buffer or have the same contents.
  RTC_DCHECK(IsConsistent());
  RTC_DCHECK(buf.IsConsistent());
  return size_ == buf.size_ &&
         (cdata() == buf.cdata() || memcmp(cdata(), buf.cdata(), size_) == 0);
}

void CopyOnWriteBuffer::SetSize(size_t size) {
  RTC_DCHECK(IsConsistent());
  if (size <= size_) {
    size_ = size;
    return;
  }

  UnshareAndEnsureCapacity(std::max(capacity(), size));
  size_ = size;
  RTC_DCHECK(IsConsistent());
}

void CopyOnWriteBuffer::EnsureCapacity(size_t new_capacity) {
  RTC_DCHECK(IsConsistent());
  if (new_capacity <= capacity()) {
    return;
  }

  UnshareAndEnsureCapacity(new_capacity);
  RTC_DCHECK(IsConsistent());
}

void CopyOnWriteBuffer::Clear() {
  if (!buffer_)
    return;

  if (!buffer_->HasOneRef()) {
    buffer_ = CreateBuffer(capacity());
  }
  offset_ = 0;
  size_ = 0;
  RTC_DCHECK(IsConsistent());
}

void CopyOnWriteBuffer::Set(std::span<const uint8_t> data) {
  RTC_DCHECK(IsConsistent());
  if (data.empty()) {
    offset_ = 0;
    size_ = 0;
    return;
  }

  if (buffer_ == nullptr || !buffer_->HasOneRef() ||
      buffer_->capacity() < data.size()) {
    buffer_ = CreateBuffer(std::max(data.size(), capacity()));
  }
  absl::c_copy(data, buffer_->data().begin());
  offset_ = 0;
  size_ = data.size();

  RTC_DCHECK(IsConsistent());
}

void CopyOnWriteBuffer::Append(std::span<const uint8_t> data) {
  RTC_DCHECK(IsConsistent());
  if (data.empty()) {
    return;
  }

  UnshareAndEnsureCapacity(std::max(capacity(), size_ + data.size()));
  absl::c_copy(data, buffer_->data().subspan(offset_ + size_).begin());
  size_ += data.size();

  RTC_DCHECK(IsConsistent());
}

void CopyOnWriteBuffer::UnshareAndEnsureCapacity(size_t new_capacity) {
  if (buffer_ != nullptr && buffer_->HasOneRef() &&
      new_capacity <= capacity()) {
    return;
  }

  scoped_refptr<RefCountedBuffer> b = CreateBuffer(new_capacity);
  absl::c_copy(AsConstSpan(), b->data().begin());
  offset_ = 0;
  buffer_ = std::move(b);
  RTC_DCHECK(IsConsistent());
}

}  // namespace webrtc
