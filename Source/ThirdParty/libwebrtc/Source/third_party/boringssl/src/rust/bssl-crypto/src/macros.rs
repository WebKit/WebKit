/* Copyright (c) 2024, Google Inc.
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

// Generates a hash function from init/update/final-style FFI functions. Rust
// doesn't accept function pointers as a generic arguments so this is the only
// mechanism to avoid duplicating the code.
//
// The name is prefixed with "unsafe_" because it contains unsafe blocks.
//
// Safety: see the "Safety" sections within about the requirements for the
// functions named in the macro parameters.
macro_rules! unsafe_iuf_algo {
    ($name:ident, $output_len:expr, $evp_md:ident, $one_shot:ident, $init:ident, $update:ident, $final_func:ident) => {
        impl Algorithm for $name {
            const OUTPUT_LEN: usize = $output_len as usize;

            fn get_md(_: sealed::Sealed) -> &'static MdRef {
                // Safety:
                // - this always returns a valid pointer to an EVP_MD.
                unsafe { MdRef::from_ptr(bssl_sys::$evp_md() as *mut _) }
            }

            fn hash_to_vec(input: &[u8]) -> Vec<u8> {
                Self::hash(input).as_slice().to_vec()
            }
        }

        impl $name {
            /// Digest `input` in a single operation.
            pub fn hash(input: &[u8]) -> [u8; $output_len] {
                // Safety: it is assumed that `$one_shot` indeed writes
                // `$output_len` bytes.
                unsafe {
                    crate::with_output_array(|out, _| {
                        bssl_sys::$one_shot(input.as_ffi_ptr(), input.len(), out);
                    })
                }
            }

            /// Create a new context for incremental hashing.
            pub fn new() -> Self {
                unsafe {
                    Self {
                        ctx: crate::initialized_struct(|ctx| {
                            // Safety: type checking will ensure that `ctx` is the
                            // correct type for `$init` to write into.
                            bssl_sys::$init(ctx);
                        }),
                    }
                }
            }

            /// Hash the contents of `input`.
            pub fn update(&mut self, input: &[u8]) {
                // Safety: arguments point to a valid buffer.
                unsafe {
                    bssl_sys::$update(&mut self.ctx, input.as_ffi_void_ptr(), input.len());
                }
            }

            /// Finish the hashing and return the digest.
            pub fn digest(mut self) -> [u8; $output_len] {
                // Safety: it is assumed that `$final_func` indeed writes
                // `$output_len` bytes.
                unsafe {
                    crate::with_output_array(|out, _| {
                        bssl_sys::$final_func(out, &mut self.ctx);
                    })
                }
            }
        }

        impl From<$name> for [u8; $output_len] {
            fn from(ctx: $name) -> [u8; $output_len] {
                ctx.digest()
            }
        }

        impl From<$name> for alloc::vec::Vec<u8> {
            fn from(ctx: $name) -> alloc::vec::Vec<u8> {
                ctx.digest().into()
            }
        }

        #[cfg(feature = "std")]
        impl std::io::Write for $name {
            fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
                self.update(buf);
                Ok(buf.len())
            }

            fn flush(&mut self) -> std::io::Result<()> {
                Ok(())
            }
        }
    };
}

macro_rules! aead_algo {
    ($name:ident, $evp_md:ident, $key_len:expr, $nonce_len:expr, $tag_len:expr) => {
        impl $name {
            /// Create a new AEAD context for the given key.
            pub fn new(key: &[u8; $key_len]) -> Self {
                // Safety: $evp_md is assumed to return a valid `EVP_MD`.
                unsafe { $name(EvpAead::new(key, { bssl_sys::$evp_md() })) }
            }
        }

        impl Aead for $name {
            type Tag = [u8; $tag_len];
            type Nonce = [u8; $nonce_len];

            fn seal(&self, nonce: &Self::Nonce, plaintext: &[u8], ad: &[u8]) -> Vec<u8> {
                self.0.seal(nonce, plaintext, ad)
            }

            fn seal_in_place(
                &self,
                nonce: &Self::Nonce,
                plaintext: &mut [u8],
                ad: &[u8],
            ) -> Self::Tag {
                self.0.seal_in_place(nonce, plaintext, ad)
            }

            fn open(&self, nonce: &Self::Nonce, ciphertext: &[u8], ad: &[u8]) -> Option<Vec<u8>> {
                self.0.open(nonce, ciphertext, ad)
            }

            fn open_in_place(
                &self,
                nonce: &Self::Nonce,
                ciphertext: &mut [u8],
                tag: &Self::Tag,
                ad: &[u8],
            ) -> Result<(), InvalidCiphertext> {
                self.0.open_in_place(nonce, ciphertext, tag, ad)
            }
        }
    };
}
