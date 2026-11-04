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

use std::{
    mem::MaybeUninit,
    thread::sleep, //
};

use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::X509StoreBuilder, //
};

use crate::{
    config::SrtpProtectionProfile,
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
    errors::Error,
    ffi::ReceiveBuffer,
    io::IoStatus, //
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
    let mut server_conn = server_ctx.new_server_connection();
    server_conn.with_mtu(500)?;
    let server_conn = server_conn.build();

    let mut client_ctx_builder = TlsContextBuilder::new_dtls();
    let ca = X509Certificate::parse_one_from_pem(super::CA)?;
    let mut cert_store = X509StoreBuilder::new();
    cert_store.set_trust(Trust::SslServer)?.add_cert(ca)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let mut client_conn = client_ctx.new_client_connection();
    client_conn.with_mtu(500)?;
    let client_conn = client_conn.build();

    Ok((server_conn, client_conn))
}

use std::time::Duration;

use crate::errors::TlsRetryReason;

fn handle_sync_dtls_timeout<R>(conn: &mut TlsConnection<R, DtlsMode>) -> Result<(), Error> {
    if let Some(timeout) = conn.dtlsv1_get_timeout() {
        sleep(timeout.min(Duration::from_millis(10)));
        conn.dtlsv1_handle_timeout()?;
    } else {
        sleep(Duration::from_millis(5));
    }
    Ok(())
}

fn drive_dtls_handshake<R>(conn: &mut TlsConnection<R, DtlsMode>) -> Result<(), Error> {
    loop {
        match conn.do_handshake() {
            Ok(None) => break Ok(()),
            Ok(Some(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
                handle_sync_dtls_timeout(conn)?;
            }
            Ok(Some(reason)) => panic!("unexpected retry reason {reason:?}"),
            Err(e) => break Err(e),
        }
    }
}

fn dtls_sync_recv<R>(
    conn: &mut TlsConnection<R, DtlsMode>,
    buf: &mut ReceiveBuffer<'_>,
) -> Result<usize, Error> {
    loop {
        match conn.sync_recv(buf) {
            Ok(IoStatus::Ok(n)) => break Ok(n),
            Ok(IoStatus::Retry(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
                handle_sync_dtls_timeout(conn)?;
            }
            Ok(status) => panic!("unexpected status {status:?}"),
            Err(e) => break Err(e),
        }
    }
}

fn dtls_sync_send<R>(conn: &mut TlsConnection<R, DtlsMode>, data: &[u8]) -> Result<usize, Error> {
    loop {
        match conn.sync_send(data) {
            Ok(IoStatus::Ok(n)) => break Ok(n),
            Ok(IoStatus::Retry(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
                handle_sync_dtls_timeout(conn)?;
            }
            Ok(status) => panic!("unexpected status {status:?}"),
            Err(e) => break Err(e),
        }
    }
}

fn dtls_sync_shutdown<R>(conn: &mut TlsConnection<R, DtlsMode>) -> Result<(), Error> {
    loop {
        let Some(mut established) = conn.established() else {
            break Ok(());
        };
        match established.sync_shutdown() {
            Ok(None) => break Ok(()),
            Ok(Some(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
                handle_sync_dtls_timeout(conn)?;
            }
            Ok(Some(reason)) => panic!("unexpected retry reason {reason:?}"),
            Err(e) => break Err(e),
        }
    }
}

fn sync_ping_pong_datagram(
    mut server_conn: TlsConnection<Server, DtlsMode>,
    mut client_conn: TlsConnection<Client, DtlsMode>,
) -> Result<(), Error> {
    let thread = std::thread::spawn(move || {
        drive_dtls_handshake(&mut server_conn)?;
        assert!(!server_conn.is_in_handshake());
        let mut message = [MaybeUninit::uninit(); 21];
        let mut message = ReceiveBuffer::new_uninit(&mut message);
        let n = dtls_sync_recv(&mut server_conn, &mut message)?;
        assert_eq!(n, 21);
        assert_eq!(*message, *b"BoringSSL is awesome!");
        dtls_sync_send(&mut server_conn, b"Oh yeah definitely!")?;
        dtls_sync_shutdown(&mut server_conn)?;
        // A `UnixDatagram` pair fails the peer's `send` with `ECONNREFUSED` as soon as this socket
        // is closed.
        // We will wait for the peer's `close_notify` here.
        // We don't care about the status of the connection after shutdown, however.
        let mut eof = [MaybeUninit::uninit(); 1];
        let mut eof = ReceiveBuffer::new_uninit(&mut eof);
        let _ = dtls_sync_recv(&mut server_conn, &mut eof);
        Ok::<_, Error>(())
    });

    drive_dtls_handshake(&mut client_conn)?;
    assert!(!client_conn.is_in_handshake());
    dtls_sync_send(&mut client_conn, b"BoringSSL is awesome!")?;
    let mut message = [MaybeUninit::uninit(); 19];
    let mut message = ReceiveBuffer::new_uninit(&mut message);
    let n = dtls_sync_recv(&mut client_conn, &mut message)?;
    assert_eq!(n, 19);
    assert_eq!(*message, *b"Oh yeah definitely!");
    dtls_sync_shutdown(&mut client_conn)?;
    thread.join().unwrap()?;

    Ok(())
}

#[cfg(unix)]
#[test]
fn dtls() {
    use crate::{io::sync_io::NoAsync, io::unix::StdDatagram};

    let (mut server_conn, mut client_conn) = dumb_dtls_server_client().unwrap();
    let (server_sock, client_sock) = std::os::unix::net::UnixDatagram::pair().unwrap();
    server_sock
        .set_read_timeout(Some(Duration::from_millis(50)))
        .unwrap();
    client_sock
        .set_read_timeout(Some(Duration::from_millis(50)))
        .unwrap();
    let server_sock = StdDatagram::new(server_sock, NoAsync);
    let client_sock = StdDatagram::new(client_sock, NoAsync);
    server_conn.set_datagram_socket(server_sock).unwrap();
    client_conn.set_datagram_socket(client_sock).unwrap();
    sync_ping_pong_datagram(server_conn, client_conn).unwrap();
}

#[test]
fn test_async_dtls() -> Result<(), Error> {
    use crate::io::IoStatus;
    use crate::tests::{TEST_DATA, create_mock_datagram};

    let (mut server_conn, mut client_conn) = dumb_dtls_server_client()?;

    let (client_socket, server_socket, mut executor) = create_mock_datagram();

    server_conn.set_datagram_socket(server_socket)?;
    client_conn.set_datagram_socket(client_socket)?;

    let test_future = async {
        futures::future::try_join(server_conn.async_handshake(), client_conn.async_handshake())
            .await?;

        let server_data = async {
            let mut buf = [0u8; TEST_DATA.len()];
            loop {
                let mut message = ReceiveBuffer::new(&mut buf);
                match server_conn.as_pin_mut().async_recv(&mut message).await? {
                    IoStatus::Ok(n) if n == TEST_DATA.len() => break,
                    _ => continue,
                }
            }
            assert_eq!(&buf, TEST_DATA);
            Ok::<(), Error>(())
        };

        let client_data = async {
            client_conn.as_pin_mut().async_send(TEST_DATA).await?;
            Ok::<(), Error>(())
        };

        futures::future::try_join(server_data, client_data).await?;

        Ok::<(), Error>(())
    };

    executor.run(test_future)?;

    Ok(())
}

#[test]
fn test_dtls_srtp_invalid_profile() {
    let mut ctx = TlsContextBuilder::new_dtls();
    assert!(ctx.with_srtp_profiles(&[]).is_err());
    assert!(
        ctx.with_srtp_profiles(&[
            SrtpProtectionProfile::AeadAes128Gcm,
            SrtpProtectionProfile::AeadAes128Gcm,
        ])
        .is_err()
    );
}

#[test]
fn test_dtls_srtp_negotiation() -> Result<(), Error> {
    let ca = Certificate::parse_one_from_pem(super::CA, None)?;
    let server_cert = Certificate::parse_one_from_pem(super::RSA_SERVER_CERT, None)?;
    let server_key = PrivateKey::from_pem(super::RSA_SERVER_KEY, || unreachable!())?;

    let mut server_ctx_builder = TlsContextBuilder::new_dtls();
    server_ctx_builder.with_srtp_profiles(&[
        SrtpProtectionProfile::AeadAes128Gcm,
        SrtpProtectionProfile::Aes128CmSha1_80,
    ])?;
    let server_cred = {
        let mut builder = TlsCredentialBuilder::new();
        builder
            .with_certificate_chain(&[server_cert, ca])?
            .with_private_key(server_key)?;
        builder.build()
    };
    server_ctx_builder.with_credential(server_cred.unwrap())?;
    let server_ctx = server_ctx_builder.build();
    let mut server_conn = server_ctx.new_server_connection();
    server_conn.with_mtu(500)?;
    let mut server_conn = server_conn.build();
    assert_eq!(server_conn.selected_srtp_profile(), None);

    let mut client_ctx_builder = TlsContextBuilder::new_dtls();
    client_ctx_builder.with_srtp_profiles(&[
        SrtpProtectionProfile::Aes128CmSha1_80,
        SrtpProtectionProfile::AeadAes128Gcm,
    ])?;
    let ca = X509Certificate::parse_one_from_pem(super::CA)?;
    let mut cert_store = X509StoreBuilder::new();
    cert_store.set_trust(Trust::SslServer)?.add_cert(ca)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let mut client_conn = client_ctx.new_client_connection();
    client_conn.with_mtu(500)?;
    let mut client_conn = client_conn.build();
    assert_eq!(client_conn.selected_srtp_profile(), None);

    let (client_socket, server_socket, mut executor) = super::create_mock_datagram();
    server_conn.set_datagram_socket(server_socket)?;
    client_conn.set_datagram_socket(client_socket)?;

    executor.run(async {
        futures::future::try_join(server_conn.async_handshake(), client_conn.async_handshake())
            .await
    })?;

    assert_eq!(
        server_conn.selected_srtp_profile(),
        Some(SrtpProtectionProfile::AeadAes128Gcm)
    );
    assert_eq!(
        client_conn.selected_srtp_profile(),
        Some(SrtpProtectionProfile::AeadAes128Gcm)
    );
    Ok(())
}
