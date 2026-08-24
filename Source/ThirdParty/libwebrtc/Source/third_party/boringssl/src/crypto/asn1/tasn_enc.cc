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

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/asn1t.h>
#include <openssl/mem.h>

#include "../bytestring/internal.h"
#include "../internal.h"
#include "internal.h"


using namespace bssl;

static int asn1_marshal_item(CBB *cbb, ASN1_VALUE **pval, const ASN1_ITEM *it,
                             CBS_ASN1_TAG tag);
static int asn1_marshal_template(CBB *cbb, ASN1_VALUE **pval,
                                 const ASN1_TEMPLATE *tt, CBS_ASN1_TAG tag);
static int asn1_marshal_template_no_explicit(CBB *cbb, ASN1_VALUE **pval,
                                             const ASN1_TEMPLATE *tt,
                                             CBS_ASN1_TAG tag);
static int asn1_marshal_primitive(CBB *cbb, ASN1_VALUE **pval,
                                  const ASN1_ITEM *it, CBS_ASN1_TAG tag);

static bool asn1_item_has_value(ASN1_VALUE **pval, const ASN1_ITEM *it);
static bool asn1_template_has_value(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);

int ASN1_item_i2d(ASN1_VALUE *val, uint8_t **out, const ASN1_ITEM *it) {
  return I2DFromCBB(/*initial_capacity=*/64, out, [&](CBB *cbb) -> bool {
    return asn1_marshal_item(cbb, &val, it, /*tag=*/0);
  });
}

// asn1_item_has_value returns whether `pval`, which must have type `it`,
// contains a value.
bool asn1_item_has_value(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  // Almost every `ASN1_VALUE` is a pointer, except for types represented as
  // `ASN1_BOOLEAN`.
  if (it->itype != ASN1_ITYPE_PRIMITIVE) {
    // All non-primitive types are pointers.
    return asn1_load_ptr(pval) != nullptr;
  }
  if (it->templates != nullptr) {
    // This is an `ASN1_ITEM_TEMPLATE`. Recurse into the template.
    return asn1_template_has_value(pval, it->templates);
  }
  if (it->utype != V_ASN1_BOOLEAN) {
    return asn1_load_ptr(pval) != nullptr;
  }
  // `ASN1_BOOLEAN` may be omitted in two ways. -1 indicates an omitted
  // OPTIONAL BOOLEAN. Additionally, if `it` is `ASN1_FBOOLEAN` or
  // `ASN1_TBOOLEAN`, this is DEFAULT FALSE and DEFAULT TRUE, respectively. This
  // is stored in `it->size`.
  ASN1_BOOLEAN b = *reinterpret_cast<const ASN1_BOOLEAN *>(pval);
  if (b == ASN1_BOOLEAN_NONE) {
    return false;
  }
  if (it->size > 0) {
    // This is DEFAULT TRUE, so only FALSE is present.
    return b == ASN1_BOOLEAN_FALSE;
  }
  if (it->size == 0) {
    // This is DEFAULT FALSE, so only TRUE is present.
    return b != ASN1_BOOLEAN_FALSE;
  }
  return true;
}

// asn1_template_has_value returns whether `pval`, which must have type `tt`,
// contains a value.
bool asn1_template_has_value(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt) {
  if (tt->flags & ASN1_TFLG_SK_MASK) {
    // If `tt` is a SEQUENCE OF or SET OF type, the C type is `STACK_OF(T)` and
    // is a pointer.
    return asn1_load_ptr(pval) != nullptr;
  }
  // Otherwise, `tt`'s representation is the underlying item.
  return asn1_item_has_value(pval, ASN1_ITEM_ptr(tt->item));
}

// asn1_marshal_item marshals `pval`, of type `it`, and writes the result to
// `cbb`. It returns one on success and zero on error. If `tag` is non-zero, the
// value is implicitly tagged with `tag`.
static int asn1_marshal_item(CBB *cbb, ASN1_VALUE **pval, const ASN1_ITEM *it,
                             CBS_ASN1_TAG tag) {
  // All fields are pointers, except for boolean `ASN1_ITYPE_PRIMITIVE`s. Those
  // cases will be checked later.
  if ((it->itype != ASN1_ITYPE_PRIMITIVE) && !asn1_load_ptr(pval)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
    return 0;
  }

  switch (it->itype) {
    case ASN1_ITYPE_PRIMITIVE:
      if (it->templates) {
        // This is an `ASN1_ITEM_TEMPLATE`, so the underlying template cannot be
        // optional. Optionality should be expressed at the layer above.
        if (it->templates->flags & ASN1_TFLG_OPTIONAL) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
          return 0;
        }
        return asn1_marshal_template(cbb, pval, it->templates, tag);
      }
      return asn1_marshal_primitive(cbb, pval, it, tag);

    case ASN1_ITYPE_MSTRING:
      // CHOICE types cannot be implicitly tagged.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      return asn1_marshal_any_string(cbb, asn1_load_ptr_as<ASN1_STRING>(pval));

    case ASN1_ITYPE_CHOICE: {
      // CHOICE types cannot be implicitly tagged.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      int i = asn1_get_choice_selector(pval, it);
      if (i < 0 || i >= it->tcount) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NO_MATCHING_CHOICE_TYPE);
        return 0;
      }
      const ASN1_TEMPLATE *chtt = &it->templates[i];
      if (chtt->flags & ASN1_TFLG_OPTIONAL) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      ASN1_VALUE **pchval = asn1_get_field_ptr(pval, chtt);
      return asn1_marshal_template(cbb, pchval, chtt, /*tag=*/0);
    }

    case ASN1_ITYPE_EXTERN: {
      // We don't support implicit tagging with external types.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      auto *ef = static_cast<const ASN1_EXTERN_FUNCS *>(it->funcs);
      return ef->asn1_ex_marshal(cbb, pval, it);
    }

    case ASN1_ITYPE_SEQUENCE: {
      Span<const uint8_t> enc;
      if (asn1_enc_restore(&enc, pval, it)) {
        return CBB_add_bytes(cbb, enc.data(), enc.size());
      }
      tag = tag == 0 ? CBS_ASN1_SEQUENCE : (tag | CBS_ASN1_CONSTRUCTED);
      CBB seq;
      if (!CBB_add_asn1(cbb, &seq, tag)) {
        return 0;
      }
      for (int i = 0; i < it->tcount; i++) {
        const ASN1_TEMPLATE *tt = &it->templates[i];
        const ASN1_TEMPLATE *seqtt = asn1_do_adb(pval, tt, /*nullerr=*/1);
        if (!seqtt) {
          return 0;
        }
        ASN1_VALUE **pseqval = asn1_get_field_ptr(pval, seqtt);
        if (!asn1_template_has_value(pseqval, seqtt)) {
          if ((seqtt->flags & ASN1_TFLG_OPTIONAL) != 0) {
            continue;
          }
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
          return 0;
        }
        if (!asn1_marshal_template(&seq, pseqval, seqtt, /*tag=*/0)) {
          return 0;
        }
      }
      return CBB_flush(cbb);
    }

    default:
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return 0;
  }
}

// asn1_marshal_template marshals `pval`, of type `tt`, and writes the result to
// `cbb`. It returns one on success and zero on error. If `tag` is non-zero, the
// value is implicitly tagged with `tag`.
//
// This function does not look at `ASN1_TFLG_OPTIONAL`. The caller is expected
// to have evaluated it already and, if applicable, skip this function.
static int asn1_marshal_template(CBB *cbb, ASN1_VALUE **pval,
                                 const ASN1_TEMPLATE *tt, CBS_ASN1_TAG tag) {
  uint32_t flags = tt->flags;
  if (!(flags & ASN1_TFLG_EXPTAG)) {
    return asn1_marshal_template_no_explicit(cbb, pval, tt, tag);
  }

  // Determine the explicit tag to use. If the caller supplied an implicit tag,
  // it overrides this one. Otherwise, use the tag in the template.
  if (tag == 0) {
    tag = asn1_tag_to_cbs(flags & ASN1_TFLG_TAG_CLASS, tt->tag);
    if (tag == 0) {
      return 0;
    }
  }

  CBB child;
  return CBB_add_asn1(cbb, &child, tag | CBS_ASN1_CONSTRUCTED) &&
         asn1_marshal_template_no_explicit(&child, pval, tt, /*tag=*/0) &&
         CBB_flush(cbb);
}

// asn1_marshal_template_no_explicit behaves like `asn1_marshal_template` except
// it ignores any explicit tagging specified in `tt`.
static int asn1_marshal_template_no_explicit(CBB *cbb, ASN1_VALUE **pval,
                                             const ASN1_TEMPLATE *tt,
                                             CBS_ASN1_TAG tag) {
  uint32_t flags = tt->flags;

  // If `tt` provides implicit tagging and the caller did not already override
  // the tag, pick up the tag from the template.
  if ((flags & ASN1_TFLG_IMPTAG) && tag == 0) {
    tag = asn1_tag_to_cbs(flags & ASN1_TFLG_TAG_CLASS, tt->tag);
    if (tag == 0) {
      return 0;
    }
  }

  if (flags & ASN1_TFLG_SK_MASK) {
    // This is a SET OF or SEQUENCE OF type.
    STACK_OF(ASN1_VALUE) *sk = asn1_load_ptr_as<STACK_OF(ASN1_VALUE)>(pval);
    if (sk == nullptr) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
      return 0;
    }
    bool is_set = (flags & ASN1_TFLG_SET_OF) != 0;
    if (is_set) {
      // Historically, types with both bits set were mutated when serialized to
      // apply the sort. We no longer support this.
      assert((flags & ASN1_TFLG_SEQUENCE_OF) == 0);
    }
    if (tag == 0) {
      tag = is_set ? CBS_ASN1_SET : CBS_ASN1_SEQUENCE;
    }
    CBB child;
    if (!CBB_add_asn1(cbb, &child, tag | CBS_ASN1_CONSTRUCTED)) {
      return 0;
    }
    for (size_t i = 0; i < sk_ASN1_VALUE_num(sk); i++) {
      ASN1_VALUE *elem = sk_ASN1_VALUE_value(sk, i);
      if (!asn1_marshal_item(&child, &elem, ASN1_ITEM_ptr(tt->item),
                             /*tag=*/0)) {
        return 0;
      }
    }
    // If this is a SET OF type, sort the encodings when flushing.
    if (is_set) {
      return CBB_flush_asn1_set_of(&child) && CBB_flush(cbb);
    }
    return CBB_flush(cbb);
  }

  return asn1_marshal_item(cbb, pval, ASN1_ITEM_ptr(tt->item), tag);
}

static int asn1_marshal_primitive(CBB *cbb, ASN1_VALUE **pval,
                                  const ASN1_ITEM *it, CBS_ASN1_TAG tag) {
  assert(it->itype == ASN1_ITYPE_PRIMITIVE);
  // Historically, `it->funcs` for primitive types contained an
  // `ASN1_PRIMITIVE_FUNCS` table of callbacks.
  assert(it->funcs == nullptr);
  if (!asn1_item_has_value(pval, it)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
    return 0;
  }

  // Handle non-string types.
  switch (it->utype) {
    case V_ASN1_ANY:
      // ANY types cannot be implicitly tagged.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      return asn1_marshal_any(cbb, asn1_load_ptr_as<ASN1_TYPE>(pval));
    case V_ASN1_OBJECT:
      return asn1_marshal_object(cbb, asn1_load_ptr_as<ASN1_OBJECT>(pval), tag);
    case V_ASN1_NULL:
      tag = tag == 0 ? CBS_ASN1_NULL : tag;
      return CBB_add_asn1_element(cbb, tag, nullptr, 0);
    case V_ASN1_BOOLEAN: {
      // `ASN1_BOOLEAN` is not a pointer type.
      ASN1_BOOLEAN b = *reinterpret_cast<const ASN1_BOOLEAN *>(pval);
      tag = tag == 0 ? CBS_ASN1_BOOLEAN : tag;
      CBB child;
      return CBB_add_asn1(cbb, &child, tag) &&
             CBB_add_u8(&child, b ? 0xff : 0x00) &&  //
             CBB_flush(cbb);
    }
  }

  // All other supported types are represented as strings.
  const ASN1_STRING *str = asn1_load_ptr_as<ASN1_STRING>(pval);
  switch (it->utype) {
    case V_ASN1_BIT_STRING:
      return asn1_marshal_bit_string(cbb, str, tag);
    case V_ASN1_INTEGER:
      return asn1_marshal_integer(cbb, str, tag);
    case V_ASN1_ENUMERATED:
      tag = tag == 0 ? CBS_ASN1_ENUMERATED : tag;
      return asn1_marshal_integer(cbb, str, tag);
    case V_ASN1_OCTET_STRING:
    case V_ASN1_NUMERICSTRING:
    case V_ASN1_PRINTABLESTRING:
    case V_ASN1_T61STRING:
    case V_ASN1_VIDEOTEXSTRING:
    case V_ASN1_IA5STRING:
    case V_ASN1_UTCTIME:
    case V_ASN1_GENERALIZEDTIME:
    case V_ASN1_GRAPHICSTRING:
    case V_ASN1_VISIBLESTRING:
    case V_ASN1_GENERALSTRING:
    case V_ASN1_UNIVERSALSTRING:
    case V_ASN1_BMPSTRING:
    case V_ASN1_UTF8STRING:
      tag = tag == 0 ? static_cast<CBS_ASN1_TAG>(it->utype) : tag;
      return asn1_marshal_octet_string(cbb, str, tag);
    case V_ASN1_SEQUENCE:
      // The `ASN1_SEQUENCE` item stores the entire element.
      return CBB_add_bytes(cbb, ASN1_STRING_get0_data(str),
                           ASN1_STRING_length(str));
    default:
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return 0;
  }
}
