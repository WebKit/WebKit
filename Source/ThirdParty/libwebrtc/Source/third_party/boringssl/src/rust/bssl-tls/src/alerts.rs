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

//! TLS Alerts

use core::{
    ffi::{CStr, c_int},
    fmt,
};

use bssl_macros::bssl_enum;

bssl_enum! {
    #[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
    #[non_exhaustive]
    /// TLS alert description
    ///
    /// These alert variants have [IANA entries].
    ///
    /// [IANA entries]: <https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-6>
    pub enum AlertDescription : u8 {
        /// `close_notify`
        ///
        /// This alert notifies the recipient that the sender will not send any more messages on
        /// this connection.
        /// Any data received after a closure alert has been received MUST be ignored.
        CloseNotify = bssl_sys::SSL_AD_CLOSE_NOTIFY as u8,
        /// `unexpected_message`
        ///
        /// This alert signals a fatal error.
        /// The connection must be terminated.
        UnexpectedMessage = bssl_sys::SSL_AD_UNEXPECTED_MESSAGE as u8,
        /// `bad_record_mac`
        ///
        /// This alert signals a fatal decryption error.
        /// The connection must be terminated.
        BadRecordMac = bssl_sys::SSL_AD_BAD_RECORD_MAC as u8,
        /// `decryption_failed`, reserved and used up until TLS 1.3.
        DecryptionFailed = bssl_sys::SSL_AD_DECRYPTION_FAILED as u8,
        /// `record_overflow`
        ///
        /// This alert signals that a record of length more than 2^14 bytes
        /// was received.
        /// The connection must be terminated.
        RecordOverflow = bssl_sys::SSL_AD_RECORD_OVERFLOW as u8,
        /// `decompression_failure`, reserved and used up until TLS 1.3.
        DecompressionFailure = bssl_sys::SSL_AD_DECOMPRESSION_FAILURE as u8,
        /// `handshake_failure`
        ///
        /// This alert signals a fatal handshake error due to failure to negotiate a supported
        /// set of parameters.
        /// The handshake must be aborted.
        HandshakeFailure = bssl_sys::SSL_AD_HANDSHAKE_FAILURE as u8,
        /// `no_certificate`, reserved and used up until TLS 1.3.
        NoCertificate = bssl_sys::SSL_AD_NO_CERTIFICATE as u8,
        /// `bad_certificate`
        ///
        /// This alert signals a certificate verification error.
        /// The reason could be the use of insecure parameters such as `SHA-1` or `MD5` hash
        /// algorithms, or the signature verification failed.
        /// The handshake must be aborted.
        BadCertificate = bssl_sys::SSL_AD_BAD_CERTIFICATE as u8,
        /// `unsupported_certificate`
        ///
        /// This alert signals an unsupported certificate was transmitted.
        /// The connection must be terminated.
        UnsupportedCertificate = bssl_sys::SSL_AD_UNSUPPORTED_CERTIFICATE as u8,
        /// `certificate_revoked`
        ///
        /// This alert signals a certificate was revoked.
        /// The connection must be terminated.
        CertificateRevoked = bssl_sys::SSL_AD_CERTIFICATE_REVOKED as u8,
        /// `certificate_expired`
        ///
        /// This alert signals a certificate has expired.
        /// The connection must be terminated.
        CertificateExpired = bssl_sys::SSL_AD_CERTIFICATE_EXPIRED as u8,
        /// `certificate_unknown`
        ///
        /// This alert signals some other unspecified issue arose in processing the certificate,
        /// rendering it unacceptable.
        /// The connection must be terminated.
        CertificateUnknown = bssl_sys::SSL_AD_CERTIFICATE_UNKNOWN as u8,
        /// `illegal_parameter`
        ///
        /// A field in the handshake was incorrect or inconsistent with other fields.
        /// This alert is used for errors which conform to the formal protocol syntax,
        /// but are otherwise incorrect.
        /// The connection must be terminated.
        IllegalParameter = bssl_sys::SSL_AD_ILLEGAL_PARAMETER as u8,
        /// `unknown_ca`
        ///
        /// A valid certificate chain or partial chain was received,
        /// but the certificate was not accepted because the CA certificate could not be located
        /// or could not be matched with a known trust anchor.
        /// The connection must be terminated.
        UnknownCa = bssl_sys::SSL_AD_UNKNOWN_CA as u8,
        /// `access_denied`
        ///
        /// A valid certificate or PSK was received, but when access control was applied,
        /// the sender decided not to proceed with negotiation.
        /// The connection must be terminated.
        AccessDenied = bssl_sys::SSL_AD_ACCESS_DENIED as u8,
        /// `decode_error`
        ///
        /// A message could not be decoded because some field was out of the specified range or
        /// the length of the message was incorrect.
        /// This alert is used for errors where the message does not conform to the formal
        /// protocol syntax.
        /// This alert should never be observed in communication between proper implementations,
        /// except when messages were corrupted in the network.
        /// The connection must be terminated.
        DecodeError = bssl_sys::SSL_AD_DECODE_ERROR as u8,
        /// `decrypt_error`
        ///
        /// A handshake cryptographic operation, which is not on the record layer, has failed,
        /// including inability to correctly verify a signature, validate a `Finished` message
        /// or a PSK binder.
        /// The connection must be terminated.
        DecryptError = bssl_sys::SSL_AD_DECRYPT_ERROR as u8,
        /// `export_restriction`, reserved and used up until TLS 1.3.
        ///
        /// The connection must be terminated.
        ExportRestriction = bssl_sys::SSL_AD_EXPORT_RESTRICTION as u8,
        /// `protocol_version`
        ///
        /// The protocol version the peer has attempted to negotiate is recognized
        /// but not supported.
        /// The connection must be terminated.
        ProtocolVersion = bssl_sys::SSL_AD_PROTOCOL_VERSION as u8,
        /// `insufficient_security`
        ///
        /// Returned instead of "handshake_failure" when a negotiation has failed specifically
        /// because the server requires parameters more secure than those supported by the client.
        /// The connection must be terminated.
        InsufficientSecurity = bssl_sys::SSL_AD_INSUFFICIENT_SECURITY as u8,
        /// `internal_error`
        ///
        /// An internal error unrelated to the peer or the correctness of the protocol,
        /// such as a memory allocation failure, makes it impossible to continue.
        /// The connection must be terminated.
        InternalError = bssl_sys::SSL_AD_INTERNAL_ERROR as u8,
        /// `inappropriate_fallback`
        ///
        /// Sent by a server in response to an invalid connection retry attempt from a client.
        /// The connection must be terminated.
        InappropriateFallback = bssl_sys::SSL_AD_INAPPROPRIATE_FALLBACK as u8,
        /// `user_cancelled`
        ///
        /// This alert notifies the recipient that the sender is canceling the handshake
        /// for some reason unrelated to a protocol failure.
        UserCancelled = bssl_sys::SSL_AD_USER_CANCELLED as u8,
        /// `no_renegotiation`, reserved and used up until TLS 1.3.
        NoRenegotiation = bssl_sys::SSL_AD_NO_RENEGOTIATION as u8,
        /// `missing_extension`
        ///
        /// Sent by endpoints that receive a handshake message not containing an extension that
        /// is mandatory to send for the offered TLS version or other negotiated parameters.
        /// The connection must be terminated.
        MissingExtension = bssl_sys::SSL_AD_MISSING_EXTENSION as u8,
        /// `unsupported_extension`
        ///
        /// Sent by endpoints receiving any handshake message containing an extension in a
        /// `ServerHello`, `HelloRetryRequest`, `EncryptedExtensions`, or `Certificate` not first
        /// offered in the corresponding `ClientHello` or `CertificateRequest`.
        UnsupportedExtension = bssl_sys::SSL_AD_UNSUPPORTED_EXTENSION as u8,
        /// `certificate_unobtainable`, reserved and used up until TLS 1.3.
        CertificateUnobtainable = bssl_sys::SSL_AD_CERTIFICATE_UNOBTAINABLE as u8,
        /// `unrecognized_name`
        ///
        /// Sent by servers when no server exists identified by the name provided by the client via
        /// the `server_name` extension.
        UnrecognizedName = bssl_sys::SSL_AD_UNRECOGNIZED_NAME as u8,
        /// `bad_certificate_status_response`
        ///
        /// Sent by clients when an invalid or unacceptable OCSP response is provided by the server
        /// via the `status_request` extension.
        BadCertificateStatusResponse = bssl_sys::SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE as u8,
        /// `bad_certificate_hash_value`, reserved and used up until TLS 1.3.
        BadCertificateHashValue = bssl_sys::SSL_AD_BAD_CERTIFICATE_HASH_VALUE as u8,
        /// `unknown_psk_identity`
        ///
        /// Sent by servers when PSK key establishment is desired but no acceptable PSK identity is
        /// provided by the client.
        /// Sending this alert is optional.
        /// Servers may instead choose to send a `decrypt_error` alert to merely indicate an invalid
        /// PSK identity.
        UnknownPskIdentity = bssl_sys::SSL_AD_UNKNOWN_PSK_IDENTITY as u8,
        /// `certificate_required`
        ///
        /// Sent by servers when a client certificate is desired but none was provided by
        /// the client.
        CertificateRequired = bssl_sys::SSL_AD_CERTIFICATE_REQUIRED as u8,
        /// `no_application_protocol`
        ///
        /// Sent by servers when a client `application_layer_protocol_negotiation` extension
        /// advertises only protocols that the server does not support.
        NoApplicationProtocol = bssl_sys::SSL_AD_NO_APPLICATION_PROTOCOL as u8,
        /// `ech_required`
        ///
        /// The client must send this alert when it offered an `encrypted_client_hello` extension
        /// but was not accepted by the server.
        EchRequired = bssl_sys::SSL_AD_ECH_REQUIRED as u8,
    }
}

impl AlertDescription {
    /// Extract long description of the alert.
    pub fn get_description(self) -> &'static str {
        let cstr = unsafe {
            // Safety: we are only accessing static NUL-terminated string data that is plain ASCII.
            CStr::from_ptr(bssl_sys::SSL_alert_desc_string_long(self as c_int))
        };
        cstr.to_str()
            .expect("BoringSSL description string unexpectedly contains non-UTF-8 bytes")
    }
}

impl fmt::Display for AlertDescription {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.get_description())
    }
}
