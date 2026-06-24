// Copyright 2014 The BoringSSL Authors
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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // needed for syscall() on Linux.
#endif

#include <openssl/rand.h>

#include "../bcm_support.h"
#include "internal.h"

#if defined(OPENSSL_RAND_URANDOM)

#include <errno.h>
#include <fcntl.h>
#include <linux/random.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../internal.h"


using namespace bssl;

#if defined(OPENSSL_MSAN)
extern "C" {
void __msan_unpoison(void *, size_t);
}
#endif

static ssize_t boringssl_getrandom(void *buf, size_t buf_len, unsigned flags) {
  ssize_t ret;
  do {
    ret = syscall(__NR_getrandom, buf, buf_len, flags);
  } while (ret == -1 && errno == EINTR);

#if defined(OPENSSL_MSAN)
  if (ret > 0) {
    // MSAN doesn't recognise |syscall| and thus doesn't notice that we have
    // initialised the output buffer.
    __msan_unpoison(buf, ret);
  }
#endif  // OPENSSL_MSAN

  return ret;
}

// kHaveGetrandom in |urandom_fd| signals that |getrandom| or |getentropy| is
// available and should be used instead.
static const int kHaveGetrandom = -3;

// urandom_fd is a file descriptor to /dev/urandom. It's protected by |once|.
static int urandom_fd;

static CRYPTO_once_t rand_once = CRYPTO_ONCE_INIT;

// init_once initializes the state of this module to values previously
// requested. This is the only function that modifies |urandom_fd|, which may be
// read safely after calling the once.
static void init_once() {
  int have_getrandom;
  uint8_t dummy;
  ssize_t getrandom_ret =
      boringssl_getrandom(&dummy, sizeof(dummy), GRND_NONBLOCK);
  if (getrandom_ret == 1) {
    have_getrandom = 1;
  } else if (getrandom_ret == -1 && errno == EAGAIN) {
    // We have getrandom, but the entropy pool has not been initialized yet.
    have_getrandom = 1;
  } else if (getrandom_ret == -1 && errno == ENOSYS) {
    // Fallthrough to using /dev/urandom, below.
    have_getrandom = 0;
  } else {
    // Other errors are fatal.
    perror("getrandom");
    abort();
  }

  if (have_getrandom) {
    urandom_fd = kHaveGetrandom;
    return;
  }

  // FIPS builds must support getrandom.
#if defined(BORINGSSL_FIPS)
  perror("getrandom not found");
  abort();
#endif

  int fd;
  do {
    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  } while (fd == -1 && errno == EINTR);

  if (fd < 0) {
    perror("failed to open /dev/urandom");
    abort();
  }

  urandom_fd = fd;
}

void bssl::CRYPTO_init_sysrand() { CRYPTO_once(&rand_once, init_once); }

// CRYPTO_sysrand writes |len| bytes of entropy into |out|.
void bssl::CRYPTO_sysrand(uint8_t *out, size_t len) {
  if (len == 0) {
    return;
  }

  CRYPTO_init_sysrand();

  // Clear |errno| so it has defined value if |read| or |getrandom|
  // "successfully" returns zero.
  errno = 0;
  while (len > 0) {
    ssize_t r;

    if (urandom_fd == kHaveGetrandom) {
      r = boringssl_getrandom(out, len, 0);
    } else {
      do {
        r = read(urandom_fd, out, len);
      } while (r == -1 && errno == EINTR);
    }

    if (r <= 0) {
      perror("entropy fill failed");
      abort();
    }
    out += r;
    len -= r;
  }
}

#endif  // OPENSSL_RAND_URANDOM
