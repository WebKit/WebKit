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
    collections::VecDeque,
    mem::MaybeUninit,
    pin::pin,
    sync::{
        Arc,
        Mutex, //
    },
    task::Context, //
};

use crate::{
    ReceiveBuffer,
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
    errors::Error,
    io::{
        AbstractReader,
        AbstractSocket,
        AbstractSocketResult,
        AbstractWriter,
        IoStatus, //
    }, //
};

use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::X509StoreBuilder, //
};
use futures::future::join;

const CA: &[u8] = include_bytes!("../../test-data/BoringSSLCATest.crt");
const RSA_SERVER_CERT: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.crt");
const RSA_SERVER_KEY: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.key");

mod datagram;
mod handshake;
mod transport;

/// Dumb server-client pair that does no certificate verification.
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
    let server_conn = server_ctx.new_server_connection(None)?.build();

    let mut client_ctx_builder = TlsContextBuilder::new_tls();
    let mut cert_store = X509StoreBuilder::new();
    cert_store
        .set_trust(Trust::SslServer)?
        .add_cert(X509Certificate::parse_one_from_pem(CA)?)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let client_conn = client_ctx.new_client_connection(None)?.build();

    Ok((server_conn, client_conn))
}

fn sync_ping_pong<
    M: crate::connection::methods::HasTlsConnectionMethod
        + crate::context::SupportedMode
        + crate::context::HasBasicIo
        + 'static,
>(
    mut server_conn: TlsConnection<Server, M>,
    mut client_conn: TlsConnection<Client, M>,
) -> Result<(), Error> {
    let thread = std::thread::spawn(move || {
        server_conn.in_handshake().unwrap().accept()?;
        assert!(!server_conn.is_in_handshake());
        // TODO: switch to `From` impls when Rust compiler is bumped to 1.95.0.
        let mut message = [MaybeUninit::uninit(); 21];
        let mut message = ReceiveBuffer::new_uninit(&mut message);
        assert!(matches!(
            server_conn.sync_read(&mut message)?,
            IoStatus::Ok(21)
        ));
        assert_eq!(*message, *b"BoringSSL is awesome!");
        server_conn.sync_write(b"Oh yeah definitely!")?;
        server_conn.established().unwrap().sync_shutdown()?;
        // Second shutdown poll.
        server_conn.established().unwrap().sync_shutdown()?;
        Ok::<_, Error>(())
    });

    client_conn.in_handshake().unwrap().connect()?;
    assert!(!client_conn.is_in_handshake());
    client_conn.sync_write(b"BoringSSL is awesome!")?;
    let mut message = [MaybeUninit::uninit(); 19];
    let mut message = ReceiveBuffer::new_uninit(&mut message);
    assert!(matches!(
        client_conn.sync_read(&mut message)?,
        IoStatus::Ok(19)
    ));
    assert_eq!(*message, *b"Oh yeah definitely!");
    client_conn.established().unwrap().sync_shutdown()?;
    thread.join().unwrap()?;

    Ok(())
}

pub const TEST_DATA: &[u8] = b"BoringSSL is awesome!";

/// A mocked pipe that will never close or emit EOF.
#[derive(Default)]
pub struct MockPipe {
    pub buf: VecDeque<u8>,
    pub data_waker: Option<std::task::Waker>,
}

pub struct OperationState {
    /// Whether the mock I/O object needs to return Pending for testing purpose.
    pub need_yield: bool,
    /// A parked waker.
    pub yield_waker: Option<std::task::Waker>,
}

impl Default for OperationState {
    fn default() -> Self {
        Self {
            need_yield: true,
            yield_waker: None,
        }
    }
}

pub struct MockSocket<P> {
    pub read_pipe: Arc<Mutex<P>>,
    pub write_pipe: Arc<Mutex<P>>,
    pub read_state: Arc<Mutex<OperationState>>,
    pub write_state: Arc<Mutex<OperationState>>,
}

impl AbstractReader for MockSocket<MockPipe> {
    fn read(&mut self, cx: Option<&mut Context<'_>>, buf: &mut [u8]) -> AbstractSocketResult {
        let mut state = self.read_state.lock().unwrap();
        if state.need_yield {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                state.yield_waker = Some(waker);
                return AbstractSocketResult::Retry;
            }
        }

        let mut pipe = self.read_pipe.lock().unwrap();
        let mut read = 0;
        if buf.is_empty() {
            return AbstractSocketResult::Ok(0);
        }
        while read < buf.len() {
            let Some(b) = pipe.buf.pop_front() else {
                break;
            };
            buf[read] = b;
            read += 1;
        }

        if read == 0 {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                pipe.data_waker = Some(waker);
            }
            // TODO(@xfding): this is correct for async sockets, but sync sockets should block until
            // arrival of new data.
            // We will need a condvar for this behaviour.
            return AbstractSocketResult::Retry;
        }
        // Reset for next operation
        state.need_yield = true;
        AbstractSocketResult::Ok(read)
    }
}

impl AbstractWriter for MockSocket<MockPipe> {
    fn write(&mut self, cx: Option<&mut Context<'_>>, buf: &[u8]) -> AbstractSocketResult {
        let mut state = self.write_state.lock().unwrap();
        if state.need_yield {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                state.yield_waker = Some(waker);
                return AbstractSocketResult::Retry;
            }
        }

        let mut pipe = self.write_pipe.lock().unwrap();
        pipe.buf.extend(buf.iter().copied());

        // Wake reader if they are waiting for data
        if let Some(waker) = pipe.data_waker.take() {
            waker.wake();
        }

        // Reset for next operation
        state.need_yield = true;

        AbstractSocketResult::Ok(buf.len())
    }

    fn flush(&mut self, _: Option<&mut Context<'_>>) -> AbstractSocketResult {
        AbstractSocketResult::Ok(0)
    }
}

impl AbstractSocket for MockSocket<MockPipe> {}

pub struct MockExecutor {
    pub states: Vec<Arc<Mutex<OperationState>>>,
}

impl MockExecutor {
    pub fn run<F: std::future::Future>(&mut self, f: F) -> F::Output {
        let waker = futures::task::noop_waker();
        let mut cx = Context::from_waker(&waker);
        let mut f = pin!(f);

        loop {
            match f.as_mut().poll(&mut cx) {
                std::task::Poll::Ready(res) => return res,
                std::task::Poll::Pending => {
                    // Resolve forced yields and reset flags
                    for state in &self.states {
                        let mut state = state.lock().unwrap();
                        if let Some(waker) = state.yield_waker.take() {
                            state.need_yield = false;
                            waker.wake();
                        }
                    }
                }
            }
        }
    }
}

pub fn create_mock_socket<P: Default>() -> (MockSocket<P>, MockSocket<P>, MockExecutor) {
    let pipe1 = Arc::new(Mutex::new(P::default()));
    let pipe2 = Arc::new(Mutex::new(P::default()));

    let socket1 = MockSocket {
        read_pipe: pipe1.clone(),
        write_pipe: pipe2.clone(),
        read_state: Arc::new(Mutex::new(OperationState::default())),
        write_state: Arc::new(Mutex::new(OperationState::default())),
    };

    let socket2 = MockSocket {
        read_pipe: pipe2,
        write_pipe: pipe1,
        read_state: Arc::new(Mutex::new(OperationState::default())),
        write_state: Arc::new(Mutex::new(OperationState::default())),
    };

    let executor = MockExecutor {
        states: vec![
            socket1.read_state.clone(),
            socket1.write_state.clone(),
            socket2.read_state.clone(),
            socket2.write_state.clone(),
        ],
    };

    (socket1, socket2, executor)
}

pub fn create_mock_pipe() -> (MockSocket<MockPipe>, MockSocket<MockPipe>, MockExecutor) {
    create_mock_socket::<MockPipe>()
}

/// A mock datagram transport that never shuts down.
#[derive(Default)]
pub struct MockDatagram {
    pub buf: VecDeque<Vec<u8>>,
    pub data_waker: Option<std::task::Waker>,
}

impl AbstractReader for MockSocket<MockDatagram> {
    fn read(&mut self, cx: Option<&mut Context<'_>>, buf: &mut [u8]) -> AbstractSocketResult {
        let mut state = self.read_state.lock().unwrap();
        if state.need_yield {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                state.yield_waker = Some(waker);
                return AbstractSocketResult::Retry;
            }
        }

        let mut pipe = self.read_pipe.lock().unwrap();
        let Some(packet) = pipe.buf.pop_front() else {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                pipe.data_waker = Some(waker);
            }
            return AbstractSocketResult::Retry;
        };

        let len = packet.len().min(buf.len());
        buf[..len].copy_from_slice(&packet[..len]);

        state.need_yield = true;

        AbstractSocketResult::Ok(len)
    }
}

impl AbstractWriter for MockSocket<MockDatagram> {
    fn write(&mut self, cx: Option<&mut Context<'_>>, buf: &[u8]) -> AbstractSocketResult {
        let mut state = self.write_state.lock().unwrap();
        if state.need_yield {
            if let Some(waker) = cx.as_ref().map(|c| c.waker().clone()) {
                state.yield_waker = Some(waker);
                return AbstractSocketResult::Retry;
            }
        }

        let mut pipe = self.write_pipe.lock().unwrap();
        pipe.buf.push_back(buf.to_vec());

        if let Some(waker) = pipe.data_waker.take() {
            waker.wake();
        }

        state.need_yield = true;

        AbstractSocketResult::Ok(buf.len())
    }

    fn flush(&mut self, _: Option<&mut Context<'_>>) -> AbstractSocketResult {
        AbstractSocketResult::Ok(0)
    }
}

impl AbstractSocket for MockSocket<MockDatagram> {}

pub(crate) fn create_mock_datagram() -> (
    MockSocket<MockDatagram>,
    MockSocket<MockDatagram>,
    MockExecutor,
) {
    create_mock_socket::<MockDatagram>()
}

#[test]
fn test_async() -> Result<(), Error> {
    let (mut server_conn, mut client_conn) = dumb_server_client()?;

    let (client_socket, server_socket, mut executor) = create_mock_pipe();

    server_conn.set_io(server_socket)?;
    client_conn.set_io(client_socket)?;

    let test_future = async move {
        let server_handshake = drive_handshake(&mut server_conn);
        let client_handshake = drive_handshake(&mut client_conn);
        join(server_handshake, client_handshake).await;

        let server_data = async move {
            let mut buf = [0u8; TEST_DATA.len()];
            let mut read_bytes = 0;
            while read_bytes < TEST_DATA.len() {
                match server_conn
                    .as_pin_mut()
                    .async_read(&mut buf[read_bytes..])
                    .await
                    .unwrap()
                {
                    IoStatus::Ok(n) => read_bytes += n,
                    IoStatus::EndOfStream => break,
                    _ => {}
                }
            }
            assert_eq!(&buf, TEST_DATA);
        };

        let client_data = async move {
            let _ = client_conn
                .as_pin_mut()
                .async_write(TEST_DATA)
                .await
                .unwrap();
        };

        join(server_data, client_data).await;
    };

    executor.run(test_future);

    Ok(())
}

pub async fn drive_handshake<R>(conn: &mut TlsConnection<R>) {
    assert!(
        conn.in_handshake()
            .unwrap()
            .async_handshake()
            .await
            .unwrap()
            .is_none()
    );
}

#[test]
fn test_async_shutdown() -> Result<(), Error> {
    use futures::future::FutureExt;

    let (mut server_conn, mut client_conn) = dumb_server_client()?;

    let (client_socket, server_socket, mut executor) = create_mock_pipe();

    server_conn.set_io(server_socket)?;
    client_conn.set_io(client_socket)?;

    let test_future = async move {
        let server_handshake = drive_handshake(&mut server_conn);
        let client_handshake = drive_handshake(&mut client_conn);

        join(server_handshake, client_handshake).await;

        let (send_signal, recv_signal) = futures::channel::oneshot::channel::<()>();

        let server_fut = async move {
            // The following shutdown call should not make progress.
            let res = server_conn.as_pin_mut().async_shutdown().now_or_never();
            assert!(res.is_none());

            // Make the client progress.
            send_signal.send(()).unwrap();

            server_conn.as_pin_mut().async_shutdown().await.unwrap();
        };

        let client_fut = async move {
            recv_signal.await.unwrap();
            match client_conn.as_pin_mut().async_shutdown().await {
                Ok(_) | Err(Error::Io(crate::errors::IoError::EndOfStream)) => {}
                Err(e) => panic!("{e:?}"),
            }
        };

        join(server_fut, client_fut).await;
    };

    executor.run(test_future);

    Ok(())
}
