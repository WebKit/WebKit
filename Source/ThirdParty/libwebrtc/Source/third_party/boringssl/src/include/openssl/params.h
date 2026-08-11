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

#ifndef OPENSSL_HEADER_PARAMS_H
#define OPENSSL_HEADER_PARAMS_H

#include <openssl/base.h>   // IWYU pragma: export

#if defined(__cplusplus)
extern "C" {
#endif

// ossl_param_st is a structure for passing arbitrary parameters. It is
// currently never returned from BoringSSL, but is defined here for
// compatibility with OpenSSL.
struct ossl_param_st {
  const char *key;
  uint32_t data_type;
  void *data;
  size_t data_size;
  size_t return_size;
};

// OSSL_PARAM_END is a terminating element in an array of `OSSL_PARAM`s.
#define OSSL_PARAM_END {NULL, 0, NULL, 0, 0}

#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_PARAMS_H
