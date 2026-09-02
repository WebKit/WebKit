// Copyright 2015 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_CRYPTO_TEST_TEST_UTIL_H
#define OPENSSL_HEADER_CRYPTO_TEST_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/span.h>

#include "../internal.h"


// hexdump writes `msg` to `fp` followed by the hex encoding of `len` bytes
// from `in`.
void hexdump(FILE *fp, const char *msg, const void *in, size_t len);

// Bytes is a wrapper over a byte slice which may be compared for equality. This
// allows it to be used in EXPECT_EQ macros.
struct Bytes {
  Bytes(const uint8_t *data_arg, size_t len_arg)
      : span_(data_arg, len_arg) {}
  Bytes(const char *data_arg, size_t len_arg)
      : span_(reinterpret_cast<const uint8_t *>(data_arg), len_arg) {}

  explicit Bytes(std::string_view str) : span_(bssl::StringAsBytes(str)) {}
  explicit Bytes(bssl::Span<const uint8_t> span) : span_(span) {}

  bssl::Span<const uint8_t> span_;
};

inline bool operator==(const Bytes &a, const Bytes &b) {
  return a.span_ == b.span_;
}

inline bool operator!=(const Bytes &a, const Bytes &b) { return !(a == b); }

// Declassified returns a declassified copy of some input.
inline std::vector<uint8_t> Declassified(bssl::Span<const uint8_t> in) {
  std::vector<uint8_t> copy(in.begin(), in.end());
  CONSTTIME_DECLASSIFY(copy.data(), copy.size());
  return copy;
}

std::ostream &operator<<(std::ostream &os, const Bytes &in);

// DecodeHex decodes `in` from hexadecimal and writes the output to `out`. It
// returns true on success and false if `in` is not a valid hexadecimal byte
// string.
bool DecodeHex(std::vector<uint8_t> *out, std::string_view in);

// EncodeHex returns `in` encoded in hexadecimal.
std::string EncodeHex(bssl::Span<const uint8_t> in);

// ErrorEquals asserts that `err` is an error with library `lib` and reason
// `reason`. Pass `std::nullopt` to either of them to not assert on it.
testing::AssertionResult ErrorEquals(uint32_t err, std::optional<int> lib,
                                     std::optional<int> reason);

// ErrorsAreAndClear asserts that the first (i.e. least recent, and thus most
// specific) errors on the error queue are as specified, and then clears the
// remainder of the queue. The first entry in `libs_and_reasons` shall be the
// error first read from `ERR_get_error`. `libs_and_reasons` is not allowed to
// be empty; instead, to just clear and assert nothing, call `ERR_clear_error`.
testing::AssertionResult ErrorsAreAndClear(
    std::initializer_list<std::pair<std::optional<int>, std::optional<int>>>
        libs_and_reasons);

// HexToBignum decodes `hex` as a hexadecimal, big-endian, unsigned integer and
// returns it as a `BIGNUM`, or nullptr on error.
bssl::UniquePtr<BIGNUM> HexToBIGNUM(const char *hex);

// BIGNUMToHex returns `bn` as a hexadecimal, big-endian, unsigned integer.
std::string BIGNUMToHex(const BIGNUM *bn);


#endif  // OPENSSL_HEADER_CRYPTO_TEST_TEST_UTIL_H
