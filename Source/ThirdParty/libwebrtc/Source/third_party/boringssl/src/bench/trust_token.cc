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

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/curve25519.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/trust_token.h>

#include "../crypto/mem_internal.h"

namespace {

const uint8_t kClientData[] = "\x70TEST CLIENT DATA";
const uint64_t kRedemptionTime = 13374242;

enum class TrustTokenStage {
  kBeginIssue,
  kIssue,
  kFinishIssue,
  kBeginRedeem,
  kRedeem,
};

// NOTE: Fixture is not thread-safe.
class TrustTokenFixture : public benchmark::Fixture {
 public:
  TrustTokenFixture() = default;

 protected:
  bool PrepareClientAndIssuer(const TRUST_TOKEN_METHOD *method,
                              size_t batchsize, benchmark::State &state) {
    method_ = method;
    batchsize_ = batchsize;
    client_.reset(TRUST_TOKEN_CLIENT_new(method_, batchsize_));
    issuer_.reset(TRUST_TOKEN_ISSUER_new(method_, batchsize_));
    size_t priv_key_len, pub_key_len;
    if (!client_ || !issuer_ ||
        !TRUST_TOKEN_generate_key(
            method_, priv_key_, &priv_key_len, TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE,
            pub_key_, &pub_key_len, TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE, 0) ||
        !TRUST_TOKEN_CLIENT_add_key(client_.get(), &key_index_, pub_key_,
                                    pub_key_len) ||
        !TRUST_TOKEN_ISSUER_add_key(issuer_.get(), priv_key_, priv_key_len)) {
      state.SkipWithError("failed to generate trust token key.");
      return false;
    }

    uint8_t public_key[32], private_key[64];
    ED25519_keypair(public_key, private_key);
    priv_.reset(
        EVP_PKEY_from_raw_private_key(EVP_pkey_ed25519(), private_key, 32));
    pub_.reset(
        EVP_PKEY_from_raw_public_key(EVP_pkey_ed25519(), public_key, 32));
    if (!priv_ || !pub_) {
      state.SkipWithError("failed to generate trust token SRR key.");
      return false;
    }

    TRUST_TOKEN_CLIENT_set_srr_key(client_.get(), pub_.get());
    TRUST_TOKEN_ISSUER_set_srr_key(issuer_.get(), priv_.get());
    return true;
  }

  void SpeedBeginIssue(benchmark::State &state) {
    for (auto _ : state) {
      bssl::UniquePtr<TRUST_TOKEN_CLIENT> copy(
          TRUST_TOKEN_CLIENT_dup_for_testing(client_.get()));
      if (copy == nullptr) {
        state.SkipWithError("TRUST_TOKEN_CLIENT_dup_for_testing failed.");
        return;
      }
      uint8_t *issue_msg = NULL;
      size_t test_msg_len;
      int ok = TRUST_TOKEN_CLIENT_begin_issuance(copy.get(), &issue_msg,
                                                 &test_msg_len, batchsize_);
      OPENSSL_free(issue_msg);
      if (!ok) {
        state.SkipWithError("TRUST_TOKEN_CLIENT_begin_issuance failed.");
        return;
      }
    }
  }
  bool BeginIssue(benchmark::State &state) {
    uint8_t *issue_msg = NULL;
    size_t msg_len = 0;
    if (!TRUST_TOKEN_CLIENT_begin_issuance(client_.get(), &issue_msg, &msg_len,
                                           batchsize_)) {
      state.SkipWithError("TRUST_TOKEN_CLIENT_begin_issuance failed.");
      return false;
    }
    free_issue_msg_.Reset(issue_msg, msg_len);
    return true;
  }
  void SpeedIssue(benchmark::State &state) {
    uint8_t *issue_resp = NULL;
    size_t test_resp_len, tokens_issued;
    for (auto _ : state) {
      int ok = TRUST_TOKEN_ISSUER_issue(
          issuer_.get(), &issue_resp, &test_resp_len, &tokens_issued,
          free_issue_msg_.data(), free_issue_msg_.size(),
          /*public_metadata=*/0,
          /*private_metadata=*/0,
          /*max_issuance=*/batchsize_);
      OPENSSL_free(issue_resp);
      if (!ok) {
        state.SkipWithError("TRUST_TOKEN_ISSUER_issue failed.");
      }
    }
  }
  bool Issue(benchmark::State &state) {
    uint8_t *issue_resp = NULL;
    size_t resp_len = 0;
    size_t tokens_issued;
    if (!TRUST_TOKEN_ISSUER_issue(issuer_.get(), &issue_resp, &resp_len,
                                  &tokens_issued, free_issue_msg_.data(),
                                  free_issue_msg_.size(),
                                  /*public_metadata=*/0, /*private_metadata=*/0,
                                  /*max_issuance=*/batchsize_)) {
      state.SkipWithError("TRUST_TOKEN_ISSUER_issue failed.");
      return false;
    }
    free_issue_resp_.Reset(issue_resp, resp_len);
    return true;
  }
  void SpeedFinishIssue(benchmark::State &state) {
    for (auto _ : state) {
      bssl::UniquePtr<TRUST_TOKEN_CLIENT> copy(
          TRUST_TOKEN_CLIENT_dup_for_testing(client_.get()));
      if (copy == nullptr) {
        state.SkipWithError("TRUST_TOKEN_CLIENT_dup_for_testing failed.");
        return;
      }
      size_t key_index2;
      bssl::UniquePtr<STACK_OF(TRUST_TOKEN)> test_tokens(
          TRUST_TOKEN_CLIENT_finish_issuance(copy.get(), &key_index2,
                                             free_issue_resp_.data(),
                                             free_issue_resp_.size()));
      if (!test_tokens) {
        state.SkipWithError("TRUST_TOKEN_CLIENT_finish_issuance failed.");
        return;
      }
    }
  }
  bool FinishIssue(benchmark::State &state) {
    tokens_.reset(TRUST_TOKEN_CLIENT_finish_issuance(client_.get(), &key_index_,
                                                     free_issue_resp_.data(),
                                                     free_issue_resp_.size()));
    if (!tokens_ || sk_TRUST_TOKEN_num(tokens_.get()) < 1) {
      state.SkipWithError("TRUST_TOKEN_CLIENT_finish_issuance failed.");
      return false;
    }
    return true;
  }
  void SpeedBeginRedeem(benchmark::State &state) {
    const TRUST_TOKEN *token = sk_TRUST_TOKEN_value(tokens_.get(), 0);
    for (auto _ : state) {
      uint8_t *redeem_msg = NULL;
      size_t test_redeem_msg_len;
      int ok = TRUST_TOKEN_CLIENT_begin_redemption(
          client_.get(), &redeem_msg, &test_redeem_msg_len, token, kClientData,
          sizeof(kClientData) - 1, kRedemptionTime);
      OPENSSL_free(redeem_msg);
      if (!ok) {
        state.SkipWithError("TRUST_TOKEN_CLIENT_begin_redemption failed.");
        return;
      }
    }
  }
  bool BeginRedeem(benchmark::State &state) {
    const TRUST_TOKEN *token = sk_TRUST_TOKEN_value(tokens_.get(), 0);
    uint8_t *redeem_msg = NULL;
    size_t redeem_msg_len = 0;
    if (!TRUST_TOKEN_CLIENT_begin_redemption(
            client_.get(), &redeem_msg, &redeem_msg_len, token, kClientData,
            sizeof(kClientData) - 1, kRedemptionTime)) {
      state.SkipWithError("TRUST_TOKEN_CLIENT_begin_redemption failed.");
      return false;
    }
    free_redeem_msg_.Reset(redeem_msg, redeem_msg_len);
    return true;
  }
  void SpeedRedeem(benchmark::State &state) {
    for (auto _ : state) {
      uint32_t public_value;
      uint8_t private_value;
      TRUST_TOKEN *rtoken;
      uint8_t *client_data = NULL;
      size_t client_data_len;
      int ok = TRUST_TOKEN_ISSUER_redeem(
          issuer_.get(), &public_value, &private_value, &rtoken, &client_data,
          &client_data_len, free_redeem_msg_.data(), free_redeem_msg_.size());
      OPENSSL_free(client_data);
      if (!ok) {
        state.SkipWithError("TRUST_TOKEN_ISSUER_redeem failed.");
        return;
      }
      TRUST_TOKEN_free(rtoken);
    }
  }

  void Speed(benchmark::State &state, const TRUST_TOKEN_METHOD *method,
             size_t batchsize, TrustTokenStage stage) {
    if (!PrepareClientAndIssuer(method, batchsize, state)) {
      return;
    }
    if (stage == TrustTokenStage::kBeginIssue) {
      SpeedBeginIssue(state);
      return;
    }
    if (!BeginIssue(state)) {
      return;
    }
    if (stage == TrustTokenStage::kIssue) {
      SpeedIssue(state);
      return;
    }
    if (!Issue(state)) {
      return;
    }
    if (stage == TrustTokenStage::kFinishIssue) {
      SpeedFinishIssue(state);
      return;
    }
    if (!FinishIssue(state)) {
      return;
    }
    if (stage == TrustTokenStage::kBeginRedeem) {
      SpeedBeginRedeem(state);
      return;
    }
    if (!BeginRedeem(state)) {
      return;
    }
    if (stage == TrustTokenStage::kRedeem) {
      SpeedRedeem(state);
      return;
    }
  }

 private:
  size_t batchsize_;
  const TRUST_TOKEN_METHOD *method_;
  bssl::UniquePtr<TRUST_TOKEN_CLIENT> client_;
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_;

  uint8_t priv_key_[TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE];
  uint8_t pub_key_[TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE];

  bssl::UniquePtr<EVP_PKEY> priv_;
  bssl::UniquePtr<EVP_PKEY> pub_;

  bssl::Array<uint8_t> free_issue_msg_;
  bssl::Array<uint8_t> free_issue_resp_;
  size_t key_index_;
  bssl::UniquePtr<STACK_OF(TRUST_TOKEN)> tokens_;
  bssl::Array<uint8_t> free_redeem_msg_;
};

void BM_SpeedTrustTokenKeyGen(benchmark::State &state,
                              const TRUST_TOKEN_METHOD *method) {
  for (auto _ : state) {
    uint8_t priv_key[TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE];
    uint8_t pub_key[TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE];
    size_t priv_key_len, pub_key_len;
    if (!TRUST_TOKEN_generate_key(
            method, priv_key, &priv_key_len, TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE,
            pub_key, &pub_key_len, TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE, 0)) {
      state.SkipWithError("TRUST_TOKEN_generate_key failed.");
      return;
    }
  }
}

BENCHMARK_CAPTURE(BM_SpeedTrustTokenKeyGen, SpeedTrustTokenKeyGen experiment_v1,
                  TRUST_TOKEN_experiment_v1());
BENCHMARK_CAPTURE(BM_SpeedTrustTokenKeyGen,
                  SpeedTrustTokenKeyGen experiment_v2_voprf,
                  TRUST_TOKEN_experiment_v2_voprf());
BENCHMARK_CAPTURE(BM_SpeedTrustTokenKeyGen,
                  SpeedTrustTokenKeyGen experiment_v2_pmb,
                  TRUST_TOKEN_experiment_v2_pmb());

#define SPEED(step, method, batchsize)                                         \
  BENCHMARK_DEFINE_F(TrustTokenFixture,                                        \
                     BM_SpeedTrustToken_##method##step##Batch##batchsize)(     \
      benchmark::State & state) {                                              \
    Speed(state, TRUST_TOKEN_##method(), batchsize, TrustTokenStage::k##step); \
  }                                                                            \
  BENCHMARK_REGISTER_F(TrustTokenFixture,                                      \
                       BM_SpeedTrustToken_##method##step##Batch##batchsize);

SPEED(BeginIssue, experiment_v1, 1)
SPEED(BeginIssue, experiment_v1, 10)
SPEED(Issue, experiment_v1, 1)
SPEED(Issue, experiment_v1, 10)
SPEED(FinishIssue, experiment_v1, 1)
SPEED(FinishIssue, experiment_v1, 10)
SPEED(BeginRedeem, experiment_v1, 1)
SPEED(BeginRedeem, experiment_v1, 10)
SPEED(Redeem, experiment_v1, 1)
SPEED(Redeem, experiment_v1, 10)

SPEED(BeginIssue, experiment_v2_pmb, 1)
SPEED(BeginIssue, experiment_v2_pmb, 10)
SPEED(Issue, experiment_v2_pmb, 1)
SPEED(Issue, experiment_v2_pmb, 10)
SPEED(FinishIssue, experiment_v2_pmb, 1)
SPEED(FinishIssue, experiment_v2_pmb, 10)
SPEED(BeginRedeem, experiment_v2_pmb, 1)
SPEED(BeginRedeem, experiment_v2_pmb, 10)
SPEED(Redeem, experiment_v2_pmb, 1)
SPEED(Redeem, experiment_v2_pmb, 10)

SPEED(BeginIssue, experiment_v2_voprf, 1)
SPEED(BeginIssue, experiment_v2_voprf, 10)
SPEED(Issue, experiment_v2_voprf, 1)
SPEED(Issue, experiment_v2_voprf, 10)
SPEED(FinishIssue, experiment_v2_voprf, 1)
SPEED(FinishIssue, experiment_v2_voprf, 10)
SPEED(BeginRedeem, experiment_v2_voprf, 1)
SPEED(BeginRedeem, experiment_v2_voprf, 10)
SPEED(Redeem, experiment_v2_voprf, 1)
SPEED(Redeem, experiment_v2_voprf, 10)

}  // namespace
