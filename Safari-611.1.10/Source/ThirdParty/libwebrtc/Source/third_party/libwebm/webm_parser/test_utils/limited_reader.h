// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef TEST_UTILS_LIMITED_READER_H_
#define TEST_UTILS_LIMITED_READER_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "webm/reader.h"
#include "webm/status.h"

namespace webm {

// An adapter that uses an underlying reader to read data, but with added
// limitations on how much data can be read/skipped. Its primary use is for
// testing APIs that consume a reader to make sure they gracefully handle
// arbitrary reading failures.
class LimitedReader : public Reader {
 public:
  LimitedReader() = delete;
  LimitedReader(const LimitedReader&) = delete;
  LimitedReader& operator=(const LimitedReader&) = delete;

  LimitedReader(LimitedReader&&) = default;
  LimitedReader& operator=(LimitedReader&&) = default;

  explicit LimitedReader(std::unique_ptr<Reader> impl);

  // Reads data using the internal reader, but limits the number of bytes that
  // can be read based on the settings of this LimitedReader. If this reader has
  // reached its cap of maximum number of bytes allowed to be read, the chosen
  // status will be returned.
  Status Read(std::size_t num_to_read, std::uint8_t* buffer,
              std::uint64_t* num_actually_read) override;

  // Skips data using the internal reader, but limits the number of bytes that
  // can be skipped based on the settings of this LimitedReader. If this reader
  // has reached its cap of maximum number of bytes allowed to be skipped, the
  // chosen status will be returned.
  Status Skip(std::uint64_t num_to_skip,
              std::uint64_t* num_actually_skipped) override;

  std::uint64_t Position() const override;

  // Sets the status that should be returned when the reader reaches its cap of
  // maximum number of bytes that can be read/skipped and cannot read/skip any
  // more bytes. By default, this reader will return Status::kWouldBlock when
  // this maximum limit is hit.
  void set_return_status_when_blocked(Status status);

  // Sets the total number of bytes that can be read in a single call to Read.
  void set_single_read_limit(std::size_t max_num_bytes);

  // Sets the total number of bytes that can be skipped in a single call to
  // Skip.
  void set_single_skip_limit(std::uint64_t max_num_bytes);

  // Sets the total number of bytes that can be read by the reader with Read.
  // This total is considered to be cumulative for reads, but not skips.
  // Setting this to std::numeric_limits<std::size_t>::max() will result in no
  // extra limitation being imposed on reads.
  void set_total_read_limit(std::size_t max_num_bytes);

  // Sets the total number of bytes that can be skipped by the reader with Skip.
  // This total is considered to be cumulative for skips, but not reads.
  // Setting this to std::numeric_limits<std::uint64_t>::max() will result in no
  // extra limitation being imposed on skips.
  void set_total_skip_limit(std::uint64_t max_num_bytes);

  // Sets the total number of bytes that can be read/skipped by the reader.
  // This total is considered to be cumulative between reads and skips.
  // Setting this to std::numeric_limits<std::uint64_t>::max() will result in no
  // extra limitation being imposed on reads/skips.
  void set_total_read_skip_limit(std::uint64_t max_num_bytes);

 private:
  // The maximum number of bytes to let a single call to Read return.
  std::size_t single_read_limit_ = std::numeric_limits<std::size_t>::max();

  // The maximum number of bytes to let a single call to Skip return.
  std::uint64_t single_skip_limit_ = std::numeric_limits<std::uint64_t>::max();

  // The total maximum number of bytes that can be read with multiple calls to
  // Read.
  std::size_t total_read_limit_ = std::numeric_limits<std::size_t>::max();

  // The total maximum number of bytes that can be skipped with multiple calls
  // to Skip.
  std::uint64_t total_skip_limit_ = std::numeric_limits<std::uint64_t>::max();

  // The total maximum number of bytes that can be read or skipped with multiple
  // calls to Read and/or Skip.
  std::uint64_t total_read_skip_limit_ =
      std::numeric_limits<std::uint64_t>::max();

  // The status to return when the reader has reached is maximum limit for
  // Read/Skip and cannot read or skip any data.
  Status return_status_when_blocked_ = Status(Status::kWouldBlock);

  // The actual reader that does the real reading/skipping.
  std::unique_ptr<Reader> impl_;
};

}  // namespace webm

#endif  // TEST_UTILS_LIMITED_READER_H_
