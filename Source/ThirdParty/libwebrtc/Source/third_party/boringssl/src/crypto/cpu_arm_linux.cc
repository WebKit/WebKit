// Copyright 2016 The BoringSSL Authors
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

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_ARM) && \
    defined(OPENSSL_LINUX) && !defined(OPENSSL_STATIC_ARMCAP)
#include <errno.h>
#include <fcntl.h>
#include <sys/auxv.h>
#include <sys/types.h>
#include <unistd.h>

#include <string_view>

#include <openssl/mem.h>

#include "cpu_arm_linux.h"

using namespace bssl;

static int open_eintr(const char *path, int flags) {
  int ret;
  do {
    ret = open(path, flags);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

static ssize_t read_eintr(int fd, void *out, size_t len) {
  ssize_t ret;
  do {
    ret = read(fd, out, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

// read_file opens |path| and reads until end-of-file. On success, it returns
// one and sets |*out_ptr| and |*out_len| to a newly-allocated buffer with the
// contents. Otherwise, it returns zero.
static int read_file(char **out_ptr, size_t *out_len, const char *path) {
  int fd = open_eintr(path, O_RDONLY);
  if (fd < 0) {
    return 0;
  }

  static const size_t kReadSize = 1024;
  int ret = 0;
  size_t cap = kReadSize, len = 0;
  char *buf = reinterpret_cast<char *>(OPENSSL_malloc(cap));
  if (buf == nullptr) {
    goto err;
  }

  for (;;) {
    if (cap - len < kReadSize) {
      size_t new_cap = cap * 2;
      if (new_cap < cap) {
        goto err;
      }
      char *new_buf = reinterpret_cast<char *>(OPENSSL_realloc(buf, new_cap));
      if (new_buf == nullptr) {
        goto err;
      }
      buf = new_buf;
      cap = new_cap;
    }

    ssize_t bytes_read = read_eintr(fd, buf + len, kReadSize);
    if (bytes_read < 0) {
      goto err;
    }
    if (bytes_read == 0) {
      break;
    }
    len += bytes_read;
  }

  *out_ptr = buf;
  *out_len = len;
  ret = 1;
  buf = nullptr;

err:
  OPENSSL_free(buf);
  close(fd);
  return ret;
}

static int g_needs_hwcap2_workaround;

void bssl::OPENSSL_cpuid_setup() {
  // Matching OpenSSL, only report other features if NEON is present.
  unsigned long hwcap = getauxval(AT_HWCAP);
  if (hwcap & CRYPTO_HWCAP_NEON) {
#if defined(HWCAP_ARM_NEON)
      static_assert(HWCAP_ARM_NEON == CRYPTO_HWCAP_NEON,
                    "CRYPTO_HWCAP values must match Linux");
#endif
    OPENSSL_armcap_P |= ARMV7_NEON;

    // Some ARMv8 Android devices don't expose AT_HWCAP2. Fall back to
    // /proc/cpuinfo. See https://crbug.com/40644934. The fix was added to
    // Android CTS in N, so, after Net.NeedsHWCAP2Workaround confirms this, we
    // should be able to disable this when __ANDROID_MIN_SDK_VERSION__ is high
    // enough. (It may not be worth carrying the workaround at all at that
    // point. Then again, AES and PMULL extensions are crucial for performance
    // when available.)
    unsigned long hwcap2 = getauxval(AT_HWCAP2);
    if (hwcap2 == 0) {
      char *cpuinfo_data = nullptr;
      size_t cpuinfo_len = 0;
      if (read_file(&cpuinfo_data, &cpuinfo_len, "/proc/cpuinfo")) {
        hwcap2 = armcap::GetHWCAP2FromCpuinfo(
            std::string_view(cpuinfo_data, cpuinfo_len));
        g_needs_hwcap2_workaround = hwcap2 != 0;
        OPENSSL_free(cpuinfo_data);
      }
    }

    // HWCAP2_* values, without the "CRYPTO_" prefix, are exposed through
    // <sys/auxv.h> in some versions of glibc(>= 2.41). Assert that we don't
    // diverge from those values.
    if (hwcap2 & CRYPTO_HWCAP2_AES) {
#if defined(HWCAP2_AES)
      static_assert(HWCAP2_AES == CRYPTO_HWCAP2_AES,
                    "CRYPTO_HWCAP2 values must match Linux");
#endif
      OPENSSL_armcap_P |= ARMV8_AES;
    }
    if (hwcap2 & CRYPTO_HWCAP2_PMULL) {
#if defined(HWCAP2_PMULL)
      static_assert(HWCAP2_PMULL == CRYPTO_HWCAP2_PMULL,
                    "CRYPTO_HWCAP2 values must match Linux");
#endif
      OPENSSL_armcap_P |= ARMV8_PMULL;
    }
    if (hwcap2 & CRYPTO_HWCAP2_SHA1) {
#if defined(HWCAP2_SHA1)
      static_assert(HWCAP2_SHA1 == CRYPTO_HWCAP2_SHA1,
                    "CRYPTO_HWCAP2 values must match Linux");
#endif
      OPENSSL_armcap_P |= ARMV8_SHA1;
    }
    if (hwcap2 & CRYPTO_HWCAP2_SHA2) {
#if defined(HWCAP2_SHA2)
      static_assert(HWCAP2_SHA2 == CRYPTO_HWCAP2_SHA2,
                    "CRYPTO_HWCAP2 values must match Linux");
#endif
      OPENSSL_armcap_P |= ARMV8_SHA256;
    }
  }
}

int CRYPTO_has_broken_NEON() { return 0; }

int CRYPTO_needs_hwcap2_workaround() {
  OPENSSL_init_cpuid();
  return g_needs_hwcap2_workaround;
}

#endif  // OPENSSL_ARM && OPENSSL_LINUX && !OPENSSL_STATIC_ARMCAP
