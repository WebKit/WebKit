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
#include <openssl/asn1t.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/pool.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <algorithm>

#include "../internal.h"
#include "internal.h"


using namespace bssl;

// Constructed types with a recursive definition could eventually exceed the
// stack given malicious input with excessive recursion. We don't currently
// support such types (our PKCS#7 parser does not use this framework and does
// not implement the full recursive PKCS#7), but limit recursion nonetheless.
#define ASN1_MAX_CONSTRUCTED_NEST 30

// The following functions parse a structure of type `it` (or `tt`) from `cbs`
// and write the result to `*pval`. They return one on success and zero on
// error.
//
// If `opt` is true, the field is optional. If an optional element is missing,
// the function returns one and consumes zero bytes from `cbs`.
//
// If `tag` is non-zero, the type is implicitly-tagged with the specified tag.
// `tag` should not have the constructed bit set. If the ASN.1 type is
// constructed, it will be set as appropriate.
//
// On entry, `*pval` may either be null or a default-initialized object,
// according to tasn_new.cc. It, however, may not be an arbitrary object.
// Historically, this code supported arbitrary objects, though not very well, so
// some remnants of more general handling remain.
//
// With the exception of `asn1_parse_item`, there may be a partial object left
// in `*pval` on error. `asn1_parse_item` is the main entrypoint and is
// responsible for recursively freeing objects on error.
static int asn1_parse_item(ASN1_VALUE **pval, CBS *cbs, const ASN1_ITEM *it,
                           CBS_ASN1_TAG tag, bool opt, int depth);
static int asn1_parse_template(ASN1_VALUE **pval, CBS *cbs,
                               const ASN1_TEMPLATE *tt, CBS_ASN1_TAG tag,
                               bool opt, int depth);

// asn1_parse_template_no_explicit behaves like `asn1_parse_template` but
// ignores any explicit tag specified in `tt`.
static int asn1_parse_template_no_explicit(ASN1_VALUE **pval, CBS *cbs,
                                           const ASN1_TEMPLATE *tt,
                                           CBS_ASN1_TAG tag, bool opt,
                                           int depth);

// asn1_parse_item_primitive behaves like `asn1_parse_item` but only supports
// primitive types.
static int asn1_parse_item_primitive(ASN1_VALUE **pval, CBS *cbs,
                                     const ASN1_ITEM *it, CBS_ASN1_TAG tag,
                                     bool opt, int depth);

unsigned long ASN1_tag2bit(int tag) {
  switch (tag) {
    case V_ASN1_BIT_STRING:
      return B_ASN1_BIT_STRING;
    case V_ASN1_OCTET_STRING:
      return B_ASN1_OCTET_STRING;
    case V_ASN1_UTF8STRING:
      return B_ASN1_UTF8STRING;
    case V_ASN1_SEQUENCE:
      return B_ASN1_SEQUENCE;
    case V_ASN1_NUMERICSTRING:
      return B_ASN1_NUMERICSTRING;
    case V_ASN1_PRINTABLESTRING:
      return B_ASN1_PRINTABLESTRING;
    case V_ASN1_T61STRING:
      return B_ASN1_T61STRING;
    case V_ASN1_VIDEOTEXSTRING:
      return B_ASN1_VIDEOTEXSTRING;
    case V_ASN1_IA5STRING:
      return B_ASN1_IA5STRING;
    case V_ASN1_UTCTIME:
      return B_ASN1_UTCTIME;
    case V_ASN1_GENERALIZEDTIME:
      return B_ASN1_GENERALIZEDTIME;
    case V_ASN1_GRAPHICSTRING:
      return B_ASN1_GRAPHICSTRING;
    case V_ASN1_ISO64STRING:
      return B_ASN1_ISO64STRING;
    case V_ASN1_GENERALSTRING:
      return B_ASN1_GENERALSTRING;
    case V_ASN1_UNIVERSALSTRING:
      return B_ASN1_UNIVERSALSTRING;
    case V_ASN1_BMPSTRING:
      return B_ASN1_BMPSTRING;
    default:
      return 0;
  }
}

ASN1_VALUE *ASN1_item_d2i(ASN1_VALUE **pval, const uint8_t **in, long len,
                          const ASN1_ITEM *it) {
  if (len < 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_BUFFER_TOO_SMALL);
    return nullptr;
  }

  // Bound `len` to comfortably fit in an `int`. Historically, this parser
  // routinely mismatched integer types and did not correctly handle overflow.
  // This is now largely fixed, but continue to cap the length to be safe.
  len = std::min(len, long{INT_MAX / 2});

  CBS cbs;
  CBS_init(&cbs, *in, len);
  ASN1_VALUE *ret = nullptr;
  if (!asn1_parse_item(&ret, &cbs, it, /*tag=*/0, /*opt=*/false, /*depth=*/0)) {
    return nullptr;
  }

  // If the caller supplied an output pointer, free the old one and replace it
  // with `ret`. This differs from OpenSSL slightly in that we don't support
  // object reuse. We run this on both success and failure. On failure, even
  // with object reuse, OpenSSL destroys the previous object.
  if (pval != nullptr) {
    ASN1_item_ex_free(pval, it);
    asn1_store_ptr(pval, ret);
  }
  *in = CBS_data(&cbs);
  return ret;
}

template <typename T, T *new_func()>
static T *ensure_value(ASN1_VALUE **pval) {
  T *obj = asn1_load_ptr_as<T>(pval);
  if (obj != nullptr) {
    return obj;
  }
  obj = new_func();
  if (obj == nullptr) {
    return nullptr;
  }
  asn1_store_ptr(pval, obj);
  return obj;
}

static int asn1_parse_item_no_error_cleanup(ASN1_VALUE **pval, CBS *cbs,
                                            const ASN1_ITEM *it,
                                            CBS_ASN1_TAG tag, bool opt,
                                            int depth,
                                            const ASN1_TEMPLATE **errtt) {
  if (++depth > ASN1_MAX_CONSTRUCTED_NEST) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_TOO_DEEP);
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
        return asn1_parse_template(pval, cbs, it->templates, tag, opt, depth);
      }
      return asn1_parse_item_primitive(pval, cbs, it, tag, opt, depth);

    case ASN1_ITYPE_MSTRING: {
      // Multi-strings are CHOICE types and cannot be implicitly tagged.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }

      tag = CBS_peek_any_asn1_tag(cbs);
      if ((tag & CBS_ASN1_CLASS_MASK) != CBS_ASN1_UNIVERSAL ||
          (tag & CBS_ASN1_CONSTRUCTED) ||
          (ASN1_tag2bit(tag & CBS_ASN1_TAG_NUMBER_MASK) & it->utype) == 0) {
        if (opt) {
          ASN1_item_ex_free(pval, it);
          return 1;  // Omitted optional field.
        }
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_MSTRING_WRONG_TAG);
        return 0;
      }
      ASN1_STRING *out = ensure_value<ASN1_STRING, ASN1_STRING_new>(pval);
      return out != nullptr && asn1_parse_any_as_string(cbs, out);
    }

    case ASN1_ITYPE_EXTERN: {
      // We don't support implicit tagging with external types.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }
      auto *ef = reinterpret_cast<const ASN1_EXTERN_FUNCS *>(it->funcs);
      return ef->asn1_ex_parse(pval, cbs, it, opt);
    }

    case ASN1_ITYPE_CHOICE: {
      // CHOICE types cannot be implicitly tagged.
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return 0;
      }

      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != nullptr ? aux->asn1_cb : nullptr;
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, nullptr)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
        return 0;
      }

      if (asn1_load_ptr(pval)) {
        // Free up and zero the CHOICE value if initialised.
        int idx = asn1_get_choice_selector(pval, it);
        if (idx >= 0 && idx < it->tcount) {
          const ASN1_TEMPLATE *tt = &it->templates[idx];
          ASN1_VALUE **pchptr = asn1_get_field_ptr(pval, tt);
          ASN1_template_free(pchptr, tt);
          asn1_set_choice_selector(pval, -1, it);
        }
      } else if (!ASN1_item_ex_new(pval, it)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        return 0;
      }

      // Parse each possibility as an optional field and find the first one.
      size_t orig_len = CBS_len(cbs);
      int idx = 0;
      for (idx = 0; idx < it->tcount; idx++) {
        const ASN1_TEMPLATE *tt = &it->templates[idx];
        ASN1_VALUE **pchptr = asn1_get_field_ptr(pval, tt);
        if (!asn1_parse_template(pchptr, cbs, tt, /*tag=*/0, /*opt=*/true,
                                 depth)) {
          // Free any partial object left in `pchptr`. `asn1_item_parse` will
          // clean up `pval`, but that will not clean up `pchptr` because the
          // choice selector has not yet been set below.
          ASN1_template_free(pchptr, tt);
          *errtt = tt;
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
          return 0;
        }
        if (CBS_len(cbs) != orig_len) {
          break;  // Found a match.
        }
      }
      if (idx == it->tcount) {
        // Nothing matched. If the CHOICE is OPTIONAL, this is OK.
        if (opt) {
          ASN1_item_ex_free(pval, it);
          return 1;
        }
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NO_MATCHING_CHOICE_TYPE);
        return 0;
      }
      asn1_set_choice_selector(pval, idx, it);
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, nullptr)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
        return 0;
      }
      return 1;
    }

    case ASN1_ITYPE_SEQUENCE: {
      tag = tag == 0 ? CBS_ASN1_SEQUENCE : (tag | CBS_ASN1_CONSTRUCTED);
      if (opt && !CBS_peek_asn1_tag(cbs, tag)) {
        ASN1_item_ex_free(pval, it);
        return 1;
      }

      CBS copy = *cbs, seq;
      if (!CBS_get_asn1(cbs, &seq, tag)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
        return 0;
      }

      if (!asn1_load_ptr(pval) && !ASN1_item_ex_new(pval, it)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        return 0;
      }

      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != nullptr ? aux->asn1_cb : nullptr;
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, nullptr)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
        return 0;
      }

      // Free up and zero any ADB found.
      for (int i = 0; i < it->tcount; i++) {
        const ASN1_TEMPLATE *tt = &it->templates[i];
        if (tt->flags & ASN1_TFLG_ADB_MASK) {
          const ASN1_TEMPLATE *seqtt = asn1_do_adb(pval, tt, 0);
          if (seqtt == nullptr) {
            continue;
          }
          ASN1_VALUE **pseqval = asn1_get_field_ptr(pval, seqtt);
          ASN1_template_free(pseqval, seqtt);
        }
      }

      // Get each field entry.
      for (int i = 0; i < it->tcount; i++) {
        const ASN1_TEMPLATE *tt = &it->templates[i];
        const ASN1_TEMPLATE *seqtt = asn1_do_adb(pval, tt, /*nullerr=*/1);
        if (seqtt == nullptr) {
          return 0;
        }
        ASN1_VALUE **pseqval = asn1_get_field_ptr(pval, seqtt);
        const bool field_opt = (tt->flags & ASN1_TFLG_OPTIONAL) != 0;
        if (!asn1_parse_template(pseqval, &seq, seqtt, /*tag=*/0, field_opt,
                                 depth)) {
          *errtt = tt;
          return 0;
        }
      }
      if (CBS_len(&seq) != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_SEQUENCE_LENGTH_MISMATCH);
        return 0;
      }
      // Save a copy of the bytes consumed.
      if (!asn1_enc_save(pval, CBS_data(&copy), CBS_len(&copy) - CBS_len(cbs),
                         it)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
        return 0;
      }
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, nullptr)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
        return 0;
      }
      return 1;
    }

    default:
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return 0;
  }
}

static int asn1_parse_item(ASN1_VALUE **pval, CBS *cbs, const ASN1_ITEM *it,
                           CBS_ASN1_TAG tag, bool opt, int depth) {
  const ASN1_TEMPLATE *errtt = nullptr;
  if (!asn1_parse_item_no_error_cleanup(pval, cbs, it, tag, opt, depth,
                                        &errtt)) {
    ASN1_item_ex_free(pval, it);
    if (errtt != nullptr) {
      ERR_add_error_data(4, "Field=", errtt->field_name, ", Type=", it->sname);
    } else {
      ERR_add_error_data(2, "Type=", it->sname);
    }
    return 0;
  }
  return 1;
}

// Templates are handled with two separate functions. One handles any EXPLICIT
// tag and the other handles the rest.

static int asn1_parse_template(ASN1_VALUE **pval, CBS *cbs,
                               const ASN1_TEMPLATE *tt, CBS_ASN1_TAG tag,
                               bool opt, int depth) {
  uint32_t flags = tt->flags;
  if ((flags & ASN1_TFLG_EXPTAG) == 0) {
    return asn1_parse_template_no_explicit(pval, cbs, tt, tag, opt, depth);
  }

  // Determine the tag.
  if (tag == 0) {
    tag = asn1_tag_to_cbs(flags & ASN1_TFLG_TAG_CLASS, tt->tag);
    if (tag == 0) {
      return 0;
    }
  }
  tag |= CBS_ASN1_CONSTRUCTED;

  if (opt && !CBS_peek_asn1_tag(cbs, tag)) {
    ASN1_template_free(pval, tt);
    return 1;
  }

  CBS child;
  if (!CBS_get_asn1(cbs, &child, tag)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }
  if (!asn1_parse_template_no_explicit(pval, &child, tt, /*tag=*/0,
                                       /*opt=*/false, depth)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
    return 0;
  }
  if (CBS_len(&child) != 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_EXPLICIT_LENGTH_MISMATCH);
    return 0;
  }
  return 1;
}

static int asn1_parse_template_no_explicit(ASN1_VALUE **pval, CBS *cbs,
                                           const ASN1_TEMPLATE *tt,
                                           CBS_ASN1_TAG tag, bool opt,
                                           int depth) {
  // If `tt` provides implicit tagging and the caller did not already override
  // the tag, pick up the tag from the template.
  uint32_t flags = tt->flags;
  if ((flags & ASN1_TFLG_IMPTAG) && tag == 0) {
    tag = asn1_tag_to_cbs(flags & ASN1_TFLG_TAG_CLASS, tt->tag);
    if (tag == 0) {
      return 0;
    }
  }

  if (flags & ASN1_TFLG_SK_MASK) {
    if (tag == 0) {
      tag = (flags & ASN1_TFLG_SET_OF) ? CBS_ASN1_SET : CBS_ASN1_SEQUENCE;
    }
    tag |= CBS_ASN1_CONSTRUCTED;

    if (opt && !CBS_peek_asn1_tag(cbs, tag)) {
      ASN1_template_free(pval, tt);
      return 1;
    }
    CBS seq;
    if (!CBS_get_asn1(cbs, &seq, tag)) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
      return 0;
    }

    STACK_OF(ASN1_VALUE) *sk =
        ensure_value<STACK_OF(ASN1_VALUE), sk_ASN1_VALUE_new_null>(pval);
    if (sk == nullptr) {
      return 0;
    }
    assert(sk_ASN1_VALUE_num(sk) == 0);

    while (CBS_len(&seq) != 0) {
      ASN1_VALUE *skfield = nullptr;
      if (!asn1_parse_item(&skfield, &seq, ASN1_ITEM_ptr(tt->item), /*tag=*/0,
                           /*opt=*/false, depth)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        ASN1_item_ex_free(&skfield, ASN1_ITEM_ptr(tt->item));
        return 0;
      }
      if (!sk_ASN1_VALUE_push(sk, skfield)) {
        ASN1_item_ex_free(&skfield, ASN1_ITEM_ptr(tt->item));
        return 0;
      }
    }
    return 1;
  }

  return asn1_parse_item(pval, cbs, ASN1_ITEM_ptr(tt->item), tag, opt, depth);
}

static int asn1_parse_item_primitive(ASN1_VALUE **pval, CBS *cbs,
                                     const ASN1_ITEM *it, CBS_ASN1_TAG tag,
                                     bool opt, int depth) {
  assert(it->itype == ASN1_ITYPE_PRIMITIVE);
  // `ASN1_ITEM_TEMPLATE` should have been handled by the caller.
  assert(it->templates == nullptr);
  // Historically, `it->funcs` for primitive types contained an
  // `ASN1_PRIMITIVE_FUNCS` table of callbacks.
  assert(it->funcs == nullptr);
  int utype = it->utype;

  // Handle ANY types.
  if (utype == V_ASN1_ANY) {
    if (tag != 0) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_TAGGED_ANY);
      return 0;
    }
    if (opt && CBS_len(cbs) == 0) {
      ASN1_item_ex_free(pval, it);
      return 1;  // Omitted OPTIONAL value.
    }
    ASN1_TYPE *typ = ensure_value<ASN1_TYPE, ASN1_TYPE_new>(pval);
    if (typ == nullptr) {
      return 0;
    }
    return asn1_parse_any(cbs, typ);
  }

  // Determine the ASN.1 tag. `utype` must be a primitive `ASN1_ITEM`, handled
  // by `DECLARE_ASN1_ITEM`.
  assert(0 <= utype && utype <= int{CBS_ASN1_TAG_NUMBER_MASK});
  if (tag == 0) {
    tag = static_cast<CBS_ASN1_TAG>(utype);
  }
  if (utype == V_ASN1_SEQUENCE || utype == V_ASN1_SET) {
    tag |= CBS_ASN1_CONSTRUCTED;
  }

  if (opt && !CBS_peek_asn1_tag(cbs, tag)) {
    ASN1_item_ex_free(pval, it);
    return 1;  // Omitted OPTIONAL value.
  }

  // Handle non-`ASN1_STRING` types.
  switch (utype) {
    case V_ASN1_OBJECT: {
      UniquePtr<ASN1_OBJECT> obj(asn1_parse_object(cbs, tag));
      if (obj == nullptr) {
        return 0;
      }
      ASN1_OBJECT_free(asn1_load_ptr_as<ASN1_OBJECT>(pval));
      asn1_store_ptr(pval, obj.release());
      return 1;
    }
    case V_ASN1_NULL: {
      CBS null;
      if (!CBS_get_asn1(cbs, &null, tag)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
        return 0;
      }
      if (CBS_len(&null) != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NULL_IS_WRONG_LENGTH);
        return 0;
      }
      asn1_store_ptr(pval, reinterpret_cast<ASN1_VALUE *>(1));
      return 1;
    }
    case V_ASN1_BOOLEAN: {
      CBS child;
      if (!CBS_get_asn1(cbs, &child, tag)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
        return 0;
      }
      // TODO(crbug.com/42290221): Reject invalid BOOLEAN encodings and just
      // call `CBS_get_asn1_bool`.
      if (CBS_len(&child) != 1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BOOLEAN_IS_WRONG_LENGTH);
        return 0;
      }
      *reinterpret_cast<ASN1_BOOLEAN *>(pval) =
          CBS_data(&child)[0] ? ASN1_BOOLEAN_TRUE : ASN1_BOOLEAN_FALSE;
      return 1;
    }
  }

  // All other types as an `ASN1_STRING` representation.
  ASN1_STRING *str = ensure_value<ASN1_STRING, ASN1_STRING_new>(pval);
  if (str == nullptr) {
    return 0;
  }

  switch (utype) {
    case V_ASN1_BIT_STRING:
      return asn1_parse_bit_string(cbs, str, tag);
    case V_ASN1_INTEGER:
      return asn1_parse_integer(cbs, str, tag);
    case V_ASN1_ENUMERATED:
      return asn1_parse_enumerated(cbs, str, tag);
    case V_ASN1_UNIVERSALSTRING:
      return asn1_parse_universal_string(cbs, str, tag);
    case V_ASN1_BMPSTRING:
      return asn1_parse_bmp_string(cbs, str, tag);
    case V_ASN1_UTF8STRING:
      return asn1_parse_utf8_string(cbs, str, tag);
    case V_ASN1_UTCTIME:
      // TODO(crbug.com/42290221): Reject timezone offsets. We need to parse
      // invalid timestamps in `X509` objects, but that parser no longer uses
      // this code.
      return asn1_parse_utc_time(cbs, str, tag, /*allow_timezone_offset=*/1);
    case V_ASN1_GENERALIZEDTIME:
      return asn1_parse_generalized_time(cbs, str, tag);
    case V_ASN1_OCTET_STRING:
      return asn1_parse_octet_string(cbs, str, tag);
    case V_ASN1_T61STRING:
      return asn1_parse_t61_string(cbs, str, tag);
    case V_ASN1_NUMERICSTRING:
    case V_ASN1_PRINTABLESTRING:
    case V_ASN1_VIDEOTEXSTRING:
    case V_ASN1_IA5STRING:
    case V_ASN1_GRAPHICSTRING:
    case V_ASN1_VISIBLESTRING:
    case V_ASN1_GENERALSTRING:
      // TODO(crbug.com/42290290): Enforce the encoding of the other string
      // types.
      return asn1_parse_string_unchecked(cbs, str, utype, tag);
    case V_ASN1_SEQUENCE: {
      // Save the entire element in the string.
      CBS elem;
      if (!CBS_get_asn1_element(cbs, &elem, tag)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
        return 0;
      }
      str->type = V_ASN1_SEQUENCE;
      return ASN1_STRING_set(str, CBS_data(&elem), CBS_len(&elem));
    }
    default:
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return 0;
  }
}
