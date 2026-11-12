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

#include <limits.h>
#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/sha2.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

// CRL score values

// No unhandled critical extensions
#define CRL_SCORE_NOCRITICAL 0x100

// certificate is within CRL scope
#define CRL_SCORE_SCOPE 0x080

// CRL times valid
#define CRL_SCORE_TIME 0x040

// Issuer name matches certificate
#define CRL_SCORE_ISSUER_NAME 0x020

// If this score or above CRL is probably valid
#define CRL_SCORE_VALID \
  (CRL_SCORE_NOCRITICAL | CRL_SCORE_TIME | CRL_SCORE_SCOPE)

// CRL issuer is certificate issuer
#define CRL_SCORE_ISSUER_CERT 0x018

// CRL issuer is on certificate path
#define CRL_SCORE_SAME_PATH 0x008

// CRL issuer matches CRL AKID
#define CRL_SCORE_AKID 0x004

namespace {

ExDataClass g_ex_data_class(/*with_app_data=*/true);

struct CRLInfo {
  UniquePtr<X509_CRL> crl;
  X509 *issuer = nullptr;
  int score = 0;
};

int null_callback(int ok, X509_STORE_CTX *e) { return ok; }

// cert_self_signed checks if `x` is self-signed. If `x` is valid, it returns
// one and sets `*out_is_self_signed` to the result. If `x` is invalid, it
// returns zero.
int cert_self_signed(X509 *x, int *out_is_self_signed) {
  if (!x509v3_cache_extensions(x)) {
    return 0;
  }
  auto *impl = FromOpaque(x);
  *out_is_self_signed = (impl->ex_flags & EXFLAG_SS) != 0;
  return 1;
}

int call_verify_cb(int ok, X509_STORE_CTX *ctx) {
  ok = ctx->verify_cb(ok, ctx);
  // Historically, callbacks returning values like -1 would be treated as a mix
  // of success or failure. Insert that callers check correctly.
  //
  // TODO(davidben): Also use this wrapper to constrain which errors may be
  // suppressed, and ensure all `verify_cb` calls remember to fill in an error.
  BSSL_CHECK(ok == 0 || ok == 1);
  return ok;
}

// TODO(crbug.com/42290549): Switch remaining `call_verify_cb` calls to this
// function.
int call_verify_cb_at_depth(int ok, X509_STORE_CTX *ctx, int depth) {
  ctx->current_cert = sk_X509_value(ctx->chain, depth);
  ctx->error_depth = depth;
  return call_verify_cb(ok, ctx);
}

// Given a certificate try and find an exact match in the store
UniquePtr<X509> lookup_cert_match(X509_STORE_CTX *ctx, const X509 *x) {
  if (ctx->trusted_stack != nullptr) {
    // Ignore the store and use the configured stack instead.
    for (X509 *found : ctx->trusted_stack) {
      if (X509_cmp(found, x) == 0) {
        return UpRef(found);
      }
    }
    return nullptr;
  }

  // Lookup all certs with matching subject name.
  UniquePtr<STACK_OF(X509)> certs(
      X509_STORE_CTX_get1_certs(ctx, X509_get_subject_name(x)));
  if (certs == nullptr) {
    return nullptr;
  }
  // Look for exact match
  for (X509 *found : certs.get()) {
    if (X509_cmp(found, x) == 0) {
      return UpRef(found);
    }
  }
  return nullptr;
}

// Given a STACK_OF(X509) find the issuer of cert (if any)
X509 *find_issuer(X509_STORE_CTX *ctx, const STACK_OF(X509) *sk,
                  const X509 *x) {
  for (X509 *issuer : sk) {
    if (x509_check_issued_with_callback(ctx, x, issuer)) {
      return issuer;
    }
  }
  return nullptr;
}

UniquePtr<X509> get_trusted_issuer(X509_STORE_CTX *ctx, const X509 *x) {
  if (ctx->trusted_stack != nullptr) {
    // Ignore the store and use the configured stack instead.
    return UpRef(find_issuer(ctx, ctx->trusted_stack, x));
  }

  X509 *issuer = nullptr;
  if (!X509_STORE_CTX_get1_issuer(&issuer, ctx, x)) {
    return nullptr;
  }
  // `X509_STORE_CTX_get1_issuer` passes ownership of `issuer`.
  return UniquePtr<X509>(issuer);
}

// Check a certificate chains extensions for consistency with the supplied
// purpose
int check_chain_extensions(X509_STORE_CTX *ctx) {
  int plen = 0;
  int purpose = ctx->param->purpose;
  int num = static_cast<int>(sk_X509_num(ctx->chain));
  for (int i = 0; i < num; i++) {
    X509Impl *x = FromOpaque(sk_X509_value(ctx->chain, i));
    if (!(ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL) &&
        (x->ex_flags & EXFLAG_CRITICAL)) {
      ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION;
      if (!call_verify_cb_at_depth(0, ctx, i)) {
        return 0;
      }
    }

    int must_be_ca = i > 0;
    if (must_be_ca && !X509_check_ca(x)) {
      ctx->error = X509_V_ERR_INVALID_CA;
      if (!call_verify_cb_at_depth(0, ctx, i)) {
        return 0;
      }
    }
    if (ctx->param->purpose > 0 &&
        X509_check_purpose(x, purpose, must_be_ca) != 1) {
      ctx->error = X509_V_ERR_INVALID_PURPOSE;
      if (!call_verify_cb_at_depth(0, ctx, i)) {
        return 0;
      }
    }
    // Check path length constraints. See steps (l) and (m) of RFC 5280,
    // section 6.1.4. Note the spec is structured differently from this
    // logic. Section 6.1.4 runs from root to leaf and does not run on
    // the leaf. `plen` counts the number of times step (l) would have
    // run. The constraint is violated if some `x->ex_pathlen`, read in
    // step (m), is too low to be decremented `plen` times.
    //
    // Note that path lengths of self-issued certificates still have to be
    // considered - they are just not counted as part of the path length!
    if (i > 1 && x->ex_pathlen != -1 && plen > x->ex_pathlen + 1) {
      ctx->error = X509_V_ERR_PATH_LENGTH_EXCEEDED;
      if (!call_verify_cb_at_depth(0, ctx, i)) {
        return 0;
      }
    }
    // Increment path length if not self issued. As only self-issued
    // _intermediates_ are skipped in (l) of RFC 5280 (simply because it
    // operates on certificate chain _edges_), always increment for the first
    // (the leaf) in the chain.
    if (i == 0 || !(x->ex_flags & EXFLAG_SI)) {
      plen++;
    }
  }

  return 1;
}

int reject_dns_name_in_common_name(X509 *x509) {
  const X509_NAME *name = X509_get_subject_name(x509);
  int i = -1;
  for (;;) {
    i = X509_NAME_get_index_by_NID(name, NID_commonName, i);
    if (i == -1) {
      return X509_V_OK;
    }

    const X509_NAME_ENTRY *entry = X509_NAME_get_entry(name, i);
    const ASN1_STRING *common_name = X509_NAME_ENTRY_get_data(entry);
    unsigned char *idval;
    int idlen = ASN1_STRING_to_UTF8(&idval, common_name);
    if (idlen < 0) {
      return X509_V_ERR_OUT_OF_MEM;
    }
    // Only process attributes that look like host names. Note it is
    // important that this check be mirrored in `X509_check_host`.
    int looks_like_dns = x509v3_looks_like_dns_name(idval, (size_t)idlen);
    OPENSSL_free(idval);
    if (looks_like_dns) {
      return X509_V_ERR_NAME_CONSTRAINTS_WITHOUT_SANS;
    }
  }
}

int check_name_constraints(X509_STORE_CTX *ctx) {
  int i, j, rv;
  int has_name_constraints = 0;
  // Check name constraints for all certificates
  for (i = (int)sk_X509_num(ctx->chain) - 1; i >= 0; i--) {
    X509Impl *x = FromOpaque(sk_X509_value(ctx->chain, i));
    // Ignore self issued certs unless last in chain
    if (i && (x->ex_flags & EXFLAG_SI)) {
      continue;
    }
    // Check against constraints for all certificates higher in chain
    // including trust anchor. Trust anchor not strictly speaking needed
    // but if it includes constraints it is to be assumed it expects them
    // to be obeyed.
    for (j = (int)sk_X509_num(ctx->chain) - 1; j > i; j--) {
      NAME_CONSTRAINTS *nc = FromOpaque(sk_X509_value(ctx->chain, j))->nc.get();
      if (nc) {
        has_name_constraints = 1;
        rv = NAME_CONSTRAINTS_check(x, nc);
        switch (rv) {
          case X509_V_OK:
            continue;
          case X509_V_ERR_OUT_OF_MEM:
            ctx->error = rv;
            return 0;
          default:
            ctx->error = rv;
            if (!call_verify_cb_at_depth(0, ctx, i)) {
              return 0;
            }
            break;
        }
      }
    }
  }

  // Name constraints do not match against the common name, but
  // `X509_check_host` still implements the legacy behavior where, on
  // certificates lacking a SAN list, DNS-like names in the common name are
  // checked instead.
  //
  // While we could apply the name constraints to the common name, name
  // constraints are rare enough that can hold such certificates to a higher
  // standard. Note this does not make "DNS-like" heuristic failures any
  // worse. A decorative common-name misidentified as a DNS name would fail
  // the name constraint anyway.
  X509Impl *leaf = FromOpaque(sk_X509_value(ctx->chain, 0));
  if (has_name_constraints && leaf->altname == nullptr) {
    rv = reject_dns_name_in_common_name(leaf);
    switch (rv) {
      case X509_V_OK:
        break;
      case X509_V_ERR_OUT_OF_MEM:
        ctx->error = rv;
        return 0;
      default:
        ctx->error = rv;
        if (!call_verify_cb_at_depth(0, ctx, /*depth=*/0)) {
          return 0;
        }
        break;
    }
  }

  return 1;
}

int check_id_error(X509_STORE_CTX *ctx, int errcode) {
  ctx->error = errcode;
  return call_verify_cb_at_depth(0, ctx, /*depth=*/0);
}

int check_hosts(X509 *x, X509_VERIFY_PARAM *param) {
  size_t i;
  size_t n = sk_OPENSSL_STRING_num(param->hosts);
  char *name;

  for (i = 0; i < n; ++i) {
    name = sk_OPENSSL_STRING_value(param->hosts, i);
    if (X509_check_host(x, name, strlen(name), param->hostflags, nullptr) > 0) {
      return 1;
    }
  }
  return n == 0;
}

int check_id(X509_STORE_CTX *ctx) {
  X509_VERIFY_PARAM *vpm = ctx->param;
  X509 *x = ctx->cert;
  if (vpm->poison) {
    if (!check_id_error(ctx, X509_V_ERR_INVALID_CALL)) {
      return 0;
    }
  }
  if (vpm->hosts && check_hosts(x, vpm) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_HOSTNAME_MISMATCH)) {
      return 0;
    }
  }
  if (vpm->email && X509_check_email(x, vpm->email, vpm->emaillen, 0) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_EMAIL_MISMATCH)) {
      return 0;
    }
  }
  if (vpm->ip && X509_check_ip(x, vpm->ip, vpm->iplen, 0) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_IP_ADDRESS_MISMATCH)) {
      return 0;
    }
  }
  return 1;
}

int check_trust(X509_STORE_CTX *ctx) {
  // If this function is called, `ctx->chain` must end with some certificate
  // from the store. Determine if we should treat it as a trust anchor in this
  // context.
  X509 *last = sk_X509_last(ctx->chain);
  int trust = X509_check_trust(last, ctx->param->trust, /*flags=*/0);
  if (trust == X509_TRUST_TRUSTED) {
    return X509_TRUST_TRUSTED;
  }
  if (trust == X509_TRUST_REJECTED) {
    // Notify callback and reject if not overridden.
    ctx->error = X509_V_ERR_CERT_REJECTED;
    if (!call_verify_cb_at_depth(0, ctx, sk_X509_num(ctx->chain) - 1)) {
      return X509_TRUST_REJECTED;
    }
  }

  // If we accept partial chains, every certificate in the store is a trust
  // anchor.
  if (ctx->param->flags & X509_V_FLAG_PARTIAL_CHAIN) {
    return X509_TRUST_TRUSTED;
  }

  return X509_TRUST_UNTRUSTED;
}

// Check CRL times against values in X509_STORE_CTX
int check_crl_time(X509_STORE_CTX *ctx, const X509_CRL *crl, bool notify,
                   int depth) {
  if (ctx->param->flags & X509_V_FLAG_NO_CHECK_TIME) {
    return 1;
  }

  int64_t ptime;
  if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME) {
    ptime = ctx->param->check_time;
  } else {
    ptime = time(nullptr);
  }

  int i = X509_cmp_time_posix(X509_CRL_get0_lastUpdate(crl), ptime);
  if (i == 0) {
    if (!notify) {
      return 0;
    }
    ctx->error = X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (i > 0) {
    if (!notify) {
      return 0;
    }
    ctx->error = X509_V_ERR_CRL_NOT_YET_VALID;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (X509_CRL_get0_nextUpdate(crl)) {
    i = X509_cmp_time_posix(X509_CRL_get0_nextUpdate(crl), ptime);

    if (i == 0) {
      if (!notify) {
        return 0;
      }
      ctx->error = X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD;
      if (!call_verify_cb_at_depth(0, ctx, depth)) {
        return 0;
      }
    }
    if (i < 0) {
      if (!notify) {
        return 0;
      }
      ctx->error = X509_V_ERR_CRL_HAS_EXPIRED;
      if (!call_verify_cb_at_depth(0, ctx, depth)) {
        return 0;
      }
    }
  }

  return 1;
}

int check_blocklist(X509_STORE_CTX *ctx) {
  static const uint8_t kBlockList[][32] = {
      {0xe0, 0x25, 0x4c, 0x7f, 0xcd, 0xc2, 0x65, 0xe2, 0xf0, 0xf8, 0xa4,
       0x28, 0xda, 0xbb, 0x75, 0xab, 0x4a, 0xe0, 0x39, 0xb4, 0x87, 0x2d,
       0x62, 0xf5, 0xee, 0x57, 0xf0, 0x87, 0x7d, 0x5a, 0x8a, 0x41},
      {0x43, 0xbf, 0xfa, 0x19, 0xd6, 0xef, 0x65, 0x14, 0x51, 0xf6, 0x84,
       0x1e, 0x01, 0x99, 0x31, 0xe9, 0x6f, 0x6f, 0x2e, 0xae, 0xff, 0x28,
       0xab, 0x4b, 0x6f, 0x1c, 0xe4, 0x57, 0x6d, 0xe2, 0x71, 0x7b},
      {0xeb, 0xed, 0x64, 0x75, 0xc2, 0xc2, 0xd8, 0x0f, 0xcb, 0x19, 0x9f,
       0x3f, 0xe8, 0xdd, 0x6e, 0x6b, 0x3c, 0xe6, 0x0b, 0x77, 0xeb, 0x15,
       0xd4, 0xe4, 0x2c, 0x3d, 0xec, 0x0c, 0x6f, 0x5b, 0x51, 0x05},
      {0x5c, 0x06, 0x60, 0xa2, 0xb1, 0x64, 0x91, 0x0d, 0x8f, 0xf5, 0x89,
       0xc1, 0x8e, 0x14, 0x55, 0x2e, 0x9e, 0x0e, 0xc2, 0x37, 0xdc, 0xef,
       0xe2, 0xcb, 0x73, 0x6d, 0x41, 0xa4, 0x81, 0x1f, 0x81, 0xa0},
      {0x63, 0xec, 0xef, 0xee, 0x43, 0x1d, 0x49, 0x52, 0x3b, 0xc1, 0x13,
       0x6c, 0x4e, 0x72, 0xd0, 0x20, 0x6a, 0x47, 0x70, 0x86, 0xca, 0xf6,
       0x06, 0xcd, 0x5c, 0xf0, 0x25, 0x68, 0x6c, 0x10, 0x9c, 0xf8},
      {0x49, 0xdd, 0x35, 0x3d, 0xc2, 0xb9, 0x19, 0xa3, 0x77, 0x06, 0x45,
       0x11, 0x7d, 0x0e, 0x60, 0xc1, 0x61, 0x15, 0xe7, 0xd1, 0xbc, 0x25,
       0x9e, 0xef, 0xe7, 0x34, 0xb0, 0x0e, 0xa4, 0x34, 0xfe, 0x5f},
      {0xec, 0x4f, 0xfa, 0x1a, 0xd0, 0xac, 0x88, 0x08, 0x13, 0x14, 0xb7,
       0xaa, 0xff, 0xef, 0x6a, 0x91, 0xf9, 0x7c, 0xd5, 0x72, 0x91, 0xbb,
       0x5b, 0xa6, 0x86, 0xc5, 0x41, 0x5f, 0x5e, 0xb4, 0xf6, 0xb6},
      {0x92, 0xc0, 0x63, 0xd6, 0x7e, 0x8f, 0x14, 0xdb, 0x71, 0xd2, 0x88,
       0xe1, 0x12, 0x66, 0xff, 0xda, 0xf6, 0x9c, 0xbf, 0x11, 0x5f, 0x7b,
       0xab, 0x14, 0xf3, 0xbc, 0x2f, 0x68, 0xd2, 0x5e, 0x0e, 0x30},
  };

  const size_t num = sk_X509_num(ctx->chain);
  for (size_t i = 0; i < num; i++) {
    const X509 *x = sk_X509_value(ctx->chain, i);
    ScopedCBB cbb;
    if (!CBB_init(cbb.get(), 64) ||
        !x509_marshal_public_key(cbb.get(), X509_get_X509_PUBKEY(x))) {
      ctx->error = X509_V_ERR_OUT_OF_MEM;
      return 0;
    }
    uint8_t spki_hash[SHA256_DIGEST_LENGTH];
    SHA256(CBB_data(cbb.get()), CBB_len(cbb.get()), spki_hash);
    for (const auto &block : kBlockList) {
      if (Span(spki_hash) == Span(block)) {
        ctx->error = X509_V_ERR_CERT_REVOKED;
        if (!call_verify_cb_at_depth(0, ctx, static_cast<int>(i))) {
          return 0;
        }
      }
    }
  }
  return 1;
}

int crl_akid_check(X509_STORE_CTX *ctx, const X509_CRL *crl, X509 **out_issuer,
                   int *out_crl_score, int depth) {
  const X509_NAME *crl_name = X509_CRL_get_issuer(crl);
  depth++;
  assert(depth < static_cast<int>(sk_X509_num(ctx->chain)));

  X509 *crl_issuer = sk_X509_value(ctx->chain, depth);
  if (X509_check_akid(crl_issuer, crl->akid) == X509_V_OK) {
    *out_crl_score |= CRL_SCORE_AKID | CRL_SCORE_ISSUER_CERT;
    *out_issuer = crl_issuer;
    return 1;
  }

  for (depth++; depth < (int)sk_X509_num(ctx->chain); depth++) {
    crl_issuer = sk_X509_value(ctx->chain, depth);
    if (X509_NAME_cmp(X509_get_subject_name(crl_issuer), crl_name)) {
      continue;
    }
    if (X509_check_akid(crl_issuer, crl->akid) == X509_V_OK) {
      *out_crl_score |= CRL_SCORE_AKID | CRL_SCORE_SAME_PATH;
      *out_issuer = crl_issuer;
      return 1;
    }
  }

  return 0;
}

int idp_check_dp(const DIST_POINT_NAME *dp_cert,
                 const DIST_POINT_NAME *dp_crl) {
  // If the CRL IDP does not have a distributionPoint field, the CRL is scoped
  // to the entire CA and matches everything.
  //
  // TODO(crbug.com/42290219): This covers a null `dp_crl`, but not `dp_cert`.
  // If `dp_cert` is null, that means the certificate's DistributionPoint lacks
  // a distributionPoint. However, DistributionPoints are required to have
  // either distributionPoint or cRLIssuer (indirect CRL) and we reject the
  // latter. However, we don't seem to check this in the parser. Enforce this so
  // that `dp_cert` cannot be null.
  if (!dp_cert || !dp_crl) {
    return 1;
  }
  // We only support fullName, not nameRelativeToCRLIssuer.
  if (dp_cert->type != 0 || dp_crl->type != 0) {
    return 0;
  }
  // Check that the CRL's distributionPoint has some name in common with the
  // certificate's. A CA might shard across multiple CRLs. This check ensures we
  // are looking at the right shard.
  //
  // TODO(crbug.com/565047760): This should not be an O(N^2) comparison.
  for (const GENERAL_NAME *gen_cert : dp_cert->name.fullname) {
    for (const GENERAL_NAME *gen_crl : dp_crl->name.fullname) {
      if (GENERAL_NAME_cmp(gen_cert, gen_crl) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

// Check CRLDP and IDP
int crl_crldp_check(const X509 *x, const X509_CRL *crl, int crl_score) {
  auto *impl = FromOpaque(x);
  if (impl->ex_flags & EXFLAG_CA) {
    if (crl->idp && crl->idp->onlyuser) {
      return 0;
    }
  } else {
    if (crl->idp && crl->idp->onlyCA) {
      return 0;
    }
  }
  for (const DIST_POINT *dp : impl->crldp.get()) {
    // Skip distribution points with a reasons field or a CRL issuer:
    //
    // We do not support CRLs partitioned by reason code. RFC 5280 requires CAs
    // include at least one DistributionPoint that covers all reasons.
    //
    // We also do not support indirect CRLs, and a CRL issuer can only match
    // indirect CRLs (RFC 5280, section 6.3.3, step b.1).
    if (dp->reasons != nullptr || dp->CRLissuer != nullptr) {
      continue;
    }
    // At this point we have already checked that the CRL issuer matches
    // the certificate issuer (and set CRL_SCORE_ISSUER_NAME);

    // RFC 5280 Section 6.3.3 step b.2
    if (!crl->idp || idp_check_dp(dp->distpoint, crl->idp->distpoint)) {
      return 1;
    }
  }

  // If the CRL does not specify an issuing distribution point, allow it to
  // match anything. This partially implements RFC 5280. The final paragraph of
  // RFC 5280, section 6.3.3, specifies a default CRL-DP. This default would
  // match IDP-less CRLs and CRLs with an IDP of the default CRL-DP. We do not
  // implement the second condition.
  return !crl->idp || !crl->idp->distpoint;
}

// For a given CRL return how suitable it is for the supplied certificate
// 'x'. The return value is a mask of several criteria, or zero if the CRL is
// unusable. The certificate issuer is returned in `*out_issuer`.
int get_crl_score(X509_STORE_CTX *ctx, X509 **out_issuer, const X509_CRL *crl,
                  const X509 *x, int depth) {
  if (crl->idp) {
    // Per RFC 5280, section 5.2.5, at most one of onlyContainsUserCerts,
    // onlyContainsCACerts, and onlyContainsAttributeCerts may be true.
    // TODO(crbug.com/42290307): Move this check to the `ISSUING_DIST_POINT`
    // parser.
    int num_only =
        (!!crl->idp->onlyuser) + (!!crl->idp->onlyCA) + (!!crl->idp->onlyattr);
    if (num_only > 1) {
      return 0;
    }
    // Reason codes and indirect CRLs are not supported.
    if (crl->idp->indirectCRL || crl->idp->onlysomereasons) {
      return 0;
    }
  }

  // We do not support indirect CRLs, so the issuer names must match.
  if (X509_NAME_cmp(X509_get_issuer_name(x), X509_CRL_get_issuer(crl))) {
    return 0;
  }

  int crl_score = CRL_SCORE_ISSUER_NAME;
  if (!(crl->flags & EXFLAG_CRITICAL)) {
    crl_score |= CRL_SCORE_NOCRITICAL;
  }

  // Check expiry
  if (check_crl_time(ctx, crl, /*notify=*/false, depth)) {
    crl_score |= CRL_SCORE_TIME;
  }

  // Check authority key ID and locate certificate issuer
  if (!crl_akid_check(ctx, crl, out_issuer, &crl_score, depth)) {
    // If we can't locate certificate issuer at this point forget it
    return 0;
  }

  // Check cert for matching CRL distribution points
  if (crl_crldp_check(x, crl, crl_score)) {
    crl_score |= CRL_SCORE_SCOPE;
  }

  return crl_score;
}

int get_crl_sk(X509_STORE_CTX *ctx, CRLInfo *best, STACK_OF(X509_CRL) *crls,
               const X509 *x, int depth) {
  for (X509_CRL *crl : crls) {
    X509 *crl_issuer = nullptr;
    int crl_score = get_crl_score(ctx, &crl_issuer, crl, x, depth);
    if (crl_score < best->score || crl_score == 0) {
      continue;
    }
    assert(crl_issuer != nullptr);
    // If current CRL is equivalent use it if it is newer
    if (crl_score == best->score && best->crl != nullptr) {
      int day, sec;
      if (ASN1_TIME_diff(&day, &sec, X509_CRL_get0_lastUpdate(best->crl.get()),
                         X509_CRL_get0_lastUpdate(crl)) == 0) {
        continue;
      }
      // ASN1_TIME_diff never returns inconsistent signs for `day`
      // and `sec`.
      if (day <= 0 && sec <= 0) {
        continue;
      }
    }
    best->crl = UpRef(crl);
    best->issuer = crl_issuer;
    best->score = crl_score;
  }

  return best->score >= CRL_SCORE_VALID;
}

// Retrieve CRL corresponding to current certificate.
int get_crl(X509_STORE_CTX *ctx, CRLInfo *out_crl_info, const X509 *x,
            int depth) {
  CRLInfo best;
  if (!get_crl_sk(ctx, &best, ctx->crls, x, depth)) {
    // If we haven't yet found a CRL with sufficiently high score, lookup CRLs
    // from the store.
    UniquePtr<STACK_OF(X509_CRL)> skcrl(
        X509_STORE_CTX_get1_crls(ctx, X509_get_issuer_name(x)));
    if (skcrl) {
      get_crl_sk(ctx, &best, skcrl.get(), x, depth);
    }
  }

  // If we got any kind of CRL use it and return success
  if (best.crl) {
    *out_crl_info = std::move(best);
    return 1;
  }

  return 0;
}

// Check CRL validity
int check_crl(X509_STORE_CTX *ctx, const CRLInfo &crl_info, int depth) {
  X509_CRL *crl = crl_info.crl.get();
  X509Impl *issuer = FromOpaque(crl_info.issuer);

  // Check for cRLSign bit if keyUsage present
  if ((issuer->ex_flags & EXFLAG_KUSAGE) &&
      !(issuer->ex_kusage & X509v3_KU_CRL_SIGN)) {
    ctx->error = X509_V_ERR_KEYUSAGE_NO_CRL_SIGN;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (!(crl_info.score & CRL_SCORE_SCOPE)) {
    ctx->error = X509_V_ERR_DIFFERENT_CRL_SCOPE;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (!(crl_info.score & CRL_SCORE_TIME)) {
    if (!check_crl_time(ctx, crl, /*notify=*/true, depth)) {
      return 0;
    }
  }

  // Attempt to get issuer certificate public key
  EVP_PKEY *ikey = X509_get0_pubkey(issuer);
  if (!ikey) {
    ctx->error = X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  } else {
    // Verify CRL signature
    if (X509_CRL_verify(crl, ikey) <= 0) {
      ctx->error = X509_V_ERR_CRL_SIGNATURE_FAILURE;
      if (!call_verify_cb_at_depth(0, ctx, depth)) {
        return 0;
      }
    }
  }

  return 1;
}

// Check certificate against CRL
int cert_crl(X509_STORE_CTX *ctx, X509_CRL *crl, const X509 *x, int depth) {
  // The rules changed for this... previously if a CRL contained unhandled
  // critical extensions it could still be used to indicate a certificate
  // was revoked. This has since been changed since critical extension can
  // change the meaning of CRL entries.
  if (!(ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL) &&
      (crl->flags & EXFLAG_CRITICAL)) {
    ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }
  // Look for serial number of certificate in CRL.
  X509_REVOKED *rev;
  if (X509_CRL_get0_by_cert(crl, &rev, x)) {
    ctx->error = X509_V_ERR_CERT_REVOKED;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  return 1;
}

int check_revocation_for_cert(X509_STORE_CTX *ctx, int depth) {
  X509 *x = sk_X509_value(ctx->chain, depth);
  CRLInfo crl_info;
  if (!get_crl(ctx, &crl_info, x, depth)) {
    ctx->error = X509_V_ERR_UNABLE_TO_GET_CRL;
    return call_verify_cb_at_depth(0, ctx, depth);
  }

  if (!check_crl(ctx, crl_info, depth) ||  //
      !cert_crl(ctx, crl_info.crl.get(), x, depth)) {
    return 0;
  }

  return 1;
}

int check_revocation(X509_STORE_CTX *ctx) {
  if (!(ctx->param->flags & X509_V_FLAG_CRL_CHECK) ||
      sk_X509_num(ctx->chain) <= 1) {
    return 1;
  }
  // Check either only the leaf, or everything but the trust anchor. The trust
  // anchor is not formally part of RFC 5280 certification path and, without an
  // issuer, we could not usefully process a CRL anyway.
  const int num_to_check = (ctx->param->flags & X509_V_FLAG_CRL_CHECK_ALL)
                               ? static_cast<int>(sk_X509_num(ctx->chain)) - 1
                               : 1;
  for (int i = 0; i < num_to_check; i++) {
    if (!check_revocation_for_cert(ctx, i)) {
      return 0;
    }
  }
  return 1;
}

int check_policy(X509_STORE_CTX *ctx) {
  int error_depth;
  int ret = X509_policy_check(ctx->chain, ctx->param->policies,
                              ctx->param->flags, &error_depth);
  if (ret != X509_V_OK) {
    ctx->error = ret;
    if (ret == X509_V_ERR_OUT_OF_MEM) {
      return 0;
    }
    return call_verify_cb_at_depth(0, ctx, error_depth);
  }

  return 1;
}

int check_cert_time(X509_STORE_CTX *ctx, X509 *x, int depth) {
  if (ctx->param->flags & X509_V_FLAG_NO_CHECK_TIME) {
    return 1;
  }

  int64_t ptime;
  if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME) {
    ptime = ctx->param->check_time;
  } else {
    ptime = time(nullptr);
  }

  int i = (ctx->param->flags & X509_V_FLAG_ALLOW_TIMEZONE_OFFSET)
              ? X509_cmp_time_posix_nonstandard(X509_get_notBefore(x), ptime)
              : X509_cmp_time_posix(X509_get_notBefore(x), ptime);
  if (i == 0) {
    ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (i > 0) {
    ctx->error = X509_V_ERR_CERT_NOT_YET_VALID;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  i = (ctx->param->flags & X509_V_FLAG_ALLOW_TIMEZONE_OFFSET)
          ? X509_cmp_time_posix_nonstandard(X509_get_notAfter(x), ptime)
          : X509_cmp_time_posix(X509_get_notAfter(x), ptime);
  if (i == 0) {
    ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  if (i < 0) {
    ctx->error = X509_V_ERR_CERT_HAS_EXPIRED;
    if (!call_verify_cb_at_depth(0, ctx, depth)) {
      return 0;
    }
  }

  return 1;
}

int verify_signature(X509_STORE_CTX *ctx, const X509 *x509,
                     const X509 *issuer) {
  EVP_PKEY *pkey = X509_get0_pubkey(issuer);
  if (pkey == nullptr) {
    return X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
  }

  int ret;
  if ((ctx->param->flags & X509_V_FLAG_USE_MTC_DRAFT_PLANTS_07) &&
      x509_is_merkle_tree_ca(issuer)) {
    ret = x509_verify_mtc(x509, pkey, issuer);
  } else {
    ret = X509_verify(x509, pkey);
  }
  return ret ? X509_V_OK : X509_V_ERR_CERT_SIGNATURE_FAILURE;
}

int check_chain_signatures_and_time(X509_STORE_CTX *ctx) {
  size_t num = sk_X509_num(ctx->chain);
  for (size_t i = num - 1; i < num; i--) {
    X509 *x509 = sk_X509_value(ctx->chain, i);
    if (i < num - 1) {
      X509 *issuer = sk_X509_value(ctx->chain, i + 1);
      if (int err = verify_signature(ctx, x509, issuer); err != X509_V_OK) {
        ctx->error = err;
        if (!call_verify_cb_at_depth(0, ctx, static_cast<int>(i))) {
          return 0;
        }
      }
    }

    if (!check_cert_time(ctx, x509, static_cast<int>(i))) {
      return 0;
    }

    // The last error (if any) is still in the error value
    if (!call_verify_cb_at_depth(1, ctx, static_cast<int>(i))) {
      return 0;
    }
  }

  return 1;
}

}  // namespace

// Given a possible certificate and issuer check them
int bssl::x509_check_issued_with_callback(X509_STORE_CTX *ctx, const X509 *x,
                                          const X509 *issuer) {
  int ret;
  ret = X509_check_issued(issuer, x);
  if (ret == X509_V_OK) {
    return 1;
  }
  // If we haven't asked for issuer errors don't set ctx
  if (!(ctx->param->flags & X509_V_FLAG_CB_ISSUER_CHECK)) {
    return 0;
  }

  ctx->error = ret;
  ctx->current_cert = const_cast<X509 *>(x);
  return call_verify_cb(0, ctx);
}

int X509_verify_cert(X509_STORE_CTX *ctx) {
  int ok = 0;
  Cleanup set_ctx_error = [&] {
    // Safety net, error returns must set ctx->error
    if (!ok && ctx->error == X509_V_OK) {
      ctx->error = X509_V_ERR_UNSPECIFIED;
    }
  };

  const X509_VERIFY_PARAM *param = ctx->param;
  if (ctx->cert == nullptr) {
    OPENSSL_PUT_ERROR(X509, X509_R_NO_CERT_SET_FOR_US_TO_VERIFY);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return 0;
  }

  if (ctx->chain != nullptr) {
    // This X509_STORE_CTX has already been used to verify a cert. We
    // cannot do another one.
    OPENSSL_PUT_ERROR(X509, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return 0;
  }

  if (param->flags &
      (X509_V_FLAG_EXTENDED_CRL_SUPPORT | X509_V_FLAG_USE_DELTAS)) {
    // We do not support indirect or delta CRLs. The flags still exist for
    // compatibility with bindings libraries, but to ensure we do not
    // inadvertently skip a CRL check that the caller expects, fail closed.
    OPENSSL_PUT_ERROR(X509, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return 0;
  }

  // Copy the untrusted certificates so we can track which have been used.
  // `untrusted` is not a `UniquePtr<STACK_OF(X509)>` because that would do a
  // deep free and this is a shallow free.
  STACK_OF(X509) *untrusted = nullptr;
  Cleanup free_untrusted = [&] { sk_X509_free(untrusted); };
  if (ctx->untrusted != nullptr &&
      (untrusted = sk_X509_dup(ctx->untrusted)) == nullptr) {
    ctx->error = X509_V_ERR_OUT_OF_MEM;
    return 0;
  }

  // `param->depth` does not include the leaf certificate or the trust anchor,
  // so the maximum size is 2 more.
  int max_chain = param->depth >= INT_MAX - 2 ? INT_MAX : param->depth + 2;
  int trust = X509_TRUST_UNTRUSTED;

  // First, check if the target certificate already exists in the trust store as
  // a trust "anchor".
  if (UniquePtr<X509> store_cert = lookup_cert_match(ctx, ctx->cert);
      store_cert != nullptr) {
    ctx->chain = sk_X509_new_null();
    if (ctx->chain == nullptr ||
        !PushToStack(ctx->chain, std::move(store_cert))) {
      ctx->error = X509_V_ERR_OUT_OF_MEM;
      return 0;
    }
    trust = check_trust(ctx);
    if (trust == X509_TRUST_REJECTED) {
      return 0;
    }
  }

  // If it was not a directly trusted (or distrusted) leaf, build a chain the
  // usual way. (This is the common case.)
  if (trust == X509_TRUST_UNTRUSTED) {
    // Start with the target certificate. Clear `ctx->chain` in case there is
    // any remnant of a failed trusted leaf above.
    sk_X509_pop_free(ctx->chain, X509_free);
    ctx->chain = sk_X509_new_null();
    if (ctx->chain == nullptr || !PushToStack(ctx->chain, UpRef(ctx->cert))) {
      ctx->error = X509_V_ERR_OUT_OF_MEM;
      return 0;
    }

    // Add certificates to the path until we reach a trust anchor, or fail.
    bool have_store_cert = false;
    for (;;) {
      if (static_cast<int>(sk_X509_num(ctx->chain)) >= max_chain) {
        break;
      }

      // If the last certificate is already self-signed, we won't get anywhere
      // by looking further.
      X509 *last = sk_X509_last(ctx->chain);
      int is_self_signed;
      if (!cert_self_signed(last, &is_self_signed)) {
        ctx->error = X509_V_ERR_INVALID_EXTENSION;
        return 0;
      }
      if (is_self_signed) {
        break;
      }

      // Prioritize looking for issuers in the store.
      if (UniquePtr<X509> issuer = get_trusted_issuer(ctx, last);
          issuer != nullptr) {
        if (!PushToStack(ctx->chain, std::move(issuer))) {
          ctx->error = X509_V_ERR_OUT_OF_MEM;
          return 0;
        }
        have_store_cert = true;
        // Certificates from the store are candidates to be trust anchors.
        trust = check_trust(ctx);
        if (trust == X509_TRUST_REJECTED) {
          return 0;
        }
        if (trust == X509_TRUST_TRUSTED) {
          break;
        }
        continue;
      }

      // Look for an issuer among peer-provided untrusted intermediates, but
      // only if we included a certificate from the store. For now, we match
      // OpenSSL by not interleaving them, but the logic that relied on this has
      // been removed.
      if (!have_store_cert) {
        if (X509 *issuer = find_issuer(ctx, untrusted, last);
            issuer != nullptr) {
          if (!PushToStack(ctx->chain, UpRef(issuer))) {
            ctx->error = X509_V_ERR_OUT_OF_MEM;
            return 0;
          }
          (void)sk_X509_delete_ptr(untrusted, issuer);
          // We must not call `check_trust` here because the last certificate in
          // the chain is not from the store. Otherwise, `check_trust` might
          // mistake it for a trust anchor.
          continue;
        }
      }

      // Couldn't find anything.
      break;
    }

    // `X509_TRUST_REJECTED` should have returned early.
    assert(trust == X509_TRUST_UNTRUSTED || X509_TRUST_TRUSTED);
    if (trust == X509_TRUST_UNTRUSTED) {
      if (static_cast<int>(sk_X509_num(ctx->chain)) >= max_chain) {
        ctx->error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
      } else if (have_store_cert) {
        ctx->error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT;
      } else {
        int is_self_signed;
        if (!cert_self_signed(sk_X509_last(ctx->chain), &is_self_signed)) {
          ctx->error = X509_V_ERR_INVALID_EXTENSION;
        } else if (!is_self_signed) {
          ctx->error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY;
        } else if (sk_X509_num(ctx->chain) == 1) {
          ctx->error = X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT;
        } else {
          ctx->error = X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN;
        }
      }
      if (!call_verify_cb_at_depth(0, ctx, sk_X509_num(ctx->chain) - 1)) {
        return 0;
      }
    }
  }

  // Perform path validation. If the caller did not suppress some error with the
  // verify callback, we now have either a path from the target certificate to
  // some trust anchor or a directly trusted leaf.
  assert(sk_X509_num(ctx->chain) >= 1);
  if (!check_chain_extensions(ctx) ||           //
      !check_id(ctx) ||                         //
      !check_revocation(ctx) ||                 //
      !check_blocklist(ctx) ||                  //
      !check_chain_signatures_and_time(ctx) ||  //
      !check_name_constraints(ctx) ||           //
      !check_policy(ctx)) {
    return 0;
  }

  ok = 1;
  return 1;
}

int X509_cmp_current_time(const ASN1_TIME *ctm) {
  return X509_cmp_time_posix(ctm, time(nullptr));
}

int X509_cmp_time(const ASN1_TIME *ctm, const time_t *cmp_time) {
  int64_t compare_time = (cmp_time == nullptr) ? time(nullptr) : *cmp_time;
  return X509_cmp_time_posix(ctm, compare_time);
}

int X509_cmp_time_posix(const ASN1_TIME *ctm, int64_t cmp_time) {
  int64_t ctm_time;
  if (!ASN1_TIME_to_posix(ctm, &ctm_time)) {
    return 0;
  }
  // The return value 0 is reserved for errors.
  return (ctm_time - cmp_time <= 0) ? -1 : 1;
}

int X509_cmp_time_posix_nonstandard(const ASN1_TIME *ctm, int64_t cmp_time) {
  int64_t ctm_time;
  if (!ASN1_TIME_to_posix_nonstandard(ctm, &ctm_time)) {
    return 0;
  }
  // The return value 0 is reserved for errors.
  return (ctm_time - cmp_time <= 0) ? -1 : 1;
}

ASN1_TIME *X509_gmtime_adj(ASN1_TIME *s, long offset_sec) {
  return X509_time_adj(s, offset_sec, nullptr);
}

ASN1_TIME *X509_time_adj(ASN1_TIME *s, long offset_sec, const time_t *in_tm) {
  return X509_time_adj_ex(s, 0, offset_sec, in_tm);
}

ASN1_TIME *X509_time_adj_ex(ASN1_TIME *s, int offset_day, long offset_sec,
                            const time_t *in_tm) {
  int64_t t = 0;

  if (in_tm) {
    t = *in_tm;
  } else {
    t = time(nullptr);
  }

  return ASN1_TIME_adj(s, t, offset_day, offset_sec);
}

int X509_STORE_CTX_get_ex_new_index(long argl, void *argp,
                                    CRYPTO_EX_unused *unused,
                                    CRYPTO_EX_dup *dup_unused,
                                    CRYPTO_EX_free *free_func) {
  return CRYPTO_get_ex_new_index_ex(&g_ex_data_class, argl, argp, free_func);
}

int X509_STORE_CTX_set_ex_data(X509_STORE_CTX *ctx, int idx, void *data) {
  return CRYPTO_set_ex_data(&ctx->ex_data, idx, data);
}

void *X509_STORE_CTX_get_ex_data(X509_STORE_CTX *ctx, int idx) {
  return CRYPTO_get_ex_data(&ctx->ex_data, idx);
}

int X509_STORE_CTX_get_error(const X509_STORE_CTX *ctx) { return ctx->error; }

void X509_STORE_CTX_set_error(X509_STORE_CTX *ctx, int err) {
  ctx->error = err;
}

int X509_STORE_CTX_get_error_depth(const X509_STORE_CTX *ctx) {
  return ctx->error_depth;
}

X509 *X509_STORE_CTX_get_current_cert(const X509_STORE_CTX *ctx) {
  return ctx->current_cert;
}

STACK_OF(X509) *X509_STORE_CTX_get_chain(const X509_STORE_CTX *ctx) {
  return ctx->chain;
}

STACK_OF(X509) *X509_STORE_CTX_get0_chain(const X509_STORE_CTX *ctx) {
  return ctx->chain;
}

STACK_OF(X509) *X509_STORE_CTX_get1_chain(const X509_STORE_CTX *ctx) {
  if (!ctx->chain) {
    return nullptr;
  }
  return X509_chain_up_ref(ctx->chain);
}

X509_STORE_CTX *X509_STORE_CTX_get0_parent_ctx(const X509_STORE_CTX *ctx) {
  // In OpenSSL, an `X509_STORE_CTX` sometimes has a parent context during CRL
  // path validation for indirect CRLs. We require the CRL to be issued
  // somewhere along the certificate path, so this is always NULL.
  return nullptr;
}

void X509_STORE_CTX_set_chain(X509_STORE_CTX *ctx, STACK_OF(X509) *sk) {
  ctx->untrusted = sk;
}

STACK_OF(X509) *X509_STORE_CTX_get0_untrusted(const X509_STORE_CTX *ctx) {
  return ctx->untrusted;
}

void X509_STORE_CTX_set0_crls(X509_STORE_CTX *ctx, STACK_OF(X509_CRL) *sk) {
  ctx->crls = sk;
}

int X509_STORE_CTX_set_purpose(X509_STORE_CTX *ctx, int purpose) {
  // If `purpose` is zero, this function historically silently did nothing.
  if (purpose == 0) {
    return 1;
  }

  const X509_PURPOSE *pobj = X509_PURPOSE_get0(purpose);
  if (pobj == nullptr) {
    OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_PURPOSE_ID);
    return 0;
  }

  int trust = X509_PURPOSE_get_trust(pobj);
  if (!X509_STORE_CTX_set_trust(ctx, trust)) {
    return 0;
  }

  if (ctx->param->purpose == 0) {
    ctx->param->purpose = purpose;
  }
  return 1;
}

int X509_STORE_CTX_set_trust(X509_STORE_CTX *ctx, int trust) {
  // If `trust` is zero, this function historically silently did nothing.
  if (trust == 0) {
    return 1;
  }

  if (!X509_is_valid_trust_id(trust)) {
    OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_TRUST_ID);
    return 0;
  }

  if (ctx->param->trust == 0) {
    ctx->param->trust = trust;
  }
  return 1;
}

X509_STORE_CTX *X509_STORE_CTX_new() { return New<X509_STORE_CTX>(); }

void X509_STORE_CTX_free(X509_STORE_CTX *ctx) {
  if (ctx == nullptr) {
    return;
  }
  X509_STORE_CTX_cleanup(ctx);
  Delete(ctx);
}

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
                        STACK_OF(X509) *chain) {
  X509_STORE_CTX_cleanup(ctx);

  ctx->ctx = store;
  ctx->cert = x509;
  ctx->untrusted = chain;

  CRYPTO_new_ex_data(&ctx->ex_data);

  if (store == nullptr) {
    OPENSSL_PUT_ERROR(X509, ERR_R_PASSED_NULL_PARAMETER);
    goto err;
  }

  ctx->param = X509_VERIFY_PARAM_new();
  if (!ctx->param) {
    goto err;
  }

  {
    // Inherit callbacks and flags from X509_STORE.

    auto *store_impl = FromOpaque(store);
    ctx->verify_cb = store_impl->verify_cb;

    if (!X509_VERIFY_PARAM_inherit(ctx->param, store_impl->param.get()) ||
        !X509_VERIFY_PARAM_inherit(ctx->param,
                                   X509_VERIFY_PARAM_lookup("default"))) {
      goto err;
    }

    if (store_impl->verify_cb) {
      ctx->verify_cb = store_impl->verify_cb;
    } else {
      ctx->verify_cb = null_callback;
    }
  }

  return 1;

err:
  X509_STORE_CTX_cleanup(ctx);
  return 0;
}

// Set alternative lookup method: just a STACK of trusted certificates. This
// avoids X509_STORE nastiness where it isn't needed.

void X509_STORE_CTX_set0_trusted_stack(X509_STORE_CTX *ctx,
                                       STACK_OF(X509) *sk) {
  ctx->trusted_stack = sk;
}

void X509_STORE_CTX_trusted_stack(X509_STORE_CTX *ctx, STACK_OF(X509) *sk) {
  X509_STORE_CTX_set0_trusted_stack(ctx, sk);
}

void X509_STORE_CTX_cleanup(X509_STORE_CTX *ctx) {
  CRYPTO_free_ex_data(&g_ex_data_class, &ctx->ex_data);
  X509_VERIFY_PARAM_free(ctx->param);
  sk_X509_pop_free(ctx->chain, X509_free);
  OPENSSL_memset(ctx, 0, sizeof(X509_STORE_CTX));
}

void X509_STORE_CTX_set_depth(X509_STORE_CTX *ctx, int depth) {
  X509_VERIFY_PARAM_set_depth(ctx->param, depth);
}

void X509_STORE_CTX_set_flags(X509_STORE_CTX *ctx, unsigned long flags) {
  X509_VERIFY_PARAM_set_flags(ctx->param, flags);
}

void X509_STORE_CTX_set_time_posix(X509_STORE_CTX *ctx, unsigned long flags,
                                   int64_t t) {
  X509_VERIFY_PARAM_set_time_posix(ctx->param, t);
}

void X509_STORE_CTX_set_time(X509_STORE_CTX *ctx, unsigned long flags,
                             time_t t) {
  X509_STORE_CTX_set_time_posix(ctx, flags, t);
}

X509 *X509_STORE_CTX_get0_cert(const X509_STORE_CTX *ctx) { return ctx->cert; }

void X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *ctx,
                                  int (*verify_cb)(int, X509_STORE_CTX *)) {
  ctx->verify_cb = verify_cb;
}

int X509_STORE_CTX_set_default(X509_STORE_CTX *ctx, const char *name) {
  const X509_VERIFY_PARAM *param = X509_VERIFY_PARAM_lookup(name);
  if (!param) {
    return 0;
  }
  return X509_VERIFY_PARAM_inherit(ctx->param, param);
}

X509_VERIFY_PARAM *X509_STORE_CTX_get0_param(X509_STORE_CTX *ctx) {
  return ctx->param;
}

void X509_STORE_CTX_set0_param(X509_STORE_CTX *ctx, X509_VERIFY_PARAM *param) {
  if (ctx->param) {
    X509_VERIFY_PARAM_free(ctx->param);
  }
  ctx->param = param;
}
