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

use crate::ForeignTypeRef;

/// The BoringSSL implemented SHA-256 digest algorithm.
#[derive(Clone)]
pub struct Sha256 {}

/// The BoringSSL implemented SHA-512 digest algorithm.
#[derive(Clone)]
pub struct Sha512 {}

/// A reference to an [`Md`], which abstracts the details of a specific hash function allowing code
/// to deal with the concept of a "hash function" without needing to know exactly which hash function
/// it is.
pub struct MdRef;

unsafe impl ForeignTypeRef for MdRef {
    type CType = bssl_sys::EVP_MD;
}

/// Used internally to get a BoringSSL internal MD
pub trait Md {
    /// The output size of the hash operation.
    const OUTPUT_SIZE: usize;

    /// Gets a reference to a message digest algorithm to be used by the hkdf implementation.
    fn get_md() -> &'static MdRef;
}

impl Md for Sha256 {
    const OUTPUT_SIZE: usize = bssl_sys::SHA256_DIGEST_LENGTH as usize;

    fn get_md() -> &'static MdRef {
        // Safety:
        // - this always returns a valid pointer to an EVP_MD
        unsafe { MdRef::from_ptr(bssl_sys::EVP_sha256() as *mut _) }
    }
}

impl Md for Sha512 {
    const OUTPUT_SIZE: usize = bssl_sys::SHA512_DIGEST_LENGTH as usize;

    fn get_md() -> &'static MdRef {
        // Safety:
        // - this always returns a valid pointer to an EVP_MD
        unsafe { MdRef::from_ptr(bssl_sys::EVP_sha512() as *mut _) }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_sha256_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha256() as *mut _).as_ptr(),
                bssl_sys::EVP_sha256() as *mut _
            )
        }
    }

    #[test]
    fn test_sha512_c_type() {
        unsafe {
            assert_eq!(
                MdRef::from_ptr(bssl_sys::EVP_sha512() as *mut _).as_ptr(),
                bssl_sys::EVP_sha512() as *mut _
            )
        }
    }
}
