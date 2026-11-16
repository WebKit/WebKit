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

//! X.509 certificate verification.
//!
//! To verify certificates, one may use [`X509Verifier`] which requires a fully constructed
//! [`X509Store`] certificate store, a slice of untrusted intermediate certificates, and the
//! final end-entity certificate as an [`X509Certificate`].
//!
//! ```rust
//! # use bssl_x509::certificates::X509Certificate;
//! # use bssl_x509::store::{X509Store, X509StoreBuilder};
//! # use bssl_x509::verify::X509Verifier;
//! let ca = X509Certificate::parse_one_from_pem(
//!     include_bytes!("tests/BoringSSLTestCA.crt")).unwrap();
//! let mut store = X509StoreBuilder::new();
//! store.add_cert(ca).unwrap();
//! let store = store.build();
//!
//! let leaf = X509Certificate::parse_one_from_pem(
//!     include_bytes!("tests/BoringSSLServerTest-RSA.crt")).unwrap();
//!
//! let mut verifier = X509Verifier::new(&leaf, &[], &store).unwrap();
//! assert!(verifier.verify().is_ok());
//! ```

use alloc::vec::Vec;
use core::{marker::PhantomData, ptr::NonNull};

use crate::{
    certificates::X509Certificate,
    check_lib_error,
    errors::{PkiError, X509VerifyResult},
    store::X509Store,
};

/// A context for X.509 certificate verification.
///
/// This corresponds to `X509_STORE_CTX` in BoringSSL.
pub struct X509Verifier<'a> {
    ptr: NonNull<bssl_sys::X509_STORE_CTX>,
    chain: CertificateStack,
    verified: bool,
    _p: PhantomData<&'a ()>,
}

// Safety: X509_STORE_CTX is not thread-safe for concurrent access, but can be moved.
unsafe impl Send for X509Verifier<'_> {}

impl Drop for X509Verifier<'_> {
    fn drop(&mut self) {
        unsafe {
            // Safety: The pointer is valid and owned by this struct.
            bssl_sys::X509_STORE_CTX_free(self.ptr.as_ptr());
        }
    }
}

impl<'a> X509Verifier<'a> {
    /// Creates a new `X509Verifier`.
    pub fn new(
        cert: &'a X509Certificate,
        untrusted: &'a [X509Certificate],
        store: &'a X509Store,
    ) -> Result<Self, PkiError> {
        let chain = CertificateStack::from_borrowed(untrusted);
        let ctx = Self::alloc(chain);
        check_lib_error!(unsafe {
            // Safety:
            // - `ctx.ptr()` is valid.
            // - `store.as_raw()` returns a valid X509_STORE pointer.
            // - `cert.ptr()` returns a valid X509 pointer.
            // - `chain` is a valid stack pointer, kept alive by `ctx.chain`.
            // - The input objects will outlive `'a`, so the verifier is outlived by
            //   these objects.
            bssl_sys::X509_STORE_CTX_init(
                ctx.ptr(),
                store.as_mut_ptr(),
                cert.ptr(),
                ctx.chain.ptr(),
            )
        });
        Ok(ctx)
    }

    fn alloc(chain: CertificateStack) -> Self {
        let Some(ctx) = NonNull::new(unsafe {
            // Safety: This function creates a new object and returns NULL on allocation failure.
            bssl_sys::X509_STORE_CTX_new()
        }) else {
            panic!("allocation error");
        };

        Self {
            ptr: ctx,
            chain,
            verified: false,
            _p: PhantomData,
        }
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::X509_STORE_CTX {
        self.ptr.as_ptr()
    }

    /// Performs the certificate verification.
    ///
    /// Returns `Ok(())` if verification succeeds.
    /// Returns `Err(X509VerifyResult)` if verification fails.
    pub fn verify(&mut self) -> Result<(), X509VerifyResult> {
        if unsafe {
            // Safety: `self.0` is valid. The context must have been initialized.
            bssl_sys::X509_verify_cert(self.ptr()) == 1
        } {
            self.verified = true;
            Ok(())
        } else {
            Err(self.get_error())
        }
    }

    fn get_error(&self) -> X509VerifyResult {
        let error_code = unsafe {
            // Safety: `self.0` is valid.
            bssl_sys::X509_STORE_CTX_get_error(self.ptr())
        };
        X509VerifyResult::try_from(error_code as i32).unwrap_or(X509VerifyResult::Unspecified)
    }

    /// Return a possibly valid certificate chain.
    ///
    /// The first certificate will be the leaf certificate and the last certificate should be one of
    /// the trusted certificate authorities, if the method returns `Some(chain)`.
    ///
    /// This method also returns [`None`] if [`Self::verify`] has not been successfully completed.
    pub fn chain(&self) -> Option<Vec<X509Certificate>> {
        if !self.verified {
            return None;
        }
        let chain = NonNull::new(unsafe {
            // Safety: `self.0` is valid.
            bssl_sys::X509_STORE_CTX_get0_chain(self.ptr())
        })?;
        let len = unsafe {
            // Safety: `chain` is valid.
            bssl_sys::sk_X509_num(chain.as_ptr())
        };
        let mut res = Vec::new();
        for i in 0..len {
            let cert = unsafe {
                // Safety: `chain` is valid and `i` < `len`.
                NonNull::new(bssl_sys::sk_X509_value(chain.as_ptr(), i))?
            };
            unsafe {
                // Safety: `cert` is borrowed from the chain; bump the ref count.
                res.push(X509Certificate::from_borrowed_raw(cert));
            }
        }
        Some(res)
    }
}

/// A transient STACK_OF(X509) that borrows certificates.
///
/// The certificates pushed into this stack have their reference counts
/// incremented, and are released when the stack is freed.
struct CertificateStack(NonNull<bssl_sys::stack_st_X509>);

// Safety: contains no thread-local data.
unsafe impl Send for CertificateStack {}

impl CertificateStack {
    /// Build a stack from borrowed certificates. Each certificate's reference
    /// count is incremented so the stack owns a reference.
    fn from_borrowed(certs: &[X509Certificate]) -> Self {
        let Some(stack) = NonNull::new(unsafe {
            // Safety: only allocation.
            bssl_sys::sk_X509_new_null()
        }) else {
            panic!("allocation error");
        };
        for cert in certs {
            unsafe {
                // Safety: `cert` is valid. Bump the ref count so the stack
                // owns its own reference.
                bssl_sys::X509_up_ref(cert.ptr());
            }
            if unsafe {
                // Safety: `cert.ptr()` is valid.
                bssl_sys::sk_X509_push(stack.as_ptr(), cert.ptr()) == 0
            } {
                panic!("allocation failure");
            }
        }
        Self(stack)
    }

    fn ptr(&self) -> *mut bssl_sys::stack_st_X509 {
        self.0.as_ptr()
    }
}

impl Drop for CertificateStack {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self` is valid. Each element's ref count was incremented
            // in `from_borrowed`, so `X509_free` will release that reference.
            bssl_sys::sk_X509_pop_free(self.ptr(), Some(bssl_sys::X509_free));
        }
    }
}
