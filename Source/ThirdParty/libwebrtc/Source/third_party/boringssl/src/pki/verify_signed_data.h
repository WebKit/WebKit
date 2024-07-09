// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BSSL_PKI_VERIFY_SIGNED_DATA_H_
#define BSSL_PKI_VERIFY_SIGNED_DATA_H_

#include <openssl/base.h>
#include <openssl/evp.h>
#include <openssl/pki/signature_verify_cache.h>

#include "signature_algorithm.h"

namespace bssl {

namespace der {
class BitString;
class Input;
}  // namespace der

// Verifies that |signature_value| is a valid signature of |signed_data| using
// the algorithm |algorithm| and the public key |public_key|.
//
//   |algorithm| - The parsed AlgorithmIdentifier
//   |signed_data| - The blob of data to verify
//   |signature_value| - The BIT STRING for the signature's value
//   |public_key| - The parsed (non-null) public key.
//
// Returns true if verification was successful.
[[nodiscard]] OPENSSL_EXPORT bool VerifySignedData(
    SignatureAlgorithm algorithm, der::Input signed_data,
    const der::BitString &signature_value, EVP_PKEY *public_key,
    SignatureVerifyCache *cache);

// Same as above overload, only the public key is inputted as an SPKI and will
// be parsed internally.
[[nodiscard]] OPENSSL_EXPORT bool VerifySignedData(
    SignatureAlgorithm algorithm, der::Input signed_data,
    const der::BitString &signature_value, der::Input public_key_spki,
    SignatureVerifyCache *cache);

[[nodiscard]] OPENSSL_EXPORT bool ParsePublicKey(
    der::Input public_key_spki, bssl::UniquePtr<EVP_PKEY> *public_key);

}  // namespace bssl

#endif  // BSSL_PKI_VERIFY_SIGNED_DATA_H_
