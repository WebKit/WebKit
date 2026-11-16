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
  const size_t input_len = static_cast<size_t>(state.range(0));
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

  uint8_t *in =
      static_cast<uint8_t *>(align_pointer(in_storage.data(), kAlignment));
  uint8_t *out =
      static_cast<uint8_t *>(align_pointer(out_storage.data(), kAlignment));
  uint8_t *tag =
      static_cast<uint8_t *>(align_pointer(tag_storage.data(), kAlignment));
  uint8_t *in2 =
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
      benchmark::DoNotOptimize(nonce.data());
      benchmark::DoNotOptimize(in);
      benchmark::DoNotOptimize(ad.data());
      benchmark::DoNotOptimize(ad_len);
      if (!EVP_AEAD_CTX_seal_scatter(
              ctx.get(), out, tag, &tag_len, overhead_len, nonce.data(),
              nonce_len, in, input_len, nullptr, 0, ad.data(), ad_len)) {
        state.SkipWithError("EVP_AEAD_CTX_seal failed.");
        return;
      }
      benchmark::DoNotOptimize(out);
      benchmark::DoNotOptimize(tag);
      benchmark::DoNotOptimize(tag_len);
      benchmark::ClobberMemory();
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
      benchmark::DoNotOptimize(nonce.data());
      benchmark::DoNotOptimize(out);
      benchmark::DoNotOptimize(out_len);
      benchmark::DoNotOptimize(ad.data());
      benchmark::DoNotOptimize(ad_len);
      // N.B. EVP_AEAD_CTX_open_gather is not implemented for all AEADs.
      if (!EVP_AEAD_CTX_open(ctx.get(), in2, &in2_len, input_len + overhead_len,
                             nonce.data(), nonce_len, out, out_len, ad.data(),
                             ad_len)) {
        state.SkipWithError("EVP_AEAD_CTX_open failed.");
        return;
      }
      benchmark::DoNotOptimize(in2);
      benchmark::DoNotOptimize(in2_len);
      benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * input_len);
  }
}

void BM_SpeedAEADv(benchmark::State &state, size_t ad_len,
                   evp_aead_direction_t direction, const EVP_AEAD *aead) {
  const unsigned kAlignment = 16;
  const size_t input_len = static_cast<size_t>(state.range(0));
  const size_t iovec_first = static_cast<size_t>(state.range(1));
  const size_t iovec_others = static_cast<size_t>(state.range(2));
  ScopedEVP_AEAD_CTX ctx;
  const size_t key_len = EVP_AEAD_key_length(aead);
  const size_t nonce_len = EVP_AEAD_nonce_length(aead);
  const size_t overhead_len = EVP_AEAD_max_overhead(aead);

  std::vector<uint8_t> key(key_len);
  std::vector<uint8_t> nonce(nonce_len);
  std::vector<uint8_t> in_storage(input_len + kAlignment);
  // N.B. for EVP_AEAD_CTX_sealv the input and output buffers may be the
  // same size. However, in the direction == evp_aead_open case we copy the tag
  // to the end of the output to use the non-gathering openv (and to minimize
  // total chunk count), hence we add overhead_len to the size of this buffer.
  std::vector<uint8_t> out_storage(input_len + overhead_len + kAlignment);
  std::vector<uint8_t> in2_storage(input_len + overhead_len + kAlignment);
  std::vector<uint8_t> ad(ad_len);
  std::vector<uint8_t> tag_storage(overhead_len + kAlignment);

  uint8_t *in =
      static_cast<uint8_t *>(align_pointer(in_storage.data(), kAlignment));
  uint8_t *out =
      static_cast<uint8_t *>(align_pointer(out_storage.data(), kAlignment));
  uint8_t *tag =
      static_cast<uint8_t *>(align_pointer(tag_storage.data(), kAlignment));
  uint8_t *in2 =
      static_cast<uint8_t *>(align_pointer(in2_storage.data(), kAlignment));

  if (!EVP_AEAD_CTX_init_with_direction(ctx.get(), aead, key.data(), key_len,
                                        EVP_AEAD_DEFAULT_TAG_LENGTH,
                                        evp_aead_seal)) {
    state.SkipWithError("Failed to create EVP_AEAD_CTX.");
    return;
  }

  // If "first" is unaligned, unalign all pointers by one. As kAlignment and
  // not just kAlignment-1 was added in the allocation, this always fits.
  if (iovec_first % kAlignment) {
    ++in;
    ++out;
    ++tag;
    ++in2;
  }

  std::vector<CRYPTO_IOVEC> inout_vec;
  std::vector<CRYPTO_IOVEC> outin2_vec;
  for (size_t pos = 0; pos < input_len;) {
    size_t can = (pos == 0) ? iovec_first : iovec_others;
    size_t have = input_len - pos;
    size_t take = std::min(can, have);
    inout_vec.push_back(CRYPTO_IOVEC{out + pos, in + pos, take});
    outin2_vec.push_back(CRYPTO_IOVEC{in2 + pos, out + pos, take});
    pos += take;
  }
  CRYPTO_IVEC ad_vec = {ad.data(), ad_len};

  if (direction == evp_aead_seal) {
    size_t tag_len;
    for (auto _ : state) {
      benchmark::DoNotOptimize(inout_vec);
      benchmark::DoNotOptimize(nonce.data());
      benchmark::DoNotOptimize(ad_vec);
      if (!EVP_AEAD_CTX_sealv(ctx.get(), inout_vec.data(), inout_vec.size(),
                              tag, &tag_len, overhead_len, nonce.data(),
                              nonce_len, &ad_vec, 1)) {
        state.SkipWithError("EVP_AEAD_CTX_sealv failed.");
        return;
      }
      benchmark::DoNotOptimize(out);
      benchmark::DoNotOptimize(tag);
      benchmark::DoNotOptimize(tag_len);
      benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * input_len);
  } else {
    size_t tag_len;
    if (!EVP_AEAD_CTX_sealv(ctx.get(), inout_vec.data(), inout_vec.size(), tag,
                            &tag_len, overhead_len, nonce.data(), nonce_len,
                            &ad_vec, 1)) {
      state.SkipWithError("EVP_AEAD_CTX_sealv failed.");
      return;
    }

    ctx.Reset();
    if (!EVP_AEAD_CTX_init_with_direction(ctx.get(), aead, key.data(), key_len,
                                          EVP_AEAD_DEFAULT_TAG_LENGTH,
                                          evp_aead_open)) {
      state.SkipWithError("Failed to create EVP_AEAD_CTX.");
      return;
    }

    // We know `out` and `in2` have enough space for the tag; so let's append
    // it to the last chunk of `out`.
    OPENSSL_memcpy(inout_vec.back().out + inout_vec.back().len, tag, tag_len);
    outin2_vec.back().len += tag_len;

    size_t in2_len;
    for (auto _ : state) {
      benchmark::DoNotOptimize(outin2_vec);
      benchmark::DoNotOptimize(nonce.data());
      benchmark::DoNotOptimize(ad_vec);
      // N.B. EVP_AEAD_CTX_openv_detached is not implemented for all AEADs.
      if (!EVP_AEAD_CTX_openv(ctx.get(), outin2_vec.data(), outin2_vec.size(),
                              &in2_len, nonce.data(), nonce_len, &ad_vec, 1)) {
        state.SkipWithError("EVP_AEAD_CTX_openv failed.");
        return;
      }
      benchmark::DoNotOptimize(in2);
      benchmark::DoNotOptimize(in2_len);
      benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * input_len);
  }
}

static const int64_t kInputSizes[] = {16, 256, 1350, 8192, 16384};

void SetInputLength(benchmark::Benchmark *bench) {
  bench->ArgNames({"InputSize"});
  auto input_sizes = bench::GetInputSizes(bench);
  for (const int64_t size : input_sizes.empty() ? kInputSizes : input_sizes) {
    bench->Args({size});
  }
}

void SetInputLengthv(benchmark::Benchmark *bench) {
  // No need to distinguish by name - the IOVec related args suffice to
  // distinguish them. This allows later making BM_SpeedAEADv the only one,
  // while renaming it to BM_SpeedAEAD.
  const std::string prefix = "BM_SpeedAEADv/";
  BSSL_CHECK(std::string(bench->GetName()).substr(0, prefix.size()) == prefix);
  bench->Name(std::string("BM_SpeedAEAD/") +
              std::string(bench->GetName()).substr(prefix.size()));

  bench->ArgNames({"InputSize", "IOVecFirst", "IOVecOthers"});
  auto input_sizes = bench::GetInputSizes(bench);
  for (const int64_t size : input_sizes.empty() ? kInputSizes : input_sizes) {
    bench->Args({size, size, 0});  // One shot.
    if (size >= 1456) {
      bench->Args({size, 1456, 1456});  // Nicely aligned.
      bench->Args({size, 1457, 1456});  // As unaligned as it gets.
    }
  }
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_gcm())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_192_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_192_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_192_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_192_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_192_gcm())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_256_gcm, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_256_gcm, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_gcm())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_chacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_chacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_chacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_chacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_chacha20_poly1305())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_xchacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_xchacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_xchacha20_poly1305, kTLSADLen,
                    evp_aead_seal, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_xchacha20_poly1305, kTLSADLen,
                    evp_aead_open, EVP_aead_xchacha20_poly1305())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha1_tls())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_cbc_sha256, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_128_cbc_sha256_tls())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_256_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_aes_256_cbc_sha1_tls())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_gcm_siv())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_256_gcm_siv, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_gcm_siv())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_128_eax())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_256_eax, kTLSADLen, evp_aead_seal,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_256_eax, kTLSADLen, evp_aead_open,
                    EVP_aead_aes_256_eax())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_ccm_bluetooth, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_ccm_bluetooth8, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_bluetooth_8())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_ccm_matter, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ccm_matter())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_128_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_128_ctr_hmac_sha256())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_seal, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_aes_256_ctr_hmac_sha256, kTLSADLen,
                    evp_aead_open, EVP_aead_aes_256_ctr_hmac_sha256())
      ->Apply(SetInputLengthv);

  BENCHMARK_CAPTURE(BM_SpeedAEAD, seal_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEAD, open_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLength);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, seal_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_seal, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLengthv);
  BENCHMARK_CAPTURE(BM_SpeedAEADv, open_des_ede3_cbc_sha1, kLegacyADLen,
                    evp_aead_open, EVP_aead_des_ede3_cbc_sha1_tls())
      ->Apply(SetInputLengthv);
}

}  // namespace
BSSL_NAMESPACE_END
