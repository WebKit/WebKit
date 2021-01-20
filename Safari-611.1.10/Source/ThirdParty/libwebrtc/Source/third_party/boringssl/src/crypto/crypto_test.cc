/* Copyright (c) 2020, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>
#include <string.h>

#include <string>

#include <openssl/base.h>
#include <openssl/crypto.h>

#include <gtest/gtest.h>

// Test that OPENSSL_VERSION_NUMBER and OPENSSL_VERSION_TEXT are consistent.
// Node.js parses the version out of OPENSSL_VERSION_TEXT instead of using
// OPENSSL_VERSION_NUMBER.
TEST(CryptoTest, Version) {
  char expected[512];
  snprintf(expected, sizeof(expected), "OpenSSL %d.%d.%d ",
           OPENSSL_VERSION_NUMBER >> 28, (OPENSSL_VERSION_NUMBER >> 20) & 0xff,
           (OPENSSL_VERSION_NUMBER >> 12) & 0xff);
  EXPECT_EQ(expected,
            std::string(OPENSSL_VERSION_TEXT).substr(0, strlen(expected)));
}
