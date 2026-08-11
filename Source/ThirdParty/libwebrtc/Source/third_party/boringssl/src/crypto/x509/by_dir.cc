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

#include <inttypes.h>
#include <string.h>

#include <algorithm>
#include <string_view>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

// A ByDirEntry tracks state for a single directory, notably the starting suffix
// for CRL lookups.
class ByDirEntry {
 public:
  static constexpr bool kAllowUniquePtr = true;

  ByDirEntry() = default;

  static UniquePtr<ByDirEntry> Create(int dir_type, std::string_view dir) {
    auto ret = MakeUnique<ByDirEntry>();
    ret->dir_type_ = dir_type;
    ret->dir_.reset(OPENSSL_strndup(dir.data(), dir.size()));
    if (ret->dir_ == nullptr) {
      return nullptr;
    }
    return ret;
  }

  int dir_type() const { return dir_type_; }
  const char *dir() const { return dir_.get(); }

  int GetCRLSuffix(uint32_t hash) const {
    MutexReadLock lock(&lock_);
    auto it = std::lower_bound(crl_suffixes_.begin(), crl_suffixes_.end(), hash);
    if (it == crl_suffixes_.end() || it->hash != hash) {
      return 0;
    }
    return it->suffix;
  }

  bool UpdateCRLSuffix(uint32_t hash, int suffix) {
    MutexWriteLock lock(&lock_);
    auto it = std::lower_bound(crl_suffixes_.begin(), crl_suffixes_.end(), hash);
    if (it != crl_suffixes_.end() && it->hash == hash) {
      it->suffix = std::max(suffix, it->suffix);
      return true;
    }
    if (!crl_suffixes_.Push(CRLSuffix{hash, suffix})) {
      return false;
    }
    std::sort(crl_suffixes_.begin(), crl_suffixes_.end());
    return true;
  }

 private:
  struct CRLSuffix {
    uint32_t hash;
    int suffix;
    bool operator<(uint32_t h) const { return hash < h; }
    bool operator<(const CRLSuffix &other) const { return hash < other.hash; }
  };

  UniquePtr<char> dir_;
  int dir_type_ = 0;
  mutable Mutex lock_;
  // crl_suffixes_ is kept sorted.
  // TODO(davidben): This should be a hash table. Insertions are O(N log N).
  Vector<CRLSuffix> crl_suffixes_;
};

struct ByDir {
  Vector<UniquePtr<ByDirEntry>> dirs;
};

static int dir_ctrl(X509_LOOKUP *ctx, int cmd, const char *argp, long argl,
                    char **ret);
static int new_dir(X509_LOOKUP *lu);
static void free_dir(X509_LOOKUP *lu);
static int add_cert_dir(ByDir *ctx, const char *dir, int type);
static int get_cert_by_subject(X509_LOOKUP *xl, int type, const X509_NAME *name,
                               X509_OBJECT *ret);
static const X509_LOOKUP_METHOD x509_dir_lookup = {
    new_dir,              // new
    free_dir,             // free
    dir_ctrl,             // ctrl
    get_cert_by_subject,  // get_by_subject
};

static int dir_ctrl(X509_LOOKUP *ctx, int cmd, const char *argp, long argl,
                    char **retp) {
  ByDir *ld = reinterpret_cast<ByDir *>(ctx->method_data);
  switch (cmd) {
    case X509_L_ADD_DIR:
      if (argl == X509_FILETYPE_DEFAULT) {
        const char *dir = getenv(X509_get_default_cert_dir_env());
        if (!add_cert_dir(ld, dir ? dir : X509_get_default_cert_dir(),
                          X509_FILETYPE_PEM)) {
          OPENSSL_PUT_ERROR(X509, X509_R_LOADING_CERT_DIR);
          return 0;
        }
        return 1;
      }
      return add_cert_dir(ld, argp, (int)argl);
  }
  return 0;
}

static int new_dir(X509_LOOKUP *lu) {
  ByDir *a = New<ByDir>();
  if (a == nullptr) {
    return 0;
  }
  lu->method_data = a;
  return 1;
}

static void free_dir(X509_LOOKUP *lu) {
  Delete(reinterpret_cast<ByDir *>(lu->method_data));
}

#if defined(OPENSSL_WINDOWS)
#define DIR_HASH_SEPARATOR ';'
#else
#define DIR_HASH_SEPARATOR ':'
#endif

static int add_cert_dir(ByDir *ctx, const char *inp, int type) {
  if (inp == nullptr || !*inp) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_DIRECTORY);
    return 0;
  }

  std::string_view rest = inp;
  do {
    // Split by `DIR_HASH_SEPARATOR`.
    size_t sep = rest.find(DIR_HASH_SEPARATOR);
    std::string_view dir;
    if (sep == std::string_view::npos) {
      dir = rest;
      rest = std::string_view();
    } else {
      dir = rest.substr(0, sep);
      rest = rest.substr(sep + 1);
    }
    if (dir.empty()) {
      continue;
    }
    // Ignore duplicates.
    if (std::any_of(ctx->dirs.begin(), ctx->dirs.end(),
                    [&](const auto &ent) { return ent->dir() == dir; })) {
      continue;
    }
    auto ent = ByDirEntry::Create(type, dir);
    if (ent == nullptr || !ctx->dirs.Push(std::move(ent))) {
      return 0;
    }
  } while (!rest.empty());
  return 1;
}

static int get_cert_by_subject(X509_LOOKUP *xl, int type, const X509_NAME *name,
                               X509_OBJECT *ret) {
  if (name == nullptr) {
    return 0;
  }

  // Set up an |X509_OBJECT| to compare against.
  UniquePtr<X509> lookup_cert;
  UniquePtr<X509_CRL> lookup_crl;
  X509_OBJECT stmp;
  const char *postfix = "";
  stmp.type = type;
  ByDir *ctx = reinterpret_cast<ByDir *>(xl->method_data);
  if (type == X509_LU_X509) {
    lookup_cert.reset(X509_new());
    if (lookup_cert == nullptr ||
        !X509_set_subject_name(lookup_cert.get(), name)) {
      return 0;
    }
    stmp.data.x509 = lookup_cert.get();
    postfix = "";
  } else if (type == X509_LU_CRL) {
    lookup_crl.reset(X509_CRL_new());
    if (lookup_crl == nullptr ||
        !X509_CRL_set_issuer_name(lookup_crl.get(), name)) {
      return 0;
    }
    stmp.data.crl = lookup_crl.get();
    postfix = "r";
  } else {
    OPENSSL_PUT_ERROR(X509, X509_R_WRONG_LOOKUP_TYPE);
    return 0;
  }

  // Try both new and old hashes.
  const uint32_t hashes[] = {X509_NAME_hash(name), X509_NAME_hash_old(name)};
  for (uint32_t hash : hashes) {
    for (UniquePtr<ByDirEntry> &ent : ctx->dirs) {
      // If a CRL, start from the previously saved suffix. Updated CRLs are
      // expected to be added until new filenames.
      // TODO(crbug.com/42290566): Is this what we want?
      int suffix = 0;
      if (type == X509_LU_CRL) {
        suffix = ent->GetCRLSuffix(hash);
      }

      // The directory format handles hash collections by incrementing a suffix
      // on the file name. Load every suffix into the cache.
      for (;;) {
        char *path = nullptr;
        if (OPENSSL_asprintf(&path, "%s/%08" PRIx32 ".%s%d", ent->dir(), hash,
                             postfix, suffix) == -1) {
          OPENSSL_PUT_ERROR(X509, ERR_R_BUF_LIB);
          return 0;
        }
        UniquePtr<char> free_path(path);
        if (type == X509_LU_X509) {
          if ((X509_load_cert_file(xl, path, ent->dir_type())) == 0) {
            // Don't expose the lower level error, All of these boil down to "we
            // could not find a CA".
            ERR_clear_error();
            break;
          }
        } else if (type == X509_LU_CRL) {
          if ((X509_load_crl_file(xl, path, ent->dir_type())) == 0) {
            // Don't expose the lower level error, All of these boil down to "we
            // could not find a CRL".
            ERR_clear_error();
            break;
          }
        }
        // The lack of a CA or CRL will be caught higher up.
        suffix++;
      }

      // We have added it to the cache so now pull it out again.
      auto *store_impl = FromOpaque(xl->store_ctx);
      store_impl->objs_lock.LockWrite();
      const X509_OBJECT *found = nullptr;
      sk_X509_OBJECT_sort(store_impl->objs.get());
      size_t idx;
      if (sk_X509_OBJECT_find(store_impl->objs.get(), &idx, &stmp)) {
        found = sk_X509_OBJECT_value(store_impl->objs.get(), idx);
      }
      store_impl->objs_lock.UnlockWrite();

      // If a CRL, store the last suffix we saw, to skip already loaded files
      // next time.
      // TODO(crbug.com/42290566): Is this what we want?
      if (type == X509_LU_CRL && !ent->UpdateCRLSuffix(hash, suffix)) {
        return 0;
      }

      if (found != nullptr) {
        // Clear any errors that might have been raised processing empty or
        // malformed files.
        ERR_clear_error();

        // TODO(crbug.com/42290561): This should manage the reference counts
        // correctly but does not.
        ret->type = found->type;
        OPENSSL_memcpy(&ret->data, &found->data, sizeof(ret->data));
        return 1;
      }
    }
  }

  return 0;
}

BSSL_NAMESPACE_END

const X509_LOOKUP_METHOD *X509_LOOKUP_hash_dir() {
  return &bssl::x509_dir_lookup;
}

int X509_LOOKUP_add_dir(X509_LOOKUP *lookup, const char *name, int type) {
  return X509_LOOKUP_ctrl(lookup, X509_L_ADD_DIR, name, type, nullptr);
}
