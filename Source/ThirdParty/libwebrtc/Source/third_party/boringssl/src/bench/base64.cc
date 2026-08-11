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

#include <string_view>

#include <benchmark/benchmark.h>

#include <openssl/base.h>
#include <openssl/base64.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "./internal.h"

namespace {
const char kInput[] =
    "MIIDtTCCAp2gAwIBAgIJALW2IrlaBKUhMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV"
    "BAYTAkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMSEwHwYDVQQKExhJbnRlcm5ldCBX"
    "aWRnaXRzIFB0eSBMdGQwHhcNMTYwNzA5MDQzODA5WhcNMTYwODA4MDQzODA5WjBF"
    "MQswCQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50"
    "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB"
    "CgKCAQEAugvahBkSAUF1fC49vb1bvlPrcl80kop1iLpiuYoz4Qptwy57+EWssZBc"
    "HprZ5BkWf6PeGZ7F5AX1PyJbGHZLqvMCvViP6pd4MFox/igESISEHEixoiXCzepB"
    "rhtp5UQSjHD4D4hKtgdMgVxX+LRtwgW3mnu/vBu7rzpr/DS8io99p3lqZ1Aky+aN"
    "lcMj6MYy8U+YFEevb/V0lRY9oqwmW7BHnXikm/vi6sjIS350U8zb/mRzYeIs2R65"
    "LUduTL50+UMgat9ocewI2dv8aO9Dph+8NdGtg8LFYyTTHcUxJoMr1PTOgnmET19W"
    "JH4PrFwk7ZE1QJQQ1L4iKmPeQistuQIDAQABo4GnMIGkMB0GA1UdDgQWBBT5m6Vv"
    "zYjVYHG30iBE+j2XDhUE8jB1BgNVHSMEbjBsgBT5m6VvzYjVYHG30iBE+j2XDhUE"
    "8qFJpEcwRTELMAkGA1UEBhMCQVUxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNV"
    "BAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZIIJALW2IrlaBKUhMAwGA1UdEwQF"
    "MAMBAf8wDQYJKoZIhvcNAQELBQADggEBAD7Jg68SArYWlcoHfZAB90Pmyrt5H6D8"
    "LRi+W2Ri1fBNxREELnezWJ2scjl4UMcsKYp4Pi950gVN+62IgrImcCNvtb5I1Cfy"
    "/MNNur9ffas6X334D0hYVIQTePyFk3umI+2mJQrtZZyMPIKSY/sYGQHhGGX6wGK+"
    "GO/og0PQk/Vu6D+GU2XRnDV0YZg1lsAsHd21XryK6fDmNkEMwbIWrts4xc7scRrG"
    "HWy+iMf6/7p/Ak/SIicM4XSwmlQ8pPxAZPr+E2LoVd9pMpWUwpW2UbtO5wsGTrY5"
    "sO45tFNN/y+jtUheB1C2ijObG/tXELaiyCdM+S/waeuv0MXtI4xnn1A=";

void BM_SpeedBase64(benchmark::State &state) {
  uint8_t out[sizeof(kInput)];
  size_t len;
  const std::string_view input = kInput;
  for (auto _ : state) {
    if (!EVP_DecodeBase64(out, &len, sizeof(out),
                          reinterpret_cast<const uint8_t *>(input.data()),
                          input.size())) {
      state.SkipWithError("base64 decode failed.");
      return;
    }
  }
  state.SetBytesProcessed(state.iterations() * input.size());
}

BSSL_BENCH_LAZY_REGISTER() {
  BENCHMARK(BM_SpeedBase64)->Apply(bssl::bench::SetThreads);
}

}  // namespace
