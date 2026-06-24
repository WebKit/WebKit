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

use alloc::{boxed::Box, vec::Vec};
use core::marker::PhantomData;

use bssl_crypto::{
    digest::{self, Algorithm},
    hkdf, hmac,
    tls12_prf::Tls12Prf,
};
use rustls::{
    Error,
    crypto::{
        hmac::Tag,
        tls12::Prf,
        tls13::{Hkdf, HkdfExpander, OkmBlock, OutputLengthError},
    },
    version::TLS12,
};

pub(crate) struct Tls12PrfImpl<A>(PhantomData<fn() -> A>);

impl<A> Tls12PrfImpl<A> {
    pub(crate) const fn new() -> Self {
        Self(PhantomData)
    }
}

impl<A: digest::Algorithm> Prf for Tls12PrfImpl<A> {
    fn for_key_exchange(
        &self,
        output: &mut [u8; 48],
        kx: Box<dyn rustls::crypto::ActiveKeyExchange>,
        peer_pub_key: &[u8],
        label: &[u8],
        seed: &[u8],
    ) -> Result<(), Error> {
        Tls12Prf::<A>::generate_secret(
            kx.complete_for_tls_version(peer_pub_key, &TLS12)?
                .secret_bytes(),
            label,
            seed,
            None,
            output,
        )
        .map_err(|_| Error::General("PRF failed".into()))
    }

    fn for_secret(&self, output: &mut [u8], secret: &[u8], label: &[u8], seed: &[u8]) {
        Tls12Prf::<A>::generate_secret(secret, label, seed, None, output)
            .expect("for_secret should be infallible");
    }
}

/// TLS 1.3 HKDF over SHA-256
pub(crate) const TLS13_HKDF_SHA256: &'static dyn Hkdf = &Tls13HkdfImpl::<digest::Sha256>::new();
/// TLS 1.3 HKDF over SHA-384
pub(crate) const TLS13_HKDF_SHA384: &'static dyn Hkdf = &Tls13HkdfImpl::<digest::Sha384>::new();

pub(crate) struct Tls13HkdfImpl<A>(PhantomData<fn() -> A>);

impl<A> Tls13HkdfImpl<A> {
    pub(crate) const fn new() -> Self {
        Self(PhantomData)
    }
}

struct Tls13HkdfExpander<A> {
    prk: hkdf::Prk,
    _p: PhantomData<fn() -> A>,
}

impl<A: Tls13HkdfDigest> HkdfExpander for Tls13HkdfExpander<A> {
    fn expand_slice(&self, info: &[&[u8]], output: &mut [u8]) -> Result<(), OutputLengthError> {
        let info: Vec<_> = info.iter().copied().flatten().copied().collect();
        self.prk
            .expand_into(&info, output)
            .map_err(|_| OutputLengthError)
    }

    fn expand_block(&self, info: &[&[u8]]) -> OkmBlock {
        let mut buf = A::zero_secret().to_vec();
        let info: Vec<_> = info.iter().copied().flatten().copied().collect();
        self.prk
            .expand_into(&info, &mut buf)
            .expect("digest length should not be too long");
        OkmBlock::new(&buf)
    }

    fn hash_len(&self) -> usize {
        A::OUTPUT_LEN
    }
}

trait Tls13HkdfDigest: digest::Algorithm {
    fn hmac_sign(key: &[u8], data: &[u8]) -> Tag;
    fn zero_secret() -> &'static [u8];
}

impl Tls13HkdfDigest for digest::Sha256 {
    fn hmac_sign(key: &[u8], data: &[u8]) -> Tag {
        Tag::new(&hmac::HmacSha256::mac(key, data))
    }
    fn zero_secret() -> &'static [u8] {
        &[0; Self::OUTPUT_LEN]
    }
}

impl Tls13HkdfDigest for digest::Sha384 {
    fn hmac_sign(key: &[u8], data: &[u8]) -> Tag {
        Tag::new(&hmac::HmacSha384::mac(key, data))
    }
    fn zero_secret() -> &'static [u8] {
        &[0; Self::OUTPUT_LEN]
    }
}

impl<A: 'static + Tls13HkdfDigest> Hkdf for Tls13HkdfImpl<A> {
    fn extract_from_zero_ikm(&self, salt: Option<&[u8]>) -> Box<dyn HkdfExpander> {
        let prk = hkdf::Hkdf::<A>::extract(
            A::zero_secret(),
            if let Some(salt) = salt {
                hkdf::Salt::NonEmpty(salt)
            } else {
                hkdf::Salt::None
            },
        );
        Box::new(Tls13HkdfExpander::<A> {
            prk,
            _p: PhantomData,
        })
    }

    fn extract_from_secret(&self, salt: Option<&[u8]>, secret: &[u8]) -> Box<dyn HkdfExpander> {
        let prk = hkdf::Hkdf::<A>::extract(
            secret,
            if let Some(salt) = salt {
                hkdf::Salt::NonEmpty(salt)
            } else {
                hkdf::Salt::None
            },
        );
        Box::new(Tls13HkdfExpander::<A> {
            prk,
            _p: PhantomData,
        })
    }

    fn expander_for_okm(&self, okm: &OkmBlock) -> Box<dyn HkdfExpander> {
        let okm = okm.as_ref();
        Box::new(Tls13HkdfExpander::<A> {
            prk: hkdf::Prk::new::<A>(okm).expect("OKM size mismatch"),
            _p: PhantomData,
        })
    }

    fn hmac_sign(&self, key: &OkmBlock, message: &[u8]) -> Tag {
        A::hmac_sign(key.as_ref(), message)
    }
}
