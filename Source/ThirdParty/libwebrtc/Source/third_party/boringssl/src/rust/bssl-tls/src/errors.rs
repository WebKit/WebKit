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

//! TLS Errors

use alloc::boxed::Box;
use core::{
    any::Any,
    ffi::{
        CStr,
        c_int,
        c_uint, //
    },
    fmt::{
        Debug,
        Display,
        Formatter,
        Result as FmtResult, //
    }, //
};

use bssl_macros::bssl_enum;
use bssl_sys::LibCode;
use bssl_x509::errors::{
    PemReason,
    PkiError, //
};

use crate::config::ConfigurationError;

/// Top-level errors
#[derive(Debug)]
pub enum Error {
    /// Error reported by the BoringSSL library
    #[allow(private_interfaces)]
    Library(u32, Option<LibCode>, Option<i32>),
    /// Configuration errors
    Configuration(ConfigurationError),
    /// Other TLS errors with reason
    TlsReason(TlsErrorReason),
    /// PEM encoding failures
    PemReason(PemReason),
    /// Quic errors,
    Quic(QuicError),
    /// IO errors
    Io(IoError),
    /// PKI errors
    Pki(PkiError),
    /// Unknown error which should be reported as bug
    Unknown(Box<dyn Any + Send + Sync>),
}

impl From<PkiError> for Error {
    fn from(err: PkiError) -> Self {
        Self::Pki(err)
    }
}

impl Error {
    #[allow(irrefutable_let_patterns)]
    fn extract_err_from_code(packed_error: c_uint) -> Self {
        let lib = unsafe {
            // Safety: extracting error source does not have side-effect and only access static data.
            bssl_sys::ERR_GET_LIB(packed_error)
        };
        let Ok(lib) = i32::try_from(lib) else {
            return Self::Library(packed_error, None, None);
        };
        let Ok(lib) = LibCode::try_from(lib) else {
            return Self::Library(packed_error, None, None);
        };
        let reason = unsafe {
            // Safety: extracting error reason does not have side-effect and only access static data.
            bssl_sys::ERR_GET_REASON(packed_error)
        };
        let Ok(reason) = i32::try_from(reason) else {
            return Self::Library(packed_error, Some(lib), None);
        };
        let ret_unknown_reason = || Self::Library(packed_error, Some(lib), Some(reason));
        match lib {
            LibCode::Ssl => {
                let Ok(reason) = TlsErrorReason::try_from(reason) else {
                    return ret_unknown_reason();
                };
                Self::TlsReason(reason)
            }
            LibCode::Pem => {
                let Ok(reason) = PemReason::try_from(reason) else {
                    return ret_unknown_reason();
                };
                Self::PemReason(reason)
            }
            _ => Self::Library(packed_error, Some(lib), Some(reason)),
        }
    }

    fn is_trivial(&self) -> bool {
        matches!(self, Self::Library(0, _, _))
    }

    pub(crate) fn extract_lib_err() -> Self {
        let packed_error = unsafe {
            // Safety: extracting error code does not have side-effect
            bssl_sys::ERR_get_error()
        };
        let error = Self::extract_err_from_code(packed_error);
        unsafe {
            // Safety: we only clear the error queue on the current thread.
            bssl_sys::ERR_clear_error();
        }
        error
    }

    pub(crate) fn extract_tls_err(code: c_int) -> Result<TlsRetryReason, Self> {
        let lib_err = Self::extract_lib_err();
        if code == bssl_sys::SSL_ERROR_SSL {
            return Err(lib_err);
        }
        if let Ok(reason) = TlsRetryReason::try_from(code) {
            return Ok(reason);
        }
        if lib_err.is_trivial() {
            Err(Self::Unknown(Box::new(alloc::format!(
                "unknown tls error ({code})"
            ))))
        } else {
            Err(lib_err)
        }
    }
}

impl core::error::Error for Error {
    fn source(&self) -> Option<&(dyn core::error::Error + 'static)> {
        None
    }

    fn description(&self) -> &str {
        "BoringSSL error; use Display for details"
    }

    fn cause(&self) -> Option<&dyn core::error::Error> {
        self.source()
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            &Error::Library(code, _, _) => {
                // Here the buffer is 120 bytes and BoringSSL uses this buffer size
                // to hold the error string.
                // Therefore, this ought to be sufficient for a human-readable message.
                let mut err_str = [0; bssl_sys::ERR_ERROR_STRING_BUF_LEN as usize];
                unsafe {
                    // Safety:
                    // - err_str is non-null and valid, so we do not need FFI-specific pointer conversion.
                    bssl_sys::ERR_error_string_n(code, err_str.as_mut_ptr(), err_str.len());
                }
                let err_str = unsafe {
                    // Safety:
                    // - err_str is still valid;
                    // - `ERR_error_string_n` guarantees that
                    CStr::from_ptr(err_str.as_ptr())
                };
                f.write_str(&err_str.to_string_lossy())
            }
            Error::Configuration(err) => Display::fmt(err, f),
            Error::TlsReason(err) => Display::fmt(err, f),
            Error::PemReason(err) => Debug::fmt(err, f),
            Error::Quic(err) => Display::fmt(err, f),
            Error::Io(err) => Display::fmt(err, f),
            Error::Pki(err) => Display::fmt(err, f),
            Error::Unknown(err) => err.fmt(f),
        }
    }
}

bssl_enum! {
    /// SSL reason codes.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum TlsErrorReason: i32 {
        /// `SSL_R_APP_DATA_IN_HANDSHAKE`
        AppDataInHandshake = bssl_sys::SSL_R_APP_DATA_IN_HANDSHAKE as i32,
        /// `SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT`
        AttemptToReuseSessionInDifferentContext = bssl_sys::SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT as i32,
        /// `SSL_R_BAD_ALERT`
        BadAlert = bssl_sys::SSL_R_BAD_ALERT as i32,
        /// `SSL_R_BAD_CHANGE_CIPHER_SPEC`
        BadChangeCipherSpec = bssl_sys::SSL_R_BAD_CHANGE_CIPHER_SPEC as i32,
        /// `SSL_R_BAD_DATA_RETURNED_BY_CALLBACK`
        BadDataReturnedByCallback = bssl_sys::SSL_R_BAD_DATA_RETURNED_BY_CALLBACK as i32,
        /// `SSL_R_BAD_DH_P_LENGTH`
        BadDhPLength = bssl_sys::SSL_R_BAD_DH_P_LENGTH as i32,
        /// `SSL_R_BAD_DIGEST_LENGTH`
        BadDigestLength = bssl_sys::SSL_R_BAD_DIGEST_LENGTH as i32,
        /// `SSL_R_BAD_ECC_CERT`
        BadEccCert = bssl_sys::SSL_R_BAD_ECC_CERT as i32,
        /// `SSL_R_BAD_ECPOINT`
        BadEcpoint = bssl_sys::SSL_R_BAD_ECPOINT as i32,
        /// `SSL_R_BAD_HANDSHAKE_RECORD`
        BadHandshakeRecord = bssl_sys::SSL_R_BAD_HANDSHAKE_RECORD as i32,
        /// `SSL_R_BAD_HELLO_REQUEST`
        BadHelloRequest = bssl_sys::SSL_R_BAD_HELLO_REQUEST as i32,
        /// `SSL_R_BAD_LENGTH`
        BadLength = bssl_sys::SSL_R_BAD_LENGTH as i32,
        /// `SSL_R_BAD_PACKET_LENGTH`
        BadPacketLength = bssl_sys::SSL_R_BAD_PACKET_LENGTH as i32,
        /// `SSL_R_BAD_RSA_ENCRYPT`
        BadRsaEncrypt = bssl_sys::SSL_R_BAD_RSA_ENCRYPT as i32,
        /// `SSL_R_BAD_SIGNATURE`
        BadSignature = bssl_sys::SSL_R_BAD_SIGNATURE as i32,
        /// `SSL_R_BAD_SRTP_MKI_VALUE`
        BadSrtpMkiValue = bssl_sys::SSL_R_BAD_SRTP_MKI_VALUE as i32,
        /// `SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST`
        BadSrtpProtectionProfileList = bssl_sys::SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST as i32,
        /// `SSL_R_BAD_SSL_FILETYPE`
        BadSslFiletype = bssl_sys::SSL_R_BAD_SSL_FILETYPE as i32,
        /// `SSL_R_BAD_WRITE_RETRY`
        BadWriteRetry = bssl_sys::SSL_R_BAD_WRITE_RETRY as i32,
        /// `SSL_R_BIO_NOT_SET`
        BioNotSet = bssl_sys::SSL_R_BIO_NOT_SET as i32,
        /// `SSL_R_BN_LIB`
        BnLib = bssl_sys::SSL_R_BN_LIB as i32,
        /// `SSL_R_BUFFER_TOO_SMALL`
        BufferTooSmall = bssl_sys::SSL_R_BUFFER_TOO_SMALL as i32,
        /// `SSL_R_CA_DN_LENGTH_MISMATCH`
        CaDnLengthMismatch = bssl_sys::SSL_R_CA_DN_LENGTH_MISMATCH as i32,
        /// `SSL_R_CA_DN_TOO_LONG`
        CaDnTooLong = bssl_sys::SSL_R_CA_DN_TOO_LONG as i32,
        /// `SSL_R_CCS_RECEIVED_EARLY`
        CcsReceivedEarly = bssl_sys::SSL_R_CCS_RECEIVED_EARLY as i32,
        /// `SSL_R_CERTIFICATE_VERIFY_FAILED`
        CertificateVerifyFailed = bssl_sys::SSL_R_CERTIFICATE_VERIFY_FAILED as i32,
        /// `SSL_R_CERT_CB_ERROR`
        CertCbError = bssl_sys::SSL_R_CERT_CB_ERROR as i32,
        /// `SSL_R_CERT_LENGTH_MISMATCH`
        CertLengthMismatch = bssl_sys::SSL_R_CERT_LENGTH_MISMATCH as i32,
        /// `SSL_R_CHANNEL_ID_NOT_P256`
        ChannelIdNotP256 = bssl_sys::SSL_R_CHANNEL_ID_NOT_P256 as i32,
        /// `SSL_R_CHANNEL_ID_SIGNATURE_INVALID`
        ChannelIdSignatureInvalid = bssl_sys::SSL_R_CHANNEL_ID_SIGNATURE_INVALID as i32,
        /// `SSL_R_CIPHER_OR_HASH_UNAVAILABLE`
        CipherOrHashUnavailable = bssl_sys::SSL_R_CIPHER_OR_HASH_UNAVAILABLE as i32,
        /// `SSL_R_CLIENTHELLO_PARSE_FAILED`
        ClienthelloParseFailed = bssl_sys::SSL_R_CLIENTHELLO_PARSE_FAILED as i32,
        /// `SSL_R_CLIENTHELLO_TLSEXT`
        ClienthelloTlsext = bssl_sys::SSL_R_CLIENTHELLO_TLSEXT as i32,
        /// `SSL_R_CONNECTION_REJECTED`
        ConnectionRejected = bssl_sys::SSL_R_CONNECTION_REJECTED as i32,
        /// `SSL_R_CONNECTION_TYPE_NOT_SET`
        ConnectionTypeNotSet = bssl_sys::SSL_R_CONNECTION_TYPE_NOT_SET as i32,
        /// `SSL_R_CUSTOM_EXTENSION_ERROR`
        CustomExtensionError = bssl_sys::SSL_R_CUSTOM_EXTENSION_ERROR as i32,
        /// `SSL_R_DATA_LENGTH_TOO_LONG`
        DataLengthTooLong = bssl_sys::SSL_R_DATA_LENGTH_TOO_LONG as i32,
        /// `SSL_R_DECODE_ERROR`
        DecodeError = bssl_sys::SSL_R_DECODE_ERROR as i32,
        /// `SSL_R_DECRYPTION_FAILED`
        DecryptionFailed = bssl_sys::SSL_R_DECRYPTION_FAILED as i32,
        /// `SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC`
        DecryptionFailedOrBadRecordMac = bssl_sys::SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC as i32,
        /// `SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG`
        DhPublicValueLengthIsWrong = bssl_sys::SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG as i32,
        /// `SSL_R_DH_P_TOO_LONG`
        DhPTooLong = bssl_sys::SSL_R_DH_P_TOO_LONG as i32,
        /// `SSL_R_DIGEST_CHECK_FAILED`
        DigestCheckFailed = bssl_sys::SSL_R_DIGEST_CHECK_FAILED as i32,
        /// `SSL_R_DTLS_MESSAGE_TOO_BIG`
        DtlsMessageTooBig = bssl_sys::SSL_R_DTLS_MESSAGE_TOO_BIG as i32,
        /// `SSL_R_ECC_CERT_NOT_FOR_SIGNING`
        EccCertNotForSigning = bssl_sys::SSL_R_ECC_CERT_NOT_FOR_SIGNING as i32,
        /// `SSL_R_EMS_STATE_INCONSISTENT`
        EmsStateInconsistent = bssl_sys::SSL_R_EMS_STATE_INCONSISTENT as i32,
        /// `SSL_R_ENCRYPTED_LENGTH_TOO_LONG`
        EncryptedLengthTooLong = bssl_sys::SSL_R_ENCRYPTED_LENGTH_TOO_LONG as i32,
        /// `SSL_R_ERROR_ADDING_EXTENSION`
        ErrorAddingExtension = bssl_sys::SSL_R_ERROR_ADDING_EXTENSION as i32,
        /// `SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST`
        ErrorInReceivedCipherList = bssl_sys::SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST as i32,
        /// `SSL_R_ERROR_PARSING_EXTENSION`
        ErrorParsingExtension = bssl_sys::SSL_R_ERROR_PARSING_EXTENSION as i32,
        /// `SSL_R_EXCESSIVE_MESSAGE_SIZE`
        ExcessiveMessageSize = bssl_sys::SSL_R_EXCESSIVE_MESSAGE_SIZE as i32,
        /// `SSL_R_EXTRA_DATA_IN_MESSAGE`
        ExtraDataInMessage = bssl_sys::SSL_R_EXTRA_DATA_IN_MESSAGE as i32,
        /// `SSL_R_FRAGMENT_MISMATCH`
        FragmentMismatch = bssl_sys::SSL_R_FRAGMENT_MISMATCH as i32,
        /// `SSL_R_GOT_NEXT_PROTO_WITHOUT_EXTENSION`
        GotNextProtoWithoutExtension = bssl_sys::SSL_R_GOT_NEXT_PROTO_WITHOUT_EXTENSION as i32,
        /// `SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO`
        HandshakeFailureOnClientHello = bssl_sys::SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO as i32,
        /// `SSL_R_HTTPS_PROXY_REQUEST`
        HttpsProxyRequest = bssl_sys::SSL_R_HTTPS_PROXY_REQUEST as i32,
        /// `SSL_R_HTTP_REQUEST`
        HttpRequest = bssl_sys::SSL_R_HTTP_REQUEST as i32,
        /// `SSL_R_INAPPROPRIATE_FALLBACK`
        InappropriateFallback = bssl_sys::SSL_R_INAPPROPRIATE_FALLBACK as i32,
        /// `SSL_R_INVALID_COMMAND`
        InvalidCommand = bssl_sys::SSL_R_INVALID_COMMAND as i32,
        /// `SSL_R_INVALID_MESSAGE`
        InvalidMessage = bssl_sys::SSL_R_INVALID_MESSAGE as i32,
        /// `SSL_R_INVALID_SSL_SESSION`
        InvalidSslSession = bssl_sys::SSL_R_INVALID_SSL_SESSION as i32,
        /// `SSL_R_INVALID_TICKET_KEYS_LENGTH`
        InvalidTicketKeysLength = bssl_sys::SSL_R_INVALID_TICKET_KEYS_LENGTH as i32,
        /// `SSL_R_LENGTH_MISMATCH`
        LengthMismatch = bssl_sys::SSL_R_LENGTH_MISMATCH as i32,
        /// `SSL_R_MISSING_EXTENSION`
        MissingExtension = bssl_sys::SSL_R_MISSING_EXTENSION as i32,
        /// `SSL_R_MISSING_RSA_CERTIFICATE`
        MissingRsaCertificate = bssl_sys::SSL_R_MISSING_RSA_CERTIFICATE as i32,
        /// `SSL_R_MISSING_TMP_DH_KEY`
        MissingTmpDhKey = bssl_sys::SSL_R_MISSING_TMP_DH_KEY as i32,
        /// `SSL_R_MISSING_TMP_ECDH_KEY`
        MissingTmpEcdhKey = bssl_sys::SSL_R_MISSING_TMP_ECDH_KEY as i32,
        /// `SSL_R_MIXED_SPECIAL_OPERATOR_WITH_GROUPS`
        MixedSpecialOperatorWithGroups = bssl_sys::SSL_R_MIXED_SPECIAL_OPERATOR_WITH_GROUPS as i32,
        /// `SSL_R_MTU_TOO_SMALL`
        MtuTooSmall = bssl_sys::SSL_R_MTU_TOO_SMALL as i32,
        /// `SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN`
        NegotiatedBothNpnAndAlpn = bssl_sys::SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN as i32,
        /// `SSL_R_NESTED_GROUP`
        NestedGroup = bssl_sys::SSL_R_NESTED_GROUP as i32,
        /// `SSL_R_NO_CERTIFICATES_RETURNED`
        NoCertificatesReturned = bssl_sys::SSL_R_NO_CERTIFICATES_RETURNED as i32,
        /// `SSL_R_NO_CERTIFICATE_ASSIGNED`
        NoCertificateAssigned = bssl_sys::SSL_R_NO_CERTIFICATE_ASSIGNED as i32,
        /// `SSL_R_NO_CERTIFICATE_SET`
        NoCertificateSet = bssl_sys::SSL_R_NO_CERTIFICATE_SET as i32,
        /// `SSL_R_NO_CIPHERS_AVAILABLE`
        NoCiphersAvailable = bssl_sys::SSL_R_NO_CIPHERS_AVAILABLE as i32,
        /// `SSL_R_NO_CIPHERS_PASSED`
        NoCiphersPassed = bssl_sys::SSL_R_NO_CIPHERS_PASSED as i32,
        /// `SSL_R_NO_CIPHER_MATCH`
        NoCipherMatch = bssl_sys::SSL_R_NO_CIPHER_MATCH as i32,
        /// `SSL_R_NO_COMPRESSION_SPECIFIED`
        NoCompressionSpecified = bssl_sys::SSL_R_NO_COMPRESSION_SPECIFIED as i32,
        /// `SSL_R_NO_METHOD_SPECIFIED`
        NoMethodSpecified = bssl_sys::SSL_R_NO_METHOD_SPECIFIED as i32,
        /// `SSL_R_NO_PRIVATE_KEY_ASSIGNED`
        NoPrivateKeyAssigned = bssl_sys::SSL_R_NO_PRIVATE_KEY_ASSIGNED as i32,
        /// `SSL_R_NO_RENEGOTIATION`
        NoRenegotiation = bssl_sys::SSL_R_NO_RENEGOTIATION as i32,
        /// `SSL_R_NO_REQUIRED_DIGEST`
        NoRequiredDigest = bssl_sys::SSL_R_NO_REQUIRED_DIGEST as i32,
        /// `SSL_R_NO_SHARED_CIPHER`
        NoSharedCipher = bssl_sys::SSL_R_NO_SHARED_CIPHER as i32,
        /// `SSL_R_NULL_SSL_CTX`
        NullSslCtx = bssl_sys::SSL_R_NULL_SSL_CTX as i32,
        /// `SSL_R_NULL_SSL_METHOD_PASSED`
        NullSslMethodPassed = bssl_sys::SSL_R_NULL_SSL_METHOD_PASSED as i32,
        /// `SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED`
        OldSessionCipherNotReturned = bssl_sys::SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED as i32,
        /// `SSL_R_OLD_SESSION_VERSION_NOT_RETURNED`
        OldSessionVersionNotReturned = bssl_sys::SSL_R_OLD_SESSION_VERSION_NOT_RETURNED as i32,
        /// `SSL_R_OUTPUT_ALIASES_INPUT`
        OutputAliasesInput = bssl_sys::SSL_R_OUTPUT_ALIASES_INPUT as i32,
        /// `SSL_R_PARSE_TLSEXT`
        ParseTlsext = bssl_sys::SSL_R_PARSE_TLSEXT as i32,
        /// `SSL_R_PATH_TOO_LONG`
        PathTooLong = bssl_sys::SSL_R_PATH_TOO_LONG as i32,
        /// `SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE`
        PeerDidNotReturnACertificate = bssl_sys::SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE as i32,
        /// `SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE`
        PeerErrorUnsupportedCertificateType = bssl_sys::SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE as i32,
        /// `SSL_R_PROTOCOL_IS_SHUTDOWN`
        ProtocolIsShutdown = bssl_sys::SSL_R_PROTOCOL_IS_SHUTDOWN as i32,
        /// `SSL_R_PSK_IDENTITY_NOT_FOUND`
        PskIdentityNotFound = bssl_sys::SSL_R_PSK_IDENTITY_NOT_FOUND as i32,
        /// `SSL_R_PSK_NO_CLIENT_CB`
        PskNoClientCb = bssl_sys::SSL_R_PSK_NO_CLIENT_CB as i32,
        /// `SSL_R_PSK_NO_SERVER_CB`
        PskNoServerCb = bssl_sys::SSL_R_PSK_NO_SERVER_CB as i32,
        /// `SSL_R_READ_TIMEOUT_EXPIRED`
        ReadTimeoutExpired = bssl_sys::SSL_R_READ_TIMEOUT_EXPIRED as i32,
        /// `SSL_R_RECORD_LENGTH_MISMATCH`
        RecordLengthMismatch = bssl_sys::SSL_R_RECORD_LENGTH_MISMATCH as i32,
        /// `SSL_R_RECORD_TOO_LARGE`
        RecordTooLarge = bssl_sys::SSL_R_RECORD_TOO_LARGE as i32,
        /// `SSL_R_RENEGOTIATION_ENCODING_ERR`
        RenegotiationEncodingErr = bssl_sys::SSL_R_RENEGOTIATION_ENCODING_ERR as i32,
        /// `SSL_R_RENEGOTIATION_MISMATCH`
        RenegotiationMismatch = bssl_sys::SSL_R_RENEGOTIATION_MISMATCH as i32,
        /// `SSL_R_REQUIRED_CIPHER_MISSING`
        RequiredCipherMissing = bssl_sys::SSL_R_REQUIRED_CIPHER_MISSING as i32,
        /// `SSL_R_RESUMED_EMS_SESSION_WITHOUT_EMS_EXTENSION`
        ResumedEmsSessionWithoutEmsExtension = bssl_sys::SSL_R_RESUMED_EMS_SESSION_WITHOUT_EMS_EXTENSION as i32,
        /// `SSL_R_RESUMED_NON_EMS_SESSION_WITH_EMS_EXTENSION`
        ResumedNonEmsSessionWithEmsExtension = bssl_sys::SSL_R_RESUMED_NON_EMS_SESSION_WITH_EMS_EXTENSION as i32,
        /// `SSL_R_SCSV_RECEIVED_WHEN_RENEGOTIATING`
        ScsvReceivedWhenRenegotiating = bssl_sys::SSL_R_SCSV_RECEIVED_WHEN_RENEGOTIATING as i32,
        /// `SSL_R_SERVERHELLO_TLSEXT`
        ServerhelloTlsext = bssl_sys::SSL_R_SERVERHELLO_TLSEXT as i32,
        /// `SSL_R_SESSION_ID_CONTEXT_UNINITIALIZED`
        SessionIdContextUninitialized = bssl_sys::SSL_R_SESSION_ID_CONTEXT_UNINITIALIZED as i32,
        /// `SSL_R_SESSION_MAY_NOT_BE_CREATED`
        SessionMayNotBeCreated = bssl_sys::SSL_R_SESSION_MAY_NOT_BE_CREATED as i32,
        /// `SSL_R_SIGNATURE_ALGORITHMS_EXTENSION_SENT_BY_SERVER`
        SignatureAlgorithmsExtensionSentByServer = bssl_sys::SSL_R_SIGNATURE_ALGORITHMS_EXTENSION_SENT_BY_SERVER as i32,
        /// `SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES`
        SrtpCouldNotAllocateProfiles = bssl_sys::SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES as i32,
        /// `SSL_R_SRTP_UNKNOWN_PROTECTION_PROFILE`
        SrtpUnknownProtectionProfile = bssl_sys::SSL_R_SRTP_UNKNOWN_PROTECTION_PROFILE as i32,
        /// `SSL_R_SSL3_EXT_INVALID_SERVERNAME`
        Ssl3ExtInvalidServername = bssl_sys::SSL_R_SSL3_EXT_INVALID_SERVERNAME as i32,
        /// `SSL_R_SSL_CTX_HAS_NO_DEFAULT_SSL_VERSION`
        SslCtxHasNoDefaultSslVersion = bssl_sys::SSL_R_SSL_CTX_HAS_NO_DEFAULT_SSL_VERSION as i32,
        /// `SSL_R_SSL_HANDSHAKE_FAILURE`
        SslHandshakeFailure = bssl_sys::SSL_R_SSL_HANDSHAKE_FAILURE as i32,
        /// `SSL_R_SSL_SESSION_ID_CONTEXT_TOO_LONG`
        SslSessionIdContextTooLong = bssl_sys::SSL_R_SSL_SESSION_ID_CONTEXT_TOO_LONG as i32,
        /// `SSL_R_TLS_PEER_DID_NOT_RESPOND_WITH_CERTIFICATE_LIST`
        TlsPeerDidNotRespondWithCertificateList = bssl_sys::SSL_R_TLS_PEER_DID_NOT_RESPOND_WITH_CERTIFICATE_LIST as i32,
        /// `SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG`
        TlsRsaEncryptedValueLengthIsWrong = bssl_sys::SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG as i32,
        /// `SSL_R_TOO_MANY_EMPTY_FRAGMENTS`
        TooManyEmptyFragments = bssl_sys::SSL_R_TOO_MANY_EMPTY_FRAGMENTS as i32,
        /// `SSL_R_TOO_MANY_WARNING_ALERTS`
        TooManyWarningAlerts = bssl_sys::SSL_R_TOO_MANY_WARNING_ALERTS as i32,
        /// `SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS`
        UnableToFindEcdhParameters = bssl_sys::SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS as i32,
        /// `SSL_R_UNEXPECTED_EXTENSION`
        UnexpectedExtension = bssl_sys::SSL_R_UNEXPECTED_EXTENSION as i32,
        /// `SSL_R_UNEXPECTED_MESSAGE`
        UnexpectedMessage = bssl_sys::SSL_R_UNEXPECTED_MESSAGE as i32,
        /// `SSL_R_UNEXPECTED_OPERATOR_IN_GROUP`
        UnexpectedOperatorInGroup = bssl_sys::SSL_R_UNEXPECTED_OPERATOR_IN_GROUP as i32,
        /// `SSL_R_UNEXPECTED_RECORD`
        UnexpectedRecord = bssl_sys::SSL_R_UNEXPECTED_RECORD as i32,
        /// `SSL_R_UNINITIALIZED`
        Uninitialized = bssl_sys::SSL_R_UNINITIALIZED as i32,
        /// `SSL_R_UNKNOWN_ALERT_TYPE`
        UnknownAlertType = bssl_sys::SSL_R_UNKNOWN_ALERT_TYPE as i32,
        /// `SSL_R_UNKNOWN_CERTIFICATE_TYPE`
        UnknownCertificateType = bssl_sys::SSL_R_UNKNOWN_CERTIFICATE_TYPE as i32,
        /// `SSL_R_UNKNOWN_CIPHER_RETURNED`
        UnknownCipherReturned = bssl_sys::SSL_R_UNKNOWN_CIPHER_RETURNED as i32,
        /// `SSL_R_UNKNOWN_CIPHER_TYPE`
        UnknownCipherType = bssl_sys::SSL_R_UNKNOWN_CIPHER_TYPE as i32,
        /// `SSL_R_UNKNOWN_DIGEST`
        UnknownDigest = bssl_sys::SSL_R_UNKNOWN_DIGEST as i32,
        /// `SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE`
        UnknownKeyExchangeType = bssl_sys::SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE as i32,
        /// `SSL_R_UNKNOWN_PROTOCOL`
        UnknownProtocol = bssl_sys::SSL_R_UNKNOWN_PROTOCOL as i32,
        /// `SSL_R_UNKNOWN_SSL_VERSION`
        UnknownSslVersion = bssl_sys::SSL_R_UNKNOWN_SSL_VERSION as i32,
        /// `SSL_R_UNKNOWN_STATE`
        UnknownState = bssl_sys::SSL_R_UNKNOWN_STATE as i32,
        /// `SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED`
        UnsafeLegacyRenegotiationDisabled = bssl_sys::SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED as i32,
        /// `SSL_R_UNSUPPORTED_CIPHER`
        UnsupportedCipher = bssl_sys::SSL_R_UNSUPPORTED_CIPHER as i32,
        /// `SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM`
        UnsupportedCompressionAlgorithm = bssl_sys::SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM as i32,
        /// `SSL_R_UNSUPPORTED_ELLIPTIC_CURVE`
        UnsupportedEllipticCurve = bssl_sys::SSL_R_UNSUPPORTED_ELLIPTIC_CURVE as i32,
        /// `SSL_R_UNSUPPORTED_PROTOCOL`
        UnsupportedProtocol = bssl_sys::SSL_R_UNSUPPORTED_PROTOCOL as i32,
        /// `SSL_R_WRONG_CERTIFICATE_TYPE`
        WrongCertificateType = bssl_sys::SSL_R_WRONG_CERTIFICATE_TYPE as i32,
        /// `SSL_R_WRONG_CIPHER_RETURNED`
        WrongCipherReturned = bssl_sys::SSL_R_WRONG_CIPHER_RETURNED as i32,
        /// `SSL_R_WRONG_CURVE`
        WrongCurve = bssl_sys::SSL_R_WRONG_CURVE as i32,
        /// `SSL_R_WRONG_MESSAGE_TYPE`
        WrongMessageType = bssl_sys::SSL_R_WRONG_MESSAGE_TYPE as i32,
        /// `SSL_R_WRONG_SIGNATURE_TYPE`
        WrongSignatureType = bssl_sys::SSL_R_WRONG_SIGNATURE_TYPE as i32,
        /// `SSL_R_WRONG_SSL_VERSION`
        WrongSslVersion = bssl_sys::SSL_R_WRONG_SSL_VERSION as i32,
        /// `SSL_R_WRONG_VERSION_NUMBER`
        WrongVersionNumber = bssl_sys::SSL_R_WRONG_VERSION_NUMBER as i32,
        /// `SSL_R_X509_LIB`
        X509Lib = bssl_sys::SSL_R_X509_LIB as i32,
        /// `SSL_R_X509_VERIFICATION_SETUP_PROBLEMS`
        X509VerificationSetupProblems = bssl_sys::SSL_R_X509_VERIFICATION_SETUP_PROBLEMS as i32,
        /// `SSL_R_SHUTDOWN_WHILE_IN_INIT`
        ShutdownWhileInInit = bssl_sys::SSL_R_SHUTDOWN_WHILE_IN_INIT as i32,
        /// `SSL_R_INVALID_OUTER_RECORD_TYPE`
        InvalidOuterRecordType = bssl_sys::SSL_R_INVALID_OUTER_RECORD_TYPE as i32,
        /// `SSL_R_UNSUPPORTED_PROTOCOL_FOR_CUSTOM_KEY`
        UnsupportedProtocolForCustomKey = bssl_sys::SSL_R_UNSUPPORTED_PROTOCOL_FOR_CUSTOM_KEY as i32,
        /// `SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS`
        NoCommonSignatureAlgorithms = bssl_sys::SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS as i32,
        /// `SSL_R_DOWNGRADE_DETECTED`
        DowngradeDetected = bssl_sys::SSL_R_DOWNGRADE_DETECTED as i32,
        /// `SSL_R_EXCESS_HANDSHAKE_DATA`
        ExcessHandshakeData = bssl_sys::SSL_R_EXCESS_HANDSHAKE_DATA as i32,
        /// `SSL_R_INVALID_COMPRESSION_LIST`
        InvalidCompressionList = bssl_sys::SSL_R_INVALID_COMPRESSION_LIST as i32,
        /// `SSL_R_DUPLICATE_EXTENSION`
        DuplicateExtension = bssl_sys::SSL_R_DUPLICATE_EXTENSION as i32,
        /// `SSL_R_MISSING_KEY_SHARE`
        MissingKeyShare = bssl_sys::SSL_R_MISSING_KEY_SHARE as i32,
        /// `SSL_R_INVALID_ALPN_PROTOCOL`
        InvalidAlpnProtocol = bssl_sys::SSL_R_INVALID_ALPN_PROTOCOL as i32,
        /// `SSL_R_TOO_MANY_KEY_UPDATES`
        TooManyKeyUpdates = bssl_sys::SSL_R_TOO_MANY_KEY_UPDATES as i32,
        /// `SSL_R_BLOCK_CIPHER_PAD_IS_WRONG`
        BlockCipherPadIsWrong = bssl_sys::SSL_R_BLOCK_CIPHER_PAD_IS_WRONG as i32,
        /// `SSL_R_NO_CIPHERS_SPECIFIED`
        NoCiphersSpecified = bssl_sys::SSL_R_NO_CIPHERS_SPECIFIED as i32,
        /// `SSL_R_RENEGOTIATION_EMS_MISMATCH`
        RenegotiationEmsMismatch = bssl_sys::SSL_R_RENEGOTIATION_EMS_MISMATCH as i32,
        /// `SSL_R_DUPLICATE_KEY_SHARE`
        DuplicateKeyShare = bssl_sys::SSL_R_DUPLICATE_KEY_SHARE as i32,
        /// `SSL_R_NO_GROUPS_SPECIFIED`
        NoGroupsSpecified = bssl_sys::SSL_R_NO_GROUPS_SPECIFIED as i32,
        /// `SSL_R_NO_SHARED_GROUP`
        NoSharedGroup = bssl_sys::SSL_R_NO_SHARED_GROUP as i32,
        /// `SSL_R_PRE_SHARED_KEY_MUST_BE_LAST`
        PreSharedKeyMustBeLast = bssl_sys::SSL_R_PRE_SHARED_KEY_MUST_BE_LAST as i32,
        /// `SSL_R_OLD_SESSION_PRF_HASH_MISMATCH`
        OldSessionPrfHashMismatch = bssl_sys::SSL_R_OLD_SESSION_PRF_HASH_MISMATCH as i32,
        /// `SSL_R_INVALID_SCT_LIST`
        InvalidSctList = bssl_sys::SSL_R_INVALID_SCT_LIST as i32,
        /// `SSL_R_TOO_MUCH_SKIPPED_EARLY_DATA`
        TooMuchSkippedEarlyData = bssl_sys::SSL_R_TOO_MUCH_SKIPPED_EARLY_DATA as i32,
        /// `SSL_R_PSK_IDENTITY_BINDER_COUNT_MISMATCH`
        PskIdentityBinderCountMismatch = bssl_sys::SSL_R_PSK_IDENTITY_BINDER_COUNT_MISMATCH as i32,
        /// `SSL_R_CANNOT_PARSE_LEAF_CERT`
        CannotParseLeafCert = bssl_sys::SSL_R_CANNOT_PARSE_LEAF_CERT as i32,
        /// `SSL_R_SERVER_CERT_CHANGED`
        ServerCertChanged = bssl_sys::SSL_R_SERVER_CERT_CHANGED as i32,
        /// `SSL_R_CERTIFICATE_AND_PRIVATE_KEY_MISMATCH`
        CertificateAndPrivateKeyMismatch = bssl_sys::SSL_R_CERTIFICATE_AND_PRIVATE_KEY_MISMATCH as i32,
        /// `SSL_R_CANNOT_HAVE_BOTH_PRIVKEY_AND_METHOD`
        CannotHaveBothPrivkeyAndMethod = bssl_sys::SSL_R_CANNOT_HAVE_BOTH_PRIVKEY_AND_METHOD as i32,
        /// `SSL_R_TICKET_ENCRYPTION_FAILED`
        TicketEncryptionFailed = bssl_sys::SSL_R_TICKET_ENCRYPTION_FAILED as i32,
        /// `SSL_R_ALPN_MISMATCH_ON_EARLY_DATA`
        AlpnMismatchOnEarlyData = bssl_sys::SSL_R_ALPN_MISMATCH_ON_EARLY_DATA as i32,
        /// `SSL_R_WRONG_VERSION_ON_EARLY_DATA`
        WrongVersionOnEarlyData = bssl_sys::SSL_R_WRONG_VERSION_ON_EARLY_DATA as i32,
        /// `SSL_R_UNEXPECTED_EXTENSION_ON_EARLY_DATA`
        UnexpectedExtensionOnEarlyData = bssl_sys::SSL_R_UNEXPECTED_EXTENSION_ON_EARLY_DATA as i32,
        /// `SSL_R_NO_SUPPORTED_VERSIONS_ENABLED`
        NoSupportedVersionsEnabled = bssl_sys::SSL_R_NO_SUPPORTED_VERSIONS_ENABLED as i32,
        /// `SSL_R_EMPTY_HELLO_RETRY_REQUEST`
        EmptyHelloRetryRequest = bssl_sys::SSL_R_EMPTY_HELLO_RETRY_REQUEST as i32,
        /// `SSL_R_EARLY_DATA_NOT_IN_USE`
        EarlyDataNotInUse = bssl_sys::SSL_R_EARLY_DATA_NOT_IN_USE as i32,
        /// `SSL_R_HANDSHAKE_NOT_COMPLETE`
        HandshakeNotComplete = bssl_sys::SSL_R_HANDSHAKE_NOT_COMPLETE as i32,
        /// `SSL_R_NEGOTIATED_TB_WITHOUT_EMS_OR_RI`
        NegotiatedTbWithoutEmsOrRi = bssl_sys::SSL_R_NEGOTIATED_TB_WITHOUT_EMS_OR_RI as i32,
        /// `SSL_R_SERVER_ECHOED_INVALID_SESSION_ID`
        ServerEchoedInvalidSessionId = bssl_sys::SSL_R_SERVER_ECHOED_INVALID_SESSION_ID as i32,
        /// `SSL_R_PRIVATE_KEY_OPERATION_FAILED`
        PrivateKeyOperationFailed = bssl_sys::SSL_R_PRIVATE_KEY_OPERATION_FAILED as i32,
        /// `SSL_R_SECOND_SERVERHELLO_VERSION_MISMATCH`
        SecondServerhelloVersionMismatch = bssl_sys::SSL_R_SECOND_SERVERHELLO_VERSION_MISMATCH as i32,
        /// `SSL_R_OCSP_CB_ERROR`
        OcspCbError = bssl_sys::SSL_R_OCSP_CB_ERROR as i32,
        /// `SSL_R_SSL_SESSION_ID_TOO_LONG`
        SslSessionIdTooLong = bssl_sys::SSL_R_SSL_SESSION_ID_TOO_LONG as i32,
        /// `SSL_R_APPLICATION_DATA_ON_SHUTDOWN`
        ApplicationDataOnShutdown = bssl_sys::SSL_R_APPLICATION_DATA_ON_SHUTDOWN as i32,
        /// `SSL_R_CERT_DECOMPRESSION_FAILED`
        CertDecompressionFailed = bssl_sys::SSL_R_CERT_DECOMPRESSION_FAILED as i32,
        /// `SSL_R_UNCOMPRESSED_CERT_TOO_LARGE`
        UncompressedCertTooLarge = bssl_sys::SSL_R_UNCOMPRESSED_CERT_TOO_LARGE as i32,
        /// `SSL_R_UNKNOWN_CERT_COMPRESSION_ALG`
        UnknownCertCompressionAlg = bssl_sys::SSL_R_UNKNOWN_CERT_COMPRESSION_ALG as i32,
        /// `SSL_R_INVALID_SIGNATURE_ALGORITHM`
        InvalidSignatureAlgorithm = bssl_sys::SSL_R_INVALID_SIGNATURE_ALGORITHM as i32,
        /// `SSL_R_DUPLICATE_SIGNATURE_ALGORITHM`
        DuplicateSignatureAlgorithm = bssl_sys::SSL_R_DUPLICATE_SIGNATURE_ALGORITHM as i32,
        /// `SSL_R_TLS13_DOWNGRADE`
        Tls13Downgrade = bssl_sys::SSL_R_TLS13_DOWNGRADE as i32,
        /// `SSL_R_QUIC_INTERNAL_ERROR`
        QuicInternalError = bssl_sys::SSL_R_QUIC_INTERNAL_ERROR as i32,
        /// `SSL_R_WRONG_ENCRYPTION_LEVEL_RECEIVED`
        WrongEncryptionLevelReceived = bssl_sys::SSL_R_WRONG_ENCRYPTION_LEVEL_RECEIVED as i32,
        /// `SSL_R_TOO_MUCH_READ_EARLY_DATA`
        TooMuchReadEarlyData = bssl_sys::SSL_R_TOO_MUCH_READ_EARLY_DATA as i32,
        /// `SSL_R_INVALID_DELEGATED_CREDENTIAL`
        InvalidDelegatedCredential = bssl_sys::SSL_R_INVALID_DELEGATED_CREDENTIAL as i32,
        /// `SSL_R_KEY_USAGE_BIT_INCORRECT`
        KeyUsageBitIncorrect = bssl_sys::SSL_R_KEY_USAGE_BIT_INCORRECT as i32,
        /// `SSL_R_INCONSISTENT_CLIENT_HELLO`
        InconsistentClientHello = bssl_sys::SSL_R_INCONSISTENT_CLIENT_HELLO as i32,
        /// `SSL_R_CIPHER_MISMATCH_ON_EARLY_DATA`
        CipherMismatchOnEarlyData = bssl_sys::SSL_R_CIPHER_MISMATCH_ON_EARLY_DATA as i32,
        /// `SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED`
        QuicTransportParametersMisconfigured = bssl_sys::SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED as i32,
        /// `SSL_R_UNEXPECTED_COMPATIBILITY_MODE`
        UnexpectedCompatibilityMode = bssl_sys::SSL_R_UNEXPECTED_COMPATIBILITY_MODE as i32,
        /// `SSL_R_NO_APPLICATION_PROTOCOL`
        NoApplicationProtocol = bssl_sys::SSL_R_NO_APPLICATION_PROTOCOL as i32,
        /// `SSL_R_NEGOTIATED_ALPS_WITHOUT_ALPN`
        NegotiatedAlpsWithoutAlpn = bssl_sys::SSL_R_NEGOTIATED_ALPS_WITHOUT_ALPN as i32,
        /// `SSL_R_ALPS_MISMATCH_ON_EARLY_DATA`
        AlpsMismatchOnEarlyData = bssl_sys::SSL_R_ALPS_MISMATCH_ON_EARLY_DATA as i32,
        /// `SSL_R_ECH_SERVER_CONFIG_AND_PRIVATE_KEY_MISMATCH`
        EchServerConfigAndPrivateKeyMismatch = bssl_sys::SSL_R_ECH_SERVER_CONFIG_AND_PRIVATE_KEY_MISMATCH as i32,
        /// `SSL_R_ECH_SERVER_CONFIG_UNSUPPORTED_EXTENSION`
        EchServerConfigUnsupportedExtension = bssl_sys::SSL_R_ECH_SERVER_CONFIG_UNSUPPORTED_EXTENSION as i32,
        /// `SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG`
        UnsupportedEchServerConfig = bssl_sys::SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG as i32,
        /// `SSL_R_ECH_SERVER_WOULD_HAVE_NO_RETRY_CONFIGS`
        EchServerWouldHaveNoRetryConfigs = bssl_sys::SSL_R_ECH_SERVER_WOULD_HAVE_NO_RETRY_CONFIGS as i32,
        /// `SSL_R_INVALID_CLIENT_HELLO_INNER`
        InvalidClientHelloInner = bssl_sys::SSL_R_INVALID_CLIENT_HELLO_INNER as i32,
        /// `SSL_R_INVALID_ALPN_PROTOCOL_LIST`
        InvalidAlpnProtocolList = bssl_sys::SSL_R_INVALID_ALPN_PROTOCOL_LIST as i32,
        /// `SSL_R_COULD_NOT_PARSE_HINTS`
        CouldNotParseHints = bssl_sys::SSL_R_COULD_NOT_PARSE_HINTS as i32,
        /// `SSL_R_INVALID_ECH_PUBLIC_NAME`
        InvalidEchPublicName = bssl_sys::SSL_R_INVALID_ECH_PUBLIC_NAME as i32,
        /// `SSL_R_INVALID_ECH_CONFIG_LIST`
        InvalidEchConfigList = bssl_sys::SSL_R_INVALID_ECH_CONFIG_LIST as i32,
        /// `SSL_R_ECH_REJECTED`
        EchRejected = bssl_sys::SSL_R_ECH_REJECTED as i32,
        /// `SSL_R_INVALID_OUTER_EXTENSION`
        InvalidOuterExtension = bssl_sys::SSL_R_INVALID_OUTER_EXTENSION as i32,
        /// `SSL_R_INCONSISTENT_ECH_NEGOTIATION`
        InconsistentEchNegotiation = bssl_sys::SSL_R_INCONSISTENT_ECH_NEGOTIATION as i32,
        /// `SSL_R_INVALID_ALPS_CODEPOINT`
        InvalidAlpsCodepoint = bssl_sys::SSL_R_INVALID_ALPS_CODEPOINT as i32,
        /// `SSL_R_NO_MATCHING_ISSUER`
        NoMatchingIssuer = bssl_sys::SSL_R_NO_MATCHING_ISSUER as i32,
        /// `SSL_R_INVALID_SPAKE2PLUSV1_VALUE`
        InvalidSpake2plusv1Value = bssl_sys::SSL_R_INVALID_SPAKE2PLUSV1_VALUE as i32,
        /// `SSL_R_PAKE_EXHAUSTED`
        PakeExhausted = bssl_sys::SSL_R_PAKE_EXHAUSTED as i32,
        /// `SSL_R_PEER_PAKE_MISMATCH`
        PeerPakeMismatch = bssl_sys::SSL_R_PEER_PAKE_MISMATCH as i32,
        /// `SSL_R_UNSUPPORTED_CREDENTIAL_LIST`
        UnsupportedCredentialList = bssl_sys::SSL_R_UNSUPPORTED_CREDENTIAL_LIST as i32,
        /// `SSL_R_INVALID_TRUST_ANCHOR_LIST`
        InvalidTrustAnchorList = bssl_sys::SSL_R_INVALID_TRUST_ANCHOR_LIST as i32,
        /// `SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST`
        InvalidCertificatePropertyList = bssl_sys::SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST as i32,
        /// `SSL_R_DUPLICATE_GROUP`
        DuplicateGroup = bssl_sys::SSL_R_DUPLICATE_GROUP as i32,
        /// `SSL_R_SSLV3_ALERT_CLOSE_NOTIFY`
        Sslv3AlertCloseNotify = bssl_sys::SSL_R_SSLV3_ALERT_CLOSE_NOTIFY as i32,
        /// `SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE`
        Sslv3AlertUnexpectedMessage = bssl_sys::SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE as i32,
        /// `SSL_R_SSLV3_ALERT_BAD_RECORD_MAC`
        Sslv3AlertBadRecordMac = bssl_sys::SSL_R_SSLV3_ALERT_BAD_RECORD_MAC as i32,
        /// `SSL_R_TLSV1_ALERT_DECRYPTION_FAILED`
        Tlsv1AlertDecryptionFailed = bssl_sys::SSL_R_TLSV1_ALERT_DECRYPTION_FAILED as i32,
        /// `SSL_R_TLSV1_ALERT_RECORD_OVERFLOW`
        Tlsv1AlertRecordOverflow = bssl_sys::SSL_R_TLSV1_ALERT_RECORD_OVERFLOW as i32,
        /// `SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE`
        Sslv3AlertDecompressionFailure = bssl_sys::SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE as i32,
        /// `SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE`
        Sslv3AlertHandshakeFailure = bssl_sys::SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE as i32,
        /// `SSL_R_SSLV3_ALERT_NO_CERTIFICATE`
        Sslv3AlertNoCertificate = bssl_sys::SSL_R_SSLV3_ALERT_NO_CERTIFICATE as i32,
        /// `SSL_R_SSLV3_ALERT_BAD_CERTIFICATE`
        Sslv3AlertBadCertificate = bssl_sys::SSL_R_SSLV3_ALERT_BAD_CERTIFICATE as i32,
        /// `SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE`
        Sslv3AlertUnsupportedCertificate = bssl_sys::SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE as i32,
        /// `SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED`
        Sslv3AlertCertificateRevoked = bssl_sys::SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED as i32,
        /// `SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED`
        Sslv3AlertCertificateExpired = bssl_sys::SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED as i32,
        /// `SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN`
        Sslv3AlertCertificateUnknown = bssl_sys::SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN as i32,
        /// `SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER`
        Sslv3AlertIllegalParameter = bssl_sys::SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER as i32,
        /// `SSL_R_TLSV1_ALERT_UNKNOWN_CA`
        Tlsv1AlertUnknownCa = bssl_sys::SSL_R_TLSV1_ALERT_UNKNOWN_CA as i32,
        /// `SSL_R_TLSV1_ALERT_ACCESS_DENIED`
        Tlsv1AlertAccessDenied = bssl_sys::SSL_R_TLSV1_ALERT_ACCESS_DENIED as i32,
        /// `SSL_R_TLSV1_ALERT_DECODE_ERROR`
        Tlsv1AlertDecodeError = bssl_sys::SSL_R_TLSV1_ALERT_DECODE_ERROR as i32,
        /// `SSL_R_TLSV1_ALERT_DECRYPT_ERROR`
        Tlsv1AlertDecryptError = bssl_sys::SSL_R_TLSV1_ALERT_DECRYPT_ERROR as i32,
        /// `SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION`
        Tlsv1AlertExportRestriction = bssl_sys::SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION as i32,
        /// `SSL_R_TLSV1_ALERT_PROTOCOL_VERSION`
        Tlsv1AlertProtocolVersion = bssl_sys::SSL_R_TLSV1_ALERT_PROTOCOL_VERSION as i32,
        /// `SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY`
        Tlsv1AlertInsufficientSecurity = bssl_sys::SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY as i32,
        /// `SSL_R_TLSV1_ALERT_INTERNAL_ERROR`
        Tlsv1AlertInternalError = bssl_sys::SSL_R_TLSV1_ALERT_INTERNAL_ERROR as i32,
        /// `SSL_R_TLSV1_ALERT_INAPPROPRIATE_FALLBACK`
        Tlsv1AlertInappropriateFallback = bssl_sys::SSL_R_TLSV1_ALERT_INAPPROPRIATE_FALLBACK as i32,
        /// `SSL_R_TLSV1_ALERT_USER_CANCELLED`
        Tlsv1AlertUserCancelled = bssl_sys::SSL_R_TLSV1_ALERT_USER_CANCELLED as i32,
        /// `SSL_R_TLSV1_ALERT_NO_RENEGOTIATION`
        Tlsv1AlertNoRenegotiation = bssl_sys::SSL_R_TLSV1_ALERT_NO_RENEGOTIATION as i32,
        /// `SSL_R_TLSV1_ALERT_UNSUPPORTED_EXTENSION`
        Tlsv1AlertUnsupportedExtension = bssl_sys::SSL_R_TLSV1_ALERT_UNSUPPORTED_EXTENSION as i32,
        /// `SSL_R_TLSV1_ALERT_CERTIFICATE_UNOBTAINABLE`
        Tlsv1AlertCertificateUnobtainable = bssl_sys::SSL_R_TLSV1_ALERT_CERTIFICATE_UNOBTAINABLE as i32,
        /// `SSL_R_TLSV1_ALERT_UNRECOGNIZED_NAME`
        Tlsv1AlertUnrecognizedName = bssl_sys::SSL_R_TLSV1_ALERT_UNRECOGNIZED_NAME as i32,
        /// `SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE`
        Tlsv1AlertBadCertificateStatusResponse = bssl_sys::SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE as i32,
        /// `SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_HASH_VALUE`
        Tlsv1AlertBadCertificateHashValue = bssl_sys::SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_HASH_VALUE as i32,
        /// `SSL_R_TLSV1_ALERT_UNKNOWN_PSK_IDENTITY`
        Tlsv1AlertUnknownPskIdentity = bssl_sys::SSL_R_TLSV1_ALERT_UNKNOWN_PSK_IDENTITY as i32,
        /// `SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED`
        Tlsv1AlertCertificateRequired = bssl_sys::SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED as i32,
        /// `SSL_R_TLSV1_ALERT_NO_APPLICATION_PROTOCOL`
        Tlsv1AlertNoApplicationProtocol = bssl_sys::SSL_R_TLSV1_ALERT_NO_APPLICATION_PROTOCOL as i32,
        /// `SSL_R_TLSV1_ALERT_ECH_REQUIRED`
        Tlsv1AlertEchRequired = bssl_sys::SSL_R_TLSV1_ALERT_ECH_REQUIRED as i32,
    }
}

impl Display for TlsErrorReason {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        Debug::fmt(self, f)
    }
}

bssl_enum! {
    /// TLS errors
    #[derive(Debug, Clone, Copy)]
    #[must_use]
    pub enum TlsRetryReason: i32 {
        /// TLS is blocked on read.
        WantRead = bssl_sys::SSL_ERROR_WANT_READ as i32,
        /// TLS is blocked on write.
        WantWrite = bssl_sys::SSL_ERROR_WANT_WRITE as i32,
        /// Pending session.
        /// Caller may retry the last operation when the session lookup is ready.
        PendingSession = bssl_sys::SSL_ERROR_PENDING_SESSION as i32,
        /// Pending certificate.
        /// Caller may retry the last operation when the certificate lookup is ready.
        PendingCertificate = bssl_sys::SSL_ERROR_PENDING_CERTIFICATE as i32,
        /// Pending certification verification.
        /// Caller may retry the last operation when the certification verification is ready.
        PendingCertificateVerify = bssl_sys::SSL_ERROR_WANT_CERTIFICATE_VERIFY as i32,
        /// Pending ticket.
        /// Caller may retry the last operation when the ticket decryption is ready.
        PendingTicket = bssl_sys::SSL_ERROR_PENDING_TICKET as i32,
        /// End of stream, due to peer close_notify.
        PeerCloseNotify = bssl_sys::SSL_ERROR_ZERO_RETURN as i32,
        /// Want to (re)connect.
        /// Caller may retry the last operation when the transport becomes ready.
        WantConnect = bssl_sys::SSL_ERROR_WANT_CONNECT as i32,
        /// Want to (re)accept.
        /// Caller may retry the last operation when the transport becomes ready.
        WantAccept = bssl_sys::SSL_ERROR_WANT_ACCEPT as i32,
        /// Pending private key operation.
        /// Caller may retry the last operation when the private key operation might be ready.
        PendingPrivateKeyOperation = bssl_sys::SSL_ERROR_WANT_PRIVATE_KEY_OPERATION as i32,
        /// Early data was rejected.
        /// Caller may call `reset_early_data_reject` to start from a clean state.
        EarlyDataRejected = bssl_sys::SSL_ERROR_EARLY_DATA_REJECTED as i32,
        /// Handshake Hints becomes ready.
        HandshakeHintsReady = bssl_sys::SSL_ERROR_HANDSHAKE_HINTS_READY as i32,
    }
}

impl core::error::Error for TlsRetryReason {}

impl Display for TlsRetryReason {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            TlsRetryReason::WantRead => f.write_str("want to read"),
            TlsRetryReason::WantWrite => f.write_str("want to write"),
            TlsRetryReason::PendingSession => f.write_str("pending session"),
            TlsRetryReason::PendingCertificate => f.write_str("pending certificate"),
            TlsRetryReason::PendingTicket => f.write_str("pending ticket"),
            TlsRetryReason::PendingCertificateVerify => f.write_str("pending certificate verify"),
            TlsRetryReason::WantConnect => f.write_str("want to (re)connect connection"),
            TlsRetryReason::WantAccept => f.write_str("want to (re)accept connection"),
            TlsRetryReason::PendingPrivateKeyOperation => {
                f.write_str("pending private key operation")
            }
            TlsRetryReason::EarlyDataRejected => f.write_str("early data rejected"),
            TlsRetryReason::HandshakeHintsReady => f.write_str("handshake hints ready"),
            TlsRetryReason::PeerCloseNotify => f.write_str("peer close notify"),
        }
    }
}

/// QUIC errors.
#[derive(Debug)]
pub enum QuicError {}

impl Display for QuicError {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.write_str("quic error")
    }
}

/// I/O errors
#[derive(Debug)]
pub enum IoError {
    /// Buffer sizes are too long.
    TooLong,
    /// Reached the end of stream.
    EndOfStream,
    /// Error during I/O operation in the underlying transport.
    Transport(Box<dyn core::error::Error + Send + Sync>),
}

impl Display for IoError {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            IoError::TooLong => f.write_str("data too long"),
            IoError::EndOfStream => f.write_str("end of stream"),
            IoError::Transport(e) => write!(f, "transport: {e}"),
        }
    }
}
