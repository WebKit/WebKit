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

#include "internal.h"

#include <chrono>
#include <vector>
#include <thread>

#include <gtest/gtest.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include "test/test_util.h"


BSSL_NAMESPACE_BEGIN
namespace {

#if defined(OPENSSL_THREADS)

static unsigned g_once_init_called = 0;

static void once_init() {
  g_once_init_called++;

  // Sleep briefly so one |call_once_func| instance will call |CRYPTO_once|
  // while the other is running this function.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

static CRYPTO_once_t g_test_once = CRYPTO_ONCE_INIT;

TEST(ThreadTest, Once) {
  ASSERT_EQ(0u, g_once_init_called)
      << "g_once_init_called was non-zero at start.";

  auto call_once_func = [] { CRYPTO_once(&g_test_once, once_init); };
  std::thread thread1(call_once_func), thread2(call_once_func);
  thread1.join();
  thread2.join();

  CRYPTO_once(&g_test_once, once_init);

  EXPECT_EQ(1u, g_once_init_called);
}

static CRYPTO_once_t once_init_value = CRYPTO_ONCE_INIT;
static Mutex mutex_init_value;
static ExDataClass ex_data_class_value;

template <typename T>
static void ExpectAllZeros(const T &t) {
  uint8_t zeros[sizeof(T)] = {};
  EXPECT_EQ(Bytes(zeros),
            Bytes(reinterpret_cast<const uint8_t *>(&t), sizeof(t)));
}

TEST(ThreadTest, InitZeros) {
  if (FIPS_mode()) {
    // Our FIPS tooling currently requires that these types are all zeros and so
    // can be placed in the BSS section.
    ExpectAllZeros(once_init_value);
    ExpectAllZeros(mutex_init_value);
    ExpectAllZeros(ex_data_class_value);
  }
}

static int g_test_thread_ok = 0;
static unsigned g_destructor_called_count = 0;

static void thread_local_destructor(void *arg) {
  if (arg == nullptr) {
    return;
  }

  unsigned *count = reinterpret_cast<unsigned*>(arg);
  (*count)++;
}

TEST(ThreadTest, ThreadLocal) {
  ASSERT_EQ(nullptr, CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_TEST))
      << "Thread-local data was non-NULL at start.";

  std::thread thread([] {
    if (CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_TEST) != nullptr ||
        !CRYPTO_set_thread_local(OPENSSL_THREAD_LOCAL_TEST,
                                 &g_destructor_called_count,
                                 thread_local_destructor) ||
        CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_TEST) !=
            &g_destructor_called_count) {
      return;
    }

    g_test_thread_ok = 1;
  });
  thread.join();

  EXPECT_TRUE(g_test_thread_ok) << "Thread-local data didn't work in thread.";
  EXPECT_EQ(1u, g_destructor_called_count);

  // Create a no-op thread to test that the thread destructor function works
  // even if thread-local storage wasn't used for a thread.
  thread = std::thread([] {});
  thread.join();
}

TEST(ThreadTest, RandState) {
  // In FIPS mode, rand.c maintains a linked-list of thread-local data because
  // we're required to clear it on process exit. This test exercises removing a
  // value from that list.
  uint8_t buf[1];
  RAND_bytes(buf, sizeof(buf));

  std::thread thread([] {
    uint8_t buf2[1];
    RAND_bytes(buf2, sizeof(buf2));
  });
  thread.join();
}

TEST(ThreadTest, PreSandboxInitThreads) {
  constexpr size_t kNumThreads = 10;

  // |CRYPTO_pre_sandbox_init| is safe to call across threads.
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(&CRYPTO_pre_sandbox_init);
  }
  for (auto &thread : threads) {
    thread.join();
  }
}

#endif  // OPENSSL_THREADS

}  // namespace
BSSL_NAMESPACE_END
