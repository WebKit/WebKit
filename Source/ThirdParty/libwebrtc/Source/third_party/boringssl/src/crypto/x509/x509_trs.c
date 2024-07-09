/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"


typedef struct x509_trust_st X509_TRUST;

struct x509_trust_st {
  int trust;
  int (*check_trust)(const X509_TRUST *, X509 *);
  int nid;
} /* X509_TRUST */;

static int trust_1oidany(const X509_TRUST *trust, X509 *x);
static int trust_compat(const X509_TRUST *trust, X509 *x);

static int obj_trust(int id, X509 *x);

static const X509_TRUST trstandard[] = {
    {X509_TRUST_COMPAT, trust_compat, 0},
    {X509_TRUST_SSL_CLIENT, trust_1oidany, NID_client_auth},
    {X509_TRUST_SSL_SERVER, trust_1oidany, NID_server_auth},
    {X509_TRUST_EMAIL, trust_1oidany, NID_email_protect},
    {X509_TRUST_OBJECT_SIGN, trust_1oidany, NID_code_sign},
    {X509_TRUST_TSA, trust_1oidany, NID_time_stamp}};

static const X509_TRUST *X509_TRUST_get0(int id) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(trstandard); i++) {
    if (trstandard[i].trust == id) {
      return &trstandard[i];
    }
  }
  return NULL;
}

int X509_check_trust(X509 *x, int id, int flags) {
  if (id == -1) {
    return X509_TRUST_TRUSTED;
  }
  // We get this as a default value
  if (id == 0) {
    int rv = obj_trust(NID_anyExtendedKeyUsage, x);
    if (rv != X509_TRUST_UNTRUSTED) {
      return rv;
    }
    return trust_compat(NULL, x);
  }
  const X509_TRUST *pt = X509_TRUST_get0(id);
  if (pt == NULL) {
    // Unknown trust IDs are silently reintrepreted as NIDs. This is unreachable
    // from the certificate verifier itself, but wpa_supplicant relies on it.
    // Note this relies on commonly-used NIDs and trust IDs not colliding.
    return obj_trust(id, x);
  }
  return pt->check_trust(pt, x);
}

int X509_is_valid_trust_id(int trust) {
  return X509_TRUST_get0(trust) != NULL;
}

static int trust_1oidany(const X509_TRUST *trust, X509 *x) {
  if (x->aux && (x->aux->trust || x->aux->reject)) {
    return obj_trust(trust->nid, x);
  }
  // we don't have any trust settings: for compatibility we return trusted
  // if it is self signed
  return trust_compat(trust, x);
}

static int trust_compat(const X509_TRUST *trust, X509 *x) {
  if (!x509v3_cache_extensions(x)) {
    return X509_TRUST_UNTRUSTED;
  }
  if (x->ex_flags & EXFLAG_SS) {
    return X509_TRUST_TRUSTED;
  } else {
    return X509_TRUST_UNTRUSTED;
  }
}

static int obj_trust(int id, X509 *x) {
  X509_CERT_AUX *ax = x->aux;
  if (!ax) {
    return X509_TRUST_UNTRUSTED;
  }
  for (size_t i = 0; i < sk_ASN1_OBJECT_num(ax->reject); i++) {
    const ASN1_OBJECT *obj = sk_ASN1_OBJECT_value(ax->reject, i);
    if (OBJ_obj2nid(obj) == id) {
      return X509_TRUST_REJECTED;
    }
  }
  for (size_t i = 0; i < sk_ASN1_OBJECT_num(ax->trust); i++) {
    const ASN1_OBJECT *obj = sk_ASN1_OBJECT_value(ax->trust, i);
    if (OBJ_obj2nid(obj) == id) {
      return X509_TRUST_TRUSTED;
    }
  }
  return X509_TRUST_UNTRUSTED;
}
