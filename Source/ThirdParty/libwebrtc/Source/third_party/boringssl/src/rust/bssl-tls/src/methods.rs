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
use core::ffi::{
    c_int,
    c_long,
    c_void, //
};

use crate::{
    Methods,
    abort_on_panic, //
};

pub(crate) unsafe extern "C" fn drop_box_rust_methods<M: Methods>(
    _parent: *mut c_void,
    ptr: *mut c_void,
    _ad: *mut bssl_sys::CRYPTO_EX_DATA,
    _index: c_int,
    _argl: c_long,
    _argp: *mut c_void,
) {
    abort_on_panic(move || {
        let _ = unsafe {
            if ptr.is_null() {
                return;
            }
            // Safety: the data was boxed and stored via SSL_CTX_set_ex_data.
            Box::from_raw(ptr as *mut M)
        };
    });
}
