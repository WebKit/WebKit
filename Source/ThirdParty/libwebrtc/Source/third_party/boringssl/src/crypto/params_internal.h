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

#ifndef OPENSSL_HEADER_CRYPTO_PARAMS_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_PARAMS_INTERNAL_H

#include <openssl/params.h>


BSSL_NAMESPACE_BEGIN

// IsEndParam returns whether `param` is a terminating element.
inline bool IsEndParam(const OSSL_PARAM &param) {
  return param.key == nullptr;
}

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_PARAMS_INTERNAL_H
