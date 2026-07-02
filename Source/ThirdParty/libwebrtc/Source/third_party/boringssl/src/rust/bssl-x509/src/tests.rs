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

use super::*;

const CA_CERT: &[u8] = include_bytes!("./tests/BoringSSLTestCA.crt");
const PEM: &[u8] = include_bytes!("./tests/consolidated.pem");
const TRUNCATED: &[u8] = include_bytes!("./tests/truncated.pem");
const TRUNCATED_ONE: &[u8] = include_bytes!("./tests/truncated-one.pem");
const PEM_WITH_PASS: &[u8] = include_bytes!("./tests/BoringSSLTestCA.key");
const PUBKEY: &[u8] = include_bytes!("./tests/BoringSSLTestCA-PubKey.pem");
const SERVER_CERT: &[u8] = include_bytes!("./tests/BoringSSLServerTest-RSA.crt");
const SERIAL_NUMBERS: &[&[u8]] = &[
    &[
        0x1e, 0x51, 0x13, 0x9d, 0x9c, 0x81, 0x90, 0x38, 0x25, 0x32, 0x72, 0x51, 0xf3, 0x33, 0x1b,
        0x70, 0xd0, 0x9b, 0x6a, 0x5c,
    ],
    &[
        0x50, 0x2d, 0x32, 0x51, 0x7b, 0xb9, 0x51, 0xf0, 0xdf, 0x6e, 0x4e, 0xc3, 0x05, 0x1f, 0x2c,
        0x4b, 0xaf, 0xcd, 0x52, 0x74,
    ],
    &[
        0x14, 0x05, 0x09, 0xad, 0x35, 0xee, 0xd1, 0xb6, 0x9a, 0xea, 0x34, 0xfb, 0x2a, 0x27, 0x19,
        0x82, 0x39, 0xfd, 0xf8, 0x84,
    ],
    &[
        0x14, 0x05, 0x09, 0xad, 0x35, 0xee, 0xd1, 0xb6, 0x9a, 0xea, 0x34, 0xfb, 0x2a, 0x27, 0x19,
        0x82, 0x39, 0xfd, 0xf8, 0x85,
    ],
    &[
        0x50, 0x2d, 0x32, 0x51, 0x7b, 0xb9, 0x51, 0xf0, 0xdf, 0x6e, 0x4e, 0xc3, 0x05, 0x1f, 0x2c,
        0x4b, 0xaf, 0xcd, 0x52, 0x73,
    ],
    &[
        0x2a, 0x7d, 0x8e, 0xd1, 0xd8, 0x49, 0xdf, 0xfa, 0xb6, 0x31, 0x9a, 0xdf, 0x0b, 0x04, 0x9f,
        0xdc, 0xcb, 0xbe, 0x82, 0x03,
    ],
];
const NOT_BEFORE: &[i64] = &[
    1769075621, // Jan 22 09:53:41 2026 GMT
    1769175652, // Jan 23 13:40:52 2026 GMT
    1770507561, // Feb  7 23:39:21 2026 GMT
    1770507569, // Feb  7 23:39:29 2026 GMT
    1769078409, // Jan 22 10:40:09 2026 GMT
    1770046795, // Feb  2 15:39:55 2026 GMT
];
const NOT_AFTER: &[i64] = &[
    1771667621,  // Feb 21 09:53:41 2026 GMT
    64884375652, // Feb  7 13:40:52 4026 GMT
    64885707561, // Feb 22 23:39:21 4026 GMT
    64885707569, // Feb 22 23:39:29 4026 GMT
    64884278409, // Feb  6 10:40:09 4026 GMT
    64885246795, // Feb 17 15:39:55 4026 GMT
];

#[test]
fn parse_all_pems() {
    let pems = certificates::X509Certificate::parse_all_from_pem(PEM).unwrap();
    assert_eq!(pems.len(), 6);
    for (((pem, &serial_number), &not_before), &not_after) in pems
        .iter()
        .zip(SERIAL_NUMBERS)
        .zip(NOT_BEFORE)
        .zip(NOT_AFTER)
    {
        assert_eq!(
            pem.serial_number().as_twos_complement_bytes(),
            serial_number
        );
        assert_eq!(pem.not_before().unwrap(), not_before);
        assert_eq!(pem.not_after().unwrap(), not_after);
    }

    assert!(matches!(
        certificates::X509Certificate::parse_all_from_pem(TRUNCATED),
        Err(errors::PkiError::X509(errors::X509Error::Pem(
            errors::PemReason::BadEndLine
        )))
    ));

    assert!(matches!(
        certificates::X509Certificate::parse_one_from_pem(TRUNCATED_ONE),
        Err(errors::PkiError::X509(errors::X509Error::Pem(
            errors::PemReason::BadEndLine
        )))
    ));

    assert!(matches!(
        certificates::X509Certificate::parse_one_from_pem(b"Ahoy!\n"),
        Err(errors::PkiError::X509(errors::X509Error::Pem(
            errors::PemReason::NoStartLine
        )))
    ));
}

#[test]
fn inspect_san() {
    let cert = certificates::X509Certificate::parse_one_from_pem(SERVER_CERT).unwrap();
    let san = cert.subject_alt_names().unwrap();
    let names: Vec<_> = san.iter().collect();
    assert_eq!(
        &names,
        &[
            certificates::GeneralName::Dns("www.google.com"),
            certificates::GeneralName::Dns("localhost"),
        ]
    )
}

#[test]
fn parse_ca_cert() {
    let ca_cert = certificates::X509Certificate::parse_all_from_pem(CA_CERT).unwrap();
    assert_eq!(ca_cert.len(), 1);
}

#[test]
fn parse_pem_privkey_with_pass() {
    assert!(keys::PrivateKey::from_pem(&PEM_WITH_PASS, || b"Try again!").is_err());
    let key = keys::PrivateKey::from_pem(&PEM_WITH_PASS, || b"BoringSSL is awesome!").unwrap();
    let ca_cert = certificates::X509Certificate::parse_one_from_pem(CA_CERT).unwrap();
    assert!(ca_cert.matches_private_key(&key));
}

#[test]
fn parse_pem_pubkey() {
    let pubkey = keys::PublicKey::from_pem(PUBKEY).unwrap();
    let ca_cert = certificates::X509Certificate::parse_one_from_pem(CA_CERT).unwrap();
    assert_eq!(ca_cert.public_key().unwrap().to_der(), pubkey.to_der());
}

#[test]
fn format_serial_number() {
    let certs = certificates::X509Certificate::parse_all_from_pem(PEM).unwrap();
    let cert = &certs[0];
    let serial = cert.serial_number();

    assert_eq!(
        format!("{serial}"),
        "173077792251592774985953586210206334486912395868"
    );

    // Alternate Display format should NOT prepend "0x" to a decimal number
    assert_eq!(
        format!("{serial:#}"),
        "173077792251592774985953586210206334486912395868"
    );

    assert_eq!(
        format!("{serial:x}"),
        "1e51139d9c81903825327251f3331b70d09b6a5c"
    );

    // Alternate LowerHex format SHOULD prepend "0x" to the hex string
    assert_eq!(
        format!("{serial:#x}"),
        "0x1e51139d9c81903825327251f3331b70d09b6a5c"
    );
}

#[test]
fn format_serial_number_padding() {
    let certs = certificates::X509Certificate::parse_all_from_pem(PEM).unwrap();
    let serial = certs[0].serial_number();

    assert_eq!(
        format!("{serial:045x}"),
        "000001e51139d9c81903825327251f3331b70d09b6a5c"
    );

    assert_eq!(
        format!("{serial:>45x}"),
        "     1e51139d9c81903825327251f3331b70d09b6a5c"
    );

    assert_eq!(
        format!("{serial:<45x}"),
        "1e51139d9c81903825327251f3331b70d09b6a5c     "
    );
}
