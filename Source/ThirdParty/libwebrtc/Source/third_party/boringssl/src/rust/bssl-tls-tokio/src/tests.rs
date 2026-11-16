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

mod convenience;
mod datagram;
mod transport;

const CA: &[u8] = include_bytes!("../../test-data/BoringSSLCATest.crt");
const RSA_SERVER_CERT: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.crt");
const RSA_SERVER_KEY: &[u8] = include_bytes!("../../test-data/BoringSSLServerTest-RSA.key");

use bssl_tls::credentials::{
    Certificate,
    TlsCredential,
    TlsCredentialBuilder, //
};
use bssl_x509::{
    certificates::X509Certificate,
    keys::PrivateKey,
    params::Trust,
    store::{X509Store, X509StoreBuilder}, //
};

/// Returns a server credential with the test certificate chain and private key.
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

/// Returns a certificate store trusting the test CA.
fn client_cert_store() -> X509Store {
    let mut store = X509StoreBuilder::new();
    store
        .set_trust(Trust::SslServer)
        .unwrap()
        .add_cert(X509Certificate::parse_one_from_pem(CA).unwrap())
        .unwrap();
    store.build()
}
