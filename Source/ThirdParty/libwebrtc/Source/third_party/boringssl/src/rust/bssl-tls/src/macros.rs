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

/// Safety: use this macro only when `$tls` refers to `*mut SSL` with exclusive access.
#[doc(hidden)]
#[macro_export]
macro_rules! check_tls_error {
    ($tls:expr, $e:expr) => {
        unsafe {
            // Safety: we have exclusive access to the connection state.
            match ::bssl_sys::SSL_get_error($tls, $e) {
                0 => None,
                rc => Some($crate::errors::Error::extract_tls_err(rc)?),
            }
        }
    };
}

/// Extract library error per BoringSSL specification.
#[doc(hidden)]
#[macro_export]
macro_rules! check_lib_error {
    ($e:expr) => {
        match $e {
            1 => {}
            _ => return Err($crate::errors::Error::extract_lib_err()),
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! crypto_buffer_wrapper {
    (
        $(#[$attr:meta])*
        $vis:vis struct $name:ident
    ) => {
        $(#[$attr])*
        #[repr(transparent)]
        $vis struct $name(::core::ptr::NonNull<::bssl_sys::CRYPTO_BUFFER>);

        impl ::core::fmt::Debug for $name {
            fn fmt(&self, f: &mut ::core::fmt::Formatter<'_>) -> ::core::fmt::Result {
                f.debug_tuple(stringify!($name)).finish()
            }
        }

        impl Clone for $name {
            #[inline(always)]
            fn clone(&self) -> Self {
                unsafe {
                    // Safety: `self.0` should still be valid now.
                    ::bssl_sys::CRYPTO_BUFFER_up_ref(self.0.as_ptr());
                }
                Self(self.0)
            }
        }

        impl Drop for $name {
            #[inline]
            fn drop(&mut self) {
                unsafe {
                    // Safety: `self.0` is still valid at dropping.
                    ::bssl_sys::CRYPTO_BUFFER_free(self.0.as_ptr());
                }
            }
        }

        impl ::core::ops::Deref for $name {
            type Target = [u8];
            fn deref(&self) -> &Self::Target {
                let (data, len) = unsafe {
                    // Safety: `self` witnesses the validity of the underlying handle.
                    (
                        ::bssl_sys::CRYPTO_BUFFER_data(self.ptr()),
                        ::bssl_sys::CRYPTO_BUFFER_len(self.ptr()),
                    )
                };
                if data.is_null() || len == 0 || len > isize::MAX as usize {
                    &[]
                } else {
                    unsafe {
                        // Safety:
                        // - `data` is 1-size and 1-align and `len` is valid by BoringSSL invariant.
                        // - `len` is sanitised to be within bound.
                        ::core::slice::from_raw_parts(data, len)
                    }
                }
            }
        }

        impl ::core::convert::AsRef<[u8]> for $name {
            fn as_ref(&self) -> &[u8] {
                &self
            }
        }

        impl $name {
            /// **NOTE: we do not sanitise or validate the input bytes.**
            #[inline(always)]
            pub fn from_bytes(bytes: &[u8], pool: Option<&$crate::context::CertificateCache>)
                -> Result<Self, Error>
            {
                $crate::ffi::crypto_buffer_from_buf(bytes, pool).map(Self)
            }

            #[inline(always)]
            #[allow(unused)]
            pub(crate) fn ptr(&self) -> *mut ::bssl_sys::CRYPTO_BUFFER {
                self.0.as_ptr()
            }

            /// This method releases our drop obligation on the buffer handle.
            #[inline(always)]
            #[allow(unused)]
            pub(crate) fn into_raw(self) -> *mut ::bssl_sys::CRYPTO_BUFFER {
                let ptr = self.0.as_ptr();
                core::mem::forget(self);
                ptr
            }
        }
    };
}

// Safety: `$obj` must outlive the returned slice.
#[doc(hidden)]
#[macro_export]
macro_rules! call_slice_getter {
    ($fn:path, $obj:expr) => {{
        let mut data = ::core::ptr::null();
        let mut len = 0;
        #[allow(unused_unsafe)]
        unsafe {
            // Safety: `obj`, `data` and `len` are all valid.
            $fn($obj, &raw mut data, &raw mut len);
        }
        #[allow(unused_unsafe)]
        unsafe {
            // Safety: data and len are returned by BoringSSL and are valid.
            $crate::ffi::sanitize_slice(data, len)
        }
    }};
}
