// Copyright 2014 The BoringSSL Authors
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

// This header is provided in order to make compiling against code that expects
// OpenSSL easier.

#ifndef OPENSSL_HEADER_OPENSSLCONF_H
#define OPENSSL_HEADER_OPENSSLCONF_H

// Keep in sync with the list in rust/bssl-sys/build.rs.

#define OPENSSL_NO_ASYNC
#define OPENSSL_NO_BF
#define OPENSSL_NO_BLAKE2
#define OPENSSL_NO_BUF_FREELISTS
#define OPENSSL_NO_CAMELLIA
#define OPENSSL_NO_CAPIENG
#define OPENSSL_NO_CAST
#define OPENSSL_NO_COMP
#define OPENSSL_NO_CT
#define OPENSSL_NO_DANE
#define OPENSSL_NO_DEPRECATED
#define OPENSSL_NO_DGRAM
#define OPENSSL_NO_DYNAMIC_ENGINE
#define OPENSSL_NO_EC_NISTP_64_GCC_128
#define OPENSSL_NO_EC2M
#define OPENSSL_NO_EGD
#define OPENSSL_NO_ENGINE
#define OPENSSL_NO_GMP
#define OPENSSL_NO_GOST
#define OPENSSL_NO_HEARTBEATS
#define OPENSSL_NO_HW
#define OPENSSL_NO_IDEA
#define OPENSSL_NO_JPAKE
#define OPENSSL_NO_KRB5
#define OPENSSL_NO_MD2
#define OPENSSL_NO_MDC2
#define OPENSSL_NO_OCB
#define OPENSSL_NO_OCSP
#define OPENSSL_NO_RC2
#define OPENSSL_NO_RC5
#define OPENSSL_NO_RFC3779
#define OPENSSL_NO_RIPEMD
#define OPENSSL_NO_RMD160
#define OPENSSL_NO_SCTP
#define OPENSSL_NO_SEED
#define OPENSSL_NO_SM2
#define OPENSSL_NO_SM3
#define OPENSSL_NO_SM4
#define OPENSSL_NO_SRP
#define OPENSSL_NO_SSL_TRACE
#define OPENSSL_NO_SSL2
#define OPENSSL_NO_SSL3
#define OPENSSL_NO_SSL3_METHOD
#define OPENSSL_NO_STATIC_ENGINE
#define OPENSSL_NO_STORE
#define OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_WHIRLPOOL

// We do not implement OpenSSL's CMS API, except for a tiny subset. Projects
// targeting the tiny subset can define BORINGSSL_NO_NO_CMS to suppress
// OPENSSL_NO_CMS, to make it easier to compile code that expects OpenSSL. This
// option does not change what APIs are exposed by BoringSSL, only this macro.
#if !defined(BORINGSSL_NO_NO_CMS)
#define OPENSSL_NO_CMS
#endif

// C and C++ handle inline functions differently. In C++, an inline function is
// defined in just the header file, potentially emitted in multiple compilation
// units (in cases the compiler did not inline), but each copy must be identical
// to satisfy ODR. In C, a non-static inline must be manually emitted in exactly
// one compilation unit with a separate extern inline declaration.
//
// In both languages, exported inline functions referencing file-local symbols
// are problematic. C forbids this altogether (though GCC and Clang seem not to
// enforce it). It works in C++, but ODR requires the definitions be identical,
// including all names in the definitions resolving to the "same entity". In
// practice, this is unlikely to be a problem, but an inline function that
// returns a pointer to a file-local symbol
// could compile oddly.
//
// Historically, we used static inline in headers. However, to satisfy ODR, use
// plain inline in C++, to allow inline consumer functions to call our header
// functions. Plain inline would also work better with C99 inline, but that is
// not used much in practice, extern inline is tedious, and there are conflicts
// with the old gnu89 model:
// https://stackoverflow.com/questions/216510/extern-inline
//
// So in C, always use static inline, whereas in C++, use static inline
// only if `BORINGSSL_ALWAYS_USE_STATIC_INLINE` is defined (which may be useful
// for some FFI integrations in conjunction with `BORINGSSL_PREFIX`, as static
// inline functions are local to the compilation unit and need no prefix).
#if !defined(__cplusplus) && !defined(BORINGSSL_ALWAYS_USE_STATIC_INLINE)
#define BORINGSSL_ALWAYS_USE_STATIC_INLINE
#endif

#endif  // OPENSSL_HEADER_OPENSSLCONF_H
