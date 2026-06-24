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

use std::io::{PipeReader, PipeWriter, Read, Write, pipe};
use std::sync::Arc;

use super::*;

use rustls::crypto::ring;
use rustls::{
    RootCertStore, ServerConfig, ServerConnection, Stream, SupportedProtocolVersion,
    client::{ClientConfig, ClientConnection},
    pki_types::{CertificateDer, ServerName, pem::PemObject},
    version::{TLS12, TLS13},
};
use tracing::{Level, debug};

// ===================================================================================
// All CSRs `server.csr` is generated from
// openssl req -new -nodes -key <input key> -out server.csr -subj '/CN=www.google.com'
// ===================================================================================

const CA_CERT: &'static [u8] = include_bytes!("../../test-data/BoringSSLCATest.crt");

const RSA_SVC_CERT: &'static [u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.crt");

const RSA_SVC_KEY: &'static [u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.key");

const ECDSA_P256_SVC_CERT: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-ECDSA-P256.crt");

const ECDSA_P256_SVC_KEY: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-ECDSA-P256.key");

const ECDSA_P384_SVC_CERT: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-ECDSA-P384.crt");

const ECDSA_P384_SVC_KEY: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-ECDSA-P384.key");

const ED25519_SVC_CERT: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-Ed25519.crt");

const ED25519_SVC_KEY: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-Ed25519.key");

const RSA_PSS_SVC_CERT: &'static [u8] =
    include_bytes!("../../test-data/BoringSSLServerTest-RSA-PSS-SHA256.crt");

struct PipeSocket {
    tx: PipeWriter,
    rx: PipeReader,
}
impl Read for PipeSocket {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.rx.read(buf)
    }
}
impl Write for PipeSocket {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.tx.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.tx.flush()
    }
}

const EXPECTED_SERVER_MSG: &[u8; 18] = b"Awesome BoringSSL!";
const EXPECTED_CLIENT_MSG: &[u8; 19] = b"Oh yeah definitely!";

fn init_tracing() {
    let _ = tracing_subscriber::fmt()
        .with_max_level(Level::DEBUG)
        .try_init();
}

fn test_cipher_suite(
    ca: &[u8],
    svc_cert: &[u8],
    svc_key: &[u8],
    client_protocols: &[&'static SupportedProtocolVersion],
    server_crypto_provider: CryptoProvider,
    client_crypto_provider: CryptoProvider,
) {
    init_tracing();
    let ca_cert = CertificateDer::from_pem_slice(ca).unwrap();
    let svc_cert = CertificateDer::from_pem_slice(svc_cert).unwrap();
    let svc_key = PrivateKeyDer::from_pem_slice(svc_key).unwrap();
    let mut root_ca = RootCertStore::empty();
    assert_eq!(root_ca.add_parsable_certificates([ca_cert.clone()]), (1, 0));
    let client_conf = ClientConfig::builder_with_provider(Arc::new(client_crypto_provider))
        .with_protocol_versions(client_protocols)
        .unwrap()
        .with_root_certificates(Arc::new(root_ca))
        .with_no_client_auth();
    let server_conf = ServerConfig::builder_with_provider(Arc::new(server_crypto_provider))
        .with_protocol_versions(&[&TLS12, &TLS13])
        .unwrap()
        .with_no_client_auth()
        .with_single_cert(vec![svc_cert, ca_cert], svc_key)
        .unwrap();
    let (server_rx, server_tx) = pipe().unwrap();
    let (client_rx, client_tx) = pipe().unwrap();
    let mut server_sock = PipeSocket {
        tx: client_tx,
        rx: server_rx,
    };
    let mut client_sock = PipeSocket {
        tx: server_tx,
        rx: client_rx,
    };
    let mut client_conn = ClientConnection::new(
        Arc::new(client_conf),
        ServerName::try_from("www.google.com").unwrap(),
    )
    .unwrap();
    let client_thread = std::thread::spawn(move || {
        let mut client_stream = Stream::new(&mut client_conn, &mut client_sock);
        let mut buf = [0; EXPECTED_SERVER_MSG.len()];
        client_stream.read_exact(&mut buf).unwrap();
        assert_eq!(&buf, EXPECTED_SERVER_MSG);
        client_stream.write_all(EXPECTED_CLIENT_MSG).unwrap();
    });

    let mut server_conn = ServerConnection::new(Arc::new(server_conf)).unwrap();
    let mut server_stream = Stream::new(&mut server_conn, &mut server_sock);
    server_stream.write_all(EXPECTED_SERVER_MSG).unwrap();
    debug!("scheduled server message, polling client message");
    let mut buf = [0; EXPECTED_CLIENT_MSG.len()];
    server_stream.read_exact(&mut buf).unwrap();
    assert_eq!(&buf, EXPECTED_CLIENT_MSG);
    debug!("received client message");
    server_stream.conn.send_close_notify();
    if server_stream.conn.write_tls(server_stream.sock).is_err() {
        return;
    }
    let _ = server_stream;
    let _ = server_conn;
    let _ = server_sock;
    client_thread.join().unwrap();
}

#[test]
fn all_key_agreement_algorithms() {
    for group in [
        super::key_exchange::ECDH_P256,
        super::key_exchange::ECDH_P384,
        super::key_exchange::X25519,
        super::key_exchange::X25519MLKEM768,
    ] {
        test_cipher_suite(
            CA_CERT,
            RSA_SVC_CERT,
            RSA_SVC_KEY,
            &[&TLS13],
            CryptoProviderBuilder::new()
                .with_key_exchange_group(group)
                .with_cipher_suite(SupportedCipherSuite::Tls13(
                    cipher_suites::TLS13_AES_256_GCM_SHA384,
                ))
                .build(),
            CryptoProviderBuilder::new()
                .with_key_exchange_group(group)
                .with_full_cipher_suites()
                .build(),
        );
    }
    for group in [
        super::key_exchange::ECDH_P256,
        super::key_exchange::ECDH_P384,
        super::key_exchange::X25519,
    ] {
        test_cipher_suite(
            CA_CERT,
            RSA_SVC_CERT,
            RSA_SVC_KEY,
            &[&TLS13],
            CryptoProviderBuilder::new()
                .with_key_exchange_group(group)
                .with_cipher_suite(SupportedCipherSuite::Tls13(
                    cipher_suites::TLS13_AES_256_GCM_SHA384,
                ))
                .build(),
            ring::default_provider(),
        );
    }
}

fn test_half_connection(
    providers: &[fn() -> CryptoProvider],
    test_provider: fn() -> CryptoProvider,
    run_test_suite: impl Fn(CryptoProvider, CryptoProvider),
) {
    for provider in providers {
        run_test_suite(provider(), test_provider());
        run_test_suite(test_provider(), provider());
    }
}

#[test]
fn tls12_ecdhe_rsa_aes_128_gcm() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                RSA_SVC_CERT,
                RSA_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            );
        },
    );
}

#[test]
fn tls12_ecdhe_rsa_pss_aes_128_gcm() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                RSA_PSS_SVC_CERT,
                RSA_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdhe_rsa_with_aes_256_gcm_sha384() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                RSA_SVC_CERT,
                RSA_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdhe_ecdsa_with_chacha20_poly1305_sha256() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdhe_ecdsa_with_aes_128_gcm_sha256() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdhe_ecdsa_with_aes_256_gcm_sha384() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls13_aes_128_gcm_sha256() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls13(
                cipher_suites::TLS13_AES_128_GCM_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS13],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls13_aes_256_gcm_sha384() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls13(
                cipher_suites::TLS13_AES_256_GCM_SHA384,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS13],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls13_chacha20_poly1305_sha256() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls13(
                cipher_suites::TLS13_CHACHA20_POLY1305_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P256_SVC_CERT,
                ECDSA_P256_SVC_KEY,
                &[&TLS13],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdhe_rsa_with_chacha20_poly1305_sha256() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_cipher_suite(SupportedCipherSuite::Tls12(
                cipher_suites::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
            ))
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                RSA_SVC_CERT,
                RSA_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls13_rsa_server_cert() {
    test_cipher_suite(
        CA_CERT,
        RSA_SVC_CERT,
        RSA_SVC_KEY,
        &[&TLS13],
        CryptoProviderBuilder::full(),
        CryptoProviderBuilder::full(),
    );
}

#[test]
fn tls13_rsa_pss_server_cert() {
    test_cipher_suite(
        CA_CERT,
        RSA_PSS_SVC_CERT,
        RSA_SVC_KEY,
        &[&TLS13],
        CryptoProviderBuilder::full(),
        CryptoProviderBuilder::full(),
    );
}

#[test]
fn tls13_ecdsa_p384_server_cert() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_full_cipher_suites()
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P384_SVC_CERT,
                ECDSA_P384_SVC_KEY,
                &[&TLS13],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls12_ecdsa_p384_server_cert() {
    let providers = [CryptoProviderBuilder::full, ring::default_provider];
    let test_provider = || {
        CryptoProviderBuilder::new()
            .with_default_key_exchange_groups()
            .with_full_cipher_suites()
            .build()
    };
    test_half_connection(
        &providers,
        test_provider,
        |client_provider, server_provider| {
            test_cipher_suite(
                CA_CERT,
                ECDSA_P384_SVC_CERT,
                ECDSA_P384_SVC_KEY,
                &[&TLS12],
                server_provider,
                client_provider,
            )
        },
    );
}

#[test]
fn tls13_ed25519_server_cert() {
    // Ed25519 is not supported by ring, so test BoringSSL on both sides.
    test_cipher_suite(
        CA_CERT,
        ED25519_SVC_CERT,
        ED25519_SVC_KEY,
        &[&TLS13],
        CryptoProviderBuilder::full(),
        CryptoProviderBuilder::full(),
    );
}

#[test]
fn full_provider_both_sides_tls13() {
    test_cipher_suite(
        CA_CERT,
        ECDSA_P256_SVC_CERT,
        ECDSA_P256_SVC_KEY,
        &[&TLS13],
        CryptoProviderBuilder::full(),
        CryptoProviderBuilder::full(),
    );
}

#[test]
fn full_provider_both_sides_tls12() {
    test_cipher_suite(
        CA_CERT,
        ECDSA_P256_SVC_CERT,
        ECDSA_P256_SVC_KEY,
        &[&TLS12],
        CryptoProviderBuilder::full(),
        CryptoProviderBuilder::full(),
    );
}
