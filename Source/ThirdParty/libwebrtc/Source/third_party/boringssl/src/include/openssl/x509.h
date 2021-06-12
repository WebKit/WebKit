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
  STACK_OF(X509_NAME_ENTRY) *entries;
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

DEFINE_STACK_OF(X509_ATTRIBUTE)
DECLARE_ASN1_SET_OF(X509_ATTRIBUTE)


struct X509_req_info_st {
  ASN1_ENCODING enc;
  ASN1_INTEGER *version;
  X509_NAME *subject;
  X509_PUBKEY *pubkey;
  //  d=2 hl=2 l=  0 cons: cont: 00
  STACK_OF(X509_ATTRIBUTE) *attributes;  // [ 0 ]
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
  ASN1_BIT_STRING *issuerUID;            // [ 1 ] optional in v2
  ASN1_BIT_STRING *subjectUID;           // [ 2 ] optional in v2
  STACK_OF(X509_EXTENSION) *extensions;  // [ 3 ] optional in v3
  ASN1_ENCODING enc;
} /* X509_CINF */;

// This stuff is certificate "auxiliary info"
// it contains details which are useful in certificate
// stores and databases. When used this is tagged onto
// the end of the certificate itself

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
  STACK_OF(DIST_POINT) *crldp;
  STACK_OF(GENERAL_NAME) *altname;
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
  STACK_OF(X509_EXTENSION) /* optional */ *extensions;
  // Set up if indirect CRL
  STACK_OF(GENERAL_NAME) *issuer;
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
  STACK_OF(X509_REVOKED) *revoked;
  STACK_OF(X509_EXTENSION) /* [0] */ *extensions;
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
  STACK_OF(GENERAL_NAMES) *issuers;
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

// X509_VERSION_* are X.509 version numbers. Note the numerical values of all
// defined X.509 versions are one less than the named version.
#define X509_VERSION_1 0
#define X509_VERSION_2 1
#define X509_VERSION_3 2

// X509_get_version returns the numerical value of |x509|'s version. Callers may
// compare the result to the |X509_VERSION_*| constants. Unknown versions are
// rejected by the parser, but a manually-created |X509| object may encode
// invalid versions. In that case, the function will return the invalid version,
// or -1 on overflow.
OPENSSL_EXPORT long X509_get_version(const X509 *x509);

// X509_set_version sets |x509|'s version to |version|, which should be one of
// the |X509V_VERSION_*| constants. It returns one on success and zero on error.
//
// If unsure, use |X509_VERSION_3|.
OPENSSL_EXPORT int X509_set_version(X509 *x509, long version);

// X509_get0_serialNumber returns |x509|'s serial number.
OPENSSL_EXPORT const ASN1_INTEGER *X509_get0_serialNumber(const X509 *x509);

// X509_set_serialNumber sets |x509|'s serial number to |serial|. It returns one
// on success and zero on error.
OPENSSL_EXPORT int X509_set_serialNumber(X509 *x509,
                                         const ASN1_INTEGER *serial);

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

// X509_get0_uids sets |*out_issuer_uid| to a non-owning pointer to the
// issuerUID field of |x509|, or NULL if |x509| has no issuerUID. It similarly
// outputs |x509|'s subjectUID field to |*out_subject_uid|.
//
// Callers may pass NULL to either |out_issuer_uid| or |out_subject_uid| to
// ignore the corresponding field.
OPENSSL_EXPORT void X509_get0_uids(const X509 *x509,
                                   const ASN1_BIT_STRING **out_issuer_uid,
                                   const ASN1_BIT_STRING **out_subject_uid);

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

// X509_REQ_VERSION_1 is the version constant for |X509_REQ| objects. Note no
// other versions are defined.
#define X509_REQ_VERSION_1 0

// X509_REQ_get_version returns the numerical value of |req|'s version. This
// will be |X509_REQ_VERSION_1| for valid certificate requests. If |req| is
// invalid, it may return another value, or -1 on overflow.
//
// TODO(davidben): Enforce the version number in the parser.
OPENSSL_EXPORT long X509_REQ_get_version(const X509_REQ *req);

// X509_REQ_get_subject_name returns |req|'s subject name. Note this function is
// not const-correct for legacy reasons.
OPENSSL_EXPORT X509_NAME *X509_REQ_get_subject_name(const X509_REQ *req);

// X509_REQ_extract_key is a legacy alias for |X509_REQ_get_pubkey|.
#define X509_REQ_extract_key(a) X509_REQ_get_pubkey(a)

// X509_name_cmp is a legacy alias for |X509_NAME_cmp|.
#define X509_name_cmp(a, b) X509_NAME_cmp((a), (b))

#define X509_CRL_VERSION_1 0
#define X509_CRL_VERSION_2 1

// X509_CRL_get_version returns the numerical value of |crl|'s version. Callers
// may compare the result to |X509_CRL_VERSION_*| constants. If |crl| is
// invalid, it may return another value, or -1 on overflow.
//
// TODO(davidben): Enforce the version number in the parser.
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

// X509_CRL_get_REVOKED returns the list of revoked certificates in |crl|, or
// NULL if |crl| omits it.
//
// TOOD(davidben): This function was originally a macro, without clear const
// semantics. It should take a const input and give const output, but the latter
// would break existing callers. For now, we match upstream.
OPENSSL_EXPORT STACK_OF(X509_REVOKED) *X509_CRL_get_REVOKED(X509_CRL *crl);

// X509_CRL_get0_extensions returns |crl|'s extension list, or NULL if |crl|
// omits it.
OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_CRL_get0_extensions(
    const X509_CRL *crl);

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
// |EVP_PKEY|, or NULL on error. The caller takes ownership of the resulting
// pointer and must call |EVP_PKEY_free| when done.
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

// X509_sign signs |x509| with |pkey| and replaces the signature algorithm and
// signature fields. It returns one on success and zero on error. This function
// uses digest algorithm |md|, or |pkey|'s default if NULL. Other signing
// parameters use |pkey|'s defaults. To customize them, use |X509_sign_ctx|.
OPENSSL_EXPORT int X509_sign(X509 *x509, EVP_PKEY *pkey, const EVP_MD *md);

// X509_sign_ctx signs |x509| with |ctx| and replaces the signature algorithm
// and signature fields. It returns one on success and zero on error. The
// signature algorithm and parameters come from |ctx|, which must have been
// initialized with |EVP_DigestSignInit|. The caller should configure the
// corresponding |EVP_PKEY_CTX| before calling this function.
OPENSSL_EXPORT int X509_sign_ctx(X509 *x509, EVP_MD_CTX *ctx);

// X509_REQ_sign signs |req| with |pkey| and replaces the signature algorithm
// and signature fields. It returns one on success and zero on error. This
// function uses digest algorithm |md|, or |pkey|'s default if NULL. Other
// signing parameters use |pkey|'s defaults. To customize them, use
// |X509_REQ_sign_ctx|.
OPENSSL_EXPORT int X509_REQ_sign(X509_REQ *req, EVP_PKEY *pkey,
                                 const EVP_MD *md);

// X509_REQ_sign_ctx signs |req| with |ctx| and replaces the signature algorithm
// and signature fields. It returns one on success and zero on error. The
// signature algorithm and parameters come from |ctx|, which must have been
// initialized with |EVP_DigestSignInit|. The caller should configure the
// corresponding |EVP_PKEY_CTX| before calling this function.
OPENSSL_EXPORT int X509_REQ_sign_ctx(X509_REQ *req, EVP_MD_CTX *ctx);

// X509_CRL_sign signs |crl| with |pkey| and replaces the signature algorithm
// and signature fields. It returns one on success and zero on error. This
// function uses digest algorithm |md|, or |pkey|'s default if NULL. Other
// signing parameters use |pkey|'s defaults. To customize them, use
// |X509_CRL_sign_ctx|.
OPENSSL_EXPORT int X509_CRL_sign(X509_CRL *crl, EVP_PKEY *pkey,
                                 const EVP_MD *md);

// X509_CRL_sign_ctx signs |crl| with |ctx| and replaces the signature algorithm
// and signature fields. It returns one on success and zero on error. The
// signature algorithm and parameters come from |ctx|, which must have been
// initialized with |EVP_DigestSignInit|. The caller should configure the
// corresponding |EVP_PKEY_CTX| before calling this function.
OPENSSL_EXPORT int X509_CRL_sign_ctx(X509_CRL *crl, EVP_MD_CTX *ctx);

// NETSCAPE_SPKI_sign signs |spki| with |pkey| and replaces the signature
// algorithm and signature fields. It returns one on success and zero on error.
// This function uses digest algorithm |md|, or |pkey|'s default if NULL. Other
// signing parameters use |pkey|'s defaults.
OPENSSL_EXPORT int NETSCAPE_SPKI_sign(NETSCAPE_SPKI *spki, EVP_PKEY *pkey,
                                      const EVP_MD *md);

// X509_pubkey_digest hashes the DER encoding of |x509|'s subjectPublicKeyInfo
// field with |md| and writes the result to |out|. |EVP_MD_CTX_size| bytes are
// written, which is at most |EVP_MAX_MD_SIZE|. If |out_len| is not NULL,
// |*out_len| is set to the number of bytes written. This function returns one
// on success and zero on error.
OPENSSL_EXPORT int X509_pubkey_digest(const X509 *x509, const EVP_MD *md,
                                      uint8_t *out, unsigned *out_len);

// X509_digest hashes |x509|'s DER encoding with |md| and writes the result to
// |out|. |EVP_MD_CTX_size| bytes are written, which is at most
// |EVP_MAX_MD_SIZE|. If |out_len| is not NULL, |*out_len| is set to the number
// of bytes written. This function returns one on success and zero on error.
// Note this digest covers the entire certificate, not just the signed portion.
OPENSSL_EXPORT int X509_digest(const X509 *x509, const EVP_MD *md, uint8_t *out,
                               unsigned *out_len);

// X509_CRL_digest hashes |crl|'s DER encoding with |md| and writes the result
// to |out|. |EVP_MD_CTX_size| bytes are written, which is at most
// |EVP_MAX_MD_SIZE|. If |out_len| is not NULL, |*out_len| is set to the number
// of bytes written. This function returns one on success and zero on error.
// Note this digest covers the entire CRL, not just the signed portion.
OPENSSL_EXPORT int X509_CRL_digest(const X509_CRL *crl, const EVP_MD *md,
                                   uint8_t *out, unsigned *out_len);

// X509_REQ_digest hashes |req|'s DER encoding with |md| and writes the result
// to |out|. |EVP_MD_CTX_size| bytes are written, which is at most
// |EVP_MAX_MD_SIZE|. If |out_len| is not NULL, |*out_len| is set to the number
// of bytes written. This function returns one on success and zero on error.
// Note this digest covers the entire certificate request, not just the signed
// portion.
OPENSSL_EXPORT int X509_REQ_digest(const X509_REQ *req, const EVP_MD *md,
                                   uint8_t *out, unsigned *out_len);

// X509_NAME_digest hashes |name|'s DER encoding with |md| and writes the result
// to |out|. |EVP_MD_CTX_size| bytes are written, which is at most
// |EVP_MAX_MD_SIZE|. If |out_len| is not NULL, |*out_len| is set to the number
// of bytes written. This function returns one on success and zero on error.
OPENSSL_EXPORT int X509_NAME_digest(const X509_NAME *name, const EVP_MD *md,
                                    uint8_t *out, unsigned *out_len);

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

// X509_ALGOR_set0 sets |alg| to an AlgorithmIdentifier with algorithm |obj| and
// parameter determined by |param_type| and |param_value|. It returns one on
// success and zero on error. This function takes ownership of |obj| and
// |param_value| on success.
//
// If |param_type| is |V_ASN1_UNDEF|, the parameter is omitted. If |param_type|
// is zero, the parameter is left unchanged. Otherwise, |param_type| and
// |param_value| are interpreted as in |ASN1_TYPE_set|.
//
// Note omitting the parameter (|V_ASN1_UNDEF|) and encoding an explicit NULL
// value (|V_ASN1_NULL|) are different. Some algorithms require one and some the
// other. Consult the relevant specification before calling this function. The
// correct parameter for an RSASSA-PKCS1-v1_5 signature is |V_ASN1_NULL|. The
// correct one for an ECDSA or Ed25519 signature is |V_ASN1_UNDEF|.
OPENSSL_EXPORT int X509_ALGOR_set0(X509_ALGOR *alg, ASN1_OBJECT *obj,
                                   int param_type, void *param_value);

// X509_ALGOR_get0 sets |*out_obj| to the |alg|'s algorithm. If |alg|'s
// parameter is omitted, it sets |*out_param_type| and |*out_param_value| to
// |V_ASN1_UNDEF| and NULL. Otherwise, it sets |*out_param_type| and
// |*out_param_value| to the parameter, using the same representation as
// |ASN1_TYPE_set0|. See |ASN1_TYPE_set0| and |ASN1_TYPE| for details.
//
// Callers that require the parameter in serialized form should, after checking
// for |V_ASN1_UNDEF|, use |ASN1_TYPE_set1| and |d2i_ASN1_TYPE|, rather than
// inspecting |*out_param_value|.
//
// Each of |out_obj|, |out_param_type|, and |out_param_value| may be NULL to
// ignore the output. If |out_param_type| is NULL, |out_param_value| is ignored.
//
// WARNING: If |*out_param_type| is set to |V_ASN1_UNDEF|, OpenSSL and older
// revisions of BoringSSL leave |*out_param_value| unset rather than setting it
// to NULL. Callers that support both OpenSSL and BoringSSL should not assume
// |*out_param_value| is uniformly initialized.
OPENSSL_EXPORT void X509_ALGOR_get0(const ASN1_OBJECT **out_obj,
                                    int *out_param_type,
                                    const void **out_param_value,
                                    const X509_ALGOR *alg);

// X509_ALGOR_set_md sets |alg| to the hash function |md|. Note this
// AlgorithmIdentifier represents the hash function itself, not a signature
// algorithm that uses |md|.
OPENSSL_EXPORT void X509_ALGOR_set_md(X509_ALGOR *alg, const EVP_MD *md);

// X509_ALGOR_cmp returns zero if |a| and |b| are equal, and some non-zero value
// otherwise. Note this function can only be used for equality checks, not an
// ordering.
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

DECLARE_ASN1_ENCODE_FUNCTIONS(X509_ALGORS, X509_ALGORS, X509_ALGORS)
DECLARE_ASN1_FUNCTIONS(X509_VAL)

DECLARE_ASN1_FUNCTIONS(X509_PUBKEY)

OPENSSL_EXPORT int X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey);
OPENSSL_EXPORT EVP_PKEY *X509_PUBKEY_get(X509_PUBKEY *key);

DECLARE_ASN1_FUNCTIONS(X509_SIG)
DECLARE_ASN1_FUNCTIONS(X509_REQ_INFO)
DECLARE_ASN1_FUNCTIONS(X509_REQ)

DECLARE_ASN1_FUNCTIONS(X509_ATTRIBUTE)

// X509_ATTRIBUTE_create returns a newly-allocated |X509_ATTRIBUTE|, or NULL on
// error. The attribute has type |nid| and contains a single value determined by
// |attrtype| and |value|, which are interpreted as in |ASN1_TYPE_set|. Note
// this function takes ownership of |value|.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create(int nid, int attrtype,
                                                     void *value);

DECLARE_ASN1_FUNCTIONS(X509_EXTENSION)
DECLARE_ASN1_ENCODE_FUNCTIONS(X509_EXTENSIONS, X509_EXTENSIONS, X509_EXTENSIONS)

DECLARE_ASN1_FUNCTIONS(X509_NAME_ENTRY)

DECLARE_ASN1_FUNCTIONS(X509_NAME)

OPENSSL_EXPORT int X509_NAME_set(X509_NAME **xn, X509_NAME *name);

DECLARE_ASN1_FUNCTIONS(X509_CINF)

DECLARE_ASN1_FUNCTIONS(X509)
DECLARE_ASN1_FUNCTIONS(X509_CERT_AUX)

// X509_up_ref adds one to the reference count of |x509| and returns one.
OPENSSL_EXPORT int X509_up_ref(X509 *x509);

OPENSSL_EXPORT int X509_get_ex_new_index(long argl, void *argp,
                                         CRYPTO_EX_unused *unused,
                                         CRYPTO_EX_dup *dup_unused,
                                         CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int X509_set_ex_data(X509 *r, int idx, void *arg);
OPENSSL_EXPORT void *X509_get_ex_data(X509 *r, int idx);
OPENSSL_EXPORT int i2d_X509_AUX(X509 *a, unsigned char **pp);
OPENSSL_EXPORT X509 *d2i_X509_AUX(X509 **a, const unsigned char **pp,
                                  long length);

// i2d_re_X509_tbs serializes the TBSCertificate portion of |x509|. If |outp| is
// NULL, nothing is written. Otherwise, if |*outp| is not NULL, the result is
// written to |*outp|, which must have enough space available, and |*outp| is
// advanced just past the output. If |outp| is non-NULL and |*outp| is NULL, it
// sets |*outp| to a newly-allocated buffer containing the result. The caller is
// responsible for releasing the buffer with |OPENSSL_free|. In all cases, this
// function returns the number of bytes in the result, whether written or not,
// or a negative value on error.
//
// This function re-encodes the TBSCertificate and may not reflect |x509|'s
// original encoding. It may be used to manually generate a signature for a new
// certificate. To verify certificates, use |i2d_X509_tbs| instead.
OPENSSL_EXPORT int i2d_re_X509_tbs(X509 *x509, unsigned char **outp);

// i2d_X509_tbs serializes the TBSCertificate portion of |x509|. If |outp| is
// NULL, nothing is written. Otherwise, if |*outp| is not NULL, the result is
// written to |*outp|, which must have enough space available, and |*outp| is
// advanced just past the output. If |outp| is non-NULL and |*outp| is NULL, it
// sets |*outp| to a newly-allocated buffer containing the result. The caller is
// responsible for releasing the buffer with |OPENSSL_free|. In all cases, this
// function returns the number of bytes in the result, whether written or not,
// or a negative value on error.
//
// This function preserves the original encoding of the TBSCertificate and may
// not reflect modifications made to |x509|. It may be used to manually verify
// the signature of an existing certificate. To generate certificates, use
// |i2d_re_X509_tbs| instead.
OPENSSL_EXPORT int i2d_X509_tbs(X509 *x509, unsigned char **outp);

// X509_set1_signature_algo sets |x509|'s signature algorithm to |algo| and
// returns one on success or zero on error. It updates both the signature field
// of the TBSCertificate structure, and the signatureAlgorithm field of the
// Certificate.
OPENSSL_EXPORT int X509_set1_signature_algo(X509 *x509, const X509_ALGOR *algo);

// X509_set1_signature_value sets |x509|'s signature to a copy of the |sig_len|
// bytes pointed by |sig|. It returns one on success and zero on error.
//
// Due to a specification error, X.509 certificates store signatures in ASN.1
// BIT STRINGs, but signature algorithms return byte strings rather than bit
// strings. This function creates a BIT STRING containing a whole number of
// bytes, with the bit order matching the DER encoding. This matches the
// encoding used by all X.509 signature algorithms.
OPENSSL_EXPORT int X509_set1_signature_value(X509 *x509, const uint8_t *sig,
                                             size_t sig_len);

// X509_get0_signature sets |*out_sig| and |*out_alg| to the signature and
// signature algorithm of |x509|, respectively. Either output pointer may be
// NULL to ignore the value.
//
// This function outputs the outer signature algorithm. For the one in the
// TBSCertificate, see |X509_get0_tbs_sigalg|. Certificates with mismatched
// signature algorithms will successfully parse, but they will be rejected when
// verifying.
OPENSSL_EXPORT void X509_get0_signature(const ASN1_BIT_STRING **out_sig,
                                        const X509_ALGOR **out_alg,
                                        const X509 *x509);

// X509_get_signature_nid returns the NID corresponding to |x509|'s signature
// algorithm, or |NID_undef| if the signature algorithm does not correspond to
// a known NID.
OPENSSL_EXPORT int X509_get_signature_nid(const X509 *x509);

OPENSSL_EXPORT int X509_alias_set1(X509 *x, const unsigned char *name, int len);
OPENSSL_EXPORT int X509_keyid_set1(X509 *x, const unsigned char *id, int len);
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

// X509_get_serialNumber returns a mutable pointer to |x509|'s serial number.
// Prefer |X509_get0_serialNumber|.
OPENSSL_EXPORT ASN1_INTEGER *X509_get_serialNumber(X509 *x509);

// X509_set_issuer_name sets |x509|'s issuer to a copy of |name|. It returns one
// on success and zero on error.
OPENSSL_EXPORT int X509_set_issuer_name(X509 *x509, X509_NAME *name);

// X509_get_issuer_name returns |x509|'s issuer.
OPENSSL_EXPORT X509_NAME *X509_get_issuer_name(const X509 *x509);

// X509_set_subject_name sets |x509|'s subject to a copy of |name|. It returns
// one on success and zero on error.
OPENSSL_EXPORT int X509_set_subject_name(X509 *x509, X509_NAME *name);

// X509_get_issuer_name returns |x509|'s subject.
OPENSSL_EXPORT X509_NAME *X509_get_subject_name(const X509 *x509);

// X509_set_pubkey sets |x509|'s public key to |pkey|. It returns one on success
// and zero on error. This function does not take ownership of |pkey| and
// internally copies and updates reference counts as needed.
OPENSSL_EXPORT int X509_set_pubkey(X509 *x509, EVP_PKEY *pkey);

// X509_get_pubkey returns |x509|'s public key as an |EVP_PKEY|, or NULL if the
// public key was unsupported or could not be decoded. This function returns a
// reference to the |EVP_PKEY|. The caller must release the result with
// |EVP_PKEY_free| when done.
OPENSSL_EXPORT EVP_PKEY *X509_get_pubkey(X509 *x509);

// X509_get0_pubkey_bitstr returns the BIT STRING portion of |x509|'s public
// key. Note this does not contain the AlgorithmIdentifier portion.
//
// WARNING: This function returns a non-const pointer for OpenSSL compatibility,
// but the caller must not modify the resulting object. Doing so will break
// internal invariants in |x509|.
OPENSSL_EXPORT ASN1_BIT_STRING *X509_get0_pubkey_bitstr(const X509 *x509);

// X509_get0_extensions returns |x509|'s extension list, or NULL if |x509| omits
// it.
OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_get0_extensions(
    const X509 *x509);

// X509_get0_tbs_sigalg returns the signature algorithm in |x509|'s
// TBSCertificate. For the outer signature algorithm, see |X509_get0_signature|.
//
// Certificates with mismatched signature algorithms will successfully parse,
// but they will be rejected when verifying.
OPENSSL_EXPORT const X509_ALGOR *X509_get0_tbs_sigalg(const X509 *x509);

// X509_REQ_set_version sets |req|'s version to |version|, which should be
// |X509_REQ_VERSION_1|. It returns one on success and zero on error.
//
// Note no versions other than |X509_REQ_VERSION_1| are defined for CSRs.
OPENSSL_EXPORT int X509_REQ_set_version(X509_REQ *req, long version);

// X509_REQ_set_subject_name sets |req|'s subject to a copy of |name|. It
// returns one on success and zero on error.
OPENSSL_EXPORT int X509_REQ_set_subject_name(X509_REQ *req, X509_NAME *name);

// X509_REQ_get0_signature sets |*out_sig| and |*out_alg| to the signature and
// signature algorithm of |req|, respectively. Either output pointer may be NULL
// to ignore the value.
OPENSSL_EXPORT void X509_REQ_get0_signature(const X509_REQ *req,
                                            const ASN1_BIT_STRING **out_sig,
                                            const X509_ALGOR **out_alg);

// X509_REQ_get_signature_nid returns the NID corresponding to |req|'s signature
// algorithm, or |NID_undef| if the signature algorithm does not correspond to
// a known NID.
OPENSSL_EXPORT int X509_REQ_get_signature_nid(const X509_REQ *req);

// i2d_re_X509_REQ_tbs serializes the CertificationRequestInfo (see RFC2986)
// portion of |req|. If |outp| is NULL, nothing is written. Otherwise, if
// |*outp| is not NULL, the result is written to |*outp|, which must have enough
// space available, and |*outp| is advanced just past the output. If |outp| is
// non-NULL and |*outp| is NULL, it sets |*outp| to a newly-allocated buffer
// containing the result. The caller is responsible for releasing the buffer
// with |OPENSSL_free|. In all cases, this function returns the number of bytes
// in the result, whether written or not, or a negative value on error.
//
// This function re-encodes the CertificationRequestInfo and may not reflect
// |req|'s original encoding. It may be used to manually generate a signature
// for a new certificate request.
OPENSSL_EXPORT int i2d_re_X509_REQ_tbs(X509_REQ *req, uint8_t **outp);

// X509_REQ_set_pubkey sets |req|'s public key to |pkey|. It returns one on
// success and zero on error. This function does not take ownership of |pkey|
// and internally copies and updates reference counts as needed.
OPENSSL_EXPORT int X509_REQ_set_pubkey(X509_REQ *req, EVP_PKEY *pkey);

// X509_REQ_get_pubkey returns |req|'s public key as an |EVP_PKEY|, or NULL if
// the public key was unsupported or could not be decoded. This function returns
// a reference to the |EVP_PKEY|. The caller must release the result with
// |EVP_PKEY_free| when done.
OPENSSL_EXPORT EVP_PKEY *X509_REQ_get_pubkey(X509_REQ *req);

// X509_REQ_extension_nid returns one if |nid| is a supported CSR attribute type
// for carrying extensions and zero otherwise. The supported types are
// |NID_ext_req| (pkcs-9-at-extensionRequest from RFC2985) and |NID_ms_ext_req|
// (a Microsoft szOID_CERT_EXTENSIONS variant).
OPENSSL_EXPORT int X509_REQ_extension_nid(int nid);

// X509_REQ_get_extensions decodes the list of requested extensions in |req| and
// returns a newly-allocated |STACK_OF(X509_EXTENSION)| containing the result.
// It returns NULL on error, or if |req| did not request extensions.
//
// This function supports both pkcs-9-at-extensionRequest from RFC2985 and the
// Microsoft szOID_CERT_EXTENSIONS variant.
OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *X509_REQ_get_extensions(X509_REQ *req);

// X509_REQ_add_extensions_nid adds an attribute to |req| of type |nid|, to
// request the certificate extensions in |exts|. It returns one on success and
// zero on error. |nid| should be |NID_ext_req| or |NID_ms_ext_req|.
OPENSSL_EXPORT int X509_REQ_add_extensions_nid(
    X509_REQ *req, const STACK_OF(X509_EXTENSION) *exts, int nid);

// X509_REQ_add_extensions behaves like |X509_REQ_add_extensions_nid|, using the
// standard |NID_ext_req| for the attribute type.
OPENSSL_EXPORT int X509_REQ_add_extensions(
    X509_REQ *req, const STACK_OF(X509_EXTENSION) *exts);

// X509_REQ_get_attr_count returns the number of attributes in |req|.
OPENSSL_EXPORT int X509_REQ_get_attr_count(const X509_REQ *req);

// X509_REQ_get_attr_by_NID returns the index of the attribute in |req| of type
// |nid|, or a negative number if not found. If found, callers can use
// |X509_REQ_get_attr| to look up the attribute by index.
//
// If |lastpos| is non-negative, it begins searching at |lastpos| + 1. Callers
// can thus loop over all matching attributes by first passing -1 and then
// passing the previously-returned value until no match is returned.
OPENSSL_EXPORT int X509_REQ_get_attr_by_NID(const X509_REQ *req, int nid,
                                            int lastpos);

// X509_REQ_get_attr_by_OBJ behaves like |X509_REQ_get_attr_by_NID| but looks
// for attributes of type |obj|.
OPENSSL_EXPORT int X509_REQ_get_attr_by_OBJ(const X509_REQ *req,
                                            const ASN1_OBJECT *obj,
                                            int lastpos);

// X509_REQ_get_attr returns the attribute at index |loc| in |req|, or NULL if
// out of bounds.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_get_attr(const X509_REQ *req, int loc);

// X509_REQ_delete_attr removes the attribute at index |loc| in |req|. It
// returns the removed attribute to the caller, or NULL if |loc| was out of
// bounds. If non-NULL, the caller must release the result with
// |X509_ATTRIBUTE_free| when done. It is also safe, but not necessary, to call
// |X509_ATTRIBUTE_free| if the result is NULL.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_delete_attr(X509_REQ *req, int loc);

// X509_REQ_add1_attr appends a copy of |attr| to |req|'s list of attributes. It
// returns one on success and zero on error.
//
// TODO(https://crbug.com/boringssl/407): |attr| should be const.
OPENSSL_EXPORT int X509_REQ_add1_attr(X509_REQ *req, X509_ATTRIBUTE *attr);

// X509_REQ_add1_attr_by_OBJ appends a new attribute to |req| with type |obj|.
// It returns one on success and zero on error. The value is determined by
// |X509_ATTRIBUTE_set1_data|.
//
// WARNING: The interpretation of |attrtype|, |data|, and |len| is complex and
// error-prone. See |X509_ATTRIBUTE_set1_data| for details.
OPENSSL_EXPORT int X509_REQ_add1_attr_by_OBJ(X509_REQ *req,
                                             const ASN1_OBJECT *obj,
                                             int attrtype,
                                             const unsigned char *data,
                                             int len);

// X509_REQ_add1_attr_by_NID behaves like |X509_REQ_add1_attr_by_OBJ| except the
// attribute type is determined by |nid|.
OPENSSL_EXPORT int X509_REQ_add1_attr_by_NID(X509_REQ *req, int nid,
                                             int attrtype,
                                             const unsigned char *data,
                                             int len);

// X509_REQ_add1_attr_by_txt behaves like |X509_REQ_add1_attr_by_OBJ| except the
// attribute type is determined by calling |OBJ_txt2obj| with |attrname|.
OPENSSL_EXPORT int X509_REQ_add1_attr_by_txt(X509_REQ *req,
                                             const char *attrname, int attrtype,
                                             const unsigned char *data,
                                             int len);

// X509_CRL_set_version sets |crl|'s version to |version|, which should be one
// of the |X509_CRL_VERSION_*| constants. It returns one on success and zero on
// error.
//
// If unsure, use |X509_CRL_VERSION_2|. Note that, unlike certificates, CRL
// versions are only defined up to v2. Callers should not use |X509_VERSION_3|.
OPENSSL_EXPORT int X509_CRL_set_version(X509_CRL *crl, long version);

// X509_CRL_set_issuer_name sets |crl|'s issuer to a copy of |name|. It returns
// one on success and zero on error.
OPENSSL_EXPORT int X509_CRL_set_issuer_name(X509_CRL *crl, X509_NAME *name);

OPENSSL_EXPORT int X509_CRL_sort(X509_CRL *crl);

// X509_CRL_up_ref adds one to the reference count of |crl| and returns one.
OPENSSL_EXPORT int X509_CRL_up_ref(X509_CRL *crl);

// X509_CRL_get0_signature sets |*out_sig| and |*out_alg| to the signature and
// signature algorithm of |crl|, respectively. Either output pointer may be NULL
// to ignore the value.
//
// This function outputs the outer signature algorithm, not the one in the
// TBSCertList. CRLs with mismatched signature algorithms will successfully
// parse, but they will be rejected when verifying.
OPENSSL_EXPORT void X509_CRL_get0_signature(const X509_CRL *crl,
                                            const ASN1_BIT_STRING **out_sig,
                                            const X509_ALGOR **out_alg);

// X509_CRL_get_signature_nid returns the NID corresponding to |crl|'s signature
// algorithm, or |NID_undef| if the signature algorithm does not correspond to
// a known NID.
OPENSSL_EXPORT int X509_CRL_get_signature_nid(const X509_CRL *crl);

// i2d_re_X509_CRL_tbs serializes the TBSCertList portion of |crl|. If |outp| is
// NULL, nothing is written. Otherwise, if |*outp| is not NULL, the result is
// written to |*outp|, which must have enough space available, and |*outp| is
// advanced just past the output. If |outp| is non-NULL and |*outp| is NULL, it
// sets |*outp| to a newly-allocated buffer containing the result. The caller is
// responsible for releasing the buffer with |OPENSSL_free|. In all cases, this
// function returns the number of bytes in the result, whether written or not,
// or a negative value on error.
//
// This function re-encodes the TBSCertList and may not reflect |crl|'s original
// encoding. It may be used to manually generate a signature for a new CRL. To
// verify CRLs, use |i2d_X509_CRL_tbs| instead.
OPENSSL_EXPORT int i2d_re_X509_CRL_tbs(X509_CRL *crl, unsigned char **outp);

// i2d_X509_CRL_tbs serializes the TBSCertList portion of |crl|. If |outp| is
// NULL, nothing is written. Otherwise, if |*outp| is not NULL, the result is
// written to |*outp|, which must have enough space available, and |*outp| is
// advanced just past the output. If |outp| is non-NULL and |*outp| is NULL, it
// sets |*outp| to a newly-allocated buffer containing the result. The caller is
// responsible for releasing the buffer with |OPENSSL_free|. In all cases, this
// function returns the number of bytes in the result, whether written or not,
// or a negative value on error.
//
// This function preserves the original encoding of the TBSCertList and may not
// reflect modifications made to |crl|. It may be used to manually verify the
// signature of an existing CRL. To generate CRLs, use |i2d_re_X509_CRL_tbs|
// instead.
OPENSSL_EXPORT int i2d_X509_CRL_tbs(X509_CRL *crl, unsigned char **outp);

// X509_CRL_set1_signature_algo sets |crl|'s signature algorithm to |algo| and
// returns one on success or zero on error. It updates both the signature field
// of the TBSCertList structure, and the signatureAlgorithm field of the CRL.
OPENSSL_EXPORT int X509_CRL_set1_signature_algo(X509_CRL *crl,
                                                const X509_ALGOR *algo);

// X509_CRL_set1_signature_value sets |crl|'s signature to a copy of the
// |sig_len| bytes pointed by |sig|. It returns one on success and zero on
// error.
//
// Due to a specification error, X.509 CRLs store signatures in ASN.1 BIT
// STRINGs, but signature algorithms return byte strings rather than bit
// strings. This function creates a BIT STRING containing a whole number of
// bytes, with the bit order matching the DER encoding. This matches the
// encoding used by all X.509 signature algorithms.
OPENSSL_EXPORT int X509_CRL_set1_signature_value(X509_CRL *crl,
                                                 const uint8_t *sig,
                                                 size_t sig_len);

// X509_REVOKED_get0_serialNumber returns the serial number of the certificate
// revoked by |revoked|.
OPENSSL_EXPORT const ASN1_INTEGER *X509_REVOKED_get0_serialNumber(
    const X509_REVOKED *revoked);

// X509_REVOKED_set_serialNumber sets |revoked|'s serial number to |serial|. It
// returns one on success or zero on error.
OPENSSL_EXPORT int X509_REVOKED_set_serialNumber(X509_REVOKED *revoked,
                                                 const ASN1_INTEGER *serial);

// X509_REVOKED_get0_revocationDate returns the revocation time of the
// certificate revoked by |revoked|.
OPENSSL_EXPORT const ASN1_TIME *X509_REVOKED_get0_revocationDate(
    const X509_REVOKED *revoked);

// X509_REVOKED_set_revocationDate sets |revoked|'s revocation time to |tm|. It
// returns one on success or zero on error.
OPENSSL_EXPORT int X509_REVOKED_set_revocationDate(X509_REVOKED *revoked,
                                                   const ASN1_TIME *tm);

// X509_REVOKED_get0_extensions returns |r|'s extensions list, or NULL if |r|
// omits it.
OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_REVOKED_get0_extensions(
    const X509_REVOKED *r);

OPENSSL_EXPORT X509_CRL *X509_CRL_diff(X509_CRL *base, X509_CRL *newer,
                                       EVP_PKEY *skey, const EVP_MD *md,
                                       unsigned int flags);

OPENSSL_EXPORT int X509_REQ_check_private_key(X509_REQ *x509, EVP_PKEY *pkey);

OPENSSL_EXPORT int X509_check_private_key(X509 *x509, const EVP_PKEY *pkey);
OPENSSL_EXPORT int X509_chain_check_suiteb(int *perror_depth, X509 *x,
                                           STACK_OF(X509) *chain,
                                           unsigned long flags);
OPENSSL_EXPORT int X509_CRL_check_suiteb(X509_CRL *crl, EVP_PKEY *pk,
                                         unsigned long flags);

// X509_chain_up_ref returns a newly-allocated |STACK_OF(X509)| containing a
// shallow copy of |chain|, or NULL on error. That is, the return value has the
// same contents as |chain|, and each |X509|'s reference count is incremented by
// one.
OPENSSL_EXPORT STACK_OF(X509) *X509_chain_up_ref(STACK_OF(X509) *chain);

OPENSSL_EXPORT int X509_issuer_and_serial_cmp(const X509 *a, const X509 *b);

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

OPENSSL_EXPORT int X509v3_get_ext_count(const STACK_OF(X509_EXTENSION) *x);
OPENSSL_EXPORT int X509v3_get_ext_by_NID(const STACK_OF(X509_EXTENSION) *x,
                                         int nid, int lastpos);
OPENSSL_EXPORT int X509v3_get_ext_by_OBJ(const STACK_OF(X509_EXTENSION) *x,
                                         const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT int X509v3_get_ext_by_critical(const STACK_OF(X509_EXTENSION) *x,
                                              int crit, int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509v3_get_ext(const STACK_OF(X509_EXTENSION) *x,
                                              int loc);
OPENSSL_EXPORT X509_EXTENSION *X509v3_delete_ext(STACK_OF(X509_EXTENSION) *x,
                                                 int loc);
OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *X509v3_add_ext(
    STACK_OF(X509_EXTENSION) **x, X509_EXTENSION *ex, int loc);

OPENSSL_EXPORT int X509_get_ext_count(const X509 *x);
OPENSSL_EXPORT int X509_get_ext_by_NID(const X509 *x, int nid, int lastpos);
OPENSSL_EXPORT int X509_get_ext_by_OBJ(const X509 *x, const ASN1_OBJECT *obj,
                                       int lastpos);
OPENSSL_EXPORT int X509_get_ext_by_critical(const X509 *x, int crit,
                                            int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509_get_ext(const X509 *x, int loc);
OPENSSL_EXPORT X509_EXTENSION *X509_delete_ext(X509 *x, int loc);
OPENSSL_EXPORT int X509_add_ext(X509 *x, X509_EXTENSION *ex, int loc);

// X509_get_ext_d2i behaves like |X509V3_get_d2i| but looks for the extension in
// |x509|'s extension list.
//
// WARNING: This function is difficult to use correctly. See the documentation
// for |X509V3_get_d2i| for details.
OPENSSL_EXPORT void *X509_get_ext_d2i(const X509 *x509, int nid,
                                      int *out_critical, int *out_idx);

// X509_add1_ext_i2d behaves like |X509V3_add1_i2d| but adds the extension to
// |x|'s extension list.
//
// WARNING: This function may return zero or -1 on error. The caller must also
// ensure |value|'s type matches |nid|. See the documentation for
// |X509V3_add1_i2d| for details.
OPENSSL_EXPORT int X509_add1_ext_i2d(X509 *x, int nid, void *value, int crit,
                                     unsigned long flags);

OPENSSL_EXPORT int X509_CRL_get_ext_count(const X509_CRL *x);
OPENSSL_EXPORT int X509_CRL_get_ext_by_NID(const X509_CRL *x, int nid,
                                           int lastpos);
OPENSSL_EXPORT int X509_CRL_get_ext_by_OBJ(const X509_CRL *x,
                                           const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT int X509_CRL_get_ext_by_critical(const X509_CRL *x, int crit,
                                                int lastpos);
OPENSSL_EXPORT X509_EXTENSION *X509_CRL_get_ext(const X509_CRL *x, int loc);
OPENSSL_EXPORT X509_EXTENSION *X509_CRL_delete_ext(X509_CRL *x, int loc);
OPENSSL_EXPORT int X509_CRL_add_ext(X509_CRL *x, X509_EXTENSION *ex, int loc);

// X509_CRL_get_ext_d2i behaves like |X509V3_get_d2i| but looks for the
// extension in |crl|'s extension list.
//
// WARNING: This function is difficult to use correctly. See the documentation
// for |X509V3_get_d2i| for details.
OPENSSL_EXPORT void *X509_CRL_get_ext_d2i(const X509_CRL *crl, int nid,
                                          int *out_critical, int *out_idx);

// X509_CRL_add1_ext_i2d behaves like |X509V3_add1_i2d| but adds the extension
// to |x|'s extension list.
//
// WARNING: This function may return zero or -1 on error. The caller must also
// ensure |value|'s type matches |nid|. See the documentation for
// |X509V3_add1_i2d| for details.
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

// X509_REVOKED_get_ext_d2i behaves like |X509V3_get_d2i| but looks for the
// extension in |revoked|'s extension list.
//
// WARNING: This function is difficult to use correctly. See the documentation
// for |X509V3_get_d2i| for details.
OPENSSL_EXPORT void *X509_REVOKED_get_ext_d2i(const X509_REVOKED *revoked,
                                              int nid, int *out_critical,
                                              int *out_idx);

// X509_REVOKED_add1_ext_i2d behaves like |X509V3_add1_i2d| but adds the
// extension to |x|'s extension list.
//
// WARNING: This function may return zero or -1 on error. The caller must also
// ensure |value|'s type matches |nid|. See the documentation for
// |X509V3_add1_i2d| for details.
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

OPENSSL_EXPORT int X509at_get_attr_count(const STACK_OF(X509_ATTRIBUTE) *x);
OPENSSL_EXPORT int X509at_get_attr_by_NID(const STACK_OF(X509_ATTRIBUTE) *x,
                                          int nid, int lastpos);
OPENSSL_EXPORT int X509at_get_attr_by_OBJ(const STACK_OF(X509_ATTRIBUTE) *sk,
                                          const ASN1_OBJECT *obj, int lastpos);
OPENSSL_EXPORT X509_ATTRIBUTE *X509at_get_attr(
    const STACK_OF(X509_ATTRIBUTE) *x, int loc);
OPENSSL_EXPORT X509_ATTRIBUTE *X509at_delete_attr(STACK_OF(X509_ATTRIBUTE) *x,
                                                  int loc);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr(
    STACK_OF(X509_ATTRIBUTE) **x, X509_ATTRIBUTE *attr);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_OBJ(
    STACK_OF(X509_ATTRIBUTE) **x, const ASN1_OBJECT *obj, int type,
    const unsigned char *bytes, int len);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_NID(
    STACK_OF(X509_ATTRIBUTE) **x, int nid, int type, const unsigned char *bytes,
    int len);
OPENSSL_EXPORT STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_txt(
    STACK_OF(X509_ATTRIBUTE) **x, const char *attrname, int type,
    const unsigned char *bytes, int len);
OPENSSL_EXPORT void *X509at_get0_data_by_OBJ(STACK_OF(X509_ATTRIBUTE) *x,
                                             ASN1_OBJECT *obj, int lastpos,
                                             int type);

// X509_ATTRIBUTE_create_by_NID returns a newly-allocated |X509_ATTRIBUTE| of
// type |nid|, or NULL on error. The value is determined as in
// |X509_ATTRIBUTE_set1_data|.
//
// If |attr| is non-NULL, the resulting |X509_ATTRIBUTE| is also written to
// |*attr|. If |*attr| was non-NULL when the function was called, |*attr| is
// reused instead of creating a new object.
//
// WARNING: The interpretation of |attrtype|, |data|, and |len| is complex and
// error-prone. See |X509_ATTRIBUTE_set1_data| for details.
//
// WARNING: The object reuse form is deprecated and may be removed in the
// future. It also currently incorrectly appends to the reused object's value
// set rather than overwriting it.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_NID(
    X509_ATTRIBUTE **attr, int nid, int attrtype, const void *data, int len);

// X509_ATTRIBUTE_create_by_OBJ behaves like |X509_ATTRIBUTE_create_by_NID|
// except the attribute's type is determined by |obj|.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_OBJ(
    X509_ATTRIBUTE **attr, const ASN1_OBJECT *obj, int attrtype,
    const void *data, int len);

// X509_ATTRIBUTE_create_by_txt behaves like |X509_ATTRIBUTE_create_by_NID|
// except the attribute's type is determined by calling |OBJ_txt2obj| with
// |attrname|.
OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_txt(
    X509_ATTRIBUTE **attr, const char *attrname, int type,
    const unsigned char *bytes, int len);

// X509_ATTRIBUTE_set1_object sets |attr|'s type to |obj|. It returns one on
// success and zero on error.
OPENSSL_EXPORT int X509_ATTRIBUTE_set1_object(X509_ATTRIBUTE *attr,
                                              const ASN1_OBJECT *obj);

// X509_ATTRIBUTE_set1_data appends a value to |attr|'s value set and returns
// one on success or zero on error. The value is determined as follows:
//
// If |attrtype| is a |MBSTRING_*| constant, the value is an ASN.1 string. The
// string is determined by decoding |len| bytes from |data| in the encoding
// specified by |attrtype|, and then re-encoding it in a form appropriate for
// |attr|'s type. If |len| is -1, |strlen(data)| is used instead. See
// |ASN1_STRING_set_by_NID| for details.
//
// TODO(davidben): Document |ASN1_STRING_set_by_NID| so the reference is useful.
//
// Otherwise, if |len| is not -1, the value is an ASN.1 string. |attrtype| is an
// |ASN1_STRING| type value and the |len| bytes from |data| are copied as the
// type-specific representation of |ASN1_STRING|. See |ASN1_STRING| for details.
//
// WARNING: If this form is used to construct a negative INTEGER or ENUMERATED,
// |attrtype| includes the |V_ASN1_NEG| flag for |ASN1_STRING|, but the function
// forgets to clear the flag for |ASN1_TYPE|. This matches OpenSSL but is
// probably a bug. For now, do not use this form with negative values.
//
// Otherwise, if |len| is -1, the value is constructed by passing |attrtype| and
// |data| to |ASN1_TYPE_set1|. That is, |attrtype| is an |ASN1_TYPE| type value,
// and |data| is cast to the corresponding pointer type.
//
// WARNING: Despite the name, this function appends to |attr|'s value set,
// rather than overwriting it. To overwrite the value set, create a new
// |X509_ATTRIBUTE| with |X509_ATTRIBUTE_new|.
//
// WARNING: If using the |MBSTRING_*| form, pass a length rather than relying on
// |strlen|. In particular, |strlen| will not behave correctly if the input is
// |MBSTRING_BMP| or |MBSTRING_UNIV|.
//
// WARNING: This function currently misinterprets |V_ASN1_OTHER| as an
// |MBSTRING_*| constant. This matches OpenSSL but means it is impossible to
// construct a value with a non-universal tag.
OPENSSL_EXPORT int X509_ATTRIBUTE_set1_data(X509_ATTRIBUTE *attr, int attrtype,
                                            const void *data, int len);

// X509_ATTRIBUTE_get0_data returns the |idx|th value of |attr| in a
// type-specific representation to |attrtype|, or NULL if out of bounds or the
// type does not match. |attrtype| is one of the type values in |ASN1_TYPE|. On
// match, the return value uses the same representation as |ASN1_TYPE_set0|. See
// |ASN1_TYPE| for details.
OPENSSL_EXPORT void *X509_ATTRIBUTE_get0_data(X509_ATTRIBUTE *attr, int idx,
                                              int attrtype, void *unused);

// X509_ATTRIBUTE_count returns the number of values in |attr|.
OPENSSL_EXPORT int X509_ATTRIBUTE_count(const X509_ATTRIBUTE *attr);

// X509_ATTRIBUTE_get0_object returns the type of |attr|.
OPENSSL_EXPORT ASN1_OBJECT *X509_ATTRIBUTE_get0_object(X509_ATTRIBUTE *attr);

// X509_ATTRIBUTE_get0_type returns the |idx|th value in |attr|, or NULL if out
// of bounds. Note this function returns one of |attr|'s values, not the type.
OPENSSL_EXPORT ASN1_TYPE *X509_ATTRIBUTE_get0_type(X509_ATTRIBUTE *attr,
                                                   int idx);

OPENSSL_EXPORT int X509_verify_cert(X509_STORE_CTX *ctx);

// lookup a cert from a X509 STACK
OPENSSL_EXPORT X509 *X509_find_by_issuer_and_serial(STACK_OF(X509) *sk,
                                                    X509_NAME *name,
                                                    ASN1_INTEGER *serial);
OPENSSL_EXPORT X509 *X509_find_by_subject(STACK_OF(X509) *sk, X509_NAME *name);

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

// X509_PUBKEY_set0_param sets |pub| to a key with AlgorithmIdentifier
// determined by |obj|, |param_type|, and |param_value|, and an encoded
// public key of |key|. On success, it takes ownership of all its parameters and
// returns one. Otherwise, it returns zero. |key| must have been allocated by
// |OPENSSL_malloc|.
//
// |obj|, |param_type|, and |param_value| are interpreted as in
// |X509_ALGOR_set0|. See |X509_ALGOR_set0| for details.
OPENSSL_EXPORT int X509_PUBKEY_set0_param(X509_PUBKEY *pub, ASN1_OBJECT *obj,
                                          int param_type, void *param_value,
                                          uint8_t *key, int key_len);

// X509_PUBKEY_get0_param outputs fields of |pub| and returns one. If |out_obj|
// is not NULL, it sets |*out_obj| to AlgorithmIdentifier's OID. If |out_key|
// is not NULL, it sets |*out_key| and |*out_key_len| to the encoded public key.
// If |out_alg| is not NULL, it sets |*out_alg| to the AlgorithmIdentifier.
//
// Note: X.509 SubjectPublicKeyInfo structures store the encoded public key as a
// BIT STRING. |*out_key| and |*out_key_len| will silently pad the key with zero
// bits if |pub| did not contain a whole number of bytes. Use
// |X509_PUBKEY_get0_public_key| to preserve this information.
OPENSSL_EXPORT int X509_PUBKEY_get0_param(ASN1_OBJECT **out_obj,
                                          const uint8_t **out_key,
                                          int *out_key_len,
                                          X509_ALGOR **out_alg,
                                          X509_PUBKEY *pub);

// X509_PUBKEY_get0_public_key returns |pub|'s encoded public key.
OPENSSL_EXPORT const ASN1_BIT_STRING *X509_PUBKEY_get0_public_key(
    const X509_PUBKEY *pub);

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
BORINGSSL_MAKE_DELETER(X509_ATTRIBUTE, X509_ATTRIBUTE_free)
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
