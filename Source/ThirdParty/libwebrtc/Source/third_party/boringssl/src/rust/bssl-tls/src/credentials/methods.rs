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

use core::{
    ffi::c_int,
    ptr::null_mut, //
};

use once_cell::sync::Lazy;

use crate::{
    Methods,
    methods::drop_box_rust_methods, //
};

pub(super) static TLS_CREDENTIAL_METHOD: Lazy<c_int> = Lazy::new(|| unsafe {
    // Safety: this a one-time registration uses only valid function pointers.
    let ret = bssl_sys::SSL_CREDENTIAL_get_ex_new_index(
        0,
        null_mut(),
        null_mut(),
        None,
        Some(drop_box_rust_methods::<RustCredentialMethods>),
    );
    if ret < 0 {
        panic!("Failed to register TLS Credential ex-data")
    } else {
        ret
    }
});

#[derive(Default)]
pub(crate) struct RustCredentialMethods {}

impl Methods for RustCredentialMethods {
    unsafe extern "C" fn from_ssl<'a>(ssl: *mut bssl_sys::SSL) -> Option<&'a Self> {
        unsafe {
            // Safety: `ssl` is valid per BoringSSL invariant.
            let cred = bssl_sys::SSL_get0_selected_credential(ssl);
            if cred.is_null() {
                return None;
            }
            // Safety: `cred` is valid and originated from `TlsCredential::new`.
            let methods = bssl_sys::SSL_CREDENTIAL_get_ex_data(cred, *TLS_CREDENTIAL_METHOD);
            if methods.is_null() {
                return None;
            }
            // Safety: `cred` is originated from `Box::into_raw`.
            Some(&*(methods as *mut RustCredentialMethods))
        }
    }
}
