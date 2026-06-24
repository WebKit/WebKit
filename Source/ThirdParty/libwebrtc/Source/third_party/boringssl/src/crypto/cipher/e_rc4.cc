// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <assert.h>
#include <string.h>

#include <openssl/cipher.h>
#include <openssl/nid.h>
#include <openssl/rc4.h>

#include "../fipsmodule/cipher/internal.h"


static int rc4_init_key(EVP_CIPHER_CTX *ctx, const uint8_t *key,
                        const uint8_t *iv, int enc) {
  RC4_KEY *rc4key = (RC4_KEY *)ctx->cipher_data;

  RC4_set_key(rc4key, EVP_CIPHER_CTX_key_length(ctx), key);
  return 1;
}

static int rc4_cipher_update(EVP_CIPHER_CTX *ctx, uint8_t *out,
                             const uint8_t *in, size_t len) {
  RC4_KEY *rc4key = (RC4_KEY *)ctx->cipher_data;

  RC4(rc4key, len, in, out);
  return 1;
}

static const EVP_CIPHER rc4 = {
    /*nid=*/NID_rc4,
    /*block_size=*/1,
    /*key_len=*/16,
    /*iv_len=*/0,
    /*ctx_size=*/sizeof(RC4_KEY),
    /*flags=*/EVP_CIPH_VARIABLE_LENGTH,
    /*init=*/rc4_init_key,
    /*cipher_update=*/rc4_cipher_update,
    /*cipher_final=*/nullptr,
    /*update_aad=*/nullptr,
    /*cleanup=*/nullptr,
    /*ctrl=*/nullptr,
};

const EVP_CIPHER *EVP_rc4() { return &rc4; }
