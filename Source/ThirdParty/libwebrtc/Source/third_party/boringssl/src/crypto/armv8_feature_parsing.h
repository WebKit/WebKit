#ifndef OPENSSL_CRYPTO_CPU_ARMV8_FEATURE_PARSING_H
#define OPENSSL_CRYPTO_CPU_ARMV8_FEATURE_PARSING_H

#include <openssl/cpu.h>
#include <stdint.h>

#include "internal.h"

#if defined(OPENSSL_AARCH64)

BSSL_NAMESPACE_BEGIN
namespace armcap {

// Common field indices based on ARM architecture specification for
// ID_AA64ISAR0_EL1. These are indices (multiplied by 4 for the bit shift).
// Note: SHA3_FIELD_IDX 8 * 4 = 32 (ID_AA64ISAR0_EL1_SHA3_SHIFT value)
#define ID_AA64ISAR0_AES_FIELD_IDX 1   // Bits [7:4]
#define ID_AA64ISAR0_SHA1_FIELD_IDX 2  // Bits [11:8]
#define ID_AA64ISAR0_SHA2_FIELD_IDX 3  // Bits [15:12]
#define ID_AA64ISAR0_SHA3_FIELD_IDX 8  // Bits [35:32]
#define NBITS_ID_FIELD 4

// Helper function to extract a 4-bit field based on its index.
inline unsigned GetIDField(uint64_t reg, unsigned field_idx) {
  // We mask with 0xf to ensure only the 4 relevant bits are returned.
  return (reg >> (field_idx * NBITS_ID_FIELD)) & 0xf;
}

// The core function that converts the raw ID_AA64ISAR0 register value
// into the OR'd capability flags (ARMV8_AES, ARMV8_SHA3, etc.).
inline uint32_t ParseISAR0Flags(uint64_t isar0) {
  uint32_t armcap = 0;
  // AES and PMULL check
  unsigned aes = GetIDField(isar0, ID_AA64ISAR0_AES_FIELD_IDX);
  if (aes > 0) {
    armcap |= ARMV8_AES;
  }
  if (aes > 1) {
    armcap |= ARMV8_PMULL;
  }
  // SHA1 check
  unsigned sha1 = GetIDField(isar0, ID_AA64ISAR0_SHA1_FIELD_IDX);
  if (sha1 > 0) {
    armcap |= ARMV8_SHA1;
  }
  // SHA256 and SHA512 check
  unsigned sha2 = GetIDField(isar0, ID_AA64ISAR0_SHA2_FIELD_IDX);
  if (sha2 > 0) {
    armcap |= ARMV8_SHA256;
  }
  if (sha2 > 1) {
    armcap |= ARMV8_SHA512;
  }
  // SHA3 (EOR3) check
  unsigned sha3 = GetIDField(isar0, ID_AA64ISAR0_SHA3_FIELD_IDX);
  if (sha3 > 0) {
    armcap |= ARMV8_SHA3;
  }
  return armcap;
}

}  // namespace armcap
BSSL_NAMESPACE_END

#endif  // OPENSSL_AARCH64

#endif  // OPENSSL_CRYPTO_CPU_ARMV8_FEATURE_PARSING_H
