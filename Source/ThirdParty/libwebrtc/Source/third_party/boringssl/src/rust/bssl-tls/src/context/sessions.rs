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

use core::ffi::c_int;

use bssl_crypto::FfiSlice as _;

use crate::{
    check_lib_error,
    config::ConfigurationError,
    context::{
        SupportedMode,
        TlsContext,
        TlsContextBuilder, //
    },
    errors::Error,
    sessions::TlsSession, //
};

/// # Sessions
impl<M> TlsContextBuilder<M>
where
    M: SupportedMode,
{
    /// Set Session ID Context.
    ///
    /// This method errs when the context is not a valid serialization of the context buffer.
    ///
    /// A session without a matching `sid_ctx` will not be picked.
    /// Also when [`crate::credentials::CertificateVerificationMode::PeerCertRequested`] is set,
    /// the server connection will reject all sessions unless this method is called.
    pub fn with_session_id_ctx(&mut self, sid_ctx: &[u8]) -> Result<&mut Self, Error> {
        if sid_ctx.len() > bssl_sys::SSL_MAX_SID_CTX_LENGTH as usize {
            return Err(Error::Configuration(
                ConfigurationError::SessionIdContextTooLarge,
            ));
        }
        check_lib_error!(unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::SSL_CTX_set_session_id_context(
                self.ptr(),
                sid_ctx.as_ffi_ptr(),
                sid_ctx.len(),
            )
        });
        Ok(self)
    }

    /// Set session cache
    pub fn with_session_cache(&mut self, mode: SessionCacheMode) -> &mut Self {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the `mode` passes the right flag bits by construction.
            bssl_sys::SSL_CTX_set_session_cache_mode(self.ptr(), mode.bits());
        }
        self
    }
}

bitflags::bitflags! {
    /// Session cache mode
    #[derive(Debug, Copy, Clone)]
    pub struct SessionCacheMode: c_int {
        /// No caching.
        const CACHE_OFF = bssl_sys::SSL_SESS_CACHE_OFF as c_int;
        /// Caching enabled for clients.
        const CACHE_CLIENT = bssl_sys::SSL_SESS_CACHE_CLIENT as c_int;
        /// Caching enabled for servers.
        const CACHE_SERVER = bssl_sys::SSL_SESS_CACHE_SERVER as c_int;
        /// Caching enabled for both clients and servers.
        const CACHE_BOTH = bssl_sys::SSL_SESS_CACHE_BOTH as c_int;
        /// Disable the default automatic session flushing after every 255 connections.
        const NO_AUTO_CLEAR = bssl_sys::SSL_SESS_CACHE_NO_AUTO_CLEAR as c_int;
        /// Disable session look-up from internal session cache **on a server**.
        const NO_INTERNAL_LOOKUP = bssl_sys::SSL_SESS_CACHE_NO_INTERNAL_LOOKUP as c_int;
        /// Disable session storage into the internal session cache **on a server**.
        const NO_INTERNAL_STORE = bssl_sys::SSL_SESS_CACHE_NO_INTERNAL_STORE as c_int;
        /// Disable session internal caching.
        const NO_INTERNAL = bssl_sys::SSL_SESS_CACHE_NO_INTERNAL as c_int;
    }
}

/// # Sessions
impl<M> TlsContext<M>
where
    M: SupportedMode,
{
    /// Add a [`TlsSession`] to the context.
    ///
    /// This method returns `true` if the session is freshly added;
    /// otherwise, it is already present in the context by returning `false`.
    pub fn add_session(&self, session: &TlsSession) -> bool {
        let ret = unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the method is synchronised with a mutex guard.
            bssl_sys::SSL_CTX_add_session(self.ptr(), session.ptr()) == 1
        };
        unsafe {
            // Safety: we are only clearing the error queue in the current thread.
            bssl_sys::ERR_clear_error();
        }
        ret
    }

    /// Remove a [`TlsSession`] from the context.
    ///
    /// This method returns `true` if the session is present and removed from the context;
    /// otherwise, it is missing by returning `false`.
    pub fn remove_session(&self, session: &TlsSession) -> bool {
        let ret = unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the method is synchronised with a mutex guard.
            bssl_sys::SSL_CTX_remove_session(self.ptr(), session.ptr()) == 1
        };
        unsafe {
            // Safety: we are only clearing the error queue in the current thread.
            bssl_sys::ERR_clear_error();
        }
        ret
    }

    /// Remove all sessions that have expired past the `deadline`, which is a POSIX timestamp.
    pub fn flush_sessions(&self, deadline: u64) {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the method is synchronised with a mutex guard.
            bssl_sys::SSL_CTX_flush_sessions(self.ptr(), deadline);
        }
    }

    /// Get the number of sessions in the context.
    pub fn count_sessions(&self) -> usize {
        unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`.
            // - the method is synchronised with a mutex guard.
            bssl_sys::SSL_CTX_sess_number(self.ptr())
        }
    }
}
