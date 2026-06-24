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

#include <openssl/pool.h>

#include <assert.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/siphash.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

static uint32_t CRYPTO_BUFFER_hash(const CryptoBuffer *buf) {
  // This function must be called while there is a read or write lock on the
  // pool, so it is safe to read |pool_|.
  return buf->pool_handle_->pool_->Hash(buf->span());
}

static int CRYPTO_BUFFER_cmp(const CryptoBuffer *a, const CryptoBuffer *b) {
  // Only |CRYPTO_BUFFER|s from the same pool have compatible hashes.
  assert(a->pool_handle_ != nullptr);
  assert(a->pool_handle_ == b->pool_handle_);
  return a->span() == b->span() ? 0 : 1;
}

CryptoBufferPool::CryptoBufferPool() : RefCounted(CheckSubClass()) {
  RAND_bytes(reinterpret_cast<uint8_t *>(&hash_key_), sizeof(hash_key_));
}

CryptoBufferPool::~CryptoBufferPool() {
  if (handle_) {
    MutexWriteLock lock(&handle_->lock_);
    handle_->pool_ = nullptr;
  }
  lh_CryptoBuffer_free(bufs_);
}

uint32_t CryptoBufferPool::Hash(Span<const uint8_t> data) const {
  return static_cast<uint32_t>(SIPHASH_24(hash_key_, data.data(), data.size()));
}

CryptoBuffer *CryptoBufferPool::FindBufferLocked(uint32_t hash,
                                                 Span<const uint8_t> data) {
  return lh_CryptoBuffer_retrieve_key(
      bufs_, &data, hash,
      [](const void *key_v, const CryptoBuffer *buf) -> int {
        Span<const uint8_t> key =
            *static_cast<const Span<const uint8_t> *>(key_v);
        return key == buf->span() ? 0 : 1;
      });
}

CRYPTO_BUFFER_POOL *CRYPTO_BUFFER_POOL_new() {
  auto pool = MakeUnique<CryptoBufferPool>();
  if (pool == nullptr) {
    return nullptr;
  }

  pool->bufs_ = lh_CryptoBuffer_new(CRYPTO_BUFFER_hash, CRYPTO_BUFFER_cmp);
  pool->handle_ = MakeUnique<CryptoBufferPoolHandle>(pool.get());
  if (pool->bufs_ == nullptr || pool->handle_ == nullptr) {
    return nullptr;
  }

  return pool.release();
}

void CRYPTO_BUFFER_POOL_free(CRYPTO_BUFFER_POOL *pool) {
  if (pool != nullptr) {
    FromOpaque(pool)->DecRefInternal();
  }
}

int CRYPTO_BUFFER_POOL_up_ref(CRYPTO_BUFFER_POOL *pool) {
  FromOpaque(pool)->UpRefInternal();
  return 1;
}

void CryptoBuffer::UpRefInternal() const {
  // This is safe in the case that |buf->pool| is NULL because it's just
  // standard reference counting in that case.
  //
  // This is also safe if |buf->pool| is non-NULL because, if it were racing
  // with |CRYPTO_BUFFER_free| then the two callers must have independent
  // references already and so the reference count will never hit zero.
  CRYPTO_refcount_inc(&references_);
}

void CryptoBuffer::DecRefInternal() {
  // If there is a pool, decrementing the refcount must synchronize with it.
  if (pool_handle_ == nullptr) {
    if (!CRYPTO_refcount_dec_and_test_zero(&references_)) {
      return;
    }
  } else {
    MutexWriteLock lock(&pool_handle_->lock_);
    if (!CRYPTO_refcount_dec_and_test_zero(&references_)) {
      return;
    }

    // We have an exclusive lock on the pool handle, therefore no concurrent
    // lookups can find this buffer and increment the reference count. Thus, if
    // the count is zero there are and can never be any more references and thus
    // we can free this buffer. It is possible the pool was already destroyed,
    // but it cannot be destroyed concurrently.
    //
    // Note it is possible |buf| is no longer in the pool, if it was replaced by
    // a static version. If that static version was since removed, it is even
    // possible for |found| to be NULL.
    if (CryptoBufferPool *pool = pool_handle_->pool_; pool != nullptr) {
      CryptoBuffer *found = lh_CryptoBuffer_retrieve(pool->bufs_, this);
      if (found == this) {
        found = lh_CryptoBuffer_delete(pool->bufs_, this);
        assert(found == this);
        (void)found;
      }
    }
  }

  this->~CryptoBuffer();
  OPENSSL_free(this);
}

CryptoBuffer::~CryptoBuffer() {
  if (!data_is_static_) {
    OPENSSL_free(data_);
  }
}

static UniquePtr<CryptoBuffer> crypto_buffer_new(Span<const uint8_t> data,
                                                 bool data_is_static) {
  UniquePtr<CryptoBuffer> buf = MakeUnique<CryptoBuffer>();
  if (buf == nullptr) {
    return nullptr;
  }

  if (data_is_static) {
    buf->data_ = const_cast<uint8_t *>(data.data());
    buf->data_is_static_ = true;
  } else {
    buf->data_ =
        static_cast<uint8_t *>(OPENSSL_memdup(data.data(), data.size()));
    if (!data.empty() && buf->data_ == nullptr) {
      return nullptr;
    }
  }

  buf->len_ = data.size();
  return buf;
}

static UniquePtr<CryptoBuffer> crypto_buffer_new_with_pool(
    Span<const uint8_t> data, bool data_is_static, CryptoBufferPool *pool) {
  if (pool == nullptr) {
    return crypto_buffer_new(data, data_is_static);
  }

  const uint32_t hash = pool->Hash(data);
  {
    // Look for a matching buffer in the pool.
    MutexReadLock lock(&pool->handle_->lock_);
    CryptoBuffer *duplicate = pool->FindBufferLocked(hash, data);
    if (data_is_static && duplicate != nullptr && !duplicate->data_is_static_) {
      // If the new |CRYPTO_BUFFER| would have static data, but the duplicate
      // does not, we replace the old one with the new static version.
      duplicate = nullptr;
    }
    if (duplicate != nullptr) {
      return UpRef(duplicate);
    }
  }

  UniquePtr<CryptoBuffer> buf = crypto_buffer_new(data, data_is_static);
  if (buf == nullptr) {
    return nullptr;
  }

  MutexWriteLock lock(&pool->handle_->lock_);
  CryptoBuffer *duplicate = pool->FindBufferLocked(hash, data);
  if (data_is_static && duplicate != nullptr && !duplicate->data_is_static_) {
    // If the new |CRYPTO_BUFFER| would have static data, but the duplicate does
    // not, we replace the old one with the new static version.
    duplicate = nullptr;
  }
  if (duplicate != nullptr) {
    return UpRef(duplicate);
  }

  // Insert |buf| into the pool. Note |old| may be non-NULL if a match was found
  // but ignored. |pool->bufs_| does not increment refcounts, so there is no
  // need to clean up after the replacement.
  buf->pool_handle_ = UpRef(pool->handle_);
  CryptoBuffer *old = nullptr;
  if (!lh_CryptoBuffer_insert(pool->bufs_, &old, buf.get())) {
    buf->pool_handle_ = nullptr;  // No need to synchronize with the pool.
    return nullptr;
  }
  return buf;
}

CRYPTO_BUFFER *CRYPTO_BUFFER_new(const uint8_t *data, size_t len,
                                 CRYPTO_BUFFER_POOL *pool) {
  return crypto_buffer_new_with_pool(Span(data, len), /*data_is_static=*/false,
                                     FromOpaque(pool))
      .release();
}

CRYPTO_BUFFER *CRYPTO_BUFFER_alloc(uint8_t **out_data, size_t len) {
  auto buf = MakeUnique<CryptoBuffer>();
  if (buf == nullptr) {
    return nullptr;
  }

  buf->data_ = reinterpret_cast<uint8_t *>(OPENSSL_malloc(len));
  if (len != 0 && buf->data_ == nullptr) {
    return nullptr;
  }
  buf->len_ = len;

  *out_data = buf->data_;
  return buf.release();
}

CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_CBS(const CBS *cbs,
                                          CRYPTO_BUFFER_POOL *pool) {
  return CRYPTO_BUFFER_new(CBS_data(cbs), CBS_len(cbs), pool);
}

CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_static_data_unsafe(
    const uint8_t *data, size_t len, CRYPTO_BUFFER_POOL *pool) {
  return crypto_buffer_new_with_pool(Span(data, len), /*data_is_static=*/true,
                                     FromOpaque(pool))
      .release();
}

void CRYPTO_BUFFER_free(CRYPTO_BUFFER *buf) {
  if (buf != nullptr) {
    FromOpaque(buf)->DecRefInternal();
  }
}

int CRYPTO_BUFFER_up_ref(CRYPTO_BUFFER *buf) {
  FromOpaque(buf)->UpRefInternal();
  return 1;
}

CRYPTO_BUFFER *CRYPTO_BUFFER_dup_ref(const CRYPTO_BUFFER *buf) {
  auto *buf_ = const_cast<CRYPTO_BUFFER *>(buf);
  FromOpaque(buf_)->UpRefInternal();
  return buf_;
}

const uint8_t *CRYPTO_BUFFER_data(const CRYPTO_BUFFER *buf) {
  return FromOpaque(buf)->data_;
}

size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER *buf) {
  return FromOpaque(buf)->len_;
}

void CRYPTO_BUFFER_init_CBS(const CRYPTO_BUFFER *buf, CBS *out) {
  *out = FromOpaque(buf)->span();
}
