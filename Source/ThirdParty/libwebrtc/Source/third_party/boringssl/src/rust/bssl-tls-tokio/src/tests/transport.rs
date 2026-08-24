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

use bssl_tls::{
    connection::{
        Client,
        Server,
        TlsConnection, //
    },
    context::TlsContextBuilder,
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
use futures::future::FutureExt;
use tokio::io::{
    AsyncReadExt,
    AsyncWriteExt, //
};

use super::{
    CA,
    RSA_SERVER_CERT,
    RSA_SERVER_KEY, //
};
use crate::{
    TokioIo,
    TokioTlsConnection, //
};

fn dumb_server_client() -> Result<(TlsConnection<Server>, TlsConnection<Client>), Error> {
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
    let server_ctx = server_ctx_builder.build();
    let server_conn = server_ctx.new_server_connection().build();

    let mut client_ctx_builder = TlsContextBuilder::new_tls();
    let mut cert_store = X509StoreBuilder::new();
    cert_store
        .set_trust(Trust::SslServer)?
        .add_cert(X509Certificate::parse_one_from_pem(&CA)?)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let client_conn = client_ctx.new_client_connection().build();

    Ok((server_conn, client_conn))
}

#[cfg(unix)]
#[tokio::test]
async fn tokio_io() -> Result<(), Error> {
    let (server_conn, client_conn) = dumb_server_client()?;

    let (server_tx, server_rx) = tokio::net::unix::pipe::pipe().unwrap();
    let (client_tx, client_rx) = tokio::net::unix::pipe::pipe().unwrap();
    let (send_signal, recv_signal) = tokio::sync::oneshot::channel();
    let server_rx = TokioIo(server_rx);
    let server_tx = TokioIo(server_tx);
    let client_rx = TokioIo(client_rx);
    let client_tx = TokioIo(client_tx);

    let mut server_conn = TokioTlsConnection::new(server_conn);
    let mut client_conn = TokioTlsConnection::new(client_conn);

    server_conn.set_split_io(client_rx, server_tx)?;
    client_conn.set_split_io(server_rx, client_tx)?;

    let thread = tokio::spawn(async move {
        let mut message = [0; 21];
        server_conn.read_exact(&mut message).await.unwrap();
        assert_eq!(message, *b"BoringSSL is awesome!");
        server_conn.write_all(b"Oh yeah definitely!").await.unwrap();

        // The original test used sync_shutdown on established()
        // We use wrapper's shutdown instead.

        let res = server_conn.shutdown().now_or_never();
        assert!(res.is_none(), "{res:?}");

        // Make the client progress
        send_signal.send(()).unwrap();

        server_conn.shutdown().await.unwrap();
    });
    client_conn
        .write_all(b"BoringSSL is awesome!")
        .await
        .unwrap();
    recv_signal.await.unwrap();
    let mut message = [0; 19];
    client_conn.read_exact(&mut message).await.unwrap();
    assert_eq!(message, *b"Oh yeah definitely!");
    client_conn.shutdown().await.unwrap();
    thread.await.unwrap();

    Ok(())
}
