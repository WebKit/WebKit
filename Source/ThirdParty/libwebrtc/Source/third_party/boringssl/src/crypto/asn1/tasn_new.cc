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

#include <openssl/asn1.h>

#include <string.h>

#include <openssl/asn1t.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

static void asn1_item_clear(ASN1_VALUE **pval, const ASN1_ITEM *it);
static int ASN1_template_new(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);
static void asn1_template_clear(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);
static int ASN1_primitive_new(ASN1_VALUE **pval, const ASN1_ITEM *it);
static void asn1_primitive_clear(ASN1_VALUE **pval, const ASN1_ITEM *it);

ASN1_VALUE *ASN1_item_new(const ASN1_ITEM *it) {
  ASN1_VALUE *ret = nullptr;
  if (ASN1_item_ex_new(&ret, it) > 0) {
    return ret;
  }
  return nullptr;
}

// Allocate an ASN1 structure

int bssl::ASN1_item_ex_new(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  const ASN1_TEMPLATE *tt = nullptr;
  const ASN1_EXTERN_FUNCS *ef;
  ASN1_VALUE **pseqval;
  int i;

  switch (it->itype) {
    case ASN1_ITYPE_EXTERN:
      ef = reinterpret_cast<const ASN1_EXTERN_FUNCS *>(it->funcs);
      if (ef && ef->asn1_ex_new) {
        if (!ef->asn1_ex_new(pval, it)) {
          goto memerr;
        }
      }
      break;

    case ASN1_ITYPE_PRIMITIVE:
      if (it->templates) {
        if (!ASN1_template_new(pval, it->templates)) {
          goto memerr;
        }
      } else if (!ASN1_primitive_new(pval, it)) {
        goto memerr;
      }
      break;

    case ASN1_ITYPE_MSTRING:
      if (!ASN1_primitive_new(pval, it)) {
        goto memerr;
      }
      break;

    case ASN1_ITYPE_CHOICE: {
      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != nullptr ? aux->asn1_cb : nullptr;
      if (asn1_cb) {
        i = asn1_cb(ASN1_OP_NEW_PRE, pval, it, nullptr);
        if (!i) {
          goto auxerr;
        }
        if (i == 2) {
          return 1;
        }
      }
      void *obj = OPENSSL_zalloc(it->size);
      if (obj == nullptr) {
        goto memerr;
      }
      asn1_store_ptr(pval, obj);
      asn1_set_choice_selector(pval, -1, it);
      if (asn1_cb && !asn1_cb(ASN1_OP_NEW_POST, pval, it, nullptr)) {
        goto auxerr2;
      }
      break;
    }

    case ASN1_ITYPE_SEQUENCE: {
      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != nullptr ? aux->asn1_cb : nullptr;
      if (asn1_cb) {
        i = asn1_cb(ASN1_OP_NEW_PRE, pval, it, nullptr);
        if (!i) {
          goto auxerr;
        }
        if (i == 2) {
          return 1;
        }
      }
      void *obj = OPENSSL_zalloc(it->size);
      if (obj == nullptr) {
        goto memerr;
      }
      asn1_store_ptr(pval, obj);
      asn1_refcount_set_one(pval, it);
      asn1_enc_init(pval, it);
      for (i = 0, tt = it->templates; i < it->tcount; tt++, i++) {
        pseqval = asn1_get_field_ptr(pval, tt);
        if (!ASN1_template_new(pseqval, tt)) {
          goto memerr2;
        }
      }
      if (asn1_cb && !asn1_cb(ASN1_OP_NEW_POST, pval, it, nullptr)) {
        goto auxerr2;
      }
      break;
    }
  }
  return 1;

memerr2:
  ASN1_item_ex_free(pval, it);
memerr:
  return 0;

auxerr2:
  ASN1_item_ex_free(pval, it);
auxerr:
  OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
  return 0;
}

static void asn1_item_clear(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  switch (it->itype) {
    case ASN1_ITYPE_EXTERN:
      asn1_store_ptr(pval, nullptr);
      break;

    case ASN1_ITYPE_PRIMITIVE:
      if (it->templates) {
        asn1_template_clear(pval, it->templates);
      } else {
        asn1_primitive_clear(pval, it);
      }
      break;

    case ASN1_ITYPE_MSTRING:
      asn1_primitive_clear(pval, it);
      break;

    case ASN1_ITYPE_CHOICE:
    case ASN1_ITYPE_SEQUENCE:
      asn1_store_ptr(pval, nullptr);
      break;
  }
}

static int ASN1_template_new(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt) {
  const ASN1_ITEM *it = ASN1_ITEM_ptr(tt->item);
  if (tt->flags & ASN1_TFLG_OPTIONAL) {
    asn1_template_clear(pval, tt);
    return 1;
  }
  // If ANY DEFINED BY, there is nothing to do.
  if (tt->flags & ASN1_TFLG_ADB_MASK) {
    asn1_store_ptr(pval, nullptr);
    return 1;
  }
  // If SET OF or SEQUENCE OF, it's a STACK.
  if (tt->flags & ASN1_TFLG_SK_MASK) {
    STACK_OF(ASN1_VALUE) *skval = sk_ASN1_VALUE_new_null();
    if (!skval) {
      return 0;
    }
    asn1_store_ptr(pval, skval);
    return 1;
  }
  // Otherwise, pass it back to the item routine.
  return ASN1_item_ex_new(pval, it);
}

static void asn1_template_clear(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt) {
  // If ADB or STACK, just NULL the field.
  if (tt->flags & (ASN1_TFLG_ADB_MASK | ASN1_TFLG_SK_MASK)) {
    asn1_store_ptr(pval, nullptr);
  } else {
    asn1_item_clear(pval, ASN1_ITEM_ptr(tt->item));
  }
}

// NB: could probably combine most of the real XXX_new() behaviour and junk
// all the old functions.

static int ASN1_primitive_new(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  if (!it) {
    return 0;
  }

  // Historically, `it->funcs` for primitive types contained an
  // `ASN1_PRIMITIVE_FUNCS` table of callbacks.
  assert(it->funcs == nullptr);

  int utype;
  if (it->itype == ASN1_ITYPE_MSTRING) {
    utype = -1;
  } else {
    utype = it->utype;
  }
  switch (utype) {
    case V_ASN1_OBJECT:
      asn1_store_ptr(pval, OBJ_get_undef());
      return 1;

    case V_ASN1_BOOLEAN:
      *reinterpret_cast<ASN1_BOOLEAN *>(pval) =
          static_cast<ASN1_BOOLEAN>(it->size);
      return 1;

    case V_ASN1_NULL:
      asn1_store_ptr(pval, reinterpret_cast<ASN1_VALUE *>(1));
      return 1;

    case V_ASN1_ANY: {
      ASN1_TYPE *typ = ASN1_TYPE_new();
      if (!typ) {
        return 0;
      }
      asn1_store_ptr(pval, typ);
      break;
    }

    default:
      asn1_store_ptr(pval, ASN1_STRING_type_new(utype));
      break;
  }
  if (asn1_load_ptr(pval)) {
    return 1;
  }
  return 0;
}

static void asn1_primitive_clear(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  int utype;
  // Historically, `it->funcs` for primitive types contained an
  // `ASN1_PRIMITIVE_FUNCS` table of callbacks.
  assert(it == nullptr || it->funcs == nullptr);
  if (!it || (it->itype == ASN1_ITYPE_MSTRING)) {
    utype = -1;
  } else {
    utype = it->utype;
  }
  if (utype == V_ASN1_BOOLEAN) {
    // `ASN1_BOOLEAN` is not a pointer type.
    *reinterpret_cast<ASN1_BOOLEAN *>(pval) =
        static_cast<ASN1_BOOLEAN>(it->size);
  } else {
    asn1_store_ptr(pval, nullptr);
  }
}
