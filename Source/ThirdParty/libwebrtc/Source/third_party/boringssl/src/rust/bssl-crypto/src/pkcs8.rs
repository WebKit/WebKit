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

//! PKCS#8 support
//!
//! This module provides PKCS#8 private key parsing for supported key types.
//! The [`SigningKey`] enum can parse DER-encoded PKCS#8 PrivateKeyInfo
//! structures containing RSA, ECDSA (P-256 and P-384), or Ed25519 keys.
//!
//! ```
//! use bssl_crypto::pkcs8;
//!
//! // A DER-encoded PKCS#8 Ed25519 private key.
//! let pkcs8_der = [
//!     0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70,
//!     0x04, 0x22, 0x04, 0x20, 0x0a, 0x00, 0xc1, 0xfd, 0xfa, 0xaa, 0xdd, 0xc7,
//!     0x35, 0x79, 0xe0, 0x06, 0x75, 0xc7, 0xcf, 0x57, 0x3a, 0x2c, 0x91, 0xe6,
//!     0x6a, 0x45, 0x11, 0x0e, 0xbd, 0xde, 0xa9, 0xa7, 0x83, 0xed, 0xbb, 0x86,
//! ];
//!
//! let key = pkcs8::SigningKey::from_der_private_key_info(&pkcs8_der)
//!     .expect("valid PKCS#8 key");
//!
//! match key {
//!     pkcs8::SigningKey::Ed25519(ed_key) => {
//!         let signature = ed_key.sign(b"message");
//!         assert_eq!(signature.len(), 64);
//!     }
//!     _ => panic!("expected Ed25519 key"),
//! }
//! ```

use crate::{ec, ecdsa, ed25519, rsa, scoped::EvpPkey};

/// PKCS#8 Signing Key
pub enum SigningKey {
    /// An RSA private key
    Rsa(rsa::PrivateKey),
    /// An NIST P-256 private key
    EcP256(ecdsa::PrivateKey<ec::P256>),
    /// An NIST P-384 private key
    EcP384(ecdsa::PrivateKey<ec::P384>),
    /// An Ed25519 key
    Ed25519(ed25519::PrivateKey),
}

impl SigningKey {
    /// Parse a DER-encoded PKCS#8 PrivateKeyInfo structure.
    pub fn from_der_private_key_info(data: &[u8]) -> Option<Self> {
        // Safety:
        // - data buffer is guaranteed to be initialised
        // - all the algorithm descriptors are static items and remain live at call time
        let mut pkey = unsafe {
            EvpPkey::from_der_private_key_info(
                data,
                &[
                    bssl_sys::EVP_pkey_rsa(),
                    bssl_sys::EVP_pkey_ec_p256(),
                    bssl_sys::EVP_pkey_ec_p384(),
                    bssl_sys::EVP_pkey_ed25519(),
                ],
            )
        }?;
        // Safety: pkey is initialised
        let id = unsafe { bssl_sys::EVP_PKEY_id(pkey.as_ffi_ptr()) };
        unsafe {
            // Safety: the pkey is completely owned here
            Some(match id {
                bssl_sys::EVP_PKEY_RSA => Self::Rsa(rsa::PrivateKey::from_evp_pkey(pkey)?),
                bssl_sys::EVP_PKEY_EC => {
                    let key = ec::Key::from_evp_pkey(pkey)?;
                    match key.get_group()? {
                        ec::Group::P256 => Self::EcP256(ecdsa::PrivateKey::from_ec_key(key)),
                        ec::Group::P384 => Self::EcP384(ecdsa::PrivateKey::from_ec_key(key)),
                    }
                }
                bssl_sys::EVP_PKEY_ED25519 => {
                    // Safety: we are sure that the key is for ED25519
                    Self::Ed25519(ed25519::PrivateKey::from_evp_pkey(pkey))
                }
                _ => return None,
            })
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_helpers::decode_hex_into_vec;

    // PKCS#8 RSA 2048-bit private key generated with:
    // openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -outform DER | \
    //     openssl pkcs8 -topk8 -nocrypt -inform DER -outform DER
    const RSA_PKCS8: &str = concat!(
        "308204bd020100300d06092a864886f70d0101010500048204a7308204a302010002820101",
        "00c5566a93495f06a465b58ea7f200058ce9a64ab0f7755c7ecfcb8b291e715619887ea8bf",
        "285e978e4820426f70c8abf308ef532f4bb6dd23ddbf120ea03434925dbc3c4748347c8c28",
        "2ddbd67b9f69b360be6a23378c30710a37e4745f5001c571ea20986976f7b6182a605874a9",
        "444ac611fa91c76e1d4b73e3d32382df4b8702929ef3d85e2b587cf1cc917e7c9ffb506c06",
        "aaf9b6e793cfd2b156922a81e5d781f6103a4680366e94ae6f6fe37a71be763ab7970e0d63",
        "7a420aa0af64a01e771af02af05cc6d89ec1b1f42cae75e9c295e06524c23e722fd1c2b236",
        "c6f1eb8748cf5b7257eda869325809f1beb99bd26cb72cabe956bc19bf86bd1543edf70203",
        "010001028201001878bacc5b67c5dbed8daccc5b40866faeea98ce464f0f07de294884c7b4",
        "d8050ae8b8c61a86ff20bd5ab172696d9be93927edf715056f24b5fdd90ae847611244e930",
        "1e74daa6c153703b181234e1779b07dcadfe584bb4e646aa7585fbac248f425b5ac5e56370",
        "4e4c89adc92b292ab9525c723c90300b7dd523075480f00d6d26bb58e5aebb555240d8b210",
        "3aac12585fdd990146251c75b8ce4d5c0b7d22746ca89991a9090f9e518d7f17f491d354dd",
        "19aaa0d8bdccdaed89e39ef09ba789e8a796fff792eef55b844f239d251b97fd679ba91edc",
        "cf06665396355945d6c69a5cef94884462a50f2473a34f628d79d25ab95af067eec3180e22",
        "041f637d02818100e5b209a07285ffc831f0bcb910ef428a5fdccf1669d07cd359fdb5227f",
        "43a08f859a6d6fd2206bb2bd4c616eec3e54887a716e5887688bea0dbdb655b1cccdc7de89",
        "62a5acfb6bd317c2ef49850f4d681057f115365256d5530b8f9f34908cff85dab30c490212",
        "73521edc9c5282ce7263d3841fabc3adbe276bf33b7f0faedd02818100dbefbeb8e742ac8d",
        "340d378c41283367a222a3d23b3449702068741f0ad91c004b239619e2db30582e1675800f",
        "c7c320953f3fe346137dcd35c79cbdd4f7ae776936bada8a5deb860907bf8b1443aab3912a",
        "f00a375ccd7a24c3e9366a77192e294308b94490ea3025811565ff05cea0bbfb56da873ff5",
        "f8908aec7ea4dd60e30281805943a864173da61aa9f5d191e657e5371b6c177ab16299b015",
        "4ff89dd0717aab6c1388a62535fe44b73640c337c23d5dd09fd66f472844ff8f99838ba80e",
        "5c866920611adbafd5c6727c8a3bbb1f2848e1d91b52d00a8dbe5788ada704698cb21cd5d2",
        "315b0a181b82f5856ca6d038e4d190b8cf0a1480a7de702055a5da756d02818100b3db5922",
        "a8ac13a3dd7f397fcf00eb18c2b48537b506cb4f90911af50fd00060151262fb84532f33cd",
        "6cbc661f818306b0466b1e96fdf590cd7c11a803f3108fc250e97932521ffb1a8365967cd9",
        "e14cbb585bb85f11db4f19a5c49fa56d040085e9b5c69c55cdcdd5bdbc1c0ef356c88731c1",
        "13302b9420d34368a7207791750281800128132f6b43c847475e9473f8e560353d82363354",
        "2bac259ecdaac23e7476b670eaf92ffb028cb76cc34482e37fedbfcab32ed00ad7846640dc",
        "d52d813494297d4c3a47b7d3ca499a7001fdd0dc7505ef14b4321afa4feac776854b9eeaad",
        "12ac12c4ea6705e3e471cc6cc601e8d2c9eabbdfbe2402095dbd2347ed974d9eec",
    );

    // PKCS#8 ECDSA P-256 private key generated with:
    // openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -outform DER | \
    //     openssl pkcs8 -topk8 -nocrypt -inform DER -outform DER
    const P256_PKCS8: &str = concat!(
        "308187020100301306072a8648ce3d020106082a8648ce3d030107046d306b020101042060",
        "b876fbddaacaaea4c67f74977fdfe960bb983b926a6c9c05d779792bd93e82a144034200",
        "040ef1c4149f8c2590451a365946342c7309352fdca02454426548bf6d357ca5b67b4506",
        "a114021559eec6d04f18f7ba2b8f7b7c16aed3fce634f337da4ff30304",
    );

    // PKCS#8 ECDSA P-384 private key generated with:
    // openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-384 -outform DER | \
    //     openssl pkcs8 -topk8 -nocrypt -inform DER -outform DER
    const P384_PKCS8: &str = concat!(
        "3081b6020100301006072a8648ce3d020106052b8104002204819e30819b020101043059",
        "591c3d78a837537b1dc48fc392c586d2168b04107c7f52b877701a747e965fe4cca32cb1",
        "befae1d48e3eea531b7ec8a164036200045027002fb62850c3401a532f459adf06237638",
        "8722f057f6c7500776835a100835706b6d9228bcad1bfec3f2a996f23743172736830feb",
        "32e0f5275cef34eb11f5c3764c44fb3a8a535ce0d8a0432167150ed53d9e01b71723699d",
        "1ecfa8fe1e",
    );

    // PKCS#8 Ed25519 private key generated with:
    // openssl genpkey -algorithm ED25519 -outform DER | \
    //     openssl pkcs8 -topk8 -nocrypt -inform DER -outform DER
    const ED25519_PKCS8: &str =
        "302e020100300506032b6570042204200a00c1fdfaaaddc73579e00675c7cf573a2c91e66a45110ebddea9a783edbb86";

    #[test]
    fn parse_rsa_pkcs8() {
        let der = decode_hex_into_vec(RSA_PKCS8);
        let key = SigningKey::from_der_private_key_info(&der).expect("valid RSA PKCS#8");
        assert!(matches!(key, SigningKey::Rsa(_)));
    }

    #[test]
    fn parse_p256_pkcs8() {
        let der = decode_hex_into_vec(P256_PKCS8);
        let key = SigningKey::from_der_private_key_info(&der).expect("valid P-256 PKCS#8");
        assert!(matches!(key, SigningKey::EcP256(_)));
    }

    #[test]
    fn parse_p384_pkcs8() {
        let der = decode_hex_into_vec(P384_PKCS8);
        let key = SigningKey::from_der_private_key_info(&der).expect("valid P-384 PKCS#8");
        assert!(matches!(key, SigningKey::EcP384(_)));
    }

    #[test]
    fn parse_ed25519_pkcs8() {
        let der = decode_hex_into_vec(ED25519_PKCS8);
        let key = SigningKey::from_der_private_key_info(&der).expect("valid Ed25519 PKCS#8");
        assert!(matches!(key, SigningKey::Ed25519(_)));
    }

    #[test]
    fn invalid_pkcs8_rejected() {
        assert!(SigningKey::from_der_private_key_info(b"").is_none());
        assert!(SigningKey::from_der_private_key_info(b"not valid der").is_none());
        // Truncated key
        let der = decode_hex_into_vec(ED25519_PKCS8);
        assert!(SigningKey::from_der_private_key_info(&der[..der.len() / 2]).is_none());
    }
}
