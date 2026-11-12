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

use super::TlsContextBuilder;

use crate::{
    alpn::AlpnProtocols,
    config::ConfigurationError,
    context::SupportedMode,
    errors::Error,
    ffi::slice_into_ffi_raw_parts, //
};

/// ALPN configurations.
impl<M: SupportedMode> TlsContextBuilder<M> {
    /// Set ALPN protocols.
    ///
    /// By passing an empty [`AlpnProtocols`] this method disables ALPN.
    ///
    /// Per [RFC 7301], empty protocol names, names longer than 255 bytes are invalid.
    /// Also, the total size of the ALPN protocol list must not exceed [`u16::MAX`] bytes.
    ///
    /// [RFC 7301]: <https://datatracker.ietf.org/doc/html/rfc7301#section-3.1>
    pub fn set_alpn_protocols<'a>(
        &mut self,
        protocols: impl IntoIterator<Item = &'a [u8]>,
    ) -> Result<&mut Self, Error> {
        let mut protos = AlpnProtocols::default();
        for proto in protocols {
            protos
                .append_protocol(proto)
                .map_err(|_| Error::Configuration(ConfigurationError::InvalidAlpnProtocols))?;
        }
        let (protos, len) = slice_into_ffi_raw_parts(protos.as_slice());
        let rc = unsafe {
            // Safety:
            // - the validity of the handle `self.ptr()` is witnessed by `self`.
            // - the validity of ALPN string is guaranteed by `AlpnProtocols` type.
            // Note: `SSL_CTX_set_alpn_protos` flips the return value around for error signal.
            bssl_sys::SSL_CTX_set_alpn_protos(self.ptr(), protos, len)
        };
        if rc == 1 {
            Err(Error::extract_lib_err_or_unknown())
        } else {
            Ok(self)
        }
    }
}
