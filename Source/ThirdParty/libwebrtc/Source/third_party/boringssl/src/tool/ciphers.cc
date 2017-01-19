/* Copyright (c) 2015, Google Inc.
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

#include <stdint.h>
#include <stdlib.h>

#include <openssl/ssl.h>

#include "internal.h"


bool Ciphers(const std::vector<std::string> &args) {
  if (args.size() != 1) {
    fprintf(stderr, "Usage: bssl ciphers <cipher suite string>\n");
    return false;
  }

  const std::string &ciphers_string = args.back();

  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(SSLv23_client_method()));
  if (!SSL_CTX_set_cipher_list(ctx.get(), ciphers_string.c_str())) {
    fprintf(stderr, "Failed to parse cipher suite config.\n");
    ERR_print_errors_fp(stderr);
    return false;
  }

  const struct ssl_cipher_preference_list_st *pref_list = ctx->cipher_list;
  STACK_OF(SSL_CIPHER) *ciphers = pref_list->ciphers;

  bool last_in_group = false;
  for (size_t i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
    bool in_group = pref_list->in_group_flags[i];
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(ciphers, i);

    if (in_group && !last_in_group) {
      printf("[\n  ");
    } else if (last_in_group) {
      printf("  ");
    }

    printf("%s\n", SSL_CIPHER_get_name(cipher));

    if (!in_group && last_in_group) {
      printf("]\n");
    }
    last_in_group = in_group;
  }

  return true;
}
