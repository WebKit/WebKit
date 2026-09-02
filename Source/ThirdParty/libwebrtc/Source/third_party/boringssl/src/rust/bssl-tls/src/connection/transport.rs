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

//! TLS Connection transport settings
//!

use core::mem::{
    MaybeUninit,
    transmute, //
};

use crate::{
    check_lib_error,
    check_tls_error,
    config::ConfigurationError,
    connection::{
        TlsConnection,
        TlsConnectionBuilder,
        methods::HasTlsConnectionMethod, //
    },
    context::DtlsMode,
    context::HasBasicIo,
    errors::Error,
    io::{
        AbstractReader,
        AbstractSocket,
        AbstractWriter,
        RustBio, //
    }, //
};

/// # Transport configurations
///
/// These are the methods to configure the underlying IO drivers and transport configurations.
impl<R, M> TlsConnection<R, M>
where
    M: HasBasicIo + HasTlsConnectionMethod,
{
    /// Set up underlying transport driver.
    pub fn set_io<S: 'static + AbstractSocket>(&mut self, socket: S) -> Result<&mut Self, Error> {
        let bio = RustBio::new_duplex(socket)?;
        unsafe {
            // Safety: the additional ref-count is to compensate for `SSL` taking ownership.
            bssl_sys::BIO_up_ref(bio.ptr());
            // Safety: the `bio` pointer has been sanitised and `self.0` is still valid.
            bssl_sys::SSL_set_bio(self.ptr(), bio.ptr(), bio.ptr());
        }
        self.get_connection_methods().bio = Some(bio);
        Ok(self)
    }

    /// Set up underlying transport driver, with a pair of read and write ends.
    pub fn set_split_io<Reader, Writer>(
        &mut self,
        read: Reader,
        write: Writer,
    ) -> Result<&mut Self, Error>
    where
        Reader: 'static + AbstractReader,
        Writer: 'static + AbstractWriter,
    {
        let bio = RustBio::new_split(read, write)?;
        unsafe {
            // Safety: the additional ref-count is to compensate for `SSL` taking ownership.
            bssl_sys::BIO_up_ref(bio.ptr());
            // Safety: the `bio` pointer has been sanitised and `self.0` is still valid.
            bssl_sys::SSL_set_bio(self.ptr(), bio.ptr(), bio.ptr());
        }
        self.get_connection_methods().bio = Some(bio);
        Ok(self)
    }

    /// Check if the underlying **transport** has closed its write end.
    pub fn is_write_closed(&self) -> bool {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map_or(true, |bio| bio.as_ref().write_eos)
    }

    /// Check if the underlying **transport** has closed its read end.
    pub fn is_read_closed(&self) -> bool {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map_or(true, |bio| bio.as_ref().read_eos)
    }

    /// Check if the underlying **transport** has closed either its read end or its write end.
    pub fn is_one_side_closed(&self) -> bool {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map_or(true, |bio| bio.as_ref().read_eos || bio.as_ref().write_eos)
    }
}

/// # Transport configurations
///
/// These are the methods to configure the underlying IO drivers and transport configurations.
impl<R, M> TlsConnectionBuilder<R, M>
where
    M: HasTlsConnectionMethod,
{
    /// **For DTLS only**, set connection's Maximum Transmission Unit.
    pub fn with_mtu(&mut self, mtu: u32) -> Result<&mut Self, Error> {
        let conn = self.ptr();
        let mtu = mtu
            .try_into()
            .map_err(|_| Error::Configuration(ConfigurationError::InvalidParameters))?;
        check_lib_error!(unsafe {
            // Safety: `conn` is still valid.
            bssl_sys::SSL_set_mtu(conn, mtu)
        });
        Ok(self)
    }

    /// **For DTLS only**, set connection's handshake timeout in milliseconds.
    pub fn with_dtlsv1_initial_timeout(&mut self, milliseconds: u32) -> Result<&mut Self, Error> {
        unsafe {
            // Safety: the validity of the handle `self.0` is witnessed by `self`.
            bssl_sys::DTLSv1_set_initial_timeout_duration(self.ptr(), milliseconds);
        }
        Ok(self)
    }
}

/// # DTLS
impl<R> TlsConnection<R, DtlsMode> {
    /// Trigger timeout handling on the connection.
    ///
    /// On success, this method call returns `true` when a timeout is hit and successfully handled;
    /// `false` when no timeout has expired yet.
    pub fn dtlsv1_handle_timeout(&mut self) -> Result<bool, Error> {
        let conn = self.ptr();
        let rc = unsafe {
            // Safety: the connection handle here is still valid.
            bssl_sys::DTLSv1_handle_timeout(conn)
        };
        if rc == 0 {
            return Ok(false);
        }
        let _ = check_tls_error!(conn, rc);
        Ok(true)
    }

    /// Get connection's remaining timeout.
    ///
    /// If a timeout is in effect, this method call returns the remaining seconds,
    /// followed by the remaining microseconds.
    pub fn dtlsv1_get_timeout(&self) -> Option<(i64, i64)> {
        #[cfg(windows)]
        #[repr(C)]
        struct timeval {
            tv_sec: core::ffi::c_long,
            tv_usec: core::ffi::c_long,
        }
        #[cfg(not(windows))]
        use bssl_sys::timeval;

        let mut timeval = MaybeUninit::<timeval>::zeroed();
        let rc = unsafe {
            // Safety:
            // - the validity of the handle `self.0` is witnessed by `self`;
            // - the buffer for timeval is valid.
            // - on Windows, the timeval structure should match the winsock2.h definition precisely.
            bssl_sys::DTLSv1_get_timeout(self.ptr(), transmute(timeval.as_mut_ptr()))
        };
        match rc {
            1 => {
                let timeval = unsafe {
                    // Safety: timeval is now valid as per BoringSSL specification.
                    timeval.assume_init()
                };
                Some((timeval.tv_sec as i64, timeval.tv_usec as i64))
            }
            0 => None,
            rc => {
                unreachable!("BoringSSL should never return {rc} when calling dtlsv1_get_timeout")
            }
        }
    }
}
