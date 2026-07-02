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

use crate::tests::dumb_server_client;

#[cfg(unix)]
#[test]
fn stdio() {
    use std::io::{Read, Write};

    use crate::io::sync_io::{NoAsync, StdIoWithReactor};

    let (mut server_conn, mut client_conn) = dumb_server_client().unwrap();

    let (server_rx, server_tx) = std::io::pipe().unwrap();
    let (client_rx, client_tx) = std::io::pipe().unwrap();
    let server_rx = StdIoWithReactor::new(server_rx, NoAsync);
    let server_tx = StdIoWithReactor::new(server_tx, NoAsync);
    let client_rx = StdIoWithReactor::new(client_rx, NoAsync);
    let client_tx = StdIoWithReactor::new(client_tx, NoAsync);

    server_conn.set_split_io(client_rx, server_tx).unwrap();
    client_conn.set_split_io(server_rx, client_tx).unwrap();

    let thread = std::thread::spawn(move || {
        let mut message = [0; 21];
        // Use `std::io::Read::read_exact`
        server_conn.read_exact(&mut message).unwrap();
        assert_eq!(message, *b"BoringSSL is awesome!");
        // Use `std::io::Write::write_all`
        server_conn.write_all(b"Oh yeah definitely!").unwrap();
        server_conn.established().unwrap().sync_shutdown().unwrap();
        // Second shutdown poll.
        server_conn.established().unwrap().sync_shutdown().unwrap();
    });

    // Use `std::io::Write::write_all`
    client_conn.write_all(b"BoringSSL is awesome!").unwrap();
    let mut message = [0; 19];
    // Use `std::io::Read::read_exact`
    client_conn.read_exact(&mut message).unwrap();
    assert_eq!(message, *b"Oh yeah definitely!");
    client_conn.established().unwrap().sync_shutdown().unwrap();
    thread.join().unwrap();
}
