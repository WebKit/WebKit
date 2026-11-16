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

#include "test_util.h"

#include <ostream>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "../internal.h"


void hexdump(FILE *fp, const char *msg, const void *in, size_t len) {
  const uint8_t *data = reinterpret_cast<const uint8_t*>(in);

  fputs(msg, fp);
  for (size_t i = 0; i < len; i++) {
    fprintf(fp, "%02x", data[i]);
  }
  fputs("\n", fp);
}

std::ostream &operator<<(std::ostream &os, const Bytes &in) {
  if (in.span_.empty()) {
    return os << "<empty Bytes>";
  }

  // Print a byte slice as hex.
  os << EncodeHex(in.span_);
  return os;
}

bool DecodeHex(std::vector<uint8_t> *out, std::string_view in) {
  out->clear();
  if (in.size() % 2 != 0) {
    return false;
  }
  out->reserve(in.size() / 2);
  for (size_t i = 0; i < in.size(); i += 2) {
    uint8_t hi, lo;
    if (!OPENSSL_fromxdigit(&hi, in[i]) ||
        !OPENSSL_fromxdigit(&lo, in[i + 1])) {
      return false;
    }
    out->push_back((hi << 4) | lo);
  }
  return true;
}

std::string EncodeHex(bssl::Span<const uint8_t> in) {
  static const char kHexDigits[] = "0123456789abcdef";
  std::string ret;
  ret.reserve(in.size() * 2);
  for (uint8_t b : in) {
    ret += kHexDigits[b >> 4];
    ret += kHexDigits[b & 0xf];
  }
  return ret;
}

testing::AssertionResult ErrorEquals(uint32_t err, std::optional<int> lib,
                                     std::optional<int> reason) {
  bool lib_matches = !lib.has_value() || (ERR_GET_LIB(err) == lib.value());
  bool reason_matches =
      !reason.has_value() || (ERR_GET_REASON(err) == reason.value());

  if (lib_matches && reason_matches) {
    return testing::AssertionSuccess();
  }

  char buf[128], expected[128];
  if (lib.has_value() && reason.has_value()) {
    return testing::AssertionFailure()
           << "Got \"" << ERR_error_string_n(err, buf, sizeof(buf))
           << "\", wanted \""
           << ERR_error_string_n(ERR_PACK(lib.value(), reason.value()),
                                 expected, sizeof(expected))
           << "\"";
  } else if (lib.has_value()) {
    return testing::AssertionFailure()
           << "Got \"" << ERR_error_string_n(err, buf, sizeof(buf))
           << "\", wanted something with library \""
           << ERR_lib_error_string(ERR_PACK(lib.value(), 0)) << "\"";
  } else if (reason.has_value()) {
    return testing::AssertionFailure()
           << "Got \"" << ERR_error_string_n(err, buf, sizeof(buf))
           << "\", wanted something with reason \""
           << ERR_reason_error_string(ERR_PACK(0, reason.value())) << "\"";
  } else {
    return testing::AssertionFailure()
           << "Unreachable code: the always-true assertion failed";
  }
}

testing::AssertionResult ErrorsAreAndClear(
    std::initializer_list<std::pair<std::optional<int>, std::optional<int>>>
        libs_and_reasons) {
  if (libs_and_reasons.size() == 0) {
    return testing::AssertionFailure()
           << "ErrorsAreAndClear with empty list of errors is nonsensical - "
              "just use ERR_clear_error directly!";
  }
  bool have_failures = false;
  testing::AssertionResult all_failures = testing::AssertionFailure();
  for (const auto &[lib, reason] : libs_and_reasons) {
    uint32_t err = ERR_get_error();
    testing::AssertionResult this_result = ErrorEquals(err, lib, reason);
    if (this_result) {
      continue;
    }
    all_failures << this_result.message();
    have_failures = true;
  }
  if (have_failures) {
    return all_failures;
  }
  ERR_clear_error();
  return testing::AssertionSuccess();
}

bssl::UniquePtr<BIGNUM> HexToBIGNUM(const char *hex) {
  BIGNUM *bn = nullptr;
  BN_hex2bn(&bn, hex);
  return bssl::UniquePtr<BIGNUM>(bn);
}

std::string BIGNUMToHex(const BIGNUM *bn) {
  bssl::UniquePtr<char> hex(BN_bn2hex(bn));
  if (hex == nullptr) {
    return "error";
  }
  return hex.get();
}
