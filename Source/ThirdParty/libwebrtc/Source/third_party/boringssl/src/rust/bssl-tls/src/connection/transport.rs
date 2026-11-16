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

use core::{
    mem::{
        MaybeUninit,
        transmute, //
    },
    time::Duration, //
};

use crate::{
    check_lib_error,
    config::{
        ConfigurationError,
        SrtpProtectionProfile, //
    },
    connection::{
        TlsConnection,
        TlsConnectionBuilder,
        methods::HasTlsConnectionMethod, //
    },
    context::{
        DtlsMode,
        HasDatagramIo,
        HasStreamIo, //
    },
    errors::{
        Error,
        TlsRetryReason,
        UnknownError, //
    },
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
    M: HasTlsConnectionMethod,
{
    /// Set up underlying transport driver.
    fn set_io_inner<S: 'static + AbstractSocket>(&mut self, socket: S) -> Result<&mut Self, Error> {
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
}

/// # Transport configurations
///
/// These are the methods to configure the underlying IO drivers and transport configurations.
impl<R, M> TlsConnection<R, M>
where
    M: HasDatagramIo + HasTlsConnectionMethod,
{
    /// Set up datagram socket driver.
    pub fn set_datagram_socket<S: 'static + AbstractSocket>(
        &mut self,
        socket: S,
    ) -> Result<&mut Self, Error> {
        self.set_io_inner(socket)
    }

    /// Return the negotiated DTLS-SRTP protection profile per [RFC 5764],
    /// or [`None`] if no profile was negotiated.
    ///
    /// [RFC 5764]: <https://datatracker.ietf.org/doc/html/rfc5764>
    pub fn selected_srtp_profile(&self) -> Option<SrtpProtectionProfile> {
        let profile = unsafe {
            // Safety: `self.ptr()` is a valid `SSL` handle, and `SSL_get_selected_srtp_profile`
            // returns either null or a pointer to a static `SRTP_PROTECTION_PROFILE`.
            bssl_sys::SSL_get_selected_srtp_profile(self.ptr()).as_ref()?
        };
        let id = u16::try_from(profile.id).ok()?;
        SrtpProtectionProfile::try_from(id).ok()
    }
}

/// # Transport configurations
///
/// These are the methods to configure the underlying IO drivers and transport configurations.
impl<R, M> TlsConnection<R, M>
where
    M: HasStreamIo + HasTlsConnectionMethod,
{
    /// Set up underlying transport driver.
    pub fn set_io<S: 'static + AbstractSocket>(&mut self, socket: S) -> Result<&mut Self, Error> {
        self.set_io_inner(socket)
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
    ///
    /// If the transport is still **unset**, the result is [`None`].
    pub fn is_write_closed(&self) -> Option<bool> {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map(|bio| bio.as_ref().write_eos)
    }

    /// Check if the underlying **transport** has closed its read end.
    ///
    /// If the transport is still **unset**, the result is [`None`].
    pub fn is_read_closed(&self) -> Option<bool> {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map(|bio| bio.as_ref().read_eos)
    }

    /// Check if the underlying **transport** has closed either its read end or its write end.
    ///
    /// If the transport is still **unset**, the result is [`None`].
    pub fn is_one_side_closed(&self) -> Option<bool> {
        self.get_connection_methods_ref()
            .bio
            .as_ref()
            .map(|bio| bio.as_ref().read_eos || bio.as_ref().write_eos)
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

        let code = unsafe {
            // Safety: inspecting the last error on an existing valid connection.
            bssl_sys::SSL_get_error(self.ptr(), rc)
        };
        match code {
            // Retransmission flight sent and timer state updated successfully; returns `Ok(true)`
            // to signal that an expired timeout was handled.
            bssl_sys::SSL_ERROR_NONE => Ok(true),

            // `DTLSv1_handle_timeout` only flushes outbound writes but we must have `rc != 0`,
            // so `SSL_ERROR_ZERO_RETURN` is unreachable here.
            bssl_sys::SSL_ERROR_ZERO_RETURN => {
                unreachable!("DTLSv1_handle_timeout only flushes outbound flights and rc != 0")
            }

            // When retransmitting flights on timeout, the write may suspend on `WantRead` or `WantWrite`.
            // The timeout was still successfully processed and retransmission initiated, so `Ok(true)`
            // is returned. Other retry reasons are not applicable here and are treated as errors.
            _ if let Ok(reason) = TlsRetryReason::try_from(code) => match reason {
                TlsRetryReason::WantWrite => Ok(true),
                TlsRetryReason::WantRead => {
                    unreachable!("DTLS should never want to read for timeout handling")
                }
                _ => Err(Error::Unknown(UnknownError("unknown dtls timeout error"))),
            },
            _ => Err(self.extract_tls_error(code)),
        }
    }

    /// Get connection's remaining DTLS timer timeout.
    ///
    /// If a timeout is in effect, this method returns the remaining [`Duration`].
    ///
    /// This function returns [`None`] when TLS does not have any pending flights.
    pub fn dtlsv1_get_timeout(&self) -> Option<Duration> {
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
                let secs = u64::try_from(timeval.tv_sec).unwrap_or(0);
                let usecs = u64::try_from(timeval.tv_usec).unwrap_or(0);
                Some(Duration::from_secs(secs) + Duration::from_micros(usecs))
            }
            0 => None,
            rc => {
                unreachable!("BoringSSL should never return {rc} when calling dtlsv1_get_timeout")
            }
        }
    }
}
