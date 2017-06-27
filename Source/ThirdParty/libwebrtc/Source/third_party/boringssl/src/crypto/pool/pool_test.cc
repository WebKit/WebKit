/* Copyright (c) 2016, Google Inc.
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

#include <gtest/gtest.h>

#include <openssl/pool.h>

#include "../test/test_util.h"


TEST(PoolTest, Unpooled) {
  static const uint8_t kData[4] = {1, 2, 3, 4};
  bssl::UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(kData, sizeof(kData), nullptr));
  ASSERT_TRUE(buf);

  EXPECT_EQ(Bytes(kData),
            Bytes(CRYPTO_BUFFER_data(buf.get()), CRYPTO_BUFFER_len(buf.get())));

  // Test that reference-counting works properly.
  CRYPTO_BUFFER_up_ref(buf.get());
  bssl::UniquePtr<CRYPTO_BUFFER> buf2(buf.get());
}

TEST(PoolTest, Empty) {
  bssl::UniquePtr<CRYPTO_BUFFER> buf(CRYPTO_BUFFER_new(nullptr, 0, nullptr));
  ASSERT_TRUE(buf);

  EXPECT_EQ(Bytes(""),
            Bytes(CRYPTO_BUFFER_data(buf.get()), CRYPTO_BUFFER_len(buf.get())));
}

TEST(PoolTest, Pooled) {
  bssl::UniquePtr<CRYPTO_BUFFER_POOL> pool(CRYPTO_BUFFER_POOL_new());
  ASSERT_TRUE(pool);

  static const uint8_t kData[4] = {1, 2, 3, 4};
  bssl::UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(kData, sizeof(kData), pool.get()));
  ASSERT_TRUE(buf);

  bssl::UniquePtr<CRYPTO_BUFFER> buf2(
      CRYPTO_BUFFER_new(kData, sizeof(kData), pool.get()));
  ASSERT_TRUE(buf2);

  EXPECT_EQ(buf.get(), buf2.get()) << "CRYPTO_BUFFER_POOL did not dedup data.";
}
