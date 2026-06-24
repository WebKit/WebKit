# References

This document collects links to useful references. It is currently incomplete, but we can gradually add to it.

[TOC]

## Cryptographic Primitives

* [AES (FIPS 197)](https://csrc.nist.gov/pubs/fips/197/final)
* [GCM (SP 800-38D)](https://csrc.nist.gov/pubs/sp/800/38/d/final)
* [ChaCha20-Poly1305 (RFC 8439)](https://www.rfc-editor.org/rfc/rfc8439.html)
* [AES-GCM-SIV (RFC 8452)](https://www.rfc-editor.org/rfc/rfc8452.html)
* [RSA (PKCS #1, RFC 8017)](https://www.rfc-editor.org/rfc/rfc8017.html)
* [X25519 (RFC 7748)](https://www.rfc-editor.org/rfc/rfc7748.html)
* [Ed25519 (RFC 8032)](https://www.rfc-editor.org/rfc/rfc8032.html)
* [ML-KEM (FIPS 203)](https://csrc.nist.gov/pubs/fips/203/final)
* [ML-DSA (FIPS 204)](https://csrc.nist.gov/pubs/fips/204/final)
* [SLH-DSA (FIPS 205)](https://csrc.nist.gov/pubs/fips/205/final)

## ASN.1

* [ASN.1 syntax (X.680)](https://www.itu.int/rec/T-REC-X.680)
* [DER and BER encoding (X.690)](https://www.itu.int/rec/T-REC-X.690)
* [A Warm Welcome to ASN.1](https://letsencrypt.org/docs/a-warm-welcome-to-asn1-and-der/)

## X.509 and Related Formats

* [PKIX (RFC 5280)](https://www.rfc-editor.org/rfc/rfc5280.html)
* [New ASN.1 Modules for PKIX (RFC 5912)](https://www.rfc-editor.org/rfc/rfc5912.html)
* [ITU X.509](https://www.itu.int/rec/t-rec-x.509)
* [Service Identity in TLS (RFC 9525)](https://www.rfc-editor.org/rfc/rfc9525.html)
   * Specifies how to match subject alternative names against reference identifiers
* [PrivateKeyInfo (PKCS #8, RFC 5208)](https://www.rfc-editor.org/rfc/rfc5208.html)
* [PKCS #7 (RFC 2315)](https://www.rfc-editor.org/rfc/rfc2315.html)
   * Called CMS 1.5, but this "CMS" now refers to the new version. PKCS #7 is the predecessor
* [Cryptographic Message Syntax (RFC 5662)](https://www.rfc-editor.org/rfc/rfc5652.html)
* [PKCS #12 (RFC 7292)](https://www.rfc-editor.org/rfc/rfc7292.html)
* [Algorithm Identifiers (RFC 3279)](https://www.rfc-editor.org/rfc/rfc3279.html)
   * AlgorithmIdentifiers for pre-SHA-2 hashes
   * SPKIs for dsa, rsaEncryption, ecPublicKey
   * Corresponding signature algorithms with pre-SHA-2 hashes
* [Additional Algorithm Identifiers for RSA (RFC 4055)](https://www.rfc-editor.org/rfc/rfc4055.html)
   * PKCS#1 v1.5 with SHA-2 in X.509
   * RSA-PSS in X.509
* [Algorithm Identifiers for Ed25519 and X25519 (RFC 8410)](https://www.rfc-editor.org/rfc/rfc8410.html)

## TLS

### Protocol Versions

* [TLS 1.3 (RFC 8446)](https://www.rfc-editor.org/rfc/rfc8446)
* [TLS 1.2 (RFC 5246)](https://www.rfc-editor.org/rfc/rfc5246)
* [TLS 1.1 (RFC 4346)](https://www.rfc-editor.org/rfc/rfc4346)
* [TLS 1.0 (RFC 2246)](https://www.rfc-editor.org/rfc/rfc2246)
* [DTLS 1.3 (RFC 9147)](https://www.rfc-editor.org/rfc/rfc9147.html)
* [DTLS 1.2 (RFC 6347)](https://www.rfc-editor.org/rfc/rfc6347)
* [DTLS 1.0 (RFC 4347)](https://www.rfc-editor.org/rfc/rfc4347)

### Cipher Suites and Extensions

* [ECC for TLS 1.2 (RFC 8422)](https://www.rfc-editor.org/rfc/rfc8422.html)
* [AES-GCM for TLS 1.2 (RFC 5288)](https://www.rfc-editor.org/rfc/rfc5288.html)
* [Various Extensions (RFC 6066)](https://www.rfc-editor.org/rfc/rfc6066.html)
  * `server_name` (aka SNI)
  * `certificate_status` (aka OCSP stapling)
* [EMS Extension (RFC 7627)](https://www.rfc-editor.org/rfc/rfc7627.html)
* [Renegotiation Indication Extension (RFC 5746)](https://www.rfc-editor.org/rfc/rfc5746.html)

## Languages and Toolchains

* [cppreference.com](https://cppreference.com/)
* [Draft C++ Standard](https://eel.is/c++draft/)
* [GNU Assembler Manual](https://sourceware.org/binutils/docs/as/)
* [GNU Linker Manual](https://sourceware.org/binutils/docs/ld/)
* [MASM](https://learn.microsoft.com/en-us/cpp/assembler/masm/microsoft-macro-assembler-reference)
* [NASM](https://www.nasm.us/docs/3.01/)

## CPU Architectures

* [Intel Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
* [Arm Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest/)
* [A64 Instruction Set Architecture Guide](https://developer.arm.com/documentation/102374/0103)
  * Overview of registers, generally how the architecture works
* [A64 Instruction Set Architecture](https://developer.arm.com/documentation/ddi0602/2025-12)
   * Information on each instruction

## Calling Conventions and ABIs

### x86

* [ELF x86](https://gitlab.com/x86-psABIs/i386-ABI/)
* [Windows x86](https://docs.microsoft.com/en-us/cpp/cpp/argument-passing-and-naming-conventions)
* [ELF x86-64](https://gitlab.com/x86-psABIs/x86-64-ABI)
* [Windows x86-64](https://docs.microsoft.com/en-us/cpp/build/x64-software-conventions)

### Arm

* [Arm base ABIs (AAPCS32 and AAPCS64)](https://github.com/ARM-software/abi-aa)
* [Linux 32-bit ARM supplement](https://web.archive.org/web/20190531122831/https://sourcery.mentor.com/sgpp/lite/arm/portal/kbattach142/arm_gnu_linux_%20abi.pdf)
* [iOS ARMv6](https://developer.apple.com/documentation/xcode/writing-armv6-code-for-ios)
* [iOS ARMv7](https://developer.apple.com/documentation/xcode/writing-armv7-code-for-ios)
* [Apple ARM64](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
* [Windows ARM64](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions)

## Implementation Techniques

* [BearSSL - Constant-Time Crypto](https://www.bearssl.org/constanttime.html)
* [Analyzing and Comparing Montgomery Multiplication Algorithms](https://www.microsoft.com/en-us/research/wp-content/uploads/1996/01/j37acmon.pdf)
* [Scalar-multiplication algorithms](https://cryptojedi.org/peter/data/eccss-20130911b.pdf)
