/* Copyright (c) 2023, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

use alloc::vec::Vec;
use core::marker::PhantomData;

use crate::{
    ec::{Curve, EcKey},
    pkey::{Pkey, PkeyCtx},
    CSliceMut, ForeignType,
};

pub use crate::ec::P256;

/// Private key used in a elliptic curve Diffie-Hellman.
pub struct PrivateKey<C: Curve> {
    /// An EcKey containing the private-public key pair
    eckey: EcKey,
    marker: PhantomData<C>,
}

/// Error type for ECDH operations.
#[derive(Debug)]
pub enum Error {
    /// Failed when trying to convert between representations.
    ConversionFailed,
    /// The Diffie-Hellman key exchange failed.
    DiffieHellmanFailed,
}

impl<C: Curve> PrivateKey<C> {
    /// Derives a shared secret from this private key and the given public key.
    ///
    /// # Panics
    /// When `OUTPUT_SIZE` is insufficient to store the output of the shared secret.
    #[allow(clippy::expect_used)]
    pub fn diffie_hellman<const OUTPUT_SIZE: usize>(
        &self,
        other_public_key: &PublicKey<C>,
    ) -> Result<SharedSecret<OUTPUT_SIZE>, Error> {
        let pkey: Pkey = (&self.eckey).into();
        let pkey_ctx = PkeyCtx::new(&pkey);
        let other_pkey: Pkey = (&other_public_key.eckey).into();
        let mut output = [0_u8; OUTPUT_SIZE];
        pkey_ctx
            .diffie_hellman(&other_pkey, CSliceMut(&mut output))
            .map(|_| SharedSecret(output))
            .map_err(|_| Error::DiffieHellmanFailed)
    }

    /// Generate a new private key for use in a Diffie-Hellman key exchange.
    pub fn generate() -> Self {
        Self {
            eckey: EcKey::generate(C::ec_group()),
            marker: PhantomData,
        }
    }

    /// Tries to convert the given bytes into an private key.
    ///
    /// `private_key_bytes` is the octet form that consists of the content octets of the
    /// `privateKey` `OCTET STRING` in an `ECPrivateKey` ASN.1 structure.
    ///
    /// Returns an error if the given bytes is not a valid representation of a P-256 private key.
    pub fn from_private_bytes(private_key_bytes: &[u8]) -> Result<Self, Error> {
        EcKey::try_from_raw_bytes(C::ec_group(), private_key_bytes)
            .map(|eckey| Self {
                eckey,
                marker: PhantomData,
            })
            .map_err(|_| Error::ConversionFailed)
    }

    /// Serializes this private key as a big-endian integer, zero-padded to the size of key's group
    /// order and returns the result.
    pub fn to_bytes(&self) -> Vec<u8> {
        self.eckey.to_raw_bytes()
    }
}

impl<'a, C: Curve> From<&'a PrivateKey<C>> for PublicKey<C> {
    fn from(value: &'a PrivateKey<C>) -> Self {
        Self {
            eckey: value.eckey.clone(),
            marker: PhantomData,
        }
    }
}

/// A public key for elliptic curve.
#[derive(Clone, Debug)]
pub struct PublicKey<C: Curve> {
    /// An EcKey containing the public key
    eckey: EcKey,
    marker: PhantomData<C>,
}

impl<C: Curve> Eq for PublicKey<C> {}

impl<C: Curve> PartialEq for PublicKey<C> {
    fn eq(&self, other: &Self) -> bool {
        self.eckey.public_key_eq(&other.eckey)
    }
}

impl<C: Curve> PublicKey<C> {
    /// Converts this public key to its byte representation.
    pub fn to_vec(&self) -> Vec<u8> {
        self.eckey.to_vec()
    }

    /// Converts the given affine coordinates into a public key.
    pub fn from_affine_coordinates<const AFFINE_COORDINATE_SIZE: usize>(
        x: &[u8; AFFINE_COORDINATE_SIZE],
        y: &[u8; AFFINE_COORDINATE_SIZE],
    ) -> Result<Self, Error> {
        assert_eq!(AFFINE_COORDINATE_SIZE, C::AFFINE_COORDINATE_SIZE);
        EcKey::try_new_public_key_from_affine_coordinates(C::ec_group(), &x[..], &y[..])
            .map(|eckey| Self {
                eckey,
                marker: PhantomData,
            })
            .map_err(|_| Error::ConversionFailed)
    }

    /// Converts this public key to its affine coordinates.
    pub fn to_affine_coordinates<const AFFINE_COORDINATE_SIZE: usize>(
        &self,
    ) -> ([u8; AFFINE_COORDINATE_SIZE], [u8; AFFINE_COORDINATE_SIZE]) {
        assert_eq!(AFFINE_COORDINATE_SIZE, C::AFFINE_COORDINATE_SIZE);
        let (bn_x, bn_y) = self.eckey.to_affine_coordinates();

        let mut x_bytes_uninit = core::mem::MaybeUninit::<[u8; AFFINE_COORDINATE_SIZE]>::uninit();
        let mut y_bytes_uninit = core::mem::MaybeUninit::<[u8; AFFINE_COORDINATE_SIZE]>::uninit();
        // Safety:
        // - `BigNum` guarantees the validity of its ptr
        // - The size of `x/y_bytes_uninit` and the length passed to `BN_bn2bin_padded` are both
        //   `AFFINE_COORDINATE_SIZE`
        let (result_x, result_y) = unsafe {
            (
                bssl_sys::BN_bn2bin_padded(
                    x_bytes_uninit.as_mut_ptr() as *mut _,
                    AFFINE_COORDINATE_SIZE,
                    bn_x.as_ptr(),
                ),
                bssl_sys::BN_bn2bin_padded(
                    y_bytes_uninit.as_mut_ptr() as *mut _,
                    AFFINE_COORDINATE_SIZE,
                    bn_y.as_ptr(),
                ),
            )
        };
        assert_eq!(result_x, 1, "bssl_sys::BN_bn2bin_padded failed");
        assert_eq!(result_y, 1, "bssl_sys::BN_bn2bin_padded failed");

        // Safety: Fields initialized by `BN_bn2bin_padded` above.
        unsafe { (x_bytes_uninit.assume_init(), y_bytes_uninit.assume_init()) }
    }
}

impl<C: Curve> TryFrom<&[u8]> for PublicKey<C> {
    type Error = Error;

    fn try_from(value: &[u8]) -> Result<Self, Error> {
        EcKey::try_new_public_key_from_bytes(C::ec_group(), value)
            .map(|eckey| Self {
                eckey,
                marker: PhantomData,
            })
            .map_err(|_| Error::ConversionFailed)
    }
}

/// Shared secret derived from a Diffie-Hellman key exchange. Don't use the shared key directly,
/// rather use a KDF and also include the two public values as inputs.
pub struct SharedSecret<const SIZE: usize>(pub(crate) [u8; SIZE]);

impl<const SIZE: usize> SharedSecret<SIZE> {
    /// Gets a copy of the shared secret.
    pub fn to_bytes(&self) -> [u8; SIZE] {
        self.0
    }

    /// Gets a reference to the underlying data in this shared secret.
    pub fn as_bytes(&self) -> &[u8; SIZE] {
        &self.0
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used)]
mod tests {
    use crate::{
        ec::{Curve, P224, P256, P384, P521},
        ecdh::{PrivateKey, PublicKey},
        test_helpers::decode_hex,
    };

    #[test]
    fn p224_test_diffie_hellman() {
        // From wycheproof ecdh_secp224r1_ecpoint_test.json, tcId 1
        // sec1 public key manually extracted from the ASN encoded test data
        let public_key_bytes: [u8; 57] = decode_hex(concat!(
            "047d8ac211e1228eb094e285a957d9912e93deee433ed777440ae9fc719b01d0",
            "50dfbe653e72f39491be87fb1a2742daa6e0a2aada98bb1aca",
        ));
        let private_key_bytes: [u8; 28] =
            decode_hex("565577a49415ca761a0322ad54e4ad0ae7625174baf372c2816f5328");
        let expected_shared_secret: [u8; 28] =
            decode_hex("b8ecdb552d39228ee332bafe4886dbff272f7109edf933bc7542bd4f");

        let public_key: PublicKey<P224> = (&public_key_bytes[..]).try_into().unwrap();
        let private_key = PrivateKey::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        let actual_shared_secret = private_key.diffie_hellman(&public_key).unwrap();

        assert_eq!(actual_shared_secret.0, expected_shared_secret);
    }

    #[test]
    fn p256_test_diffie_hellman() {
        // From wycheproof ecdh_secp256r1_ecpoint_test.json, tcId 1
        // sec1 public key manually extracted from the ASN encoded test data
        let public_key_bytes: [u8; 65] = decode_hex(concat!(
            "0462d5bd3372af75fe85a040715d0f502428e07046868b0bfdfa61d731afe44f",
            "26ac333a93a9e70a81cd5a95b5bf8d13990eb741c8c38872b4a07d275a014e30cf",
        ));
        let private_key_bytes: [u8; 32] =
            decode_hex("0612465c89a023ab17855b0a6bcebfd3febb53aef84138647b5352e02c10c346");
        let expected_shared_secret: [u8; 32] =
            decode_hex("53020d908b0219328b658b525f26780e3ae12bcd952bb25a93bc0895e1714285");

        let public_key: PublicKey<P256> = (&public_key_bytes[..]).try_into().unwrap();
        let private_key = PrivateKey::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        let actual_shared_secret = private_key.diffie_hellman(&public_key).unwrap();

        assert_eq!(actual_shared_secret.0, expected_shared_secret);
    }

    #[test]
    fn p384_test_diffie_hellman() {
        // From wycheproof ecdh_secp384r1_ecpoint_test.json, tcId 1
        // sec1 public key manually extracted from the ASN encoded test data
        let public_key_bytes: [u8; 97] = decode_hex(concat!(
            "04790a6e059ef9a5940163183d4a7809135d29791643fc43a2f17ee8bf677ab8",
            "4f791b64a6be15969ffa012dd9185d8796d9b954baa8a75e82df711b3b56eadf",
            "f6b0f668c3b26b4b1aeb308a1fcc1c680d329a6705025f1c98a0b5e5bfcb163caa",
        ));
        let private_key_bytes: [u8; 48] = decode_hex(concat!(
            "766e61425b2da9f846c09fc3564b93a6f8603b7392c785165bf20da948c49fd1",
            "fb1dee4edd64356b9f21c588b75dfd81"
        ));
        let expected_shared_secret: [u8; 48] = decode_hex(concat!(
            "6461defb95d996b24296f5a1832b34db05ed031114fbe7d98d098f93859866e4",
            "de1e229da71fef0c77fe49b249190135"
        ));

        let public_key: PublicKey<P384> = (&public_key_bytes[..]).try_into().unwrap();
        let private_key = PrivateKey::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        let actual_shared_secret = private_key.diffie_hellman(&public_key).unwrap();

        assert_eq!(actual_shared_secret.0, expected_shared_secret);
    }

    #[test]
    fn p521_test_diffie_hellman() {
        // From wycheproof ecdh_secp521r1_ecpoint_test.json, tcId 1
        // sec1 public key manually extracted from the ASN encoded test data
        let public_key_bytes: [u8; 133] = decode_hex(concat!(
            "040064da3e94733db536a74a0d8a5cb2265a31c54a1da6529a198377fbd38575",
            "d9d79769ca2bdf2d4c972642926d444891a652e7f492337251adf1613cf30779",
            "99b5ce00e04ad19cf9fd4722b0c824c069f70c3c0e7ebc5288940dfa92422152",
            "ae4a4f79183ced375afb54db1409ddf338b85bb6dbfc5950163346bb63a90a70",
            "c5aba098f7",
        ));
        let private_key_bytes: [u8; 66] = decode_hex(concat!(
            "01939982b529596ce77a94bc6efd03e92c21a849eb4f87b8f619d506efc9bb22",
            "e7c61640c90d598f795b64566dc6df43992ae34a1341d458574440a7371f611c",
            "7dcd"
        ));
        let expected_shared_secret: [u8; 66] = decode_hex(concat!(
            "01f1e410f2c6262bce6879a3f46dfb7dd11d30eeee9ab49852102e1892201dd1",
            "0f27266c2cf7cbccc7f6885099043dad80ff57f0df96acf283fb090de53df95f",
            "7d87",
        ));

        let public_key: PublicKey<P521> = (&public_key_bytes[..]).try_into().unwrap();
        let private_key = PrivateKey::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        let actual_shared_secret = private_key.diffie_hellman(&public_key).unwrap();

        assert_eq!(actual_shared_secret.0, expected_shared_secret);
    }

    #[test]
    fn p224_generate_diffie_hellman_matches() {
        generate_diffie_hellman_matches::<P224, 28>()
    }

    #[test]
    fn p256_generate_diffie_hellman_matches() {
        generate_diffie_hellman_matches::<P256, 32>()
    }

    #[test]
    fn p384_generate_diffie_hellman_matches() {
        generate_diffie_hellman_matches::<P384, 48>()
    }

    #[test]
    fn p521_generate_diffie_hellman_matches() {
        generate_diffie_hellman_matches::<P521, 66>()
    }

    fn generate_diffie_hellman_matches<C: Curve, const OUTPUT_SIZE: usize>() {
        let private_key_1 = PrivateKey::<C>::generate();
        let private_key_2 = PrivateKey::<C>::generate();
        let public_key_1 = PublicKey::from(&private_key_1);
        let public_key_2 = PublicKey::from(&private_key_2);

        let diffie_hellman_1 = private_key_1
            .diffie_hellman::<OUTPUT_SIZE>(&public_key_2)
            .unwrap();
        let diffie_hellman_2 = private_key_2
            .diffie_hellman::<OUTPUT_SIZE>(&public_key_1)
            .unwrap();

        assert_eq!(diffie_hellman_1.to_bytes(), diffie_hellman_2.to_bytes());
    }

    #[test]
    fn p224_to_private_bytes() {
        let private_key_bytes: [u8; 28] =
            decode_hex("565577a49415ca761a0322ad54e4ad0ae7625174baf372c2816f5328");
        let private_key = PrivateKey::<P224>::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        assert_eq!(&private_key.to_bytes()[..], &private_key_bytes[..]);
    }

    #[test]
    fn p256_to_private_bytes() {
        let private_key_bytes: [u8; 32] =
            decode_hex("0612465c89a023ab17855b0a6bcebfd3febb53aef84138647b5352e02c10c346");
        let private_key = PrivateKey::<P256>::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        assert_eq!(&private_key.to_bytes()[..], &private_key_bytes[..]);
    }

    #[test]
    fn p384_to_private_bytes() {
        let private_key_bytes: [u8; 48] = decode_hex(concat!(
            "766e61425b2da9f846c09fc3564b93a6f8603b7392c785165bf20da948c49fd1",
            "fb1dee4edd64356b9f21c588b75dfd81"
        ));
        let private_key = PrivateKey::<P384>::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        assert_eq!(&private_key.to_bytes()[..], &private_key_bytes[..]);
    }

    #[test]
    fn p521_to_private_bytes() {
        let private_key_bytes: [u8; 66] = decode_hex(concat!(
            "01939982b529596ce77a94bc6efd03e92c21a849eb4f87b8f619d506efc9bb22",
            "e7c61640c90d598f795b64566dc6df43992ae34a1341d458574440a7371f611c",
            "7dcd",
        ));
        let private_key = PrivateKey::<P521>::from_private_bytes(&private_key_bytes)
            .expect("Input private key should be valid");
        assert_eq!(&private_key.to_bytes()[..], &private_key_bytes[..]);
    }

    #[test]
    fn p224_affine_coordinates_test() {
        affine_coordinates_test::<P224, { P224::AFFINE_COORDINATE_SIZE }>();
    }

    #[test]
    fn p256_affine_coordinates_test() {
        affine_coordinates_test::<P256, { P256::AFFINE_COORDINATE_SIZE }>();
    }

    #[test]
    fn p384_affine_coordinates_test() {
        affine_coordinates_test::<P384, { P384::AFFINE_COORDINATE_SIZE }>();
    }

    #[test]
    fn p521_affine_coordinates_test() {
        affine_coordinates_test::<P521, { P521::AFFINE_COORDINATE_SIZE }>();
    }

    fn affine_coordinates_test<C: Curve, const AFFINE_COORDINATE_SIZE: usize>() {
        let private_key = PrivateKey::<C>::generate();
        let public_key = PublicKey::from(&private_key);

        let (x, y) = public_key.to_affine_coordinates::<AFFINE_COORDINATE_SIZE>();

        let recreated_public_key = PublicKey::from_affine_coordinates(&x, &y);
        assert_eq!(public_key, recreated_public_key.unwrap());
    }
}
