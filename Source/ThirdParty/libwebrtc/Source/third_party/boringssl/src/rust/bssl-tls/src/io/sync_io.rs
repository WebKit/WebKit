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

//! Synchronous I/O adapters.

use crate::io::stdio::PollFor;
use crate::io::{AbstractReader, AbstractSocket, AbstractSocketResult, AbstractWriter};
use std::{
    io,
    marker::PhantomData,
    task::{Context, Poll},
};

#[macro_export]
#[doc(hidden)]
macro_rules! retry_on_interrupt {
    ($e:ident) => {
        if matches!($e.kind(), ::std::io::ErrorKind::Interrupted) {
            continue;
        } else {
            $crate::io::sync_io::translate_stdio_err($e)
        }
    };
}

/// `std::io::Read` and `std::io::Write` wrapper.
pub struct StdIoWithReactor<Io, Reactor> {
    io: Io,
    reactor: Reactor,
    _p: PhantomData<fn() -> Reactor>,
}

impl<Io, Reactor> StdIoWithReactor<Io, Reactor> {
    /// Construct a new I/O wrapper
    pub fn new(io: Io, reactor: Reactor) -> Self {
        Self {
            io,
            reactor,
            _p: PhantomData,
        }
    }
}

impl<Io: io::Read + Send, R: PollFor<Io> + Send> AbstractReader for StdIoWithReactor<Io, R> {
    fn read(
        &mut self,
        mut async_ctx: Option<&mut Context<'_>>,
        buffer: &mut [u8],
    ) -> AbstractSocketResult {
        loop {
            let res = match <Io as io::Read>::read(&mut self.io, buffer) {
                Ok(0) if !buffer.is_empty() => return AbstractSocketResult::EndOfStream,
                Ok(bytes) => return AbstractSocketResult::Ok(bytes),
                Err(e) => retry_on_interrupt!(e),
            };
            if let Some(async_ctx) = &mut async_ctx
                && matches!(res, AbstractSocketResult::Retry)
            {
                match self.reactor.poll_read(async_ctx) {
                    Poll::Ready(Err(e)) => return AbstractSocketResult::Err(Box::new(e)),
                    Poll::Pending => return AbstractSocketResult::Retry,
                    Poll::Ready(Ok(())) => continue,
                }
            }
            return res;
        }
    }
}

impl<Io: io::Write + Send, R: PollFor<Io> + Send> AbstractWriter for StdIoWithReactor<Io, R> {
    fn write(
        &mut self,
        mut async_ctx: Option<&mut Context<'_>>,
        buffer: &[u8],
    ) -> AbstractSocketResult {
        loop {
            let res = match <Io as io::Write>::write(&mut self.io, buffer) {
                Ok(bytes) => return AbstractSocketResult::Ok(bytes),
                Err(e) => retry_on_interrupt!(e),
            };
            if let Some(async_ctx) = &mut async_ctx
                && matches!(res, AbstractSocketResult::Retry)
            {
                match self.reactor.poll_write(async_ctx) {
                    Poll::Ready(Err(e)) => return AbstractSocketResult::Err(Box::new(e)),
                    Poll::Pending => return AbstractSocketResult::Retry,
                    Poll::Ready(Ok(())) => continue,
                }
            }
            return res;
        }
    }

    fn flush(&mut self, mut async_ctx: Option<&mut Context<'_>>) -> AbstractSocketResult {
        loop {
            let res = match <Io as io::Write>::flush(&mut self.io) {
                Ok(_) => return AbstractSocketResult::Ok(0),
                Err(e) => retry_on_interrupt!(e),
            };
            if let Some(async_ctx) = &mut async_ctx
                && matches!(res, AbstractSocketResult::Retry)
            {
                match self.reactor.poll_write(async_ctx) {
                    Poll::Ready(Err(e)) => return AbstractSocketResult::Err(Box::new(e)),
                    Poll::Pending => return AbstractSocketResult::Retry,
                    Poll::Ready(Ok(())) => continue,
                }
            }
            return res;
        }
    }
}

impl<Io: io::Read + io::Write + Send, R: PollFor<Io> + Send> AbstractSocket
    for StdIoWithReactor<Io, R>
{
}

/// Translates a `std::io::Error` into an `AbstractSocketResult`.
pub(crate) fn translate_stdio_err(err: io::Error) -> AbstractSocketResult {
    match err.kind() {
        io::ErrorKind::WouldBlock => AbstractSocketResult::Retry,
        io::ErrorKind::ConnectionReset
        | io::ErrorKind::ConnectionRefused
        | io::ErrorKind::ConnectionAborted
        | io::ErrorKind::BrokenPipe
        | io::ErrorKind::NotConnected
        | io::ErrorKind::UnexpectedEof => AbstractSocketResult::EndOfStream,
        _ => AbstractSocketResult::Err(Box::new(err)),
    }
}

/// Reactor is absent.
/// Polling returns immediately with success, turning any I/O operation into a blocking call.
///
/// # Warning ⚠️
///
/// If the underlying I/O is non-blocking, this reactor will lead to busy-waiting and excessive
/// CPU usage.
pub struct NoAsync;

impl<Io> PollFor<Io> for NoAsync {
    fn poll_read(&mut self, _: &mut Context<'_>) -> Poll<Result<(), io::Error>> {
        Poll::Ready(Ok(()))
    }

    fn poll_write(&mut self, _: &mut Context<'_>) -> Poll<Result<(), io::Error>> {
        Poll::Ready(Ok(()))
    }
}
