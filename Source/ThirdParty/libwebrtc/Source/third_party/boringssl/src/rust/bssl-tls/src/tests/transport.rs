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

use bssl_x509::params::Trust;

use crate::tests::dumb_server_client;

#[cfg(unix)]
#[test]
fn stdio() {
    use std::io::{Read, Write};

    use crate::io::sync_io::{NoAsync, StdIoWithReactor};

    let (mut server_conn, mut client_conn) = dumb_server_client().unwrap();

    let (server_rx, server_tx) = std::io::pipe().unwrap();
    let (client_rx, client_tx) = std::io::pipe().unwrap();
    let server_rx = StdIoWithReactor::new(server_rx, NoAsync);
    let server_tx = StdIoWithReactor::new(server_tx, NoAsync);
    let client_rx = StdIoWithReactor::new(client_rx, NoAsync);
    let client_tx = StdIoWithReactor::new(client_tx, NoAsync);

    server_conn.set_split_io(client_rx, server_tx).unwrap();
    client_conn.set_split_io(server_rx, client_tx).unwrap();

    let thread = std::thread::spawn(move || {
        let mut message = [0; 21];
        // Use `std::io::Read::read_exact`
        server_conn.read_exact(&mut message).unwrap();
        assert_eq!(message, *b"BoringSSL is awesome!");
        // Use `std::io::Write::write_all`
        server_conn.write_all(b"Oh yeah definitely!").unwrap();
        server_conn.established().unwrap().sync_shutdown().unwrap();
        // Second shutdown poll.
        server_conn.established().unwrap().sync_shutdown().unwrap();
    });

    // Use `std::io::Write::write_all`
    client_conn.write_all(b"BoringSSL is awesome!").unwrap();
    let mut message = [0; 19];
    // Use `std::io::Read::read_exact`
    client_conn.read_exact(&mut message).unwrap();
    assert_eq!(message, *b"Oh yeah definitely!");
    client_conn.established().unwrap().sync_shutdown().unwrap();
    thread.join().unwrap();
}

#[cfg(feature = "std")]
#[test]
fn high_level_sync() -> Result<(), crate::errors::Error> {
    use crate::context::TlsContextBuilder;
    use crate::credentials::{Certificate, TlsCredentialBuilder};
    use crate::tests::{CA, RSA_SERVER_CERT, RSA_SERVER_KEY};
    use bssl_x509::certificates::X509Certificate;
    use bssl_x509::keys::PrivateKey;
    use bssl_x509::store::X509StoreBuilder;
    use std::io::{Read, Write};

    let ca = Certificate::parse_one_from_pem(CA, None)?;
    let server_cert = Certificate::parse_one_from_pem(RSA_SERVER_CERT, None)?;
    let server_key = PrivateKey::from_pem(RSA_SERVER_KEY, || unreachable!())?;

    let mut server_ctx_builder = TlsContextBuilder::new_tls();
    let server_cred = {
        let mut builder = TlsCredentialBuilder::new();
        builder
            .with_certificate_chain(&[server_cert, ca])?
            .with_private_key(server_key)?;
        builder.build()
    };
    server_ctx_builder.with_credential(server_cred.unwrap())?;
    let mut builder = TlsContextBuilder::new_tls();
    let ca = X509Certificate::parse_one_from_pem(CA)?;
    let store = {
        let mut store = X509StoreBuilder::new();
        store.set_trust(Trust::SslServer)?.add_cert(ca)?;
        store.build()
    };
    builder.with_certificate_store(&store);
    let connector = builder.build_connector();
    let acceptor = server_ctx_builder.build_acceptor();

    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();

    let server_thread = std::thread::spawn(move || {
        let (stream, _) = listener.accept().unwrap();
        let mut tls_stream = acceptor.accept(stream).unwrap();

        let mut buf = [0; 5];
        tls_stream.read_exact(&mut buf).unwrap();
        assert_eq!(&buf, b"hello");

        tls_stream.write_all(b"world").unwrap();
        tls_stream.flush().unwrap();
    });

    let stream = std::net::TcpStream::connect(addr).unwrap();
    let mut tls_stream = connector.connect("www.google.com", stream).unwrap();

    tls_stream.write_all(b"hello").unwrap();
    tls_stream.flush().unwrap();

    let mut buf = [0; 5];
    tls_stream.read_exact(&mut buf).unwrap();
    assert_eq!(&buf, b"world");

    server_thread.join().unwrap();

    Ok(())
}
