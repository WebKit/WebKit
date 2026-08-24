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
use core::{
    ffi::c_int,

    ptr::{
        NonNull,
        null_mut, //
    }, //
};

use once_cell::sync::Lazy;

use crate::{
    Methods,
    PrivateKeyMethods,
    abort_on_panic,
    connection::methods::private_key_op_from_ssl,
    credentials::{
        DecryptionOperation,
        PrivateKeyDelegate,
        PrivateKeyOperationResult,
        SignatureOperation,
        waker_data_from_ssl, //
    },
    ffi::{
        sanitise_mut_byteslice,
        sanitize_slice, //
    },
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
pub(crate) struct RustCredentialMethods {
    pub(crate) private_key_methods: Option<Box<dyn PrivateKeyDelegate>>,
}

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

impl PrivateKeyMethods for RustCredentialMethods {
    fn private_key_methods(&self) -> Option<&dyn PrivateKeyDelegate> {
        self.private_key_methods.as_deref()
    }
}

macro_rules! private_key_method_prelude {
    ($M:ty, $ssl:ident => $context:ident, $private_key_methods:ident, $task:ident) => {
        let Some(ssl) = NonNull::new($ssl) else {
            return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
        };
        let Some(methods) = (unsafe {
            // Safety: `ssl` outlives `methods` and must be valid by BoringSSL contract.
            <$M>::from_ssl(ssl.as_ptr())
        }) else {
            return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
        };
        let waker = unsafe {
            // Safety: `ssl` must be constructed by `TlsConnection` as this is called from
            waker_data_from_ssl(ssl)
        };
        let mut $context = if let Some(waker) = &waker {
            Some(core::task::Context::from_waker(waker))
        } else {
            None
        };
        #[allow(unused_mut)]
        let $task = unsafe {
            // Safety: `ssl` must be constructed by `TlsConnection` as this is called from
            private_key_op_from_ssl(ssl)
        };
        let Some($private_key_methods) = methods.private_key_methods() else {
            return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
        };
    };
}

pub(crate) unsafe extern "C" fn sign<Method: PrivateKeyMethods>(
    ssl: *mut bssl_sys::SSL,
    out: *mut u8,
    out_len: *mut usize,
    max_out: usize,
    sig_alg: u16,
    msg: *const u8,
    msg_len: usize,
) -> bssl_sys::ssl_private_key_result_t {
    private_key_method_prelude!(Method, ssl => context, private_key_methods, task);
    if task.is_some() {
        abort_on_panic(|| {
            let _ = task.take();
        });
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    }

    // Unwind-safety: later when panic happens, we detach the poisoned private key method
    // without calling destructor.
    let algorithm = match sig_alg.try_into() {
        Ok(sig_alg) => sig_alg,
        // TODO(@xfding) maybe we should log this error?
        Err(_) => return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure,
    };
    let Some(output) = (unsafe {
        // Safety: the slice will only be used within this callback.
        sanitise_mut_byteslice(out, max_out)
    }) else {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    };
    if output.is_empty() {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    }
    let Some(message) = (unsafe {
        // Safety: `msg` outlives `message` because it is owned by BoringSSL.
        sanitize_slice(msg, msg_len)
    }) else {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    };
    // Unwind-safety: when panic happens, we will not inspect the `output` buffer.
    abort_on_panic(move || {
        let sign_op = SignatureOperation {
            output,
            message,
            algorithm,
        };
        let mut outstanding_task = private_key_methods.sign(sign_op);
        match outstanding_task.complete(context.as_mut(), output) {
            PrivateKeyOperationResult::Success(len) => {
                unsafe {
                    // Safety: `out_len` is a valid pointer by BoringSSL invariant.
                    *out_len = len;
                }
                bssl_sys::ssl_private_key_result_t_ssl_private_key_success
            }
            PrivateKeyOperationResult::Pending => {
                *task = Some(outstanding_task);
                bssl_sys::ssl_private_key_result_t_ssl_private_key_retry
            }
            PrivateKeyOperationResult::Error => {
                bssl_sys::ssl_private_key_result_t_ssl_private_key_failure
            }
        }
    })
}

pub(crate) unsafe extern "C" fn decrypt<Method: PrivateKeyMethods>(
    ssl: *mut bssl_sys::SSL,
    out: *mut u8,
    out_len: *mut usize,
    max_out: usize,
    ciphertext: *const u8,
    ciphertext_len: usize,
) -> bssl_sys::ssl_private_key_result_t {
    private_key_method_prelude!(Method, ssl => context, private_key_methods, task);
    if task.is_some() {
        abort_on_panic(|| {
            let _ = task.take();
        });
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    }

    // Unwind-safety: later when panic happens, we detach the poisoned private key method
    // without calling destructor.
    let Some(output) = (unsafe {
        // Safety: the slice will only be used within this callback.
        sanitise_mut_byteslice(out, max_out)
    }) else {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    };
    if output.is_empty() {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    }
    let Some(ciphertext) = (unsafe {
        // Safety: `ciphertext` is now owned by BoringSSL and outlives the slice.
        sanitize_slice(ciphertext, ciphertext_len)
    }) else {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    };
    // Unwind-safety: when panic happens, we will not inspect the `output` buffer.
    abort_on_panic(move || {
        let decrypt_op = DecryptionOperation { output, ciphertext };
        let mut outstanding_task = private_key_methods.decrypt(decrypt_op);
        match outstanding_task.complete(context.as_mut(), output) {
            PrivateKeyOperationResult::Success(len) => {
                unsafe {
                    // Safety: `out_len` is a valid pointer by BoringSSL invariant.
                    *out_len = len;
                }
                bssl_sys::ssl_private_key_result_t_ssl_private_key_success
            }
            PrivateKeyOperationResult::Pending => {
                *task = Some(outstanding_task);
                bssl_sys::ssl_private_key_result_t_ssl_private_key_retry
            }
            PrivateKeyOperationResult::Error => {
                bssl_sys::ssl_private_key_result_t_ssl_private_key_failure
            }
        }
    })
}

pub(crate) unsafe extern "C" fn complete<Method: PrivateKeyMethods>(
    ssl: *mut bssl_sys::SSL,
    out: *mut u8,
    out_len: *mut usize,
    max_out: usize,
) -> bssl_sys::ssl_private_key_result_t {
    private_key_method_prelude!(Method, ssl => context, _private_key_methods, task);

    // Unwind-safety: later when panic happens, we detach the poisoned private key method
    // without calling destructor.
    let Some(output) = (unsafe {
        // Safety: the slice will only be used within this callback.
        sanitise_mut_byteslice(out, max_out)
    }) else {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    };
    if output.is_empty() {
        return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
    }
    // Unwind-safety: when panic happens, we will not inspect the `output` buffer.
    abort_on_panic(move || {
        let Some(outstanding_task) = task else {
            return bssl_sys::ssl_private_key_result_t_ssl_private_key_failure;
        };
        match outstanding_task.complete(context.as_mut(), output) {
            PrivateKeyOperationResult::Success(len) => {
                unsafe {
                    // Safety: `out_len` is a valid pointer by BoringSSL invariant.
                    *out_len = len;
                }
                bssl_sys::ssl_private_key_result_t_ssl_private_key_success
            }
            PrivateKeyOperationResult::Pending => {
                bssl_sys::ssl_private_key_result_t_ssl_private_key_retry
            }
            PrivateKeyOperationResult::Error => {
                bssl_sys::ssl_private_key_result_t_ssl_private_key_failure
            }
        }
    })
}

pub(super) const PRIVATE_KEY_METHODS: *const bssl_sys::SSL_PRIVATE_KEY_METHOD = {
    &bssl_sys::SSL_PRIVATE_KEY_METHOD {
        sign: Some(sign::<RustCredentialMethods>),
        decrypt: Some(decrypt::<RustCredentialMethods>),
        complete: Some(complete::<RustCredentialMethods>),
    } as _
};
