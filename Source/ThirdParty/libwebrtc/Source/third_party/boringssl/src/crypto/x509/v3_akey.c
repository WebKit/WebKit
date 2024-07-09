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

#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "internal.h"


static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(
    const X509V3_EXT_METHOD *method, void *ext, STACK_OF(CONF_VALUE) *extlist);
static void *v2i_AUTHORITY_KEYID(const X509V3_EXT_METHOD *method,
                                 const X509V3_CTX *ctx,
                                 const STACK_OF(CONF_VALUE) *values);

const X509V3_EXT_METHOD v3_akey_id = {
    NID_authority_key_identifier,
    X509V3_EXT_MULTILINE,
    ASN1_ITEM_ref(AUTHORITY_KEYID),
    0,
    0,
    0,
    0,
    0,
    0,
    i2v_AUTHORITY_KEYID,
    v2i_AUTHORITY_KEYID,
    0,
    0,
    NULL,
};

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(
    const X509V3_EXT_METHOD *method, void *ext, STACK_OF(CONF_VALUE) *extlist) {
  const AUTHORITY_KEYID *akeyid = ext;
  int extlist_was_null = extlist == NULL;
  if (akeyid->keyid) {
    char *tmp = x509v3_bytes_to_hex(akeyid->keyid->data, akeyid->keyid->length);
    int ok = tmp != NULL && X509V3_add_value("keyid", tmp, &extlist);
    OPENSSL_free(tmp);
    if (!ok) {
      goto err;
    }
  }
  if (akeyid->issuer) {
    STACK_OF(CONF_VALUE) *tmpextlist =
        i2v_GENERAL_NAMES(NULL, akeyid->issuer, extlist);
    if (tmpextlist == NULL) {
      goto err;
    }
    extlist = tmpextlist;
  }
  if (akeyid->serial) {
    if (!X509V3_add_value_int("serial", akeyid->serial, &extlist)) {
      goto err;
    }
  }
  return extlist;

err:
  if (extlist_was_null) {
    sk_CONF_VALUE_pop_free(extlist, X509V3_conf_free);
  }
  return NULL;
}

// Currently two options: keyid: use the issuers subject keyid, the value
// 'always' means its is an error if the issuer certificate doesn't have a
// key id. issuer: use the issuers cert issuer and serial number. The default
// is to only use this if keyid is not present. With the option 'always' this
// is always included.

static void *v2i_AUTHORITY_KEYID(const X509V3_EXT_METHOD *method,
                                 const X509V3_CTX *ctx,
                                 const STACK_OF(CONF_VALUE) *values) {
  char keyid = 0, issuer = 0;
  int j;
  ASN1_OCTET_STRING *ikeyid = NULL;
  X509_NAME *isname = NULL;
  GENERAL_NAMES *gens = NULL;
  GENERAL_NAME *gen = NULL;
  ASN1_INTEGER *serial = NULL;
  const X509 *cert;
  AUTHORITY_KEYID *akeyid;

  for (size_t i = 0; i < sk_CONF_VALUE_num(values); i++) {
    const CONF_VALUE *cnf = sk_CONF_VALUE_value(values, i);
    if (!strcmp(cnf->name, "keyid")) {
      keyid = 1;
      if (cnf->value && !strcmp(cnf->value, "always")) {
        keyid = 2;
      }
    } else if (!strcmp(cnf->name, "issuer")) {
      issuer = 1;
      if (cnf->value && !strcmp(cnf->value, "always")) {
        issuer = 2;
      }
    } else {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNKNOWN_OPTION);
      ERR_add_error_data(2, "name=", cnf->name);
      return NULL;
    }
  }

  if (!ctx || !ctx->issuer_cert) {
    if (ctx && (ctx->flags == X509V3_CTX_TEST)) {
      return AUTHORITY_KEYID_new();
    }
    OPENSSL_PUT_ERROR(X509V3, X509V3_R_NO_ISSUER_CERTIFICATE);
    return NULL;
  }

  cert = ctx->issuer_cert;

  if (keyid) {
    j = X509_get_ext_by_NID(cert, NID_subject_key_identifier, -1);
    const X509_EXTENSION *ext;
    if ((j >= 0) && (ext = X509_get_ext(cert, j))) {
      ikeyid = X509V3_EXT_d2i(ext);
    }
    if (keyid == 2 && !ikeyid) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_KEYID);
      return NULL;
    }
  }

  if ((issuer && !ikeyid) || (issuer == 2)) {
    isname = X509_NAME_dup(X509_get_issuer_name(cert));
    serial = ASN1_INTEGER_dup(X509_get0_serialNumber(cert));
    if (!isname || !serial) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_DETAILS);
      goto err;
    }
  }

  if (!(akeyid = AUTHORITY_KEYID_new())) {
    goto err;
  }

  if (isname) {
    if (!(gens = sk_GENERAL_NAME_new_null()) || !(gen = GENERAL_NAME_new()) ||
        !sk_GENERAL_NAME_push(gens, gen)) {
      goto err;
    }
    gen->type = GEN_DIRNAME;
    gen->d.dirn = isname;
  }

  akeyid->issuer = gens;
  akeyid->serial = serial;
  akeyid->keyid = ikeyid;

  return akeyid;

err:
  X509_NAME_free(isname);
  ASN1_INTEGER_free(serial);
  ASN1_OCTET_STRING_free(ikeyid);
  return NULL;
}
