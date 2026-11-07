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

use std::io;

use super::Error;
use crate::{
    ReceiveBuffer,
    connection::TlsConnection,
    context::TlsMode,
    errors::{
        IoError,
        TlsRetryReason, //
    },
    io::IoStatus, //
};

fn translate_res_for_stdio(res: Result<IoStatus, Error>) -> Result<usize, io::Error> {
    match res {
        Ok(IoStatus::Ok(bytes)) => Ok(bytes),
        // We must be able to differentiate between a graceful shutdown and a transport EOF
        Err(Error::Io(IoError::EndOfStream)) => Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "unexpected eof",
        )),

        Ok(IoStatus::Retry(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
            Err(io::Error::new(io::ErrorKind::WouldBlock, "would block"))
        }
        Ok(IoStatus::Retry(reason)) => Err(io::Error::new(io::ErrorKind::Other, reason)),
        Err(Error::Io(IoError::Transport(e))) => match e.downcast::<io::Error>() {
            Ok(err) => Err(*err),
            Err(e) => Err(io::Error::new(io::ErrorKind::Other, e)),
        },
        Err(
            e @ (Error::Library(..)
            | Error::Configuration(..)
            | Error::TlsReason(..)
            | Error::PemReason(..)
            | Error::Quic(..)
            | Error::Pki(..)
            | Error::Io(..)
            | Error::Unknown(..)),
        ) => Err(io::Error::new(io::ErrorKind::Other, e)),
    }
}

impl<R> io::Read for TlsConnection<R, TlsMode> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let mut buf = ReceiveBuffer::new(buf);
        let res = self.poll_read(&mut buf);
        translate_res_for_stdio(res)
    }
}

impl<R> io::Write for TlsConnection<R, TlsMode> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        translate_res_for_stdio(self.poll_write(buf))
    }

    fn flush(&mut self) -> io::Result<()> {
        translate_res_for_stdio(self.poll_flush()).map(|_| ())
    }
}
