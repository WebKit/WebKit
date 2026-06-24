// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/bn.h>

#include <assert.h>
#include <string.h>

#include <utility>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "../../internal.h"
#include "../../mem_internal.h"


using namespace bssl;

DECLARE_OPAQUE_STRUCT(bignum_ctx, BignumCtx)

BSSL_NAMESPACE_BEGIN

class BignumCtx : public bignum_ctx {
 public:
  ~BignumCtx() {
    // All |BN_CTX_start| calls must be matched with |BN_CTX_end|, otherwise the
    // function may use more memory than expected, potentially without bound if
    // done in a loop. Assert that all |BIGNUM|s have been released.
    assert(used_ == 0 || error_);
  }

  // bignums_ is the stack of |BIGNUM|s managed by this |BN_CTX|.
  Vector<UniquePtr<BIGNUM>> bignums_;
  // stack_ is the stack of |BN_CTX_start| frames. It is the value of |used_| at
  // the time |BN_CTX_start| was called.
  Vector<size_t> stack_;
  // used_ is the number of |BIGNUM|s from |bignums_| that have been used.
  size_t used_ = 0;
  // error_ is whether any operation on this |BN_CTX| failed. All subsequent
  // operations will fail.
  bool error_ = false;
  // defer_error_ is whether an operation on this |BN_CTX| has failed, but no
  // error has been pushed to the queue yet. This is used to defer errors from
  // |BN_CTX_start| to |BN_CTX_get|.
  bool defer_error_ = false;
};

BSSL_NAMESPACE_END

BN_CTX *BN_CTX_new() { return New<BignumCtx>(); }

void BN_CTX_free(BN_CTX *ctx) {
  if (ctx != nullptr) {
    Delete(FromOpaque(ctx));
  }
}

void BN_CTX_start(BN_CTX *ctx) {
  auto *impl = FromOpaque(ctx);

  if (impl->error_) {
    // Once an operation has failed, |impl->stack| no longer matches the number
    // of |BN_CTX_end| calls to come. Do nothing.
    return;
  }

  if (!impl->stack_.Push(impl->used_)) {
    impl->error_ = true;
    // |BN_CTX_start| cannot fail, so defer the error to |BN_CTX_get|.
    impl->defer_error_ = true;
    ERR_clear_error();
  }
}

BIGNUM *BN_CTX_get(BN_CTX *ctx) {
  auto *impl = FromOpaque(ctx);

  // Once any operation has failed, they all do.
  if (impl->error_) {
    if (impl->defer_error_) {
      OPENSSL_PUT_ERROR(BN, BN_R_TOO_MANY_TEMPORARY_VARIABLES);
      impl->defer_error_ = false;
    }
    return nullptr;
  }

  if (impl->used_ == impl->bignums_.size()) {
    UniquePtr<BIGNUM> bn(BN_new());
    if (bn == nullptr || !impl->bignums_.Push(std::move(bn))) {
      OPENSSL_PUT_ERROR(BN, BN_R_TOO_MANY_TEMPORARY_VARIABLES);
      impl->error_ = true;
      return nullptr;
    }
  }

  BIGNUM *ret = impl->bignums_[impl->used_].get();
  BN_zero(ret);
  // This is bounded by |impl->bignums_.size()|, so it cannot overflow.
  impl->used_++;
  return ret;
}

void BN_CTX_end(BN_CTX *ctx) {
  auto *impl = FromOpaque(ctx);

  if (impl->error_) {
    // Once an operation has failed, |impl->stack_| no longer matches the number
    // of |BN_CTX_end| calls to come. Do nothing.
    return;
  }

  assert(!impl->stack_.empty());
  impl->used_ = impl->stack_.back();
  impl->stack_.pop_back();
}
