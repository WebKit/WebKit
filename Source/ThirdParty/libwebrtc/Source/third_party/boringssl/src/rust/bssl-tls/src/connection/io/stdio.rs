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

use super::{
    Error,
    TlsMode, //
};
use crate::{
    ReceiveBuffer,
    connection::TlsConnection,
    context::DtlsMode,
    errors::{
        IoError,
        TlsRetryReason, //
    },
    io::{
        AbstractSocketResult,
        IoStatus,
        stdio::DatagramSocket, //
    }, //
};

fn translate_res_for_stdio(res: Result<IoStatus, Error>) -> Result<usize, io::Error> {
    match res {
        Ok(IoStatus::Ok(bytes)) => Ok(bytes),
        Ok(IoStatus::EndOfStream) | Err(Error::Io(IoError::EndOfStream)) => Ok(0),
        Ok(IoStatus::Retry(TlsRetryReason::WantRead | TlsRetryReason::WantWrite)) => {
            Err(io::Error::new(io::ErrorKind::WouldBlock, "would block"))
        }
        Ok(IoStatus::Retry(reason)) => Err(io::Error::new(io::ErrorKind::Other, reason)),
        Ok(IoStatus::Err) => Err(io::Error::new(
            io::ErrorKind::Other,
            "The transport has failed the I/O operation",
        )),
        Ok(IoStatus::Empty) => Err(io::Error::new(
            io::ErrorKind::ConnectionReset,
            "connection reset or panicked",
        )),
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
        let res = self.sync_read(&mut buf);
        translate_res_for_stdio(res)
    }
}

impl<R> io::Write for TlsConnection<R, TlsMode> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        translate_res_for_stdio(self.sync_write(buf))
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

fn translate_result_for_datagram(res: Result<IoStatus, Error>) -> AbstractSocketResult {
    match res {
        Ok(IoStatus::Ok(bytes)) => AbstractSocketResult::Ok(bytes),
        Ok(IoStatus::EndOfStream) | Err(Error::Io(IoError::EndOfStream)) => {
            AbstractSocketResult::EndOfStream
        }
        Ok(IoStatus::Retry(_)) => AbstractSocketResult::Retry,
        Ok(IoStatus::Empty | IoStatus::Err) => AbstractSocketResult::Err(Box::new(io::Error::new(
            io::ErrorKind::Other,
            "transport failed or empty",
        ))),
        Err(e) => AbstractSocketResult::Err(Box::new(io::Error::new(io::ErrorKind::Other, e))),
    }
}

impl<R> DatagramSocket for TlsConnection<R, DtlsMode> {
    fn send(&mut self, datagram: &[u8]) -> AbstractSocketResult {
        translate_result_for_datagram(self.sync_write(datagram))
    }

    fn recv(&mut self, datagram: &mut [u8]) -> AbstractSocketResult {
        let mut datagram = ReceiveBuffer::new(datagram);
        translate_result_for_datagram(self.sync_read(&mut datagram))
    }
}
