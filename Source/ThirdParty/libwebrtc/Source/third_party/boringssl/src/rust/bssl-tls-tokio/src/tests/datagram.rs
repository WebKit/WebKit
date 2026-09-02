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

#![cfg(unix)]

use bssl_tls::{
    connection::{
        Client,
        Server,
        TlsConnection, //
    },
    context::{
        DtlsMode,
        TlsContextBuilder, //
    },
    credentials::{
        Certificate,
        TlsCredentialBuilder, //
    },
    errors::Error, //
};
use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::X509StoreBuilder, //
};

use super::{
    CA,
    RSA_SERVER_CERT,
    RSA_SERVER_KEY, //
};
use crate::{
    TokioDatagramIo,
    new_std_datagram_with_tokio, //
};

fn dumb_dtls_server_client() -> Result<
    (
        TlsConnection<Server, DtlsMode>,
        TlsConnection<Client, DtlsMode>,
    ),
    Error,
> {
    let ca = Certificate::parse_one_from_pem(CA, None)?;
    let server_cert = Certificate::parse_one_from_pem(RSA_SERVER_CERT, None)?;
    let server_key = PrivateKey::from_pem(RSA_SERVER_KEY, || unreachable!())?;

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
    let ca = X509Certificate::parse_one_from_pem(CA)?;
    let mut cert_store = X509StoreBuilder::new();
    cert_store.set_trust(Trust::SslServer)?.add_cert(ca)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let client_conn = client_ctx.new_client_connection().build();

    Ok((server_conn, client_conn))
}

async fn async_ping_pong(
    mut server_conn: TlsConnection<Server, DtlsMode>,
    mut client_conn: TlsConnection<Client, DtlsMode>,
) -> Result<(), Error> {
    use bssl_tls::io::IoStatus;
    use std::time::Duration;

    let task = tokio::spawn(async move {
        server_conn.async_handshake().await?;

        let mut message = [0; 21];
        let mut read_bytes = 0;
        while read_bytes < 21 {
            match server_conn
                .as_pin_mut()
                .async_read(&mut message[read_bytes..])
                .await?
            {
                IoStatus::Ok(n) => read_bytes += n,
                IoStatus::EndOfStream => break,
                _ => {}
            }
        }
        assert_eq!(&message, b"BoringSSL is awesome!");
        tokio::time::sleep(Duration::from_secs(2)).await;
        server_conn
            .as_pin_mut()
            .async_write(b"Oh yeah definitely!")
            .await?;
        server_conn.as_pin_mut().async_shutdown().await?;
        Ok::<_, Error>(())
    });

    client_conn.async_handshake().await?;
    client_conn
        .as_pin_mut()
        .async_write(b"BoringSSL is awesome!")
        .await?;
    let mut message = [0; 19];
    let mut read_bytes = 0;
    while read_bytes < 19 {
        match client_conn
            .as_pin_mut()
            .async_read(&mut message[read_bytes..])
            .await?
        {
            IoStatus::Ok(n) => read_bytes += n,
            IoStatus::EndOfStream => break,
            _ => {}
        }
    }
    assert_eq!(&message, b"Oh yeah definitely!");
    assert!(matches!(
        client_conn.as_pin_mut().async_shutdown().await,
        Ok(_) | Err(Error::Io(bssl_tls::errors::IoError::EndOfStream))
    ));
    task.await.unwrap()?;
    Ok(())
}

#[cfg(unix)]
#[tokio::test]
#[ignore = "https://crbug.com/532601068"]
async fn async_dtls() -> Result<(), Error> {
    let (mut server_conn, mut client_conn) = dumb_dtls_server_client().unwrap();
    let (server_sock, client_sock) = tokio::net::UnixDatagram::pair().unwrap();
    server_conn.set_io(TokioDatagramIo(server_sock)).unwrap();
    client_conn.set_io(TokioDatagramIo(client_sock)).unwrap();

    async_ping_pong(server_conn, client_conn).await
}

#[cfg(unix)]
#[tokio::test]
#[ignore = "https://crbug.com/532601068"]
async fn async_dtls_over_fd() -> Result<(), Error> {
    let (mut server_conn, mut client_conn) = dumb_dtls_server_client().unwrap();
    let (server_sock, client_sock) = std::os::unix::net::UnixDatagram::pair().unwrap();
    server_sock.set_nonblocking(true).unwrap();
    client_sock.set_nonblocking(true).unwrap();
    let server_sock = new_std_datagram_with_tokio(server_sock).unwrap();
    let client_sock = new_std_datagram_with_tokio(client_sock).unwrap();
    server_conn.set_io(server_sock).unwrap();
    client_conn.set_io(client_sock).unwrap();

    async_ping_pong(server_conn, client_conn).await
}
