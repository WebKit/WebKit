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

use bssl_x509::keys::PrivateKeyAlgorithm;
use futures::future::try_join;

use super::*;
use crate::{
    credentials::{
        PskHash,
        TlsCredential, //
    },
    tests::{
        create_mock_pipe, //
    },
};

#[test]
fn parse_none() {
    let certs = Certificate::parse_all_from_pem(b"", None).unwrap();
    assert!(certs.is_empty());
}

#[test]
fn parse_one() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
";
    let certs = Certificate::parse_all_from_pem(PEM, None).unwrap();
    assert_eq!(certs.len(), 1);
    assert_eq!(certs[0].as_der_bytes(), b"Hello World!");
}

#[test]
fn parse_all_pems() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
BoringSSL is ...
-----BEGIN CERTIFICATE-----
QXdlc29tZSBCb3JpbmdTU0wh
-----END CERTIFICATE-----
Trailing bits ...
";
    let certs = Certificate::parse_all_from_pem(PEM, None).unwrap();
    assert_eq!(certs.len(), 2);
    assert_eq!(certs[0].as_der_bytes(), b"Hello World!");
    assert_eq!(certs[1].as_der_bytes(), b"Awesome BoringSSL!");
}

#[test]
fn parse_all_pems_fail() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
BoringSSL is ...
-----BEGIN CERTIFICATE-----
QXdlc29tZSBCb3JpbmdTU0wh
-----END CERTIFICATE-----
But something badddd...
-----BEGIN CERTIFICATE-----
";
    let _ = Certificate::parse_all_from_pem(PEM, None).unwrap_err();
}

#[test]
fn parse_private_key() {
    const PEM: &'_ [u8] = b"-----BEGIN ENCRYPTED PRIVATE KEY-----
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
-----END ENCRYPTED PRIVATE KEY-----
";
    let priv_key = PrivateKey::from_pem(PEM, || b"Hello BoringSSL!").unwrap();
    assert!(matches!(
        priv_key.algorithm(),
        Some(PrivateKeyAlgorithm::Rsa)
    ));
}

#[test]
fn psk_tls13_handshake() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let key = b"test-key-test-key-test-key-test-key";
    let identity = b"test-identity";
    let context = b"test-context";

    let cred = TlsCredential::new_pre_shared_key(key, identity, PskHash::Sha256, context)?;

    let mut server_ctx = crate::context::TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred.clone())?;

    let mut client_ctx = crate::context::TlsContextBuilder::new_tls();
    client_ctx.with_credential(cred)?;

    let server_ctx = server_ctx.build();
    let client_ctx = client_ctx.build();

    let (client_socket, server_socket, mut executor) = create_mock_pipe();

    let mut client_conn = client_ctx.new_client_connection(None)?.build();
    let mut server_conn = server_ctx.new_server_connection(None)?.build();

    client_conn.set_io(client_socket)?;
    server_conn.set_io(server_socket)?;

    let test_future = async {
        let client_handshake = async {
            let mut in_handshake = client_conn.in_handshake().unwrap();
            in_handshake.async_handshake().await?;
            Ok::<(), crate::errors::Error>(())
        };

        let server_handshake = async {
            let mut in_handshake = server_conn.in_handshake().unwrap();
            in_handshake.async_handshake().await?;
            Ok::<(), crate::errors::Error>(())
        };

        try_join(client_handshake, server_handshake).await?;
        Ok::<(), crate::errors::Error>(())
    };

    executor.run(test_future)?;

    Ok(())
}

#[cfg(all(unix, feature = "std"))]
#[test]
fn psk_tls13_handshake_sync() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use crate::credentials::{PskHash, TlsCredential};
    use crate::io::sync_io::{NoAsync, StdIoWithReactor};
    use std::io::pipe;

    let (server_rx, client_tx) = pipe().unwrap();
    let (client_rx, server_tx) = pipe().unwrap();

    let client_reader = StdIoWithReactor::new(client_rx, NoAsync);
    let client_writer = StdIoWithReactor::new(client_tx, NoAsync);
    let server_reader = StdIoWithReactor::new(server_rx, NoAsync);
    let server_writer = StdIoWithReactor::new(server_tx, NoAsync);

    let key = b"test-key-test-key-test-key-test-key";
    let identity = b"test-identity";
    let context = b"test-context";

    let cred = TlsCredential::new_pre_shared_key(key, identity, PskHash::Sha256, context)?;

    let mut server_ctx = crate::context::TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred.clone())?;
    let server_ctx = server_ctx.build();

    let mut client_ctx = crate::context::TlsContextBuilder::new_tls();
    client_ctx.with_credential(cred)?;
    let client_ctx = client_ctx.build();

    let mut client_conn = client_ctx.new_client_connection(None)?.build();
    let mut server_conn = server_ctx.new_server_connection(None)?.build();

    client_conn.set_split_io(client_reader, client_writer)?;
    server_conn.set_split_io(server_reader, server_writer)?;

    let server_thread = std::thread::spawn(move || {
        let mut in_handshake = server_conn.in_handshake().unwrap();
        in_handshake.do_handshake().unwrap();
        server_conn
    });

    let mut in_handshake = client_conn.in_handshake().unwrap();
    in_handshake.do_handshake().unwrap();

    let _server_conn = server_thread.join().unwrap();

    Ok(())
}

unsafe extern "C" fn accept_any_verify(
    _ssl: *mut bssl_sys::SSL,
    _out_alert: *mut u8,
) -> bssl_sys::ssl_verify_result_t {
    bssl_sys::ssl_verify_result_t_ssl_verify_ok
}

#[test]
fn rpk_tls13_handshake() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use crate::credentials::{
        CertificateType, CertificateVerificationMode, RawPublicKeyMode, TlsCredentialBuilder,
    };
    use crate::tests::create_mock_pipe;
    use bssl_x509::keys::PrivateKey;

    const PEM: &'_ [u8] = b"-----BEGIN ENCRYPTED PRIVATE KEY-----
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
-----END ENCRYPTED PRIVATE KEY-----
";
    let priv_key = PrivateKey::from_pem(PEM, || b"Hello BoringSSL!").unwrap();

    let cred = TlsCredentialBuilder::<RawPublicKeyMode>::new_raw_public_key(priv_key)
        .build()
        .unwrap();

    let mut server_ctx = crate::context::TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred.clone())?;
    let server_ctx = server_ctx.build();

    let mut client_ctx = crate::context::TlsContextBuilder::new_tls();
    client_ctx.with_accepted_peer_cert_types(&[CertificateType::Rpk])?;
    let client_ctx = client_ctx.build();

    let mut client_conn_builder = client_ctx.new_client_connection(None)?;
    client_conn_builder.with_certificate_verification_mode(CertificateVerificationMode::None);
    let mut client_conn = client_conn_builder.build();
    unsafe {
        // Safety:
        // - `client_conn.ptr()` is a valid `SSL` handle.
        // - `accept_any_verify` is a valid callback.
        bssl_sys::SSL_set_custom_verify(
            client_conn.ptr(),
            bssl_sys::SSL_VERIFY_PEER as _,
            Some(accept_any_verify),
        );
    }
    let mut server_conn = server_ctx.new_server_connection(None)?.build();

    let (sock_client, sock_server, mut executor) = create_mock_pipe();

    client_conn.set_io(sock_client)?;
    server_conn.set_io(sock_server)?;

    executor.run(try_join(
        Pin::new(&mut client_conn).async_write(b"hello"),
        Pin::new(&mut server_conn).async_write(b"world"),
    ))?;

    assert_eq!(
        client_conn.get_peer_certificate_type(),
        Some(CertificateType::Rpk)
    );

    Ok(())
}
