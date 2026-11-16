#pragma once

#include <sframe/result.h>
#include <sframe/sframe.h>

namespace SFRAME_NAMESPACE {

Result<size_t>
cipher_digest_size(CipherSuite suite);
Result<size_t>
cipher_key_size(CipherSuite suite);
Result<size_t>
cipher_enc_key_size(CipherSuite suite);
Result<size_t>
cipher_nonce_size(CipherSuite suite);
Result<size_t>
cipher_overhead(CipherSuite suite);
Result<int>
checked_int(size_t size);
Result<void>
validate_ctr_size(size_t size);
void
clear_openssl_errors();

///
/// HKDF
///

static constexpr size_t max_hkdf_extract_size = 64;
static constexpr size_t max_hkdf_expand_size = 96;

Result<owned_bytes<max_hkdf_extract_size>>
hkdf_extract(CipherSuite suite, input_bytes salt, input_bytes ikm);

Result<owned_bytes<max_hkdf_expand_size>>
hkdf_expand(CipherSuite suite, input_bytes prk, input_bytes info, size_t size);

///
/// AEAD Algorithms
///

Result<output_bytes>
seal(CipherSuite suite,
     input_bytes key,
     input_bytes nonce,
     output_bytes ct,
     input_bytes aad,
     input_bytes pt);

Result<output_bytes>
open(CipherSuite suite,
     input_bytes key,
     input_bytes nonce,
     output_bytes pt,
     input_bytes aad,
     input_bytes ct);

} // namespace SFRAME_NAMESPACE
