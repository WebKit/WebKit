/* Copyright (c) 2021, Google Inc.
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

#include <string>
#include <vector>

#include <openssl/bio.h>
#include <openssl/conf.h>

#include <gtest/gtest.h>

#include "internal.h"


TEST(ConfTest, Parse) {
  // Check that basic parsing works. (We strongly recommend that people don't
  // use the [N]CONF functions.)

  static const char kConf[] = R"(
# Comment

key=value

[section_name]
key=value2
)";

  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kConf, sizeof(kConf) - 1));
  ASSERT_TRUE(bio);
  bssl::UniquePtr<CONF> conf(NCONF_new(nullptr));
  ASSERT_TRUE(conf);
  ASSERT_TRUE(NCONF_load_bio(conf.get(), bio.get(), nullptr));
  EXPECT_TRUE(NCONF_get_section(conf.get(), "section_name"));
  EXPECT_FALSE(NCONF_get_section(conf.get(), "other_section"));
  EXPECT_STREQ(NCONF_get_string(conf.get(), nullptr, "key"), "value");
  EXPECT_STREQ(NCONF_get_string(conf.get(), "section_name", "key"), "value2");
  EXPECT_STREQ(NCONF_get_string(conf.get(), "other_section", "key"), nullptr);
}

TEST(ConfTest, ParseList) {
  const struct {
    const char *list;
    char sep;
    bool remove_whitespace;
    std::vector<std::string> expected;
  } kTests[] = {
      {"", ',', /*remove_whitespace=*/0, {""}},
      {"", ',', /*remove_whitespace=*/1, {""}},

      {" ", ',', /*remove_whitespace=*/0, {" "}},
      {" ", ',', /*remove_whitespace=*/1, {""}},

      {"hello world", ',', /*remove_whitespace=*/0, {"hello world"}},
      {"hello world", ',', /*remove_whitespace=*/1, {"hello world"}},

      {" hello world ", ',', /*remove_whitespace=*/0, {" hello world "}},
      {" hello world ", ',', /*remove_whitespace=*/1, {"hello world"}},

      {"hello,world", ',', /*remove_whitespace=*/0, {"hello", "world"}},
      {"hello,world", ',', /*remove_whitespace=*/1, {"hello", "world"}},

      {"hello,,world", ',', /*remove_whitespace=*/0, {"hello", "", "world"}},
      {"hello,,world", ',', /*remove_whitespace=*/1, {"hello", "", "world"}},

      {"\tab cd , , ef gh ",
       ',',
       /*remove_whitespace=*/0,
       {"\tab cd ", " ", " ef gh "}},
      {"\tab cd , , ef gh ",
       ',',
       /*remove_whitespace=*/1,
       {"ab cd", "", "ef gh"}},
  };
  for (const auto& t : kTests) {
    SCOPED_TRACE(t.list);
    SCOPED_TRACE(t.sep);
    SCOPED_TRACE(t.remove_whitespace);

    std::vector<std::string> result;
    auto append_to_vector = [](const char *elem, size_t len, void *arg) -> int {
      auto *vec = static_cast<std::vector<std::string> *>(arg);
      vec->push_back(std::string(elem, len));
      return 1;
    };
    ASSERT_TRUE(CONF_parse_list(t.list, t.sep, t.remove_whitespace,
                                append_to_vector, &result));
    EXPECT_EQ(result, t.expected);
  }
}
