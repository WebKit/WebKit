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

//! X.509 certificate store for verification

use alloc::ffi::CString;
use core::{
    ffi::c_int,
    panic,
    ptr::{NonNull, null},
};

use crate::{
    certificates::X509Certificate,
    check_lib_error,
    errors::{PkiError, X509Error},
    params::{CertificateVerificationParams, Purpose, Trust, VerificationFlags},
};

/// An X.509 certificate store builder.
pub struct X509StoreBuilder(NonNull<bssl_sys::X509_STORE>);

// Safety: `X509_STORE` is ref-counted and always requires exclusive accesses.
unsafe impl Send for X509StoreBuilder {}

impl Drop for X509StoreBuilder {
    fn drop(&mut self) {
        unsafe {
            bssl_sys::X509_STORE_free(self.0.as_ptr());
        }
    }
}

impl X509StoreBuilder {
    /// Construct a new X.509 certificate store.
    pub fn new() -> Self {
        let store = unsafe { bssl_sys::X509_STORE_new() };
        let Some(store) = NonNull::new(store) else {
            panic!("allocation error")
        };
        Self(store)
    }

    pub(crate) fn ptr(&self) -> *mut bssl_sys::X509_STORE {
        self.0.as_ptr()
    }

    /// Finalise the certificate store.
    pub fn build(self) -> X509Store {
        let ptr = self.0;
        core::mem::forget(self);
        X509Store(ptr)
    }

    /// Add a certificate to the store.
    pub fn add_cert(&mut self, cert: X509Certificate) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe { bssl_sys::X509_STORE_add_cert(self.ptr(), cert.ptr()) });
        Ok(self)
    }

    /// Load certificates from a file or directory.
    ///
    /// `file` and `dir` cannot be both [`None`];
    /// otherwise, this method returns [`X509Error::InvalidParameters`].
    pub fn load_locations(
        &mut self,
        file: Option<&str>,
        dir: Option<&str>,
    ) -> Result<&mut Self, PkiError> {
        if file.is_none() && dir.is_none() {
            return Err(PkiError::X509(X509Error::InvalidParameters));
        }
        let file_cstr;
        let dir_cstr;
        let file = if let Some(file) = file {
            file_cstr = CString::new(file).map_err(|_| PkiError::InternalNullBytes)?;
            file_cstr.as_ptr()
        } else {
            null()
        };
        let dir = if let Some(dir) = dir {
            dir_cstr = CString::new(dir).map_err(|_| PkiError::InternalNullBytes)?;
            dir_cstr.as_ptr()
        } else {
            null()
        };

        check_lib_error!(unsafe {
            // Safety:
            // - the handle `self.0` is valid.
            // - `file` and `dir` are either null or valid NUL-terminated C-strings.
            bssl_sys::X509_STORE_load_locations(self.ptr(), file, dir)
        });
        Ok(self)
    }

    /// Set verification flags.
    pub fn set_flags(&mut self, flags: VerificationFlags) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the flag bits are valid by construction.
            bssl_sys::X509_STORE_set_flags(self.ptr(), flags.bits())
        });
        Ok(self)
    }

    /// Set verification depth.
    pub fn set_depth(&mut self, depth: u16) -> Result<&mut Self, PkiError> {
        let depth = c_int::try_from(depth).map_err(|_| PkiError::ValueOutOfRange)?;
        check_lib_error!(unsafe { bssl_sys::X509_STORE_set_depth(self.ptr(), depth) });
        Ok(self)
    }

    /// Set verification purpose.
    pub fn set_purpose(&mut self, purpose: Purpose) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the `purpose` value is valid per construction.
            bssl_sys::X509_STORE_set_purpose(self.ptr(), purpose as c_int)
        });
        Ok(self)
    }

    /// Set verification trust.
    pub fn set_trust(&mut self, trust: Trust) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the `trust` value is valid per construction.
            bssl_sys::X509_STORE_set_trust(self.ptr(), trust as c_int)
        });
        Ok(self)
    }

    /// Set verification parameters.
    pub fn set_param(
        &mut self,
        param: &CertificateVerificationParams,
    ) -> Result<&mut Self, PkiError> {
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::X509_STORE_set1_param(self.ptr(), param.ptr())
        });
        Ok(self)
    }
}

/// An X.509 certificate store
pub struct X509Store(NonNull<bssl_sys::X509_STORE>);

// Safety: `X509_STORE` is ref-counted and mutation behind a shared reference is protected
// behind a lock.
unsafe impl Send for X509Store {}
unsafe impl Sync for X509Store {}

impl X509Store {
    /// Extract the handle for cross-language interoperability.
    ///
    /// # Safety
    ///
    /// `self` **must** outlive the uses of the returned handle.
    /// Verify the callsite contract to honour the lifetime contracts.
    pub unsafe fn as_mut_ptr(&self) -> *mut bssl_sys::X509_STORE {
        self.0.as_ptr()
    }
}

impl Clone for X509Store {
    fn clone(&self) -> Self {
        unsafe {
            bssl_sys::X509_STORE_up_ref(self.0.as_ptr());
        }
        Self(self.0)
    }
}

impl Drop for X509Store {
    fn drop(&mut self) {
        unsafe {
            bssl_sys::X509_STORE_free(self.0.as_ptr());
        }
    }
}
