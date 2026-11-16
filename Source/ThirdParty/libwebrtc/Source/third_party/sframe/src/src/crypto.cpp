#include "crypto.h"
#include "header.h"

#include <openssl/err.h>

#include <climits>

namespace SFRAME_NAMESPACE {

Result<size_t>
cipher_digest_size(CipherSuite suite)
{
  switch (suite) {
    case CipherSuite::AES_128_CTR_HMAC_SHA256_80:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_64:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_32:
    case CipherSuite::AES_GCM_128_SHA256:
      return 32;

    case CipherSuite::AES_GCM_256_SHA512:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_80:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_64:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_32:
      return 64;

    default:
      return SFrameErrorType::unsupported_ciphersuite_error;
  }
}

Result<size_t>
cipher_key_size(CipherSuite suite)
{
  switch (suite) {
    case CipherSuite::AES_128_CTR_HMAC_SHA256_80:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_64:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_32:
      // 16-byte AES key + 32-byte HMAC key
      return 48;

    case CipherSuite::AES_256_CTR_HMAC_SHA512_80:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_64:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_32:
      // 32-byte AES key + 64-byte HMAC key
      return 96;

    case CipherSuite::AES_GCM_128_SHA256:
      return 16;

    case CipherSuite::AES_GCM_256_SHA512:
      return 32;

    default:
      return SFrameErrorType::unsupported_ciphersuite_error;
  }
}

Result<size_t>
cipher_enc_key_size(CipherSuite suite)
{
  switch (suite) {
    case CipherSuite::AES_128_CTR_HMAC_SHA256_80:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_64:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_32:
      return 16;

    case CipherSuite::AES_256_CTR_HMAC_SHA512_80:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_64:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_32:
      return 32;

    default:
      return SFrameErrorType::unsupported_ciphersuite_error;
  }
}

Result<size_t>
cipher_nonce_size(CipherSuite suite)
{
  switch (suite) {
    case CipherSuite::AES_128_CTR_HMAC_SHA256_80:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_64:
    case CipherSuite::AES_128_CTR_HMAC_SHA256_32:
    case CipherSuite::AES_GCM_128_SHA256:
    case CipherSuite::AES_GCM_256_SHA512:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_80:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_64:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_32:
      return 12;

    default:
      return SFrameErrorType::unsupported_ciphersuite_error;
  }
}

Result<size_t>
cipher_overhead(CipherSuite suite)
{
  switch (suite) {
    case CipherSuite::AES_128_CTR_HMAC_SHA256_80:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_80:
      return 10; // 80-bit tag

    case CipherSuite::AES_128_CTR_HMAC_SHA256_64:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_64:
      return 8; // 64-bit tag

    case CipherSuite::AES_128_CTR_HMAC_SHA256_32:
    case CipherSuite::AES_256_CTR_HMAC_SHA512_32:
      return 4; // 32-bit tag

    case CipherSuite::AES_GCM_128_SHA256:
    case CipherSuite::AES_GCM_256_SHA512:
      return 16; // Full 128-bit AES-GCM tag

    default:
      return SFrameErrorType::unsupported_ciphersuite_error;
  }
}

Result<int>
checked_int(size_t size)
{
  if (size > INT_MAX) {
    return SFrameError(SFrameErrorType::invalid_parameter_error,
                       "Input too large for OpenSSL");
  }

  return static_cast<int>(size);
}

Result<void>
validate_ctr_size(size_t size)
{
  static constexpr uint64_t max_ctr_size = uint64_t(1) << 36;
  if (uint64_t(size) > max_ctr_size) {
    return SFrameError(SFrameErrorType::invalid_parameter_error,
                       "CTR input too large");
  }

  auto size_int = checked_int(size);
  if (size_int.is_err()) {
    return size_int.error();
  }

  return Result<void>::ok();
}

void
clear_openssl_errors()
{
  ERR_clear_error();
}

} // namespace SFRAME_NAMESPACE
