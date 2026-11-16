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

use crate::errors::Error;

#[cfg(unix)]
#[test]
fn ping_pong() -> Result<(), Error> {
    use bssl_x509::{
        certificates::X509Certificate, keys::PrivateKey, params::Trust, store::X509StoreBuilder,
    };

    use super::{CA, RSA_SERVER_CERT, RSA_SERVER_KEY};
    use crate::{
        context::{CertificateCache, TlsContextBuilder},
        credentials::{Certificate, CertificateVerificationMode, TlsCredentialBuilder},
        io::sync_io::{NoAsync, StdIoWithReactor},
        tests::sync_ping_pong,
    };

    let cache = CertificateCache::new();
    let ca = Certificate::parse_one_from_pem(CA, Some(&cache))?;
    let server_cert = Certificate::parse_one_from_pem(RSA_SERVER_CERT, Some(&cache))?;
    let server_key = PrivateKey::from_pem(RSA_SERVER_KEY, || unreachable!())?;

    let mut server_ctx_builder = TlsContextBuilder::new_tls();
    let server_cred = {
        let mut builder = TlsCredentialBuilder::new();
        builder
            .with_certificate_chain(&[server_cert, ca])?
            .with_private_key(server_key)?;
        builder.build()
    };
    server_ctx_builder
        .with_certificate_cache(Some(&cache))
        .with_credential(server_cred.unwrap())?;
    let server_ctx = server_ctx_builder.build();
    let mut server_conn = server_ctx.new_server_connection().build();

    let mut client_ctx_builder = TlsContextBuilder::new_tls();
    let mut cert_store = X509StoreBuilder::new();
    cert_store
        .set_trust(Trust::SslServer)?
        .add_cert(X509Certificate::parse_one_from_pem(&CA)?)?;
    let cert_store = cert_store.build();
    client_ctx_builder
        .with_certificate_cache(Some(&cache))
        .with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let mut client_conn = client_ctx.new_client_connection();
    client_conn.with_certificate_verification_mode(CertificateVerificationMode::PeerCertMandatory);
    let mut client_conn = client_conn.build();
    client_conn
        .in_handshake()
        .unwrap()
        .set_host("www.google.com")?;

    let (server_rx, server_tx) = std::io::pipe().unwrap();
    let (client_rx, client_tx) = std::io::pipe().unwrap();
    let server_rx = StdIoWithReactor::new(server_rx, NoAsync);
    let server_tx = StdIoWithReactor::new(server_tx, NoAsync);
    let client_rx = StdIoWithReactor::new(client_rx, NoAsync);
    let client_tx = StdIoWithReactor::new(client_tx, NoAsync);
    server_conn.set_split_io(client_rx, server_tx)?;
    client_conn.set_split_io(server_rx, client_tx)?;
    sync_ping_pong(server_conn, client_conn)
}
