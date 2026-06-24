// Copyright 2023 The BoringSSL Authors
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

extern crate alloc;

use crate::{CSlice, CSliceMut};
use alloc::{vec, vec::Vec};
use bssl_sys::EVP_CIPHER;
use core::marker::PhantomData;

/// AES-CTR stream cipher operations.
pub mod aes_ctr;

/// AES-CBC stream cipher operations.
pub mod aes_cbc;

/// Error returned in the event of an unsuccessful cipher operation.
#[derive(Debug)]
pub struct CipherError;

/// Synchronous stream cipher trait.
pub trait StreamCipher {
    /// The byte array key type which specifies the size of the key used to instantiate the cipher.
    type Key: AsRef<[u8]>;

    /// The byte array nonce type which specifies the size of the nonce used in the cipher
    /// operations.
    type Nonce: AsRef<[u8]>;

    /// Instantiate a new instance of a stream cipher from a `key` and `iv`.
    fn new(key: &Self::Key, iv: &Self::Nonce) -> Self;

    /// Applies the cipher keystream to `buffer` in place, returning CipherError on an unsuccessful
    /// operation.
    fn apply_keystream(&mut self, buffer: &mut [u8]) -> Result<(), CipherError>;
}

/// Synchronous block cipher trait.
pub trait BlockCipher {
    /// The byte array key type which specifies the size of the key used to instantiate the cipher.
    type Key: AsRef<[u8]>;

    /// The byte array nonce type which specifies the size of the nonce used in the cipher
    /// operations.
    type Nonce: AsRef<[u8]>;

    /// Instantiate a new instance of a block cipher for encryption from a `key` and `iv`.
    fn new_encrypt(key: &Self::Key, iv: &Self::Nonce) -> Self;

    /// Instantiate a new instance of a block cipher for decryption from a `key` and `iv`.
    fn new_decrypt(key: &Self::Key, iv: &Self::Nonce) -> Self;

    /// Encrypts the given data in `buffer`, and returns the result (with padding) in a newly
    /// allocated vector, or a [`CipherError`] if the operation was unsuccessful.
    fn encrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError>;

    /// Decrypts the given data in a `buffer`, and returns the result (with padding removed) in a
    /// newly allocated vector, or a [`CipherError`] if the operation was unsuccessful.
    fn decrypt_padded(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError>;
}

/// A cipher type, where `Key` is the size of the Key and `Nonce` is the size of the nonce or IV.
/// This must only be exposed publicly by types who ensure that `Key` is the correct size for the
/// given CipherType. This can be checked via `bssl_sys::EVP_CIPHER_key_length`.
trait EvpCipherType {
    type Key: AsRef<[u8]>;
    type Nonce: AsRef<[u8]>;
    fn evp_cipher() -> *const EVP_CIPHER;
}

struct EvpAes128Ctr;
impl EvpCipherType for EvpAes128Ctr {
    type Key = [u8; 16];
    type Nonce = [u8; 16];
    fn evp_cipher() -> *const EVP_CIPHER {
        // Safety:
        // - this just returns a constant value
        unsafe { bssl_sys::EVP_aes_128_ctr() }
    }
}

struct EvpAes256Ctr;
impl EvpCipherType for EvpAes256Ctr {
    type Key = [u8; 32];
    type Nonce = [u8; 16];
    fn evp_cipher() -> *const EVP_CIPHER {
        // Safety:
        // - this just returns a constant value
        unsafe { bssl_sys::EVP_aes_256_ctr() }
    }
}

struct EvpAes128Cbc;
impl EvpCipherType for EvpAes128Cbc {
    type Key = [u8; 16];
    type Nonce = [u8; 16];
    fn evp_cipher() -> *const EVP_CIPHER {
        // Safety:
        // - this just returns a constant value
        unsafe { bssl_sys::EVP_aes_128_cbc() }
    }
}

struct EvpAes256Cbc;
impl EvpCipherType for EvpAes256Cbc {
    type Key = [u8; 32];
    type Nonce = [u8; 16];
    fn evp_cipher() -> *const EVP_CIPHER {
        // Safety:
        // - this just returns a constant value
        unsafe { bssl_sys::EVP_aes_256_cbc() }
    }
}

enum CipherInitPurpose {
    Encrypt,
    Decrypt,
}

/// Internal cipher implementation which wraps `EVP_CIPHER_*`
struct Cipher<C: EvpCipherType> {
    ctx: *mut bssl_sys::EVP_CIPHER_CTX,
    _marker: PhantomData<C>,
}

impl<C: EvpCipherType> Cipher<C> {
    fn new(key: &C::Key, iv: &C::Nonce, purpose: CipherInitPurpose) -> Self {
        // Safety:
        // - Panics on allocation failure.
        let ctx = unsafe { bssl_sys::EVP_CIPHER_CTX_new() };
        assert!(!ctx.is_null());

        let key_cslice = CSlice::from(key.as_ref());
        let iv_cslice = CSlice::from(iv.as_ref());

        // Safety:
        // - Key size and iv size must be properly set by the higher level wrapper types.
        // - Panics on allocation failure.
        let result = match purpose {
            CipherInitPurpose::Encrypt => unsafe {
                bssl_sys::EVP_EncryptInit_ex(
                    ctx,
                    C::evp_cipher(),
                    core::ptr::null_mut(),
                    key_cslice.as_ptr(),
                    iv_cslice.as_ptr(),
                )
            },
            CipherInitPurpose::Decrypt => unsafe {
                bssl_sys::EVP_DecryptInit_ex(
                    ctx,
                    C::evp_cipher(),
                    core::ptr::null_mut(),
                    key_cslice.as_ptr(),
                    iv_cslice.as_ptr(),
                )
            },
        };
        assert_eq!(result, 1);

        Self {
            ctx,
            _marker: Default::default(),
        }
    }

    fn cipher_mode(&self) -> u32 {
        // Safety:
        // - The cipher context is initialized with `EVP_EncryptInit_ex` in `new`
        unsafe { bssl_sys::EVP_CIPHER_CTX_mode(self.ctx) }
    }

    fn apply_keystream_in_place(&mut self, buffer: &mut [u8]) -> Result<(), CipherError> {
        // WARNING: This is not safe to reuse for the CBC mode of operation since it is applying
        // the key stream in-place.
        assert_eq!(
            self.cipher_mode(),
            bssl_sys::EVP_CIPH_CTR_MODE as u32,
            "Cannot use apply_keystream_in_place for non-CTR modes"
        );
        let mut cslice_buf_mut = CSliceMut::from(buffer);
        let mut out_len = 0;

        // Safety: the input and output buffer bounds are passed into `EVP_EncryptUpdate_ex`.
        let result = unsafe {
            bssl_sys::EVP_EncryptUpdate_ex(
                self.ctx,
                cslice_buf_mut.as_mut_ptr(),
                &mut out_len,
                cslice_buf_mut.len(),
                cslice_buf_mut.as_mut_ptr(),
                cslice_buf_mut.len(),
            )
        };
        if result == 1 {
            assert_eq!(out_len, cslice_buf_mut.len());
            Ok(())
        } else {
            Err(CipherError)
        }
    }

    #[allow(clippy::expect_used)]
    fn encrypt(self, buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Safety: self.ctx is initialized with a cipher in `new()`.
        let block_size_u32 = unsafe { bssl_sys::EVP_CIPHER_CTX_block_size(self.ctx) };
        let block_size: usize = block_size_u32
            .try_into()
            .expect("Block size should always fit in usize");
        let max_encrypt_total_output_size = buffer.len() + block_size;
        let mut output_vec = vec![0_u8; max_encrypt_total_output_size];
        // EncryptUpdate block
        let update_out_len = {
            let mut cslice_out_buf_mut = CSliceMut::from(&mut output_vec[..]);
            let mut update_out_len = 0;

            let cslice_in_buf = CSlice::from(buffer);

            // Safety: the input and output buffer bounds are passed into `EVP_EncryptUpdate_ex`.
            let update_result = unsafe {
                bssl_sys::EVP_EncryptUpdate_ex(
                    self.ctx,
                    cslice_out_buf_mut.as_mut_ptr(),
                    &mut update_out_len,
                    cslice_out_buf_mut.len(),
                    cslice_in_buf.as_ptr(),
                    cslice_in_buf.len(),
                )
            };
            if update_result != 1 {
                return Err(CipherError);
            }
            update_out_len
        };

        // EncryptFinal block
        {
            // Slice indexing here will not panic because we ensured `output_vec` is larger than
            // what `EncryptUpdate` will write.
            #[allow(clippy::indexing_slicing)]
            let mut cslice_finalize_buf_mut = CSliceMut::from(&mut output_vec[update_out_len..]);
            let mut final_out_len = 0;
            // Safety: the output buffer bounds are passed into `EVP_EncryptFinal_ex2`.
            let final_result = unsafe {
                bssl_sys::EVP_EncryptFinal_ex2(
                    self.ctx,
                    cslice_finalize_buf_mut.as_mut_ptr(),
                    &mut final_out_len,
                    cslice_finalize_buf_mut.len(),
                )
            };
            if final_result == 1 {
                output_vec.truncate(update_out_len + final_out_len)
            } else {
                return Err(CipherError);
            }
        }
        Ok(output_vec)
    }

    #[allow(clippy::expect_used)]
    fn decrypt(self, in_buffer: &[u8]) -> Result<Vec<u8>, CipherError> {
        // Safety: self.ctx is initialized with a cipher in `new()`.
        let mut output_vec = vec![0_u8; in_buffer.len()];

        // DecryptUpdate block
        let update_out_len = {
            let mut cslice_out_buf_mut = CSliceMut::from(&mut output_vec[..]);
            let mut update_out_len = 0;

            let cslice_in_buf = CSlice::from(in_buffer);

            // Safety: the input and output buffer bounds are passed into `EVP_DecryptUpdate_ex`.
            let update_result = unsafe {
                bssl_sys::EVP_DecryptUpdate_ex(
                    self.ctx,
                    cslice_out_buf_mut.as_mut_ptr(),
                    &mut update_out_len,
                    cslice_out_buf_mut.len(),
                    cslice_in_buf.as_ptr(),
                    cslice_in_buf.len(),
                )
            };
            if update_result != 1 {
                return Err(CipherError);
            }
            update_out_len
        };

        // DecryptFinal block
        {
            // Slice indexing here will not panic because we ensured `output_vec` is larger than
            // what `DecryptUpdate` will write.
            #[allow(clippy::indexing_slicing)]
            let mut cslice_final_buf_mut = CSliceMut::from(&mut output_vec[update_out_len..]);
            let mut final_out_len = 0;
            // Safety: the output buffer bounds are passed into `EVP_DecryptFinal_ex2`.
            let final_result = unsafe {
                bssl_sys::EVP_DecryptFinal_ex2(
                    self.ctx,
                    cslice_final_buf_mut.as_mut_ptr(),
                    &mut final_out_len,
                    cslice_final_buf_mut.len(),
                )
            };

            if final_result == 1 {
                output_vec.truncate(update_out_len + final_out_len)
            } else {
                return Err(CipherError);
            }
        }
        Ok(output_vec)
    }
}

impl<C: EvpCipherType> Drop for Cipher<C> {
    fn drop(&mut self) {
        // Safety:
        // - `self.ctx` was allocated by `EVP_CIPHER_CTX_new` and has not yet been freed.
        unsafe { bssl_sys::EVP_CIPHER_CTX_free(self.ctx) }
    }
}

#[cfg(test)]
mod test {
    use crate::cipher::{CipherInitPurpose, EvpAes128Cbc, EvpAes128Ctr};

    use super::Cipher;

    #[test]
    fn test_cipher_mode() {
        assert_eq!(
            Cipher::<EvpAes128Ctr>::new(&[0; 16], &[0; 16], CipherInitPurpose::Encrypt)
                .cipher_mode(),
            bssl_sys::EVP_CIPH_CTR_MODE as u32
        );

        assert_eq!(
            Cipher::<EvpAes128Cbc>::new(&[0; 16], &[0; 16], CipherInitPurpose::Encrypt)
                .cipher_mode(),
            bssl_sys::EVP_CIPH_CBC_MODE as u32
        );
    }

    #[should_panic]
    #[test]
    fn test_apply_keystream_on_cbc() {
        let mut cipher =
            Cipher::<EvpAes128Cbc>::new(&[0; 16], &[0; 16], CipherInitPurpose::Encrypt);
        let mut buf = [0; 16];
        let _ = cipher.apply_keystream_in_place(&mut buf); // This should panic
    }
}
