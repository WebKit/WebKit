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

#include <openssl/base64.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/type_check.h>


/* Encoding. */

static const unsigned char data_bin2ascii[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define conv_bin2ascii(a) (data_bin2ascii[(a) & 0x3f])

OPENSSL_COMPILE_ASSERT(sizeof(((EVP_ENCODE_CTX *)(NULL))->data) % 3 == 0,
                       data_length_must_be_multiple_of_base64_chunk_size);

int EVP_EncodedLength(size_t *out_len, size_t len) {
  if (len + 2 < len) {
    return 0;
  }
  len += 2;
  len /= 3;

  if (((len << 2) >> 2) != len) {
    return 0;
  }
  len <<= 2;

  if (len + 1 < len) {
    return 0;
  }
  len++;

  *out_len = len;
  return 1;
}

void EVP_EncodeInit(EVP_ENCODE_CTX *ctx) {
  memset(ctx, 0, sizeof(EVP_ENCODE_CTX));
}

void EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, uint8_t *out, int *out_len,
                      const uint8_t *in, size_t in_len) {
  size_t total = 0;

  *out_len = 0;
  if (in_len == 0) {
    return;
  }

  assert(ctx->data_used < sizeof(ctx->data));

  if (sizeof(ctx->data) - ctx->data_used > in_len) {
    memcpy(&ctx->data[ctx->data_used], in, in_len);
    ctx->data_used += (unsigned)in_len;
    return;
  }

  if (ctx->data_used != 0) {
    const size_t todo = sizeof(ctx->data) - ctx->data_used;
    memcpy(&ctx->data[ctx->data_used], in, todo);
    in += todo;
    in_len -= todo;

    size_t encoded = EVP_EncodeBlock(out, ctx->data, sizeof(ctx->data));
    ctx->data_used = 0;

    out += encoded;
    *(out++) = '\n';
    *out = '\0';

    total = encoded + 1;
  }

  while (in_len >= sizeof(ctx->data)) {
    size_t encoded = EVP_EncodeBlock(out, in, sizeof(ctx->data));
    in += sizeof(ctx->data);
    in_len -= sizeof(ctx->data);

    out += encoded;
    *(out++) = '\n';
    *out = '\0';

    if (total + encoded + 1 < total) {
      *out_len = 0;
      return;
    }

    total += encoded + 1;
  }

  if (in_len != 0) {
    memcpy(ctx->data, in, in_len);
  }

  ctx->data_used = (unsigned)in_len;

  if (total > INT_MAX) {
    /* We cannot signal an error, but we can at least avoid making *out_len
     * negative. */
    total = 0;
  }
  *out_len = (int)total;
}

void EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, uint8_t *out, int *out_len) {
  if (ctx->data_used == 0) {
    *out_len = 0;
    return;
  }

  size_t encoded = EVP_EncodeBlock(out, ctx->data, ctx->data_used);
  out[encoded++] = '\n';
  out[encoded] = '\0';
  ctx->data_used = 0;

  /* ctx->data_used is bounded by sizeof(ctx->data), so this does not
   * overflow. */
  assert(encoded <= INT_MAX);
  *out_len = (int)encoded;
}

size_t EVP_EncodeBlock(uint8_t *dst, const uint8_t *src, size_t src_len) {
  uint32_t l;
  size_t remaining = src_len, ret = 0;

  while (remaining) {
    if (remaining >= 3) {
      l = (((uint32_t)src[0]) << 16L) | (((uint32_t)src[1]) << 8L) | src[2];
      *(dst++) = conv_bin2ascii(l >> 18L);
      *(dst++) = conv_bin2ascii(l >> 12L);
      *(dst++) = conv_bin2ascii(l >> 6L);
      *(dst++) = conv_bin2ascii(l);
      remaining -= 3;
    } else {
      l = ((uint32_t)src[0]) << 16L;
      if (remaining == 2) {
        l |= ((uint32_t)src[1] << 8L);
      }

      *(dst++) = conv_bin2ascii(l >> 18L);
      *(dst++) = conv_bin2ascii(l >> 12L);
      *(dst++) = (remaining == 1) ? '=' : conv_bin2ascii(l >> 6L);
      *(dst++) = '=';
      remaining = 0;
    }
    ret += 4;
    src += 3;
  }

  *dst = '\0';
  return ret;
}


/* Decoding. */

int EVP_DecodedLength(size_t *out_len, size_t len) {
  if (len % 4 != 0) {
    return 0;
  }

  *out_len = (len / 4) * 3;
  return 1;
}

void EVP_DecodeInit(EVP_ENCODE_CTX *ctx) {
  memset(ctx, 0, sizeof(EVP_ENCODE_CTX));
}

/* kBase64ASCIIToBinData maps characters (c < 128) to their base64 value, or
 * else 0xff if they are invalid. As a special case, the padding character
 * ('=') is mapped to zero. */
static const uint8_t kBase64ASCIIToBinData[128] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff,
    0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static uint8_t base64_ascii_to_bin(uint8_t a) {
  if (a >= 128) {
    return 0xFF;
  }

  return kBase64ASCIIToBinData[a];
}

/* base64_decode_quad decodes a single “quad” (i.e. four characters) of base64
 * data and writes up to three bytes to |out|. It sets |*out_num_bytes| to the
 * number of bytes written, which will be less than three if the quad ended
 * with padding.  It returns one on success or zero on error. */
static int base64_decode_quad(uint8_t *out, size_t *out_num_bytes,
                              const uint8_t *in) {
  const uint8_t a = base64_ascii_to_bin(in[0]);
  const uint8_t b = base64_ascii_to_bin(in[1]);
  const uint8_t c = base64_ascii_to_bin(in[2]);
  const uint8_t d = base64_ascii_to_bin(in[3]);
  if (a == 0xff || b == 0xff || c == 0xff || d == 0xff) {
    return 0;
  }

  const uint32_t v = ((uint32_t)a) << 18 | ((uint32_t)b) << 12 |
                     ((uint32_t)c) << 6 | (uint32_t)d;

  const unsigned padding_pattern = (in[0] == '=') << 3 |
                                   (in[1] == '=') << 2 |
                                   (in[2] == '=') << 1 |
                                   (in[3] == '=');

  switch (padding_pattern) {
    case 0:
      /* The common case of no padding. */
      *out_num_bytes = 3;
      out[0] = v >> 16;
      out[1] = v >> 8;
      out[2] = v;
      break;

    case 1: /* xxx= */
      *out_num_bytes = 2;
      out[0] = v >> 16;
      out[1] = v >> 8;
      break;

    case 3: /* xx== */
      *out_num_bytes = 1;
      out[0] = v >> 16;
      break;

    default:
      return 0;
  }

  return 1;
}

int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, uint8_t *out, int *out_len,
                     const uint8_t *in, size_t in_len) {
  *out_len = 0;

  if (ctx->error_encountered) {
    return -1;
  }

  size_t bytes_out = 0, i;
  for (i = 0; i < in_len; i++) {
    const char c = in[i];
    switch (c) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        continue;
    }

    if (base64_ascii_to_bin(c) == 0xff || ctx->eof_seen) {
      ctx->error_encountered = 1;
      return -1;
    }

    ctx->data[ctx->data_used++] = c;
    if (ctx->data_used == 4) {
      size_t num_bytes_resulting;
      if (!base64_decode_quad(out, &num_bytes_resulting, ctx->data)) {
        ctx->error_encountered = 1;
        return -1;
      }

      ctx->data_used = 0;
      bytes_out += num_bytes_resulting;
      out += num_bytes_resulting;

      if (num_bytes_resulting < 3) {
        ctx->eof_seen = 1;
      }
    }
  }

  if (bytes_out > INT_MAX) {
    ctx->error_encountered = 1;
    *out_len = 0;
    return -1;
  }
  *out_len = (int)bytes_out;

  if (ctx->eof_seen) {
    return 0;
  }

  return 1;
}

int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, uint8_t *out, int *out_len) {
  *out_len = 0;
  if (ctx->error_encountered || ctx->data_used != 0) {
    return -1;
  }

  return 1;
}

int EVP_DecodeBase64(uint8_t *out, size_t *out_len, size_t max_out,
                     const uint8_t *in, size_t in_len) {
  *out_len = 0;

  if (in_len % 4 != 0) {
    return 0;
  }

  size_t max_len;
  if (!EVP_DecodedLength(&max_len, in_len) ||
      max_out < max_len) {
    return 0;
  }

  size_t i, bytes_out = 0;
  for (i = 0; i < in_len; i += 4) {
    size_t num_bytes_resulting;

    if (!base64_decode_quad(out, &num_bytes_resulting, &in[i])) {
      return 0;
    }

    bytes_out += num_bytes_resulting;
    out += num_bytes_resulting;
    if (num_bytes_resulting != 3 && i != in_len - 4) {
      return 0;
    }
  }

  *out_len = bytes_out;
  return 1;
}

int EVP_DecodeBlock(uint8_t *dst, const uint8_t *src, size_t src_len) {
  /* Trim spaces and tabs from the beginning of the input. */
  while (src_len > 0) {
    if (src[0] != ' ' && src[0] != '\t') {
      break;
    }

    src++;
    src_len--;
  }

  /* Trim newlines, spaces and tabs from the end of the line. */
  while (src_len > 0) {
    switch (src[src_len-1]) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        src_len--;
        continue;
    }

    break;
  }

  size_t dst_len;
  if (!EVP_DecodedLength(&dst_len, src_len) ||
      dst_len > INT_MAX ||
      !EVP_DecodeBase64(dst, &dst_len, dst_len, src, src_len)) {
    return -1;
  }

  /* EVP_DecodeBlock does not take padding into account, so put the
   * NULs back in... so the caller can strip them back out. */
  while (dst_len % 3 != 0) {
    dst[dst_len++] = '\0';
  }
  assert(dst_len <= INT_MAX);

  return (int)dst_len;
}
