/* Copyright (c) 2024, Google Inc.
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_TEST_DATA_H
#define OPENSSL_HEADER_CRYPTO_TEST_TEST_DATA_H

#include <string>

// GetTestData returns the test data for |path|, or aborts on error. |path|
// must be a slash-separated path, relative to the BoringSSL source tree. By
// default, this is implemented by reading from the filesystem, relative to
// the BORINGSSL_TEST_DATA_ROOT environment variable, or the current working
// directory if unset.
//
// Callers with more complex needs can build with
// BORINGSSL_CUSTOM_GET_TEST_DATA and then link in an alternate implementation
// of this function.
//
// Callers running from Bazel can define BORINGSSL_USE_BAZEL_RUNFILES to use
// the Bazel runfiles library.
std::string GetTestData(const char *path);

#endif  // OPENSSL_HEADER_CRYPTO_TEST_TEST_DATA_H
