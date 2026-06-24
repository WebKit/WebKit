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

use alloc::{boxed::Box, vec::Vec};
use core::{
    fmt::{Debug, Formatter, Result as FmtResult},
    marker::PhantomData,
};

use bssl_crypto::{digest, ec, ecdsa, ed25519, rsa};
use rustls::{
    Error, SignatureAlgorithm, SignatureScheme,
    pki_types::{PrivatePkcs1KeyDer, PrivateSec1KeyDer},
    sign::{Signer, SigningKey},
};

use crate::{RsaSignatureDigest, pki};

/// A generic `id-rsaEncryption` RSA key
///
/// This key type is only intended for signature
pub(crate) struct RsaPrivateKey(pub rsa::PrivateKey);

impl TryFrom<PrivatePkcs1KeyDer<'_>> for RsaPrivateKey {
    type Error = Error;
    fn try_from(der: PrivatePkcs1KeyDer) -> Result<Self, Self::Error> {
        let der = der.secret_pkcs1_der();
        let key = rsa::PrivateKey::from_der_rsa_private_key(der)
            .ok_or(Error::General("Cannot parse PKCS#1 key from DER".into()))?;
        Ok(Self(key))
    }
}

impl Debug for RsaPrivateKey {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("RsaPrivateKey").finish()
    }
}

struct RsaPkcs1Signer<D> {
    key: rsa::PrivateKey,
    _p: PhantomData<fn() -> D>,
}

impl<D> Debug for RsaPkcs1Signer<D> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("RsaPkcs1Signer").finish()
    }
}

impl<D: RsaSignatureDigest> RsaPkcs1Signer<D> {
    fn new(key: rsa::PrivateKey) -> Self {
        Self {
            key,
            _p: PhantomData,
        }
    }
}

impl<D: RsaSignatureDigest> Signer for RsaPkcs1Signer<D> {
    fn sign(&self, message: &[u8]) -> Result<Vec<u8>, Error> {
        Ok(self.key.sign_pkcs1::<D>(message))
    }

    fn scheme(&self) -> SignatureScheme {
        match D::ALGORITHM {
            pki::DigestAlgorithm::Sha256 => SignatureScheme::RSA_PKCS1_SHA256,
            pki::DigestAlgorithm::Sha384 => SignatureScheme::RSA_PKCS1_SHA384,
            pki::DigestAlgorithm::Sha512 => SignatureScheme::RSA_PKCS1_SHA512,
        }
    }
}

struct RsaPssSigner<D> {
    key: rsa::PrivateKey,
    _p: PhantomData<fn() -> D>,
}

impl<D> Debug for RsaPssSigner<D> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("RsaPssSigner").finish()
    }
}

impl<D: RsaSignatureDigest> RsaPssSigner<D> {
    fn new(key: rsa::PrivateKey) -> Self {
        Self {
            key,
            _p: PhantomData,
        }
    }
}

impl<D: RsaSignatureDigest> Signer for RsaPssSigner<D> {
    fn sign(&self, message: &[u8]) -> Result<Vec<u8>, Error> {
        Ok(self.key.sign_pss::<D>(message))
    }

    fn scheme(&self) -> SignatureScheme {
        match D::ALGORITHM {
            pki::DigestAlgorithm::Sha256 => SignatureScheme::RSA_PSS_SHA256,
            pki::DigestAlgorithm::Sha384 => SignatureScheme::RSA_PSS_SHA384,
            pki::DigestAlgorithm::Sha512 => SignatureScheme::RSA_PSS_SHA512,
        }
    }
}

impl SigningKey for RsaPrivateKey {
    fn choose_scheme(&self, offered: &[SignatureScheme]) -> Option<Box<dyn Signer>> {
        for offer in offered {
            match offer {
                SignatureScheme::RSA_PKCS1_SHA1 => continue,
                SignatureScheme::RSA_PKCS1_SHA256 => {
                    return Some(Box::new(RsaPkcs1Signer::<digest::Sha256>::new(
                        self.0.clone(),
                    )));
                }
                SignatureScheme::RSA_PKCS1_SHA384 => {
                    return Some(Box::new(RsaPkcs1Signer::<digest::Sha384>::new(
                        self.0.clone(),
                    )));
                }
                SignatureScheme::RSA_PKCS1_SHA512 => {
                    return Some(Box::new(RsaPkcs1Signer::<digest::Sha512>::new(
                        self.0.clone(),
                    )));
                }
                SignatureScheme::RSA_PSS_SHA256 => {
                    return Some(Box::new(RsaPssSigner::<digest::Sha256>::new(
                        self.0.clone(),
                    )));
                }
                SignatureScheme::RSA_PSS_SHA384 => {
                    return Some(Box::new(RsaPssSigner::<digest::Sha384>::new(
                        self.0.clone(),
                    )));
                }
                SignatureScheme::RSA_PSS_SHA512 => {
                    return Some(Box::new(RsaPssSigner::<digest::Sha512>::new(
                        self.0.clone(),
                    )));
                }
                _ => continue,
            }
        }
        None
    }

    fn algorithm(&self) -> SignatureAlgorithm {
        SignatureAlgorithm::RSA
    }
}

/// An ECDSA signing key
#[derive(Clone)]
pub(crate) struct EcdsaPrivateKey<C: ec::Curve>(pub ecdsa::PrivateKey<C>);

impl<C: ec::Curve> Debug for EcdsaPrivateKey<C> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("EcdsaPrivateKey").finish()
    }
}

impl<C: ec::Curve> TryFrom<&'_ PrivateSec1KeyDer<'_>> for EcdsaPrivateKey<C> {
    type Error = Error;

    fn try_from(der: &PrivateSec1KeyDer) -> Result<Self, Self::Error> {
        let der = der.secret_sec1_der();
        let key = ecdsa::PrivateKey::from_der_ec_private_key(der)
            .ok_or(Error::General("Cannot parse Sec1 key from DER".into()))?;
        Ok(Self(key))
    }
}

macro_rules! ecdsa_signer {
    ($scheme:path, $curve:path) => {
        impl SigningKey for EcdsaPrivateKey<$curve> {
            fn choose_scheme(&self, offered: &[SignatureScheme]) -> Option<Box<dyn Signer>> {
                for offer in offered {
                    match offer {
                        // We do not support any SHA1 scheme
                        SignatureScheme::ECDSA_SHA1_Legacy => continue,
                        $scheme => return Some(Box::new(EcdsaSigner(self.0.clone()))),
                        _ => continue,
                    }
                }
                None
            }

            fn algorithm(&self) -> SignatureAlgorithm {
                SignatureAlgorithm::ECDSA
            }
        }

        impl Signer for EcdsaSigner<$curve> {
            fn sign(&self, message: &[u8]) -> Result<Vec<u8>, Error> {
                Ok(self.0.sign(message))
            }

            fn scheme(&self) -> SignatureScheme {
                $scheme
            }
        }
    };
}

ecdsa_signer!(SignatureScheme::ECDSA_NISTP256_SHA256, ec::P256);
ecdsa_signer!(SignatureScheme::ECDSA_NISTP384_SHA384, ec::P384);

struct EcdsaSigner<C: ec::Curve>(ecdsa::PrivateKey<C>);

impl<C: ec::Curve> Debug for EcdsaSigner<C> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("EcdsaSigner").finish()
    }
}

#[derive(Clone)]
pub(crate) struct EddsaPrivateKey(pub ed25519::PrivateKey);

impl Debug for EddsaPrivateKey {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("EddsaPrivateKey").finish()
    }
}

impl SigningKey for EddsaPrivateKey {
    fn choose_scheme(&self, offered: &[SignatureScheme]) -> Option<Box<dyn Signer>> {
        for offer in offered {
            if matches!(offer, SignatureScheme::ED25519) {
                return Some(Box::new(EddsaSigner(self.0.clone())));
            }
        }
        None
    }

    fn algorithm(&self) -> SignatureAlgorithm {
        SignatureAlgorithm::ED25519
    }
}

struct EddsaSigner(ed25519::PrivateKey);

impl Debug for EddsaSigner {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_tuple("EddsaSigner").finish()
    }
}

impl Signer for EddsaSigner {
    fn sign(&self, message: &[u8]) -> Result<Vec<u8>, Error> {
        Ok(self.0.sign(message).to_vec())
    }

    fn scheme(&self) -> SignatureScheme {
        SignatureScheme::ED25519
    }
}
