/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <openssl/mem.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <openssl/err.h>

#if defined(OPENSSL_WINDOWS)
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <windows.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#endif

#include "internal.h"


#define OPENSSL_MALLOC_PREFIX 8
OPENSSL_STATIC_ASSERT(OPENSSL_MALLOC_PREFIX >= sizeof(size_t),
                      "size_t too large");

#if defined(OPENSSL_ASAN)
void __asan_poison_memory_region(const volatile void *addr, size_t size);
void __asan_unpoison_memory_region(const volatile void *addr, size_t size);
#else
static void __asan_poison_memory_region(const void *addr, size_t size) {}
static void __asan_unpoison_memory_region(const void *addr, size_t size) {}
#endif

// Windows doesn't really support weak symbols as of May 2019, and Clang on
// Windows will emit strong symbols instead. See
// https://bugs.llvm.org/show_bug.cgi?id=37598
#if defined(__ELF__) && defined(__GNUC__)
#define WEAK_SYMBOL_FUNC(rettype, name, args) \
  rettype name args __attribute__((weak));
#else
#define WEAK_SYMBOL_FUNC(rettype, name, args) static rettype(*name) args = NULL;
#endif

// sdallocx is a sized |free| function. By passing the size (which we happen to
// always know in BoringSSL), the malloc implementation can save work. We cannot
// depend on |sdallocx| being available, however, so it's a weak symbol.
//
// This will always be safe, but will only be overridden if the malloc
// implementation is statically linked with BoringSSL. So, if |sdallocx| is
// provided in, say, libc.so, we still won't use it because that's dynamically
// linked. This isn't an ideal result, but its helps in some cases.
WEAK_SYMBOL_FUNC(void, sdallocx, (void *ptr, size_t size, int flags));

// The following three functions can be defined to override default heap
// allocation and freeing. If defined, it is the responsibility of
// |OPENSSL_memory_free| to zero out the memory before returning it to the
// system. |OPENSSL_memory_free| will not be passed NULL pointers.
//
// WARNING: These functions are called on every allocation and free in
// BoringSSL across the entire process. They may be called by any code in the
// process which calls BoringSSL, including in process initializers and thread
// destructors. When called, BoringSSL may hold pthreads locks. Any other code
// in the process which, directly or indirectly, calls BoringSSL may be on the
// call stack and may itself be using arbitrary synchronization primitives.
//
// As a result, these functions may not have the usual programming environment
// available to most C or C++ code. In particular, they may not call into
// BoringSSL, or any library which depends on BoringSSL. Any synchronization
// primitives used must tolerate every other synchronization primitive linked
// into the process, including pthreads locks. Failing to meet these constraints
// may result in deadlocks, crashes, or memory corruption.
WEAK_SYMBOL_FUNC(void*, OPENSSL_memory_alloc, (size_t size));
WEAK_SYMBOL_FUNC(void, OPENSSL_memory_free, (void *ptr));
WEAK_SYMBOL_FUNC(size_t, OPENSSL_memory_get_size, (void *ptr));

void *OPENSSL_malloc(size_t size) {
  if (OPENSSL_memory_alloc != NULL) {
    assert(OPENSSL_memory_free != NULL);
    assert(OPENSSL_memory_get_size != NULL);
    return OPENSSL_memory_alloc(size);
  }

  if (size + OPENSSL_MALLOC_PREFIX < size) {
    return NULL;
  }

  void *ptr = malloc(size + OPENSSL_MALLOC_PREFIX);
  if (ptr == NULL) {
    return NULL;
  }

  *(size_t *)ptr = size;

  __asan_poison_memory_region(ptr, OPENSSL_MALLOC_PREFIX);
  return ((uint8_t *)ptr) + OPENSSL_MALLOC_PREFIX;
}

void OPENSSL_free(void *orig_ptr) {
  if (orig_ptr == NULL) {
    return;
  }

  if (OPENSSL_memory_free != NULL) {
    OPENSSL_memory_free(orig_ptr);
    return;
  }

  void *ptr = ((uint8_t *)orig_ptr) - OPENSSL_MALLOC_PREFIX;
  __asan_unpoison_memory_region(ptr, OPENSSL_MALLOC_PREFIX);

  size_t size = *(size_t *)ptr;
  OPENSSL_cleanse(ptr, size + OPENSSL_MALLOC_PREFIX);
  if (sdallocx) {
    sdallocx(ptr, size + OPENSSL_MALLOC_PREFIX, 0 /* flags */);
  } else {
    free(ptr);
  }
}

void *OPENSSL_realloc(void *orig_ptr, size_t new_size) {
  if (orig_ptr == NULL) {
    return OPENSSL_malloc(new_size);
  }

  size_t old_size;
  if (OPENSSL_memory_get_size != NULL) {
    old_size = OPENSSL_memory_get_size(orig_ptr);
  } else {
    void *ptr = ((uint8_t *)orig_ptr) - OPENSSL_MALLOC_PREFIX;
    __asan_unpoison_memory_region(ptr, OPENSSL_MALLOC_PREFIX);
    old_size = *(size_t *)ptr;
    __asan_poison_memory_region(ptr, OPENSSL_MALLOC_PREFIX);
  }

  void *ret = OPENSSL_malloc(new_size);
  if (ret == NULL) {
    return NULL;
  }

  size_t to_copy = new_size;
  if (old_size < to_copy) {
    to_copy = old_size;
  }

  memcpy(ret, orig_ptr, to_copy);
  OPENSSL_free(orig_ptr);

  return ret;
}

void OPENSSL_cleanse(void *ptr, size_t len) {
#if defined(OPENSSL_WINDOWS)
  SecureZeroMemory(ptr, len);
#else
  OPENSSL_memset(ptr, 0, len);

#if !defined(OPENSSL_NO_ASM)
  /* As best as we can tell, this is sufficient to break any optimisations that
     might try to eliminate "superfluous" memsets. If there's an easy way to
     detect memset_s, it would be better to use that. */
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
#endif  // !OPENSSL_NO_ASM
}

void OPENSSL_clear_free(void *ptr, size_t unused) {
  OPENSSL_free(ptr);
}

int CRYPTO_memcmp(const void *in_a, const void *in_b, size_t len) {
  const uint8_t *a = in_a;
  const uint8_t *b = in_b;
  uint8_t x = 0;

  for (size_t i = 0; i < len; i++) {
    x |= a[i] ^ b[i];
  }

  return x;
}

uint32_t OPENSSL_hash32(const void *ptr, size_t len) {
  // These are the FNV-1a parameters for 32 bits.
  static const uint32_t kPrime = 16777619u;
  static const uint32_t kOffsetBasis = 2166136261u;

  const uint8_t *in = ptr;
  uint32_t h = kOffsetBasis;

  for (size_t i = 0; i < len; i++) {
    h ^= in[i];
    h *= kPrime;
  }

  return h;
}

size_t OPENSSL_strnlen(const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (s[i] == 0) {
      return i;
    }
  }

  return len;
}

char *OPENSSL_strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  const size_t len = strlen(s) + 1;
  char *ret = OPENSSL_malloc(len);
  if (ret == NULL) {
    return NULL;
  }
  OPENSSL_memcpy(ret, s, len);
  return ret;
}

int OPENSSL_tolower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

int OPENSSL_strcasecmp(const char *a, const char *b) {
  for (size_t i = 0;; i++) {
    const int aa = OPENSSL_tolower(a[i]);
    const int bb = OPENSSL_tolower(b[i]);

    if (aa < bb) {
      return -1;
    } else if (aa > bb) {
      return 1;
    } else if (aa == 0) {
      return 0;
    }
  }
}

int OPENSSL_strncasecmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    const int aa = OPENSSL_tolower(a[i]);
    const int bb = OPENSSL_tolower(b[i]);

    if (aa < bb) {
      return -1;
    } else if (aa > bb) {
      return 1;
    } else if (aa == 0) {
      return 0;
    }
  }

  return 0;
}

int BIO_snprintf(char *buf, size_t n, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = BIO_vsnprintf(buf, n, format, args);
  va_end(args);
  return ret;
}

int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args) {
  return vsnprintf(buf, n, format, args);
}

char *OPENSSL_strndup(const char *str, size_t size) {
  char *ret;
  size_t alloc_size;

  if (str == NULL) {
    return NULL;
  }

  size = OPENSSL_strnlen(str, size);

  alloc_size = size + 1;
  if (alloc_size < size) {
    // overflow
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  ret = OPENSSL_malloc(alloc_size);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }

  OPENSSL_memcpy(ret, str, size);
  ret[size] = '\0';
  return ret;
}

size_t OPENSSL_strlcpy(char *dst, const char *src, size_t dst_size) {
  size_t l = 0;

  for (; dst_size > 1 && *src; dst_size--) {
    *dst++ = *src++;
    l++;
  }

  if (dst_size) {
    *dst = 0;
  }

  return l + strlen(src);
}

size_t OPENSSL_strlcat(char *dst, const char *src, size_t dst_size) {
  size_t l = 0;
  for (; dst_size > 0 && *dst; dst_size--, dst++) {
    l++;
  }
  return l + OPENSSL_strlcpy(dst, src, dst_size);
}

void *OPENSSL_memdup(const void *data, size_t size) {
  if (size == 0) {
    return NULL;
  }

  void *ret = OPENSSL_malloc(size);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_MALLOC_FAILURE);
    return NULL;
  }

  OPENSSL_memcpy(ret, data, size);
  return ret;
}
