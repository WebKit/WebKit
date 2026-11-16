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

use std::time::Duration;

use bssl_tls::{
    ReceiveBuffer,
    connection::{
        Client,
        Server,
        TlsConnection, //
    },
    context::{
        DtlsMode,
        TlsContextBuilder, //
    },
    errors::Error,
    io::IoStatus, //
};
use tokio::{
    select,
    time::sleep, //
};

use crate::{
    TokioDatagramIo,
    new_std_datagram_with_tokio, //
};

fn dumb_dtls_server_client() -> (
    TlsConnection<Server, DtlsMode>,
    TlsConnection<Client, DtlsMode>,
) {
    let mut server_ctx_builder = TlsContextBuilder::new_dtls();
    server_ctx_builder
        .with_credential(super::server_credential())
        .unwrap();
    let mut server_conn = server_ctx_builder.build().new_server_connection();
    server_conn.with_mtu(500).unwrap();
    let server_conn = server_conn.build();

    let mut client_ctx_builder = TlsContextBuilder::new_dtls();
    client_ctx_builder.with_certificate_store(&super::client_cert_store());
    let mut client_conn = client_ctx_builder.build().new_client_connection();
    client_conn.with_mtu(500).unwrap();
    let client_conn = client_conn.build();

    (server_conn, client_conn)
}

async fn drive_async_dtls_handshake<R: Send + 'static>(
    conn: &mut TlsConnection<R, DtlsMode>,
) -> Result<(), Error> {
    loop {
        let timeout = conn.dtlsv1_get_timeout().unwrap_or(Duration::from_secs(5));
        select! {
            biased;
            res = conn.async_handshake() => match res? {
                None => break Ok(()),
                Some(reason) => panic!("unexpected retry reason {reason:?}"),
            },
            _ = sleep(timeout) => {
                conn.dtlsv1_handle_timeout()?;
            }
        }
    }
}

async fn async_dtls_recv<R: Send + 'static>(
    conn: &mut TlsConnection<R, DtlsMode>,
    buf: &mut ReceiveBuffer<'_>,
) -> Result<IoStatus, Error> {
    conn.as_pin_mut().async_recv(buf).await
}

async fn async_dtls_send<R: Send + 'static>(
    conn: &mut TlsConnection<R, DtlsMode>,
    data: &[u8],
) -> Result<IoStatus, Error> {
    conn.as_pin_mut().async_send(data).await
}

async fn async_dtls_ping_pong(
    mut server_conn: TlsConnection<Server, DtlsMode>,
    mut client_conn: TlsConnection<Client, DtlsMode>,
) -> Result<(), Error> {
    let task = tokio::spawn(async move {
        drive_async_dtls_handshake(&mut server_conn).await?;

        let mut buf = [0u8; 21];
        let expected = b"BoringSSL is awesome!";
        loop {
            let mut message = ReceiveBuffer::new(&mut buf);
            match async_dtls_recv(&mut server_conn, &mut message).await? {
                IoStatus::Ok(n) if n == expected.len() => break,
                _ => continue,
            }
        }
        assert_eq!(&buf, expected);
        async_dtls_send(&mut server_conn, b"Oh yeah definitely!").await?;
        Ok::<_, Error>(())
    });

    drive_async_dtls_handshake(&mut client_conn).await?;
    async_dtls_send(&mut client_conn, b"BoringSSL is awesome!").await?;
    let mut buf = [0u8; 19];
    let expected = b"Oh yeah definitely!";
    loop {
        let mut message = ReceiveBuffer::new(&mut buf);
        match async_dtls_recv(&mut client_conn, &mut message).await? {
            IoStatus::Ok(n) if n == expected.len() => break,
            _ => continue,
        }
    }
    assert_eq!(&buf, expected);
    task.await.unwrap()?;
    Ok(())
}

#[cfg(unix)]
#[tokio::test]
async fn async_dtls() -> Result<(), Error> {
    let (mut server_conn, mut client_conn) = dumb_dtls_server_client();
    let (server_sock, client_sock) = tokio::net::UnixDatagram::pair().unwrap();
    server_conn
        .set_datagram_socket(TokioDatagramIo(server_sock))
        .unwrap();
    client_conn
        .set_datagram_socket(TokioDatagramIo(client_sock))
        .unwrap();

    async_dtls_ping_pong(server_conn, client_conn).await
}

#[cfg(unix)]
#[tokio::test]
async fn async_dtls_over_fd() -> Result<(), Error> {
    let (mut server_conn, mut client_conn) = dumb_dtls_server_client();
    let (server_sock, client_sock) = std::os::unix::net::UnixDatagram::pair().unwrap();
    server_sock.set_nonblocking(true).unwrap();
    client_sock.set_nonblocking(true).unwrap();
    let server_sock = new_std_datagram_with_tokio(server_sock).unwrap();
    let client_sock = new_std_datagram_with_tokio(client_sock).unwrap();
    server_conn.set_datagram_socket(server_sock).unwrap();
    client_conn.set_datagram_socket(client_sock).unwrap();

    async_dtls_ping_pong(server_conn, client_conn).await
}
