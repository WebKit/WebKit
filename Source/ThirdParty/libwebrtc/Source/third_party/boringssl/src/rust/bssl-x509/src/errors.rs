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

//! X.509 certificate verification errors.

use core::ffi::{c_char, CStr};
use core::fmt::{self, Debug, Display};

use bssl_macros::bssl_enum;
use bssl_sys::LibCode;

bssl_enum! {
    /// X.509 certificate verification result code.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum X509VerifyResult: i32 {
        /// The operation was successful.
        Ok = bssl_sys::X509_V_OK as i32,
        /// Unspecified error.
        Unspecified = bssl_sys::X509_V_ERR_UNSPECIFIED as i32,
        /// Unable to get issuer certificate.
        UnableToGetIssuerCert = bssl_sys::X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT as i32,
        /// Unable to get CRL.
        UnableToGetCrl = bssl_sys::X509_V_ERR_UNABLE_TO_GET_CRL as i32,
        /// Unable to decrypt certificate signature.
        UnableToDecryptCertSignature = bssl_sys::X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE as i32,
        /// Unable to decrypt CRL signature.
        UnableToDecryptCrlSignature = bssl_sys::X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE as i32,
        /// Unable to decode issuer public key.
        UnableToDecodeIssuerPublicKey = bssl_sys::X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY as i32,
        /// Certificate signature failure.
        CertSignatureFailure = bssl_sys::X509_V_ERR_CERT_SIGNATURE_FAILURE as i32,
        /// CRL signature failure.
        CrlSignatureFailure = bssl_sys::X509_V_ERR_CRL_SIGNATURE_FAILURE as i32,
        /// Certificate is not yet valid.
        CertNotYetValid = bssl_sys::X509_V_ERR_CERT_NOT_YET_VALID as i32,
        /// Certificate has expired.
        CertHasExpired = bssl_sys::X509_V_ERR_CERT_HAS_EXPIRED as i32,
        /// CRL is not yet valid.
        CrlNotYetValid = bssl_sys::X509_V_ERR_CRL_NOT_YET_VALID as i32,
        /// CRL has expired.
        CrlHasExpired = bssl_sys::X509_V_ERR_CRL_HAS_EXPIRED as i32,
        /// Error in certificate NotBefore field.
        ErrorInCertNotBeforeField = bssl_sys::X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD as i32,
        /// Error in certificate NotAfter field.
        ErrorInCertNotAfterField = bssl_sys::X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD as i32,
        /// Error in CRL LastUpdate field.
        ErrorInCrlLastUpdateField = bssl_sys::X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD as i32,
        /// Error in CRL NextUpdate field.
        ErrorInCrlNextUpdateField = bssl_sys::X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD as i32,
        /// Out of memory.
        OutOfMem = bssl_sys::X509_V_ERR_OUT_OF_MEM as i32,
        /// Depth zero self-signed certificate.
        DepthZeroSelfSignedCert = bssl_sys::X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT as i32,
        /// Self-signed certificate in chain.
        SelfSignedCertInChain = bssl_sys::X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN as i32,
        /// Unable to get issuer certificate locally.
        UnableToGetIssuerCertLocally = bssl_sys::X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY as i32,
        /// Unable to verify leaf signature.
        UnableToVerifyLeafSignature = bssl_sys::X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE as i32,
        /// Certificate chain too long.
        CertChainTooLong = bssl_sys::X509_V_ERR_CERT_CHAIN_TOO_LONG as i32,
        /// Certificate revoked.
        CertRevoked = bssl_sys::X509_V_ERR_CERT_REVOKED as i32,
        /// Invalid CA.
        InvalidCa = bssl_sys::X509_V_ERR_INVALID_CA as i32,
        /// Path length exceeded.
        PathLengthExceeded = bssl_sys::X509_V_ERR_PATH_LENGTH_EXCEEDED as i32,
        /// Invalid purpose.
        InvalidPurpose = bssl_sys::X509_V_ERR_INVALID_PURPOSE as i32,
        /// Certificate untrusted.
        CertUntrusted = bssl_sys::X509_V_ERR_CERT_UNTRUSTED as i32,
        /// Certificate rejected.
        CertRejected = bssl_sys::X509_V_ERR_CERT_REJECTED as i32,
        /// Subject issuer mismatch.
        SubjectIssuerMismatch = bssl_sys::X509_V_ERR_SUBJECT_ISSUER_MISMATCH as i32,
        /// Authority Key Identifier and Subject Key Identifier mismatch.
        AkidSkidMismatch = bssl_sys::X509_V_ERR_AKID_SKID_MISMATCH as i32,
        /// Authority Key Identifier issuer serial mismatch.
        AkidIssuerSerialMismatch = bssl_sys::X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH as i32,
        /// Key usage does not include certificate signing.
        KeyusageNoCertsign = bssl_sys::X509_V_ERR_KEYUSAGE_NO_CERTSIGN as i32,
        /// Unable to get CRL issuer.
        UnableToGetCrlIssuer = bssl_sys::X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER as i32,
        /// Unhandled critical extension.
        UnhandledCriticalExtension = bssl_sys::X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION as i32,
        /// Key usage does not include CRL signing.
        KeyusageNoCrlSign = bssl_sys::X509_V_ERR_KEYUSAGE_NO_CRL_SIGN as i32,
        /// Unhandled critical CRL extension.
        UnhandledCriticalCrlExtension = bssl_sys::X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION as i32,
        /// Invalid non-CA.
        InvalidNonCa = bssl_sys::X509_V_ERR_INVALID_NON_CA as i32,
        /// Proxy path length exceeded.
        ProxyPathLengthExceeded = bssl_sys::X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED as i32,
        /// Key usage does not include digital signature.
        KeyusageNoDigitalSignature = bssl_sys::X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE as i32,
        /// Proxy certificates not allowed.
        ProxyCertificatesNotAllowed = bssl_sys::X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED as i32,
        /// Invalid extension.
        InvalidExtension = bssl_sys::X509_V_ERR_INVALID_EXTENSION as i32,
        /// Invalid policy extension.
        InvalidPolicyExtension = bssl_sys::X509_V_ERR_INVALID_POLICY_EXTENSION as i32,
        /// No explicit policy.
        NoExplicitPolicy = bssl_sys::X509_V_ERR_NO_EXPLICIT_POLICY as i32,
        /// Different CRL scope.
        DifferentCrlScope = bssl_sys::X509_V_ERR_DIFFERENT_CRL_SCOPE as i32,
        /// Unsupported extension feature.
        UnsupportedExtensionFeature = bssl_sys::X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE as i32,
        /// Unnested resource.
        UnnestedResource = bssl_sys::X509_V_ERR_UNNESTED_RESOURCE as i32,
        /// Permitted violation.
        PermittedViolation = bssl_sys::X509_V_ERR_PERMITTED_VIOLATION as i32,
        /// Excluded violation.
        ExcludedViolation = bssl_sys::X509_V_ERR_EXCLUDED_VIOLATION as i32,
        /// Subtree min/max.
        SubtreeMinmax = bssl_sys::X509_V_ERR_SUBTREE_MINMAX as i32,
        /// Application verification.
        ApplicationVerification = bssl_sys::X509_V_ERR_APPLICATION_VERIFICATION as i32,
        /// Unsupported constraint type.
        UnsupportedConstraintType = bssl_sys::X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE as i32,
        /// Unsupported constraint syntax.
        UnsupportedConstraintSyntax = bssl_sys::X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX as i32,
        /// Unsupported name syntax.
        UnsupportedNameSyntax = bssl_sys::X509_V_ERR_UNSUPPORTED_NAME_SYNTAX as i32,
        /// CRL path validation error.
        CrlPathValidationError = bssl_sys::X509_V_ERR_CRL_PATH_VALIDATION_ERROR as i32,
        /// Hostname mismatch.
        HostnameMismatch = bssl_sys::X509_V_ERR_HOSTNAME_MISMATCH as i32,
        /// Email mismatch.
        EmailMismatch = bssl_sys::X509_V_ERR_EMAIL_MISMATCH as i32,
        /// IP address mismatch.
        IpAddressMismatch = bssl_sys::X509_V_ERR_IP_ADDRESS_MISMATCH as i32,
        /// Invalid call.
        InvalidCall = bssl_sys::X509_V_ERR_INVALID_CALL as i32,
        /// Store lookup error.
        StoreLookup = bssl_sys::X509_V_ERR_STORE_LOOKUP as i32,
        /// Name constraints without SANs.
        NameConstraintsWithoutSans = bssl_sys::X509_V_ERR_NAME_CONSTRAINTS_WITHOUT_SANS as i32,
    }
}

bssl_enum! {
    /// X.509 reason codes.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum X509Reason: i32 {
        /// `X509_R_AKID_MISMATCH`
        AkidMismatch = bssl_sys::X509_R_AKID_MISMATCH as i32,
        /// `X509_R_BAD_PKCS7_VERSION`
        BadPkcs7Version = bssl_sys::X509_R_BAD_PKCS7_VERSION as i32,
        /// `X509_R_BAD_X509_FILETYPE`
        BadX509Filetype = bssl_sys::X509_R_BAD_X509_FILETYPE as i32,
        /// `X509_R_BASE64_DECODE_ERROR`
        Base64DecodeError = bssl_sys::X509_R_BASE64_DECODE_ERROR as i32,
        /// `X509_R_CANT_CHECK_DH_KEY`
        CantCheckDhKey = bssl_sys::X509_R_CANT_CHECK_DH_KEY as i32,
        /// `X509_R_CERT_ALREADY_IN_HASH_TABLE`
        CertAlreadyInHashTable = bssl_sys::X509_R_CERT_ALREADY_IN_HASH_TABLE as i32,
        /// `X509_R_CRL_ALREADY_DELTA`
        CrlAlreadyDelta = bssl_sys::X509_R_CRL_ALREADY_DELTA as i32,
        /// `X509_R_CRL_VERIFY_FAILURE`
        CrlVerifyFailure = bssl_sys::X509_R_CRL_VERIFY_FAILURE as i32,
        /// `X509_R_IDP_MISMATCH`
        IdpMismatch = bssl_sys::X509_R_IDP_MISMATCH as i32,
        /// `X509_R_INVALID_BIT_STRING_BITS_LEFT`
        InvalidBitStringBitsLeft = bssl_sys::X509_R_INVALID_BIT_STRING_BITS_LEFT as i32,
        /// `X509_R_INVALID_DIRECTORY`
        InvalidDirectory = bssl_sys::X509_R_INVALID_DIRECTORY as i32,
        /// `X509_R_INVALID_FIELD_NAME`
        InvalidFieldName = bssl_sys::X509_R_INVALID_FIELD_NAME as i32,
        /// `X509_R_INVALID_PSS_PARAMETERS`
        InvalidPssParameters = bssl_sys::X509_R_INVALID_PSS_PARAMETERS as i32,
        /// `X509_R_INVALID_TRUST`
        InvalidTrust = bssl_sys::X509_R_INVALID_TRUST as i32,
        /// `X509_R_ISSUER_MISMATCH`
        IssuerMismatch = bssl_sys::X509_R_ISSUER_MISMATCH as i32,
        /// `X509_R_KEY_TYPE_MISMATCH`
        KeyTypeMismatch = bssl_sys::X509_R_KEY_TYPE_MISMATCH as i32,
        /// `X509_R_KEY_VALUES_MISMATCH`
        KeyValuesMismatch = bssl_sys::X509_R_KEY_VALUES_MISMATCH as i32,
        /// `X509_R_LOADING_CERT_DIR`
        LoadingCertDir = bssl_sys::X509_R_LOADING_CERT_DIR as i32,
        /// `X509_R_LOADING_DEFAULTS`
        LoadingDefaults = bssl_sys::X509_R_LOADING_DEFAULTS as i32,
        /// `X509_R_NEWER_CRL_NOT_NEWER`
        NewerCrlNotNewer = bssl_sys::X509_R_NEWER_CRL_NOT_NEWER as i32,
        /// `X509_R_NOT_PKCS7_SIGNED_DATA`
        NotPkcs7SignedData = bssl_sys::X509_R_NOT_PKCS7_SIGNED_DATA as i32,
        /// `X509_R_NO_CERTIFICATES_INCLUDED`
        NoCertificatesIncluded = bssl_sys::X509_R_NO_CERTIFICATES_INCLUDED as i32,
        /// `X509_R_NO_CERT_SET_FOR_US_TO_VERIFY`
        NoCertSetForUsToVerify = bssl_sys::X509_R_NO_CERT_SET_FOR_US_TO_VERIFY as i32,
        /// `X509_R_NO_CRLS_INCLUDED`
        NoCrlsIncluded = bssl_sys::X509_R_NO_CRLS_INCLUDED as i32,
        /// `X509_R_NO_CRL_NUMBER`
        NoCrlNumber = bssl_sys::X509_R_NO_CRL_NUMBER as i32,
        /// `X509_R_PUBLIC_KEY_DECODE_ERROR`
        PublicKeyDecodeError = bssl_sys::X509_R_PUBLIC_KEY_DECODE_ERROR as i32,
        /// `X509_R_PUBLIC_KEY_ENCODE_ERROR`
        PublicKeyEncodeError = bssl_sys::X509_R_PUBLIC_KEY_ENCODE_ERROR as i32,
        /// `X509_R_SHOULD_RETRY`
        ShouldRetry = bssl_sys::X509_R_SHOULD_RETRY as i32,
        /// `X509_R_UNKNOWN_KEY_TYPE`
        UnknownKeyType = bssl_sys::X509_R_UNKNOWN_KEY_TYPE as i32,
        /// `X509_R_UNKNOWN_NID`
        UnknownNid = bssl_sys::X509_R_UNKNOWN_NID as i32,
        /// `X509_R_UNKNOWN_PURPOSE_ID`
        UnknownPurposeId = bssl_sys::X509_R_UNKNOWN_PURPOSE_ID as i32,
        /// `X509_R_UNKNOWN_TRUST_ID`
        UnknownTrustId = bssl_sys::X509_R_UNKNOWN_TRUST_ID as i32,
        /// `X509_R_UNSUPPORTED_ALGORITHM`
        UnsupportedAlgorithm = bssl_sys::X509_R_UNSUPPORTED_ALGORITHM as i32,
        /// `X509_R_WRONG_LOOKUP_TYPE`
        WrongLookupType = bssl_sys::X509_R_WRONG_LOOKUP_TYPE as i32,
        /// `X509_R_WRONG_TYPE`
        WrongType = bssl_sys::X509_R_WRONG_TYPE as i32,
        /// `X509_R_NAME_TOO_LONG`
        NameTooLong = bssl_sys::X509_R_NAME_TOO_LONG as i32,
        /// `X509_R_INVALID_PARAMETER`
        InvalidParameter = bssl_sys::X509_R_INVALID_PARAMETER as i32,
        /// `X509_R_SIGNATURE_ALGORITHM_MISMATCH`
        SignatureAlgorithmMismatch = bssl_sys::X509_R_SIGNATURE_ALGORITHM_MISMATCH as i32,
        /// `X509_R_DELTA_CRL_WITHOUT_CRL_NUMBER`
        DeltaCrlWithoutCrlNumber = bssl_sys::X509_R_DELTA_CRL_WITHOUT_CRL_NUMBER as i32,
        /// `X509_R_INVALID_FIELD_FOR_VERSION`
        InvalidFieldForVersion = bssl_sys::X509_R_INVALID_FIELD_FOR_VERSION as i32,
        /// `X509_R_INVALID_VERSION`
        InvalidVersion = bssl_sys::X509_R_INVALID_VERSION as i32,
        /// `X509_R_NO_CERTIFICATE_FOUND`
        NoCertificateFound = bssl_sys::X509_R_NO_CERTIFICATE_FOUND as i32,
        /// `X509_R_NO_CERTIFICATE_OR_CRL_FOUND`
        NoCertificateOrCrlFound = bssl_sys::X509_R_NO_CERTIFICATE_OR_CRL_FOUND as i32,
        /// `X509_R_NO_CRL_FOUND`
        NoCrlFound = bssl_sys::X509_R_NO_CRL_FOUND as i32,
        /// `X509_R_INVALID_POLICY_EXTENSION`
        InvalidPolicyExtension = bssl_sys::X509_R_INVALID_POLICY_EXTENSION as i32,
    }
}

bssl_enum! {
    /// PEM parser failure reasons.
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum PemReason: i32 {
        /// `PEM_R_BAD_BASE64_DECODE`
        BadBase64Decode = bssl_sys::PEM_R_BAD_BASE64_DECODE as i32,
        /// `PEM_R_BAD_DECRYPT`
        BadDecrypt = bssl_sys::PEM_R_BAD_DECRYPT as i32,
        /// `PEM_R_BAD_END_LINE`
        BadEndLine = bssl_sys::PEM_R_BAD_END_LINE as i32,
        /// `PEM_R_BAD_IV_CHARS`
        BadIvChars = bssl_sys::PEM_R_BAD_IV_CHARS as i32,
        /// `PEM_R_BAD_PASSWORD_READ`
        BadPasswordRead = bssl_sys::PEM_R_BAD_PASSWORD_READ as i32,
        /// `PEM_R_CIPHER_IS_NULL`
        CipherIsNull = bssl_sys::PEM_R_CIPHER_IS_NULL as i32,
        /// `PEM_R_ERROR_CONVERTING_PRIVATE_KEY`
        ErrorConvertingPrivateKey = bssl_sys::PEM_R_ERROR_CONVERTING_PRIVATE_KEY as i32,
        /// `PEM_R_NOT_DEK_INFO`
        NotDekInfo = bssl_sys::PEM_R_NOT_DEK_INFO as i32,
        /// `PEM_R_NOT_ENCRYPTED`
        NotEncrypted = bssl_sys::PEM_R_NOT_ENCRYPTED as i32,
        /// `PEM_R_NOT_PROC_TYPE`
        NotProcType = bssl_sys::PEM_R_NOT_PROC_TYPE as i32,
        /// `PEM_R_NO_START_LINE`
        NoStartLine = bssl_sys::PEM_R_NO_START_LINE as i32,
        /// `PEM_R_READ_KEY`
        ReadKey = bssl_sys::PEM_R_READ_KEY as i32,
        /// `PEM_R_SHORT_HEADER`
        ShortHeader = bssl_sys::PEM_R_SHORT_HEADER as i32,
        /// `PEM_R_UNSUPPORTED_CIPHER`
        UnsupportedCipher = bssl_sys::PEM_R_UNSUPPORTED_CIPHER as i32,
        /// `PEM_R_UNSUPPORTED_ENCRYPTION`
        UnsupportedEncryption = bssl_sys::PEM_R_UNSUPPORTED_ENCRYPTION as i32,
        /// `PEM_R_UNSUPPORTED_PROC_TYPE_VERSION`
        UnsupportedProcTypeVersion = bssl_sys::PEM_R_UNSUPPORTED_PROC_TYPE_VERSION as i32,
        /// `PEM_R_NO_DATA`
        NoData = bssl_sys::PEM_R_NO_DATA as i32,
    }
}

/// X.509 related errors
#[derive(Debug)]
pub enum X509Error {
    /// PEM string is too long.
    PemTooLong,
    /// PEM Password has interior null byte.
    PemPasswordHasInteriorNull,
    /// X.509 certificate verification error.
    Verify(X509VerifyResult),
    /// No PEM blocks found.
    NoPemBlocks,
    /// Invalid parameters.
    InvalidParameters,
    /// X.509 module failure with reasons.
    Reason(X509Reason),
    /// PEM module errors.
    Pem(PemReason),
}

impl Display for X509Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            X509Error::PemTooLong => f.write_str("PEM string too long"),
            X509Error::PemPasswordHasInteriorNull => {
                f.write_str("PEM password has interior null byte")
            }
            X509Error::Verify(err) => Debug::fmt(err, f),
            X509Error::NoPemBlocks => f.write_str("no PEM blocks found"),
            X509Error::InvalidParameters => f.write_str("invalid X.509 parameters"),
            X509Error::Reason(err) => Debug::fmt(err, f),
            X509Error::Pem(err) => Debug::fmt(err, f),
        }
    }
}

/// Main PKI errors
#[derive(Debug)]
pub enum PkiError {
    /// X.509 errors
    X509(X509Error),
    /// Byte string contains internal NUL bytes.
    InternalNullBytes,
    /// Invalid IP address.
    InvalidIp,
    /// Value out of range.
    ValueOutOfRange,
    /// Library sourced error.
    Library(u32, Option<LibCode>, Option<i32>),
}

impl core::error::Error for PkiError {}

impl Display for PkiError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PkiError::X509(err) => Display::fmt(err, f),
            PkiError::InternalNullBytes => f.write_str("byte string contains internal NUL bytes"),
            PkiError::InvalidIp => f.write_str("invalid IP address"),
            PkiError::ValueOutOfRange => f.write_str("value out of range"),
            &PkiError::Library(code, _, _) => {
                // Here the buffer is 120 bytes and BoringSSL uses this buffer size
                // to hold the error string.
                // Therefore, this ought to be sufficient for a human-readable message.
                let mut err_str: [c_char; _] = [0; bssl_sys::ERR_ERROR_STRING_BUF_LEN as usize];
                unsafe {
                    // Safety:
                    // - err_str is non-null and valid, so we do not need FFI-specific pointer conversion.
                    bssl_sys::ERR_error_string_n(code, err_str.as_mut_ptr(), err_str.len());
                }
                let err_str = unsafe {
                    // Safety:
                    // - err_str is still valid;
                    // - `ERR_error_string_n` guarantees that the buffer is NUL-terminated.
                    CStr::from_ptr(err_str.as_ptr())
                };
                f.write_str(&err_str.to_string_lossy())
            }
        }
    }
}

impl PkiError {
    #[inline(always)]
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

    #[allow(irrefutable_let_patterns)]
    fn extract_err_from_code(packed_error: u32) -> Self {
        let lib = unsafe {
            // Safety: extracting error source does not have a side-effect and only accesses static data.
            bssl_sys::ERR_GET_LIB(packed_error)
        };
        let Ok(lib) = i32::try_from(lib) else {
            return Self::Library(packed_error, None, None);
        };
        let Ok(lib) = LibCode::try_from(lib) else {
            return Self::Library(packed_error, None, None);
        };
        let reason = unsafe {
            // Safety: extracting error reason does not have a side-effect and only accesses static data.
            bssl_sys::ERR_GET_REASON(packed_error)
        };
        let Ok(reason) = i32::try_from(reason) else {
            return Self::Library(packed_error, Some(lib), None);
        };
        match lib {
            LibCode::X509 => {
                let Ok(reason) = X509Reason::try_from(reason) else {
                    return Self::Library(packed_error, Some(lib), Some(reason));
                };
                Self::X509(X509Error::Reason(reason))
            }
            LibCode::Pem => {
                let Ok(reason) = PemReason::try_from(reason) else {
                    return Self::Library(packed_error, Some(lib), Some(reason));
                };
                Self::X509(X509Error::Pem(reason))
            }
            _ => Self::Library(packed_error, Some(lib), Some(reason)),
        }
    }
}
