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

#ifndef OPENSSL_HEADER_PEM_H
#define OPENSSL_HEADER_PEM_H

#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/pkcs7.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

// For compatibility with open-iscsi, which assumes that it can get
// `OPENSSL_malloc` from pem.h or err.h
#include <openssl/crypto.h>

#if defined(__cplusplus)
extern "C" {
#endif


// PEM.
//
// This library contains functions for reading and writing data encoded in PEM
// format. This format originated in Privacy-Enhanced Mail (RFC 1421). PEM
// consists of a series of PEM blocks, which are line-wrapped, base64-encoded
// data, wrapped in BEGIN and END lines, for example:
//
//   -----BEGIN PUBLIC KEY-----
//   MCowBQYDK2VwAyEAGb9ECWmEzf6FQbrBZ9w7lshQhqowtrbLDFw4rXAxZuE=
//   -----END PUBLIC KEY-----
//
// The BEGIN and END markers specify the type of data encoded. Multiple PEM
// blocks can be concatenated together. For example, it is common to represent a
// certificate chain as a series of PEM blocks of type CERTIFICATE. Owing to its
// email roots, PEM blocks can also be embedded in non-PEM data. The parsers in
// this library will generally skip over any non-PEM data, as well as any PEM
// blocks that are not of the expected type.
//
// PEM blocks can be encrypted with a password, specified in RFC 1423. PEM
// encryption is vulnerable to padding oracle attacks and should not be used as
// a load-bearing security measure. It is implemented for interoperability with
// legacy systems only.
//
// Callers should only use PEM for compatibility with legacy systems. PEM use
// should be limited to the components that directly interoperate with those
// systems, converting to and from more modern formats. PEM adds overhead to fit
// in email's 7-bit ASCII limitations, a constraint that is not relevant to most
// applications.


// API conventions.

// pem_password_cb is an application-supplied callback to supply a PEM password.
// On success, the callback should write the password into `out`, which has room
// for at most `max_out` bytes. On error, including if `max_out` is too small,
// it should return -1. `enc` is one if the password will be used for encrypting
// and zero if it is used for decrypting. `userdata` is the application-supplied
// parameter of the same name to the PEM function.
typedef int pem_password_cb(char *out, int max_out, int enc, void *userdata);

// PEM_def_callback is the default implementation of `pem_password_cb`. It
// interprets `userdata` as a NUL-terminated C string and outputs it according
// to `pem_password_cb`. If `userdata` is NULL, it returns -1.
//
// This differs from OpenSSL, which interactively prompts for a password by
// default.
OPENSSL_EXPORT int PEM_def_callback(char *buf, int size, int enc,
                                    void *userdata);

// The following sample functions document the calling conventions used by this
// library.

#if 0  // Sample functions

// PEM_read_bio_SAMPLE reads a PEM block from `bio`, skipping non-PEM data and
// unexpected block types. It then decodes the resulting PEM block and returns a
// newly-allocated `SAMPLE` object containing the parsed structure. If `out` is
// non-NULL, it additionally frees the previous value at `*out` and updates
// `*out` to the result.
//
// On decode or allocation error, or if EOF is reached before a matching PEM
// block is found, it returns NULL. In the error case, it will add
// `PEM_R_NO_START_LINE` to the error queue. Callers can check the error queue
// to distinguish these cases.
//
// If the PEM block is encrypted, `cb` will be called to look up the password.
// See `pem_password_cb` for details. If `cb` is NULL, `PEM_def_callback` is
// used and `userdata` should be a NUL-terminated C string containing the
// password. Set both `cb` and `userdata` to NULL to only handle plaintext
// blocks.
SAMPLE *PEM_read_bio_SAMPLE(BIO *bio, SAMPLE **out, pem_password_cb *cb,
                            void *userdata);

// PEM_write_bio_SAMPLE encodes `in` as a PEM block and writes it to `bio`. It
// returns one on success and zero on error.
//
// If `enc` is non-NULL, the PEM block is encrypted with the specified cipher
// and a password. If `pass` is non-NULL, `pass_len` bytes from `pass` is
// used as the password. Otherwise, `cb` is called. If `cb` is NULL,
// `PEM_def_callback` is used. PEM encryption is vulnerable to padding oracle
// attacks and should not be used.
//
// Some functions of this convention do not support encryption. In this case,
// the encryption-related parameters will be omitted.
//
// On error, the state of `bio` is undefined. It is possible a prefix of a PEM
// block was left in the `bio`.
int PEM_write_bio_SAMPLE(BIO *bio, const SAMPLE *in, const EVP_CIPHER *enc,
                         const uint8_t *pass, int pass_len, pem_password_cb *cb,
                         void *userdata);

#endif  // Sample functions


// Reading and writing X.509 structures as PEM.

// PEM_read_bio_X509 reads a PEM block of type "CERTIFICATE" (RFC 7468) or "X509
// CERTIFICATE", as described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT X509 *PEM_read_bio_X509(BIO *bio, X509 **out,
                                       pem_password_cb *cb, void *userdata);

// PEM_write_bio_X509 writes a PEM block of type "CERTIFICATE" (RFC 7468), as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_X509(BIO *bio, const X509 *in);

// PEM_read_bio_X509_AUX reads a PEM block of type "CERTIFICATE" (RFC 7468),
// "X509 CERTIFICATE", or "TRUSTED CERTIFICATE", as described in
// `PEM_read_bio_SAMPLE`.
//
// WARNING: This function parses auxiliary properties as in `d2i_X509_AUX`.
// Passing untrusted input to this function allows an attacker to influence
// those properties. See `d2i_X509_AUX` for details.
OPENSSL_EXPORT X509 *PEM_read_bio_X509_AUX(BIO *bio, X509 **out,
                                           pem_password_cb *cb, void *userdata);

// PEM_write_bio_X509_AUX writes a PEM block of type "TRUSTED CERTIFICATE", as
// described in `PEM_write_bio_SAMPLE`.
//
// WARNING: This function writes a custom OpenSSL-specific format that includes
// auxiliary properties. See `i2d_X509_AUX`.
OPENSSL_EXPORT int PEM_write_bio_X509_AUX(BIO *bio, const X509 *in);

// PEM_read_bio_X509_CRL reads a PEM block of type "X509 CRL" (RFC 7468), as
// described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT X509_CRL *PEM_read_bio_X509_CRL(BIO *bio, X509_CRL **out,
                                               pem_password_cb *cb,
                                               void *userdata);

// PEM_write_bio_X509_CRL writes a PEM block of type "X509 CRL" (RFC 7468), as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_X509_CRL(BIO *bio, const X509_CRL *in);

// PEM_read_bio_X509_REQ reads a PEM block of type "CERTIFICATE REQUEST"
// (RFC 7468) or "NEW CERTIFICATE REQUEST", as described in
// `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT X509_REQ *PEM_read_bio_X509_REQ(BIO *bio, X509_REQ **out,
                                               pem_password_cb *cb,
                                               void *userdata);

// PEM_write_bio_X509_REQ writes a PEM block of type "CERTIFICATE REQUEST"
// (RFC 7468), as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_X509_REQ(BIO *bio, const X509_REQ *in);

// PEM_write_bio_X509_REQ_NEW writes a PEM block of type "NEW CERTIFICATE
// REQUEST", as described in `PEM_write_bio_SAMPLE`. Prefer to use
// `PEM_write_bio_X509_REQ`. "NEW CERTIFICATE REQUEST" is the older type.
OPENSSL_EXPORT int PEM_write_bio_X509_REQ_NEW(BIO *bio, const X509_REQ *in);

// PEM_read_bio_PKCS7 reads a PEM block of type "PKCS7" (RFC 7468) or "PKCS #7
// SIGNED DATA", as described in `PEM_read_bio_SAMPLE`.
//
// This function also accepts type "CERTIFICATE" but decodes the contents as a
// PKCS #7 structure rather than a certificate. This is a historical workaround
// for an old CA bug.
OPENSSL_EXPORT PKCS7 *PEM_read_bio_PKCS7(BIO *bio, PKCS7 **out,
                                         pem_password_cb *cb, void *userdata);

// PEM_write_bio_PKCS7 writes a PEM block of type "PKCS7" (RFC 7468), as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_PKCS7(BIO *bio, const PKCS7 *in);

// PEM_X509_INFO_read_bio reads PEM blocks from `bio` and decodes any
// certificates, CRLs, and private keys found. It returns a
// `STACK_OF(X509_INFO)` structure containing the results, or NULL on error.
//
// If `sk` is NULL, the result on success will be a newly-allocated
// `STACK_OF(X509_INFO)` structure which should be released with
// `sk_X509_INFO_pop_free` and `X509_INFO_free` when done.
//
// If `sk` is non-NULL, it appends the results to `sk` instead and returns `sk`
// on success. In this case, the caller retains ownership of `sk` in both
// success and failure.
//
// This function will decrypt any encrypted certificates in `bio`, using `cb`,
// but it will not decrypt encrypted private keys. Encrypted private keys are
// instead represented as placeholder `X509_INFO` objects with an empty `x_pkey`
// field. This allows this function to be used with inputs with unencrypted
// certificates, but encrypted passwords, without knowing the password. However,
// it also means that this function cannot be used to decrypt the private key
// when the password is known.
//
// WARNING: If the input contains "TRUSTED CERTIFICATE" PEM blocks, this
// function parses auxiliary properties as in `d2i_X509_AUX`. Passing untrusted
// input to this function allows an attacker to influence those properties. See
// `d2i_X509_AUX` for details.
OPENSSL_EXPORT STACK_OF(X509_INFO) *PEM_X509_INFO_read_bio(
    BIO *bio, STACK_OF(X509_INFO) *sk, pem_password_cb *cb, void *userdata);


// Reading and writing keys as PEM.
//
// There are multiple PEM formats for public and private keys:
//
// Public keys are generally encoded with type "PUBLIC KEY" (RFC 7468), which
// encodes a SubjectPublicKeyInfo structure (RFC 5280).
//
// Private keys may be encoded with type "PRIVATE KEY" or "ENCRYPTED PRIVATE
// KEY" (RFC 7468), which encode a PrivateKeyInfo or EncryptedPrivateKeyInfo
// (RFC 5208) structure. EncryptedPrivateKeyInfo is, itself, a mechanism for
// encrypting private keys with a password, so private keys may be encrypted
// with PEM encryption, PKCS #8 encryption, or both.
//
// There are also older algorithm-specific PEM types for public and private
// keys, including "RSA PUBLIC KEY", "RSA PRIVATE KEY", "EC PRIVATE KEY", and
// "DSA PRIVATE KEY". Some functions in this library will read or write them.
//
// If unsure, use the "PUBLIC KEY" and "PRIVATE KEY" formats.

// PEM_read_bio_PrivateKey reads a PEM block containing a private key, as
// described in `PEM_read_bio_SAMPLE`. It handles generic PKCS #8 blocks of type
// "PRIVATE KEY" and "ENCRYPTED PRIVATE KEY", as well as key-specific formats
// like "RSA PRIVATE KEY", "EC PRIVATE KEY", and "DSA PRIVATE KEY".
//
// `cb` and `userdata` are used to look up the password for both PEM-level
// encryption as well as PKCS #8 encryption, in the case of "ENCRYPTED PRIVATE
// KEY" blocks.
OPENSSL_EXPORT EVP_PKEY *PEM_read_bio_PrivateKey(BIO *bio, EVP_PKEY **out,
                                                 pem_password_cb *cb,
                                                 void *userdata);

// PEM_write_bio_PrivateKey writes `in` to `bio` in PKCS#8 format, as described
// in `PEM_write_bio_SAMPLE`. However, it interprets encryption parameters
// differently:
//
// - If not encrypting (`enc` is NULL), it writes the key as a PEM block of type
//   "PRIVATE KEY".
//
// - If encrypted (`enc` is not NULL), it encrypts the key in an
//   EncryptedPrivateKeyInfo using PBES2 (see `PKCS8_encrypt`) and writes the
//   result as a PEM block of type "ENCRYPTED PRIVATE KEY".
OPENSSL_EXPORT int PEM_write_bio_PrivateKey(BIO *bio, const EVP_PKEY *in,
                                            const EVP_CIPHER *enc,
                                            const uint8_t *pass, int pass_len,
                                            pem_password_cb *cb,
                                            void *userdata);

// PEM_read_bio_PUBKEY reads a PEM block of type "PUBLIC KEY", as described in
// `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT EVP_PKEY *PEM_read_bio_PUBKEY(BIO *bio, EVP_PKEY **out,
                                             pem_password_cb *cb,
                                             void *userdata);

// PEM_write_bio_PUBKEY writes `in` as a PEM block of type "PUBLIC KEY", as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_PUBKEY(BIO *bio, const EVP_PKEY *in);

// PEM_write_bio_PKCS8PrivateKey_nid behaves like `PEM_write_bio_PrivateKey`
// but uses the PBES1 algorithm specified by `nid` for encryption. See also
// `PKCS8_encrypt`. If `nid` is -1, it writes the private key unencrypted.
OPENSSL_EXPORT int PEM_write_bio_PKCS8PrivateKey_nid(
    BIO *bio, const EVP_PKEY *in, int nid, const char *pass, int pass_len,
    pem_password_cb *cb, void *userdata);

// PEM_write_bio_PKCS8PrivateKey is an alias for `PEM_write_bio_PrivateKey`.
OPENSSL_EXPORT int PEM_write_bio_PKCS8PrivateKey(BIO *bio, const EVP_PKEY *in,
                                                 const EVP_CIPHER *enc,
                                                 const char *pass, int pass_len,
                                                 pem_password_cb *cb,
                                                 void *userdata);

// PEM_write_PKCS8PrivateKey_nid behaves like
// `PEM_write_bio_PKCS8PrivateKey_nid` but writes to `fp`.
OPENSSL_EXPORT int PEM_write_PKCS8PrivateKey_nid(FILE *fp, const EVP_PKEY *in,
                                                 int nid, const char *pass,
                                                 int pass_len,
                                                 pem_password_cb *cb,
                                                 void *userdata);

// PEM_write_PKCS8PrivateKey behaves like `PEM_write_bio_PKCS8PrivateKey` but
// writes to `fp`.
OPENSSL_EXPORT int PEM_write_PKCS8PrivateKey(FILE *fp, const EVP_PKEY *in,
                                             const EVP_CIPHER *enc,
                                             const char *pass, int pass_len,
                                             pem_password_cb *cb,
                                             void *userdata);

// PEM_read_bio_PKCS8 reads a PEM block of type "ENCRYPTED PRIVATE KEY", as
// described in `PEM_read_bio_SAMPLE`.
//
// Although this function accepts `cb` and `userdata` to decrypt the PEM-level
// encryption, it does not decrypt the EncryptedPrivateKeyInfo structure. It
// returns the encrypted key as an `X509_SIG` object which, despite its name, is
// an algorithm and octet string pair.
OPENSSL_EXPORT X509_SIG *PEM_read_bio_PKCS8(BIO *bio, X509_SIG **out,
                                            pem_password_cb *cb,
                                            void *userdata);

// PEM_write_bio_PKCS8 writes a PEM block of type "ENCRYPTED PRIVATE KEY", as
// described in `PEM_write_bio_SAMPLE`. `in` is used to represent an
// already-encrypted EncryptedPrivateKeyInfo structure.
OPENSSL_EXPORT int PEM_write_bio_PKCS8(BIO *bio, const X509_SIG *in);

// PEM_read_bio_PKCS8_PRIV_KEY_INFO reads a PEM block of type "PRIVATE KEY", as
// described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *PEM_read_bio_PKCS8_PRIV_KEY_INFO(
    BIO *bio, PKCS8_PRIV_KEY_INFO **out, pem_password_cb *cb, void *userdata);

// PEM_write_bio_PKCS8_PRIV_KEY_INFO writes a PEM block of type "PRIVATE KEY",
// as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_PKCS8_PRIV_KEY_INFO(
    BIO *bio, const PKCS8_PRIV_KEY_INFO *in);

// PEM_read_bio_RSAPrivateKey behaves like `PEM_read_bio_PrivateKey` but only
// returns RSA keys, represented as an `RSA` object. Keys of other types result
// in an error.
OPENSSL_EXPORT RSA *PEM_read_bio_RSAPrivateKey(BIO *bio, RSA **out,
                                               pem_password_cb *cb,
                                               void *userdata);

// PEM_write_bio_RSAPrivateKey writes `in` as a PEM block of type "RSA PRIVATE
// KEY", as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_RSAPrivateKey(
    BIO *bio, const RSA *in, const EVP_CIPHER *enc, const uint8_t *pass,
    int pass_len, pem_password_cb *cb, void *userdata);

// PEM_read_bio_RSAPublicKey reads a PEM block of type "RSA PUBLIC KEY", as
// described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT RSA *PEM_read_bio_RSAPublicKey(BIO *bio, RSA **out,
                                              pem_password_cb *cb,
                                              void *userdata);

// PEM_write_bio_RSAPublicKey writes a PEM block of type "RSA PUBLIC KEY", as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_RSAPublicKey(BIO *bio, const RSA *in);

// PEM_read_bio_RSA_PUBKEY behaves like `PEM_read_bio_PUBKEY` but only returns
// RSA keys, represented as an `RSA` object. Keys of other types result in an
// error.
OPENSSL_EXPORT RSA *PEM_read_bio_RSA_PUBKEY(BIO *bio, RSA **out,
                                            pem_password_cb *cb,
                                            void *userdata);

// PEM_write_bio_RSA_PUBKEY writes `in` as a PEM block of type "PUBLIC KEY", as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_RSA_PUBKEY(BIO *bio, const RSA *in);

// PEM_read_bio_DSAPrivateKey behaves like `PEM_read_bio_PrivateKey` but only
// returns DSA keys, represented as a `DSA` object. Keys of other types result
// in an error.
OPENSSL_EXPORT DSA *PEM_read_bio_DSAPrivateKey(BIO *bio, DSA **out,
                                               pem_password_cb *cb,
                                               void *userdata);

// PEM_write_bio_DSAPrivateKey writes `in` as a PEM block of type "DSA PRIVATE
// KEY", as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_DSAPrivateKey(
    BIO *bio, const DSA *in, const EVP_CIPHER *enc, const uint8_t *pass,
    int pass_len, pem_password_cb *cb, void *userdata);

// PEM_read_bio_DSA_PUBKEY behaves like `PEM_read_bio_PUBKEY` but only returns
// DSA keys, represented as a `DSA` object. Keys of other types result in an
// error.
OPENSSL_EXPORT DSA *PEM_read_bio_DSA_PUBKEY(BIO *bio, DSA **out,
                                            pem_password_cb *cb,
                                            void *userdata);

// PEM_write_bio_DSA_PUBKEY writes `in` in SubjectPublicKeyInfo format as a PEM
// block of type "PUBLIC KEY", as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_DSA_PUBKEY(BIO *bio, const DSA *in);

// PEM_read_bio_DSAparams reads a PEM block of type "DSA PARAMETERS", as
// described in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT DSA *PEM_read_bio_DSAparams(BIO *bio, DSA **out,
                                           pem_password_cb *cb, void *userdata);

// PEM_write_bio_DSAparams writes a PEM block of type "DSA PARAMETERS", as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_DSAparams(BIO *bio, const DSA *in);

// PEM_read_bio_ECPrivateKey behaves like `PEM_read_bio_PrivateKey` but only
// returns EC keys, represented as an `EC_KEY` object. Keys of other types
// result in an error.
OPENSSL_EXPORT EC_KEY *PEM_read_bio_ECPrivateKey(BIO *bio, EC_KEY **out,
                                                 pem_password_cb *cb,
                                                 void *userdata);

// PEM_write_bio_ECPrivateKey writes `in` as a PEM block of type "EC PRIVATE
// KEY", as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_ECPrivateKey(BIO *bio, const EC_KEY *in,
                                              const EVP_CIPHER *enc,
                                              const uint8_t *pass, int pass_len,
                                              pem_password_cb *cb,
                                              void *userdata);

// PEM_read_bio_EC_PUBKEY behaves like `PEM_read_bio_PUBKEY` but only returns
// EC keys, represented as an `EC_KEY` object. Keys of other types result in an
// error.
OPENSSL_EXPORT EC_KEY *PEM_read_bio_EC_PUBKEY(BIO *bio, EC_KEY **out,
                                              pem_password_cb *cb,
                                              void *userdata);

// PEM_write_bio_EC_PUBKEY writes `in` in SubjectPublicKeyInfo format as a PEM
// block of type "PUBLIC KEY", as described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_EC_PUBKEY(BIO *bio, const EC_KEY *in);

// PEM_read_bio_DHparams reads a PEM block of type "DH PARAMETERS", as described
// in `PEM_read_bio_SAMPLE`.
OPENSSL_EXPORT DH *PEM_read_bio_DHparams(BIO *bio, DH **out,
                                         pem_password_cb *cb, void *userdata);

// PEM_write_bio_DHparams writes a PEM block of type "DH PARAMETERS", as
// described in `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_write_bio_DHparams(BIO *bio, const DH *in);


// File-based functions.

// The following functions behave like the corresponding `PEM_read_bio_*`
// functions, but read from `fp`.
OPENSSL_EXPORT X509 *PEM_read_X509(FILE *fp, X509 **out, pem_password_cb *cb,
                                   void *userdata);
OPENSSL_EXPORT X509_CRL *PEM_read_X509_CRL(FILE *fp, X509_CRL **out,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT X509 *PEM_read_X509_AUX(FILE *fp, X509 **out,
                                       pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT STACK_OF(X509_INFO) *PEM_X509_INFO_read(FILE *fp,
                                                       STACK_OF(X509_INFO) *sk,
                                                       pem_password_cb *cb,
                                                       void *userdata);
OPENSSL_EXPORT X509_REQ *PEM_read_X509_REQ(FILE *fp, X509_REQ **out,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT PKCS7 *PEM_read_PKCS7(FILE *fp, PKCS7 **out, pem_password_cb *cb,
                                     void *userdata);
OPENSSL_EXPORT X509_SIG *PEM_read_PKCS8(FILE *fp, X509_SIG **out,
                                        pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *PEM_read_PKCS8_PRIV_KEY_INFO(
    FILE *fp, PKCS8_PRIV_KEY_INFO **out, pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **out,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT RSA *PEM_read_RSAPublicKey(FILE *fp, RSA **out,
                                          pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT RSA *PEM_read_RSA_PUBKEY(FILE *fp, RSA **out,
                                        pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT DSA *PEM_read_DSAPrivateKey(FILE *fp, DSA **out,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT DSA *PEM_read_DSA_PUBKEY(FILE *fp, DSA **out,
                                        pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT DSA *PEM_read_DSAparams(FILE *fp, DSA **out, pem_password_cb *cb,
                                       void *userdata);
OPENSSL_EXPORT EC_KEY *PEM_read_ECPrivateKey(FILE *fp, EC_KEY **out,
                                             pem_password_cb *cb,
                                             void *userdata);
OPENSSL_EXPORT EC_KEY *PEM_read_EC_PUBKEY(FILE *fp, EC_KEY **out,
                                          pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT DH *PEM_read_DHparams(FILE *fp, DH **out, pem_password_cb *cb,
                                     void *userdata);
OPENSSL_EXPORT EVP_PKEY *PEM_read_PrivateKey(FILE *fp, EVP_PKEY **out,
                                             pem_password_cb *cb,
                                             void *userdata);
OPENSSL_EXPORT EVP_PKEY *PEM_read_PUBKEY(FILE *fp, EVP_PKEY **out,
                                         pem_password_cb *cb, void *userdata);

// The following functions behave like the corresponding `PEM_write_bio_*`
// functions, but write to `fp`.
OPENSSL_EXPORT int PEM_write_X509(FILE *fp, const X509 *x);
OPENSSL_EXPORT int PEM_write_X509_CRL(FILE *fp, const X509_CRL *in);
OPENSSL_EXPORT int PEM_write_X509_AUX(FILE *fp, const X509 *in);
OPENSSL_EXPORT int PEM_write_X509_REQ(FILE *fp, const X509_REQ *in);
OPENSSL_EXPORT int PEM_write_X509_REQ_NEW(FILE *fp, const X509_REQ *in);
OPENSSL_EXPORT int PEM_write_PKCS7(FILE *fp, const PKCS7 *in);
OPENSSL_EXPORT int PEM_write_PKCS8(FILE *fp, const X509_SIG *in);
OPENSSL_EXPORT int PEM_write_PKCS8_PRIV_KEY_INFO(FILE *fp,
                                                 const PKCS8_PRIV_KEY_INFO *in);
OPENSSL_EXPORT int PEM_write_RSAPrivateKey(FILE *fp, const RSA *in,
                                           const EVP_CIPHER *enc,
                                           const uint8_t *pass, int pass_len,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT int PEM_write_RSAPublicKey(FILE *fp, const RSA *in);
OPENSSL_EXPORT int PEM_write_RSA_PUBKEY(FILE *fp, const RSA *in);
OPENSSL_EXPORT int PEM_write_DSAPrivateKey(FILE *fp, const DSA *in,
                                           const EVP_CIPHER *enc,
                                           const uint8_t *pass, int pass_len,
                                           pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT int PEM_write_DSA_PUBKEY(FILE *fp, const DSA *in);
OPENSSL_EXPORT int PEM_write_DSAparams(FILE *fp, const DSA *in);
OPENSSL_EXPORT int PEM_write_ECPrivateKey(FILE *fp, const EC_KEY *in,
                                          const EVP_CIPHER *enc,
                                          const uint8_t *pass, int pass_len,
                                          pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT int PEM_write_EC_PUBKEY(FILE *fp, const EC_KEY *in);
OPENSSL_EXPORT int PEM_write_DHparams(FILE *fp, const DH *in);
OPENSSL_EXPORT int PEM_write_PrivateKey(FILE *fp, const EVP_PKEY *in,
                                        const EVP_CIPHER *enc,
                                        const uint8_t *pass, int pass_len,
                                        pem_password_cb *cb, void *userdata);
OPENSSL_EXPORT int PEM_write_PUBKEY(FILE *fp, const EVP_PKEY *in);


// Reading and writing raw PEM blocks.
//
// The functions in this section read and write PEM blocks without decoding
// their contents.

// PEM_BUFSIZE is an arbitrary buffer size used within the library. Some
// external callers depend on it being defined.
#define PEM_BUFSIZE 1024

// The following constants are a collection of known PEM types.
#define PEM_STRING_X509_OLD "X509 CERTIFICATE"
#define PEM_STRING_X509 "CERTIFICATE"
#define PEM_STRING_X509_PAIR "CERTIFICATE PAIR"
#define PEM_STRING_X509_TRUSTED "TRUSTED CERTIFICATE"
#define PEM_STRING_X509_REQ_OLD "NEW CERTIFICATE REQUEST"
#define PEM_STRING_X509_REQ "CERTIFICATE REQUEST"
#define PEM_STRING_X509_CRL "X509 CRL"
#define PEM_STRING_PUBLIC "PUBLIC KEY"
#define PEM_STRING_RSA "RSA PRIVATE KEY"
#define PEM_STRING_RSA_PUBLIC "RSA PUBLIC KEY"
#define PEM_STRING_DSA "DSA PRIVATE KEY"
#define PEM_STRING_DSA_PUBLIC "DSA PUBLIC KEY"
#define PEM_STRING_EC "EC PRIVATE KEY"
#define PEM_STRING_PKCS7 "PKCS7"
#define PEM_STRING_PKCS7_SIGNED "PKCS #7 SIGNED DATA"
#define PEM_STRING_PKCS8 "ENCRYPTED PRIVATE KEY"
#define PEM_STRING_PKCS8INF "PRIVATE KEY"
#define PEM_STRING_DHPARAMS "DH PARAMETERS"
#define PEM_STRING_SSL_SESSION "SSL SESSION PARAMETERS"
#define PEM_STRING_DSAPARAMS "DSA PARAMETERS"
#define PEM_STRING_ECDSA_PUBLIC "ECDSA PUBLIC KEY"
#define PEM_STRING_ECPRIVATEKEY "EC PRIVATE KEY"
#define PEM_STRING_CMS "CMS"

// PEM_STRING_EVP_PKEY is not a PEM type, but is an implementation detail of
// `PEM_read_bio_PrivateKey`.
#define PEM_STRING_EVP_PKEY "ANY PRIVATE KEY"

// PEM_read_bio reads from `bio` until the next PEM block. If one is found, it
// returns one and sets `*out_name`, `*out_header`, and `*out_data` to
// newly-allocated buffers containing the PEM type, the header block, and the
// decoded data, respectively. `*out_name` and `*out_header` are NUL-terminated
// C strings, while `*out_data` has `*out_len` bytes. The caller must release
// each of `*out_name`, `*out_header`, and `*out_data` with `OPENSSL_free` when
// done.
//
// If no PEM block is found, this function returns zero and pushes
// `PEM_R_NO_START_LINE` to the error queue. If one is found, but there is an
// error decoding it, it returns zero and pushes some other error to the error
// queue.
//
// This function does not decrypt encrypted PEM blocks and instead returns the
// header and (possibly encrypted) data unprocessed. See `PEM_bytes_read_bio` to
// decrypt blocks.
OPENSSL_EXPORT int PEM_read_bio(BIO *bio, char **out_name, char **out_header,
                                uint8_t **out_data, long *out_len);

// PEM_read behaves like `PEM_read_bio` but reads from `fp`.
OPENSSL_EXPORT int PEM_read(FILE *fp, char **out_name, char **out_header,
                            uint8_t **out_data, long *out_len);

// PEM_write_bio writes a PEM block to `bio`, containing `len` bytes from `data`
// as data. `name` and `hdr` are NUL-terminated C strings containing the PEM
// type and header block, respectively. This function returns zero on error and
// the number of bytes written on success.
OPENSSL_EXPORT int PEM_write_bio(BIO *bio, const char *name, const char *hdr,
                                 const uint8_t *data, long len);

// PEM_write behaves like `PEM_write_bio` but reads from `fp`.
OPENSSL_EXPORT int PEM_write(FILE *fp, const char *name, const char *hdr,
                             const uint8_t *data, long len);

// PEM_bytes_read_bio reads from `bio` until it finds a PEM block whose name
// matches `expected_name`. If one is found, it sets `*out_name` and `*out_data`
// to newly-allocated buffers containing the PEM type and (possibly decrypted)
// PEM data. `*out_name` is a NUL-terminated C string, while `*out_data` has
// `*out_len` bytes. The caller must release `*out_name` and `*out_data` with
// `OPENSSL_free` when done.
//
// If the PEM block is encrypted, `cb` will be called to look up the password.
// See `pem_password_cb` for details. If `cb` is NULL, `PEM_def_callback` is
// used and `userdata` should be a NUL-terminated C string containing the
// password. Set both `cb` and `userdata` to NULL to only handle plaintext
// blocks.
//
// `expected_name` and `*out_name` may not necessarily be the same value, so
// callers must check `*out_name` before decoding `*out_data`. In addition to an
// exact match, the following values are also accepted:
//
// - If `expected_name` is "CERTIFICATE", the older "X509 CERTIFICATE" type is
//   also accepted.
//
// - If `expected_name` is "CERTIFICATE REQUEST", the older "NEW CERTIFICATE
//   REQUEST" type is also accepted.
//
// - If `expected_name` is "TRUSTED CERTIFICATE", "CERTIFICATE" and the older
//   "X509 CERTIFICATE" are also accepted.
//
// - If `expected_name` is "PKCS7", "PKCS #7 SIGNED DATA" is also accepted.
//
// - If `expected_name` is "PKCS7", "CERTIFICATE" is also accepted. This is an
//   exposed implementation detail of `PKCS7_get_PEM_certificates`, which works
//   around a 2000-era mistake by some CAs.
//
// - If `expected_name` is "ANY PRIVATE KEY", the type "ANY PRIVATE KEY" is not
//   accepted and, instead, the function accepts "PRIVATE KEY", "ENCRYPTED
//   PRIVATE KEY", "RSA PRIVATE KEY", "EC PRIVATE KEY", and "DSA PRIVATE KEY".
//   This is an exposed implementation detail of `PEM_read_bio_PrivateKey`.
//
// TODO(davidben): Can some of the older aliases and workarounds be removed now?
//
// If no PEM block is found, this function returns zero and pushes
// `PEM_R_NO_START_LINE` to the error queue. If one is found, but there is an
// error decoding it, it returns zero and pushes some other error to the error
// queue.
OPENSSL_EXPORT int PEM_bytes_read_bio(uint8_t **out_data, long *out_len,
                                      char **out_name,
                                      const char *expected_name, BIO *bio,
                                      pem_password_cb *cb, void *userdata);


// Reading and writing DER-encoded private keys.

// d2i_PKCS8PrivateKey_bio reads a DER-encoded EncryptedPrivateKey structure
// (RFC 5208) from `bio`, decrypts it with `PKCS8_decrypt`, and returns the
// result as a newly-allocated `EVP_PKEY`, or NULL on error. On success, if
// `out` is non-NULL, it additionally frees the previous value at `*out` and
// updates `*out` to the result. The password is determined by calling `cb`, or
// `PEM_def_callback` if NULL.
OPENSSL_EXPORT EVP_PKEY *d2i_PKCS8PrivateKey_bio(BIO *bio, EVP_PKEY **out,
                                                 pem_password_cb *cb,
                                                 void *userdata);

// i2d_PKCS8PrivateKey_bio encodes `in` as a DER-encoded structure and writes
// it to `bio`. It returns one on success and zero on error. The structure used
// depends on `enc`:
//
// - If `enc` is NULL, it writes a PrivateKeyInfo structure (RFC 5208).
//
// - If `enc` is non-NULL, it writes an EncryptedPrivateKeyInfo structure (RFC
//   5280). The key is encrypted with PBES2, as in `PKCS8_encrypt`. The password
//   is specified by `pass`, `cb`, and `userdata`, as in `PEM_write_bio_SAMPLE`.
//
// WARNING: PrivateKeyInfo and EncryptedPrivateKeyInfo are different formats,
// and DER does not include a type header. The encrypted and unencrypted modes
// of this function should not be mixed in the same context.
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_bio(BIO *bio, const EVP_PKEY *in,
                                           const EVP_CIPHER *enc,
                                           const char *pass, int pass_len,
                                           pem_password_cb *cb, void *userdata);

// i2d_PKCS8PrivateKey_nid_bio encodes `in` as a DER-encoded structure and
// writes it to `bio`. It returns one on success and zero on error. The
// structure used depends on `nid`:
//
// - If `nid` is -1, it writes a PrivateKeyInfo structure (RFC 5208).
//
// - Otherwise, it writes an EncryptedPrivateKeyInfo structure (RFC 5280). The
//   key is encrypted with PBES1, as in `PKCS8_encrypt`. The password is
//   specified by `pass`, `cb`, and `userdata`, as in `PEM_write_bio_SAMPLE`.
//
// WARNING: PrivateKeyInfo and EncryptedPrivateKeyInfo are different formats,
// and DER does not include a type header. The encrypted and unencrypted modes
// of this function should not be mixed in the same context.
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_nid_bio(BIO *bio, const EVP_PKEY *in,
                                               int nid, const char *pass,
                                               int pass_len,
                                               pem_password_cb *cb,
                                               void *userdata);

// i2d_PKCS8PrivateKey_fp behaves like `i2d_PKCS8PrivateKey_bio` but writes to
// `fp`.
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_fp(FILE *fp, const EVP_PKEY *in,
                                          const EVP_CIPHER *enc,
                                          const char *pass, int pass_len,
                                          pem_password_cb *cb, void *userdata);

// i2d_PKCS8PrivateKey_nid_fp behaves like `i2d_PKCS8PrivateKey_nid_bio` but
// writes to `fp`.
OPENSSL_EXPORT int i2d_PKCS8PrivateKey_nid_fp(FILE *fp, const EVP_PKEY *in,
                                              int nid, const char *pass,
                                              int pass_len, pem_password_cb *cb,
                                              void *userdata);

// d2i_PKCS8PrivateKey_fp behaves like `d2i_PKCS8PrivateKey_bio` but reads from
// `fp`.
OPENSSL_EXPORT EVP_PKEY *d2i_PKCS8PrivateKey_fp(FILE *fp, EVP_PKEY **out,
                                                pem_password_cb *cb,
                                                void *userdata);


// Internal functions.
//
// The following functions are used to implement `PEM_read_bio_*` and
// `PEM_write_bio_*`. They should not be used outside the library.

// PEM_ASN1_read_bio calls `PEM_bytes_read_bio` and then decodes the resulting
// data with `d2i`, which should behave as in `d2i_SAMPLE`.
OPENSSL_EXPORT void *PEM_ASN1_read_bio(d2i_of_void *d2i, const char *name,
                                       BIO *bio, void **out,
                                       pem_password_cb *cb, void *userdata);

// PEM_ASN1_read behaves like `PEM_ASN1_read_bio` but reads from `fp`.
OPENSSL_EXPORT void *PEM_ASN1_read(d2i_of_void *d2i, const char *name, FILE *fp,
                                   void **out, pem_password_cb *cb,
                                   void *userdata);

// PEM_ASN1_write_bio encodes `in` with `i2d`, which should behave as in
// `i2d_SAMPLE`. It then writes the result to `bio` as in
// `PEM_write_bio_SAMPLE`.
OPENSSL_EXPORT int PEM_ASN1_write_bio(i2d_of_void *i2d, const char *name,
                                      BIO *bio, const void *in,
                                      const EVP_CIPHER *enc,
                                      const uint8_t *pass, int pass_len,
                                      pem_password_cb *cb, void *userdata);

// PEM_ASN1_write behaves like `PEM_ASN1_write_bio` but reads from `fp`.
OPENSSL_EXPORT int PEM_ASN1_write(i2d_of_void *i2d, const char *name, FILE *fp,
                                  const void *in, const EVP_CIPHER *enc,
                                  const uint8_t *pass, int pass_len,
                                  pem_password_cb *callback, void *userdata);


#if defined(__cplusplus)
}  // extern C
#endif

#define PEM_R_BAD_BASE64_DECODE 100
#define PEM_R_BAD_DECRYPT 101
#define PEM_R_BAD_END_LINE 102
#define PEM_R_BAD_IV_CHARS 103
#define PEM_R_BAD_PASSWORD_READ 104
#define PEM_R_CIPHER_IS_NULL 105
#define PEM_R_ERROR_CONVERTING_PRIVATE_KEY 106
#define PEM_R_NOT_DEK_INFO 107
#define PEM_R_NOT_ENCRYPTED 108
#define PEM_R_NOT_PROC_TYPE 109
#define PEM_R_NO_START_LINE 110
#define PEM_R_READ_KEY 111
#define PEM_R_SHORT_HEADER 112
#define PEM_R_UNSUPPORTED_CIPHER 113
#define PEM_R_UNSUPPORTED_ENCRYPTION 114
#define PEM_R_UNSUPPORTED_PROC_TYPE_VERSION 115
#define PEM_R_NO_DATA 116

#endif  // OPENSSL_HEADER_PEM_H
