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

//! I/O model for Unix-like systems
//!
//! It is strongly recommended that the non-blocking I/O is enabled by calling
//! [`std::net::TcpStream::set_nonblocking`] to `true`.

use std::{
    io,
    os::{
        fd::{
            AsRawFd,
            RawFd, //
        },
        unix::net::UnixDatagram, //
    },
    task::{
        Context,
        Poll, //
    }, //
};

#[cfg(feature = "libc")]
use crate::ffi::{
    mut_slice_into_ffi_raw_parts,
    slice_into_ffi_raw_parts, //
};
use crate::io::stdio::{
    DatagramSocket,
    PollFor, //
};

use super::{
    AbstractReader,
    AbstractSocket,
    AbstractSocketResult,
    AbstractWriter, //
};

// ============
// Datagrams
// ============

/// A datagram socket.
pub struct StdDatagram<Socket, Reactor> {
    reactor: Reactor,
    socket: Socket,
}

impl<S, R> StdDatagram<S, R> {
    /// Construct a new Unix Datagram.
    pub fn new(socket: S, reactor: R) -> Self {
        Self { socket, reactor }
    }
}

impl<Socket: DatagramSocket, Reactor: PollFor<Socket> + Send> AbstractReader
    for StdDatagram<Socket, Reactor>
{
    fn read(
        &mut self,
        mut async_ctx: Option<&mut Context<'_>>,
        buffer: &mut [u8],
    ) -> AbstractSocketResult {
        loop {
            match (self.socket.recv(buffer), async_ctx.as_mut()) {
                (AbstractSocketResult::Retry, Some(ctx)) => match self.reactor.poll_read(ctx) {
                    Poll::Pending => return AbstractSocketResult::Retry,
                    Poll::Ready(Ok(_)) => {}
                    Poll::Ready(Err(e)) => {
                        return AbstractSocketResult::Err(Box::new(e));
                    }
                },
                (res, _) => return res,
            }
        }
    }
}

impl<Socket: DatagramSocket, Reactor: PollFor<Socket> + Send> AbstractWriter
    for StdDatagram<Socket, Reactor>
{
    fn write(
        &mut self,
        mut async_ctx: Option<&mut Context<'_>>,
        buffer: &[u8],
    ) -> AbstractSocketResult {
        loop {
            match (self.socket.send(buffer), async_ctx.as_mut()) {
                (AbstractSocketResult::Retry, Some(ctx)) => {
                    if matches!(self.reactor.poll_write(ctx), Poll::Pending) {
                        return AbstractSocketResult::Retry;
                    }
                }
                (res, _) => return res,
            }
        }
    }

    fn flush(&mut self, _: Option<&mut Context<'_>>) -> AbstractSocketResult {
        AbstractSocketResult::Ok(0)
    }
}

impl<Socket: DatagramSocket, Reactor: PollFor<Socket> + Send> AbstractSocket
    for StdDatagram<Socket, Reactor>
{
}

impl DatagramSocket for UnixDatagram {
    fn send(&mut self, datagram: &[u8]) -> AbstractSocketResult {
        loop {
            return match UnixDatagram::send(self, datagram) {
                Ok(bytes) => AbstractSocketResult::Ok(bytes),
                Err(e) => crate::retry_on_interrupt!(e),
            };
        }
    }

    fn recv(&mut self, datagram: &mut [u8]) -> AbstractSocketResult {
        loop {
            return match UnixDatagram::recv(self, datagram) {
                Ok(bytes) => AbstractSocketResult::Ok(bytes),
                Err(e) if matches!(e.kind(), io::ErrorKind::Interrupted) => continue,
                Err(e) => crate::retry_on_interrupt!(e),
            };
        }
    }
}

/// A wrapper to operate the IO object through file descriptor.
///
/// # Notice to **BSD** datagram socket users
///
/// Whether a `SIGPIPE` signal will be raised is controlled at socket configuration time with
/// `setsocketopt` plus `SO_NOSIGPIPE`.
/// If the signal should be suppressed, it should be configured before using this type.
pub struct UseFd<Io: AsRawFd>(pub Io);

#[cfg(feature = "libc")]
impl<Io: AsRawFd> io::Read for UseFd<Io> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let (ptr, len) = mut_slice_into_ffi_raw_parts(buf);
        let ret = unsafe {
            // Safety: `ptr` has been valid with the sufficient capacity of length `len`.
            libc::read(self.as_raw_fd(), ptr as _, len)
        };
        if ret < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ret as usize)
        }
    }
}

#[cfg(feature = "libc")]
impl<Io: AsRawFd> io::Write for UseFd<Io> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let (ptr, len) = slice_into_ffi_raw_parts(buf);
        let ret = unsafe {
            // Safety: `ptr` has been valid for length `len`.
            libc::write(self.as_raw_fd(), ptr as _, len)
        };
        if ret < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ret as usize)
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[cfg(feature = "libc")]
impl<Io: AsRawFd + Send> DatagramSocket for UseFd<Io> {
    fn send(&mut self, datagram: &[u8]) -> AbstractSocketResult {
        let (buf, len) = slice_into_ffi_raw_parts(datagram);
        #[cfg(any(target_os = "linux", target_os = "android"))]
        let flag = libc::MSG_NOSIGNAL;
        #[cfg(any(
            target_os = "freebsd",
            target_os = "netbsd",
            target_os = "openbsd",
            target_os = "macos",
            target_os = "ios",
        ))]
        let flag = 0;
        #[cfg(any(windows, target_os = "none"))]
        let flag = 0;
        loop {
            let rc = unsafe {
                // Safety: the socket file descriptor is exclusively owned.
                libc::send(self.as_raw_fd(), buf as _, len, flag)
            };
            return if rc < 0 {
                let err = io::Error::last_os_error();
                crate::retry_on_interrupt!(err)
            } else {
                AbstractSocketResult::Ok(rc as usize)
            };
        }
    }

    fn recv(&mut self, datagram: &mut [u8]) -> AbstractSocketResult {
        let (buf, len) = mut_slice_into_ffi_raw_parts(datagram);
        loop {
            let rc = unsafe {
                // Safety: the socket file descriptor is exclusively owned.
                libc::recv(self.as_raw_fd(), buf as _, len, 0)
            };
            return if rc < 0 {
                let err = io::Error::last_os_error();
                crate::retry_on_interrupt!(err)
            } else {
                AbstractSocketResult::Ok(rc as usize)
            };
        }
    }
}

impl<Io: AsRawFd> AsRawFd for UseFd<Io> {
    fn as_raw_fd(&self) -> RawFd {
        self.0.as_raw_fd()
    }
}
