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

#include <openssl/bio.h>

#if !defined(OPENSSL_TRUSTY)

#include <errno.h>
#include <string.h>

#if !defined(OPENSSL_WINDOWS)
#include <unistd.h>
#else
#include <io.h>
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <windows.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#endif

#include <openssl/err.h>
#include <openssl/mem.h>

#include "internal.h"
#include "../internal.h"


static int bio_fd_non_fatal_error(int err) {
  if (
#ifdef EWOULDBLOCK
    err == EWOULDBLOCK ||
#endif
#ifdef WSAEWOULDBLOCK
    err == WSAEWOULDBLOCK ||
#endif
#ifdef ENOTCONN
    err == ENOTCONN ||
#endif
#ifdef EINTR
    err == EINTR ||
#endif
#ifdef EAGAIN
    err == EAGAIN ||
#endif
#ifdef EPROTO
    err == EPROTO ||
#endif
#ifdef EINPROGRESS
    err == EINPROGRESS ||
#endif
#ifdef EALREADY
    err == EALREADY ||
#endif
    0) {
    return 1;
  }
  return 0;
}

#if defined(OPENSSL_WINDOWS)
  #define BORINGSSL_ERRNO (int)GetLastError()
  #define BORINGSSL_CLOSE _close
  #define BORINGSSL_LSEEK _lseek
  #define BORINGSSL_READ _read
  #define BORINGSSL_WRITE _write
#else
  #define BORINGSSL_ERRNO errno
  #define BORINGSSL_CLOSE close
  #define BORINGSSL_LSEEK lseek
  #define BORINGSSL_READ read
  #define BORINGSSL_WRITE write
#endif

int bio_fd_should_retry(int i) {
  if (i == -1) {
    return bio_fd_non_fatal_error(BORINGSSL_ERRNO);
  }
  return 0;
}

BIO *BIO_new_fd(int fd, int close_flag) {
  BIO *ret = BIO_new(BIO_s_fd());
  if (ret == NULL) {
    return NULL;
  }
  BIO_set_fd(ret, fd, close_flag);
  return ret;
}

static int fd_new(BIO *bio) {
  // num is used to store the file descriptor.
  bio->num = -1;
  return 1;
}

static int fd_free(BIO *bio) {
  if (bio->shutdown) {
    if (bio->init) {
      BORINGSSL_CLOSE(bio->num);
    }
    bio->init = 0;
  }
  return 1;
}

static int fd_read(BIO *b, char *out, int outl) {
  int ret = 0;

  ret = (int)BORINGSSL_READ(b->num, out, outl);
  BIO_clear_retry_flags(b);
  if (ret <= 0) {
    if (bio_fd_should_retry(ret)) {
      BIO_set_retry_read(b);
    }
  }

  return ret;
}

static int fd_write(BIO *b, const char *in, int inl) {
  int ret = (int)BORINGSSL_WRITE(b->num, in, inl);
  BIO_clear_retry_flags(b);
  if (ret <= 0) {
    if (bio_fd_should_retry(ret)) {
      BIO_set_retry_write(b);
    }
  }

  return ret;
}

static long fd_ctrl(BIO *b, int cmd, long num, void *ptr) {
  long ret = 1;
  int *ip;

  switch (cmd) {
    case BIO_CTRL_RESET:
      num = 0;
      OPENSSL_FALLTHROUGH;
    case BIO_C_FILE_SEEK:
      ret = 0;
      if (b->init) {
        ret = (long)BORINGSSL_LSEEK(b->num, num, SEEK_SET);
      }
      break;
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
      ret = 0;
      if (b->init) {
        ret = (long)BORINGSSL_LSEEK(b->num, 0, SEEK_CUR);
      }
      break;
    case BIO_C_SET_FD:
      fd_free(b);
      b->num = *((int *)ptr);
      b->shutdown = (int)num;
      b->init = 1;
      break;
    case BIO_C_GET_FD:
      if (b->init) {
        ip = (int *)ptr;
        if (ip != NULL) {
          *ip = b->num;
        }
        return b->num;
      } else {
        ret = -1;
      }
      break;
    case BIO_CTRL_GET_CLOSE:
      ret = b->shutdown;
      break;
    case BIO_CTRL_SET_CLOSE:
      b->shutdown = (int)num;
      break;
    case BIO_CTRL_PENDING:
    case BIO_CTRL_WPENDING:
      ret = 0;
      break;
    case BIO_CTRL_FLUSH:
      ret = 1;
      break;
    default:
      ret = 0;
      break;
  }

  return ret;
}

static int fd_gets(BIO *bp, char *buf, int size) {
  if (size <= 0) {
    return 0;
  }

  char *ptr = buf;
  char *end = buf + size - 1;
  while (ptr < end && fd_read(bp, ptr, 1) > 0) {
    char c = ptr[0];
    ptr++;
    if (c == '\n') {
      break;
    }
  }

  ptr[0] = '\0';

  // The output length is bounded by |size|.
  return (int)(ptr - buf);
}

static const BIO_METHOD methods_fdp = {
    BIO_TYPE_FD, "file descriptor", fd_write, fd_read, NULL /* puts */,
    fd_gets,     fd_ctrl,           fd_new,   fd_free, NULL /* callback_ctrl */,
};

const BIO_METHOD *BIO_s_fd(void) { return &methods_fdp; }

int BIO_set_fd(BIO *bio, int fd, int close_flag) {
  return (int)BIO_int_ctrl(bio, BIO_C_SET_FD, close_flag, fd);
}

int BIO_get_fd(BIO *bio, int *out_fd) {
  return (int)BIO_ctrl(bio, BIO_C_GET_FD, 0, (char *) out_fd);
}

#endif  // OPENSSL_TRUSTY
