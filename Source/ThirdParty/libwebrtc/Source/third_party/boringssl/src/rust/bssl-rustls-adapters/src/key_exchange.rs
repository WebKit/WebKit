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

//! Supported key exchange groups
//!
//! We support the following standard groups.
//!
//! - [ECDH_P256] for elliptic curve based Diffie-Hellman ECDH-P256
//! - [ECDH_P384] for elliptic curve based Diffie-Hellman ECDH-P384
//! - [X25519] for X25519
//! - [X25519MLKEM768] for X25519MLKEM768

use alloc::{
    boxed::Box,
    fmt::{Debug, Formatter, Result as FmtResult},
    vec::Vec,
};
use core::marker::PhantomData;

use bssl_crypto::{ec, ecdh, x25519};
use rustls::{
    Error, NamedGroup, PeerMisbehaved,
    crypto::{ActiveKeyExchange, SharedSecret, SupportedKxGroup},
};

mod mlkem;
pub use mlkem::X25519MLKEM768;

/// Elliptic Curve Diffie-Hellman key exchange group
struct EcGroup<C: ec::Curve>(PhantomData<fn() -> C>);
/// Elliptic Curve Diffie-Hellman key exchange group `ECDH-P256`
pub const ECDH_P256: &'static dyn SupportedKxGroup = &EcGroup::<ec::P256>(PhantomData);
/// Elliptic Curve Diffie-Hellman key exchange group `ECDH-P384`
pub const ECDH_P384: &'static dyn SupportedKxGroup = &EcGroup::<ec::P384>(PhantomData);

impl<C: ec::Curve> Debug for EcGroup<C> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("EcGroup").finish()
    }
}

impl<C: ec::Curve + 'static> SupportedKxGroup for EcGroup<C> {
    fn start(&self) -> Result<Box<dyn ActiveKeyExchange>, Error> {
        let priv_key = ecdh::PrivateKey::<C>::generate();
        let pub_key = priv_key.to_public_key();
        let pub_key_x962_uncompressed = pub_key.to_x962_uncompressed().as_ref().into();
        Ok(Box::new(EcActiveKeyExchange {
            priv_key,
            pub_key_x962_uncompressed,
        }))
    }

    fn name(&self) -> NamedGroup {
        match C::group() {
            ec::Group::P256 => NamedGroup::secp256r1,
            ec::Group::P384 => NamedGroup::secp384r1,
        }
    }
}

/// This type encodes the Diffie-Hellman key exchange state during TLS.
struct EcActiveKeyExchange<C: ec::Curve> {
    priv_key: ecdh::PrivateKey<C>,
    pub_key_x962_uncompressed: Vec<u8>,
}
impl<C: ec::Curve> ActiveKeyExchange for EcActiveKeyExchange<C> {
    fn complete(self: Box<Self>, peer_pub_key: &[u8]) -> Result<SharedSecret, Error> {
        let peer_pub_key = ecdh::PublicKey::from_x962_uncompressed(peer_pub_key)
            .ok_or(Error::PeerMisbehaved(PeerMisbehaved::InvalidKeyShare))?;
        let shared_secret = self.priv_key.compute_shared_key(&peer_pub_key);
        Ok(shared_secret.into())
    }

    fn pub_key(&self) -> &[u8] {
        &self.pub_key_x962_uncompressed
    }

    fn group(&self) -> NamedGroup {
        match C::group() {
            ec::Group::P256 => NamedGroup::secp256r1,
            ec::Group::P384 => NamedGroup::secp384r1,
        }
    }
}

/// X25519 Key exchange group
#[derive(Debug)]
struct X25519Group;
/// X25519 key exchange group
pub const X25519: &'static dyn SupportedKxGroup = &X25519Group;

impl SupportedKxGroup for X25519Group {
    fn start(&self) -> Result<Box<dyn ActiveKeyExchange>, Error> {
        let (pub_key, priv_key) = x25519::PrivateKey::generate();
        Ok(Box::new(X25519ActiveKeyExchange { pub_key, priv_key }))
    }

    fn name(&self) -> NamedGroup {
        NamedGroup::X25519
    }
}

struct X25519ActiveKeyExchange {
    pub_key: x25519::PublicKey,
    priv_key: x25519::PrivateKey,
}

impl ActiveKeyExchange for X25519ActiveKeyExchange {
    fn complete(self: Box<Self>, peer_pub_key: &[u8]) -> Result<SharedSecret, Error> {
        let secret = self
            .priv_key
            .compute_shared_key(
                peer_pub_key
                    .try_into()
                    .map_err(|_| Error::PeerMisbehaved(PeerMisbehaved::InvalidKeyShare))?,
            )
            .ok_or(Error::PeerMisbehaved(PeerMisbehaved::InvalidKeyShare))?
            .to_vec();
        Ok(secret.into())
    }

    fn pub_key(&self) -> &[u8] {
        &self.pub_key
    }

    fn group(&self) -> NamedGroup {
        NamedGroup::X25519
    }
}
