// Copyright 2026 The BoringSSL Authors
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


#include <openssl/evp.h>

#include "../fipsmodule/digest/internal.h"  // IWYU pragma: keep
#include "../fipsmodule/keccak/internal.h"


BSSL_NAMESPACE_BEGIN
namespace {
template <boringssl_keccak_config_t Config>
void keccak_init(EVP_MD_CTX *ctx) {
  BORINGSSL_keccak_init(reinterpret_cast<BORINGSSL_keccak_st *>(ctx->md_data),
                        Config);
}

void keccak_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  BORINGSSL_keccak_absorb(reinterpret_cast<BORINGSSL_keccak_st *>(ctx->md_data),
                          static_cast<const uint8_t *>(data), count);
}

template <size_t Size>
void keccak_final(EVP_MD_CTX *ctx, uint8_t *out) {
  BORINGSSL_keccak_squeeze(
      reinterpret_cast<BORINGSSL_keccak_st *>(ctx->md_data),
      static_cast<uint8_t *>(out), Size);
}

void keccak_squeeze(EVP_MD_CTX *ctx, uint8_t *out, size_t len) {
  BORINGSSL_keccak_squeeze(
      reinterpret_cast<BORINGSSL_keccak_st *>(ctx->md_data),
      static_cast<uint8_t *>(out), len);
}

constexpr EVP_MD evp_md_sha3_256 = {
    /*type=*/NID_undef,
    /*md_size=*/SHA3_256_DIGEST_LENGTH,
    /*flags=*/0,
    /*init=*/keccak_init<boringssl_sha3_256>,
    /*update=*/keccak_update,
    /*final=*/keccak_final<SHA3_256_DIGEST_LENGTH>,
    /*squeeze=*/nullptr,
    /*block_size=*/SHA3_256_CBLOCK,
    /*ctx_size=*/sizeof(BORINGSSL_keccak_st),
};

constexpr EVP_MD evp_md_sha3_384 = {
    /*type=*/NID_undef,
    /*md_size=*/SHA3_384_DIGEST_LENGTH,
    /*flags=*/0,
    /*init=*/keccak_init<boringssl_sha3_384>,
    /*update=*/keccak_update,
    /*final=*/keccak_final<SHA3_384_DIGEST_LENGTH>,
    /*squeeze=*/nullptr,
    /*block_size=*/SHA3_384_CBLOCK,
    /*ctx_size=*/sizeof(BORINGSSL_keccak_st),
};

constexpr EVP_MD evp_md_sha3_512 = {
    /*type=*/NID_undef,
    /*md_size=*/SHA3_512_DIGEST_LENGTH,
    /*flags=*/0,
    /*init=*/keccak_init<boringssl_sha3_512>,
    /*update=*/keccak_update,
    /*final=*/keccak_final<SHA3_512_DIGEST_LENGTH>,
    /*squeeze=*/nullptr,
    /*block_size=*/SHA3_512_CBLOCK,
    /*ctx_size=*/sizeof(BORINGSSL_keccak_st),
};

constexpr EVP_MD evp_md_shake256 = {
    /*type=*/NID_undef,
    /*md_size=*/32,
    /*flags=*/0,
    /*init=*/keccak_init<boringssl_shake256>,
    /*update=*/keccak_update,
    /*final=*/keccak_final<32>,
    /*squeeze=*/keccak_squeeze,
    /*block_size=*/SHAKE256_CBLOCK,
    /*ctx_size=*/sizeof(BORINGSSL_keccak_st),
};

constexpr EVP_MD evp_md_turboshake256 = {
    /*type=*/NID_undef,
    /*md_size=*/32,
    /*flags=*/0,
    /*init=*/keccak_init<boringssl_turboshake256>,
    /*update=*/keccak_update,
    /*final=*/keccak_final<32>,
    /*squeeze=*/keccak_squeeze,
    /*block_size=*/SHAKE256_CBLOCK,
    /*ctx_size=*/sizeof(BORINGSSL_keccak_st),
};

static_assert(sizeof(BORINGSSL_keccak_st) <= EVP_MAX_MD_DATA_SIZE);
static_assert(SHA3_256_CBLOCK <= EVP_MAX_MD_BLOCK_SIZE);
static_assert(SHAKE256_CBLOCK <= EVP_MAX_MD_BLOCK_SIZE);
}  // namespace
BSSL_NAMESPACE_END

const EVP_MD *EVP_sha3_256() { return &bssl::evp_md_sha3_256; }
const EVP_MD *EVP_sha3_384() { return &bssl::evp_md_sha3_384; }
const EVP_MD *EVP_sha3_512() { return &bssl::evp_md_sha3_512; }
const EVP_MD *EVP_shake256() { return &bssl::evp_md_shake256; }
const EVP_MD *EVP_turboshake256() { return &bssl::evp_md_turboshake256; }
