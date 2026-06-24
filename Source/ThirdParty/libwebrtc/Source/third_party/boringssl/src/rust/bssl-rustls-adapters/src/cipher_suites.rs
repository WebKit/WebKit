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

//! Supported cipher suites

use bssl_crypto::digest;
use rustls::{
    CipherSuite, CipherSuiteCommon, SignatureScheme, Tls12CipherSuite, Tls13CipherSuite,
    crypto::KeyExchangeAlgorithm,
};

use super::HashAlgorithm;
use super::prf::Tls12PrfImpl;

macro_rules! tls12_suites {
    ($($suite:ident {
        $hash:ident,
        $confid:expr,
        $kx:ident,
        $sign:expr,
        $aead:ident
    })*) => {
        $(
            #[doc = concat!("TLS 1.2 cipher suite `", stringify!($suite), "`")]
            pub const $suite: &'static Tls12CipherSuite = &Tls12CipherSuite {
                common: CipherSuiteCommon {
                    suite: CipherSuite::$suite,
                    hash_provider: &HashAlgorithm::<digest::$hash>::new(),
                    confidentiality_limit: $confid,
                },
                prf_provider: &Tls12PrfImpl::<digest::$hash>::new(),
                kx: KeyExchangeAlgorithm::$kx,
                sign: $sign,
                aead_alg: super::aead::$aead,
            };
        )*
    };
    ($($tt:tt)*) => {
        tls12_suites!($($tt),*)
    };
}

tls12_suites! {
    TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 {
        Sha256,
        u64::MAX,
        ECDHE,
        TLS12_ECDSA_SCHEMES,
        TLS12_CHACHA20_POLY1305_AEAD
    }
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 {
        Sha256,
        u64::MAX,
        ECDHE,
        TLS12_RSA_SCHEMES,
        TLS12_CHACHA20_POLY1305_AEAD
    }
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 {
        Sha256,
        1 << 24,
        ECDHE,
        TLS12_RSA_SCHEMES,
        TLS12_AES_128_GCM_AEAD
    }
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 {
        Sha384,
        1 << 24,
        ECDHE,
        TLS12_RSA_SCHEMES,
        TLS12_AES_256_GCM_AEAD
    }
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 {
        Sha256,
        1 << 24,
        ECDHE,
        TLS12_ECDSA_SCHEMES,
        TLS12_AES_128_GCM_AEAD
    }
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 {
        Sha384,
        1 << 24,
        ECDHE,
        TLS12_ECDSA_SCHEMES,
        TLS12_AES_256_GCM_AEAD
    }
}

const TLS12_ECDSA_SCHEMES: &[SignatureScheme] = &[
    SignatureScheme::ED25519,
    SignatureScheme::ECDSA_NISTP384_SHA384,
    SignatureScheme::ECDSA_NISTP256_SHA256,
];

const TLS12_RSA_SCHEMES: &[SignatureScheme] = &[
    SignatureScheme::RSA_PKCS1_SHA256,
    SignatureScheme::RSA_PKCS1_SHA384,
    SignatureScheme::RSA_PKCS1_SHA512,
    SignatureScheme::RSA_PSS_SHA256,
    SignatureScheme::RSA_PSS_SHA384,
    SignatureScheme::RSA_PSS_SHA512,
];

/// Cipher suite TLS13_AES_128_GCM_SHA256
pub const TLS13_AES_128_GCM_SHA256: &'static Tls13CipherSuite = &Tls13CipherSuite {
    common: CipherSuiteCommon {
        suite: CipherSuite::TLS13_AES_128_GCM_SHA256,
        hash_provider: &HashAlgorithm::<digest::Sha256>::new(),
        confidentiality_limit: 1 << 24,
    },
    hkdf_provider: super::prf::TLS13_HKDF_SHA256,
    aead_alg: super::aead::TLS13_AES_128_GCM,
    quic: None,
};

/// Cipher suite TLS13_AES_256_GCM_SHA384
pub const TLS13_AES_256_GCM_SHA384: &'static Tls13CipherSuite = &Tls13CipherSuite {
    common: CipherSuiteCommon {
        suite: CipherSuite::TLS13_AES_256_GCM_SHA384,
        hash_provider: &HashAlgorithm::<digest::Sha384>::new(),
        confidentiality_limit: 1 << 24,
    },
    hkdf_provider: super::prf::TLS13_HKDF_SHA384,
    aead_alg: super::aead::TLS13_AES_256_GCM,
    quic: None,
};

/// Cipher suite TLS13_CHACHA20_POLY1305_SHA256
pub const TLS13_CHACHA20_POLY1305_SHA256: &'static Tls13CipherSuite = &Tls13CipherSuite {
    common: CipherSuiteCommon {
        suite: CipherSuite::TLS13_CHACHA20_POLY1305_SHA256,
        hash_provider: &HashAlgorithm::<digest::Sha256>::new(),
        confidentiality_limit: u64::MAX,
    },
    hkdf_provider: super::prf::TLS13_HKDF_SHA256,
    aead_alg: super::aead::TLS13_CHACHA20_POLY1305,
    quic: None,
};
