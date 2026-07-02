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

//! TLS I/O protocols under `std`

use std::{
    io,
    task::{Context, Poll},
};

use super::AbstractSocketResult;

/// A datagram socket protocol
pub trait DatagramSocket: Send {
    /// Send a complete datagram through the socket.
    ///
    /// By returning [`AbstractSocketResult::Retry`] the socket signals that the datagram
    /// has not been sent down the transport.
    fn send(&mut self, datagram: &[u8]) -> AbstractSocketResult;
    /// Receive a complete datagram through the socket.
    ///
    /// By returning [`AbstractSocketResult::Retry`] the socket signals that a datagram
    /// has not been received from the transport.
    ///
    /// If the `datagram` is not large enough to receive the whole datagram,
    /// the datagram will be truncated while the actual size of the consumed datagram is reported
    /// as [`AbstractSocketResult::Ok`].
    ///
    /// The datagram will be consumed on successful reception, even with `datagram.is_empty()`.
    fn recv(&mut self, datagram: &mut [u8]) -> AbstractSocketResult;
}

/// Protocol and mechanism to register interest in an `async` runtime.
pub trait PollFor<Io> {
    /// Register an interest in reading from the `io` object.
    fn poll_read(&mut self, async_ctx: &mut Context<'_>) -> Poll<Result<(), io::Error>>;
    /// Register an interest in writing to the `io` object.
    fn poll_write(&mut self, async_ctx: &mut Context<'_>) -> Poll<Result<(), io::Error>>;
}
