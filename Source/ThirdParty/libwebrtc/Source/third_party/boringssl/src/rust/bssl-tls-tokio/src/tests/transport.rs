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
    errors::Error, //
};
use tokio::io::{
    AsyncReadExt,
    AsyncWriteExt, //
};

use crate::{
    TokioIo,
    TokioTlsConnection, //
};

fn dumb_server_client() -> (TlsConnection<Server>, TlsConnection<Client>) {
    use bssl_tls::context::TlsContextBuilder;

    let mut server_builder = TlsContextBuilder::new_tls();
    server_builder
        .with_credential(super::server_credential())
        .unwrap();
    let server_conn = server_builder.build().new_server_connection().build();

    let mut client_builder = TlsContextBuilder::new_tls();
    client_builder.with_certificate_store(&super::client_cert_store());
    let client_conn = client_builder.build().new_client_connection().build();

    (server_conn, client_conn)
}

#[cfg(unix)]
#[tokio::test]
async fn tokio_io() -> Result<(), Error> {
    let (server_conn, client_conn) = dumb_server_client();

    let (server_tx, server_rx) = tokio::net::unix::pipe::pipe().unwrap();
    let (client_tx, client_rx) = tokio::net::unix::pipe::pipe().unwrap();
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

        server_conn.shutdown().await.unwrap();

        let mut eof = [0; 1];
        let _ = server_conn.read(&mut eof).await;
    });
    client_conn
        .write_all(b"BoringSSL is awesome!")
        .await
        .unwrap();
    let mut message = [0; 19];
    client_conn.read_exact(&mut message).await.unwrap();
    assert_eq!(message, *b"Oh yeah definitely!");
    client_conn.shutdown().await.unwrap();
    thread.await.unwrap();

    Ok(())
}
