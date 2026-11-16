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

use crate::alpn::{
    AlpnProtocols,
    H2,
    HTTP11, //
};

#[test]
fn alpn_config() {
    let mut config = AlpnProtocols::default();
    assert!(config.append_protocol(H2).is_ok());
    assert!(config.append_protocol(HTTP11).is_ok());
    assert_eq!(config.as_slice(), b"\x02h2\x08http/1.1");

    // Empty protocol is rejected.
    let mut config = AlpnProtocols::default();
    assert!(config.append_protocol(b"").is_err());

    // > 255 bytes is rejected, 255 is accepted.
    let mut config = AlpnProtocols::default();
    assert!(config.append_protocol(&[b'a'; 256]).is_err());
    assert_eq!(config.as_slice(), b"");
    assert!(config.append_protocol(&[b'a'; 255]).is_ok());
    assert_eq!(config.as_slice()[0], 255);
    assert_eq!(&config.as_slice()[1..], &[b'a'; 255]);
}
