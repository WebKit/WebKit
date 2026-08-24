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

use core::future::{
    Ready,
    ready, //
};

use bssl_crypto::ecdsa::ParsedPrivateKey;
use bssl_x509::keys::{
    PrivateKey,
    PrivateKeyAlgorithm, //
};
use futures::future::try_join;

use super::*;
use crate::{
    context::{
        TlsContextBuilder,
        TlsMode, //
    },
    errors::Error,
    tests::{
        P256_SERVER_KEY,
        P256_SERVER_KEY_DER,
        create_mock_pipe, //
    }, //
};

#[test]
fn parse_none() {
    let certs = Certificate::parse_all_from_pem(b"", None).unwrap();
    assert!(certs.is_empty());
}

#[test]
fn parse_one() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
";
    let certs = Certificate::parse_all_from_pem(PEM, None).unwrap();
    assert_eq!(certs.len(), 1);
    assert_eq!(certs[0].as_der_bytes(), b"Hello World!");
}

#[test]
fn parse_all_pems() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
BoringSSL is ...
-----BEGIN CERTIFICATE-----
QXdlc29tZSBCb3JpbmdTU0wh
-----END CERTIFICATE-----
Trailing bits ...
";
    let certs = Certificate::parse_all_from_pem(PEM, None).unwrap();
    assert_eq!(certs.len(), 2);
    assert_eq!(certs[0].as_der_bytes(), b"Hello World!");
    assert_eq!(certs[1].as_der_bytes(), b"Awesome BoringSSL!");
}

#[test]
fn parse_all_pems_fail() {
    const PEM: &'_ [u8] = b"
Hello world!
-----BEGIN CERTIFICATE-----
SGVsbG8gV29ybGQh
-----END CERTIFICATE-----
BoringSSL is ...
-----BEGIN CERTIFICATE-----
QXdlc29tZSBCb3JpbmdTU0wh
-----END CERTIFICATE-----
But something badddd...
-----BEGIN CERTIFICATE-----
";
    let _ = Certificate::parse_all_from_pem(PEM, None).unwrap_err();
}

#[test]
fn parse_private_key() {
    const PEM: &'_ [u8] = b"-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIJtTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQyZj/WEBsVe3FKYbT
c423LgICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEFs8Xuche6vD6u85
2LwsNGEEgglQOzaEdw7c/mwKrIDdQESZWEDiNjBPRYGmNZuhilm3xHTBs1WcqC2S
BkbIlxEHOVdOfes6A0wLH0v/Pv6J/qrazTAQeg5jAfTQILip2grygHTYU/4Jhezs
Z2mk56TQ6oayohX4/B259IYVM2V6Us62J1+cZMFZ51erDrVn6c1RP7b1tw6AzmbM
NTMHgj54WCENqFcI3q/9JSvm36MU1OCDi9gJ76uJIQ5gyjmGrp7J8BwkicW616Jo
u+f3JKETR9BRPTICdbvQBev6Tc/fDyueLKsQsR3cxsFqydadmmQjIFjNplVmpWI+
3veZj3pUQtmPsXzD7cXx66ygbRNEEEeWaXwl+RJkO2IqPBVF/nCXWNsbPFvxbBqJ
tlGi0wQcMPfuuJwdEFX9+ZBHoed4XSd/FWdOaYHN/86nqtggECfNpTISn8RPnkDm
Xp3IHoVPx8VQPZodvdqkET6i0QGTeZol9+D3Doa+VUnv6+IF+D3TDnTmGcBuqT3e
Tm50LtrKAexXGjdYmTMB3cu/QS963yOwvsUVXifutcS7UX9fpDAbtf2UUYln5l5H
LqlwbTRYrIEYAO2WylJ3KSw/7aQMlxHLGygpoDPtdURsLXyNcpLml7L203EHixX8
uK7DomFqg9/mR13rKKpnLJGy4f68UYA02RwqHRVTTg7wuTBAX5XF/vL+A5MCklsL
igDvCGLNN3F6s5oGfxoBNznGrXKkIP5+BagAkZrq9vmX2iFsueU+kBzHjytf9h93
QWoLi/dkDXiBPyzGPiUvi1AGgQylv2pRR0fg3D5F/U207JsMvyg9YZHe1ZbyEueG
TQCObkSA1+H+mbWTg8gMdQRWnfGQZ0F0Jet5cFh8NcU6tM5fs/PUxNi9QaHeT5TL
BpXaaq0d1UYGap06qp977QquBTwTufAKoTtnmxFm0CPn/lckUq2VJc0hHjZqGgSf
IIIUpxoehCQw81d7tGRIRnneM4AsL7FHBx4WDviFvldU5HPmbj/EI8gOsbF72eMq
NbZ7YP8VD0JZ5uQfO4wcLEWOv6TlXzE/UNXBPBQSMH2Uoj1I2WO6i6HunNe0HXN2
2Qea9nhnWuDEcEkDds14UxxNnp2FylLhm+RWNYsixk5Dky+BMf5eUJfT+Tkks42N
SAnKuPeRGlO5m6HGpfwQEthC8hlvxPx2fW+NHJ3VojTamdNB5nFe2y3be0KF1v17
pHtJXQPABPKDlW3Z1WHqkI006EZVUOpop6LYxEjQ4UJ574y9xykiUEhyV+KeC1Uo
1ZH9TiJj0PReLzDx+19nLGW9GvjQwylXX96gmC/OPbzHDPHLj5WyvZNZ14uVKC4s
FmHthA4GJORVbkzBZxgPs8nrQvTgwkuJ1t9DPknz9q7m6HyRee0jTE/Q9nAu1Ecx
F0rTOTu7xf6sK68+a/obPNlWW565aKaICDkaTo84wRuutF2KNAI/xaSoZibALCAJ
foPfTHHWNFpQmKe+PQwi6hmSUklkboMxg5+eiCpD/ke3315lOvnr2oKdpBOPN3iT
kAzrV1xnEJvXIv3O+zmexoSbQ3zmN3ExubUE1g5cia25Yop2kHVTbbjqFx7cSMSD
zAeSoDrDEJY/pZn/+wPLdBz9rp+vh/3rDa2q7Uj94c8jSQivKCvVQaeB8JuLpMZM
yavSF+iAk8cQw5h0ZfyxNsnSC8eI8sqQjNEixOroUDvjfCEyF6W6edm//vOnMv1a
iDIvMVigXBP46cTgxhK1RsgfdadUmK6gDlAdwIlKlW3xUPVD7K9WXu0A2lK8OA7+
WVuQildnQiQ8eKYiAWbKFP9/E2vKfz+sJ8w35oFAFVR6q+KQWlx7YpTv8XeeIM4y
HcmP/I2oBikATfFpQu1i9cfkBmWsv08MPQAa435iBPiBjBov6hnqdppZES6qTc3D
G3pVAAGLB9JZdqs3bAOZs40aJxnR7v6FNFz/fhMDSkpzJDVYgNFz0a4S5cXNo7Ju
lyzyUL/WMTwgEV1vVqudXDA+XBjsARDPO1MIuwPejSM8RomeFdlIJ1vTm592ixNt
Ll3v1nK0jsJ1XXlFPhdQwsvie6kth/Z5RVUl9o/uhjv92T1Lrh5T4Datr7bhKbSZ
cLyhfFgEE08rId1Je9SsXuhrxwTjOXoBh+tCUCD1l/lcctWsRzQkgtf8tx/p/6Rl
DJnCMvJKJgFgTMMy3dG4PapxOHJ9ouLwNwYITWs1GEYKm+SJ3odIuh6msgra0E4l
C/XhGhdTZt/EUp3rz25x9aJ50iIGx0JeWfi2sBJaTjb2Dmo/rQ8Z4Jp4Kn9Sig1I
WVeI8oDXEvbG1rs4o2+JD9LhYybSxYZctr420+wxMPDJh7bIal5+XBT8ZNAZRO5t
bPE529NMkg1ZprqJLBI0l88Ex4r6d07MqX4mp6geYmMjkUC+UOfTxwMVQB/013MB
WkgRd7nb7td5PSBF9B7ff4yU4PzPmfE46hEMleGmUYnjUm/aEUG/oUqX0aQ6rd2f
QRgqcKMS3HiDfN9WDokvjuGry65+fb+XAcvDcDdPW4z6aOtOM0ld83CbniIb39Sg
vwbw9eVVz1snfkPa0kpaJyGea5ZI+/B2Gpa8NlFbCeZ33wvzWIAxgsunCTz3ueQm
CZAttGE8lGGOcgTRvpiS7Zu3M6/54rbnlkeSu3CKwYKCjsu0x+ZVHom3Ax5q3suG
hMtVqVLQJvbNgxvHMojSoxDffbY/XPHOERbkVz2h3fttIc2zFvx2zbScqPi12X4T
WpmnsjzOdcrr5FTJ4tEKr8KaHCETfJjF3s6Z4s783C286tjMOgNF1HSG5yqYq9Fb
NAksrCPMAJkEhWKrEETPd0DWS0zYD+wGhQYZApfZjyas6Eg3kSgNPFIS9kBs4lKM
mg77d/xMWaw1wjY34dCXMhPgJ+rlWBEa4G+yndu7oD8MaSGRd8/ZZF+B+yZv7jg8
E/RWaavKuZGjHl6VLqS/sf2s9Uaea3dnODof9c/APqfDpmzTzc+PRfoEloHofo8g
aU/TeEx333VC493Dzj00c3WobYWU/w3EPgutlGUbi/W0NkA7h/98NxntMpoRy2jr
cPzbQnwdjykFQ1JXTcQnqzdbSlrTEXtArhIggG98rvJEZ55LXaVq/tTp4F89VR/W
lTU7GxRvRinKa52GnUNLqxkmTTcFegGMevICfN7JUaUTDiEQGGJ6jNw=
-----END ENCRYPTED PRIVATE KEY-----
";
    let priv_key = PrivateKey::from_pem(PEM, || b"Hello BoringSSL!").unwrap();
    assert!(matches!(
        priv_key.algorithm(),
        Some(PrivateKeyAlgorithm::Rsa)
    ));
}

#[test]
fn psk_tls13_handshake() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let key = b"test-key-test-key-test-key-test-key";
    let identity = b"test-identity";
    let context = b"test-context";

    let cred = TlsCredential::new_pre_shared_key(key, identity, PskHash::Sha256, context)?;

    let mut server_ctx = TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred.clone())?;

    let mut client_ctx = TlsContextBuilder::new_tls();
    client_ctx.with_credential(cred)?;

    let server_ctx = server_ctx.build();
    let client_ctx = client_ctx.build();

    let (client_socket, server_socket, mut executor) = create_mock_pipe();

    let mut client_conn = client_ctx.new_client_connection().build();
    let mut server_conn = server_ctx.new_server_connection().build();

    client_conn.set_io(client_socket)?;
    server_conn.set_io(server_socket)?;

    let test_future = async {
        let client_handshake = async {
            assert!(client_conn.async_handshake().await?.is_none());
            Ok::<(), Error>(())
        };

        let server_handshake = async {
            assert!(server_conn.async_handshake().await?.is_none());
            Ok::<(), Error>(())
        };

        try_join(client_handshake, server_handshake).await?;
        Ok::<(), Error>(())
    };

    executor.run(test_future)?;

    Ok(())
}

#[cfg(all(unix, feature = "std"))]
#[test]
fn psk_tls13_handshake_sync() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use crate::io::sync_io::{NoAsync, StdIoWithReactor};
    use std::io::pipe;

    let (server_rx, client_tx) = pipe().unwrap();
    let (client_rx, server_tx) = pipe().unwrap();

    let client_reader = StdIoWithReactor::new(client_rx, NoAsync);
    let client_writer = StdIoWithReactor::new(client_tx, NoAsync);
    let server_reader = StdIoWithReactor::new(server_rx, NoAsync);
    let server_writer = StdIoWithReactor::new(server_tx, NoAsync);

    let key = b"test-key-test-key-test-key-test-key";
    let identity = b"test-identity";
    let context = b"test-context";

    let cred = TlsCredential::new_pre_shared_key(key, identity, PskHash::Sha256, context)?;

    let mut server_ctx = TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred.clone())?;
    let server_ctx = server_ctx.build();

    let mut client_ctx = TlsContextBuilder::new_tls();
    client_ctx.with_credential(cred)?;
    let client_ctx = client_ctx.build();

    let mut client_conn = client_ctx.new_client_connection().build();
    let mut server_conn = server_ctx.new_server_connection().build();

    client_conn.set_split_io(client_reader, client_writer)?;
    server_conn.set_split_io(server_reader, server_writer)?;

    let server_thread = std::thread::spawn(move || {
        server_conn.do_handshake().unwrap();
        server_conn
    });

    client_conn.do_handshake().unwrap();

    let _server_conn = server_thread.join().unwrap();

    Ok(())
}

/// Helper: perform an RPK handshake with the given server credential and client verifier.
fn rpk_handshake_test(
    cred: TlsCredential,
    verifier: impl VerifyCertificate + 'static,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let mut server_ctx = TlsContextBuilder::new_tls();
    server_ctx.with_credential(cred)?;
    let server_ctx = server_ctx.build();

    let mut client_ctx = TlsContextBuilder::new_tls();
    client_ctx
        .with_accepted_peer_cert_types(&[CertificateType::Rpk])?
        .with_certificate_verifier(CertificateVerificationMode::PeerCertMandatory, verifier);
    let client_ctx = client_ctx.build();

    let mut client_conn = client_ctx.new_client_connection().build();
    let mut server_conn = server_ctx.new_server_connection().build();

    let (sock_client, sock_server, mut executor) = create_mock_pipe();

    client_conn.set_io(sock_client)?;
    server_conn.set_io(sock_server)?;

    executor.run(try_join(
        Pin::new(&mut client_conn).async_write(b"hello"),
        Pin::new(&mut server_conn).async_write(b"world"),
    ))?;

    assert_eq!(
        client_conn.get_peer_certificate_type(),
        Some(CertificateType::Rpk)
    );

    Ok(())
}

#[test]
fn rpk_tls13_handshake() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let priv_key = PrivateKey::from_pem(crate::tests::RSA_SERVER_KEY, || unreachable!()).unwrap();

    let cred = TlsCredentialBuilder::new_raw_public_key(priv_key)
        .build()
        .unwrap();

    rpk_handshake_test(cred, AcceptAnyVerifier)
}

#[test]
fn rpk_tls13_handshake_with_delegate() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use crate::credentials::{
        AsyncPrivateKeyDelegate, AsyncPrivateKeyDelegateAdapter, SignatureAlgorithm,
    };

    let priv_key = PrivateKey::from_pem(P256_SERVER_KEY, || unreachable!()).unwrap();
    let pub_key = priv_key.to_public_key().clone();
    let expected_spki_der = pub_key.to_der();

    struct P256KeyDelegate {
        key_der: &'static [u8],
    }

    impl AsyncPrivateKeyDelegate for P256KeyDelegate {
        type SignOp = Ready<Option<Vec<u8>>>;
        type DecryptOp = Ready<Option<Vec<u8>>>;

        fn sign(&self, message: &[u8], algorithm: SignatureAlgorithm) -> Self::SignOp {
            assert!(matches!(
                algorithm,
                SignatureAlgorithm::EcdsaSecp256r1Sha256
            ));
            let Some(ParsedPrivateKey::P256(key)) = ParsedPrivateKey::from_der(self.key_der) else {
                panic!("failed to parse P256 key")
            };
            ready(Some(key.sign(message)))
        }

        fn decrypt(&self, _ciphertext: &[u8]) -> Self::DecryptOp {
            unreachable!()
        }
    }

    let delegate = AsyncPrivateKeyDelegateAdapter(P256KeyDelegate {
        key_der: P256_SERVER_KEY_DER,
    });
    let cred = TlsCredentialBuilder::new_raw_public_key_with_delegate(pub_key, delegate)
        .build()
        .unwrap();

    rpk_handshake_test(cred, RpkVerifier { expected_spki_der })
}

struct AcceptAnyVerifier;

impl VerifyCertificate for AcceptAnyVerifier {
    fn verify<'a>(
        &self,
        _: &'a VerifyCertificateContext,
        _: crate::credentials::CertificateChainIterator<'a>,
    ) -> Box<dyn VerifyCertificateTask> {
        Box::new(crate::credentials::VerifyCertificateAccepted)
    }
}

struct RpkVerifier {
    expected_spki_der: Vec<u8>,
}

impl VerifyCertificate for RpkVerifier {
    fn verify<'a>(
        &self,
        ctx: &'a VerifyCertificateContext,
        _: crate::credentials::CertificateChainIterator<'a>,
    ) -> Box<dyn VerifyCertificateTask> {
        let Some(der) = ctx.get_peer_raw_public_key() else {
            return Box::new(VerifyCertificateRejected(None));
        };
        if der != self.expected_spki_der {
            return Box::new(VerifyCertificateRejected(None));
        }

        Box::new(crate::credentials::VerifyCertificateAccepted)
    }
}

#[test]
fn psk_rpk_fallback_test() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    use std::sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    };

    use bssl_x509::keys::PrivateKey;

    use crate::{
        context::TlsContextBuilder,
        credentials::{
            CertificateType, CertificateVerificationMode, PskHash, RawPublicKeyMode, TlsCredential,
            TlsCredentialBuilder,
            early_callback::{ClientHello, EarlyCallback, EarlyCallbackResult, ExtensionType},
        },
        io::IoStatus,
    };

    let priv_key = PrivateKey::from_pem(crate::tests::RSA_SERVER_KEY, || unreachable!())?;

    let rpk_cred = TlsCredentialBuilder::<RawPublicKeyMode>::new_raw_public_key(priv_key.clone())
        .build()
        .ok_or("raw public key failed to parse".to_string())?;

    let server_certs =
        crate::credentials::Certificate::parse_all_from_pem(crate::tests::RSA_SERVER_CERT, None)?;
    let mut server_cred_builder = TlsCredentialBuilder::<crate::credentials::X509Mode>::new();
    server_cred_builder
        .with_certificate_chain(&server_certs)?
        .with_private_key(priv_key.clone())?;
    let server_cred = server_cred_builder
        .build()
        .ok_or("credential is incomplete".to_string())?;

    let x509_cert = bssl_x509::certificates::X509Certificate::parse_one_from_pem(
        crate::tests::RSA_SERVER_CERT,
    )?;
    let expected_rpk_der = x509_cert
        .public_key()
        .ok_or("public key is missing in x509".to_string())?
        .to_der();

    let key = b"test-key-test-key-test-key-test-key";
    let identity = b"test-identity";
    let context = b"test-context";
    let psk_cred = TlsCredential::new_pre_shared_key(key, identity, PskHash::Sha256, context)?;

    type AtomicFlag = Arc<AtomicBool>;
    struct ServerSelectCallback {
        server_cred: TlsCredential,
        psk_cred: TlsCredential,
        selected_rpk: AtomicFlag,
        selected_psk: AtomicFlag,
        failed: AtomicFlag,
    }

    impl EarlyCallback<TlsMode> for ServerSelectCallback {
        fn process(&self, client_hello: &mut ClientHello<'_, TlsMode>) -> EarlyCallbackResult {
            let offered_rpk = client_hello
                .get_extension(ExtensionType::CLIENT_CERT_TYPE)
                .is_some();
            let offered_psk = client_hello
                .get_extension(ExtensionType::PRE_SHARED_KEY)
                .is_some();

            if offered_rpk {
                // TODO: switch to `try` block on stabilisation
                if (|| {
                    client_hello
                        .connection_mut()
                        .add_credential(&self.server_cred)?
                        .set_certificate_verification_mode(
                            CertificateVerificationMode::PeerCertMandatory,
                        );
                    Ok::<_, Error>(())
                })()
                .is_err()
                {
                    return EarlyCallbackResult::ErrorResult;
                };
                self.selected_rpk.store(true, Ordering::SeqCst);
                EarlyCallbackResult::Success
            } else if offered_psk {
                let Ok(_) = client_hello.connection_mut().add_credential(&self.psk_cred) else {
                    return EarlyCallbackResult::ErrorResult;
                };
                self.selected_psk.store(true, Ordering::SeqCst);
                EarlyCallbackResult::Success
            } else {
                self.failed.store(true, Ordering::SeqCst);
                EarlyCallbackResult::ErrorResult
            }
        }
    }

    for &(offer_psk, offer_rpk) in &[(true, false), (false, true), (true, true), (false, false)] {
        let selected_rpk = AtomicFlag::default();
        let selected_psk = AtomicFlag::default();
        let failed = AtomicFlag::default();

        let cb = ServerSelectCallback {
            server_cred: server_cred.clone(),
            psk_cred: psk_cred.clone(),
            selected_rpk: selected_rpk.clone(),
            selected_psk: selected_psk.clone(),
            failed: failed.clone(),
        };

        let mut server_ctx_builder = TlsContextBuilder::new_tls();
        server_ctx_builder
            .with_accepted_peer_cert_types(&[CertificateType::Rpk])?
            .with_early_callback(cb)
            .with_certificate_verifier(
                CertificateVerificationMode::None,
                RpkVerifier {
                    expected_spki_der: expected_rpk_der.clone(),
                },
            );
        let server_ctx = server_ctx_builder.build();

        let mut client_ctx_builder = TlsContextBuilder::new_tls();
        if offer_psk {
            client_ctx_builder.with_credential(psk_cred.clone())?;
        }
        if offer_rpk {
            client_ctx_builder
                .with_credential(rpk_cred.clone())?
                .with_available_client_cert_types(&[CertificateType::Rpk])?;
        }
        let client_ctx = client_ctx_builder.build();

        let (sock_client, sock_server, mut executor) = create_mock_pipe();

        let mut server_conn = server_ctx.new_server_connection().build();
        server_conn.set_io(sock_server)?;

        let mut client_conn_builder = client_ctx.new_client_connection();
        client_conn_builder
            .with_certificate_verification_mode(CertificateVerificationMode::None)
            .with_certificate_verifier(
                CertificateVerificationMode::PeerCertRequested,
                AcceptAnyVerifier,
            );
        let mut client_conn = client_conn_builder.build();
        client_conn.set_io(sock_client)?;

        let test_future = async {
            let client_handshake = client_conn.async_handshake();

            let server_handshake = server_conn.async_handshake();

            let handshake_result =
                futures::future::try_join(client_handshake, server_handshake).await;

            if !offer_psk && !offer_rpk {
                assert!(handshake_result.is_err(), "Handshake should have failed");
                return Ok::<(), Error>(());
            }

            handshake_result?;

            client_conn.as_pin_mut().async_write(b"hello").await?;

            let mut server_buf = [0u8; 5];
            let read_len = server_conn.as_pin_mut().async_read(&mut server_buf).await?;
            assert!(matches!(read_len, IoStatus::Ok(5)));
            assert_eq!(&server_buf, b"hello");

            server_conn.as_pin_mut().async_write(b"world").await?;

            let mut client_buf = [0u8; 5];
            let read_len = client_conn.as_pin_mut().async_read(&mut client_buf).await?;
            assert!(matches!(read_len, IoStatus::Ok(5)));
            assert_eq!(&client_buf, b"world");

            Ok::<(), Error>(())
        };

        executor.run(test_future)?;

        if !offer_psk && !offer_rpk {
            assert!(
                failed.load(Ordering::SeqCst),
                "Should fail if no valid credentials offered"
            );
            continue;
        }

        if offer_rpk {
            assert!(
                selected_rpk.load(Ordering::SeqCst),
                "Should select RPK if offered"
            );
            assert_eq!(
                server_conn.get_peer_certificate_type(),
                Some(CertificateType::Rpk)
            );
        } else if offer_psk {
            assert!(
                selected_psk.load(Ordering::SeqCst),
                "Should fallback to PSK if only PSK offered"
            );
        }
    }

    Ok(())
}
