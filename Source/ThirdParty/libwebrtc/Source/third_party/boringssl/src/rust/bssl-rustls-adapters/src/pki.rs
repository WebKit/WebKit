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

//! PKI definitions for TLS

use core::{
    fmt::{Debug, Formatter, Result as FmtResult},
    marker::PhantomData,
};

use bssl_crypto::{digest, ec, ecdsa, ed25519, rsa};
use rustls::pki_types::{
    AlgorithmIdentifier, InvalidSignature, SignatureVerificationAlgorithm, alg_id,
};

use crate::RsaSignatureDigest;

/// A PKI object with assigned Object Identifier
pub(crate) trait PkiIdentified {
    /// The assigned identifier
    const OBJECT_IDENTIFIER: AlgorithmIdentifier;
}

/// PKI Verification algorithm
pub(crate) trait PkiPublicKeyAlgorithm: PkiIdentified {
    /// Public key type that operates based on the identified algorithm.
    type PublicKey;

    /// Decode a signature public key from a `subjectPublicKey` field.
    fn from_der_subject_public_key(spk: &[u8]) -> Option<Self::PublicKey>;
}

/// PKI Verification algorithm
pub(crate) trait PkiSignatureAlgorithm: PkiIdentified {
    /// Supported public key, which can be deserialised from a DER-serialised
    /// `SubjectPublicKeyInfo` field.
    type PublicKeyAlgorithm: PkiPublicKeyAlgorithm;

    /// Perform verification of the signature against the deserialised public key
    /// and the original message.
    fn verify(
        public_key: <Self::PublicKeyAlgorithm as PkiPublicKeyAlgorithm>::PublicKey,
        message: &[u8],
        signature: &[u8],
    ) -> bool;
}

/// PKI verification as prescribed in [RFC 3279](https://datatracker.ietf.org/doc/html/rfc3279)
struct PkiSignatureVerification<S>(PhantomData<fn() -> S>);

impl<S> PkiSignatureVerification<S> {
    pub(crate) const fn new() -> Self {
        Self(PhantomData)
    }
}

impl<S> Debug for PkiSignatureVerification<S> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("PkiVerification").finish()
    }
}

impl<S: PkiSignatureAlgorithm> SignatureVerificationAlgorithm for PkiSignatureVerification<S> {
    fn verify_signature(
        &self,
        subject_public_key: &[u8],
        message: &[u8],
        signature: &[u8],
    ) -> Result<(), InvalidSignature> {
        let public_key = S::PublicKeyAlgorithm::from_der_subject_public_key(subject_public_key)
            .ok_or(InvalidSignature)?;
        if S::verify(public_key, message, signature) {
            Ok(())
        } else {
            Err(InvalidSignature)
        }
    }

    fn public_key_alg_id(&self) -> AlgorithmIdentifier {
        S::PublicKeyAlgorithm::OBJECT_IDENTIFIER
    }

    fn signature_alg_id(&self) -> AlgorithmIdentifier {
        S::OBJECT_IDENTIFIER
    }
}

/// RSA encryption
///
/// OID: 1.2.840.113549.1.1.1
struct PkiRsaPublicKey;

impl PkiIdentified for PkiRsaPublicKey {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_ENCRYPTION;
}

impl PkiPublicKeyAlgorithm for PkiRsaPublicKey {
    type PublicKey = rsa::PublicKey;

    fn from_der_subject_public_key(spk: &[u8]) -> Option<Self::PublicKey> {
        rsa::PublicKey::from_der_rsa_public_key(spk)
    }
}

/// Elliptic curve (EC) public key encoded in X9.62
///
/// OID: 1.2.840.10045.2.1
struct PkiEcPublicKey<C> {
    pub(crate) _p: PhantomData<fn() -> C>,
}

/// EC public key over curve P-256
///
/// OID: 1.2.840.10045.3.1.7
impl PkiIdentified for PkiEcPublicKey<ec::P256> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ECDSA_P256;
}

/// EC public key over curve P-384
///
/// OID: 1.3.132.0.34
impl PkiIdentified for PkiEcPublicKey<ec::P384> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ECDSA_P384;
}

impl<C: ec::Curve> PkiPublicKeyAlgorithm for PkiEcPublicKey<C>
where
    Self: PkiIdentified,
{
    type PublicKey = ecdsa::PublicKey<C>;

    fn from_der_subject_public_key(spk: &[u8]) -> Option<Self::PublicKey> {
        ecdsa::PublicKey::from_x962_uncompressed(spk)
            .or_else(|| ecdsa::PublicKey::from_x962_compressed(spk))
    }
}

/// Ed25519/EdDSA public key
///
/// OID: 1.3.101.112
pub(crate) struct PkiEd25519PublicKey;

impl PkiIdentified for PkiEd25519PublicKey {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ED25519;
}

impl PkiPublicKeyAlgorithm for PkiEd25519PublicKey {
    type PublicKey = ed25519::PublicKey;

    fn from_der_subject_public_key(spk: &[u8]) -> Option<Self::PublicKey> {
        Some(ed25519::PublicKey::from_bytes(spk.try_into().ok()?))
    }
}

/// A family of RSA signature scheme
struct PkiRsaPkcs1SignatureScheme<D> {
    pub(crate) _p: PhantomData<fn() -> D>,
}

pub(crate) enum DigestAlgorithm {
    Sha256,
    Sha384,
    Sha512,
}

/// RSA with SHA256 signature scheme
///
/// OID: 1.2.840.113549.1.1.11
impl PkiIdentified for PkiRsaPkcs1SignatureScheme<digest::Sha256> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PKCS1_SHA256;
}

/// RSA with SHA384
///
/// OID: 1.2.840.113549.1.1.12
impl PkiIdentified for PkiRsaPkcs1SignatureScheme<digest::Sha384> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PKCS1_SHA384;
}

/// RSA with SHA512
///
/// OID: 1.2.840.113549.1.1.13
impl PkiIdentified for PkiRsaPkcs1SignatureScheme<digest::Sha512> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PKCS1_SHA512;
}

impl<D: RsaSignatureDigest> PkiSignatureAlgorithm for PkiRsaPkcs1SignatureScheme<D>
where
    Self: PkiIdentified,
{
    type PublicKeyAlgorithm = PkiRsaPublicKey;

    fn verify(
        public_key: <Self::PublicKeyAlgorithm as PkiPublicKeyAlgorithm>::PublicKey,
        message: &[u8],
        signature: &[u8],
    ) -> bool {
        public_key.verify_pkcs1::<D>(message, signature).is_ok()
    }
}

/// A family of RSA signature scheme
struct PkiRsaPssSignatureScheme<D> {
    pub(crate) _p: PhantomData<fn() -> D>,
}

/// RSA PSS with SHA256 signature scheme
///
/// OID: 1.2.840.113549.1.1.10
impl PkiIdentified for PkiRsaPssSignatureScheme<digest::Sha256> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PSS_SHA256;
}

/// RSA PSS with SHA384
///
/// OID: 1.2.840.113549.1.1.10
impl PkiIdentified for PkiRsaPssSignatureScheme<digest::Sha384> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PSS_SHA384;
}

/// RSA PSS with SHA512
///
/// OID: 1.2.840.113549.1.1.10
impl PkiIdentified for PkiRsaPssSignatureScheme<digest::Sha512> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::RSA_PSS_SHA512;
}

impl<D: RsaSignatureDigest> PkiSignatureAlgorithm for PkiRsaPssSignatureScheme<D>
where
    Self: PkiIdentified,
{
    type PublicKeyAlgorithm = PkiRsaPublicKey;

    fn verify(
        public_key: <Self::PublicKeyAlgorithm as PkiPublicKeyAlgorithm>::PublicKey,
        message: &[u8],
        signature: &[u8],
    ) -> bool {
        public_key.verify_pss::<D>(message, signature).is_ok()
    }
}

struct PkiEcdsaScheme<C> {
    pub(crate) _p: PhantomData<fn() -> C>,
}

/// ECDSA with SHA-256
///
/// OID: 1.2.840.10045.4.3.2
impl PkiIdentified for PkiEcdsaScheme<ec::P256> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ECDSA_SHA256;
}

/// ECDSA with SHA-384
///
/// OID: 1.2.840.10045.4.3.3
impl PkiIdentified for PkiEcdsaScheme<ec::P384> {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ECDSA_SHA384;
}

impl<C: ec::Curve> PkiSignatureAlgorithm for PkiEcdsaScheme<C>
where
    Self: PkiIdentified,
    PkiEcPublicKey<C>: PkiIdentified,
{
    type PublicKeyAlgorithm = PkiEcPublicKey<C>;

    fn verify(
        public_key: <Self::PublicKeyAlgorithm as PkiPublicKeyAlgorithm>::PublicKey,
        message: &[u8],
        signature: &[u8],
    ) -> bool {
        // Safety: we would only allow SHA-256 hashing onto P-256 and SHA-384 hashing onto P-384
        public_key.verify(message, signature).is_ok()
    }
}

/// Signature algorithm EdDSA
///
/// OID: 1.3.101.112
struct PkiEddsa;

impl PkiIdentified for PkiEddsa {
    const OBJECT_IDENTIFIER: AlgorithmIdentifier = alg_id::ED25519;
}

impl PkiSignatureAlgorithm for PkiEddsa {
    type PublicKeyAlgorithm = PkiEd25519PublicKey;

    fn verify(
        public_key: <Self::PublicKeyAlgorithm as PkiPublicKeyAlgorithm>::PublicKey,
        message: &[u8],
        signature: &[u8],
    ) -> bool {
        let Ok(signature) = signature.try_into() else {
            return false;
        };
        public_key.verify(message, signature).is_ok()
    }
}

/// PKI Signature scheme `ECDSA_NISTP256_SHA256`
pub const ECDSA_NISTP256_SHA256: &'static dyn SignatureVerificationAlgorithm =
    &PkiSignatureVerification::<PkiEcdsaScheme<ec::P256>>::new();

/// PKI Signature scheme `ECDSA_NISTP384_SHA384`
pub const ECDSA_NISTP384_SHA384: &'static dyn SignatureVerificationAlgorithm =
    &PkiSignatureVerification::<PkiEcdsaScheme<ec::P384>>::new();

/// PKI Signature scheme `ED25519`
pub const ED25519: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiEddsa>::new();

/// PKI Signature scheme `RSA_PKCS1_SHA256`
pub const RSA_PKCS1_SHA256: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPkcs1SignatureScheme<digest::Sha256>>::new();

/// PKI Signature scheme `RSA_PKCS1_SHA384`
pub const RSA_PKCS1_SHA384: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPkcs1SignatureScheme<digest::Sha384>>::new();

/// PKI Signature scheme `RSA_PKCS1_SHA512`
pub const RSA_PKCS1_SHA512: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPkcs1SignatureScheme<digest::Sha512>>::new();

/// PKI Signature scheme `RSA_PSS_SHA256`
pub const RSA_PSS_SHA256: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPssSignatureScheme<digest::Sha256>>::new();

/// PKI Signature scheme `RSA_PSS_SHA384`
pub const RSA_PSS_SHA384: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPssSignatureScheme<digest::Sha384>>::new();

/// PKI Signature scheme `RSA_PSS_SHA512`
pub const RSA_PSS_SHA512: &'static (dyn SignatureVerificationAlgorithm + 'static) =
    &PkiSignatureVerification::<PkiRsaPssSignatureScheme<digest::Sha512>>::new();
