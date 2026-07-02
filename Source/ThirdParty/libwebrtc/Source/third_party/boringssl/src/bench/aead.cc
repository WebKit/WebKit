// Copyright 2025 The BoringSSL Authors
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

#include <vector>

#include <benchmark/benchmark.h>

#include <openssl/aead.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

// kTLSADLen is the number of bytes of additional data that TLS passes to
// AEADs.
const size_t kTLSADLen = 13;
// kLegacyADLen is the number of bytes that TLS passes to the "legacy" AEADs.
// These are AEADs that weren't originally defined as AEADs, but which we use
// via the AEAD interface. In order for that to work, they have some TLS
// knowledge in them and construct a couple of the AD bytes internally.
const size_t kLegacyADLen = kTLSADLen - 2;

void BM_SpeedAEAD(benchmark::State &state, size_t ad_len,
                  evp_aead_direction_t direction, const EVP_AEAD *aead) {
  const unsigned kAlignment = 16;
  size_t input_len = static_cast<size_t>(state.range(0));
  ScopedEVP_AEAD_CTX ctx;
  const size_t key_len = EVP_AEAD_key_length(aead);
  const size_t nonce_len = EVP_AEAD_nonce_length(aead);
  const size_t overhead_len = EVP_AEAD_max_overhead(aead);

  std::vector<uint8_t> key(key_len);
  std::vector<uint8_t> nonce(nonce_len);
  std::vector<uint8_t> in_storage(input_len + kAlignment);
  // N.B. for EVP_AEAD_CTX_seal_scatter the input and output buffers may be the
  // same size. However, in the direction == evp_aead_open case we still use
  // non-scattering seal, hence we add overhead_len to the size of this buffer.
  std::vector<uint8_t> out_storage(input_len + overhead_len + kAlignment);
  std::vector<uint8_t> in2_storage(input_len + overhead_len + kAlignment);
  std::vector<uint8_t> ad(ad_len);
  std::vector<uint8_t> tag_storage(overhead_len + kAlignment);

  uint8_t *const in =
      static_cast<uint8_t *>(align_pointer(in_storage.data(), kAlignment));
  uint8_t *const out =
      static_cast<uint8_t *>(align_pointer(out_storage.data(), kAlignment));
  uint8_t *const tag =
      static_cast<uint8_t *>(align_pointer(tag_storage.data(), kAlignment));
  uint8_t *const in2 =
      static_cast<uint8_t *>(align_pointer(in2_storage.data(), kAlignment));

  if (!EVP_AEAD_CTX_init_with_direction(ctx.get(), aead, key.data(), key_len,
                                        EVP_AEAD_DEFAULT_TAG_LENGTH,
                                        evp_aead_seal)) {
    state.SkipWithError("Failed to create EVP_AEAD_CTX.");
    return;
  }

  if (direction == evp_aead_seal) {
    size_t tag_len;
    for (auto _ : state) {
      if (!EVP_AEAD_CTX_seal_scatter(
              ctx.get(), out, tag, &tag_len, overhead_len, nonce.data(),
              nonce_len, in, input_len, nullptr, 0, ad.data(), ad_len)) {
        state.SkipWithError("EVP_AEAD_CTX_seal failed.");
        return;
      }
    }
    state.SetBytesProcessed(state.iterations() * input_len);
  } else {
    size_t out_len;
    if (!EVP_AEAD_CTX_seal(ctx.get(), out, &out_len, input_len + overhead_len,
                           nonce.data(), nonce_len, in, input_len, ad.data(),
                           ad_len)) {
      state.SkipWithError("EVP_AEAD_CTX_seal failed.");
      return;
    }

    ctx.Reset();
    if (!EVP_AEAD_CTX_init_with_direction(ctx.get(), aead, key.data(), key_len,
                                          EVP_AEAD_DEFAULT_TAG_LENGTH,
                                          evp_aead_open)) {
      state.SkipWithError("Failed to create EVP_AEAD_CTX.");
      return;
    }

    size_t in2_len;
    for (auto _ : state) {
      // N.B. EVP_AEAD_CTX_open_gather is not implemented for all AEADs.
      if (!EVP_AEAD_CTX_open(ctx.get(), in2, &in2_len, input_len + overhead_len,
                             nonce.data(), nonce_len, out, out_len, ad.data(),
                             ad_len)) {
        state.SkipWithError("EVP_AEAD_CTX_open failed.");
        return;
      }
    }
    state.SetBytesProcessed(state.iterations() * input_len);
  }
}

static const int64_t kInputSizes[] = {16, 256, 1350, 8192, 16384};

void SetInputLength(benchmark::Benchmark *bench) {
  bench->ArgName("InputSize");
  auto input_sizes = bench::GetInputSizes(bench);
  if (input_sizes.empty()) {
    bench->ArgsProduct(
        {std::vector<int64_t>(kInputSizes, std::end(kInputSizes))});
  } else {
    bench->ArgsProduct(
        {std::vector<int64_t>(input_sizes.begin(), input_sizes.end())});
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_192_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_192_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_chacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_chacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_xchacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_xchacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLength);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLength);
}

}  // namespace
BSSL_NAMESPACE_END
