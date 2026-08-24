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

use bssl_x509::{
    certificates::X509Certificate, keys::PrivateKey, params::Trust, store::X509StoreBuilder,
};

use crate::{
    connection::{Client, Server, TlsConnection},
    context::{DtlsMode, TlsContextBuilder},
    credentials::{Certificate, TlsCredentialBuilder},
    errors::Error,
};

// TODO(@xfding): this function will come useful for Windows tests.
#[allow(unused)]
fn dumb_dtls_server_client() -> Result<
    (
        TlsConnection<Server, DtlsMode>,
        TlsConnection<Client, DtlsMode>,
    ),
    Error,
> {
    let ca = Certificate::parse_one_from_pem(super::CA, None)?;
    let server_cert = Certificate::parse_one_from_pem(super::RSA_SERVER_CERT, None)?;
    let server_key = PrivateKey::from_pem(super::RSA_SERVER_KEY, || unreachable!())?;

    let mut server_ctx_builder = TlsContextBuilder::new_dtls();
    let server_cred = {
        let mut builder = TlsCredentialBuilder::new();
        builder
            .with_certificate_chain(&[server_cert, ca])?
            .with_private_key(server_key)?;
        builder.build()
    };
    server_ctx_builder.with_credential(server_cred.unwrap())?;
    let server_ctx = server_ctx_builder.build();
    let server_conn = server_ctx.new_server_connection().build();

    let mut client_ctx_builder = TlsContextBuilder::new_dtls();
    let ca = X509Certificate::parse_one_from_pem(super::CA)?;
    let mut cert_store = X509StoreBuilder::new();
    cert_store.set_trust(Trust::SslServer)?.add_cert(ca)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let client_conn = client_ctx.new_client_connection().build();

    Ok((server_conn, client_conn))
}

#[cfg(unix)]
#[ignore = "https://crbug.com/532601068"]
#[test]
fn dtls() {
    use crate::{io::sync_io::NoAsync, io::unix::StdDatagram, tests::sync_ping_pong};

    let (mut server_conn, mut client_conn) = dumb_dtls_server_client().unwrap();
    let (server_sock, client_sock) = std::os::unix::net::UnixDatagram::pair().unwrap();
    let server_sock = StdDatagram::new(server_sock, NoAsync);
    let client_sock = StdDatagram::new(client_sock, NoAsync);
    server_conn.set_io(server_sock).unwrap();
    client_conn.set_io(client_sock).unwrap();
    sync_ping_pong(server_conn, client_conn).unwrap();
}

#[ignore = "https://crbug.com/532601068"]
#[test]
fn test_async_dtls() -> Result<(), Error> {
    use crate::io::IoStatus;
    use crate::tests::{TEST_DATA, create_mock_datagram};

    let (mut server_conn, mut client_conn) = dumb_dtls_server_client()?;

    let (client_socket, server_socket, mut executor) = create_mock_datagram();

    server_conn.set_io(server_socket)?;
    client_conn.set_io(client_socket)?;

    let test_future = async {
        futures::future::try_join(server_conn.async_handshake(), client_conn.async_handshake())
            .await?;

        let server_data = async {
            let mut buf = [0u8; TEST_DATA.len()];
            let mut read_bytes = 0;
            while read_bytes < TEST_DATA.len() {
                match server_conn
                    .as_pin_mut()
                    .async_read(&mut buf[read_bytes..])
                    .await?
                {
                    IoStatus::Ok(n) => read_bytes += n,
                    IoStatus::EndOfStream => break,
                    _ => {}
                }
            }
            assert_eq!(&buf, TEST_DATA);
            Ok::<(), Error>(())
        };

        let client_data = async {
            client_conn.as_pin_mut().async_write(TEST_DATA).await?;
            Ok::<(), Error>(())
        };

        futures::future::try_join(server_data, client_data).await?;

        Ok::<(), Error>(())
    };

    executor.run(test_future)?;

    Ok(())
}
