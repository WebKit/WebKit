// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

static int X509_OBJECT_idx_by_subject(STACK_OF(X509_OBJECT) *h, int type,
                                      const X509_NAME *name);
static X509_OBJECT *X509_OBJECT_retrieve_by_subject(STACK_OF(X509_OBJECT) *h,
                                                    int type,
                                                    const X509_NAME *name);
static X509_OBJECT *X509_OBJECT_retrieve_match(STACK_OF(X509_OBJECT) *h,
                                               X509_OBJECT *x);
static int X509_OBJECT_up_ref_count(X509_OBJECT *a);

static X509_LOOKUP *X509_LOOKUP_new(const X509_LOOKUP_METHOD *method,
                                    X509_STORE *store);
static int X509_LOOKUP_by_subject(X509_LOOKUP *ctx, int type,
                                  const X509_NAME *name, X509_OBJECT *ret);

static X509_LOOKUP *X509_LOOKUP_new(const X509_LOOKUP_METHOD *method,
                                    X509_STORE *store) {
  X509_LOOKUP *ret = New<X509_LOOKUP>();
  if (ret == nullptr) {
    return nullptr;
  }

  ret->method = method;
  ret->store_ctx = store;
  if (method->new_item != nullptr && !method->new_item(ret)) {
    Delete(ret);
    return nullptr;
  }
  return ret;
}

void X509_LOOKUP_free(X509_LOOKUP *ctx) {
  if (ctx == nullptr) {
    return;
  }
  if (ctx->method != nullptr && ctx->method->free != nullptr) {
    (*ctx->method->free)(ctx);
  }
  Delete(ctx);
}

int X509_LOOKUP_ctrl(X509_LOOKUP *ctx, int cmd, const char *argc, long argl,
                     char **ret) {
  if (ctx->method == nullptr) {
    return -1;
  }
  if (ctx->method->ctrl != nullptr) {
    return ctx->method->ctrl(ctx, cmd, argc, argl, ret);
  } else {
    return 1;
  }
}

static int X509_LOOKUP_by_subject(X509_LOOKUP *ctx, int type,
                                  const X509_NAME *name, X509_OBJECT *ret) {
  if (ctx->method == nullptr || ctx->method->get_by_subject == nullptr) {
    return 0;
  }
  // Note |get_by_subject| leaves |ret| in an inconsistent state. It has
  // pointers to an |X509| or |X509_CRL|, but has not bumped the refcount yet.
  // For now, the caller is expected to fix this, but ideally we'd fix the
  // |X509_LOOKUP| convention itself.
  return ctx->method->get_by_subject(ctx, type, name, ret) > 0;
}

// x509_object_cmp_name compares |a| against the specified type and name. This
// avoids needing to construct a reference certificate or CRL.
static int x509_object_cmp_name(const X509_OBJECT *a, int type,
                                const X509_NAME *name) {
  int ret = a->type - type;
  if (ret) {
    return ret;
  }
  switch (type) {
    case X509_LU_X509:
      return X509_NAME_cmp(X509_get_subject_name(a->data.x509), name);
    case X509_LU_CRL:
      return X509_NAME_cmp(X509_CRL_get_issuer(a->data.crl), name);
    default:
      // abort();
      return 0;
  }
}

static int x509_object_cmp(const X509_OBJECT *a, const X509_OBJECT *b) {
  switch (b->type) {
    case X509_LU_X509:
      return x509_object_cmp_name(a, b->type,
                                  X509_get_subject_name(b->data.x509));
    case X509_LU_CRL:
      return x509_object_cmp_name(a, b->type, X509_CRL_get_issuer(b->data.crl));
    default:
      // abort();
      return 0;
  }
}

static int x509_object_cmp_sk(const X509_OBJECT *const *a,
                              const X509_OBJECT *const *b) {
  return x509_object_cmp(*a, *b);
}

X509Store::X509Store()
    : RefCounted(CheckSubClass()),
      objs(sk_X509_OBJECT_new(x509_object_cmp_sk)),
      param(X509_VERIFY_PARAM_new()) {}

X509_STORE *X509_STORE_new() {
  UniquePtr<X509Store> ret(New<X509Store>());
  if (ret == nullptr || ret->objs == nullptr || ret->param == nullptr) {
    return nullptr;
  }
  return ret.release();
}

int X509_STORE_up_ref(X509_STORE *store) {
  FromOpaque(store)->UpRefInternal();
  return 1;
}

void X509_STORE_free(X509_STORE *vfy) {
  if (vfy == nullptr) {
    return;
  }
  FromOpaque(vfy)->DecRefInternal();
}

X509_LOOKUP *X509_STORE_add_lookup(X509_STORE *store,
                                   const X509_LOOKUP_METHOD *method) {
  auto *impl = FromOpaque(store);
  auto it =
      std::find_if(impl->get_cert_methods.begin(), impl->get_cert_methods.end(),
                   [&](const auto &lu) { return method == lu->method; });
  if (it != impl->get_cert_methods.end()) {
    return it->get();
  }

  UniquePtr<X509_LOOKUP> lu(X509_LOOKUP_new(method, store));
  X509_LOOKUP *lu_raw = lu.get();
  if (lu == nullptr || !impl->get_cert_methods.Push(std::move(lu))) {
    return nullptr;
  }

  return lu_raw;
}

int X509_STORE_CTX_get_by_subject(X509_STORE_CTX *vs, int type,
                                  const X509_NAME *name, X509_OBJECT *ret) {
  X509Store *ctx = FromOpaque(vs->ctx);
  X509_OBJECT stmp;
  ctx->objs_lock.LockWrite();
  X509_OBJECT *tmp =
      X509_OBJECT_retrieve_by_subject(ctx->objs.get(), type, name);
  ctx->objs_lock.UnlockWrite();

  if (tmp == nullptr || type == X509_LU_CRL) {
    for (const auto &lu : ctx->get_cert_methods) {
      if (X509_LOOKUP_by_subject(lu.get(), type, name, &stmp)) {
        tmp = &stmp;
        break;
      }
    }
    if (tmp == nullptr) {
      return 0;
    }
  }

  // TODO(crbug.com/boringssl/685): This should call
  // |X509_OBJECT_free_contents|.
  ret->type = tmp->type;
  ret->data = tmp->data;
  X509_OBJECT_up_ref_count(ret);
  return 1;
}

static int x509_store_add(X509Store *ctx, void *x, int is_crl) {
  if (x == nullptr) {
    return 0;
  }

  X509_OBJECT *const obj = X509_OBJECT_new();
  if (obj == nullptr) {
    return 0;
  }

  if (is_crl) {
    obj->type = X509_LU_CRL;
    obj->data.crl = (X509_CRL *)x;
  } else {
    obj->type = X509_LU_X509;
    obj->data.x509 = (X509 *)x;
  }
  X509_OBJECT_up_ref_count(obj);

  ctx->objs_lock.LockWrite();

  int ret = 1;
  int added = 0;
  // Duplicates are silently ignored
  if (!X509_OBJECT_retrieve_match(ctx->objs.get(), obj)) {
    ret = added = (sk_X509_OBJECT_push(ctx->objs.get(), obj) != 0);
  }

  ctx->objs_lock.UnlockWrite();

  if (!added) {
    X509_OBJECT_free(obj);
  }

  return ret;
}

int X509_STORE_add_cert(X509_STORE *ctx, X509 *x) {
  return x509_store_add(FromOpaque(ctx), x, /*is_crl=*/0);
}

int X509_STORE_add_crl(X509_STORE *ctx, X509_CRL *x) {
  return x509_store_add(FromOpaque(ctx), x, /*is_crl=*/1);
}

X509_OBJECT *X509_OBJECT_new() { return New<X509_OBJECT>(); }

void X509_OBJECT_free(X509_OBJECT *obj) {
  if (obj == nullptr) {
    return;
  }
  X509_OBJECT_free_contents(obj);
  Delete(obj);
}

static int X509_OBJECT_up_ref_count(X509_OBJECT *a) {
  switch (a->type) {
    case X509_LU_X509:
      X509_up_ref(a->data.x509);
      break;
    case X509_LU_CRL:
      X509_CRL_up_ref(a->data.crl);
      break;
  }
  return 1;
}

void X509_OBJECT_free_contents(X509_OBJECT *a) {
  switch (a->type) {
    case X509_LU_X509:
      X509_free(a->data.x509);
      break;
    case X509_LU_CRL:
      X509_CRL_free(a->data.crl);
      break;
  }

  OPENSSL_memset(a, 0, sizeof(X509_OBJECT));
}

int X509_OBJECT_get_type(const X509_OBJECT *a) { return a->type; }

X509 *X509_OBJECT_get0_X509(const X509_OBJECT *a) {
  if (a == nullptr || a->type != X509_LU_X509) {
    return nullptr;
  }
  return a->data.x509;
}

static int x509_object_idx_cnt(STACK_OF(X509_OBJECT) *h, int type,
                               const X509_NAME *name, int *out_num_match) {
  sk_X509_OBJECT_sort(h);

  // Find the first matching object. |sk_X509_OBJECT_find| would require
  // constructing an |X509| or |X509_CRL| object, so implement our own binary
  // search.
  size_t start = 0, end = sk_X509_OBJECT_num(h);
  while (end - start > 1) {
    // Bias |mid| towards |start|. The range has more than one element, so |mid|
    // is not the last element.
    size_t mid = start + (end - start - 1) / 2;
    assert(start <= mid && mid + 1 < end);
    int r = x509_object_cmp_name(sk_X509_OBJECT_value(h, mid), type, name);
    if (r < 0) {
      start = mid + 1;  // |mid| is too low.
    } else if (r > 0) {
      end = mid;  // |mid| is too high.
    } else {
      // |mid| matches, but we need to keep searching to find the first match.
      end = mid + 1;
    }
  }
  if (start == end ||
      x509_object_cmp_name(sk_X509_OBJECT_value(h, start), type, name) != 0) {
    return -1;
  }

  if (out_num_match != nullptr) {
    *out_num_match = 1;
    for (size_t tidx = start + 1; tidx < sk_X509_OBJECT_num(h); tidx++) {
      const X509_OBJECT *tobj = sk_X509_OBJECT_value(h, tidx);
      if (x509_object_cmp_name(tobj, type, name) != 0) {
        break;
      }
      (*out_num_match)++;
    }
  }

  assert(start <= INT_MAX);  // |STACK_OF(T)| never stores more than |INT_MAX|.
  return static_cast<int>(start);
}

static int X509_OBJECT_idx_by_subject(STACK_OF(X509_OBJECT) *h, int type,
                                      const X509_NAME *name) {
  return x509_object_idx_cnt(h, type, name, nullptr);
}

static X509_OBJECT *X509_OBJECT_retrieve_by_subject(STACK_OF(X509_OBJECT) *h,
                                                    int type,
                                                    const X509_NAME *name) {
  int idx;
  idx = X509_OBJECT_idx_by_subject(h, type, name);
  if (idx == -1) {
    return nullptr;
  }
  return sk_X509_OBJECT_value(h, idx);
}

static X509_OBJECT *x509_object_dup(const X509_OBJECT *obj) {
  X509_OBJECT *ret = X509_OBJECT_new();
  if (ret == nullptr) {
    return nullptr;
  }
  ret->type = obj->type;
  ret->data = obj->data;
  X509_OBJECT_up_ref_count(ret);
  return ret;
}

STACK_OF(X509_OBJECT) *X509_STORE_get1_objects(X509_STORE *store) {
  auto *impl = FromOpaque(store);
  MutexReadLock lock(&impl->objs_lock);
  return sk_X509_OBJECT_deep_copy(impl->objs.get(), x509_object_dup,
                                  X509_OBJECT_free);
}

STACK_OF(X509_OBJECT) *X509_STORE_get0_objects(X509_STORE *store) {
  auto *impl = FromOpaque(store);
  return impl->objs.get();
}

STACK_OF(X509) *X509_STORE_CTX_get1_certs(X509_STORE_CTX *ctx,
                                          const X509_NAME *nm) {
  int cnt;
  STACK_OF(X509) *sk = sk_X509_new_null();
  if (sk == nullptr) {
    return nullptr;
  }
  X509Store *store = FromOpaque(ctx->ctx);
  store->objs_lock.LockWrite();
  int idx = x509_object_idx_cnt(store->objs.get(), X509_LU_X509, nm, &cnt);
  if (idx < 0) {
    // Nothing found in cache: do lookup to possibly add new objects to
    // cache
    X509_OBJECT xobj;
    store->objs_lock.UnlockWrite();
    if (!X509_STORE_CTX_get_by_subject(ctx, X509_LU_X509, nm, &xobj)) {
      sk_X509_free(sk);
      return nullptr;
    }
    X509_OBJECT_free_contents(&xobj);
    store->objs_lock.LockWrite();
    idx = x509_object_idx_cnt(store->objs.get(), X509_LU_X509, nm, &cnt);
    if (idx < 0) {
      store->objs_lock.UnlockWrite();
      sk_X509_free(sk);
      return nullptr;
    }
  }
  for (int i = 0; i < cnt; i++, idx++) {
    X509_OBJECT *obj = sk_X509_OBJECT_value(store->objs.get(), idx);
    X509 *x = obj->data.x509;
    if (!sk_X509_push(sk, x)) {
      store->objs_lock.UnlockWrite();
      sk_X509_pop_free(sk, X509_free);
      return nullptr;
    }
    X509_up_ref(x);
  }
  store->objs_lock.UnlockWrite();
  return sk;
}

STACK_OF(X509_CRL) *X509_STORE_CTX_get1_crls(X509_STORE_CTX *ctx,
                                             const X509_NAME *nm) {
  int cnt;
  X509_OBJECT xobj;
  STACK_OF(X509_CRL) *sk = sk_X509_CRL_new_null();
  if (sk == nullptr) {
    return nullptr;
  }

  // Always do lookup to possibly add new CRLs to cache.
  if (!X509_STORE_CTX_get_by_subject(ctx, X509_LU_CRL, nm, &xobj)) {
    sk_X509_CRL_free(sk);
    return nullptr;
  }
  X509_OBJECT_free_contents(&xobj);
  X509Store *store = FromOpaque(ctx->ctx);
  store->objs_lock.LockWrite();
  int idx = x509_object_idx_cnt(store->objs.get(), X509_LU_CRL, nm, &cnt);
  if (idx < 0) {
    store->objs_lock.UnlockWrite();
    sk_X509_CRL_free(sk);
    return nullptr;
  }

  for (int i = 0; i < cnt; i++, idx++) {
    X509_OBJECT *obj = sk_X509_OBJECT_value(store->objs.get(), idx);
    X509_CRL *x = obj->data.crl;
    X509_CRL_up_ref(x);
    if (!sk_X509_CRL_push(sk, x)) {
      store->objs_lock.UnlockWrite();
      X509_CRL_free(x);
      sk_X509_CRL_pop_free(sk, X509_CRL_free);
      return nullptr;
    }
  }
  store->objs_lock.UnlockWrite();
  return sk;
}

static X509_OBJECT *X509_OBJECT_retrieve_match(STACK_OF(X509_OBJECT) *h,
                                               X509_OBJECT *x) {
  sk_X509_OBJECT_sort(h);
  size_t idx;
  if (!sk_X509_OBJECT_find(h, &idx, x)) {
    return nullptr;
  }
  if ((x->type != X509_LU_X509) && (x->type != X509_LU_CRL)) {
    return sk_X509_OBJECT_value(h, idx);
  }
  for (size_t i = idx; i < sk_X509_OBJECT_num(h); i++) {
    X509_OBJECT *obj = sk_X509_OBJECT_value(h, i);
    if (x509_object_cmp(obj, x)) {
      return nullptr;
    }
    if (x->type == X509_LU_X509) {
      if (!X509_cmp(obj->data.x509, x->data.x509)) {
        return obj;
      }
    } else if (x->type == X509_LU_CRL) {
      if (!X509_CRL_match(obj->data.crl, x->data.crl)) {
        return obj;
      }
    } else {
      return obj;
    }
  }
  return nullptr;
}

int X509_STORE_CTX_get1_issuer(X509 **out_issuer, X509_STORE_CTX *ctx,
                               const X509 *x) {
  X509_OBJECT obj;
  X509_NAME *xn = X509_get_issuer_name(x);
  if (!X509_STORE_CTX_get_by_subject(ctx, X509_LU_X509, xn, &obj)) {
    return 0;
  }
  // If certificate matches all OK
  if (x509_check_issued_with_callback(ctx, x, obj.data.x509)) {
    *out_issuer = obj.data.x509;
    return 1;
  }
  X509_OBJECT_free_contents(&obj);

  // Else find index of first cert accepted by
  // |x509_check_issued_with_callback|.
  X509Store *store = FromOpaque(ctx->ctx);
  MutexWriteLock lock(&store->objs_lock);
  int idx = X509_OBJECT_idx_by_subject(store->objs.get(), X509_LU_X509, xn);
  if (idx != -1) {  // should be true as we've had at least one match
    // Look through all matching certs for suitable issuer
    for (size_t i = idx; i < sk_X509_OBJECT_num(store->objs.get()); i++) {
      X509_OBJECT *pobj = sk_X509_OBJECT_value(store->objs.get(), i);
      // See if we've run past the matches.
      //
      // This works because the objects are sorted by type, then subject
      // name, using |x509_object_cmp|.
      if (pobj->type != X509_LU_X509) {
        return 0;
      }
      if (X509_NAME_cmp(xn, X509_get_subject_name(pobj->data.x509))) {
        return 0;
      }
      if (x509_check_issued_with_callback(ctx, x, pobj->data.x509)) {
        *out_issuer = pobj->data.x509;
        X509_OBJECT_up_ref_count(pobj);
        return 1;
      }
    }
  }
  return 0;
}

int X509_STORE_set_flags(X509_STORE *ctx, unsigned long flags) {
  auto *impl = FromOpaque(ctx);
  return X509_VERIFY_PARAM_set_flags(impl->param.get(), flags);
}

int X509_STORE_set_depth(X509_STORE *ctx, int depth) {
  auto *impl = FromOpaque(ctx);
  X509_VERIFY_PARAM_set_depth(impl->param.get(), depth);
  return 1;
}

int X509_STORE_set_purpose(X509_STORE *ctx, int purpose) {
  auto *impl = FromOpaque(ctx);
  return X509_VERIFY_PARAM_set_purpose(impl->param.get(), purpose);
}

int X509_STORE_set_trust(X509_STORE *ctx, int trust) {
  auto *impl = FromOpaque(ctx);
  return X509_VERIFY_PARAM_set_trust(impl->param.get(), trust);
}

int X509_STORE_set1_param(X509_STORE *ctx, const X509_VERIFY_PARAM *param) {
  auto *impl = FromOpaque(ctx);
  return X509_VERIFY_PARAM_set1(impl->param.get(), param);
}

X509_VERIFY_PARAM *X509_STORE_get0_param(X509_STORE *ctx) {
  return FromOpaque(ctx)->param.get();
}

void X509_STORE_set_verify_cb(X509_STORE *ctx,
                              X509_STORE_CTX_verify_cb verify_cb) {
  auto *impl = FromOpaque(ctx);
  impl->verify_cb = verify_cb;
}

X509_STORE *X509_STORE_CTX_get0_store(const X509_STORE_CTX *ctx) {
  return ctx->ctx;
}
