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

#include <openssl/lhash.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>


static std::unique_ptr<char[]> RandString(void) {
  unsigned len = 1 + (rand() % 3);
  std::unique_ptr<char[]> ret(new char[len + 1]);

  for (unsigned i = 0; i < len; i++) {
    ret[i] = '0' + (rand() & 7);
  }
  ret[len] = 0;

  return ret;
}

struct FreeLHASH {
  void operator()(_LHASH *lh) { lh_free(lh); }
};

static const char *Lookup(
    std::map<std::string, std::unique_ptr<char[]>> *dummy_lh, const char *key) {
  // Using operator[] implicitly inserts into the map.
  auto iter = dummy_lh->find(key);
  if (iter == dummy_lh->end()) {
    return nullptr;
  }
  return iter->second.get();
}

TEST(LHashTest, Basic) {
  std::unique_ptr<_LHASH, FreeLHASH> lh(
      lh_new((lhash_hash_func)lh_strhash, (lhash_cmp_func)strcmp));
  ASSERT_TRUE(lh);

  // lh is expected to store a canonical instance of each string. dummy_lh
  // mirrors what it stores for comparison. It also manages ownership of the
  // pointers.
  std::map<std::string, std::unique_ptr<char[]>> dummy_lh;

  for (unsigned i = 0; i < 100000; i++) {
    EXPECT_EQ(dummy_lh.size(), lh_num_items(lh.get()));

    // Check the entire contents and test |lh_doall_arg|. This takes O(N) time,
    // so only do it every few iterations.
    //
    // TODO(davidben): |lh_doall_arg| also supports modifying the hash in the
    // callback. Test this.
    if (i % 1000 == 0) {
      using ValueList = std::vector<const char *>;
      ValueList expected, actual;
      for (const auto &pair : dummy_lh) {
        expected.push_back(pair.second.get());
      }
      std::sort(expected.begin(), expected.end());

      lh_doall_arg(lh.get(),
                   [](void *ptr, void *arg) {
                     ValueList *out = reinterpret_cast<ValueList *>(arg);
                     out->push_back(reinterpret_cast<char *>(ptr));
                   },
                   &actual);
      std::sort(actual.begin(), actual.end());

      EXPECT_EQ(expected, actual);
    }

    enum Action {
      kRetrieve = 0,
      kInsert,
      kDelete,
    };

    Action action = static_cast<Action>(rand() % 3);
    switch (action) {
      case kRetrieve: {
        std::unique_ptr<char[]> key = RandString();
        void *value = lh_retrieve(lh.get(), key.get());
        EXPECT_EQ(Lookup(&dummy_lh, key.get()), value);
        break;
      }

      case kInsert: {
        std::unique_ptr<char[]> key = RandString();
        void *previous;
        ASSERT_TRUE(lh_insert(lh.get(), &previous, key.get()));
        EXPECT_EQ(Lookup(&dummy_lh, key.get()), previous);
        dummy_lh[key.get()] = std::move(key);
        break;
      }

      case kDelete: {
        std::unique_ptr<char[]> key = RandString();
        void *value = lh_delete(lh.get(), key.get());
        EXPECT_EQ(Lookup(&dummy_lh, key.get()), value);
        dummy_lh.erase(key.get());
        break;
      }
    }
  }
}
