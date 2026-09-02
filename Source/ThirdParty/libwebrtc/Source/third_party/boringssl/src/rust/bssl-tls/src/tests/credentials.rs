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
    future::Future,
    pin::Pin,
    sync::{
        Arc,
        Mutex, //
    },
    task::{
        Context,
        Poll, //
    },
};

use bssl_crypto::ecdsa::ParsedPrivateKey;
use bssl_x509::{
    certificates::X509Certificate,
    params::Trust,
    store::X509StoreBuilder, //
};
use futures::channel::oneshot;

use super::{
    CA,
    P256_SERVER_CERT, //
};
use crate::{
    context::TlsContextBuilder,
    credentials::{
        AsyncPrivateKeyDelegate,
        Certificate,
        CertificateVerificationMode,
        SignatureAlgorithm,
        TlsCredentialBuilder, //
    },
    errors::TlsRetryReason,
    io::IoStatus,
    tests::create_mock_pipe, //
};

#[test]
fn test_private_key_methods() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let ca = Certificate::parse_one_from_pem(CA, None)?;
    let server_cert = Certificate::parse_one_from_pem(P256_SERVER_CERT, None)?;

    let (client_to_server_tx, client_to_server_rx) = oneshot::channel::<()>();
    let (server_to_client_tx, server_to_client_rx) = oneshot::channel::<()>();

    let private_key_method = MyPrivateKeyMethod {
        key: crate::tests::P256_SERVER_KEY_DER,
        client_to_server_rx: Arc::new(Mutex::new(Some(client_to_server_rx))),
        server_to_client_tx: Arc::new(Mutex::new(Some(server_to_client_tx))),
    };

    struct MyPrivateKeyMethod {
        key: &'static [u8],
        client_to_server_rx: Arc<Mutex<Option<oneshot::Receiver<()>>>>,
        server_to_client_tx: Arc<Mutex<Option<oneshot::Sender<()>>>>,
    }

    impl AsyncPrivateKeyDelegate for MyPrivateKeyMethod {
        type DecryptOp = Pin<Box<dyn Send + Sync + Future<Output = Option<Vec<u8>>>>>;
        type SignOp = Pin<Box<dyn Send + Sync + Future<Output = Option<Vec<u8>>>>>;
        fn sign(&self, message: &[u8], algorithm: SignatureAlgorithm) -> Self::SignOp {
            let message = message.to_vec();
            let Some(ParsedPrivateKey::P256(key)) = ParsedPrivateKey::from_der(self.key) else {
                panic!()
            };
            let client_to_server_rx = self.client_to_server_rx.clone();
            let server_to_client_tx = self.server_to_client_tx.clone();
            Box::pin(async move {
                let tx = server_to_client_tx.lock().unwrap().take();
                if let Some(tx) = tx {
                    tx.send(()).unwrap();
                }

                let rx = client_to_server_rx.lock().unwrap().take();
                if let Some(rx) = rx {
                    rx.await.unwrap();
                }

                assert!(matches!(
                    algorithm,
                    SignatureAlgorithm::EcdsaSecp256r1Sha256
                ));
                Some(key.sign(&message))
            })
        }

        fn decrypt(&self, _: &[u8]) -> Self::DecryptOp {
            unreachable!()
        }
    }

    let mut server_ctx_builder = TlsContextBuilder::new_tls();
    let server_cred = {
        let mut builder = TlsCredentialBuilder::new();
        builder
            .with_certificate_chain(&[server_cert, ca])?
            .with_private_key_delegate(Some(crate::credentials::AsyncPrivateKeyDelegateAdapter(
                private_key_method,
            )));
        builder.build().unwrap()
    };
    server_ctx_builder.with_credential(server_cred)?;
    let server_ctx = server_ctx_builder.build();
    let mut server_conn = server_ctx.new_server_connection().build();

    let mut client_ctx_builder = TlsContextBuilder::new_tls();
    let mut cert_store = X509StoreBuilder::new();
    cert_store
        .set_trust(Trust::SslServer)?
        .add_cert(X509Certificate::parse_one_from_pem(CA)?)?;
    let cert_store = cert_store.build();
    client_ctx_builder.with_certificate_store(&cert_store);
    let client_ctx = client_ctx_builder.build();
    let mut client_conn = client_ctx.new_client_connection();
    client_conn.with_certificate_verification_mode(CertificateVerificationMode::PeerCertMandatory);
    let mut client_conn = client_conn.build();
    client_conn
        .in_handshake()
        .unwrap()
        .set_host("www.google.com")?;

    let (client_socket, server_socket, mut executor) = create_mock_pipe();
    client_conn.set_io(client_socket)?;
    server_conn.set_io(server_socket)?;

    let mut server_task = async move || -> Result<(), crate::errors::Error> {
        loop {
            match server_conn.async_handshake().await {
                Ok(None) => break,
                Ok(Some(TlsRetryReason::PendingPrivateKeyOperation)) => {
                    struct Yield(bool);
                    impl std::future::Future for Yield {
                        type Output = ();
                        fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
                            if self.0 {
                                return Poll::Ready(());
                            }
                            self.0 = true;
                            cx.waker().wake_by_ref();
                            Poll::Pending
                        }
                    }
                    Yield(false).await;
                }
                res => panic!("Unexpected handshake result: {:?}", res),
            }
        }

        let mut message = [0; 21];
        assert!(matches!(
            server_conn.as_pin_mut().async_read(&mut message).await?,
            IoStatus::Ok(21)
        ));
        assert_eq!(message, *b"BoringSSL is awesome!");
        server_conn.as_pin_mut().async_shutdown().await?;
        Ok(())
    };

    let client_task = async move || -> Result<(), crate::errors::Error> {
        let res = client_conn.do_handshake();
        assert!(
            matches!(res, Ok(Some(TlsRetryReason::WantRead))),
            "Expected WantRead, got {:?}",
            res
        );

        server_to_client_rx.await.unwrap();
        client_to_server_tx.send(()).unwrap();

        client_conn
            .as_pin_mut()
            .async_write(b"BoringSSL is awesome!")
            .await?;

        client_conn.as_pin_mut().async_shutdown().await?;
        Ok(())
    };

    let test_closure = async move || -> Result<(), crate::errors::Error> {
        futures::future::try_join(server_task(), client_task()).await?;
        Ok(())
    };

    executor.run(test_closure())?;

    Ok(())
}
