// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
// Copyright 2005 Nokia. All rights reserved.
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

// IWYU pragma: private


#ifndef OPENSSL_HEADER_SSL_DEPRECATED_H
#define OPENSSL_HEADER_SSL_DEPRECATED_H

#include <openssl/base.h>  // IWYU pragma: export
#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Deprecated SSL functions.
//
// This header contains functions that are part of <openssl/ssl.h>, but
// deprecated. They are placed in a separate file to make it clearer that they
// are deprecated, but there is no need to include this header directly.
// Including <openssl/ssl.h> will always include this header.


// SSL_library_init returns one.
OPENSSL_EXPORT int SSL_library_init(void);

// SSL_CIPHER_description writes a description of `cipher` into `buf` and
// returns `buf`. If `buf` is NULL, it returns a newly allocated string, to be
// freed with `OPENSSL_free`, or NULL on error.
//
// The description includes a trailing newline and has the form:
// AES128-SHA              Kx=RSA      Au=RSA  Enc=AES(128)  Mac=SHA1
//
// Consider `SSL_CIPHER_standard_name` or `SSL_CIPHER_get_name` instead.
OPENSSL_EXPORT const char *SSL_CIPHER_description(const SSL_CIPHER *cipher,
                                                  char *buf, int len);

// SSL_CIPHER_get_version returns the string "TLSv1/SSLv3".
OPENSSL_EXPORT const char *SSL_CIPHER_get_version(const SSL_CIPHER *cipher);

// SSL_CIPHER_get_id returns `cipher`'s IANA-assigned number, OR-d with
// 0x03000000. This is part of OpenSSL's SSL 2.0 legacy. SSL 2.0 has long since
// been removed from BoringSSL. Use `SSL_CIPHER_get_protocol_id` instead.
OPENSSL_EXPORT uint32_t SSL_CIPHER_get_id(const SSL_CIPHER *cipher);

// SSL_CIPHER_get_name returns the OpenSSL name of `cipher`. For example,
// "ECDHE-RSA-AES128-GCM-SHA256". Callers are recommended to use
// `SSL_CIPHER_standard_name` instead.
OPENSSL_EXPORT const char *SSL_CIPHER_get_name(const SSL_CIPHER *cipher);

typedef void COMP_METHOD;
typedef struct ssl_comp_st SSL_COMP;

// SSL_COMP_get_compression_methods returns NULL.
OPENSSL_EXPORT STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void);

// SSL_COMP_add_compression_method returns one.
OPENSSL_EXPORT int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm);

// SSL_COMP_get_name returns NULL.
OPENSSL_EXPORT const char *SSL_COMP_get_name(const COMP_METHOD *comp);

// SSL_COMP_get0_name returns the `name` member of `comp`.
OPENSSL_EXPORT const char *SSL_COMP_get0_name(const SSL_COMP *comp);

// SSL_COMP_get_id returns the `id` member of `comp`.
OPENSSL_EXPORT int SSL_COMP_get_id(const SSL_COMP *comp);

// SSL_COMP_free_compression_methods does nothing.
OPENSSL_EXPORT void SSL_COMP_free_compression_methods(void);

// SSLv23_method calls `TLS_method`.
OPENSSL_EXPORT const SSL_METHOD *SSLv23_method(void);

// These version-specific methods behave exactly like `TLS_method` and
// `DTLS_method` except they also call `SSL_CTX_set_min_proto_version` and
// `SSL_CTX_set_max_proto_version` to lock connections to that protocol
// version.
OPENSSL_EXPORT const SSL_METHOD *TLSv1_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_method(void);

// These client- and server-specific methods call their corresponding generic
// methods.
OPENSSL_EXPORT const SSL_METHOD *TLS_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLS_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *SSLv23_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *SSLv23_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLS_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLS_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_client_method(void);

// SSL_clear resets `ssl` to allow another connection and returns one on success
// or zero on failure. It returns most configuration state but releases memory
// associated with the current connection.
//
// Free `ssl` and create a new one instead.
OPENSSL_EXPORT int SSL_clear(SSL *ssl);

// SSL_CTX_set_tmp_rsa_callback does nothing.
OPENSSL_EXPORT void SSL_CTX_set_tmp_rsa_callback(
    SSL_CTX *ctx, RSA *(*cb)(SSL *ssl, int is_export, int keylength));

// SSL_set_tmp_rsa_callback does nothing.
OPENSSL_EXPORT void SSL_set_tmp_rsa_callback(SSL *ssl,
                                             RSA *(*cb)(SSL *ssl, int is_export,
                                                        int keylength));

// SSL_CTX_sess_connect returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_connect(const SSL_CTX *ctx);

// SSL_CTX_sess_connect_good returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_connect_good(const SSL_CTX *ctx);

// SSL_CTX_sess_connect_renegotiate returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_connect_renegotiate(const SSL_CTX *ctx);

// SSL_CTX_sess_accept returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_accept(const SSL_CTX *ctx);

// SSL_CTX_sess_accept_renegotiate returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_accept_renegotiate(const SSL_CTX *ctx);

// SSL_CTX_sess_accept_good returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_accept_good(const SSL_CTX *ctx);

// SSL_CTX_sess_hits returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_hits(const SSL_CTX *ctx);

// SSL_CTX_sess_cb_hits returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_cb_hits(const SSL_CTX *ctx);

// SSL_CTX_sess_misses returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_misses(const SSL_CTX *ctx);

// SSL_CTX_sess_timeouts returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_timeouts(const SSL_CTX *ctx);

// SSL_CTX_sess_cache_full returns zero.
OPENSSL_EXPORT int SSL_CTX_sess_cache_full(const SSL_CTX *ctx);

// SSL_cutthrough_complete calls `SSL_in_false_start`.
OPENSSL_EXPORT int SSL_cutthrough_complete(const SSL *ssl);

// SSL_num_renegotiations calls `SSL_total_renegotiations`.
OPENSSL_EXPORT int SSL_num_renegotiations(const SSL *ssl);

// SSL_CTX_need_tmp_RSA returns zero.
OPENSSL_EXPORT int SSL_CTX_need_tmp_RSA(const SSL_CTX *ctx);

// SSL_need_tmp_RSA returns zero.
OPENSSL_EXPORT int SSL_need_tmp_RSA(const SSL *ssl);

// SSL_CTX_set_tmp_rsa returns one.
OPENSSL_EXPORT int SSL_CTX_set_tmp_rsa(SSL_CTX *ctx, const RSA *rsa);

// SSL_set_tmp_rsa returns one.
OPENSSL_EXPORT int SSL_set_tmp_rsa(SSL *ssl, const RSA *rsa);

// SSL_CTX_get_read_ahead returns zero.
OPENSSL_EXPORT int SSL_CTX_get_read_ahead(const SSL_CTX *ctx);

// SSL_CTX_set_read_ahead returns one.
OPENSSL_EXPORT int SSL_CTX_set_read_ahead(SSL_CTX *ctx, int yes);

// SSL_get_read_ahead returns zero.
OPENSSL_EXPORT int SSL_get_read_ahead(const SSL *ssl);

// SSL_set_read_ahead returns one.
OPENSSL_EXPORT int SSL_set_read_ahead(SSL *ssl, int yes);

// SSL_set_state does nothing.
OPENSSL_EXPORT void SSL_set_state(SSL *ssl, int state);

// SSL_get_shared_ciphers writes an empty string to `buf` and returns a
// pointer to `buf`, or NULL if `len` is less than or equal to zero.
OPENSSL_EXPORT char *SSL_get_shared_ciphers(const SSL *ssl, char *buf, int len);

// SSL_get_shared_sigalgs returns zero.
OPENSSL_EXPORT int SSL_get_shared_sigalgs(SSL *ssl, int idx, int *psign,
                                          int *phash, int *psignandhash,
                                          uint8_t *rsig, uint8_t *rhash);

// SSL_MODE_HANDSHAKE_CUTTHROUGH is the same as SSL_MODE_ENABLE_FALSE_START.
#define SSL_MODE_HANDSHAKE_CUTTHROUGH SSL_MODE_ENABLE_FALSE_START

// i2d_SSL_SESSION serializes `in`, as described in `i2d_SAMPLE`.
//
// Use `SSL_SESSION_to_bytes` instead.
OPENSSL_EXPORT int i2d_SSL_SESSION(const SSL_SESSION *in, uint8_t **pp);

// d2i_SSL_SESSION parses a serialized session from the `len` bytes pointed to
// by `*inp`, as described in `d2i_SAMPLE`.
//
// Use `SSL_SESSION_from_bytes` instead.
OPENSSL_EXPORT SSL_SESSION *d2i_SSL_SESSION(SSL_SESSION **out,
                                            const uint8_t **inp, long len);

// i2d_SSL_SESSION_bio serializes `session` and writes the result to `bio`. It
// returns the number of bytes written on success and <= 0 on error.
OPENSSL_EXPORT int i2d_SSL_SESSION_bio(BIO *bio, const SSL_SESSION *session);

// d2i_SSL_SESSION_bio reads a serialized `SSL_SESSION` from `bio` and returns a
// newly-allocated `SSL_SESSION` or NULL on error. If `out` is not NULL, it also
// frees `*out` and sets `*out` to the new `SSL_SESSION`.
OPENSSL_EXPORT SSL_SESSION *d2i_SSL_SESSION_bio(BIO *bio, SSL_SESSION **out);

// ERR_load_SSL_strings does nothing.
OPENSSL_EXPORT void ERR_load_SSL_strings(void);

// SSL_load_error_strings does nothing.
OPENSSL_EXPORT void SSL_load_error_strings(void);

// SSL_CTX_set_tlsext_use_srtp calls `SSL_CTX_set_srtp_profiles`. It returns
// zero on success and one on failure.
//
// WARNING: this function is dangerous because it breaks the usual return value
// convention. Use `SSL_CTX_set_srtp_profiles` instead.
OPENSSL_EXPORT int SSL_CTX_set_tlsext_use_srtp(SSL_CTX *ctx,
                                               const char *profiles);

// SSL_set_tlsext_use_srtp calls `SSL_set_srtp_profiles`. It returns zero on
// success and one on failure.
//
// WARNING: this function is dangerous because it breaks the usual return value
// convention. Use `SSL_set_srtp_profiles` instead.
OPENSSL_EXPORT int SSL_set_tlsext_use_srtp(SSL *ssl, const char *profiles);

// SSL_get_current_compression returns NULL.
OPENSSL_EXPORT const COMP_METHOD *SSL_get_current_compression(SSL *ssl);

// SSL_get_current_expansion returns NULL.
OPENSSL_EXPORT const COMP_METHOD *SSL_get_current_expansion(SSL *ssl);

// SSL_get_server_tmp_key returns zero.
OPENSSL_EXPORT int SSL_get_server_tmp_key(SSL *ssl, EVP_PKEY **out_key);

// SSL_get_peer_tmp_key returns zero.
OPENSSL_EXPORT int SSL_get_peer_tmp_key(SSL *ssl, EVP_PKEY **out_key);

// SSL_CTX_set_tmp_dh returns 1.
OPENSSL_EXPORT int SSL_CTX_set_tmp_dh(SSL_CTX *ctx, const DH *dh);

// SSL_set_tmp_dh returns 1.
OPENSSL_EXPORT int SSL_set_tmp_dh(SSL *ssl, const DH *dh);

// SSL_CTX_set_tmp_dh_callback does nothing.
OPENSSL_EXPORT void SSL_CTX_set_tmp_dh_callback(
    SSL_CTX *ctx, DH *(*cb)(SSL *ssl, int is_export, int keylength));

// SSL_set_tmp_dh_callback does nothing.
OPENSSL_EXPORT void SSL_set_tmp_dh_callback(SSL *ssl,
                                            DH *(*cb)(SSL *ssl, int is_export,
                                                      int keylength));

// SSL_CTX_set1_sigalgs takes `num_values` ints and interprets them as pairs
// where the first is the nid of a hash function and the second is an
// `EVP_PKEY_*` value. It configures the signature algorithm preferences for
// `ctx` based on them and returns one on success or zero on error.
//
// This API is compatible with OpenSSL. However, BoringSSL-specific code should
// prefer `SSL_CTX_set_signing_algorithm_prefs` because it's clearer and it's
// more convenient to codesearch for specific algorithm values.
OPENSSL_EXPORT int SSL_CTX_set1_sigalgs(SSL_CTX *ctx, const int *values,
                                        size_t num_values);

// SSL_set1_sigalgs takes `num_values` ints and interprets them as pairs where
// the first is the nid of a hash function and the second is an `EVP_PKEY_*`
// value. It configures the signature algorithm preferences for `ssl` based on
// them and returns one on success or zero on error.
//
// This API is compatible with OpenSSL. However, BoringSSL-specific code should
// prefer `SSL_CTX_set_signing_algorithm_prefs` because it's clearer and it's
// more convenient to codesearch for specific algorithm values.
OPENSSL_EXPORT int SSL_set1_sigalgs(SSL *ssl, const int *values,
                                    size_t num_values);

// SSL_CTX_set1_sigalgs_list takes a textual specification of a set of signature
// algorithms and configures them on `ctx`. It returns one on success and zero
// on error. See
// https://www.openssl.org/docs/man1.1.0/man3/SSL_CTX_set1_sigalgs_list.html for
// a description of the text format. Also note that TLS 1.3 names (e.g.
// "rsa_pkcs1_md5_sha1") can also be used (as in OpenSSL, although OpenSSL
// doesn't document that).
//
// This API is compatible with OpenSSL. However, BoringSSL-specific code should
// prefer `SSL_CTX_set_signing_algorithm_prefs` because it's clearer and it's
// more convenient to codesearch for specific algorithm values.
OPENSSL_EXPORT int SSL_CTX_set1_sigalgs_list(SSL_CTX *ctx, const char *str);

// SSL_set1_sigalgs_list takes a textual specification of a set of signature
// algorithms and configures them on `ssl`. It returns one on success and zero
// on error. See
// https://www.openssl.org/docs/man1.1.0/man3/SSL_CTX_set1_sigalgs_list.html for
// a description of the text format. Also note that TLS 1.3 names (e.g.
// "rsa_pkcs1_md5_sha1") can also be used (as in OpenSSL, although OpenSSL
// doesn't document that).
//
// This API is compatible with OpenSSL. However, BoringSSL-specific code should
// prefer `SSL_CTX_set_signing_algorithm_prefs` because it's clearer and it's
// more convenient to codesearch for specific algorithm values.
OPENSSL_EXPORT int SSL_set1_sigalgs_list(SSL *ssl, const char *str);

#define SSL_set_app_data(s, arg) (SSL_set_ex_data(s, 0, (char *)(arg)))
#define SSL_get_app_data(s) (SSL_get_ex_data(s, 0))
#define SSL_SESSION_set_app_data(s, a) \
  (SSL_SESSION_set_ex_data(s, 0, (char *)(a)))
#define SSL_SESSION_get_app_data(s) (SSL_SESSION_get_ex_data(s, 0))
#define SSL_CTX_get_app_data(ctx) (SSL_CTX_get_ex_data(ctx, 0))
#define SSL_CTX_set_app_data(ctx, arg) \
  (SSL_CTX_set_ex_data(ctx, 0, (char *)(arg)))

#define OpenSSL_add_ssl_algorithms() SSL_library_init()
#define SSLeay_add_ssl_algorithms() SSL_library_init()

#define SSL_get_cipher(ssl) SSL_CIPHER_get_name(SSL_get_current_cipher(ssl))
#define SSL_get_cipher_bits(ssl, out_alg_bits) \
  SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), out_alg_bits)
#define SSL_get_cipher_version(ssl) \
  SSL_CIPHER_get_version(SSL_get_current_cipher(ssl))
#define SSL_get_cipher_name(ssl) \
  SSL_CIPHER_get_name(SSL_get_current_cipher(ssl))
#define SSL_get_time(session) SSL_SESSION_get_time(session)
#define SSL_set_time(session, time) SSL_SESSION_set_time((session), (time))
#define SSL_get_timeout(session) SSL_SESSION_get_timeout(session)
#define SSL_set_timeout(session, timeout) \
  SSL_SESSION_set_timeout((session), (timeout))

struct ssl_comp_st {
  int id;
  const char *name;
  char *method;
};

DEFINE_STACK_OF(SSL_COMP)

// The following flags do nothing and are included only to make it easier to
// compile code with BoringSSL.
#define SSL_MODE_AUTO_RETRY 0
#define SSL_MODE_RELEASE_BUFFERS 0
#define SSL_MODE_SEND_CLIENTHELLO_TIME 0
#define SSL_MODE_SEND_SERVERHELLO_TIME 0
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0
#define SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS 0
#define SSL_OP_EPHEMERAL_RSA 0
#define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER 0
#define SSL_OP_MICROSOFT_SESS_ID_BUG 0
#define SSL_OP_MSIE_SSLV2_RSA_PADDING 0
#define SSL_OP_NETSCAPE_CA_DN_BUG 0
#define SSL_OP_NETSCAPE_CHALLENGE_BUG 0
#define SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG 0
#define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG 0
#define SSL_OP_NO_COMPRESSION 0
#define SSL_OP_NO_RENEGOTIATION 0  // ssl_renegotiate_never is the default
#define SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION 0
#define SSL_OP_NO_SSLv2 0
#define SSL_OP_NO_SSLv3 0
#define SSL_OP_PKCS1_CHECK_1 0
#define SSL_OP_PKCS1_CHECK_2 0
#define SSL_OP_SINGLE_DH_USE 0
#define SSL_OP_SINGLE_ECDH_USE 0
#define SSL_OP_SSLEAY_080_CLIENT_DH_BUG 0
#define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG 0
#define SSL_OP_TLS_BLOCK_PADDING_BUG 0
#define SSL_OP_TLS_D5_BUG 0
#define SSL_OP_TLS_ROLLBACK_BUG 0
#define SSL_VERIFY_CLIENT_ONCE 0

// SSL_cache_hit calls `SSL_session_reused`.
OPENSSL_EXPORT int SSL_cache_hit(SSL *ssl);

// SSL_get_default_timeout returns `SSL_DEFAULT_SESSION_TIMEOUT`.
OPENSSL_EXPORT long SSL_get_default_timeout(const SSL *ssl);

// SSL_get_version returns a string describing the TLS version used by `ssl`.
// For example, "TLSv1.2" or "DTLSv1".
OPENSSL_EXPORT const char *SSL_get_version(const SSL *ssl);

// SSL_get_all_version_names outputs a list of possible strings
// `SSL_get_version` may return in this version of BoringSSL. It writes at most
// `max_out` entries to `out` and returns the total number it would have
// written, if `max_out` had been large enough. `max_out` may be initially set
// to zero to size the output.
//
// This function is only intended to help initialize tables in callers that want
// possible strings pre-declared. This list would not be suitable to set a list
// of supported features. It is in no particular order, and may contain
// placeholder, experimental, or deprecated values that do not apply to every
// caller. Future versions of BoringSSL may also return strings not in this
// list, so this does not apply if, say, sending strings across services.
OPENSSL_EXPORT size_t SSL_get_all_version_names(const char **out,
                                                size_t max_out);

// SSL_get_cipher_list returns the name of the `n`th cipher in the output of
// `SSL_get_ciphers` or NULL if out of range. Use `SSL_get_ciphers` instead.
OPENSSL_EXPORT const char *SSL_get_cipher_list(const SSL *ssl, int n);

// SSL_CTX_set_client_cert_cb sets a callback which is called on the client if
// the server requests a client certificate and none is configured. On success,
// the callback should return one and set `*out_x509` to `*out_pkey` to a leaf
// certificate and private key, respectively, passing ownership. It should
// return zero to send no certificate and -1 to fail or pause the handshake. If
// the handshake is paused, `SSL_get_error` will return
// `SSL_ERROR_WANT_X509_LOOKUP`.
//
// The callback may call `SSL_get0_certificate_types` and
// `SSL_get_client_CA_list` for information on the server's certificate request.
//
// Use `SSL_CTX_set_cert_cb` instead. Configuring intermediate certificates with
// this function is confusing. This callback may not be registered concurrently
// with `SSL_CTX_set_cert_cb` or `SSL_set_cert_cb`.
OPENSSL_EXPORT void SSL_CTX_set_client_cert_cb(
    SSL_CTX *ctx, int (*cb)(SSL *ssl, X509 **out_x509, EVP_PKEY **out_pkey));

#define SSL_NOTHING SSL_ERROR_NONE
#define SSL_WRITING SSL_ERROR_WANT_WRITE
#define SSL_READING SSL_ERROR_WANT_READ

// SSL_want returns one of the above values to determine what the most recent
// operation on `ssl` was blocked on. Use `SSL_get_error` instead.
OPENSSL_EXPORT int SSL_want(const SSL *ssl);

#define SSL_want_read(ssl) (SSL_want(ssl) == SSL_READING)
#define SSL_want_write(ssl) (SSL_want(ssl) == SSL_WRITING)

// SSL_get_finished writes up to `count` bytes of the Finished message sent by
// `ssl` to `buf`. It returns the total untruncated length or zero if none has
// been sent yet. At TLS 1.3 and later, it returns zero.
//
// Use `SSL_get_tls_unique` instead.
OPENSSL_EXPORT size_t SSL_get_finished(const SSL *ssl, void *buf, size_t count);

// SSL_get_peer_finished writes up to `count` bytes of the Finished message
// received from `ssl`'s peer to `buf`. It returns the total untruncated length
// or zero if none has been received yet. At TLS 1.3 and later, it returns
// zero.
//
// Use `SSL_get_tls_unique` instead.
OPENSSL_EXPORT size_t SSL_get_peer_finished(const SSL *ssl, void *buf,
                                            size_t count);

// SSL_alert_type_string returns "!". Use `SSL_alert_type_string_long`
// instead.
OPENSSL_EXPORT const char *SSL_alert_type_string(int value);

// SSL_alert_desc_string returns "!!". Use `SSL_alert_desc_string_long`
// instead.
OPENSSL_EXPORT const char *SSL_alert_desc_string(int value);

// SSL_state_string returns "!!!!!!". Use `SSL_state_string_long` for a more
// intelligible string.
OPENSSL_EXPORT const char *SSL_state_string(const SSL *ssl);

// SSL_TXT_* expand to strings.
#define SSL_TXT_MEDIUM "MEDIUM"
#define SSL_TXT_HIGH "HIGH"
#define SSL_TXT_FIPS "FIPS"
#define SSL_TXT_kRSA "kRSA"
#define SSL_TXT_kDHE "kDHE"
#define SSL_TXT_kEDH "kEDH"
#define SSL_TXT_kECDHE "kECDHE"
#define SSL_TXT_kEECDH "kEECDH"
#define SSL_TXT_kPSK "kPSK"
#define SSL_TXT_aRSA "aRSA"
#define SSL_TXT_aECDSA "aECDSA"
#define SSL_TXT_aPSK "aPSK"
#define SSL_TXT_DH "DH"
#define SSL_TXT_DHE "DHE"
#define SSL_TXT_EDH "EDH"
#define SSL_TXT_RSA "RSA"
#define SSL_TXT_ECDH "ECDH"
#define SSL_TXT_ECDHE "ECDHE"
#define SSL_TXT_EECDH "EECDH"
#define SSL_TXT_ECDSA "ECDSA"
#define SSL_TXT_PSK "PSK"
#define SSL_TXT_3DES "3DES"
#define SSL_TXT_RC4 "RC4"
#define SSL_TXT_AES128 "AES128"
#define SSL_TXT_AES256 "AES256"
#define SSL_TXT_AES "AES"
#define SSL_TXT_AES_GCM "AESGCM"
#define SSL_TXT_CHACHA20 "CHACHA20"
#define SSL_TXT_MD5 "MD5"
#define SSL_TXT_SHA1 "SHA1"
#define SSL_TXT_SHA "SHA"
#define SSL_TXT_SHA256 "SHA256"
#define SSL_TXT_SHA384 "SHA384"
#define SSL_TXT_SSLV3 "SSLv3"
#define SSL_TXT_TLSV1 "TLSv1"
#define SSL_TXT_TLSV1_1 "TLSv1.1"
#define SSL_TXT_TLSV1_2 "TLSv1.2"
#define SSL_TXT_TLSV1_3 "TLSv1.3"
#define SSL_TXT_ALL "ALL"
#define SSL_TXT_CMPDEF "COMPLEMENTOFDEFAULT"

typedef struct ssl_conf_ctx_st SSL_CONF_CTX;

// SSL_state returns `SSL_ST_INIT` if a handshake is in progress and `SSL_ST_OK`
// otherwise.
//
// Use `SSL_is_init` instead.
OPENSSL_EXPORT int SSL_state(const SSL *ssl);

#define SSL_get_state(ssl) SSL_state(ssl)

// SSL_set_shutdown causes `ssl` to behave as if the shutdown bitmask (see
// `SSL_get_shutdown`) were `mode`. This may be used to skip sending or
// receiving close_notify in `SSL_shutdown` by causing the implementation to
// believe the events already happened.
//
// It is an error to use `SSL_set_shutdown` to unset a bit that has already been
// set. Doing so will trigger an `assert` in debug builds and otherwise be
// ignored.
//
// Use `SSL_CTX_set_quiet_shutdown` instead.
OPENSSL_EXPORT void SSL_set_shutdown(SSL *ssl, int mode);

// SSL_CTX_set_tmp_ecdh calls `SSL_CTX_set1_groups` with a one-element list
// containing `ec_key`'s curve. The remainder of `ec_key` is ignored.
OPENSSL_EXPORT int SSL_CTX_set_tmp_ecdh(SSL_CTX *ctx, const EC_KEY *ec_key);

// SSL_set_tmp_ecdh calls `SSL_set1_groups` with a one-element list containing
// `ec_key`'s curve. The remainder of `ec_key` is ignored.
OPENSSL_EXPORT int SSL_set_tmp_ecdh(SSL *ssl, const EC_KEY *ec_key);

#if !defined(OPENSSL_NO_FILESYSTEM)
// SSL_add_dir_cert_subjects_to_stack lists files in directory `dir`. It calls
// `SSL_add_file_cert_subjects_to_stack` on each file and returns one on success
// or zero on error. This function is only available from the libdecrepit
// library.
OPENSSL_EXPORT int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *out,
                                                      const char *dir);
#endif

// SSL_CTX_enable_tls_channel_id calls `SSL_CTX_set_tls_channel_id_enabled`.
OPENSSL_EXPORT int SSL_CTX_enable_tls_channel_id(SSL_CTX *ctx);

// SSL_enable_tls_channel_id calls `SSL_set_tls_channel_id_enabled`.
OPENSSL_EXPORT int SSL_enable_tls_channel_id(SSL *ssl);

// BIO_f_ssl returns a `BIO_METHOD` that can wrap an `SSL*` in a `BIO*`. Note
// that this has quite different behaviour from the version in OpenSSL (notably
// that it doesn't try to auto renegotiate).
//
// IMPORTANT: if you are not curl, don't use this.
OPENSSL_EXPORT const BIO_METHOD *BIO_f_ssl(void);

// BIO_set_ssl sets `ssl` as the underlying connection for `bio`, which must
// have been created using `BIO_f_ssl`. If `take_owership` is true, `bio` will
// call `SSL_free` on `ssl` when closed. It returns one on success or something
// other than one on error.
OPENSSL_EXPORT long BIO_set_ssl(BIO *bio, SSL *ssl, int take_owership);

// SSL_CTX_set_ecdh_auto returns one.
#define SSL_CTX_set_ecdh_auto(ctx, onoff) 1

// SSL_set_ecdh_auto returns one.
#define SSL_set_ecdh_auto(ssl, onoff) 1

// SSL_get_session returns a non-owning pointer to `ssl`'s session. For
// historical reasons, which session it returns depends on `ssl`'s state.
//
// Prior to the start of the initial handshake, it returns the session the
// caller set with `SSL_set_session`. After the initial handshake has finished
// and if no additional handshakes are in progress, it returns the currently
// active session. Its behavior is undefined while a handshake is in progress.
//
// If trying to add new sessions to an external session cache, use
// `SSL_CTX_sess_set_new_cb` instead. In particular, using the callback is
// required as of TLS 1.3. For compatibility, this function will return an
// unresumable session which may be cached, but will never be resumed.
//
// If querying properties of the connection, use APIs on the `SSL` object.
OPENSSL_EXPORT SSL_SESSION *SSL_get_session(const SSL *ssl);

// SSL_get0_session is an alias for `SSL_get_session`.
#define SSL_get0_session SSL_get_session

// SSL_get1_session acts like `SSL_get_session` but returns a new reference to
// the session.
OPENSSL_EXPORT SSL_SESSION *SSL_get1_session(const SSL *ssl);

#define OPENSSL_INIT_NO_LOAD_SSL_STRINGS 0
#define OPENSSL_INIT_LOAD_SSL_STRINGS 0
#define OPENSSL_INIT_SSL_DEFAULT 0

// OPENSSL_init_ssl returns one.
OPENSSL_EXPORT int OPENSSL_init_ssl(uint64_t opts,
                                    const OPENSSL_INIT_SETTINGS *settings);

// The following constants are legacy aliases for RSA-PSS with rsaEncryption
// keys. Use the new names instead.
#define SSL_SIGN_RSA_PSS_SHA256 SSL_SIGN_RSA_PSS_RSAE_SHA256
#define SSL_SIGN_RSA_PSS_SHA384 SSL_SIGN_RSA_PSS_RSAE_SHA384
#define SSL_SIGN_RSA_PSS_SHA512 SSL_SIGN_RSA_PSS_RSAE_SHA512

// SSL_set_tlsext_status_type configures a client to request OCSP stapling if
// `type` is `TLSEXT_STATUSTYPE_ocsp` and disables it otherwise. It returns one
// on success and zero if handshake configuration has already been shed.
//
// Use `SSL_enable_ocsp_stapling` instead.
OPENSSL_EXPORT int SSL_set_tlsext_status_type(SSL *ssl, int type);

// SSL_get_tlsext_status_type returns `TLSEXT_STATUSTYPE_ocsp` if the client
// requested OCSP stapling and `TLSEXT_STATUSTYPE_nothing` otherwise. On the
// client, this reflects whether OCSP stapling was enabled via, e.g.,
// `SSL_set_tlsext_status_type`. On the server, this is determined during the
// handshake. It may be queried in callbacks set by `SSL_CTX_set_cert_cb`. The
// result is undefined after the handshake completes.
OPENSSL_EXPORT int SSL_get_tlsext_status_type(const SSL *ssl);

// SSL_set_tlsext_status_ocsp_resp sets the OCSP response. It returns one on
// success and zero on error. On success, `ssl` takes ownership of `resp`, which
// must have been allocated by `OPENSSL_malloc`.
//
// Use `SSL_set_ocsp_response` instead.
OPENSSL_EXPORT int SSL_set_tlsext_status_ocsp_resp(SSL *ssl, uint8_t *resp,
                                                   size_t resp_len);

// SSL_get_tlsext_status_ocsp_resp sets `*out` to point to the OCSP response
// from the server. It returns the length of the response. If there was no
// response, it sets `*out` to NULL and returns zero.
//
// Use `SSL_get0_ocsp_response` instead.
//
// WARNING: the returned data is not guaranteed to be well formed.
OPENSSL_EXPORT size_t SSL_get_tlsext_status_ocsp_resp(const SSL *ssl,
                                                      const uint8_t **out);

// SSL_CTX_set_tlsext_status_cb configures the legacy OpenSSL OCSP callback and
// returns one. Though the type signature is the same, this callback has
// different behavior for client and server connections:
//
// For clients, the callback is called after certificate verification. It should
// return one for success, zero for a bad OCSP response, and a negative number
// for internal error. Instead, handle this as part of certificate verification.
// (Historically, OpenSSL verified certificates just before parsing stapled OCSP
// responses, but BoringSSL fixes this ordering. All server credentials are
// available during verification.)
//
// Do not use this callback as a server. It is provided for compatibility
// purposes only. For servers, it is called to configure server credentials. It
// should return `SSL_TLSEXT_ERR_OK` on success, `SSL_TLSEXT_ERR_NOACK` to
// ignore OCSP requests, or `SSL_TLSEXT_ERR_ALERT_FATAL` on error. It is usually
// used to fetch OCSP responses on demand, which is not ideal. Instead, treat
// OCSP responses like other server credentials, such as certificates or SCT
// lists. Configure, store, and refresh them eagerly. This avoids downtime if
// the CA's OCSP responder is briefly offline.
OPENSSL_EXPORT int SSL_CTX_set_tlsext_status_cb(SSL_CTX *ctx,
                                                int (*callback)(SSL *ssl,
                                                                void *arg));

// SSL_CTX_set_tlsext_status_arg sets additional data for
// `SSL_CTX_set_tlsext_status_cb`'s callback and returns one.
OPENSSL_EXPORT int SSL_CTX_set_tlsext_status_arg(SSL_CTX *ctx, void *arg);

// The following symbols are compatibility aliases for reason codes used when
// receiving an alert from the peer. Use the other names instead, which fit the
// naming convention.
//
// TODO(davidben): Fix references to `SSL_R_TLSV1_CERTIFICATE_REQUIRED` and
// remove the compatibility value. The others come from OpenSSL.
#define SSL_R_TLSV1_UNSUPPORTED_EXTENSION \
  SSL_R_TLSV1_ALERT_UNSUPPORTED_EXTENSION
#define SSL_R_TLSV1_CERTIFICATE_UNOBTAINABLE \
  SSL_R_TLSV1_ALERT_CERTIFICATE_UNOBTAINABLE
#define SSL_R_TLSV1_UNRECOGNIZED_NAME SSL_R_TLSV1_ALERT_UNRECOGNIZED_NAME
#define SSL_R_TLSV1_BAD_CERTIFICATE_STATUS_RESPONSE \
  SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE
#define SSL_R_TLSV1_BAD_CERTIFICATE_HASH_VALUE \
  SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_HASH_VALUE
#define SSL_R_TLSV1_CERTIFICATE_REQUIRED SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED

// The following symbols are compatibility aliases for `SSL_GROUP_*`.
#define SSL_CURVE_SECP256R1 SSL_GROUP_SECP256R1
#define SSL_CURVE_SECP384R1 SSL_GROUP_SECP384R1
#define SSL_CURVE_SECP521R1 SSL_GROUP_SECP521R1
#define SSL_CURVE_X25519 SSL_GROUP_X25519

// SSL_get_curve_id calls `SSL_get_group_id`.
OPENSSL_EXPORT uint16_t SSL_get_curve_id(const SSL *ssl);

// SSL_get_curve_name calls `SSL_get_group_name`.
OPENSSL_EXPORT const char *SSL_get_curve_name(uint16_t curve_id);

// SSL_get_all_curve_names calls `SSL_get_all_group_names`.
OPENSSL_EXPORT size_t SSL_get_all_curve_names(const char **out, size_t max_out);

// SSL_CTX_set1_curves calls `SSL_CTX_set1_groups`.
OPENSSL_EXPORT int SSL_CTX_set1_curves(SSL_CTX *ctx, const int *curves,
                                       size_t num_curves);

// SSL_set1_curves calls `SSL_set1_groups`.
OPENSSL_EXPORT int SSL_set1_curves(SSL *ssl, const int *curves,
                                   size_t num_curves);

// SSL_CTX_set1_curves_list calls `SSL_CTX_set1_groups_list`.
OPENSSL_EXPORT int SSL_CTX_set1_curves_list(SSL_CTX *ctx, const char *curves);

// SSL_set1_curves_list calls `SSL_set1_groups_list`.
OPENSSL_EXPORT int SSL_set1_curves_list(SSL *ssl, const char *curves);

// TLSEXT_nid_unknown is a constant used in OpenSSL for
// `SSL_get_negotiated_group` to return an unrecognized group. BoringSSL never
// returns this value, but we define this constant for compatibility.
#define TLSEXT_nid_unknown 0x1000000

// SSL_CTX_check_private_key returns one if `ctx` has both a certificate and
// private key, and zero otherwise.
//
// This function does not check consistency because the library checks when the
// certificate and key are individually configured. However, if the private key
// is configured before the certificate, inconsistent private keys are silently
// dropped. Some callers are inadvertently relying on this function to detect
// when this happens.
//
// Instead, callers should configure the certificate first, then the private
// key, checking for errors in each. This function is then unnecessary.
OPENSSL_EXPORT int SSL_CTX_check_private_key(const SSL_CTX *ctx);

// SSL_check_private_key returns one if `ssl` has both a certificate and private
// key, and zero otherwise.
//
// See discussion in `SSL_CTX_check_private_key`.
OPENSSL_EXPORT int SSL_check_private_key(const SSL *ssl);

// SSL_CTX_get_security_level returns zero.
//
// This function is not meaningful in BoringSSL. OpenSSL has an arbitrary
// mapping from algorithms to "security levels" and offers an API to filter TLS
// configuration by those levels. In OpenSSL, this function does not return how
// secure `ctx` is, just what security level the caller previously configured.
// As BoringSSL does not implement this API, we return zero to report that the
// security levels mechanism is not used.
OPENSSL_EXPORT int SSL_CTX_get_security_level(const SSL_CTX *ctx);

// SSL_CTX_set0_buffer_pool calls `SSL_CTX_set1_buffer_pool`. Use
// `SSL_CTX_set1_buffer_pool` instead.
//
// WARNING: Despite being named set0, this function does not adopt the caller's
// reference to `pool` and instead increments its own reference like a set1
// function. Historically, `CRYPTO_BUFFER_POOL` was not reference-counted and
// this function saved a non-owning pointer, expecting the caller to maintain a
// lifetime relationship between the two objects. Now that pools are
// reference-counted, the compatible behavior is to treat it as set0 rather than
// ownership-transfering.
OPENSSL_EXPORT void SSL_CTX_set0_buffer_pool(SSL_CTX *ctx,
                                             CRYPTO_BUFFER_POOL *pool);


// Nodejs compatibility section (hidden).
//
// These defines exist for node.js, with the hope that we can eliminate the
// need for them over time.

#define SSLerr(function, reason) \
  ERR_put_error(ERR_LIB_SSL, 0, reason, __FILE__, __LINE__)


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // OPENSSL_HEADER_SSL_DEPRECATED_H
