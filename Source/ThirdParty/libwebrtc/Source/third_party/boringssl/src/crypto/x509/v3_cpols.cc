// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <stdio.h>
#include <string.h>

#include <utility>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "internal.h"


using namespace bssl;

// Certificate policies extension support: this one is a bit complex...

static int i2r_certpol(const X509V3_EXT_METHOD *method, void *ext, BIO *out,
                       int indent);
static void *r2i_certpol(const X509V3_EXT_METHOD *method, const X509V3_CTX *ctx,
                         const char *value);
static void print_qualifiers(BIO *out, const STACK_OF(POLICYQUALINFO) *quals,
                             int indent);
static void print_notice(BIO *out, const USERNOTICE *notice, int indent);
static UniquePtr<POLICYINFO> policy_section(const X509V3_CTX *ctx,
                                            const STACK_OF(CONF_VALUE) *polstrs,
                                            int ia5org);
static UniquePtr<POLICYQUALINFO> notice_section(
    const X509V3_CTX *ctx, const STACK_OF(CONF_VALUE) *unot, int ia5org);
static int nref_nos(STACK_OF(ASN1_INTEGER) *nnums,
                    const STACK_OF(CONF_VALUE) *nos);

const X509V3_EXT_METHOD bssl::v3_cpols = {
    NID_certificate_policies,
    0,
    ASN1_ITEM_ref(CERTIFICATEPOLICIES),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    i2r_certpol,
    r2i_certpol,
    nullptr,
};

ASN1_SEQUENCE(NOTICEREF) = {
    ASN1_SIMPLE(NOTICEREF, organization, DISPLAYTEXT),
    ASN1_SEQUENCE_OF(NOTICEREF, noticenos, ASN1_INTEGER),
} ASN1_SEQUENCE_END(NOTICEREF)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(NOTICEREF)

ASN1_SEQUENCE(USERNOTICE) = {
    ASN1_OPT(USERNOTICE, noticeref, NOTICEREF),
    ASN1_OPT(USERNOTICE, exptext, DISPLAYTEXT),
} ASN1_SEQUENCE_END(USERNOTICE)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(USERNOTICE)

ASN1_ADB_TEMPLATE(policydefault) = ASN1_SIMPLE(POLICYQUALINFO, d.other,
                                               ASN1_ANY);

ASN1_ADB(POLICYQUALINFO) = {
    ADB_ENTRY(NID_id_qt_cps,
              ASN1_SIMPLE(POLICYQUALINFO, d.cpsuri, ASN1_IA5STRING)),
    ADB_ENTRY(NID_id_qt_unotice,
              ASN1_SIMPLE(POLICYQUALINFO, d.usernotice, USERNOTICE)),
} ASN1_ADB_END(POLICYQUALINFO, 0, pqualid, 0, &policydefault_tt, NULL);

ASN1_SEQUENCE(POLICYQUALINFO) = {
    ASN1_SIMPLE(POLICYQUALINFO, pqualid, ASN1_OBJECT),
    ASN1_ADB_OBJECT(POLICYQUALINFO),
} ASN1_SEQUENCE_END(POLICYQUALINFO)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(POLICYQUALINFO)

ASN1_SEQUENCE(POLICYINFO) = {
    ASN1_SIMPLE(POLICYINFO, policyid, ASN1_OBJECT),
    ASN1_SEQUENCE_OF_OPT(POLICYINFO, qualifiers, POLICYQUALINFO),
} ASN1_SEQUENCE_END(POLICYINFO)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(POLICYINFO)

ASN1_ITEM_TEMPLATE(CERTIFICATEPOLICIES) = ASN1_EX_TEMPLATE_TYPE(
    ASN1_TFLG_SEQUENCE_OF, 0, CERTIFICATEPOLICIES, POLICYINFO)
ASN1_ITEM_TEMPLATE_END(CERTIFICATEPOLICIES)

IMPLEMENT_ASN1_FUNCTIONS_const(CERTIFICATEPOLICIES)

static void *r2i_certpol(const X509V3_EXT_METHOD *method, const X509V3_CTX *ctx,
                         const char *value) {
  UniquePtr<STACK_OF(POLICYINFO)> pols(sk_POLICYINFO_new_null());
  if (pols == nullptr) {
    return nullptr;
  }
  UniquePtr<STACK_OF(CONF_VALUE)> vals(X509V3_parse_list(value));
  if (vals == nullptr) {
    OPENSSL_PUT_ERROR(X509V3, ERR_R_X509V3_LIB);
    return nullptr;
  }
  int ia5org = 0;
  for (const CONF_VALUE *cnf : vals.get()) {
    if (cnf->value || !cnf->name) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_POLICY_IDENTIFIER);
      X509V3_conf_err(cnf);
      return nullptr;
    }
    UniquePtr<POLICYINFO> pol;
    const char *pstr = cnf->name;
    if (!strcmp(pstr, "ia5org")) {
      ia5org = 1;
      continue;
    } else if (*pstr == '@') {
      const STACK_OF(CONF_VALUE) *polsect = X509V3_get_section(ctx, pstr + 1);
      if (!polsect) {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_SECTION);

        X509V3_conf_err(cnf);
        return nullptr;
      }
      pol = policy_section(ctx, polsect, ia5org);
      if (!pol) {
        return nullptr;
      }
    } else {
      UniquePtr<ASN1_OBJECT> pobj(OBJ_txt2obj(cnf->name, 0));
      if (pobj == nullptr) {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_OBJECT_IDENTIFIER);
        X509V3_conf_err(cnf);
        return nullptr;
      }
      pol.reset(POLICYINFO_new());
      if (pol == nullptr) {
        return nullptr;
      }
      pol->policyid = pobj.release();
    }
    if (!PushToStack(pols.get(), std::move(pol))) {
      return nullptr;
    }
  }
  return pols.release();
}

static UniquePtr<POLICYINFO> policy_section(const X509V3_CTX *ctx,
                                            const STACK_OF(CONF_VALUE) *polstrs,
                                            int ia5org) {
  UniquePtr<POLICYINFO> pol(POLICYINFO_new());
  if (pol == nullptr) {
    return nullptr;
  }
  for (const CONF_VALUE *cnf : polstrs) {
    if (!strcmp(cnf->name, "policyIdentifier")) {
      ASN1_OBJECT *pobj;
      if (!(pobj = OBJ_txt2obj(cnf->value, 0))) {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_OBJECT_IDENTIFIER);
        X509V3_conf_err(cnf);
        return nullptr;
      }
      ASN1_OBJECT_free(pol->policyid);
      pol->policyid = pobj;

    } else if (x509v3_conf_name_matches(cnf->name, "CPS")) {
      if (pol->qualifiers == nullptr) {
        pol->qualifiers = sk_POLICYQUALINFO_new_null();
      }
      UniquePtr<POLICYQUALINFO> qual(POLICYQUALINFO_new());
      if (qual == nullptr || pol->qualifiers == nullptr) {
        return nullptr;
      }
      qual->pqualid = OBJ_nid2obj(NID_id_qt_cps);
      if (qual->pqualid == nullptr) {
        OPENSSL_PUT_ERROR(X509V3, ERR_R_INTERNAL_ERROR);
        return nullptr;
      }
      qual->d.cpsuri = ASN1_IA5STRING_new();
      if (qual->d.cpsuri == nullptr) {
        return nullptr;
      }
      if (!ASN1_STRING_set(qual->d.cpsuri, cnf->value, strlen(cnf->value))) {
        return nullptr;
      }
      if (!PushToStack(pol->qualifiers, std::move(qual))) {
        return nullptr;
      }
    } else if (x509v3_conf_name_matches(cnf->name, "userNotice")) {
      if (*cnf->value != '@') {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_EXPECTED_A_SECTION_NAME);
        X509V3_conf_err(cnf);
        return nullptr;
      }
      const STACK_OF(CONF_VALUE) *unot =
          X509V3_get_section(ctx, cnf->value + 1);
      if (!unot) {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_SECTION);
        X509V3_conf_err(cnf);
        return nullptr;
      }
      UniquePtr<POLICYQUALINFO> qual = notice_section(ctx, unot, ia5org);
      if (qual == nullptr) {
        return nullptr;
      }
      if (pol->qualifiers == nullptr) {
        pol->qualifiers = sk_POLICYQUALINFO_new_null();
      }
      if (pol->qualifiers == nullptr ||
          !PushToStack(pol->qualifiers, std::move(qual))) {
        return nullptr;
      }
    } else {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_OPTION);

      X509V3_conf_err(cnf);
      return nullptr;
    }
  }
  if (!pol->policyid) {
    OPENSSL_PUT_ERROR(X509V3, X509V3_R_NO_POLICY_IDENTIFIER);
    return nullptr;
  }

  return pol;
}

static UniquePtr<POLICYQUALINFO> notice_section(
    const X509V3_CTX *ctx, const STACK_OF(CONF_VALUE) *unot, int ia5org) {
  UniquePtr<POLICYQUALINFO> qual(POLICYQUALINFO_new());
  if (qual == nullptr) {
    return nullptr;
  }
  qual->pqualid = OBJ_nid2obj(NID_id_qt_unotice);
  if (qual->pqualid == nullptr) {
    OPENSSL_PUT_ERROR(X509V3, ERR_R_INTERNAL_ERROR);
    return nullptr;
  }
  USERNOTICE *notice = USERNOTICE_new();
  if (notice == nullptr) {
    return nullptr;
  }
  qual->d.usernotice = notice;
  for (const CONF_VALUE *cnf : unot) {
    if (!strcmp(cnf->name, "explicitText")) {
      notice->exptext = ASN1_VISIBLESTRING_new();
      if (notice->exptext == nullptr) {
        return nullptr;
      }
      if (!ASN1_STRING_set(notice->exptext, cnf->value, strlen(cnf->value))) {
        return nullptr;
      }
    } else if (!strcmp(cnf->name, "organization")) {
      NOTICEREF *nref;
      if (!notice->noticeref) {
        if (!(nref = NOTICEREF_new())) {
          return nullptr;
        }
        notice->noticeref = nref;
      } else {
        nref = notice->noticeref;
      }
      if (ia5org) {
        nref->organization->type = V_ASN1_IA5STRING;
      } else {
        nref->organization->type = V_ASN1_VISIBLESTRING;
      }
      if (!ASN1_STRING_set(nref->organization, cnf->value,
                           strlen(cnf->value))) {
        return nullptr;
      }
    } else if (!strcmp(cnf->name, "noticeNumbers")) {
      NOTICEREF *nref;
      if (!notice->noticeref) {
        if (!(nref = NOTICEREF_new())) {
          return nullptr;
        }
        notice->noticeref = nref;
      } else {
        nref = notice->noticeref;
      }
      UniquePtr<STACK_OF(CONF_VALUE)> nos(X509V3_parse_list(cnf->value));
      if (!nos || !sk_CONF_VALUE_num(nos.get())) {
        OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_NUMBERS);
        X509V3_conf_err(cnf);
        return nullptr;
      }
      if (!nref_nos(nref->noticenos, nos.get())) {
        return nullptr;
      }
    } else {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_OPTION);
      X509V3_conf_err(cnf);
      return nullptr;
    }
  }

  if (notice->noticeref &&
      (!notice->noticeref->noticenos || !notice->noticeref->organization)) {
    OPENSSL_PUT_ERROR(X509V3, X509V3_R_NEED_ORGANIZATION_AND_NUMBERS);
    return nullptr;
  }

  return qual;
}

static int nref_nos(STACK_OF(ASN1_INTEGER) *nnums,
                    const STACK_OF(CONF_VALUE) *nos) {
  for (size_t i = 0; i < sk_CONF_VALUE_num(nos); i++) {
    const CONF_VALUE *cnf = sk_CONF_VALUE_value(nos, i);
    ASN1_INTEGER *aint = s2i_ASN1_INTEGER(nullptr, cnf->name);
    if (aint == nullptr) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_NUMBER);
      return 0;
    }
    if (!sk_ASN1_INTEGER_push(nnums, aint)) {
      ASN1_INTEGER_free(aint);
      return 0;
    }
  }
  return 1;
}

static int i2r_certpol(const X509V3_EXT_METHOD *method, void *ext, BIO *out,
                       int indent) {
  const STACK_OF(POLICYINFO) *pol =
      reinterpret_cast<const STACK_OF(POLICYINFO) *>(ext);
  // First print out the policy OIDs
  for (size_t i = 0; i < sk_POLICYINFO_num(pol); i++) {
    const POLICYINFO *pinfo = sk_POLICYINFO_value(pol, i);
    BIO_printf(out, "%*sPolicy: ", indent, "");
    i2a_ASN1_OBJECT(out, pinfo->policyid);
    BIO_puts(out, "\n");
    if (pinfo->qualifiers) {
      print_qualifiers(out, pinfo->qualifiers, indent + 2);
    }
  }
  return 1;
}

static void print_qualifiers(BIO *out, const STACK_OF(POLICYQUALINFO) *quals,
                             int indent) {
  for (size_t i = 0; i < sk_POLICYQUALINFO_num(quals); i++) {
    const POLICYQUALINFO *qualinfo = sk_POLICYQUALINFO_value(quals, i);
    switch (OBJ_obj2nid(qualinfo->pqualid)) {
      case NID_id_qt_cps:
        BIO_printf(out, "%*sCPS: %.*s\n", indent, "",
                   qualinfo->d.cpsuri->length, qualinfo->d.cpsuri->data);
        break;

      case NID_id_qt_unotice:
        BIO_printf(out, "%*sUser Notice:\n", indent, "");
        print_notice(out, qualinfo->d.usernotice, indent + 2);
        break;

      default:
        BIO_printf(out, "%*sUnknown Qualifier: ", indent + 2, "");

        i2a_ASN1_OBJECT(out, qualinfo->pqualid);
        BIO_puts(out, "\n");
        break;
    }
  }
}

static void print_notice(BIO *out, const USERNOTICE *notice, int indent) {
  if (notice->noticeref) {
    NOTICEREF *ref;
    ref = notice->noticeref;
    BIO_printf(out, "%*sOrganization: %.*s\n", indent, "",
               ref->organization->length, ref->organization->data);
    BIO_printf(out, "%*sNumber%s: ", indent, "",
               sk_ASN1_INTEGER_num(ref->noticenos) > 1 ? "s" : "");
    for (size_t i = 0; i < sk_ASN1_INTEGER_num(ref->noticenos); i++) {
      ASN1_INTEGER *num;
      char *tmp;
      num = sk_ASN1_INTEGER_value(ref->noticenos, i);
      if (i) {
        BIO_puts(out, ", ");
      }
      if (num == nullptr) {
        BIO_puts(out, "(null)");
      } else {
        tmp = i2s_ASN1_INTEGER(nullptr, num);
        if (tmp == nullptr) {
          return;
        }
        BIO_puts(out, tmp);
        OPENSSL_free(tmp);
      }
    }
    BIO_puts(out, "\n");
  }
  if (notice->exptext) {
    BIO_printf(out, "%*sExplicit Text: %.*s\n", indent, "",
               notice->exptext->length, notice->exptext->data);
  }
}
