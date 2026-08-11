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

#![deny(
    missing_docs,
    unsafe_op_in_unsafe_fn,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used
)]
#![allow(private_bounds)]

//! BoringSSL-backed [`rustls`] adapters.
//!
//! This crate provides a [`rustls::crypto::CryptoProvider`] backed by
//! BoringSSL, for use with the [`rustls`] TLS stack.
//! The resulting provider can be used with [`rustls`] to establish TLS 1.2 and TLS 1.3
//! connections.
//!
//! ```
//! use std::sync::Arc;
//! use bssl_rustls_adapters::CryptoProviderBuilder;
//! use rustls::client::ClientConfig;
//!
//! let provider = CryptoProviderBuilder::full();
//! let _config = ClientConfig::builder_with_provider(Arc::new(provider))
//!     .with_safe_default_protocol_versions()
//!     .unwrap()
//!     .with_root_certificates(rustls::RootCertStore::empty())
//!     .with_no_client_auth();
//! ```
//!
//! For finer-grained control, individual cipher suites and key exchange
//! groups can be selected:
//!
//! ```
//! use std::sync::Arc;
//! use bssl_rustls_adapters::{CryptoProviderBuilder, cipher_suites, key_exchange};
//! use rustls::{SupportedCipherSuite, client::ClientConfig};
//!
//! let provider = CryptoProviderBuilder::new()
//!     .with_key_exchange_group(key_exchange::X25519)
//!     .with_cipher_suite(SupportedCipherSuite::Tls13(
//!         cipher_suites::TLS13_AES_256_GCM_SHA384,
//!     ))
//!     .build();
//! let _config = ClientConfig::builder_with_provider(Arc::new(provider))
//!     .with_safe_default_protocol_versions()
//!     .unwrap()
//!     .with_root_certificates(rustls::RootCertStore::empty())
//!     .with_no_client_auth();
//! ```

extern crate alloc;
extern crate core;

use alloc::{boxed::Box, sync::Arc, vec, vec::Vec};
use bssl_sys::RAND_bytes;
use core::{
    fmt::{Debug, Formatter, Result as FmtResult},
    marker::PhantomData,
};

use rustls::{
    Error, SignatureScheme, SupportedCipherSuite,
    crypto::{
        self, CryptoProvider, KeyProvider as RustlsKeyProvider, SecureRandom, SupportedKxGroup,
        WebPkiSupportedAlgorithms,
    },
    pki_types::PrivateKeyDer,
    sign::SigningKey,
};

use bssl_crypto::{FfiMutSlice, digest, ec, pkcs8};

mod aead;
pub mod cipher_suites;
pub mod key_exchange;
pub mod pki;
mod prf;
mod sign;

// A sealed trait only for supported digests
pub(crate) trait RsaSignatureDigest: digest::Algorithm {
    /// A description of the digest algorithm.
    const ALGORITHM: pki::DigestAlgorithm;
}

impl RsaSignatureDigest for digest::Sha256 {
    const ALGORITHM: pki::DigestAlgorithm = pki::DigestAlgorithm::Sha256;
}

impl RsaSignatureDigest for digest::Sha384 {
    const ALGORITHM: pki::DigestAlgorithm = pki::DigestAlgorithm::Sha384;
}

impl RsaSignatureDigest for digest::Sha512 {
    const ALGORITHM: pki::DigestAlgorithm = pki::DigestAlgorithm::Sha512;
}

/// This random source delegates to [`RAND_bytes`] which aborts on insufficient
/// entropy.
struct Rand;

impl Debug for Rand {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("Rand").finish()
    }
}

impl SecureRandom for Rand {
    fn fill(&self, buf: &mut [u8]) -> Result<(), crypto::GetRandomFailed> {
        // Safety: `RAND_bytes` guarantees that the writes are in-bound
        unsafe {
            RAND_bytes(buf.as_mut_ffi_ptr(), buf.len());
        }
        Ok(())
    }
}

const ALL_SIGNATURE_ALGORITHMS: WebPkiSupportedAlgorithms = WebPkiSupportedAlgorithms {
    all: &[
        pki::ECDSA_NISTP256_SHA256,
        pki::ECDSA_NISTP384_SHA384,
        pki::ED25519,
        pki::RSA_PKCS1_SHA256,
        pki::RSA_PKCS1_SHA384,
        pki::RSA_PKCS1_SHA512,
        pki::RSA_PSS_SHA256,
        pki::RSA_PSS_SHA384,
        pki::RSA_PSS_SHA512,
    ],
    mapping: &[
        (
            SignatureScheme::ECDSA_NISTP256_SHA256,
            &[pki::ECDSA_NISTP256_SHA256],
        ),
        (
            SignatureScheme::ECDSA_NISTP384_SHA384,
            &[pki::ECDSA_NISTP384_SHA384],
        ),
        (SignatureScheme::ED25519, &[pki::ED25519]),
        (SignatureScheme::RSA_PKCS1_SHA256, &[pki::RSA_PKCS1_SHA256]),
        (SignatureScheme::RSA_PKCS1_SHA384, &[pki::RSA_PKCS1_SHA384]),
        (SignatureScheme::RSA_PKCS1_SHA512, &[pki::RSA_PKCS1_SHA512]),
        (SignatureScheme::RSA_PSS_SHA256, &[pki::RSA_PSS_SHA256]),
        (SignatureScheme::RSA_PSS_SHA384, &[pki::RSA_PSS_SHA384]),
        (SignatureScheme::RSA_PSS_SHA512, &[pki::RSA_PSS_SHA512]),
    ],
};

struct KeyProvider;

impl Debug for KeyProvider {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("FipsKeyProvider").finish()
    }
}

impl RustlsKeyProvider for KeyProvider {
    fn load_private_key(
        &self,
        key_der: PrivateKeyDer<'static>,
    ) -> Result<Arc<dyn SigningKey>, Error> {
        match key_der {
            PrivateKeyDer::Pkcs1(der) => {
                sign::RsaPrivateKey::try_from(der).map(|key| Arc::new(key) as _)
            }
            PrivateKeyDer::Sec1(ref der) => sign::EcdsaPrivateKey::<ec::P256>::try_from(der)
                .map(|key| Arc::new(key) as _)
                .or_else(|_| {
                    sign::EcdsaPrivateKey::<ec::P384>::try_from(der).map(|key| Arc::new(key) as _)
                }),
            PrivateKeyDer::Pkcs8(ref der) => {
                pkcs8::SigningKey::from_der_private_key_info(der.secret_pkcs8_der())
                    .map(|key| match key {
                        pkcs8::SigningKey::Rsa(rsa) => Arc::new(sign::RsaPrivateKey(rsa)) as _,
                        pkcs8::SigningKey::EcP256(key) => Arc::new(sign::EcdsaPrivateKey(key)) as _,
                        pkcs8::SigningKey::EcP384(key) => Arc::new(sign::EcdsaPrivateKey(key)) as _,
                        pkcs8::SigningKey::Ed25519(key) => {
                            Arc::new(sign::EddsaPrivateKey(key)) as _
                        }
                    })
                    .ok_or(Error::General("unsupported PKCS #8 private key".into()))
            }
            _ => Err(Error::General("type of key to load is unrecognised".into())),
        }
    }

    fn fips(&self) -> bool {
        false
    }
}

#[derive(Clone)]
struct HashContext<A>(A);
impl<A> Debug for HashContext<A> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("HashContext").finish()
    }
}

impl<A: digest::Algorithm + Send + Sync + Clone + 'static> crypto::hash::Context
    for HashContext<A>
{
    fn fork_finish(&self) -> crypto::hash::Output {
        let digest = self.0.clone().digest_to_vec();
        assert!(digest.len() <= crypto::hash::Output::MAX_LEN);
        crypto::hash::Output::new(&digest)
    }

    fn fork(&self) -> Box<dyn crypto::hash::Context> {
        Box::new(self.clone())
    }

    fn finish(self: Box<Self>) -> crypto::hash::Output {
        let digest = self.0.digest_to_vec();
        assert!(digest.len() <= crypto::hash::Output::MAX_LEN);
        crypto::hash::Output::new(&digest)
    }

    fn update(&mut self, data: &[u8]) {
        self.0.update(data);
    }
}

struct HashAlgorithm<A>(PhantomData<fn() -> A>);

impl<A> HashAlgorithm<A> {
    const fn new() -> Self {
        Self(PhantomData)
    }
}

macro_rules! impl_crypto_hash {
    ($algo:path, $id:path) => {
        impl crypto::hash::Hash for HashAlgorithm<$algo> {
            fn start(&self) -> Box<dyn crypto::hash::Context> {
                Box::new(HashContext(<$algo as digest::Algorithm>::new()))
            }

            fn hash(&self, data: &[u8]) -> crypto::hash::Output {
                let mut ctx = <$algo as digest::Algorithm>::new();
                ctx.update(data);
                let digest = digest::Algorithm::digest_to_vec(ctx);
                assert!(digest.len() <= crypto::hash::Output::MAX_LEN);
                crypto::hash::Output::new(&digest)
            }

            fn output_len(&self) -> usize {
                <$algo as digest::Algorithm>::OUTPUT_LEN
            }

            fn algorithm(&self) -> crypto::hash::HashAlgorithm {
                $id
            }
        }
    };
}

impl_crypto_hash!(digest::Sha256, crypto::hash::HashAlgorithm::SHA256);
impl_crypto_hash!(digest::Sha384, crypto::hash::HashAlgorithm::SHA384);
impl_crypto_hash!(digest::Sha512, crypto::hash::HashAlgorithm::SHA512);

/// The main provider builder.
pub struct CryptoProviderBuilder {
    kx_groups: Vec<&'static dyn SupportedKxGroup>,
    cipher_suites: Vec<SupportedCipherSuite>,
}

impl CryptoProviderBuilder {
    /// Make a new provider builder.
    pub fn new() -> Self {
        Self {
            kx_groups: vec![],
            cipher_suites: vec![],
        }
    }

    /// Include all possible cipher suites and key agreement groups.
    pub fn full() -> CryptoProvider {
        Self::new()
            .with_default_key_exchange_groups()
            .with_full_cipher_suites()
            .build()
    }

    /// Include a key exchange group, with a lower priority than previously registered groups.
    pub fn with_key_exchange_group(mut self, group: &'static dyn SupportedKxGroup) -> Self {
        self.kx_groups.push(group);
        self
    }

    /// Use the default key exchange groups, with a lower priority than previously registered
    /// groups.
    pub fn with_default_key_exchange_groups(mut self) -> Self {
        self.kx_groups.extend_from_slice(&[
            key_exchange::X25519,
            key_exchange::ECDH_P256,
            key_exchange::ECDH_P384,
        ]);
        self
    }

    /// Include post-quantum MLKEM hybrid key exchange groups, with a lower priority than previously
    /// registered groups.
    pub fn with_mlkem_groups(mut self) -> Self {
        self.kx_groups
            .extend_from_slice(&[key_exchange::X25519MLKEM768]);
        self
    }

    /// Use all the provided cipher suites
    #[inline]
    pub fn with_full_cipher_suites(mut self) -> Self {
        self.cipher_suites.extend([
            SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
            ),
            SupportedCipherSuite::Tls12(cipher_suites::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256),
            SupportedCipherSuite::Tls12(cipher_suites::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256),
            SupportedCipherSuite::Tls12(cipher_suites::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384),
            SupportedCipherSuite::Tls12(cipher_suites::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
            SupportedCipherSuite::Tls12(cipher_suites::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384),
            SupportedCipherSuite::Tls13(cipher_suites::TLS13_AES_128_GCM_SHA256),
            SupportedCipherSuite::Tls13(cipher_suites::TLS13_AES_256_GCM_SHA384),
            SupportedCipherSuite::Tls13(cipher_suites::TLS13_CHACHA20_POLY1305_SHA256),
        ]);
        self
    }

    /// Add a [`SupportedCipherSuite`].
    /// More cipher suites are available in [`cipher_suites`].
    #[inline]
    pub fn with_cipher_suite(mut self, cipher_suite: SupportedCipherSuite) -> Self {
        self.cipher_suites.push(cipher_suite);
        self
    }

    #[inline]
    /// Finalise and build the [`CryptoProvider`] for `rustls`.
    pub fn build(self) -> CryptoProvider {
        CryptoProvider {
            cipher_suites: self.cipher_suites,
            kx_groups: self.kx_groups,
            signature_verification_algorithms: ALL_SIGNATURE_ALGORITHMS,
            secure_random: &Rand,
            key_provider: &KeyProvider,
        }
    }
}

#[cfg(test)]
mod tests;
