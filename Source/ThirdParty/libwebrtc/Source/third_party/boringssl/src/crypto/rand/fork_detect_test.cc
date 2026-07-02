// Copyright 2020 The BoringSSL Authors
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

#include <openssl/base.h>

#include "../bcm_support.h"
#include "internal.h"

// TSAN cannot cope with this test and complains that "starting new threads
// after multi-threaded fork is not supported".
#if defined(OPENSSL_FORK_DETECTION) && !defined(OPENSSL_TSAN) && \
    !defined(OPENSSL_IOS)

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <tuple>

#if defined(OPENSSL_THREADS)
#include <thread>
#include <vector>
#endif

#if defined(OPENSSL_LINUX)
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#endif

#include <gtest/gtest.h>

#include <openssl/mem.h>

#include "../internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

static pid_t WaitpidEINTR(pid_t pid, int *out_status, int options) {
  pid_t ret;
  do {
    ret = waitpid(pid, out_status, options);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

// The *InChild functions run inside a child process and must report errors via
// |stderr| and |_exit| rather than GTest.

static void CheckGenerationAtLeastInChild(const char *name,
                                   uint64_t minimum_expected) {
  uint64_t generation = CRYPTO_get_fork_generation();
  if (generation < minimum_expected) {
    fprintf(stderr, "%s generation (#1) was %" PRIu64 ", wanted %" PRIu64 ".\n",
            name, generation, minimum_expected);
    _exit(1);
  }

  // The generation should be stable.
  uint64_t new_generation = CRYPTO_get_fork_generation();
  if (new_generation != generation) {
    fprintf(stderr, "%s generation (#2) was %" PRIu64 ", wanted %" PRIu64 ".\n",
            name, new_generation, generation);
    _exit(1);
  }
}

static void WaitForChildOrDie(pid_t pid) {
  // Wait for the child and pass its exit code up.
  int status;
  if (WaitpidEINTR(pid, &status, 0) < 0) {
    perror("waitpid");
    _exit(1);
  }
  if (!WIFEXITED(status)) {
    fprintf(stderr, "Child did not exit cleanly.\n");
    _exit(1);
  }
  if (WEXITSTATUS(status) != 0) {
    // Pass the failure up.
    _exit(WEXITSTATUS(status));
  }
}

// ForkInChild forks a child which runs |f|. If the child exits unsuccessfully,
// this function will also exit unsuccessfully.
static void ForkInChild(std::function<void()> f) {
  fflush(stderr);  // Avoid duplicating any buffered output.
  const pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    _exit(1);
  }
  if (pid == 0) {
    f();
    _exit(0);
  }
  WaitForChildOrDie(pid);
}

// Many systems provide an alternate API to create a child process and run code
// in it.
static bool AlternateForkInChild(std::function<void()> f) {
  fflush(stderr);  // Avoid duplicating any buffered output.
#if defined(OPENSSL_LINUX)
  // On Linux, clone() can make new processes, too (even by default).
  const size_t kAlign = 16;
  const size_t kStackSize = 65536;
  void *temp_stack = OPENSSL_malloc(kStackSize + kAlign - 1);
  void *stack_top =
      static_cast<char *>(align_pointer(temp_stack, kAlign)) + kStackSize;
  pid_t pid = clone(
      +[](void *fp) {
        (*static_cast<std::function<void()> *>(fp))();
        return 0;
      },
      stack_top, SIGCHLD, &f);
  if (pid < 0) {
    perror("clone");
    _exit(1);
  }
  OPENSSL_free(temp_stack);
  WaitForChildOrDie(pid);
  return true;
#elif defined(OPENSSL_FREEBSD)
  // On FreeBSD, rfork() can be used to simulate fork().
  const pid_t pid = rfork(RFFDG | RFPROC);
  if (pid < 0) {
    perror("fork");
    _exit(1);
  }
  if (pid == 0) {
    f();
    _exit(0);
  }
  WaitForChildOrDie(pid);
  return true;
#else
  // No alternate methods known.
  return false;
#endif
}


TEST(ForkDetect, OSSupport) {
  uint64_t start = CRYPTO_get_fork_generation();
#if defined(OPENSSL_LINUX)
  if (start == 0) {
    struct utsname u;
    if (uname(&u) == 0) {
      const char *dot = strchr(u.release, '.');
      if (dot != nullptr) {
        auto version = std::tuple(atoi(u.release), atoi(dot + 1));
        if (version < std::tuple(4, 14)) {
          GTEST_SKIP() << "Fork detection not supported on Linux " << u.release
                       << " which is < 4.14. Skipping test.\n";
        }
      }
    }

    // Qemu claims support for any unsupported MADV_ flags, so the production
    // code currently always disables reliance on MADV_WIPEONFORK when on qemu.
    // Detect this situation explicitly here too so the test does not fail.
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT_GT(page_size, 0) << "System does not provide its page size. Expect "
                               "reduced performance, but no security impact.";
    void *addr =
        mmap(nullptr, static_cast<size_t>(page_size), PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(addr, nullptr)
        << "mmap() cannot produce a private anonymous mapping. Expect reduced "
           "performance, but no security impact.";
    int madvise_broken = madvise(addr, page_size, -1) == 0;
    munmap(addr, page_size);
    if (madvise_broken) {
      GTEST_SKIP()
          << "System claims support for madvise() with invalid flags (typical "
             "of qemu), which means return values are unreliable. Expect even "
             "more reduced performance at runtime.\n";
    }
  }
#endif
  EXPECT_NE(start, uint64_t{0})
      << "Fork detection not supported, but support is configured to be "
         "expected on this platform in crypto/rand/internal.h. Typically this "
         "means your OS or kernel is too old. Expect reduced performance, but "
         "no security impact.";
}

TEST(ForkDetect, Test) {
  uint64_t start = CRYPTO_get_fork_generation();
  if (start == 0) {
    GTEST_SKIP() << "Fork detection not supported. Skipping test.\n";
  }

  // The fork generation should be stable.
  EXPECT_EQ(start, CRYPTO_get_fork_generation());

  fflush(stderr);
  const pid_t child = fork();

  if (child == 0) {
    // Fork grandchildren before observing the fork generation. The
    // grandchildren will observe |start| + 1.
    for (int i = 0; i < 2; i++) {
      ForkInChild(
          [&] { CheckGenerationAtLeastInChild("Grandchild", start + 1); });
    }

    // Now the child also observes |start| + 1. This is fine because it has
    // already diverged from the grandchild at this point.

    CheckGenerationAtLeastInChild("Child", start + 1);

    // In the pthread_atfork the value may have changed.
    uint64_t child_generation = CRYPTO_get_fork_generation();
    // Forked grandchildren will now observe |start| + 2.
    for (int i = 0; i < 2; i++) {
      ForkInChild([&] {
        CheckGenerationAtLeastInChild("Grandchild", child_generation + 1);
      });
    }

#if defined(OPENSSL_THREADS)
    // The fork generation logic itself must be thread-safe. We test this in a
    // child process to capture the actual fork detection. This segment is meant
    // to be tested in TSan.
    ForkInChild([&] {
      std::vector<std::thread> threads(4);
      for (int i = 0; i < 2; i++) {
        for (auto &t : threads) {
          t = std::thread([&] {
            CheckGenerationAtLeastInChild("Grandchild thread",
                                          child_generation + 1);
          });
        }
        for (auto &t : threads) {
          t.join();
        }
      }
    });
#endif  // OPENSSL_THREADS

    // The child's observed value should be unchanged.
    if (child_generation != CRYPTO_get_fork_generation()) {
      fprintf(stderr,
              "Child generation (final stable check) was %" PRIu64
              ", wanted %" PRIu64 ".\n",
              child_generation, CRYPTO_get_fork_generation());
      _exit(1);
    }

    _exit(0);
  }

  ASSERT_GT(child, 0) << "Error in fork: " << strerror(errno);
  int status;
  ASSERT_EQ(child, WaitpidEINTR(child, &status, 0))
      << "Error in waitpid: " << strerror(errno);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(0, WEXITSTATUS(status)) << "Error in child process";

  // We still observe |start|.
  EXPECT_EQ(start, CRYPTO_get_fork_generation());
}

TEST(ForkDetect, TestAlternateFork) {
  uint64_t start = CRYPTO_get_fork_generation();
  if (start == 0) {
    GTEST_SKIP() << "Fork detection not supported. Skipping test.\n";
  }

  // The fork generation should be stable.
  EXPECT_EQ(start, CRYPTO_get_fork_generation());

  if (!AlternateForkInChild(
          [&] { CheckGenerationAtLeastInChild("Child", start + 1); })) {
    GTEST_SKIP() << "No alternate fork method detected. Skipping test.\n";
  }

  // We still observe |start|.
  EXPECT_EQ(start, CRYPTO_get_fork_generation());
}

}  // namespace
BSSL_NAMESPACE_END

#endif  // OPENSSL_FORK_DETECTION && !OPENSSL_TSAN && !OPENSSL_IOS
