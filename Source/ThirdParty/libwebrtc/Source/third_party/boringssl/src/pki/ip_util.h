// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BSSL_PKI_IP_UTIL_H_
#define BSSL_PKI_IP_UTIL_H_

#include <openssl/base.h>

#include "input.h"

namespace bssl {

inline constexpr size_t kIPv4AddressSize = 4;
inline constexpr size_t kIPv6AddressSize = 16;

// Returns whether `mask` is a valid netmask. I.e., whether it is the length of
// an IPv4 or IPv6 address, and is some number of ones, followed by some number
// of zeros.
OPENSSL_EXPORT bool IsValidNetmask(der::Input mask);

// Returns whether `addr1` and `addr2` are equal under the netmask `mask`.
OPENSSL_EXPORT bool IPAddressMatchesWithNetmask(der::Input addr1,
                                                der::Input addr2,
                                                der::Input mask);

}  // namespace bssl

#endif  // BSSL_PKI_IP_UTIL_H_
