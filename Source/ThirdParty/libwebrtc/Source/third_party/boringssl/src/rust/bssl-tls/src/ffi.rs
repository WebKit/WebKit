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

use core::{
    iter::FusedIterator,
    marker::PhantomData,
    mem::{
        MaybeUninit,
        forget, //
    },
    ptr::{
        NonNull,
        null,
        null_mut, //
    },
    slice::{
        from_raw_parts,
        from_raw_parts_mut, //
    }, //
};

use bssl_crypto::FfiSlice;

use crate::{
    context::CertificateCache,
    errors::{
        Error,
        IoError, //
    }, //
};

pub(crate) fn slice_into_ffi_raw_parts<T>(slice: &[T]) -> (*const T, usize) {
    if slice.is_empty() {
        (null(), 0)
    } else {
        (slice.as_ptr(), slice.len())
    }
}

pub(crate) fn mut_slice_into_ffi_raw_parts<T>(slice: &mut [T]) -> (*mut T, usize) {
    if slice.is_empty() {
        (null_mut(), 0)
    } else {
        (slice.as_mut_ptr(), slice.len())
    }
}

/// Safety: use it to wrap allocation only if the pointer is
/// to be freed with `OPENSSL_free`.
pub(crate) struct Alloc<T>(pub *mut T);

impl<T> Drop for Alloc<T> {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self.0` is still valid at dropping, even if it is `NULL`.
            bssl_sys::OPENSSL_free(self.0 as _);
        }
    }
}

/// Sanitize the data pointer and length and reconstitute the slice.
///
/// This method returns an empty slice if the length is 0 or the pointer is NULL.
/// # Safety
/// Caller must ensure that `'a` outlives `input`.
#[inline]
pub(crate) unsafe fn sanitize_slice<'a, T>(input: *const T, len: usize) -> Option<&'a [T]> {
    if len == 0 || input.is_null() {
        return Some(&[]);
    }
    if !input.is_aligned() || len.checked_mul(size_of::<T>())? > isize::MAX as usize {
        return None;
    }
    unsafe {
        // Safety: the pointer and the size has been sanitised.
        Some(from_raw_parts(input, len))
    }
}

/// Sanitize the data pointer and length and reconstitute the mutable slice.
///
/// `capacity` counts the number of `T`s that `out` can hold, **not number of bytes**.
///
/// This method returns an empty slice if the length is 0 or the pointer is NULL.
/// # Safety
/// Caller must ensure that `'a` outlives `input`.
#[inline]
pub(crate) unsafe fn sanitise_mut_byteslice<'a>(
    out: *mut u8,
    capacity: usize,
) -> Option<&'a mut [u8]> {
    if capacity == 0 || out.is_null() {
        return Some(&mut []);
    }
    if capacity > isize::MAX as usize {
        return None;
    }
    unsafe {
        // Safety: `out` is 1-aligned and `0` is a valid pattern for `u8`.
        core::ptr::write_bytes(out, 0, capacity);
        Some(from_raw_parts_mut(out, capacity))
    }
}

pub(crate) fn crypto_buffer_from_buf(
    buf: &[u8],
    pool: Option<&CertificateCache>,
) -> Result<NonNull<bssl_sys::CRYPTO_BUFFER>, Error> {
    let pool = if let Some(pool) = pool {
        pool.ptr()
    } else {
        null_mut()
    };
    let (ptr, len) = slice_into_ffi_raw_parts(buf);
    let buf = unsafe {
        // Safety: `ptr` and `len` are valid and sanitised.
        bssl_sys::CRYPTO_BUFFER_new(ptr, len, pool)
    };
    let buf = NonNull::new(buf).expect("allocation failure");
    Ok(buf)
}

/// BIO wrapper only for internal use.
pub(crate) struct Bio<'a>(NonNull<bssl_sys::BIO>, PhantomData<&'a ()>);

impl<'a> Bio<'a> {
    /// # Safety
    /// Caller must ensure that the lifetime of this BIO outlives the backing object.
    /// It is strongly recommended to call the builder functions.
    pub(crate) unsafe fn new(bio: NonNull<bssl_sys::BIO>) -> Self {
        Bio(bio, PhantomData)
    }

    pub fn from_bytes(buf: &'a [u8]) -> Result<Self, Error> {
        let len = if let Ok(len) = buf.len().try_into() {
            len
        } else {
            return Err(Error::Io(IoError::TooLong));
        };
        let mem_buf = unsafe {
            // Safety: buf is still valid
            bssl_sys::BIO_new_mem_buf(buf.as_ffi_void_ptr(), len)
        };
        let mem_buf = NonNull::new(mem_buf).expect("allocation failure");
        Ok(unsafe {
            // Safety: our returned object is outlived by the input buffer.
            Self::new(mem_buf)
        })
    }

    pub fn ptr(&mut self) -> *mut bssl_sys::BIO {
        self.0.as_ptr()
    }
}

impl<'a> Drop for Bio<'a> {
    fn drop(&mut self) {
        unsafe {
            // Safety: the BIO handle should still be valid
            bssl_sys::BIO_free(self.0.as_ptr());
        }
    }
}

/// A buffer region that can be safely written to.
pub struct ReceiveBuffer<'a> {
    ptr: *mut u8,
    capacity: usize,
    cursor: usize,
    _p: PhantomData<&'a mut [u8]>,
}

impl<'a> ReceiveBuffer<'a> {
    /// Create a new receiver buffer, with uninitialised bytes.
    pub fn new_uninit(buffer: &'a mut [MaybeUninit<u8>]) -> Self {
        let (ptr, capacity) = mut_slice_into_ffi_raw_parts(buffer);
        ReceiveBuffer {
            ptr: ptr as _,
            capacity,
            cursor: 0,
            _p: PhantomData,
        }
    }

    /// Create a new receiver buffer.
    pub fn new(buffer: &'a mut [u8]) -> Self {
        let (ptr, capacity) = mut_slice_into_ffi_raw_parts(buffer);
        ReceiveBuffer {
            ptr,
            capacity,
            cursor: 0,
            _p: PhantomData,
        }
    }

    /// Return a pointer to the first byte of the unfilled bytes.
    ///
    /// **INTERNAL function only, keep it as pub(crate)**
    ///
    /// # Example for BoringSSL Authors
    ///
    /// ```ignore
    /// let mut recv_buf: ReceiveBuffer<'_>;
    /// let nr_recv = unsafe {
    ///     // Safety: ...
    ///     bssl_sys::SSL_read(ssl, recv_buf.head(), recv_buf.remaining())
    /// };
    /// assert!(nr_recv >= 0);
    /// unsafe {
    ///     // Safety: by BoringSSL contract, it is guaranteed that nr_recv bytes are filled.
    ///     recv_buf.advance(nr_recv as usize);
    /// }
    /// ```
    ///
    /// # Safety
    /// - all uses of the returned buffer must be outlived by `'a`.
    /// - all reads into the byte under the returned pointer and those [`Self::remaining`]
    ///   bytes following it must be proceeded by at least one write; otherwise, it is **undefined
    ///   behaviour**.
    pub(crate) unsafe fn head(&mut self) -> *mut u8 {
        debug_assert!(self.cursor <= self.capacity && self.cursor <= isize::MAX as usize);
        unsafe {
            // Safety: the cursor is still in-bound and the buffer is still owned by `self`.
            self.ptr.add(self.cursor)
        }
    }

    /// Advance the cursor to mark `bytes` from the [`Self::head`] as filled.
    ///
    /// **INTERNAL function only, keep it as pub(crate)**
    ///
    /// # Safety
    /// The bytes in between [`Self::head`] and `Self::head() + bytes` must have been filled
    /// by the caller before calling this method.
    pub(crate) unsafe fn advance(&mut self, bytes: usize) {
        self.cursor += bytes;
        debug_assert!(self.cursor <= self.capacity);
    }

    /// Extract a slice to the filled data.
    pub fn filled(&self) -> &[u8] {
        unsafe {
            // Safety: `self` still exclusively owns the buffer region and the range of bytes
            // is known to be initialised by us. See `advance`.
            sanitize_slice(self.ptr, self.cursor).unwrap_or(&[])
        }
    }

    /// Reports remaining capacity in the destination buffer.
    pub fn remaining(&self) -> usize {
        self.capacity - self.cursor
    }

    /// Reports written data in the destination buffer.
    pub fn written(&self) -> usize {
        self.cursor
    }
}

impl core::ops::Deref for ReceiveBuffer<'_> {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        self.filled()
    }
}

pub(crate) trait CryptoBufferWrapper {
    /// Safety: `buf` must be exclusively owned.
    unsafe fn from_crypto_buffer(buf: core::ptr::NonNull<::bssl_sys::CRYPTO_BUFFER>) -> Self;
}

pub(crate) unsafe trait BsslStack: Sized {
    type Element: StackElement;

    fn new() -> *mut Self;

    /// Safety: `this` handle must be a live `stack_st_*` handle.
    unsafe fn size(this: *const Self) -> usize;

    /// Safety: `this` handle must be live and `idx` must be in bounds.
    unsafe fn index(this: *const Self, idx: usize) -> *const Self::Element;

    /// Safety: both `this` and `elem` cannot be aliased.
    unsafe fn push(this: *mut Self, elem: *mut Self::Element);

    const POP_FREE: unsafe extern "C" fn(
        *mut Self,
        Option<unsafe extern "C" fn(*mut Self::Element)>,
    );
}

unsafe impl BsslStack for bssl_sys::stack_st_CRYPTO_BUFFER {
    type Element = bssl_sys::CRYPTO_BUFFER;

    fn new() -> *mut Self {
        let st = unsafe {
            // Safety: this call only allocates memory
            bssl_sys::sk_CRYPTO_BUFFER_new_null()
        };
        if st.is_null() {
            panic!("allocation failed")
        }
        st
    }

    unsafe fn size(this: *const Self) -> usize {
        unsafe {
            // Safety: `this` is still live and valid.
            bssl_sys::sk_CRYPTO_BUFFER_num(this)
        }
    }

    unsafe fn index(this: *const Self, idx: usize) -> *const Self::Element {
        unsafe {
            // Safety: `this` is valid and live
            bssl_sys::sk_CRYPTO_BUFFER_value(this, idx)
        }
    }

    unsafe fn push(this: *mut Self, elem: *mut bssl_sys::CRYPTO_BUFFER) {
        let rc = unsafe {
            // Safety: `this` and `elem` are exclusively owned and valid.
            bssl_sys::sk_CRYPTO_BUFFER_push(this, elem)
        };
        if rc == 0 {
            panic!("allocation failed")
        }
    }

    const POP_FREE: unsafe extern "C" fn(
        *mut Self,
        Option<unsafe extern "C" fn(*mut Self::Element)>,
    ) = bssl_sys::sk_CRYPTO_BUFFER_pop_free;
}

pub(crate) unsafe trait StackElement: Sized {
    type Stack: BsslStack<Element = Self>;

    const FREE: unsafe extern "C" fn(*mut Self);
}

unsafe impl StackElement for bssl_sys::CRYPTO_BUFFER {
    type Stack = bssl_sys::stack_st_CRYPTO_BUFFER;

    const FREE: unsafe extern "C" fn(*mut Self) = bssl_sys::CRYPTO_BUFFER_free;
}

pub(crate) struct Stack<T: StackElement> {
    inner: *mut T::Stack,
    _p: PhantomData<T>,
}

impl<T: StackElement> Drop for Stack<T> {
    fn drop(&mut self) {
        unsafe {
            // Safety: we still own the stack at this moment
            T::Stack::POP_FREE(self.inner, Some(T::FREE))
        }
    }
}

impl<T: StackElement> Stack<T> {
    pub fn new() -> Self {
        Self {
            inner: T::Stack::new(),
            _p: PhantomData,
        }
    }

    // Safety: `elem` must not alias because its ownership will be transferred.
    pub unsafe fn push(&mut self, elem: *mut T) {
        unsafe {
            // Safety: `this` is owned by the caller.
            T::Stack::push(self.inner, elem);
        }
    }

    pub fn into_raw(self) -> *mut T::Stack {
        let ptr = self.inner;
        forget(self);
        ptr
    }
}

#[derive(Clone, Copy)]
pub(crate) struct StackIterator<'a, T: StackElement> {
    sk: *const T::Stack,
    len: usize,
    curr: usize,
    _p: PhantomData<&'a fn() -> T>,
}

impl<'a, T: StackElement> StackIterator<'a, T> {
    /// Safety: caller must ensure that `sk` outlives `'a`.
    pub(crate) unsafe fn new(sk: *const T::Stack) -> Self {
        let len = if sk.is_null() {
            0
        } else {
            unsafe {
                // Safety: `sk` is valid now.
                T::Stack::size(sk)
            }
        };
        Self {
            sk,
            len,
            curr: 0,
            _p: PhantomData,
        }
    }
}

impl<'a, T: StackElement> Iterator for StackIterator<'a, T> {
    type Item = *const T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.curr >= self.len {
            return None;
        }
        let elem = unsafe {
            // Safety: `self.sk` is still valid now and `self.curr` is within the bound.
            T::Stack::index(self.sk, self.curr)
        };
        self.curr += 1;
        if elem.is_null() {
            // Fuse the iterator.
            self.curr = self.len;
            return None;
        }
        Some(elem)
    }
}

impl<T: StackElement> ExactSizeIterator for StackIterator<'_, T> {
    fn len(&self) -> usize {
        self.len - self.curr
    }
}

impl<T: StackElement> FusedIterator for StackIterator<'_, T> {}
