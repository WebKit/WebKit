// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "test_utils/limited_reader.h"

namespace webm {

LimitedReader::LimitedReader(std::unique_ptr<Reader> impl)
    : impl_(std::move(impl)) {}

Status LimitedReader::Read(std::size_t num_to_read, std::uint8_t* buffer,
                           std::uint64_t* num_actually_read) {
  assert(num_to_read > 0);
  assert(buffer != nullptr);
  assert(num_actually_read != nullptr);

  *num_actually_read = 0;
  std::size_t expected = num_to_read;

  num_to_read = std::min({num_to_read, single_read_limit_, total_read_limit_});

  // Handle total_read_skip_limit_ separately since std::size_t can be
  // smaller than std::uint64_t.
  if (num_to_read > total_read_skip_limit_) {
    num_to_read = static_cast<std::size_t>(total_read_skip_limit_);
  }

  if (num_to_read == 0) {
    return return_status_when_blocked_;
  }

  Status status = impl_->Read(num_to_read, buffer, num_actually_read);
  assert(*num_actually_read <= num_to_read);

  if (status.code == Status::kOkCompleted && *num_actually_read < expected) {
    status.code = Status::kOkPartial;
  }

  if (total_read_limit_ != std::numeric_limits<std::size_t>::max()) {
    total_read_limit_ -= static_cast<std::size_t>(*num_actually_read);
  }

  if (total_read_skip_limit_ != std::numeric_limits<std::uint64_t>::max()) {
    total_read_skip_limit_ -= *num_actually_read;
  }

  return status;
}

Status LimitedReader::Skip(std::uint64_t num_to_skip,
                           std::uint64_t* num_actually_skipped) {
  assert(num_to_skip > 0);
  assert(num_actually_skipped != nullptr);

  *num_actually_skipped = 0;
  std::uint64_t expected = num_to_skip;

  num_to_skip = std::min({num_to_skip, single_skip_limit_, total_skip_limit_,
                          total_read_skip_limit_});

  if (num_to_skip == 0) {
    return return_status_when_blocked_;
  }

  Status status = impl_->Skip(num_to_skip, num_actually_skipped);
  assert(*num_actually_skipped <= num_to_skip);

  if (status.code == Status::kOkCompleted && *num_actually_skipped < expected) {
    status.code = Status::kOkPartial;
  }

  if (total_skip_limit_ != std::numeric_limits<std::uint64_t>::max()) {
    total_skip_limit_ -= *num_actually_skipped;
  }

  if (total_read_skip_limit_ != std::numeric_limits<std::uint64_t>::max()) {
    total_read_skip_limit_ -= *num_actually_skipped;
  }

  return status;
}

std::uint64_t LimitedReader::Position() const { return impl_->Position(); }

void LimitedReader::set_return_status_when_blocked(Status status) {
  return_status_when_blocked_ = status;
}

void LimitedReader::set_single_read_limit(std::size_t max_num_bytes) {
  single_read_limit_ = max_num_bytes;
}

void LimitedReader::set_single_skip_limit(std::uint64_t max_num_bytes) {
  single_skip_limit_ = max_num_bytes;
}

void LimitedReader::set_total_read_limit(std::size_t max_num_bytes) {
  total_read_limit_ = max_num_bytes;
}

void LimitedReader::set_total_skip_limit(std::uint64_t max_num_bytes) {
  total_skip_limit_ = max_num_bytes;
}

void LimitedReader::set_total_read_skip_limit(std::uint64_t max_num_bytes) {
  total_read_skip_limit_ = max_num_bytes;
}

}  // namespace webm
