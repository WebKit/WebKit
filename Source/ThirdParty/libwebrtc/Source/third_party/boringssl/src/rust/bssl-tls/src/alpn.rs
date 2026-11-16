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

//! ALPN configuration and handling.
//!
//! The ALPN extension [RFC 7301] allows negotiating different application-layer
//! protocols over a single port.
//! This is used, for example, to negotiate HTTP/2.
/// A full list of values is available in [IANA].
///
/// [RFC 7301]: <https://datatracker.ietf.org/doc/html/rfc7301>
/// [IANA]: <https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#alpn-protocol-ids>
use alloc::vec::Vec;

#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub(crate) struct AlpnProtocols(Vec<u8>);

impl AlpnProtocols {
    /// Append one ALPN protocol to the list.
    ///
    /// This method returns [`Err`] if `config` is empty or longer than 255 bytes, or the list
    /// would exceed [`u16::MAX`] bytes.
    pub fn append_protocol(&mut self, proto: &[u8]) -> Result<(), ()> {
        if proto.is_empty() {
            return Err(());
        }
        let Ok(len) = u8::try_from(proto.len()) else {
            return Err(());
        };
        if self.0.len() + 1 + len as usize > u16::MAX as usize {
            return Err(());
        }

        self.0.push(len);
        self.0.extend_from_slice(proto);
        Ok(())
    }

    pub fn as_slice(&self) -> &[u8] {
        &self.0
    }
}

/// HTTP/2 over TLS protocol identifier.
pub const H2: &[u8] = b"h2";

/// HTTP/2 over plain TCP protocol identifier.
pub const H2C: &[u8] = b"h2c";

/// HTTP/1.1 protocol identifier.
pub const HTTP11: &[u8] = b"http/1.1";

/// WebRTC protocol identifier.
pub const WEBRTC: &[u8] = b"webrtc";

/// Confidential WebRTC protocol identifier.
pub const CONFIDENTIAL_WEBRTC: &[u8] = b"c-webrtc";

/// CoAP over TLS protocol identifier.
pub const COAP: &[u8] = b"coap";

/// CoAP/DTLS protocol identifier.
pub const COAP_DTLS: &[u8] = b"co";

/// HTTP/3 protocol identifier.
pub const H3: &[u8] = b"h3";

/// ACME protocol identifier.
pub const ACME: &[u8] = b"acme-tls/1";

/// IMAP over TLS protocol identifier.
pub const IMAP: &[u8] = b"imap";

/// POP3 over TLS protocol identifier.
pub const POP3: &[u8] = b"pop3";

/// FTP over TLS protocol identifier.
pub const FTP: &[u8] = b"ftp";
