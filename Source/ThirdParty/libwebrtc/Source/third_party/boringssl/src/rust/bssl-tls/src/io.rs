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

//! TLS I/O model

use alloc::boxed::Box;
use core::{
    ffi::{
        CStr,
        c_char,
        c_int,
        c_long,
        c_void, //
    },
    fmt,
    ptr::NonNull,
    task::{
        Context,
        Waker, //
    },
};

use once_cell::sync::Lazy;

use crate::{
    abort_on_panic,
    errors::{
        Error,
        TlsRetryReason, //
    },
    ffi::{
        sanitise_mut_byteslice,
        sanitize_slice, //
    },
};

#[cfg(feature = "std")]
pub mod stdio;
#[cfg(feature = "std")]
pub mod sync_io;

#[cfg(all(unix, feature = "std"))]
pub mod unix;

/// A wrapper around a `dyn AbstractSocket`, delegating BIO methods to the
/// underlying `AbstractSocket` implementations.
///
/// # Safety
///
/// [`RustBio`] can only be accessed through exclusive ownership.
// TODO(@xfding): switch to `derive(CoercePointee)` when it stabilises for flattening the layout.
pub(crate) struct RustBio {
    // We may need to propagate the waker, but nothing more.
    waker: Option<Waker>,
    pub read_eos: bool,
    pub write_eos: bool,
    socket: Option<Box<dyn AbstractSocket>>,
    reader: Option<Box<dyn AbstractReader>>,
    writer: Option<Box<dyn AbstractWriter>>,
    io_err: Option<Box<dyn core::error::Error + Send + Sync>>,
}

fn _assert_rust_bio()
where
    RustBio: Send + Unpin,
{
}

/// Purely module-internal implementation details
impl RustBio {
    fn init() -> Self {
        Self {
            waker: None,
            socket: None,
            reader: None,
            writer: None,
            read_eos: false,
            write_eos: false,
            io_err: None,
        }
    }

    pub fn attach_socket(&mut self, socket: Box<dyn AbstractSocket>) {
        self.socket = Some(socket);
    }

    pub fn attach_reader(&mut self, reader: Box<dyn AbstractReader>) {
        self.reader = Some(reader);
    }

    pub fn attach_writer(&mut self, writer: Box<dyn AbstractWriter>) {
        self.writer = Some(writer);
    }

    pub fn get_reader(&mut self) -> Option<&mut dyn AbstractReader> {
        if let Some(reader) = &mut self.reader {
            Some(&mut **reader)
        } else if let Some(socket) = &mut self.socket {
            Some(&mut **socket)
        } else {
            None
        }
    }

    pub fn get_writer(&mut self) -> Option<&mut dyn AbstractWriter> {
        if let Some(writer) = &mut self.writer {
            Some(&mut **writer)
        } else if let Some(socket) = &mut self.socket {
            Some(&mut **socket)
        } else {
            None
        }
    }
}

/// Crate-level `BIO` implementation details
impl RustBio {
    pub fn set_waker(&mut self, waker: &Waker) {
        if let Some(other_waker) = &self.waker
            && waker.will_wake(other_waker)
        {
            return;
        }
        self.waker = Some(waker.clone());
    }

    pub fn new_duplex<T: 'static + AbstractSocket + Sized>(
        socket: T,
    ) -> Result<RustBioHandle, Error> {
        let bio = unsafe {
            // Safety: the BIO_METH will be valid if this is the first call.
            bssl_sys::BIO_new(get_bio_method())
        };
        let bio = NonNull::new(bio).expect("allocation failure");
        unsafe {
            // Safety: `bio` was constructed with our own BIO_METH, so the data must be valid
            // as a `RustBio`
            rust_bio_data_mut(bio.as_ptr()).attach_socket(Box::new(socket));
        }
        Ok(RustBioHandle(bio))
    }

    pub fn new_split<R, W>(reader: R, writer: W) -> Result<RustBioHandle, Error>
    where
        R: 'static + AbstractReader + Sized,
        W: 'static + AbstractWriter + Sized,
    {
        let bio = unsafe {
            // Safety: the BIO_METH will be valid if this is the first call.
            bssl_sys::BIO_new(get_bio_method())
        };
        let bio = NonNull::new(bio).expect("allocation failure");
        let data = unsafe {
            // Safety: `bio` was constructed with our own BIO_METH, so the data must be valid
            // as a `RustBio`
            rust_bio_data_mut(bio.as_ptr())
        };
        data.attach_reader(Box::new(reader));
        data.attach_writer(Box::new(writer));
        Ok(RustBioHandle(bio))
    }

    pub fn take_io_err(&mut self) -> Option<Box<dyn core::error::Error + Send + Sync>> {
        self.io_err.take()
    }

    fn transform_result(
        &mut self,
        res: AbstractSocketResult,
        reason_on_retry: TlsRetryReason,
    ) -> IoStatus {
        match res {
            AbstractSocketResult::Ok(bytes) => IoStatus::Ok(bytes),
            AbstractSocketResult::Retry => IoStatus::Retry(reason_on_retry),
            AbstractSocketResult::EndOfStream => IoStatus::EndOfStream,
            AbstractSocketResult::Err(e) => {
                self.io_err = Some(e);
                IoStatus::Err
            }
        }
    }
}

/// An exclusively owned handle to a BIO constructed by this crate.
pub(crate) struct RustBioHandle(NonNull<bssl_sys::BIO>);

impl RustBioHandle {
    pub fn ptr(&self) -> *mut bssl_sys::BIO {
        self.0.as_ptr()
    }

    pub fn as_mut(&mut self) -> &mut RustBio {
        unsafe {
            // Safety: `self` witnesses the validity of the handle.
            rust_bio_data_mut(self.ptr())
        }
    }

    pub fn as_ref(&self) -> &RustBio {
        unsafe {
            // Safety: `self` witnesses the validity of the handle.
            rust_bio_data(self.ptr())
        }
    }

    pub fn set_waker(&mut self, waker: &Waker) {
        self.as_mut().set_waker(waker);
    }
}

impl Drop for RustBioHandle {
    fn drop(&mut self) {
        unsafe {
            // Safety: the BIO handle should still be valid and was created by this crate.
            bssl_sys::BIO_free(self.0.as_ptr());
        }
    }
}

/// Safety: caller must ensure that `bio` is created with `rust_bio_create` and outlives `'a`
/// for exclusive access.
unsafe fn rust_bio_data_mut<'a>(bio: *mut bssl_sys::BIO) -> &'a mut RustBio {
    let data = unsafe {
        // Safety: `bio` is still valid
        bssl_sys::BIO_get_data(bio)
    };
    unsafe { &mut *(data as *mut RustBio) }
}

/// Safety: caller must ensure that `bio` is created with `rust_bio_create` and outlives `'a`
/// for shared access.
unsafe fn rust_bio_data<'a>(bio: *mut bssl_sys::BIO) -> &'a RustBio {
    let data = unsafe {
        // Safety: `bio` is still valid
        bssl_sys::BIO_get_data(bio)
    };
    unsafe { &*(data as *const RustBio) }
}

/// I/O Status of the possibly pending operation.
#[derive(Debug, Clone, Copy)]
pub enum IoStatus {
    /// Successfully performed I/O of bytes at certain size.
    Ok(usize),
    /// There is no more data to read or write.
    EndOfStream,
    /// I/O operation should be retried with the exactly same buffers when applicable.
    Retry(TlsRetryReason),
    /// There is no backing socket.
    Empty,
    /// I/O operation has failed.
    Err,
}

/// Result of operating an [`AbstractSocket`].
pub enum AbstractSocketResult {
    /// I/O completed by committing some amount of bytes.
    Ok(usize),
    /// I/O is pending completion; the I/O operation should be invoked again with the same parameter.
    Retry,
    /// I/O is impossible because the stream has ended.
    EndOfStream,
    /// I/O operation failed.
    Err(Box<dyn core::error::Error + Send + Sync>),
}

/// Abstract reader.
pub trait AbstractReader: Send {
    /// Read data from the socket.
    fn read(
        &mut self,
        async_ctx: Option<&mut Context<'_>>,
        buffer: &mut [u8],
    ) -> AbstractSocketResult;
}

/// Abstract writer.
pub trait AbstractWriter: Send {
    /// Write data to the socket.
    fn write(&mut self, async_ctx: Option<&mut Context<'_>>, buffer: &[u8])
    -> AbstractSocketResult;
    /// Flush the socket.
    fn flush(&mut self, async_ctx: Option<&mut Context<'_>>) -> AbstractSocketResult;
}

/// Abstract socket wrapper around Rust types that may support async I/O.
pub trait AbstractSocket: AbstractReader + AbstractWriter {}

// NOTE: this is not dead code, we are asserting that `dyn AbstractSocket` is a well-formed type,
// or `AbstractSocket` is dyn-compatible specifically.
fn _assert_dyn_compat()
where
    dyn AbstractSocket:,
{
}

fn get_bio_type() -> c_int {
    static BIO_TYPE: Lazy<c_int> = Lazy::new(|| {
        // Safety: this call does not have side-effect other than ID assignment
        unsafe { bssl_sys::BIO_get_new_index() }
    });
    *BIO_TYPE
}

struct BioMethod(*mut bssl_sys::BIO_METHOD);
/// Safety: once constructed this BIO vtable will stay immutable.
unsafe impl Send for BioMethod {}
/// Safety: once constructed this BIO vtable will stay immutable.
unsafe impl Sync for BioMethod {}

fn get_bio_method() -> *const bssl_sys::BIO_METHOD {
    static BIO_METHOD: Lazy<BioMethod> = Lazy::new(|| {
        let cstr = const {
            if let Ok(cstr) = CStr::from_bytes_with_nul(b"rust_bio\0") {
                cstr
            } else {
                // Compile-time assertion
                unreachable!()
            }
        };
        let vtable = unsafe {
            // Safety: this call does not have side-effect other than allocation.
            bssl_sys::BIO_meth_new(get_bio_type(), cstr.as_ptr())
        };
        unsafe {
            // Safety: all the following calls are simple assignments to the vtable entries.
            bssl_sys::BIO_meth_set_read(vtable, Some(rust_bio_read));
            bssl_sys::BIO_meth_set_write(vtable, Some(rust_bio_write));
            bssl_sys::BIO_meth_set_ctrl(vtable, Some(rust_bio_ctrl));
            bssl_sys::BIO_meth_set_create(vtable, Some(rust_bio_create));
            bssl_sys::BIO_meth_set_destroy(vtable, Some(rust_bio_destroy));
        }
        BioMethod(vtable)
    });
    BIO_METHOD.0
}

unsafe extern "C" fn rust_bio_read(
    bio: *mut bssl_sys::BIO,
    buffer: *mut c_char,
    buf_len: c_int,
) -> c_int {
    let rust_bio = unsafe {
        // Safety: `bio` is still valid and so is the `RustBio` which we have exclusive access to.
        rust_bio_data_mut(bio)
    };
    if rust_bio.read_eos {
        return 0;
    }
    let Ok(len) = usize::try_from(buf_len) else {
        return -1;
    };
    let waker = rust_bio.waker.clone();
    let mut async_ctx = if let Some(waker) = &waker {
        Some(Context::from_waker(waker))
    } else {
        None
    };
    // Zero the buffer now.
    // TODO(@xfding): maybe we want to have a buffer wrapper that tracks initialised region.
    let Some(buf) = (unsafe {
        // Safety: `buffer` and `len` are sanitised and initialised for the right memory region.
        sanitise_mut_byteslice(buffer as *mut u8, len)
    }) else {
        return -1;
    };
    let work = {
        let Some(reader) = rust_bio.get_reader() else {
            return -1;
        };
        move || reader.read(async_ctx.as_mut(), buf)
    };
    let res = abort_on_panic(work);
    match rust_bio.transform_result(res, TlsRetryReason::WantRead) {
        IoStatus::Ok(bytes) => {
            if let Ok(bytes) = c_int::try_from(bytes) {
                // Here the `bytes` read out from the transport as reported by the application does
                // not necessary stay in-bound, it could be application error or active exploit.
                // Therefore, we can fully trust that the application behaves well.
                // In order to not break `libssl` we can cap the number of writes written.
                return bytes.min(buf_len);
            }
            -1
        }
        IoStatus::EndOfStream => {
            rust_bio.read_eos = true;
            -1
        }
        IoStatus::Retry(_) => {
            unsafe {
                // Safety: `bio` is still valid now.
                bssl_sys::BIO_set_retry_read(bio);
            }
            -1
        }
        IoStatus::Empty | IoStatus::Err => -1,
    }
}

unsafe extern "C" fn rust_bio_write(
    bio: *mut bssl_sys::BIO,
    buffer: *const c_char,
    buf_len: c_int,
) -> c_int {
    let rust_bio = unsafe {
        // Safety: `bio` is still valid and so is the `RustBio` which we have exclusive access to.
        rust_bio_data_mut(bio)
    };
    if rust_bio.write_eos {
        return 0;
    }
    let Ok(len) = usize::try_from(buf_len) else {
        return -1;
    };
    let waker = rust_bio.waker.clone();
    let mut async_ctx = if let Some(waker) = &waker {
        Some(Context::from_waker(waker))
    } else {
        None
    };
    let Some(buf) = (unsafe {
        // Safety: `buffer` and `len` are sanitised and initialised for the right memory region.
        sanitize_slice(buffer as *mut u8, len)
    }) else {
        return -1;
    };
    let work = {
        let Some(writer) = rust_bio.get_writer() else {
            return -1;
        };
        move || writer.write(async_ctx.as_mut(), buf)
    };
    let res = abort_on_panic(work);
    match rust_bio.transform_result(res, TlsRetryReason::WantWrite) {
        IoStatus::Ok(bytes) => {
            if let Ok(bytes) = c_int::try_from(bytes) {
                // Here the `bytes` sent out to the transport as reported by the application does
                // not necessary stay in-bound, it could be application error or active exploit.
                // Therefore, we can fully trust that the application behaves well.
                return bytes.min(buf_len);
            }
            -1
        }
        IoStatus::EndOfStream => {
            rust_bio.write_eos = true;
            0
        }
        IoStatus::Retry(_) => {
            unsafe {
                // Safety: `bio` is still valid now.
                bssl_sys::BIO_set_retry_write(bio);
            }
            -1
        }
        IoStatus::Empty | IoStatus::Err => -1,
    }
}

unsafe fn rust_bio_flush(bio: *mut bssl_sys::BIO) -> c_long {
    let rust_bio = unsafe {
        // Safety: `bio` is still valid
        rust_bio_data_mut(bio)
    };
    if rust_bio.write_eos {
        return 0;
    }
    let waker = rust_bio.waker.clone();
    let mut async_ctx = if let Some(waker) = &waker {
        Some(Context::from_waker(waker))
    } else {
        None
    };
    let work = {
        let Some(writer) = rust_bio.get_writer() else {
            return -1;
        };
        move || writer.flush(async_ctx.as_mut())
    };
    let res = abort_on_panic(work);
    match rust_bio.transform_result(res, TlsRetryReason::WantWrite) {
        IoStatus::Ok(_) | IoStatus::Retry(_) => 1,
        IoStatus::EndOfStream => {
            rust_bio.write_eos = true;
            0
        }
        IoStatus::Empty | IoStatus::Err => 0,
    }
}

unsafe extern "C" fn rust_bio_ctrl(
    bio: *mut bssl_sys::BIO,
    ctrl: c_int,
    _: c_long,
    _: *mut c_void,
) -> c_long {
    match ctrl {
        bssl_sys::BIO_CTRL_FLUSH => unsafe {
            // Safety: `bio` is still valid.
            rust_bio_flush(bio)
        },
        _ => 0,
    }
}

unsafe extern "C" fn rust_bio_create(bio: *mut bssl_sys::BIO) -> c_int {
    let data = Box::new(RustBio::init());
    let data = Box::into_raw(data);
    unsafe {
        // Safety: both `bio` and `data` are still valid and exclusively owned.
        bssl_sys::BIO_set_data(bio, data as _);
        // Safety: it is now already safe to mark BIO initialised.
        bssl_sys::BIO_set_init(bio, 1);
    }
    1
}

unsafe extern "C" fn rust_bio_destroy(bio: *mut bssl_sys::BIO) -> c_int {
    let rust_bio = unsafe {
        // Safety: `bio` is still valid
        bssl_sys::BIO_get_data(bio) as *mut RustBio
    };
    let rust_bio = unsafe {
        // Safety: `rust_bio` is created from `rust_bio_create`
        Box::from_raw(rust_bio)
    };
    // Try to catch unwinding on the FFI boundary.
    // NOTE: it is not safe to drop the error value because its destructor can panic again.
    abort_on_panic(move || {
        let _ = rust_bio;
    });
    unsafe {
        // Safety: `bio` is still valid, we just need to signal that it is inactive.
        bssl_sys::BIO_set_init(bio, 0);
    }
    1
}

/// Asynchronous methods were invoked outside `async` context
#[derive(Debug)]
pub struct NoAsyncContext;

impl core::error::Error for NoAsyncContext {}

impl fmt::Display for NoAsyncContext {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("async method is called outside async context")
    }
}
