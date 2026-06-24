// Copyright 2018 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_CPU_ARM_LINUX_H
#define OPENSSL_HEADER_CRYPTO_CPU_ARM_LINUX_H

#include <openssl/base.h>

#include <assert.h>

#include <string_view>

#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace armcap {

// The cpuinfo parser lives in a header file so it may be accessible from
// cross-platform fuzzers without adding code to those platforms normally.

#define CRYPTO_HWCAP_NEON (1 << 12)

// See /usr/include/asm/hwcap.h on an ARM installation for the source of
// these values.
// We add the prefix "CRYPTO_" to the definitions so as not to collide with
// some versions of glibc (>= 2.41) that expose them through <sys/auxv.h>.
#define CRYPTO_HWCAP2_AES (1 << 0)
#define CRYPTO_HWCAP2_PMULL (1 << 1)
#define CRYPTO_HWCAP2_SHA1 (1 << 2)
#define CRYPTO_HWCAP2_SHA2 (1 << 3)

// SplitStringView finds the first occurrence of `sep` in `in` and, if found,
// sets `*out_left` and `*out_right` to `in` split before and after `sep`, and
// returns true. If not found, it returns false.
inline bool SplitStringView(std::string_view *out_left,
                            std::string_view *out_right, std::string_view in,
                            char sep) {
  auto pos = in.find(sep);
  if (pos == std::string_view::npos) {
    return false;
  }
  *out_left = in.substr(0, pos);
  *out_right = in.substr(pos + 1);
  return true;
}

// GetDelimited reads a `sep`-delimited entry from `s`, writing it to `out` and
// updating `s` to point beyond it. It returns true on success and false if `s`
// is empty. If `s` has no copies of `sep` and is non-empty, it reads the entire
// string to `out`.
inline bool GetDelimited(std::string_view *s, std::string_view *out, char sep) {
  if (s->empty()) {
    return false;
  }
  if (!SplitStringView(out, s, *s, sep)) {
    // `s` had no instances of `sep`. Return the entire string.
    *out = *s;
    *s = std::string_view();
  }
  return true;
}

// TrimStringView removes leading and trailing whitespace from `s`.
inline std::string_view TrimStringView(std::string_view s) {
  size_t pos = s.find_first_not_of(" \t");
  if (pos == std::string_view::npos) {
    return {};
  }
  s = s.substr(pos);
  pos = s.find_last_not_of(" \t");
  assert(pos != std::string_view::npos);
  return s.substr(0, pos + 1);
}

// ExtractCpuinfoField extracts a /proc/cpuinfo field named `field` from `in`.
// If found, it returns the value. Otherwise, it returns the empty string.
inline std::string_view ExtractCpuinfoField(std::string_view in,
                                            std::string_view field) {
  // Process `in` one line at a time.
  std::string_view line;
  while (GetDelimited(&in, &line, '\n')) {
    std::string_view key, value;
    if (!SplitStringView(&key, &value, line, ':')) {
      continue;
    }
    if (TrimStringView(key) == field) {
      return TrimStringView(value);
    }
  }

  return {};
}

// HasListItem treats `list` as a space-separated list of items and returns
// whether `item` is contained in `list`.
inline bool HasListItem(std::string_view list, std::string_view item) {
  std::string_view feature;
  while (GetDelimited(&list, &feature, ' ')) {
    if (feature == item) {
      return true;
    }
  }
  return false;
}

// GetHWCAP2FromCpuinfo returns an equivalent ARM `AT_HWCAP2` value from
// `cpuinfo`.
inline unsigned long GetHWCAP2FromCpuinfo(std::string_view cpuinfo) {
  std::string_view features = ExtractCpuinfoField(cpuinfo, "Features");
  unsigned long ret = 0;
  if (HasListItem(features, "aes")) {
    ret |= CRYPTO_HWCAP2_AES;
  }
  if (HasListItem(features, "pmull")) {
    ret |= CRYPTO_HWCAP2_PMULL;
  }
  if (HasListItem(features, "sha1")) {
    ret |= CRYPTO_HWCAP2_SHA1;
  }
  if (HasListItem(features, "sha2")) {
    ret |= CRYPTO_HWCAP2_SHA2;
  }
  return ret;
}

}  // namespace armcap
BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_CPU_ARM_LINUX_H
