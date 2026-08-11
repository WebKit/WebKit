// Copyright 2016 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_POOL_H
#define OPENSSL_HEADER_POOL_H

#include <openssl/base.h>  // IWYU pragma: export

#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Buffers and buffer pools.
//
// `CRYPTO_BUFFER`s are simply reference-counted, immutable byte string. A
// single `CRYPTO_BUFFER` can be referenced from multiple parts of an
// application without storing multiple copies of the underlying data in
// memory.
//
// A `CRYPTO_BUFFER_POOL` can be used to additionally deduplicate
// `CRYPTO_BUFFER`s on construction. It maintains weak references to associated
// `CRYPTO_BUFFER`s and returns an existing `CRYPTO_BUFFER` if matching ones
// already exist.
//
// Without a `CRYPTO_BUFFER_POOL`, if two parts of application construct an
// identical `CRYPTO_BUFFER` (e.g. if two TLS connections received the same
// certificate), there will be two copies of the buffer in memory. A
// `CRYPTO_BUFFER_POOL` allows two parts of an application that construct
// `CRYPTO_BUFFER`s to deduplicate their contents, at the cost of more thread
// contention.


DEFINE_STACK_OF(CRYPTO_BUFFER)

// CRYPTO_BUFFER_POOL_new returns a freshly allocated `CRYPTO_BUFFER_POOL` or
// NULL on error.
OPENSSL_EXPORT CRYPTO_BUFFER_POOL *CRYPTO_BUFFER_POOL_new(void);

// CRYPTO_BUFFER_POOL_free decrements the reference count of `pool` and frees it
// if the reference count drops to zero.
OPENSSL_EXPORT void CRYPTO_BUFFER_POOL_free(CRYPTO_BUFFER_POOL *pool);

// CRYPTO_BUFFER_POOL_up_ref increments the reference count of `pool` and
// returns one. It does not mutate `pool` for thread-safety purposes and may be
// used concurrently.
OPENSSL_EXPORT int CRYPTO_BUFFER_POOL_up_ref(CRYPTO_BUFFER_POOL *pool);

// CRYPTO_BUFFER_new returns a `CRYPTO_BUFFER` containing a copy of `data`, or
// else NULL on error. If `pool` is not NULL then the returned value may be a
// reference to a previously existing `CRYPTO_BUFFER` that contained the same
// data. Otherwise, the returned, fresh `CRYPTO_BUFFER` will be added to the
// pool.
//
// There is no requirement that `pool` outlive the `CRYPTO_BUFFER`, or vice
// versa. If the `CRYPTO_BUFFER` is released first, it will be removed from
// `pool`. If `pool` is released first, the `CRYPTO_BUFFER` remains valid.
OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new(const uint8_t *data, size_t len,
                                                CRYPTO_BUFFER_POOL *pool);

// CRYPTO_BUFFER_alloc creates an unpooled `CRYPTO_BUFFER` of the given size and
// writes the underlying data pointer to `*out_data`. It returns NULL on error.
//
// After calling this function, `len` bytes of contents must be written to
// `out_data` before passing the returned pointer to any other BoringSSL
// functions. Once initialized, the `CRYPTO_BUFFER` should be treated as
// immutable.
OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_alloc(uint8_t **out_data,
                                                  size_t len);

// CRYPTO_BUFFER_new_from_CBS acts the same as `CRYPTO_BUFFER_new`.
OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_CBS(
    const CBS *cbs, CRYPTO_BUFFER_POOL *pool);

// CRYPTO_BUFFER_new_from_static_data_unsafe behaves like `CRYPTO_BUFFER_new`
// but does not copy `data`. `data` must be immutable and last for the lifetime
// of the address space.
OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_static_data_unsafe(
    const uint8_t *data, size_t len, CRYPTO_BUFFER_POOL *pool);

// CRYPTO_BUFFER_free decrements the reference count of `buf`. If there are no
// other references, or if the only remaining reference is from a pool, then
// `buf` will be freed.
OPENSSL_EXPORT void CRYPTO_BUFFER_free(CRYPTO_BUFFER *buf);

// CRYPTO_BUFFER_up_ref increments the reference count of `buf` and returns
// one.
OPENSSL_EXPORT int CRYPTO_BUFFER_up_ref(CRYPTO_BUFFER *buf);

// CRYPTO_BUFFER_dup_ref increments the reference count of `buf` and returns
// `buf`. The caller must call `CRYPTO_BUFFER_free` on the result to release the
// reference.
OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_dup_ref(const CRYPTO_BUFFER *buf);

// CRYPTO_BUFFER_data returns a pointer to the data contained in `buf`.
OPENSSL_EXPORT const uint8_t *CRYPTO_BUFFER_data(const CRYPTO_BUFFER *buf);

// CRYPTO_BUFFER_len returns the length, in bytes, of the data contained in
// `buf`.
OPENSSL_EXPORT size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER *buf);

// CRYPTO_BUFFER_init_CBS initialises `out` to point at the data from `buf`.
OPENSSL_EXPORT void CRYPTO_BUFFER_init_CBS(const CRYPTO_BUFFER *buf, CBS *out);


#if defined(__cplusplus)
}  // extern C

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(CRYPTO_BUFFER_POOL, CRYPTO_BUFFER_POOL_free)
BORINGSSL_MAKE_UP_REF(CRYPTO_BUFFER_POOL, CRYPTO_BUFFER_POOL_up_ref)
BORINGSSL_MAKE_DELETER(CRYPTO_BUFFER, CRYPTO_BUFFER_free)
BORINGSSL_MAKE_UP_REF(CRYPTO_BUFFER, CRYPTO_BUFFER_up_ref)

BSSL_NAMESPACE_END

}  // extern C++

#endif

#endif  // OPENSSL_HEADER_POOL_H
