# Arm Cryptography Features

Arm cryptography features are a bit of a zoo. This table is available as a
reference.

| Feature | Armv8.0 | Armv8.2 | ID_AA64ISAR0_EL1 | Arm C Language Extensions | Arm C Language Extensions (old) | GCC/LLVM targets | GCC/LLVM targets (old) | getauxval | Windows |
|---|---|---|---|---|---|---|---|---|---|
| FEAT_AES | Cryptography Extension | AES functionality | none / AES / AES+PMULL | __ARM_FEATURE_AES | __ARM_FEATURE_CRYPTO | aes | crypto | HWCAP_AES | PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE |
| FEAT_PMULL | Cryptography Extension | AES functionality | none / AES / AES+PMULL | __ARM_FEATURE_AES | __ARM_FEATURE_CRYPTO | aes | crypto | HWCAP_PMULL | PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE |
| FEAT_SHA1 | Cryptography Extension | SHA-1 and SHA-256 functionality | none / SHA1 | __ARM_FEATURE_SHA2 | __ARM_FEATURE_CRYPTO | sha2 | crypto | HWCAP_SHA1 | PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE |
| FEAT_SHA256 | Cryptography Extension | SHA-1 and SHA-256 functionality | none / SHA256 / SHA256+SHA512 | __ARM_FEATURE_SHA2 | __ARM_FEATURE_CRYPTO | sha2 | crypto | HWCAP_SHA256 | PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE |
| FEAT_SHA512 | | SHA-512 instructions | none / SHA256 / SHA256+SHA512 | __ARM_FEATURE_SHA512 | | sha3 | | HWCAP_SHA512 | PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE |
| FEAT_SHA3 | | SHA-3 instructions | none / SHA3 | __ARM_FEATURE_SHA3 | | sha3 | | HWCAP_SHA3 | PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE |

## Footnotes

* The Arm reference manual refers to `FEAT_*` names but they don't appear to actually be used in the ISA anywhere.

* Armv8.0 considered all four initial cryptography features as one extension.

* Armv8.2 now says the original four features can be implemented separately in groups of two. That carried over into various `crypto` features splitting into `aes` and `sha2`. (But note `aes` in this formulation includes `FEAT_PMULL` and `sha2` includes `FEAT_SHA1`.)

* `ID_AA64ISAR0_EL1` is Arm's CPUID equivalent. It is a collection of four-bit fields. They tend to group features together. E.g. in the "AES" field, 0b0000 = none, 0b0001 = AES, 0b0010 = AES+PMULL. This means, for example, it is not possible to meaningfully implement `FEAT_PMULL` without `FEAT_AES`.

* GCC/LLVM targets refer to strings used in places like `-march=armv8-a+aes` or `__attribute__((target("aes")))`.

* `__ARM_FEATURE_SHA2` is not a typo. "SHA2" in ACLE includes SHA-1 but not SHA-512.

* The `sha3` GCC/LLVM target is also not a typo. `sha3` includes SHA-512, despite the preprocessor macros being finer-grained. There is no `sha512` GCC/LLVM target.
