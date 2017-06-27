/* Copyright (c) 2014, Google Inc.
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

#include <gtest/gtest.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/mem.h>


TEST(ErrTest, Overflow) {
  for (unsigned i = 0; i < ERR_NUM_ERRORS*2; i++) {
    ERR_put_error(1, 0 /* unused */, i+1, "test", 1);
  }

  for (unsigned i = 0; i < ERR_NUM_ERRORS - 1; i++) {
    SCOPED_TRACE(i);
    uint32_t err = ERR_get_error();
    /* Errors are returned in order they were pushed, with the least recent ones
     * removed, up to |ERR_NUM_ERRORS - 1| errors. So the errors returned are
     * |ERR_NUM_ERRORS + 2| through |ERR_NUM_ERRORS * 2|, inclusive. */
    EXPECT_NE(0u, err);
    EXPECT_EQ(static_cast<int>(i + ERR_NUM_ERRORS + 2), ERR_GET_REASON(err));
  }

  EXPECT_EQ(0u, ERR_get_error());
}

TEST(ErrTest, PutError) {
  ASSERT_EQ(0u, ERR_get_error())
      << "ERR_get_error returned value before an error was added.";

  ERR_put_error(1, 0 /* unused */, 2, "test", 4);
  ERR_add_error_data(1, "testing");

  int peeked_line, line, peeked_flags, flags;
  const char *peeked_file, *file, *peeked_data, *data;
  uint32_t peeked_packed_error =
      ERR_peek_error_line_data(&peeked_file, &peeked_line, &peeked_data,
                               &peeked_flags);
  uint32_t packed_error = ERR_get_error_line_data(&file, &line, &data, &flags);

  EXPECT_EQ(peeked_packed_error, packed_error);
  EXPECT_EQ(peeked_file, file);
  EXPECT_EQ(peeked_data, data);
  EXPECT_EQ(peeked_flags, flags);

  EXPECT_STREQ("test", file);
  EXPECT_EQ(4, line);
  EXPECT_TRUE(flags & ERR_FLAG_STRING);
  EXPECT_EQ(1, ERR_GET_LIB(packed_error));
  EXPECT_EQ(2, ERR_GET_REASON(packed_error));
  EXPECT_STREQ("testing", data);
}

TEST(ErrTest, ClearError) {
  ASSERT_EQ(0u, ERR_get_error())
      << "ERR_get_error returned value before an error was added.";

  ERR_put_error(1, 0 /* unused */, 2, "test", 4);
  ERR_clear_error();

  // The error queue should be cleared.
  EXPECT_EQ(0u, ERR_get_error());
}

TEST(ErrTest, Print) {
  ERR_put_error(1, 0 /* unused */, 2, "test", 4);
  ERR_add_error_data(1, "testing");
  uint32_t packed_error = ERR_get_error();

  char buf[256];
  for (size_t i = 0; i <= sizeof(buf); i++) {
    ERR_error_string_n(packed_error, buf, i);
  }
}

TEST(ErrTest, Release) {
  ERR_put_error(1, 0 /* unused */, 2, "test", 4);
  ERR_remove_thread_state(NULL);

  // The error queue should be cleared.
  EXPECT_EQ(0u, ERR_get_error());
}

static bool HasSuffix(const char *str, const char *suffix) {
  size_t suffix_len = strlen(suffix);
  size_t str_len = strlen(str);
  if (str_len < suffix_len) {
    return false;
  }
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

TEST(ErrTest, PutMacro) {
  int expected_line = __LINE__ + 1;
  OPENSSL_PUT_ERROR(USER, ERR_R_INTERNAL_ERROR);

  int line;
  const char *file;
  uint32_t error = ERR_get_error_line(&file, &line);

  EXPECT_PRED2(HasSuffix, file, "err_test.cc");
  EXPECT_EQ(expected_line, line);
  EXPECT_EQ(ERR_LIB_USER, ERR_GET_LIB(error));
  EXPECT_EQ(ERR_R_INTERNAL_ERROR, ERR_GET_REASON(error));
}
