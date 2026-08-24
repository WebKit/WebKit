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

use crate::{
    connection::{
        Client,
        Server,
        TlsConnection, //
    },
    context::TlsContext,
    io::sync_io::{NoAsync, StdIoWithReactor}, //
};

use std::{
    io::{
        Read,
        Write, //
    },
    marker::PhantomData, //
};

/// A convenient wrapper around `TlsContext` for creating synchronous client connections.
pub struct TlsConnector {
    ctx: TlsContext,
}

impl TlsConnector {
    /// Construct a new `TlsConnector`.
    pub(crate) fn new(ctx: TlsContext) -> Self {
        Self { ctx }
    }

    /// Connect to the given domain using the provided stream.
    pub fn connect<S>(
        &self,
        domain: &str,
        stream: S,
    ) -> Result<TlsStream<Client, S>, crate::errors::Error>
    where
        S: Read + Write + Send + 'static,
    {
        let mut conn = self.ctx.new_client_connection().build();
        {
            conn.in_handshake()
                .expect("connection is freshly constructed and it cannot already be established")
                .set_host(domain)?;
            conn.set_io(StdIoWithReactor::new(stream, NoAsync))?
                .do_handshake()?;
        }

        Ok(TlsStream {
            conn,
            _marker: PhantomData,
        })
    }
}

/// A wrapper around `TlsContext` for creating synchronous server connections.
pub struct TlsAcceptor {
    ctx: TlsContext,
}

impl TlsAcceptor {
    /// Construct a new `TlsAcceptor`.
    pub(crate) fn new(ctx: TlsContext) -> Self {
        Self { ctx }
    }

    /// Accept a new connection using the provided stream.
    pub fn accept<S>(&self, stream: S) -> Result<TlsStream<Server, S>, crate::errors::Error>
    where
        S: Read + Write + Send + 'static,
    {
        let mut conn = self.ctx.new_server_connection().build();
        conn.set_io(StdIoWithReactor::new(stream, NoAsync))?;
        conn.do_handshake()?;

        Ok(TlsStream {
            conn,
            _marker: PhantomData,
        })
    }
}

/// A TLS stream driven by synchronous I/O.
pub struct TlsStream<Role, S> {
    conn: TlsConnection<Role>,
    _marker: PhantomData<S>,
}

impl<Role, S> TlsStream<Role, S> {
    /// Get a reference to the underlying `TlsConnection`.
    pub fn get_ref(&self) -> &TlsConnection<Role> {
        &self.conn
    }

    /// Get a mutable reference to the underlying `TlsConnection`.
    pub fn get_mut(&mut self) -> &mut TlsConnection<Role> {
        &mut self.conn
    }
}

impl<Role, S> Read for TlsStream<Role, S> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.conn.read(buf)
    }
}

impl<Role, S> Write for TlsStream<Role, S> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.conn.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Write::flush(&mut self.conn)
    }
}
