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

#ifndef OPENSSL_HEADER_CRYPTO_POOL_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_POOL_INTERNAL_H

#include "../internal.h"
#include "../lhash/internal.h"
#include "../mem_internal.h"


DECLARE_OPAQUE_STRUCT(crypto_buffer_st, CryptoBuffer)
DECLARE_OPAQUE_STRUCT(crypto_buffer_pool_st, CryptoBufferPool)

BSSL_NAMESPACE_BEGIN

// A CryptoBufferPoolHandle is the portion of the pool that lasts as long as any
// live buffer or pool. This allows buffers to outlive the pool. (The pool is
// only needed as long as callers wish to create new buffers.)
class CryptoBufferPoolHandle : public RefCounted<CryptoBufferPoolHandle> {
 public:
  explicit CryptoBufferPoolHandle(CryptoBufferPool *pool)
      : RefCounted(CheckSubClass()), pool_(pool) {}

  // pool_ is protected by lock_.
  Mutex lock_;
  CryptoBufferPool *pool_ = nullptr;

 private:
  friend RefCounted;
  ~CryptoBufferPoolHandle() = default;
};

class CryptoBuffer : public crypto_buffer_st {
 public:
  CryptoBuffer() = default;
  CryptoBuffer(const CryptoBuffer &) = delete;
  CryptoBuffer &operator=(const CryptoBuffer &) = delete;

  Span<const uint8_t> span() const { return Span(data_, len_); }

  // Instead of subclassing RefCounted<T>, implement refcounting by hand.
  // CryptoBuffer's refcounting must synchronize with CryptoBufferPool.
  static constexpr bool kAllowRefCountedUniquePtr = true;
  void UpRefInternal() const;
  void DecRefInternal();

  UniquePtr<CryptoBufferPoolHandle> pool_handle_;
  uint8_t *data_ = nullptr;
  size_t len_ = 0;
  mutable CRYPTO_refcount_t references_ = 1;
  bool data_is_static_ = false;

 private:
  ~CryptoBuffer();
};

DEFINE_LHASH_OF(CryptoBuffer)

class CryptoBufferPool : public crypto_buffer_pool_st,
                         public RefCounted<CryptoBufferPool> {
 public:
  CryptoBufferPool();

  // Hash returns the hash of `data`.
  uint32_t Hash(Span<const uint8_t> data) const;

  // FindBufferLocked looks for a buffer with hash `hash` and contents `data`.
  // It returns it if found and nullptr otherwise. `handle_->lock_` must be
  // locked for reading or writing before calling this.
  CryptoBuffer *FindBufferLocked(uint32_t hash, Span<const uint8_t> data);

  UniquePtr<CryptoBufferPoolHandle> handle_;
  LHASH_OF(CryptoBuffer) *bufs_ = nullptr;
  uint64_t hash_key_[2];

 private:
  friend RefCounted;
  ~CryptoBufferPool();
};

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_POOL_INTERNAL_H
