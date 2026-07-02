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

use bssl_crypto::{mlkem, x25519};
use rustls::{
    Error, NamedGroup, PeerMisbehaved, ProtocolVersion,
    crypto::{ActiveKeyExchange, CompletedKeyExchange, SharedSecret, SupportedKxGroup},
};

/// X25519MLKEM768 key exchange group
pub const X25519MLKEM768: &'static dyn SupportedKxGroup = &X25519Mlkem768Group;

macro_rules! ok_or_invalid_share {
    ($e:expr) => {
        match $e {
            Some(v) => v,
            None => return Err(Error::PeerMisbehaved(PeerMisbehaved::InvalidKeyShare)),
        }
    };
}

#[derive(Debug)]
struct X25519Mlkem768Group;

impl X25519Mlkem768Group {
    fn extract_keys(client_pub_key: &[u8]) -> Option<(mlkem::PublicKey768, x25519::PublicKey)> {
        // X25519MLKEM768 transposed the order of concatenation in spite of the naming
        let (client_encap_key, client_pub_key) =
            client_pub_key.split_at_checked(mlkem::PUBLIC_KEY_BYTES_768)?;
        let client_encap_key = mlkem::PublicKey768::parse(client_encap_key)?;
        Some((client_encap_key, client_pub_key.try_into().ok()?))
    }
}

impl SupportedKxGroup for X25519Mlkem768Group {
    fn start(&self) -> Result<Box<dyn ActiveKeyExchange>, Error> {
        let (encap_key, decap_key, _) = mlkem::PrivateKey768::generate();
        let (pub_key, priv_key) = x25519::PrivateKey::generate();
        // X25519MLKEM768 transposed the order of concatenation in spite of the naming
        let mut client_share = encap_key;
        client_share.extend_from_slice(&pub_key);
        Ok(Box::new(X25519MlKem768ActiveKeyExchange {
            decap_key,
            priv_key,
            pub_key,
            client_share,
        }))
    }

    // This override is for server side share
    fn start_and_complete(&self, client_share: &[u8]) -> Result<CompletedKeyExchange, Error> {
        // X25519MLKEM768 transposed the order of concatenation in spite of the naming
        let (client_encap_key, client_pub_key) =
            ok_or_invalid_share!(Self::extract_keys(client_share));
        let (pub_key, priv_key) = x25519::PrivateKey::generate();
        let dh_secret = ok_or_invalid_share!(priv_key.compute_shared_key(&client_pub_key));
        let (mlkem_ctxt, quantum_secret) = client_encap_key.encapsulate();

        let mut server_share = mlkem_ctxt;
        server_share.extend(pub_key);

        let mut secret = Vec::with_capacity(mlkem::SHARED_SECRET_BYTES + x25519::SHARED_KEY_LEN);
        secret.extend(quantum_secret);
        secret.extend(dh_secret);
        Ok(CompletedKeyExchange {
            group: NamedGroup::X25519MLKEM768,
            pub_key: server_share,
            secret: secret.into(),
        })
    }

    fn name(&self) -> NamedGroup {
        NamedGroup::X25519MLKEM768
    }

    fn usable_for_version(&self, version: ProtocolVersion) -> bool {
        matches!(version, ProtocolVersion::TLSv1_3)
    }
}

struct X25519MlKem768ActiveKeyExchange {
    decap_key: mlkem::PrivateKey768,
    priv_key: x25519::PrivateKey,
    pub_key: x25519::PublicKey,
    client_share: Vec<u8>,
}

impl X25519MlKem768ActiveKeyExchange {
    fn compute_dh_share(&self, peer_pub_key: &[u8]) -> Result<[u8; x25519::SHARED_KEY_LEN], Error> {
        peer_pub_key
            .try_into()
            .ok()
            .and_then(|peer_pub_key| self.priv_key.compute_shared_key(&peer_pub_key))
            .ok_or(Error::PeerMisbehaved(PeerMisbehaved::InvalidKeyShare))
    }
}

impl ActiveKeyExchange for X25519MlKem768ActiveKeyExchange {
    fn complete(self: Box<Self>, peer_pub_key: &[u8]) -> Result<SharedSecret, Error> {
        // X25519MLKEM768 transposed the order of concatenation in spite of the naming
        let (peer_mlkem_share, peer_x25519_share) =
            ok_or_invalid_share!(peer_pub_key.split_at_checked(mlkem::CIPHERTEXT_BYTES_768));
        let quantum_secret = ok_or_invalid_share!(self.decap_key.decapsulate(peer_mlkem_share));
        let dh_secret = self.compute_dh_share(peer_x25519_share)?;
        let mut shared_secret =
            Vec::with_capacity(mlkem::SHARED_SECRET_BYTES + x25519::SHARED_KEY_LEN);
        shared_secret.extend(quantum_secret);
        shared_secret.extend(dh_secret);
        Ok(shared_secret.into())
    }

    fn pub_key(&self) -> &[u8] {
        &self.client_share
    }

    fn hybrid_component(&self) -> Option<(NamedGroup, &[u8])> {
        Some((NamedGroup::X25519, &self.pub_key))
    }

    fn complete_hybrid_component(
        self: Box<Self>,
        peer_pub_key: &[u8],
    ) -> Result<SharedSecret, Error> {
        self.compute_dh_share(peer_pub_key)
            .map(|secret| Vec::from(secret).into())
    }

    fn group(&self) -> NamedGroup {
        NamedGroup::X25519MLKEM768
    }
}
