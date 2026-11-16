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

#![cfg(feature = "hyper")]

use bssl_tls::alpn::H2;
use bssl_tls::context::TlsContextBuilder;
use bssl_tls::credentials::{Certificate, TlsCredential, TlsCredentialBuilder};
use bssl_tls_tokio::TokioTlsExt;
use bssl_tls_tokio::hyper::HyperBsslConnector;
use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::{X509Store, X509StoreBuilder},
};
use hyper::body::{Body, Bytes, Frame};
use hyper::service::service_fn;
use hyper_util::rt::TokioIo as HyperTokioIo;
use std::convert::Infallible;
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};
use tokio::net::{TcpListener, TcpStream};
use tower::Service;

const CA: &[u8] = include_bytes!("../../test-data/BoringSSLCATest.crt");
const RSA_SERVER_CERT: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.crt");
const RSA_SERVER_KEY: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.key");

fn server_credential() -> TlsCredential {
    let ca = Certificate::parse_one_from_pem(CA, None).unwrap();
    let server_cert = Certificate::parse_one_from_pem(RSA_SERVER_CERT, None).unwrap();
    let server_key = PrivateKey::from_pem(RSA_SERVER_KEY, || unreachable!()).unwrap();
    let mut builder = TlsCredentialBuilder::new();
    builder
        .with_certificate_chain(&[server_cert, ca])
        .unwrap()
        .with_private_key(server_key)
        .unwrap();
    builder.build().unwrap()
}

fn client_cert_store() -> X509Store {
    let mut store = X509StoreBuilder::new();
    store
        .set_trust(Trust::SslServer)
        .unwrap()
        .add_cert(X509Certificate::parse_one_from_pem(CA).unwrap())
        .unwrap();
    store.build()
}

/// A body that yields a single data frame, or is empty.
struct SimpleBody(Option<Bytes>);

impl SimpleBody {
    fn new(data: &'static str) -> Self {
        Self(Some(Bytes::from(data)))
    }

    fn empty() -> Self {
        Self(None)
    }
}

impl Body for SimpleBody {
    type Data = Bytes;
    type Error = Infallible;

    fn poll_frame(
        mut self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Self::Data>, Self::Error>>> {
        match self.0.take() {
            Some(data) => Poll::Ready(Some(Ok(Frame::data(data)))),
            None => Poll::Ready(None),
        }
    }
}

/// A mock TCP connector that connects to a fixed address, implementing
/// `tower::Service<Uri>` so it can be wrapped in [`HyperBsslConnector`].
struct MockTcpConnector {
    addr: std::net::SocketAddr,
}

impl Service<hyper::http::Uri> for MockTcpConnector {
    type Response = HyperTokioIo<TcpStream>;
    type Error = std::io::Error;
    type Future = Pin<Box<dyn Future<Output = Result<Self::Response, Self::Error>> + Send>>;

    fn poll_ready(&mut self, _: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        Poll::Ready(Ok(()))
    }

    fn call(&mut self, _: hyper::http::Uri) -> Self::Future {
        let addr = self.addr;
        Box::pin(async move {
            let stream = TcpStream::connect(addr).await?;
            Ok(HyperTokioIo::new(stream))
        })
    }
}

/// Sends an HTTP/2 request over TLS using `HyperBsslConnector` and verifies
/// that a hyper HTTP/2 server receives and responds correctly.
#[tokio::test]
async fn test_hyper_h2_roundtrip() {
    // Bind to an ephemeral port.
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();

    // Set up the TLS server context.
    let mut server_ctx_builder = TlsContextBuilder::new_tls();
    server_ctx_builder
        .with_credential(server_credential())
        .unwrap();
    server_ctx_builder.set_alpn_protocols([H2]).unwrap();
    let acceptor = server_ctx_builder.build_tokio_acceptor();

    // Server: accept a connection, do TLS, then serve one HTTP/2 request.
    let server_fut = async move {
        let (stream, _) = listener.accept().await.unwrap();
        let tls_stream = acceptor.accept(stream).await.unwrap();

        hyper::server::conn::http2::Builder::new(hyper_util::rt::TokioExecutor::new())
            .serve_connection(
                tls_stream,
                service_fn(|_req| async {
                    Ok::<_, hyper::Error>(hyper::Response::new(SimpleBody::new(
                        "hello from h2 server",
                    )))
                }),
            )
            .await
            .unwrap();
    };

    // Client: use HyperBsslConnector to establish TLS, then do HTTP/2.
    let client_fut = async move {
        let mock_connector = MockTcpConnector { addr };

        let mut client_ctx_builder = TlsContextBuilder::new_tls();
        client_ctx_builder.with_certificate_store(&client_cert_store());
        client_ctx_builder.set_alpn_protocols([H2]).unwrap();
        let connector = client_ctx_builder.build_tokio_connector();

        let mut hyper_connector = HyperBsslConnector::new(mock_connector, connector);
        let tls_stream = hyper_connector
            .call("https://localhost/".parse().unwrap())
            .await
            .unwrap();

        let (mut sender, conn) =
            hyper::client::conn::http2::handshake(hyper_util::rt::TokioExecutor::new(), tls_stream)
                .await
                .unwrap();

        // Drive the connection in the background.
        tokio::spawn(async move {
            conn.await.unwrap();
        });

        let req = hyper::Request::get("/").body(SimpleBody::empty()).unwrap();
        let resp = sender.send_request(req).await.unwrap();
        assert_eq!(resp.status(), 200);

        // Read the response body frame by frame.
        let body = resp.into_body();
        let mut body = std::pin::pin!(body);
        let mut result = Vec::new();
        while let Some(frame) = futures::future::poll_fn(|cx| body.as_mut().poll_frame(cx)).await {
            if let Ok(frame) = frame {
                if let Some(data) = frame.data_ref() {
                    result.extend_from_slice(data);
                }
            }
        }
        assert_eq!(result, b"hello from h2 server");
    };

    tokio::join!(server_fut, client_fut);
}
