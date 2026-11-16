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

use alloc::boxed::Box;
use core::marker::PhantomData;

use rustls::{
    ConnectionTrafficSecrets, ContentType, Error, ProtocolVersion,
    crypto::cipher::{
        AeadKey, InboundOpaqueMessage, InboundPlainMessage, Iv, KeyBlockShape, MessageDecrypter,
        MessageEncrypter, OutboundOpaqueMessage, OutboundPlainMessage, PrefixedPayload,
        Tls12AeadAlgorithm, Tls13AeadAlgorithm, UnsupportedOperationError, make_tls12_aad,
        make_tls13_aad,
    },
};

use bssl_crypto::aead;

struct Tls12AdditionalData;

impl TlsAdditionalData for Tls12AdditionalData {
    #[inline]
    fn build_additional_data(
        seq: u64,
        typ: ContentType,
        version: ProtocolVersion,
        len: usize,
        ad_buf: &mut [u8],
    ) -> Option<&[u8]> {
        let ad_buf: &mut [u8; 13] = ad_buf.try_into().ok()?;
        ad_buf.copy_from_slice(&make_tls12_aad(seq, typ, version, len));
        Some(ad_buf)
    }
}

const NONCE_LEN: usize = 12;

enum AeadKind {
    Aes128Gcm,
    Aes256Gcm,
    Chacha20Poly1305,
}

trait AeadConstructible: aead::Aead + Sized {
    const KEY_LEN: usize;
    const TAG_LEN: usize;
    const KIND: AeadKind;
    fn new_with_key(key: &AeadKey) -> Option<Self>;
    fn nonce_from_buf(buf: &[u8]) -> Option<&Self::Nonce>;
    fn tag_from_buf(buf: &[u8]) -> Option<&Self::Tag>;
    fn tag_to_buf(tag: &Self::Tag) -> &[u8];
}

fn make_nonce_from_iv_seq(iv: [u8; NONCE_LEN], seq: u64) -> [u8; NONCE_LEN] {
    let mut buf = [0; NONCE_LEN];
    buf[4..].copy_from_slice(&seq.to_be_bytes());
    for i in 0..NONCE_LEN {
        buf[i] ^= iv[i];
    }
    buf
}

macro_rules! aead_ctor {
    ($($algo:ty : {
        key = $key_len:literal,
        tag = $tag_len:literal,
        kind = $kind:expr
    }),+) => { $(
        impl AeadConstructible for $algo {
            const KEY_LEN: usize = $key_len;
            const TAG_LEN: usize = $tag_len;
            const KIND: AeadKind = $kind;
            fn new_with_key(key: &AeadKey) -> Option<Self> {
                let key: [u8; $key_len] = key.as_ref().try_into().ok()?;
                Some(Self::new(&key))
            }
            fn nonce_from_buf(buf: &[u8]) -> Option<&[u8; NONCE_LEN]> {
                buf.try_into().ok()
            }
            fn tag_from_buf(buf: &[u8]) -> Option<&[u8; $tag_len]> {
                buf.try_into().ok()
            }
            fn tag_to_buf(tag: &[u8; $tag_len]) -> &[u8] {
                &*tag
            }
        }
    )+ };
    () => {};
    ($($algo:ty : $tt:tt),+,) => { aead_ctor!($($algo : $tt),+); }
}

aead_ctor!(
    aead::Aes128Gcm : {key = 16, tag = 16, kind = AeadKind::Aes128Gcm},
    aead::Aes256Gcm : {key = 32, tag = 16, kind = AeadKind::Aes256Gcm},
    aead::Chacha20Poly1305 : {key = 32, tag = 16, kind = AeadKind::Chacha20Poly1305},
);

trait Gcm: AeadConstructible {}
impl Gcm for aead::Aes128Gcm {}
impl Gcm for aead::Aes256Gcm {}

trait Chacha20: AeadConstructible {}
impl Chacha20 for aead::Chacha20Poly1305 {}

// ===================================================
// TLS 1.2 AES GCM cipher
// ===================================================

enum MaybeValidByteArray<const N: usize> {
    Valid([u8; N]),
    Invalid,
}

impl<const N: usize> From<&'_ [u8]> for MaybeValidByteArray<N> {
    fn from(value: &[u8]) -> Self {
        if let Ok(value) = value.try_into() {
            Self::Valid(value)
        } else {
            Self::Invalid
        }
    }
}

struct Tls12GcmMessageEncrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<4>,
    explicit_nonce: MaybeValidByteArray<8>,
    _p: PhantomData<fn() -> (A, AD)>,
}

trait TlsAdditionalData {
    fn build_additional_data(
        seq: u64,
        typ: ContentType,
        version: ProtocolVersion,
        len: usize,
        ad_buf: &mut [u8],
    ) -> Option<&[u8]>;
}

const TLS12_AAD_SIZE: usize = 13;
const TLS12_GCM_EXTRA_NONCE_SIZE: usize = 8;

#[inline]
fn gcm_iv(iv: &[u8; 4], extra: &[u8; TLS12_GCM_EXTRA_NONCE_SIZE]) -> [u8; NONCE_LEN] {
    let mut gcm_iv = [0; NONCE_LEN];
    gcm_iv[..4].copy_from_slice(iv);
    gcm_iv[4..].copy_from_slice(extra);
    gcm_iv
}

const GCM_EXPLICIT_NONCE_LEN: usize = 8;

impl<A: Gcm, AD: TlsAdditionalData> MessageEncrypter for Tls12GcmMessageEncrypter<A, AD> {
    fn encrypt(
        &mut self,
        msg: OutboundPlainMessage<'_>,
        seq: u64,
    ) -> Result<OutboundOpaqueMessage, Error> {
        let payload_len = msg.payload.len();
        let mut payload = PrefixedPayload::with_capacity(self.encrypted_payload_len(payload_len));
        let aead = A::new_with_key(&self.key).ok_or(Error::EncryptError)?;
        let (MaybeValidByteArray::Valid(iv), MaybeValidByteArray::Valid(explicit_nonce)) =
            (&self.iv, &self.explicit_nonce)
        else {
            return Err(Error::DecryptError);
        };
        let write_iv = gcm_iv(iv, explicit_nonce);
        let nonce = make_nonce_from_iv_seq(write_iv, seq);
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::EncryptError)?;
        let mut ad_buf = [0; TLS12_AAD_SIZE];
        let ad = AD::build_additional_data(seq, msg.typ, msg.version, payload_len, &mut ad_buf)
            .ok_or(Error::EncryptError)?;
        payload.extend_from_slice(&nonce.as_ref()[4..]);
        payload.extend_from_chunks(&msg.payload);
        let tag = aead.seal_in_place(
            &nonce,
            &mut payload.as_mut()[GCM_EXPLICIT_NONCE_LEN..GCM_EXPLICIT_NONCE_LEN + payload_len],
            ad,
        );
        payload.extend_from_slice(A::tag_to_buf(&tag));
        Ok(OutboundOpaqueMessage::new(msg.typ, msg.version, payload))
    }

    fn encrypted_payload_len(&self, payload_len: usize) -> usize {
        payload_len + GCM_EXPLICIT_NONCE_LEN + A::TAG_LEN
    }
}

struct Tls12GcmMessageDecrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<4>,
    _p: PhantomData<fn() -> (A, AD)>,
}

impl<A: Gcm, AD: TlsAdditionalData> MessageDecrypter for Tls12GcmMessageDecrypter<A, AD> {
    fn decrypt<'a>(
        &mut self,
        mut msg: InboundOpaqueMessage<'a>,
        seq: u64,
    ) -> Result<InboundPlainMessage<'a>, Error> {
        let aead = A::new_with_key(&self.key).ok_or(Error::DecryptError)?;
        let InboundOpaqueMessage {
            typ,
            version,
            ref mut payload,
        } = msg;
        let Some((explicit_nonce, payload)) = payload.split_at_mut_checked(GCM_EXPLICIT_NONCE_LEN)
        else {
            return Err(Error::DecryptError);
        };
        let Ok(extra) = explicit_nonce.try_into() else {
            unreachable!("length should be 8")
        };
        let MaybeValidByteArray::Valid(iv) = &self.iv else {
            return Err(Error::DecryptError);
        };
        let nonce = gcm_iv(iv, &extra);
        let Some(ctxt_len) = payload.len().checked_sub(A::TAG_LEN) else {
            return Err(Error::DecryptError);
        };
        // TODO(@xfding) Should we check the fragment size?
        let Some((ciphertext, tag)) = payload.split_at_mut_checked(ctxt_len) else {
            return Err(Error::DecryptError);
        };
        let mut ad_buf = [0; TLS12_AAD_SIZE];
        let ad = AD::build_additional_data(seq, typ, version, ctxt_len, &mut ad_buf)
            .ok_or(Error::DecryptError)?;
        let tag = A::tag_from_buf(tag).ok_or(Error::DecryptError)?;
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::DecryptError)?;
        aead.open_in_place(&nonce, ciphertext, &tag, ad)
            .map_err(|_| Error::DecryptError)?;
        Ok(msg.into_plain_message_range(GCM_EXPLICIT_NONCE_LEN..GCM_EXPLICIT_NONCE_LEN + ctxt_len))
    }
}

struct Tls12GcmAeadAlgorithm<A>(PhantomData<fn() -> A>);

impl<A> Tls12GcmAeadAlgorithm<A> {}

impl<A: 'static + Gcm> Tls12AeadAlgorithm for Tls12GcmAeadAlgorithm<A> {
    fn encrypter(&self, key: AeadKey, iv: &[u8], extra: &[u8]) -> Box<dyn MessageEncrypter> {
        Box::new(Tls12GcmMessageEncrypter::<A, Tls12AdditionalData> {
            key,
            iv: iv.into(),
            explicit_nonce: extra.into(),
            _p: PhantomData,
        })
    }

    fn decrypter(&self, key: AeadKey, iv: &[u8]) -> Box<dyn MessageDecrypter> {
        Box::new(Tls12GcmMessageDecrypter::<A, Tls12AdditionalData> {
            key,
            iv: iv.into(),
            _p: PhantomData,
        })
    }

    fn key_block_shape(&self) -> KeyBlockShape {
        KeyBlockShape {
            enc_key_len: A::KEY_LEN,
            fixed_iv_len: 4,
            explicit_nonce_len: 8,
        }
    }

    fn extract_keys(
        &self,
        key: AeadKey,
        iv: &[u8],
        explicit: &[u8],
    ) -> Result<ConnectionTrafficSecrets, UnsupportedOperationError> {
        let iv = Iv::new(gcm_iv(
            iv.try_into().map_err(|_| UnsupportedOperationError)?,
            explicit.try_into().map_err(|_| UnsupportedOperationError)?,
        ));
        Ok(match A::KEY_LEN {
            16 => ConnectionTrafficSecrets::Aes128Gcm { key, iv },
            32 => ConnectionTrafficSecrets::Aes256Gcm { key, iv },
            _ => unreachable!(),
        })
    }
}

/// TLS 1.2 AEAD scheme AES 128 GCM
pub(crate) const TLS12_AES_128_GCM_AEAD: &'static dyn Tls12AeadAlgorithm =
    &Tls12GcmAeadAlgorithm::<aead::Aes128Gcm>(PhantomData);

/// TLS 1.2 AEAD scheme AES 256 GCM
pub(crate) const TLS12_AES_256_GCM_AEAD: &'static dyn Tls12AeadAlgorithm =
    &Tls12GcmAeadAlgorithm::<aead::Aes256Gcm>(PhantomData);

// ===================================================
// TLS 1.2 ChaCha20 cipher
// ===================================================

struct Tls12Chacha20PolyMessageEncrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<NONCE_LEN>,
    _p: PhantomData<fn() -> (A, AD)>,
}

impl<A: Chacha20, AD: TlsAdditionalData> MessageEncrypter
    for Tls12Chacha20PolyMessageEncrypter<A, AD>
{
    fn encrypt(
        &mut self,
        msg: OutboundPlainMessage<'_>,
        seq: u64,
    ) -> Result<OutboundOpaqueMessage, Error> {
        let payload_len = msg.payload.len();
        let mut payload = PrefixedPayload::with_capacity(self.encrypted_payload_len(payload_len));
        let aead = A::new_with_key(&self.key).ok_or(Error::EncryptError)?;
        let MaybeValidByteArray::Valid(iv) = self.iv else {
            return Err(Error::EncryptError);
        };
        let nonce = make_nonce_from_iv_seq(iv, seq);
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::EncryptError)?;
        let mut ad_buf = [0; TLS12_AAD_SIZE];
        let ad = AD::build_additional_data(seq, msg.typ, msg.version, payload_len, &mut ad_buf)
            .ok_or(Error::EncryptError)?;
        payload.extend_from_chunks(&msg.payload);
        let tag = aead.seal_in_place(&nonce, &mut payload.as_mut()[..payload_len], ad);
        payload.extend_from_slice(A::tag_to_buf(&tag));
        Ok(OutboundOpaqueMessage::new(msg.typ, msg.version, payload))
    }

    fn encrypted_payload_len(&self, payload_len: usize) -> usize {
        payload_len + A::TAG_LEN
    }
}

struct Tls12Chacha20PolyMessageDecrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<NONCE_LEN>,
    _p: PhantomData<fn() -> (A, AD)>,
}

impl<A: Chacha20, AD: TlsAdditionalData> MessageDecrypter
    for Tls12Chacha20PolyMessageDecrypter<A, AD>
{
    fn decrypt<'a>(
        &mut self,
        mut msg: InboundOpaqueMessage<'a>,
        seq: u64,
    ) -> Result<InboundPlainMessage<'a>, Error> {
        let aead = A::new_with_key(&self.key).ok_or(Error::DecryptError)?;
        let MaybeValidByteArray::Valid(iv) = self.iv else {
            return Err(Error::DecryptError);
        };
        let nonce = make_nonce_from_iv_seq(iv, seq);
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::DecryptError)?;
        let mut ad_buf = [0; TLS12_AAD_SIZE];
        let Some(ctxt_len) = msg.payload.len().checked_sub(A::TAG_LEN) else {
            return Err(Error::DecryptError);
        };
        let ad = AD::build_additional_data(seq, msg.typ, msg.version, ctxt_len, &mut ad_buf)
            .ok_or(Error::DecryptError)?;
        // TODO(@xfding) Should we check the fragment size?
        let Some((ciphertext, tag)) = msg.payload.split_at_mut_checked(ctxt_len) else {
            return Err(Error::DecryptError);
        };
        let tag = A::tag_from_buf(tag).ok_or(Error::DecryptError)?;
        aead.open_in_place(&nonce, ciphertext, tag, ad)
            .map_err(|_| Error::DecryptError)?;
        Ok(msg.into_plain_message_range(0..ctxt_len))
    }
}

struct Tls12Chacha20AeadAlgorithm<A>(PhantomData<fn() -> A>);

impl<A: 'static + Chacha20> Tls12AeadAlgorithm for Tls12Chacha20AeadAlgorithm<A> {
    fn encrypter(&self, key: AeadKey, iv: &[u8], _extra: &[u8]) -> Box<dyn MessageEncrypter> {
        Box::new(
            Tls12Chacha20PolyMessageEncrypter::<A, Tls12AdditionalData> {
                key,
                iv: iv.into(),
                _p: PhantomData,
            },
        )
    }

    fn decrypter(&self, key: AeadKey, iv: &[u8]) -> Box<dyn MessageDecrypter> {
        Box::new(
            Tls12Chacha20PolyMessageDecrypter::<A, Tls12AdditionalData> {
                key,
                iv: iv.into(),
                _p: PhantomData,
            },
        )
    }

    fn key_block_shape(&self) -> KeyBlockShape {
        KeyBlockShape {
            enc_key_len: 32,
            fixed_iv_len: 12,
            explicit_nonce_len: 0,
        }
    }

    fn extract_keys(
        &self,
        key: AeadKey,
        iv: &[u8],
        _explicit: &[u8],
    ) -> Result<ConnectionTrafficSecrets, UnsupportedOperationError> {
        let iv = Iv::new(iv.try_into().map_err(|_| UnsupportedOperationError)?);
        Ok(ConnectionTrafficSecrets::Chacha20Poly1305 { key, iv })
    }

    fn fips(&self) -> bool {
        false
    }
}

/// TLS 1.2 AEAD cipher with Chacha202-Poly1305
pub(crate) static TLS12_CHACHA20_POLY1305_AEAD: &'static dyn Tls12AeadAlgorithm =
    &Tls12Chacha20AeadAlgorithm::<aead::Chacha20Poly1305>(PhantomData);

// ===================================================
// TLS 1.3 AEAD cipher family
// ===================================================

struct Tls13AdditionalData;

impl TlsAdditionalData for Tls13AdditionalData {
    #[inline]
    fn build_additional_data<'a>(
        _seq: u64,
        _typ: ContentType,
        _version: ProtocolVersion,
        len: usize,
        ad_buf: &'a mut [u8],
    ) -> Option<&'a [u8]> {
        let ad_buf: &mut [u8; 5] = ad_buf.try_into().ok()?;
        ad_buf.copy_from_slice(&make_tls13_aad(len));
        Some(ad_buf)
    }
}

/// TLS 1.3 Cipher suite TLS13_AES_128_GCM
pub(crate) const TLS13_AES_128_GCM: &'static dyn Tls13AeadAlgorithm =
    &Tls13AeadAlgorithmImpl::<aead::Aes128Gcm>(PhantomData);

/// TLS 1.3 Cipher suite TLS13_AES_256_GCM
pub(crate) const TLS13_AES_256_GCM: &'static dyn Tls13AeadAlgorithm =
    &Tls13AeadAlgorithmImpl::<aead::Aes256Gcm>(PhantomData);

/// TLS 1.3 Cipher suite TLS13_CHACHA20_POLY1305
pub(crate) const TLS13_CHACHA20_POLY1305: &'static dyn Tls13AeadAlgorithm =
    &Tls13AeadAlgorithmImpl::<aead::Chacha20Poly1305>(PhantomData);

const TLS13_AAD_SIZE: usize = 5;

struct Tls13MessageEncrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<NONCE_LEN>,
    _p: PhantomData<fn() -> (A, AD)>,
}

impl<A: AeadConstructible, AD: TlsAdditionalData> MessageEncrypter
    for Tls13MessageEncrypter<A, AD>
{
    fn encrypt(
        &mut self,
        msg: OutboundPlainMessage<'_>,
        seq: u64,
    ) -> Result<OutboundOpaqueMessage, Error> {
        let plaintxt_len = msg.payload.len();
        let payload_len = self.encrypted_payload_len(plaintxt_len);
        let mut payload = PrefixedPayload::with_capacity(payload_len);
        let aead = A::new_with_key(&self.key).ok_or(Error::EncryptError)?;
        let MaybeValidByteArray::Valid(iv) = self.iv else {
            return Err(Error::EncryptError);
        };
        let nonce = make_nonce_from_iv_seq(iv, seq);
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::EncryptError)?;
        let mut ad_buf = [0; TLS13_AAD_SIZE];
        let ad = AD::build_additional_data(seq, msg.typ, msg.version, payload_len, &mut ad_buf)
            .ok_or(Error::EncryptError)?;
        // Layout: [<..PAYLOAD, TYPE (size 1)>, TAG (size TAG_LEN)]
        payload.extend_from_chunks(&msg.payload);
        payload.extend_from_slice(&msg.typ.to_array());
        let tag = aead.seal_in_place(&nonce, &mut payload.as_mut()[0..plaintxt_len + 1], ad);
        payload.extend_from_slice(A::tag_to_buf(&tag));
        Ok(OutboundOpaqueMessage::new(
            ContentType::ApplicationData,
            ProtocolVersion::TLSv1_2,
            payload,
        ))
    }

    fn encrypted_payload_len(&self, payload_len: usize) -> usize {
        payload_len + A::TAG_LEN + 1
    }
}

struct Tls13MessageDecrypter<A, AD> {
    key: AeadKey,
    iv: MaybeValidByteArray<NONCE_LEN>,
    _p: PhantomData<fn() -> (A, AD)>,
}

impl<A: AeadConstructible, AD: TlsAdditionalData> MessageDecrypter
    for Tls13MessageDecrypter<A, AD>
{
    fn decrypt<'a>(
        &mut self,
        mut msg: InboundOpaqueMessage<'a>,
        seq: u64,
    ) -> Result<InboundPlainMessage<'a>, Error> {
        let aead = A::new_with_key(&self.key).ok_or(Error::DecryptError)?;
        let MaybeValidByteArray::Valid(iv) = self.iv else {
            return Err(Error::DecryptError);
        };
        let nonce = make_nonce_from_iv_seq(iv, seq);
        let nonce = A::nonce_from_buf(&nonce).ok_or(Error::DecryptError)?;
        let InboundOpaqueMessage {
            typ,
            version,
            ref mut payload,
        } = msg;
        // Layout: [<..PAYLOAD, TYPE (size 1)>, TAG (size TAG_LEN)]
        let total_len = payload.len();
        let Some(ctxt_len) = total_len.checked_sub(A::TAG_LEN) else {
            return Err(Error::DecryptError);
        };
        let Some((ciphertext, tag)) = payload.split_at_mut_checked(ctxt_len) else {
            return Err(Error::DecryptError);
        };
        let mut ad_buf = [0; TLS13_AAD_SIZE];
        let ad = AD::build_additional_data(seq, typ, version, total_len, &mut ad_buf)
            .ok_or(Error::DecryptError)?;
        let tag = A::tag_from_buf(tag).ok_or(Error::DecryptError)?;
        aead.open_in_place(&nonce, ciphertext, tag, ad)
            .map_err(|_| Error::DecryptError)?;
        payload.truncate(ctxt_len);
        msg.into_tls13_unpadded_message()
    }
}

struct Tls13AeadAlgorithmImpl<A>(PhantomData<fn() -> A>);

impl<A: 'static + AeadConstructible> Tls13AeadAlgorithm for Tls13AeadAlgorithmImpl<A> {
    fn encrypter(&self, key: AeadKey, iv: Iv) -> Box<dyn MessageEncrypter> {
        Box::new(Tls13MessageEncrypter::<A, Tls13AdditionalData> {
            key,
            iv: iv.as_ref().into(),
            _p: PhantomData,
        })
    }

    fn decrypter(&self, key: AeadKey, iv: Iv) -> Box<dyn MessageDecrypter> {
        Box::new(Tls13MessageDecrypter::<A, Tls13AdditionalData> {
            key,
            iv: iv.as_ref().into(),
            _p: PhantomData,
        })
    }

    fn key_len(&self) -> usize {
        A::KEY_LEN
    }

    fn extract_keys(
        &self,
        key: AeadKey,
        iv: Iv,
    ) -> Result<ConnectionTrafficSecrets, UnsupportedOperationError> {
        Ok(match A::KIND {
            AeadKind::Aes128Gcm => ConnectionTrafficSecrets::Aes128Gcm { key, iv },
            AeadKind::Aes256Gcm => ConnectionTrafficSecrets::Aes256Gcm { key, iv },
            AeadKind::Chacha20Poly1305 => ConnectionTrafficSecrets::Chacha20Poly1305 { key, iv },
        })
    }

    fn fips(&self) -> bool {
        false
    }
}
