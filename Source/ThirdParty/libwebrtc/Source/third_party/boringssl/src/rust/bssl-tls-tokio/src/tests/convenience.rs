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
    context::TlsContextBuilder,
    credentials::{
        Certificate,
        TlsCredentialBuilder, //
    }, //
};
use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::X509StoreBuilder, //
};
use tokio::io::{
    AsyncReadExt,
    AsyncWriteExt, //
};

use super::{
    CA,
    RSA_SERVER_CERT,
    RSA_SERVER_KEY, //
};
use crate::TokioTlsExt;

#[tokio::test]
async fn high_level_tokio() -> Result<(), bssl_tls::errors::Error> {
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
    let connector = builder.build_tokio_connector();
    let acceptor = server_ctx_builder.build_tokio_acceptor();

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
