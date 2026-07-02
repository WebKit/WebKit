/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FUZZERS_FUZZ_DATA_HELPER_H_
#define TEST_FUZZERS_FUZZ_DATA_HELPER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "absl/strings/string_view.h"

namespace webrtc {

// Helper class to take care of the fuzzer input, read from it, and keep track
// of when the end of the data has been reached.
class FuzzDataHelper {
 public:
  explicit FuzzDataHelper(std::span<const uint8_t> data) : data_(data) {}

  // Returns true if n bytes can be read.
  bool CanReadBytes(size_t n) const { return data_ix_ + n <= data_.size(); }

  // Reads and returns data of type T.
  template <typename T>
    requires(std::is_trivial_v<T>)
  T Read();

  // Reads and returns data of type T. Returns default_value if not enough
  // fuzzer input remains to read a T.
  template <typename T>
    requires(std::is_trivial_v<T>)
  T ReadOrDefaultValue(T default_value) {
    if (!CanReadBytes(sizeof(T))) {
      return default_value;
    }
    return Read<T>();
  }

  // Like ReadOrDefaultValue, but replaces the value 0 with default_value.
  template <typename T>
    requires(std::is_integral_v<T>)
  T ReadOrDefaultValueNotZero(T default_value) {
    T x = ReadOrDefaultValue(default_value);
    return x == 0 ? default_value : x;
  }

  // Returns one of the elements from the provided input array. The selection
  // is based on the fuzzer input data. If not enough fuzzer data is available,
  // the method will return the first element in the input array. The reason for
  // not flagging this as an error is to allow the method to be called from
  // class constructors, and in constructors we typically do not handle
  // errors. The code will work anyway, and the fuzzer will likely see that
  // providing more data will actually make this method return something else.
  template <typename T, size_t N>
  T SelectOneOf(const T (&select_from)[N]) {
    static_assert(N <= std::numeric_limits<uint8_t>::max(), "");
    // Read an index between 0 and select_from.size() - 1 from the fuzzer data.
    uint8_t index = ReadOrDefaultValue<uint8_t>(0) % N;
    return select_from[index];
  }

  // Same as `SelectOneOf` but move the selected item from the array.
  template <typename T, size_t N>
  T MoveOneOf(T (&select_from)[N]) {
    static_assert(N <= std::numeric_limits<uint8_t>::max(), "");
    // Read an index between 0 and select_from.size() - 1 from the fuzzer data.
    uint8_t index = ReadOrDefaultValue<uint8_t>(0) % N;
    return std::move(select_from[index]);
  }

  std::span<const uint8_t> ReadByteArray(size_t bytes) {
    if (!CanReadBytes(bytes)) {
      return {};
    }
    const size_t index_to_return = data_ix_;
    data_ix_ += bytes;
    return data_.subspan(index_to_return, bytes);
  }

  // Returns all unused fuzzing bytes. May return an empty view.
  std::span<const uint8_t> ReadRemaining() {
    std::span<const uint8_t> result = data_.subspan(data_ix_);
    data_ix_ = data_.size();
    return result;
  }

  // Returns all unused fuzzing bytes as a string.
  absl::string_view ReadString() {
    std::span<const uint8_t> raw = ReadRemaining();
    return absl::string_view(reinterpret_cast<const char*>(raw.data()),
                             raw.size());
  }

  // If sizeof(T) > BytesLeft then the remaining bytes will be used and the rest
  // of the object will be zero initialized.
  template <typename T>
    requires(std::is_trivial_v<T>)
  void CopyTo(T& object) {
    std::span<uint8_t, sizeof(T)> object_memory(
        reinterpret_cast<uint8_t*>(&object), sizeof(T));

    size_t bytes_to_copy = std::min(BytesLeft(), sizeof(T));
    std::ranges::copy(data_.subspan(data_ix_, bytes_to_copy),
                      object_memory.begin());
    std::ranges::fill(object_memory.subspan(bytes_to_copy), uint8_t{0});
    data_ix_ += bytes_to_copy;
  }

  size_t BytesRead() const { return data_ix_; }

  size_t BytesLeft() const { return data_.size() - data_ix_; }

  size_t size() const { return data_.size(); }

#if WEBRTC_WEBKIT_BUILD
    std::span<const uint8_t> span() const { return data_; }
#endif

 private:
  std::span<const uint8_t> data_;
  size_t data_ix_ = 0;
};

template <typename T>
  requires(std::is_trivial_v<T>)
T FuzzDataHelper::Read() {
  if constexpr (sizeof(T) == 1) {
    if (BytesLeft() == 0) {
      return {};
    } else {
      return static_cast<T>(data_[data_ix_++]);
    }
  }

  T value;
  CopyTo(value);
  return value;
}

template <>
inline bool FuzzDataHelper::Read<bool>() {
  // Return `true' or 'false' with 50% chance each.
  return (Read<uint8_t>() & 0b1) != 0;
}

}  // namespace webrtc

#endif  // TEST_FUZZERS_FUZZ_DATA_HELPER_H_
