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

use bssl_tls::context::TlsContextBuilder;

use tokio::io::{
    AsyncReadExt,
    AsyncWriteExt, //
};

use crate::TokioTlsExt;

#[tokio::test]
async fn high_level_tokio() -> Result<(), bssl_tls::errors::Error> {
    let mut server_builder = TlsContextBuilder::new_tls();
    server_builder.with_credential(super::server_credential())?;
    let acceptor = server_builder.build_tokio_acceptor();

    let mut client_builder = TlsContextBuilder::new_tls();
    client_builder.with_certificate_store(&super::client_cert_store());
    let connector = client_builder.build_tokio_connector();

    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();

    let server_task = tokio::spawn(async move {
        let (stream, _) = listener.accept().await.unwrap();
        let mut tls_stream = acceptor.accept(stream).await.unwrap();

        let mut buf = [0; 5];
        tls_stream.read_exact(&mut buf).await.unwrap();
        assert_eq!(&buf, b"hello");

        tls_stream.write_all(b"world").await.unwrap();
        tls_stream.flush().await.unwrap();
    });

    let stream = tokio::net::TcpStream::connect(addr).await.unwrap();
    let mut tls_stream = connector.connect("www.google.com", stream).await.unwrap();

    tls_stream.write_all(b"hello").await.unwrap();
    tls_stream.flush().await.unwrap();

    let mut buf = [0; 5];
    tls_stream.read_exact(&mut buf).await.unwrap();
    assert_eq!(&buf, b"world");

    server_task.await.unwrap();

    Ok(())
}
