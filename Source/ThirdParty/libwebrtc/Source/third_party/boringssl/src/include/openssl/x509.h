/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#ifndef HEADER_X509_H
#define HEADER_X509_H

#include <openssl/asn1.h>
#include <openssl/base.h>
#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/dh.h>
#include <openssl/dsa.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj.h>
#include <openssl/pkcs7.h>
#include <openssl/pool.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/thread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


// Legacy X.509 library.
//
// This header is part of OpenSSL's X.509 implementation. It is retained for
// compatibility but otherwise underdocumented and not actively maintained. In
// the future, a replacement library will be available. Meanwhile, minimize
// dependencies on this header where possible.


#define X509_FILETYPE_PEM 1
#define X509_FILETYPE_ASN1 2
#define X509_FILETYPE_DEFAULT 3

#define X509v3_KU_DIGITAL_SIGNATURE 0x0080
#define X509v3_KU_NON_REPUDIATION 0x0040
#define X509v3_KU_KEY_ENCIPHERMENT 0x0020
#define X509v3_KU_DATA_ENCIPHERMENT 0x0010
#define X509v3_KU_KEY_AGREEMENT 0x0008
#define X509v3_KU_KEY_CERT_SIGN 0x0004
#define X509v3_KU_CRL_SIGN 0x0002
#define X509v3_KU_ENCIPHER_ONLY 0x0001
#define X509v3_KU_DECIPHER_ONLY 0x8000
#define X509v3_KU_UNDEF 0xffff

DEFINE_STACK_OF(X509_ALGOR)
DECLARE_ASN1_SET_OF(X509_ALGOR)

typedef STACK_OF(X509_ALGOR) X509_ALGORS;

struct X509_val_st {
  ASN1_TIME *notBefore;
  ASN1_TIME *notAfter;
} /* X509_VAL */;

struct X509_pubkey_st {
  X509_ALGOR *algor;
  ASN1_BIT_STRING *public_key;
  EVP_PKEY *pkey;
};

struct X509_sig_st {
  X509_ALGOR *algor;
  ASN1_OCTET_STRING *digest;
} /* X509_SIG */;

struct X509_name_entry_st {
  ASN1_OBJECT *object;
  ASN1_STRING *value;
  int set;
  int size;  // temp variable
} /* X509_NAME_ENTRY */;

DEFINE_STACK_OF(X509_NAME_ENTRY)
DECLARE_ASN1_SET_OF(X509_NAME_ENTRY)

// we always keep X509_NAMEs in 2 forms.
struct X509_name_st {
  STACK_OF(X509_NAME_ENTRY) * entries;
  int modified;  // true if 'bytes' needs to be built
  BUF_MEM *bytes;
  // unsigned long hash; Keep the hash around for lookups
  unsigned char *canon_enc;
  int canon_enclen;
} /* X509_NAME */;

DEFINE_STACK_OF(X509_NAME)

struct X509_extension_st {
  ASN1_OBJECT *object;
  ASN1_BOOLEAN critical;
  ASN1_OCTET_STRING *value;
} /* X509_EXTENSION */;

typedef STACK_OF(X509_EXTENSION) X509_EXTENSIONS;

DEFINE_STACK_OF(X509_EXTENSION)
DECLARE_ASN1_SET_OF(X509_EXTENSION)

// a sequence of these are used
struct x509_attributes_st {
  ASN1_OBJECT *object;
  int single;  // 0 for a set, 1 for a single item (which is wrong)
  union {
    char *ptr;
    /* 0 */ STACK_OF(ASN1_TYPE) * set;
    /* 1 */ ASN1_TYPE *single;
  } value;
} /* X509_ATTRIBUTE */;

DEFINE_STACK_OF(X509_ATTRIBUTE)
DECLARE_ASN1_SET_OF(X509_ATTRIBUTE)


struct X509_req_info_st {
  ASN1_ENCODING enc;
  ASN1_INTEGER *version;
  X509_NAME *subject;
  X509_PUBKEY *pubkey;
  //  d=2 hl=2 l=  0 cons: cont: 00
  STACK_OF(X509_ATTRIBUTE) * attributes;  // [ 0 ]
} /* X509_REQ_INFO */;

struct X509_req_st {
  X509_REQ_INFO *req_info;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
  CRYPTO_refcount_t references;
} /* X509_REQ */;

struct x509_cinf_st {
  ASN1_INTEGER *version;  // [ 0 ] default of v1
  ASN1_INTEGER *serialNumber;
  X509_ALGOR *signature;
  X509_NAME *issuer;
  X509_VAL *validity;
  X509_NAME *subject;
  X509_PUBKEY *key;
  ASN1_BIT_STRING *issuerUID;             // [ 1 ] optional in v2
  ASN1_BIT_STRING *subjectUID;            // [ 2 ] optional in v2
  STACK_OF(X509_EXTENSION) * extensions;  // [ 3 ] optional in v3
  ASN1_ENCODING enc;
} /* X509_CINF */;

// This stuff is certificate "auxiliary info"
// it contains details which are useful in certificate
// stores and databases. When used this is tagged onto
// the end of the certificate itself

struct x509_cert_aux_st {
  STACK_OF(ASN1_OBJECT) * trust;   // trusted uses
  STACK_OF(ASN1_OBJECT) * reject;  // rejected uses
  ASN1_UTF8STRING *alias;          // "friendly name"
  ASN1_OCTET_STRING *keyid;        // key id of private key
  STACK_OF(X509_ALGOR) * other;    // other unspecified info
} /* X509_CERT_AUX */;

DECLARE_STACK_OF(DIST_POINT)
DECLARE_STACK_OF(GENERAL_NAME)

struct x509_st {
  X509_CINF *cert_info;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
  CRYPTO_refcount_t references;
  CRYPTO_EX_DATA ex_data;
  // These contain copies of various extension values
  long ex_pathlen;
  long ex_pcpathlen;
  unsigned long ex_flags;
  unsigned long ex_kusage;
  unsigned long ex_xkusage;
  unsigned long ex_nscert;
  ASN1_OCTET_STRING *skid;
  AUTHORITY_KEYID *akid;
  X509_POLICY_CACHE *policy_cache;
  STACK_OF(DIST_POINT) * crldp;
  STACK_OF(GENERAL_NAME) * altname;
  NAME_CONSTRAINTS *nc;
  unsigned char sha1_hash[SHA_DIGEST_LENGTH];
  X509_CERT_AUX *aux;
  CRYPTO_BUFFER *buf;
  CRYPTO_MUTEX lock;
} /* X509 */;

DEFINE_STACK_OF(X509)
DECLARE_ASN1_SET_OF(X509)

// This is used for a table of trust checking functions

struct x509_trust_st {
  int trust;
  int flags;
  int (*check_trust)(struct x509_trust_st *, X509 *, int);
  char *name;
  int arg1;
  void *arg2;
} /* X509_TRUST */;

DEFINE_STACK_OF(X509_TRUST)

// standard trust ids

#define X509_TRUST_DEFAULT (-1)  // Only valid in purpose settings

#define X509_TRUST_COMPAT 1
#define X509_TRUST_SSL_CLIENT 2
#define X509_TRUST_SSL_SERVER 3
#define X509_TRUST_EMAIL 4
#define X509_TRUST_OBJECT_SIGN 5
#define X509_TRUST_OCSP_SIGN 6
#define X509_TRUST_OCSP_REQUEST 7
#define X509_TRUST_TSA 8

// Keep these up to date!
#define X509_TRUST_MIN 1
#define X509_TRUST_MAX 8


// trust_flags values
#define X509_TRUST_DYNAMIC 1
#define X509_TRUST_DYNAMIC_NAME 2

// check_trust return codes

#define X509_TRUST_TRUSTED 1
#define X509_TRUST_REJECTED 2
#define X509_TRUST_UNTRUSTED 3

// Flags for X509_print_ex()

#define X509_FLAG_COMPAT 0
#define X509_FLAG_NO_HEADER 1L
#define X509_FLAG_NO_VERSION (1L << 1)
#define X509_FLAG_NO_SERIAL (1L << 2)
#define X509_FLAG_NO_SIGNAME (1L << 3)
#define X509_FLAG_NO_ISSUER (1L << 4)
#define X509_FLAG_NO_VALIDITY (1L << 5)
#define X509_FLAG_NO_SUBJECT (1L << 6)
#define X509_FLAG_NO_PUBKEY (1L << 7)
#define X509_FLAG_NO_EXTENSIONS (1L << 8)
#define X509_FLAG_NO_SIGDUMP (1L << 9)
#define X509_FLAG_NO_AUX (1L << 10)
#define X509_FLAG_NO_ATTRIBUTES (1L << 11)
#define X509_FLAG_NO_IDS (1L << 12)

// Flags specific to X509_NAME_print_ex()

// The field separator information

#define XN_FLAG_SEP_MASK (0xf << 16)

#define XN_FLAG_COMPAT 0  // Traditional SSLeay: use old X509_NAME_print
#define XN_FLAG_SEP_COMMA_PLUS (1 << 16)  // RFC2253 ,+
#define XN_FLAG_SEP_CPLUS_SPC (2 << 16)   // ,+ spaced: more readable
#define XN_FLAG_SEP_SPLUS_SPC (3 << 16)   // ;+ spaced
#define XN_FLAG_SEP_MULTILINE (4 << 16)   // One line per field

#define XN_FLAG_DN_REV (1 << 20)  // Reverse DN order

// How the field name is shown

#define XN_FLAG_FN_MASK (0x3 << 21)

#define XN_FLAG_FN_SN 0            // Object short name
#define XN_FLAG_FN_LN (1 << 21)    // Object long name
#define XN_FLAG_FN_OID (2 << 21)   // Always use OIDs
#define XN_FLAG_FN_NONE (3 << 21)  // No field names

#define XN_FLAG_SPC_EQ (1 << 23)  // Put spaces round '='

// This determines if we dump fields we don't recognise:
// RFC2253 requires this.

#define XN_FLAG_DUMP_UNKNOWN_FIELDS (1 << 24)

#define XN_FLAG_FN_ALIGN (1 << 25)  // Align field names to 20 characters

// Complete set of RFC2253 flags

#define XN_FLAG_RFC2253                                             \
  (ASN1_STRFLGS_RFC2253 | XN_FLAG_SEP_COMMA_PLUS | XN_FLAG_DN_REV | \
   XN_FLAG_FN_SN | XN_FLAG_DUMP_UNKNOWN_FIELDS)

// readable oneline form

#define XN_FLAG_ONELINE                                                    \
  (ASN1_STRFLGS_RFC2253 | ASN1_STRFLGS_ESC_QUOTE | XN_FLAG_SEP_CPLUS_SPC | \
   XN_FLAG_SPC_EQ | XN_FLAG_FN_SN)

// readable multiline form

#define XN_FLAG_MULTILINE                                                 \
  (ASN1_STRFLGS_ESC_CTRL | ASN1_STRFLGS_ESC_MSB | XN_FLAG_SEP_MULTILINE | \
   XN_FLAG_SPC_EQ | XN_FLAG_FN_LN | XN_FLAG_FN_ALIGN)

struct x509_revoked_st {
  ASN1_INTEGER *serialNumber;
  ASN1_TIME *revocationDate;
  STACK_OF(X509_EXTENSION) /* optional */ * extensions;
  // Set up if indirect CRL
  STACK_OF(GENERAL_NAME) * issuer;
  // Revocation reason
  int reason;
  int sequence;  // load sequence
};

DEFINE_STACK_OF(X509_REVOKED)
DECLARE_ASN1_SET_OF(X509_REVOKED)

struct X509_crl_info_st {
  ASN1_INTEGER *version;
  X509_ALGOR *sig_alg;
  X509_NAME *issuer;
  ASN1_TIME *lastUpdate;
  ASN1_TIME *nextUpdate;
  STACK_OF(X509_REVOKED) * revoked;
  STACK_OF(X509_EXTENSION) /* [0] */ * extensions;
  ASN1_ENCODING enc;
} /* X509_CRL_INFO */;

DECLARE_STACK_OF(GENERAL_NAMES)

struct X509_crl_st {
  // actual signature
  X509_CRL_INFO *crl;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
  CRYPTO_refcount_t references;
  int flags;
  // Copies of various extensions
  AUTHORITY_KEYID *akid;
  ISSUING_DIST_POINT *idp;
  // Convenient breakdown of IDP
  int idp_flags;
  int idp_reasons;
  // CRL and base CRL numbers for delta processing
  ASN1_INTEGER *crl_number;
  ASN1_INTEGER *base_crl_number;
  unsigned char sha1_hash[SHA_DIGEST_LENGTH];
  STACK_OF(GENERAL_NAMES) * issuers;
  const X509_CRL_METHOD *meth;
  void *meth_data;
} /* X509_CRL */;

DEFINE_STACK_OF(X509_CRL)
DECLARE_ASN1_SET_OF(X509_CRL)

struct private_key_st {
  int version;
  // The PKCS#8 data types
  X509_ALGOR *enc_algor;
  ASN1_OCTET_STRING *enc_pkey;  // encrypted pub key

  // When decrypted, the following will not be NULL
  EVP_PKEY *dec_pkey;

  // used to encrypt and decrypt
  int key_length;
  char *key_data;
  int key_free;  // true if we should auto free key_data

  // expanded version of 'enc_algor'
  EVP_CIPHER_INFO cipher;
} /* X509_PKEY */;

#ifndef OPENSSL_NO_EVP
struct X509_info_st {
  X509 *x509;
  X509_CRL *crl;
  X509_PKEY *x_pkey;

  EVP_CIPHER_INFO enc_cipher;
  int enc_len;
  char *enc_data;

} /* X509_INFO */;

DEFINE_STACK_OF(X509_INFO)
#endif

// The next 2 structures and their 8 routines were sent to me by
// Pat Richard <patr@x509.com> and are used to manipulate
// Netscapes spki structures - useful if you are writing a CA web page
struct Netscape_spkac_st {
  X509_PUBKEY *pubkey;
  ASN1_IA5STRING *challenge;  // challenge sent in atlas >= PR2
} /* NETSCAPE_SPKAC */;

struct Netscape_spki_st {
  NETSCAPE_SPKAC *spkac;  // signed public key and challenge
  X509_ALGOR *sig_algor;
  ASN1_BIT_STRING *signature;
} /* NETSCAPE_SPKI */;

#ifdef __cplusplus
}
#endif

#include <openssl/x509_vfy.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO(davidben): Document remaining functions, reorganize them, and define
// supported patterns for using |X509| objects in general. In particular, when
// it is safe to call mutating functions is a little tricky due to various
// internal caches.

// X509_get_version returns the numerical value of |x509|'s version. That is,
// it returns zero for X.509v1, one for X.509v2, and two for X.509v3. Unknown
// versions are rejected by the parser, but a manually-created |X509| object may
// encode invalid versions. In that case, the function will return the invalid
// version, or -1 on overflow.
OPENSSL_EXPORT long X509_get_version(const X509 *x509);

// X509_get0_serialNumber returns |x509|'s serial number.
OPENSSL_EXPORT const ASN1_INTEGER *X509_get0_serialNumber(const X509 *x509);

// X509_get0_notBefore returns |x509|'s notBefore time.
OPENSSL_EXPORT const ASN1_TIME *X509_get0_notBefore(const X509 *x509);

// X509_get0_notAfter returns |x509|'s notAfter time.
OPENSSL_EXPORT const ASN1_TIME *X509_get0_notAfter(const X509 *x509);

// X509_set1_notBefore sets |x509|'s notBefore time to |tm|. It returns one on
// success and zero on error.
OPENSSL_EXPORT int X509_set1_notBefore(X509 *x509, const ASN1_TIME *tm);

// X509_set1_notAfter sets |x509|'s notAfter time to |tm|. it returns one on
// success and zero on error.
OPENSSL_EXPORT int X509_set1_notAfter(X509 *x509, const ASN1_TIME *tm);

// X509_getm_notBefore returns a mutable pointer to |x509|'s notBefore time.
OPENSSL_EXPORT ASN1_TIME *X509_getm_notBefore(X509 *x509);

// X509_getm_notAfter returns a mutable pointer to |x509|'s notAfter time.
OPENSSL_EXPORT ASN1_TIME *X509_getm_notAfter(X509 *x);

// X509_get_notBefore returns |x509|'s notBefore time. Note this function is not
// const-correct for legacy reasons. Use |X509_get0_notBefore| or
// |X509_getm_notBefore| instead.
OPENSSL_EXPORT ASN1_TIME *X509_get_notBefore(const X509 *x509);

// X509_get_notAfter returns |x509|'s notAfter time. Note this function is not
// const-correct for legacy reasons. Use |X509_get0_notAfter| or
// |X509_getm_notAfter| instead.
OPENSSL_EXPORT ASN1_TIME *X509_get_notAfter(const X509 *x509);

// X509_set_notBefore calls |X509_set1_notBefore|. Use |X509_set1_notBefore|
// instead.
OPENSSL_EXPORT int X509_set_notBefore(X509 *x509, const ASN1_TIME *tm);

// X509_set_notAfter calls |X509_set1_notAfter|. Use |X509_set1_notAfter|
// instead.
OPENSSL_EXPORT int X509_set_notAfter(X509 *x509, const ASN1_TIME *tm);

// X509_get0_uids sets |*out_issuer_uid| and |*out_subject_uid| to non-owning
// pointers to the issuerUID and subjectUID fields, respectively, of |x509|.
// Either output pointer may be NULL to skip the field.
OPENSSL_EXPORT void X509_get0_uids(const X509 *x509,
                                   const ASN1_BIT_STRING **out_issuer_uid,
                                   const ASN1_BIT_STRING **out_subject_uid);

// X509_get_cert_info returns |x509|'s TBSCertificate structure. Note this
// function is not const-correct for legacy reasons.
//
// This function is deprecated and may be removed in the future. It is not
// present in OpenSSL and constrains some improvements to the library.
OPENSSL_EXPORT X509_CINF *X509_get_cert_info(const X509 *x509);

// X509_extract_key is a legacy alias to |X509_get_pubkey|. Use
// |X509_get_pubkey| instead.
#define X509_extract_key(x) X509_get_pubkey(x)

// X509_get_pathlen returns path length constraint from the basic constraints
// extension in |x509|. (See RFC5280, section 4.2.1.9.) It returns -1 if the
// constraint is not present, or if some extension in |x509| was invalid.
//
// Note that decoding an |X509| object will not check for invalid extensions. To
// detect the error case, call |X509_get_extensions_flags| and check the
// |EXFLAG_INVALID| bit.
OPENSSL_EXPORT long X509_get_pathlen(X509 *x509);

// X509_REQ_get_version returns the numerical value of |req|'s version. That is,
// it returns zero for a v1 request. If |req| is invalid, it may return another
// value, or -1 on overflow.
OPENSSL_EXPORT long X509_REQ_get_version(const X509_REQ *req);

// X509_REQ_get_subject_name returns |req|'s subject name. Note this function is
// not const-correct for legacy reasons.
OPENSSL_EXPORT X509_NAME *X509_REQ_get_subject_name(const X509_REQ *req);

// X509_REQ_extract_key is a legacy alias for |X509_REQ_get_pubkey|.
#define X509_REQ_extract_key(a) X509_REQ_get_pubkey(a)

// X509_name_cmp is a legacy alias for |X509_NAME_cmp|.
#define X509_name_cmp(a, b) X509_NAME_cmp((a), (b))

// X509_REQ_get_version returns the numerical value of |crl|'s version. That is,
// it returns zero for a v1 CRL and one for a v2 CRL. If |crl| is invalid, it
// may return another value, or -1 on overflow.
OPENSSL_EXPORT long X509_CRL_get_version(const X509_CRL *crl);

// X509_CRL_get0_lastUpdate returns |crl|'s lastUpdate time.
OPENSSL_EXPORT const ASN1_TIME *X509_CRL_get0_lastUpdate(const X509_CRL *crl);

// X509_CRL_get0_nextUpdate returns |crl|'s nextUpdate time, or NULL if |crl|
// has none.
OPENSSL_EXPORT const ASN1_TIME *X509_CRL_get0_nextUpdate(const X509_CRL *crl);

// X509_CRL_set1_lastUpdate sets |crl|'s lastUpdate time to |tm|. It returns one
// on success and zero on error.
OPENSSL_EXPORT int X509_CRL_set1_lastUpdate(X509_CRL *crl, const ASN1_TIME *tm);

// X509_CRL_set1_nextUpdate sets |crl|'s nextUpdate time to |tm|. It returns one
// on success and zero on error.
OPENSSL_EXPORT int X509_CRL_set1_nextUpdate(X509_CRL *crl, const ASN1_TIME *tm);

// The following symbols are deprecated aliases to |X509_CRL_set1_*|.
#define X509_CRL_set_lastUpdate X509_CRL_set1_lastUpdate
#define X509_CRL_set_nextUpdate X509_CRL_set1_nextUpdate

// X509_CRL_get_lastUpdate returns a mutable pointer to |crl|'s lastUpdate time.
// Use |X509_CRL_get0_lastUpdate| or |X509_CRL_set1_lastUpdate| instead.
OPENSSL_EXPORT ASN1_TIME *X509_CRL_get_lastUpdate(X509_CRL *crl);

// X509_CRL_get_nextUpdate returns a mutable pointer to |crl|'s nextUpdate time,
// or NULL if |crl| has none. Use |X509_CRL_get0_nextUpdate| or
// |X509_CRL_set1_nextUpdate| instead.
OPENSSL_EXPORT ASN1_TIME *X509_CRL_get_nextUpdate(X509_CRL *crl);

// X509_CRL_get_issuer returns |crl|'s issuer name. Note this function is not
// const-correct for legacy reasons.
OPENSSL_EXPORT X509_NAME *X509_CRL_get_issuer(const X509_CRL *crl);

// X509_CRL_get_REVOKED returns the list of revoked certificates in |crl|.
//
// TOOD(davidben): This function was originally a macro, without clear const
// semantics. It should take a const input and give const output, but the latter
// would break existing callers. For now, we match upstream.
OPENSSL_EXPORT STACK_OF(X509_REVOKED) *X509_CRL_get_REVOKED(X509_CRL *crl);

// X509_CRL_get0_extensions returns |crl|'s extension list.
OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *
    X509_CRL_get0_extensions(const X509_CRL *crl);

// X509_CINF_set_modified marks |cinf| as modified so that changes will be
// reflected in serializing the structure.
//
// This function is deprecated and may be removed in the future. It is not
// present in OpenSSL and constrains some improvements to the library.
OPENSSL_EXPORT void X509_CINF_set_modified(X509_CINF *cinf);

// X509_CINF_get_signature returns the signature algorithm in |cinf|. Note this
// isn't the signature itself, but the extra copy of the signature algorithm
// in the TBSCertificate.
//
// This function is deprecated and may be removed in the future. It is not
// present in OpenSSL and constrains some improvements to the library. Use
// |X509_get0_tbs_sigalg| instead.
OPENSSL_EXPORT const X509_ALGOR *X509_CINF_get_signature(const X509_CINF *cinf);

// X509_SIG_get0 sets |*out_alg| and |*out_digest| to non-owning pointers to
// |sig|'s algorithm and digest fields, respectively. Either |out_alg| and
// |out_digest| may be NULL to skip those fields.
OPENSSL_EXPORT void X509_SIG_get0(const X509_SIG *sig,
                                  const X509_ALGOR **out_alg,
                                  const ASN1_OCTET_STRING **out_digest);

// X509_SIG_getm behaves like |X509_SIG_get0| but returns mutable pointers.
OPENSSL_EXPORT void X509_SIG_getm(X509_SIG *sig, X509_ALGOR **out_alg,
                                  ASN1_OCTET_STRING **out_digest);

OPENSSL_EXPORT void X509_CRL_set_default_method(const X509_CRL_METHOD *meth);
OPENSSL_EXPORT X509_CRL_METHOD *X509_CRL_METHOD_new(
    int (*crl_init)(X509_CRL *crl), int (*crl_free)(X509_CRL *crl),
    int (*crl_lookup)(X509_CRL *crl, X509_REVOKED **ret, ASN1_INTEGER *ser,
                      X509_NAME *issuer),
    int (*crl_verify)(X509_CRL *crl, EVP_PKEY *pk));
OPENSSL_EXPORT void X509_CRL_METHOD_free(X509_CRL_METHOD *m);

OPENSSL_EXPORT void X509_CRL_set_meth_data(X509_CRL *crl, void *dat);
OPENSSL_EXPORT void *X509_CRL_get_meth_data(X509_CRL *crl);

// X509_get_X509_PUBKEY returns the public key of |x509|. Note this function is
// not const-correct for legacy reasons. Callers should not modify the returned
// object.
OPENSSL_EXPORT X509_PUBKEY *X509_get_X509_PUBKEY(const X509 *x509);

// X509_verify_cert_error_string returns |err| as a human-readable string, where
// |err| should be one of the |X509_V_*| values. If |err| is unknown, it returns
// a default description.
//
// TODO(davidben): Move this function to x509_vfy.h, with the |X509_V_*|
// definitions, or fold x509_vfy.h into this function.
OPENSSL_EXPORT const char *X509_verify_cert_error_string(long err);

// X509_verify checks that |x509| has a valid signature by |pkey|. It returns
// one if the signature is valid and zero otherwise. Note this function only
// checks the signature itself and does not perform a full certificate
// validation.
OPENSSL_EXPORT int X509_verify(X509 *x509, EVP_PKEY *pkey);

// X509_REQ_verify checks that |req| has a valid signature by |pkey|. It returns
// one if the signature is valid and zero otherwise.
OPENSSL_EXPORT int X509_REQ_verify(X509_REQ *req, EVP_PKEY *pkey);

// X509_CRL_verify checks that |crl| has a valid signature by |pkey|. It returns
// one if the signature is valid and zero otherwise.
OPENSSL_EXPORT int X509_CRL_verify(X509_CRL *crl, EVP_PKEY *pkey);

// NETSCAPE_SPKI_verify checks that |spki| has a valid signature by |pkey|. It
// returns one if the signature is valid and zero otherwise.
OPENSSL_EXPORT int NETSCAPE_SPKI_verify(NETSCAPE_SPKI *spki, EVP_PKEY *pkey);

// NETSCAPE_SPKI_b64_decode decodes |len| bytes from |str| as a base64-encoded
// Netscape signed public key and challenge (SPKAC) structure. It returns a
// newly-allocated |NETSCAPE_SPKI| structure with the result, or NULL on error.
// If |len| is 0 or negative, the length is calculated with |strlen| and |str|
// must be a NUL-terminated C string.
OPENSSL_EXPORT NETSCAPE_SPKI *NETSCAPE_SPKI_b64_decode(const char *str,
                                                       int len);

// NETSCAPE_SPKI_b64_encode encodes |spki| as a base64-encoded Netscape signed
// public key and challenge (SPKAC) structure. It returns a newly-allocated
// NUL-terminated C string with the result, or NULL on error. The caller must
// release the memory with |OPENSSL_free| when done.
OPENSSL_EXPORT char *NETSCAPE_SPKI_b64_encode(NETSCAPE_SPKI *spki);

// NETSCAPE_SPKI_get_pubkey decodes and returns the public key in |spki| as an
// |EVP_PKEY|, or NULL on error. The resulting pointer is non-owning and valid
// until |spki| is released or mutated. The caller should take a reference with
// |EVP_PKEY_up_ref| to extend the lifetime.
OPENSSL_EXPORT EVP_PKEY *NETSCAPE_SPKI_get_pubkey(NETSCAPE_SPKI *spki);

// NETSCAPE_SPKI_set_pubkey sets |spki|'s public key to |pkey|. It returns one
// on success or zero on error. This function does not take ownership of |pkey|,
// so the caller may continue to manage its lifetime independently of |spki|.
OPENSSL_EXPORT int NETSCAPE_SPKI_set_pubkey(NETSCAPE_SPKI *spki,
                                            EVP_PKEY *pkey);

// X509_signature_dump writes a human-readable representation of |sig| to |bio|,
// indented with |indent| spaces. It returns one on success and zero on error.
OPENSSL_EXPORT int X509_signature_dump(BIO *bio, const ASN1_STRING *sig,
                                       int indent);

// X509_signature_print writes a human-readable representation of |alg| and
// |sig| to |bio|. It returns one on success and zero on error.
OPENSSL_EXPORT int X509_signature_print(BIO *bio, const X509_ALGOR *alg,
                                        const ASN1_STRING *sig);

OPENSSL_EXPORT int X509_sign(X509 *x, EVP_PKEY *pkey, const EVP_MD *md);
OPENSSL_EXPORT int X509_sign_ctx(X509 *x, EVP_MD_CTX *ctx);
OPENSSL_EXPORT int X509_REQ_sign(X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md);
OPENSSL_EXPORT int X509_REQ_sign_ctx(X509_REQ *x, EVP_MD_CTX *ctx);
OPENSSL_EXPORT int X509_CRL_sign(X509_CRL *x, EVP_PKEY *pkey, const EVP_MD *md);
OPENSSL_EXPORT int X509_CRL_sign_ctx(X509_CRL *x, EVP_MD_CTX *ctx);
OPENSSL_EXPORT int NETSCAPE_SPKI_sign(NETSCAPE_SPKI *x, EVP_PKEY *pkey,
                                      const EVP_MD *md);

OPENSSL_EXPORT int X509_pubkey_digest(const X509 *data, const EVP_MD *type,
                                      unsigned char *md, unsigned int *len);
OPENSSL_EXPORT int X509_digest(const X509 *data, const EVP_MD *type,
                               unsigned char *md, unsigned int *len);
OPENSSL_EXPORT int X509_CRL_digest(const X509_CRL *data, const EVP_MD *type,
                                   unsigned char *md, unsigned int *len);
OPENSSL_EXPORT int X509_REQ_digest(const X509_REQ *data, const EVP_MD *type,
                                   unsigned char *md, unsigned int *len);
OPENSSL_EXPORT int X509_NAME_digest(const X509_NAME *data, const EVP_MD *type,
                                    unsigned char *md, unsigned int *len);

// X509_parse_from_buffer parses an X.509 structure from |buf| and returns a
// fresh X509 or NULL on error. There must not be any trailing data in |buf|.
// The returned structure (if any) holds a reference to |buf| rather than
// copying parts of it as a normal |d2i_X509| call would do.
OPENSSL_EXPORT X509 *X509_parse_from_buffer(CRYPTO_BUFFER *buf);

#ifndef OPENSSL_NO_FP_API
OPENSSL_EXPORT X509 *d2i_X509_fp(FILE *fp, X509 **x509);
OPENSSL_EXPORT int i2d_X509_fp(FILE *fp, X509 *x509);
OPENSSL_EXPORT X509_CRL *d2i_X509_CRL_fp(FILE *fp, X509_CRL **crl);
OPENSSL_EXPORT int i2d_X509_CRL_fp(FILE *fp, X509_CRL *crl);
OPENSSL_EXPORT X509_REQ *d2i_X509_REQ_fp(FILE *fp, X509_REQ **req);
OPENSSL_EXPORT int i2d_X509_REQ_fp(FILE *fp, X509_REQ *req);
OPENSSL_EXPORT RSA *d2i_RSAPrivateKey_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSAPrivateKey_fp(FILE *fp, RSA *rsa);
OPENSSL_EXPORT RSA *d2i_RSAPublicKey_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSAPublicKey_fp(FILE *fp, RSA *rsa);
OPENSSL_EXPORT RSA *d2i_RSA_PUBKEY_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSA_PUBKEY_fp(FILE *fp, RSA *rsa);
#ifndef OPENSSL_NO_DSA
OPENSSL_EXPORT DSA *d2i_DSA_PUBKEY_fp(FILE *fp, DSA **dsa);
OPENSSL_EXPORT int i2d_DSA_PUBKEY_fp(FILE *fp, DSA *dsa);
OPENSSL_EXPORT DSA *d2i_DSAPrivateKey_fp(FILE *fp, DSA **dsa);
OPENSSL_EXPORT int i2d_DSAPrivateKey_fp(FILE *fp, DSA *dsa);
#endif
OPENSSL_EXPORT EC_KEY *d2i_EC_PUBKEY_fp(FILE *fp, EC_KEY **eckey);
OPENSSL_EXPORT int i2d_EC_PUBKEY_fp(FILE *fp, EC_KEY *eckey);
OPENSSL_EXPORT EC_KEY *d2i_ECPrivateKey_fp(FILE *fp, EC_KEY **eckey);
OPENSSL_EXPORT int i2d_ECPrivateKey_fp(FILE *fp, EC_KEY *eckey);
OPENSSL_EXPORT X509_SIG *d2i_PKCS8_fp(FILE *fp, X509_SIG **p8);
OPENSSL_EXPORT int i2d_PKCS8_fp(FILE *fp, X509_SIG *p8);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_fp(
    FILE *fp, PKCS8_PRIV_KEY_INFO **p8inf);
OPENSSL_EXPORT int i2d_PKCS8_PRIV_KEY_INFO_fp(FILE *fp,
                                              PKCS8_PRIV_KEY_INFO *p8inf);
OPENSSL_EXPORT int i2d_PKCS8PrivateKeyInfo_fp(FILE *fp, EVP_PKEY *key);
OPENSSL_EXPORT int i2d_PrivateKey_fp(FILE *fp, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *d2i_PrivateKey_fp(FILE *fp, EVP_PKEY **a);
OPENSSL_EXPORT int i2d_PUBKEY_fp(FILE *fp, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *d2i_PUBKEY_fp(FILE *fp, EVP_PKEY **a);
#endif

OPENSSL_EXPORT X509 *d2i_X509_bio(BIO *bp, X509 **x509);
OPENSSL_EXPORT int i2d_X509_bio(BIO *bp, X509 *x509);
OPENSSL_EXPORT X509_CRL *d2i_X509_CRL_bio(BIO *bp, X509_CRL **crl);
OPENSSL_EXPORT int i2d_X509_CRL_bio(BIO *bp, X509_CRL *crl);
OPENSSL_EXPORT X509_REQ *d2i_X509_REQ_bio(BIO *bp, X509_REQ **req);
OPENSSL_EXPORT int i2d_X509_REQ_bio(BIO *bp, X509_REQ *req);
OPENSSL_EXPORT RSA *d2i_RSAPrivateKey_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSAPrivateKey_bio(BIO *bp, RSA *rsa);
OPENSSL_EXPORT RSA *d2i_RSAPublicKey_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSAPublicKey_bio(BIO *bp, RSA *rsa);
OPENSSL_EXPORT RSA *d2i_RSA_PUBKEY_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT int i2d_RSA_PUBKEY_bio(BIO *bp, RSA *rsa);
#ifndef OPENSSL_NO_DSA
OPENSSL_EXPORT DSA *d2i_DSA_PUBKEY_bio(BIO *bp, DSA **dsa);
OPENSSL_EXPORT int i2d_DSA_PUBKEY_bio(BIO *bp, DSA *dsa);
OPENSSL_EXPORT DSA *d2i_DSAPrivateKey_bio(BIO *bp, DSA **dsa);
OPENSSL_EXPORT int i2d_DSAPrivateKey_bio(BIO *bp, DSA *dsa);
#endif
OPENSSL_EXPORT EC_KEY *d2i_EC_PUBKEY_bio(BIO *bp, EC_KEY **eckey);
OPENSSL_EXPORT int i2d_EC_PUBKEY_bio(BIO *bp, EC_KEY *eckey);
OPENSSL_EXPORT EC_KEY *d2i_ECPrivateKey_bio(BIO *bp, EC_KEY **eckey);
OPENSSL_EXPORT int i2d_ECPrivateKey_bio(BIO *bp, EC_KEY *eckey);
OPENSSL_EXPORT X509_SIG *d2i_PKCS8_bio(BIO *bp, X509_SIG **p8);
OPENSSL_EXPORT int i2d_PKCS8_bio(BIO *bp, X509_SIG *p8);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_bio(
    BIO *bp, PKCS8_PRIV_KEY_INFO **p8inf);
OPENSSL_EXPORT int i2d_PKCS8_PRIV_KEY_INFO_bio(BIO *bp,
                                               PKCS8_PRIV_KEY_INFO *p8inf);
OPENSSL_EXPORT int i2d_PKCS8PrivateKeyInfo_bio(BIO *bp, EVP_PKEY *key);
OPENSSL_EXPORT int i2d_PrivateKey_bio(BIO *bp, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *d2i_PrivateKey_bio(BIO *bp, EVP_PKEY **a);
OPENSSL_EXPORT int i2d_PUBKEY_bio(BIO *bp, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *d2i_PUBKEY_bio(BIO *bp, EVP_PKEY **a);
OPENSSL_EXPORT DH *d2i_DHparams_bio(BIO *bp, DH **dh);
OPENSSL_EXPORT int i2d_DHparams_bio(BIO *bp, const DH *dh);

OPENSSL_EXPORT X509 *X509_dup(X509 *x509);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_dup(X509_ATTRIBUTE *xa);
OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_dup(X509_EXTENSION *ex);
OPENSSL_EXPORT X509_CRL *X509_CRL_dup(X509_CRL *crl);
OPENSSL_EXPORT X509_REVOKED *X509_REVOKED_dup(X509_REVOKED *rev);
OPENSSL_EXPORT X509_REQ *X509_REQ_dup(X509_REQ *req);
OPENSSL_EXPORT X509_ALGOR *X509_ALGOR_dup(X509_ALGOR *xn);
OPENSSL_EXPORT int X509_ALGOR_set0(X509_ALGOR *alg, const ASN1_OBJECT *aobj,
                                   int ptype, void *pval);
OPENSSL_EXPORT void X509_ALGOR_get0(const ASN1_OBJECT **paobj, int *pptype,
                                    const void **ppval,
                                    const X509_ALGOR *algor);
OPENSSL_EXPORT void X509_ALGOR_set_md(X509_ALGOR *alg, const EVP_MD *md);
OPENSSL_EXPORT int X509_ALGOR_cmp(const X509_ALGOR *a, const X509_ALGOR *b);

OPENSSL_EXPORT X509_NAME *X509_NAME_dup(X509_NAME *xn);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_dup(X509_NAME_ENTRY *ne);
OPENSSL_EXPORT int X509_NAME_ENTRY_set(const X509_NAME_ENTRY *ne);

OPENSSL_EXPORT int X509_NAME_get0_der(X509_NAME *nm, const unsigned char **pder,
                                      size_t *pderlen);

OPENSSL_EXPORT int X509_cmp_time(const ASN1_TIME *s, time_t *t);
OPENSSL_EXPORT int X509_cmp_current_time(const ASN1_TIME *s);
OPENSSL_EXPORT ASN1_TIME *X509_time_adj(ASN1_TIME *s, long adj, time_t *t);
OPENSSL_EXPORT ASN1_TIME *X509_time_adj_ex(ASN1_TIME *s, int offset_day,
                                           long offset_sec, time_t *t);
OPENSSL_EXPORT ASN1_TIME *X509_gmtime_adj(ASN1_TIME *s, long adj);

OPENSSL_EXPORT const char *X509_get_default_cert_area(void);
OPENSSL_EXPORT const char *X509_get_default_cert_dir(void);
OPENSSL_EXPORT const char *X509_get_default_cert_file(void);
OPENSSL_EXPORT const char *X509_get_default_cert_dir_env(void);
OPENSSL_EXPORT const char *X509_get_default_cert_file_env(void);
OPENSSL_EXPORT const char *X509_get_default_private_dir(void);

OPENSSL_EXPORT X509_REQ *X509_to_X509_REQ(X509 *x, EVP_PKEY *pkey,
                                          const EVP_MD *md);
OPENSSL_EXPORT X509 *X509_REQ_to_X509(X509_REQ *r, int days, EVP_PKEY *pkey);

DECLARE_ASN1_ENCODE_FUNCTIONS(X509_ALGORS, X509_ALGORS, X509_ALGORS)
DECLARE_ASN1_FUNCTIONS(X509_VAL)

DECLARE_ASN1_FUNCTIONS(X509_PUBKEY)

OPENSSL_EXPORT int X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *X509_PUBKEY_get(X509_PUBKEY *key);

DECLARE_ASN1_FUNCTIONS(X509_SIG)
DECLARE_ASN1_FUNCTIONS(X509_REQ_INFO)
DECLARE_ASN1_FUNCTIONS(X509_REQ)

DECLARE_ASN1_FUNCTIONS(X509_ATTRIBUTE)
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create(int nid, int atrtype,
                                                     void *value);

DECLARE_ASN1_FUNCTIONS(X509_EXTENSION)
DECLARE_ASN1_ENCODE_FUNCTIONS(X509_EXTENSIONS, X509_EXTENSIONS, X509_EXTENSIONS)

DECLARE_ASN1_FUNCTIONS(X509_NAME_ENTRY)

DECLARE_ASN1_FUNCTIONS(X509_NAME)

OPENSSL_EXPORT int X509_NAME_set(X509_NAME **xn, X509_NAME *name);

DECLARE_ASN1_FUNCTIONS(X509_CINF)

DECLARE_ASN1_FUNCTIONS(X509)
DECLARE_ASN1_FUNCTIONS(X509_CERT_AUX)

// X509_up_ref adds one to the reference count of |x| and returns one.
OPENSSL_EXPORT int X509_up_ref(X509 *x);

OPENSSL_EXPORT int X509_get_ex_new_index(long argl, void *argp,
                                         CRYPTO_EX_unused *unused,
                                         CRYPTO_EX_dup *dup_unused,
                                         CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int X509_set_ex_data(X509 *r, int idx, void *arg);
OPENSSL_EXPORT void *X509_get_ex_data(X509 *r, int idx);
OPENSSL_EXPORT int i2d_X509_AUX(X509 *a, unsigned char **pp);
OPENSSL_EXPORT X509 *d2i_X509_AUX(X509 **a, const unsigned char **pp,
                                  long length);

OPENSSL_EXPORT int i2d_re_X509_tbs(X509 *x, unsigned char **pp);

OPENSSL_EXPORT void X509_get0_signature(const ASN1_BIT_STRING **psig,
                                        const X509_ALGOR **palg, const X509 *x);
OPENSSL_EXPORT int X509_get_signature_nid(const X509 *x);

OPENSSL_EXPORT int X509_alias_set1(X509 *x, unsigned char *name, int len);
OPENSSL_EXPORT int X509_keyid_set1(X509 *x, unsigned char *id, int len);
OPENSSL_EXPORT unsigned char *X509_alias_get0(X509 *x, int *len);
OPENSSL_EXPORT unsigned char *X509_keyid_get0(X509 *x, int *len);
OPENSSL_EXPORT int (*X509_TRUST_set_default(int (*trust)(int, X509 *,
                                                         int)))(int, X509 *,
                                                                int);
OPENSSL_EXPORT int X509_TRUST_set(int *t, int trust);
OPENSSL_EXPORT int X509_add1_trust_object(X509 *x, ASN1_OBJECT *obj);
OPENSSL_EXPORT int X509_add1_reject_object(X509 *x, ASN1_OBJECT *obj);
OPENSSL_EXPORT void X509_trust_clear(X509 *x);
OPENSSL_EXPORT void X509_reject_clear(X509 *x);

DECLARE_ASN1_FUNCTIONS(X509_REVOKED)
DECLARE_ASN1_FUNCTIONS(X509_CRL_INFO)
DECLARE_ASN1_FUNCTIONS(X509_CRL)

OPENSSL_EXPORT int X509_CRL_add0_revoked(X509_CRL *crl, X509_REVOKED *rev);
OPENSSL_EXPORT int X509_CRL_get0_by_serial(X509_CRL *crl, X509_REVOKED **ret,
                                           ASN1_INTEGER *serial);
OPENSSL_EXPORT int X509_CRL_get0_by_cert(X509_CRL *crl, X509_REVOKED **ret,
                                         X509 *x);

OPENSSL_EXPORT X509_PKEY *X509_PKEY_new(void);
OPENSSL_EXPORT void X509_PKEY_free(X509_PKEY *a);

DECLARE_ASN1_FUNCTIONS(NETSCAPE_SPKI)
DECLARE_ASN1_FUNCTIONS(NETSCAPE_SPKAC)

OPENSSL_EXPORT X509_INFO *X509_INFO_new(void);
OPENSSL_EXPORT void X509_INFO_free(X509_INFO *a);
OPENSSL_EXPORT char *X509_NAME_oneline(const X509_NAME *a, char *buf, int size);

OPENSSL_EXPORT int ASN1_digest(i2d_of_void *i2d, const EVP_MD *type, char *data,
                               unsigned char *md, unsigned int *len);

OPENSSL_EXPORT int ASN1_item_digest(const ASN1_ITEM *it, const EVP_MD *type,
                                    void *data, unsigned char *md,
                                    unsigned int *len);

OPENSSL_EXPORT int ASN1_item_verify(const ASN1_ITEM *it, X509_ALGOR *algor1,
                                    ASN1_BIT_STRING *signature, void *data,
                                    EVP_PKEY *pkey);

OPENSSL_EXPORT int ASN1_item_sign(const ASN1_ITEM *it, X509_ALGOR *algor1,
                                  X509_ALGOR *algor2,
                                  ASN1_BIT_STRING *signature, void *data,
                                  EVP_PKEY *pkey, const EVP_MD *type);
OPENSSL_EXPORT int ASN1_item_sign_ctx(const ASN1_ITEM *it, X509_ALGOR *algor1,
                                      X509_ALGOR *algor2,
                                      ASN1_BIT_STRING *signature, void *asn,
                                      EVP_MD_CTX *ctx);

OPENSSL_EXPORT int X509_set_version(X509 *x, long version);
OPENSSL_EXPORT int X509_set_serialNumber(X509 *x, ASN1_INTEGER *serial);
OPENSSL_EXPORT ASN1_INTEGER *X509_get_serialNumber(X509 *x);
OPENSSL_EXPORT int X509_set_issuer_name(X509 *x, X509_NAME *name);
OPENSSL_EXPORT X509_NAME *X509_get_issuer_name(const X509 *a);
OPENSSL_EXPORT int X509_set_subject_name(X509 *x, X509_NAME *name);
OPENSSL_EXPORT X509_NAME *X509_get_subject_name(const X509 *a);
OPENSSL_EXPORT int X509_set_pubkey(X509 *x, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *X509_get_pubkey(X509 *x);
OPENSSL_EXPORT ASN1_BIT_STRING *X509_get0_pubkey_bitstr(const X509 *x);
// TODO(davidben): |X509_get0_extensions| should return a const pointer to
// match upstream.
OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *X509_get0_extensions(const X509 *x);
OPENSSL_EXPORT const X509_ALGOR *X509_get0_tbs_sigalg(const X509 *x);

OPENSSL_EXPORT int X509_REQ_set_version(X509_REQ *x, long version);
OPENSSL_EXPORT int X509_REQ_set_subject_name(X509_REQ *req, X509_NAME *name);
OPENSSL_EXPORT void X509_REQ_get0_signature(const X509_REQ *req,
                                            const ASN1_BIT_STRING **psig,
                                            const X509_ALGOR **palg);
OPENSSL_EXPORT int X509_REQ_get_signature_nid(const X509_REQ *req);
OPENSSL_EXPORT int i2d_re_X509_REQ_tbs(X509_REQ *req, unsigned char **pp);
OPENSSL_EXPORT int X509_REQ_set_pubkey(X509_REQ *x, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *X509_REQ_get_pubkey(X509_REQ *req);
OPENSSL_EXPORT int X509_REQ_extension_nid(int nid);
OPENSSL_EXPORT const int *X509_REQ_get_extension_nids(void);
OPENSSL_EXPORT void X509_REQ_set_extension_nids(const int *nids);
OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *
    X509_REQ_get_extensions(X509_REQ *req);
OPENSSL_EXPORT int X509_REQ_add_extensions_nid(X509_REQ *req,
                                               STACK_OF(X509_EXTENSION) * exts,
                                               int nid);
OPENSSL_EXPORT int X509_REQ_add_extensions(X509_REQ *req,
                                           STACK_OF(X509_EXTENSION) * exts);
OPENSSL_EXPORT int X509_REQ_get_attr_count(const X509_REQ *req);
OPENSSL_EXPORT int X509_REQ_get_attr_by_NID(const X509_REQ *req, int nid,
                                            int lastpos);
OPENSSL_EXPORT int X509_REQ_get_attr_by_OBJ(const X509_REQ *req,
                                            ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_get_attr(const X509_REQ *req, int loc);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_delete_attr(X509_REQ *req, int loc);
OPENSSL_EXPORT int X509_REQ_add1_attr(X509_REQ *req, X509_ATTRIBUTE *attr);
OPENSSL_EXPORT int X509_REQ_add1_attr_by_OBJ(X509_REQ *req,
                                             const ASN1_OBJECT *obj, int type,
                                             const unsigned char *bytes,
                                             int len);
OPENSSL_EXPORT int X509_REQ_add1_attr_by_NID(X509_REQ *req, int nid, int type,
                                             const unsigned char *bytes,
                                             int len);
OPENSSL_EXPORT int X509_REQ_add1_attr_by_txt(X509_REQ *req,
                                             const char *attrname, int type,
                                             const unsigned char *bytes,
                                             int len);

OPENSSL_EXPORT int X509_CRL_set_version(X509_CRL *x, long version);
OPENSSL_EXPORT int X509_CRL_set_issuer_name(X509_CRL *x, X509_NAME *name);
OPENSSL_EXPORT int X509_CRL_sort(X509_CRL *crl);
OPENSSL_EXPORT int X509_CRL_up_ref(X509_CRL *crl);

OPENSSL_EXPORT void X509_CRL_get0_signature(const X509_CRL *crl,
                                            const ASN1_BIT_STRING **psig,
                                            const X509_ALGOR **palg);
OPENSSL_EXPORT int X509_CRL_get_signature_nid(const X509_CRL *crl);
OPENSSL_EXPORT int i2d_re_X509_CRL_tbs(X509_CRL *req, unsigned char **pp);

OPENSSL_EXPORT const ASN1_INTEGER *X509_REVOKED_get0_serialNumber(
    const X509_REVOKED *x);
OPENSSL_EXPORT int X509_REVOKED_set_serialNumber(X509_REVOKED *x,
                                                 ASN1_INTEGER *serial);
OPENSSL_EXPORT const ASN1_TIME *X509_REVOKED_get0_revocationDate(
    const X509_REVOKED *x);
OPENSSL_EXPORT int X509_REVOKED_set_revocationDate(X509_REVOKED *r,
                                                   ASN1_TIME *tm);

// X509_REVOKED_get0_extensions returns |r|'s extensions.
OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *
    X509_REVOKED_get0_extensions(const X509_REVOKED *r);

OPENSSL_EXPORT X509_CRL *X509_CRL_diff(X509_CRL *base, X509_CRL *newer,
                                       EVP_PKEY *skey, const EVP_MD *md,
                                       unsigned int flags);

OPENSSL_EXPORT int X509_REQ_check_private_key(X509_REQ *x509, EVP_PKEY *pkey);

OPENSSL_EXPORT int X509_check_private_key(X509 *x509, const EVP_PKEY *pkey);
OPENSSL_EXPORT int X509_chain_check_suiteb(int *perror_depth, X509 *x,
                                           STACK_OF(X509) * chain,
                                           unsigned long flags);
OPENSSL_EXPORT int X509_CRL_check_suiteb(X509_CRL *crl, EVP_PKEY *pk,
                                         unsigned long flags);
OPENSSL_EXPORT STACK_OF(X509) * X509_chain_up_ref(STACK_OF(X509) * chain);

OPENSSL_EXPORT int X509_issuer_and_serial_cmp(const X509 *a, const X509 *b);
OPENSSL_EXPORT unsigned long X509_issuer_and_serial_hash(X509 *a);

OPENSSL_EXPORT int X509_issuer_name_cmp(const X509 *a, const X509 *b);
OPENSSL_EXPORT unsigned long X509_issuer_name_hash(X509 *a);

OPENSSL_EXPORT int X509_subject_name_cmp(const X509 *a, const X509 *b);
OPENSSL_EXPORT unsigned long X509_subject_name_hash(X509 *x);

OPENSSL_EXPORT unsigned long X509_issuer_name_hash_old(X509 *a);
OPENSSL_EXPORT unsigned long X509_subject_name_hash_old(X509 *x);

OPENSSL_EXPORT int X509_cmp(const X509 *a, const X509 *b);
OPENSSL_EXPORT int X509_NAME_cmp(const X509_NAME *a, const X509_NAME *b);
OPENSSL_EXPORT unsigned long X509_NAME_hash(X509_NAME *x);
OPENSSL_EXPORT unsigned long X509_NAME_hash_old(X509_NAME *x);

OPENSSL_EXPORT int X509_CRL_cmp(const X509_CRL *a, const X509_CRL *b);
OPENSSL_EXPORT int X509_CRL_match(const X509_CRL *a, const X509_CRL *b);
#ifndef OPENSSL_NO_FP_API
OPENSSL_EXPORT int X509_print_ex_fp(FILE *bp, X509 *x, unsigned long nmflag,
                                    unsigned long cflag);
OPENSSL_EXPORT int X509_print_fp(FILE *bp, X509 *x);
OPENSSL_EXPORT int X509_CRL_print_fp(FILE *bp, X509_CRL *x);
OPENSSL_EXPORT int X509_REQ_print_fp(FILE *bp, X509_REQ *req);
OPENSSL_EXPORT int X509_NAME_print_ex_fp(FILE *fp, const X509_NAME *nm,
                                         int indent, unsigned long flags);
#endif

OPENSSL_EXPORT int X509_NAME_print(BIO *bp, const X509_NAME *name, int obase);
OPENSSL_EXPORT int X509_NAME_print_ex(BIO *out, const X509_NAME *nm, int indent,
                                      unsigned long flags);
OPENSSL_EXPORT int X509_print_ex(BIO *bp, X509 *x, unsigned long nmflag,
                                 unsigned long cflag);
OPENSSL_EXPORT int X509_print(BIO *bp, X509 *x);
OPENSSL_EXPORT int X509_ocspid_print(BIO *bp, X509 *x);
OPENSSL_EXPORT int X509_CERT_AUX_print(BIO *bp, X509_CERT_AUX *x, int indent);
OPENSSL_EXPORT int X509_CRL_print(BIO *bp, X509_CRL *x);
OPENSSL_EXPORT int X509_REQ_print_ex(BIO *bp, X509_REQ *x, unsigned long nmflag,
                                     unsigned long cflag);
OPENSSL_EXPORT int X509_REQ_print(BIO *bp, X509_REQ *req);

OPENSSL_EXPORT int X509_NAME_entry_count(const X509_NAME *name);
OPENSSL_EXPORT int X509_NAME_get_text_by_NID(const X509_NAME *name, int nid,
                                             char *buf, int len);
OPENSSL_EXPORT int X509_NAME_get_text_by_OBJ(const X509_NAME *name,
                                             const ASN1_OBJECT *obj, char *buf,
                                             int len);

// NOTE: you should be passsing -1, not 0 as lastpos.  The functions that use
// lastpos, search after that position on.
OPENSSL_EXPORT int X509_NAME_get_index_by_NID(const X509_NAME *name, int nid,
                                              int lastpos);
OPENSSL_EXPORT int X509_NAME_get_index_by_OBJ(const X509_NAME *name,
                                              const ASN1_OBJECT *obj,
                                              int lastpos);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_get_entry(const X509_NAME *name,
                                                    int loc);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_delete_entry(X509_NAME *name,
                                                       int loc);
OPENSSL_EXPORT int X509_NAME_add_entry(X509_NAME *name, X509_NAME_ENTRY *ne,
                                       int loc, int set);
OPENSSL_EXPORT int X509_NAME_add_entry_by_OBJ(X509_NAME *name, ASN1_OBJECT *obj,
                                              int type,
                                              const unsigned char *bytes,
                                              int len, int loc, int set);
OPENSSL_EXPORT int X509_NAME_add_entry_by_NID(X509_NAME *name, int nid,
                                              int type,
                                              const unsigned char *bytes,
                                              int len, int loc, int set);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_txt(
    X509_NAME_ENTRY **ne, const char *field, int type,
    const unsigned char *bytes, int len);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_NID(
    X509_NAME_ENTRY **ne, int nid, int type, const unsigned char *bytes,
    int len);
OPENSSL_EXPORT int X509_NAME_add_entry_by_txt(X509_NAME *name,
                                              const char *field, int type,
                                              const unsigned char *bytes,
                                              int len, int loc, int set);
OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_OBJ(
    X509_NAME_ENTRY **ne, const ASN1_OBJECT *obj, int type,
    const unsigned char *bytes, int len);
OPENSSL_EXPORT int X509_NAME_ENTRY_set_object(X509_NAME_ENTRY *ne,
                                              const ASN1_OBJECT *obj);
OPENSSL_EXPORT int X509_NAME_ENTRY_set_data(X509_NAME_ENTRY *ne, int type,
                                            const unsigned char *bytes,
                                            int len);
OPENSSL_EXPORT ASN1_OBJECT *X509_NAME_ENTRY_get_object(
    const X509_NAME_ENTRY *ne);
OPENSSL_EXPORT ASN1_STRING *X509_NAME_ENTRY_get_data(const X509_NAME_ENTRY *ne);

OPENSSL_EXPORT int X509v3_get_ext_count(const STACK_OF(X509_EXTENSION) * x);
OPENSSL_EXPORT int X509v3_get_ext_by_NID(const STACK_OF(X509_EXTENSION) * x,
                                         int nid, int lastpos);
OPENSSL_EXPORT int X509v3_get_ext_by_OBJ(const STACK_OF(X509_EXTENSION) * x,
                                         const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT int X509v3_get_ext_by_critical(const STACK_OF(X509_EXTENSION) *
                                                  x,
                                              int crit, int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509v3_get_ext(const STACK_OF(X509_EXTENSION) *
                                                  x,
                                              int loc);
OPENSSL_EXPORT X509_EXTENSION *X509v3_delete_ext(STACK_OF(X509_EXTENSION) * x,
                                                 int loc);
OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *
    X509v3_add_ext(STACK_OF(X509_EXTENSION) * *x, X509_EXTENSION *ex, int loc);

OPENSSL_EXPORT int X509_get_ext_count(const X509 *x);
OPENSSL_EXPORT int X509_get_ext_by_NID(const X509 *x, int nid, int lastpos);
OPENSSL_EXPORT int X509_get_ext_by_OBJ(const X509 *x, const ASN1_OBJECT *obj,
                                       int lastpos);
OPENSSL_EXPORT int X509_get_ext_by_critical(const X509 *x, int crit,
                                            int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509_get_ext(const X509 *x, int loc);
OPENSSL_EXPORT X509_EXTENSION *X509_delete_ext(X509 *x, int loc);
OPENSSL_EXPORT int X509_add_ext(X509 *x, X509_EXTENSION *ex, int loc);
OPENSSL_EXPORT void *X509_get_ext_d2i(const X509 *x, int nid, int *crit, int *idx);
OPENSSL_EXPORT int X509_add1_ext_i2d(X509 *x, int nid, void *value, int crit,
                                     unsigned long flags);

OPENSSL_EXPORT int X509_CRL_get_ext_count(const X509_CRL *x);
OPENSSL_EXPORT int X509_CRL_get_ext_by_NID(const X509_CRL *x, int nid, int lastpos);
OPENSSL_EXPORT int X509_CRL_get_ext_by_OBJ(const X509_CRL *x,
                                           const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT int X509_CRL_get_ext_by_critical(const X509_CRL *x, int crit,
                                                int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509_CRL_get_ext(const X509_CRL *x, int loc);
OPENSSL_EXPORT X509_EXTENSION *X509_CRL_delete_ext(X509_CRL *x, int loc);
OPENSSL_EXPORT int X509_CRL_add_ext(X509_CRL *x, X509_EXTENSION *ex, int loc);
OPENSSL_EXPORT void *X509_CRL_get_ext_d2i(const X509_CRL *x, int nid, int *crit,
                                          int *idx);
OPENSSL_EXPORT int X509_CRL_add1_ext_i2d(X509_CRL *x, int nid, void *value,
                                         int crit, unsigned long flags);

OPENSSL_EXPORT int X509_REVOKED_get_ext_count(const X509_REVOKED *x);
OPENSSL_EXPORT int X509_REVOKED_get_ext_by_NID(const X509_REVOKED *x, int nid,
                                               int lastpos);
OPENSSL_EXPORT int X509_REVOKED_get_ext_by_OBJ(const X509_REVOKED *x,
                                               const ASN1_OBJECT *obj,
                                               int lastpos);
OPENSSL_EXPORT int X509_REVOKED_get_ext_by_critical(const X509_REVOKED *x,
                                                    int crit, int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509_REVOKED_get_ext(const X509_REVOKED *x,
                                                    int loc);
OPENSSL_EXPORT X509_EXTENSION *X509_REVOKED_delete_ext(X509_REVOKED *x,
                                                       int loc);
OPENSSL_EXPORT int X509_REVOKED_add_ext(X509_REVOKED *x, X509_EXTENSION *ex,
                                        int loc);
OPENSSL_EXPORT void *X509_REVOKED_get_ext_d2i(const X509_REVOKED *x, int nid,
                                              int *crit, int *idx);
OPENSSL_EXPORT int X509_REVOKED_add1_ext_i2d(X509_REVOKED *x, int nid,
                                             void *value, int crit,
                                             unsigned long flags);

OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_create_by_NID(
    X509_EXTENSION **ex, int nid, int crit, const ASN1_OCTET_STRING *data);
OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_create_by_OBJ(
    X509_EXTENSION **ex, const ASN1_OBJECT *obj, int crit,
    const ASN1_OCTET_STRING *data);
OPENSSL_EXPORT int X509_EXTENSION_set_object(X509_EXTENSION *ex,
                                             const ASN1_OBJECT *obj);
OPENSSL_EXPORT int X509_EXTENSION_set_critical(X509_EXTENSION *ex, int crit);
OPENSSL_EXPORT int X509_EXTENSION_set_data(X509_EXTENSION *ex,
                                           const ASN1_OCTET_STRING *data);
OPENSSL_EXPORT ASN1_OBJECT *X509_EXTENSION_get_object(X509_EXTENSION *ex);
OPENSSL_EXPORT ASN1_OCTET_STRING *X509_EXTENSION_get_data(X509_EXTENSION *ne);
OPENSSL_EXPORT int X509_EXTENSION_get_critical(X509_EXTENSION *ex);

OPENSSL_EXPORT int X509at_get_attr_count(const STACK_OF(X509_ATTRIBUTE) * x);
OPENSSL_EXPORT int X509at_get_attr_by_NID(const STACK_OF(X509_ATTRIBUTE) * x,
                                          int nid, int lastpos);
OPENSSL_EXPORT int X509at_get_attr_by_OBJ(const STACK_OF(X509_ATTRIBUTE) * sk,
                                          const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT X509_ATTRIBUTE *X509at_get_attr(const STACK_OF(X509_ATTRIBUTE) *
                                                   x,
                                               int loc);
OPENSSL_EXPORT X509_ATTRIBUTE *X509at_delete_attr(STACK_OF(X509_ATTRIBUTE) * x,
                                                  int loc);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *
    X509at_add1_attr(STACK_OF(X509_ATTRIBUTE) * *x, X509_ATTRIBUTE *attr);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *
    X509at_add1_attr_by_OBJ(STACK_OF(X509_ATTRIBUTE) * *x,
                            const ASN1_OBJECT *obj, int type,
                            const unsigned char *bytes, int len);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *
    X509at_add1_attr_by_NID(STACK_OF(X509_ATTRIBUTE) * *x, int nid, int type,
                            const unsigned char *bytes, int len);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *
    X509at_add1_attr_by_txt(STACK_OF(X509_ATTRIBUTE) * *x, const char *attrname,
                            int type, const unsigned char *bytes, int len);
OPENSSL_EXPORT void *X509at_get0_data_by_OBJ(STACK_OF(X509_ATTRIBUTE) * x,
                                             ASN1_OBJECT *obj, int lastpos,
                                             int type);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_NID(
    X509_ATTRIBUTE **attr, int nid, int atrtype, const void *data, int len);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_OBJ(
    X509_ATTRIBUTE **attr, const ASN1_OBJECT *obj, int atrtype,
    const void *data, int len);
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_txt(
    X509_ATTRIBUTE **attr, const char *atrname, int type,
    const unsigned char *bytes, int len);
OPENSSL_EXPORT int X509_ATTRIBUTE_set1_object(X509_ATTRIBUTE *attr,
                                              const ASN1_OBJECT *obj);
OPENSSL_EXPORT int X509_ATTRIBUTE_set1_data(X509_ATTRIBUTE *attr, int attrtype,
                                            const void *data, int len);
OPENSSL_EXPORT void *X509_ATTRIBUTE_get0_data(X509_ATTRIBUTE *attr, int idx,
                                              int atrtype, void *data);
OPENSSL_EXPORT int X509_ATTRIBUTE_count(X509_ATTRIBUTE *attr);
OPENSSL_EXPORT ASN1_OBJECT *X509_ATTRIBUTE_get0_object(X509_ATTRIBUTE *attr);
OPENSSL_EXPORT ASN1_TYPE *X509_ATTRIBUTE_get0_type(X509_ATTRIBUTE *attr,
                                                   int idx);

OPENSSL_EXPORT int X509_verify_cert(X509_STORE_CTX *ctx);

// lookup a cert from a X509 STACK
OPENSSL_EXPORT X509 *X509_find_by_issuer_and_serial(STACK_OF(X509) * sk,
                                                    X509_NAME *name,
                                                    ASN1_INTEGER *serial);
OPENSSL_EXPORT X509 *X509_find_by_subject(STACK_OF(X509) * sk, X509_NAME *name);

// PKCS#8 utilities

DECLARE_ASN1_FUNCTIONS(PKCS8_PRIV_KEY_INFO)

OPENSSL_EXPORT EVP_PKEY *EVP_PKCS82PKEY(PKCS8_PRIV_KEY_INFO *p8);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8(EVP_PKEY *pkey);

OPENSSL_EXPORT int PKCS8_pkey_set0(PKCS8_PRIV_KEY_INFO *priv, ASN1_OBJECT *aobj,
                                   int version, int ptype, void *pval,
                                   unsigned char *penc, int penclen);
OPENSSL_EXPORT int PKCS8_pkey_get0(ASN1_OBJECT **ppkalg,
                                   const unsigned char **pk, int *ppklen,
                                   X509_ALGOR **pa, PKCS8_PRIV_KEY_INFO *p8);

OPENSSL_EXPORT int X509_PUBKEY_set0_param(X509_PUBKEY *pub,
                                          const ASN1_OBJECT *aobj, int ptype,
                                          void *pval, unsigned char *penc,
                                          int penclen);
OPENSSL_EXPORT int X509_PUBKEY_get0_param(ASN1_OBJECT **ppkalg,
                                          const unsigned char **pk, int *ppklen,
                                          X509_ALGOR **pa, X509_PUBKEY *pub);

OPENSSL_EXPORT int X509_check_trust(X509 *x, int id, int flags);
OPENSSL_EXPORT int X509_TRUST_get_count(void);
OPENSSL_EXPORT X509_TRUST *X509_TRUST_get0(int idx);
OPENSSL_EXPORT int X509_TRUST_get_by_id(int id);
OPENSSL_EXPORT int X509_TRUST_add(int id, int flags,
                                  int (*ck)(X509_TRUST *, X509 *, int),
                                  char *name, int arg1, void *arg2);
OPENSSL_EXPORT void X509_TRUST_cleanup(void);
OPENSSL_EXPORT int X509_TRUST_get_flags(const X509_TRUST *xp);
OPENSSL_EXPORT char *X509_TRUST_get0_name(const X509_TRUST *xp);
OPENSSL_EXPORT int X509_TRUST_get_trust(const X509_TRUST *xp);


typedef struct rsa_pss_params_st {
  X509_ALGOR *hashAlgorithm;
  X509_ALGOR *maskGenAlgorithm;
  ASN1_INTEGER *saltLength;
  ASN1_INTEGER *trailerField;
} RSA_PSS_PARAMS;

DECLARE_ASN1_FUNCTIONS(RSA_PSS_PARAMS)



#ifdef __cplusplus
}
#endif

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(NETSCAPE_SPKI, NETSCAPE_SPKI_free)
BORINGSSL_MAKE_DELETER(RSA_PSS_PARAMS, RSA_PSS_PARAMS_free)
BORINGSSL_MAKE_DELETER(X509, X509_free)
BORINGSSL_MAKE_UP_REF(X509, X509_up_ref)
BORINGSSL_MAKE_DELETER(X509_ALGOR, X509_ALGOR_free)
BORINGSSL_MAKE_DELETER(X509_CRL, X509_CRL_free)
BORINGSSL_MAKE_UP_REF(X509_CRL, X509_CRL_up_ref)
BORINGSSL_MAKE_DELETER(X509_CRL_METHOD, X509_CRL_METHOD_free)
BORINGSSL_MAKE_DELETER(X509_EXTENSION, X509_EXTENSION_free)
BORINGSSL_MAKE_DELETER(X509_INFO, X509_INFO_free)
BORINGSSL_MAKE_DELETER(X509_LOOKUP, X509_LOOKUP_free)
BORINGSSL_MAKE_DELETER(X509_NAME, X509_NAME_free)
BORINGSSL_MAKE_DELETER(X509_NAME_ENTRY, X509_NAME_ENTRY_free)
BORINGSSL_MAKE_DELETER(X509_PKEY, X509_PKEY_free)
BORINGSSL_MAKE_DELETER(X509_POLICY_TREE, X509_policy_tree_free)
BORINGSSL_MAKE_DELETER(X509_PUBKEY, X509_PUBKEY_free)
BORINGSSL_MAKE_DELETER(X509_REQ, X509_REQ_free)
BORINGSSL_MAKE_DELETER(X509_REVOKED, X509_REVOKED_free)
BORINGSSL_MAKE_DELETER(X509_SIG, X509_SIG_free)
BORINGSSL_MAKE_DELETER(X509_STORE, X509_STORE_free)
BORINGSSL_MAKE_DELETER(X509_STORE_CTX, X509_STORE_CTX_free)
BORINGSSL_MAKE_DELETER(X509_VERIFY_PARAM, X509_VERIFY_PARAM_free)

using ScopedX509_STORE_CTX =
    internal::StackAllocated<X509_STORE_CTX, void, X509_STORE_CTX_zero,
                             X509_STORE_CTX_cleanup>;

BSSL_NAMESPACE_END

}  // extern C++
#endif  // !BORINGSSL_NO_CXX

#define X509_R_AKID_MISMATCH 100
#define X509_R_BAD_PKCS7_VERSION 101
#define X509_R_BAD_X509_FILETYPE 102
#define X509_R_BASE64_DECODE_ERROR 103
#define X509_R_CANT_CHECK_DH_KEY 104
#define X509_R_CERT_ALREADY_IN_HASH_TABLE 105
#define X509_R_CRL_ALREADY_DELTA 106
#define X509_R_CRL_VERIFY_FAILURE 107
#define X509_R_IDP_MISMATCH 108
#define X509_R_INVALID_BIT_STRING_BITS_LEFT 109
#define X509_R_INVALID_DIRECTORY 110
#define X509_R_INVALID_FIELD_NAME 111
#define X509_R_INVALID_PSS_PARAMETERS 112
#define X509_R_INVALID_TRUST 113
#define X509_R_ISSUER_MISMATCH 114
#define X509_R_KEY_TYPE_MISMATCH 115
#define X509_R_KEY_VALUES_MISMATCH 116
#define X509_R_LOADING_CERT_DIR 117
#define X509_R_LOADING_DEFAULTS 118
#define X509_R_NEWER_CRL_NOT_NEWER 119
#define X509_R_NOT_PKCS7_SIGNED_DATA 120
#define X509_R_NO_CERTIFICATES_INCLUDED 121
#define X509_R_NO_CERT_SET_FOR_US_TO_VERIFY 122
#define X509_R_NO_CRLS_INCLUDED 123
#define X509_R_NO_CRL_NUMBER 124
#define X509_R_PUBLIC_KEY_DECODE_ERROR 125
#define X509_R_PUBLIC_KEY_ENCODE_ERROR 126
#define X509_R_SHOULD_RETRY 127
#define X509_R_UNKNOWN_KEY_TYPE 128
#define X509_R_UNKNOWN_NID 129
#define X509_R_UNKNOWN_PURPOSE_ID 130
#define X509_R_UNKNOWN_TRUST_ID 131
#define X509_R_UNSUPPORTED_ALGORITHM 132
#define X509_R_WRONG_LOOKUP_TYPE 133
#define X509_R_WRONG_TYPE 134
#define X509_R_NAME_TOO_LONG 135
#define X509_R_INVALID_PARAMETER 136
#define X509_R_SIGNATURE_ALGORITHM_MISMATCH 137
#define X509_R_DELTA_CRL_WITHOUT_CRL_NUMBER 138
#define X509_R_INVALID_FIELD_FOR_VERSION 139
#define X509_R_INVALID_VERSION 140

#endif
