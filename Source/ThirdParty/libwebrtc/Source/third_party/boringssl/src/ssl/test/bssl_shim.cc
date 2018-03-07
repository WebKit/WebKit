/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <openssl/base.h>

#if !defined(OPENSSL_WINDOWS)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <io.h>
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <winsock2.h>
#include <ws2tcpip.h>
OPENSSL_MSVC_PRAGMA(warning(pop))

OPENSSL_MSVC_PRAGMA(comment(lib, "Ws2_32.lib"))
#endif

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#include <openssl/aead.h>
#include <openssl/bio.h>
#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../crypto/internal.h"
#include "../internal.h"
#include "async_bio.h"
#include "fuzzer_tags.h"
#include "packeted_bio.h"
#include "test_config.h"


static CRYPTO_BUFFER_POOL *g_pool = nullptr;

#if !defined(OPENSSL_WINDOWS)
static int closesocket(int sock) {
  return close(sock);
}

static void PrintSocketError(const char *func) {
  perror(func);
}
#else
static void PrintSocketError(const char *func) {
  fprintf(stderr, "%s: %d\n", func, WSAGetLastError());
}
#endif

static int Usage(const char *program) {
  fprintf(stderr, "Usage: %s [flags...]\n", program);
  return 1;
}

struct TestState {
  // async_bio is async BIO which pauses reads and writes.
  BIO *async_bio = nullptr;
  // packeted_bio is the packeted BIO which simulates read timeouts.
  BIO *packeted_bio = nullptr;
  bssl::UniquePtr<EVP_PKEY> channel_id;
  bool cert_ready = false;
  bssl::UniquePtr<SSL_SESSION> session;
  bssl::UniquePtr<SSL_SESSION> pending_session;
  bool early_callback_called = false;
  bool handshake_done = false;
  // private_key is the underlying private key used when testing custom keys.
  bssl::UniquePtr<EVP_PKEY> private_key;
  std::vector<uint8_t> private_key_result;
  // private_key_retries is the number of times an asynchronous private key
  // operation has been retried.
  unsigned private_key_retries = 0;
  bool got_new_session = false;
  bssl::UniquePtr<SSL_SESSION> new_session;
  bool ticket_decrypt_done = false;
  bool alpn_select_done = false;
  bool is_resume = false;
  bool early_callback_ready = false;
  bool custom_verify_ready = false;
  std::string msg_callback_text;
  bool msg_callback_ok = true;
  // cert_verified is true if certificate verification has been driven to
  // completion. This tests that the callback is not called again after this.
  bool cert_verified = false;
};

static void TestStateExFree(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                            int index, long argl, void *argp) {
  delete ((TestState *)ptr);
}

static int g_config_index = 0;
static int g_state_index = 0;

static bool SetTestConfig(SSL *ssl, const TestConfig *config) {
  return SSL_set_ex_data(ssl, g_config_index, (void *)config) == 1;
}

static const TestConfig *GetTestConfig(const SSL *ssl) {
  return (const TestConfig *)SSL_get_ex_data(ssl, g_config_index);
}

static bool SetTestState(SSL *ssl, std::unique_ptr<TestState> state) {
  // |SSL_set_ex_data| takes ownership of |state| only on success.
  if (SSL_set_ex_data(ssl, g_state_index, state.get()) == 1) {
    state.release();
    return true;
  }
  return false;
}

static TestState *GetTestState(const SSL *ssl) {
  return (TestState *)SSL_get_ex_data(ssl, g_state_index);
}

static bool LoadCertificate(bssl::UniquePtr<X509> *out_x509,
                            bssl::UniquePtr<STACK_OF(X509)> *out_chain,
                            const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return false;
  }

  out_x509->reset(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  if (!*out_x509) {
    return false;
  }

  out_chain->reset(sk_X509_new_null());
  if (!*out_chain) {
    return false;
  }

  // Keep reading the certificate chain.
  for (;;) {
    bssl::UniquePtr<X509> cert(
        PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
    if (!cert) {
      break;
    }

    if (!sk_X509_push(out_chain->get(), cert.get())) {
      return false;
    }
    cert.release();  // sk_X509_push takes ownership.
  }

  uint32_t err = ERR_peek_last_error();
  if (ERR_GET_LIB(err) != ERR_LIB_PEM ||
      ERR_GET_REASON(err) != PEM_R_NO_START_LINE) {
    return false;
}

  ERR_clear_error();
  return true;
}

static bssl::UniquePtr<EVP_PKEY> LoadPrivateKey(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL));
}

static bool FromHexDigit(uint8_t *out, char c) {
  if ('0' <= c && c <= '9') {
    *out = c - '0';
    return true;
  }
  if ('a' <= c && c <= 'f') {
    *out = c - 'a' + 10;
    return true;
  }
  if ('A' <= c && c <= 'F') {
    *out = c - 'A' + 10;
    return true;
  }
  return false;
}

static bool HexDecode(std::string *out, const std::string &in) {
  if ((in.size() & 1) != 0) {
    return false;
  }

  std::unique_ptr<uint8_t[]> buf(new uint8_t[in.size() / 2]);
  for (size_t i = 0; i < in.size() / 2; i++) {
    uint8_t high, low;
    if (!FromHexDigit(&high, in[i*2]) ||
        !FromHexDigit(&low, in[i*2+1])) {
      return false;
    }
    buf[i] = (high << 4) | low;
  }

  out->assign(reinterpret_cast<const char *>(buf.get()), in.size() / 2);
  return true;
}

static std::vector<std::string> SplitParts(const std::string &in,
                                           const char delim) {
  std::vector<std::string> ret;
  size_t start = 0;

  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == delim) {
      ret.push_back(in.substr(start, i - start));
      start = i + 1;
    }
  }

  ret.push_back(in.substr(start, std::string::npos));
  return ret;
}

static std::vector<std::string> DecodeHexStrings(
    const std::string &hex_strings) {
  std::vector<std::string> ret;
  const std::vector<std::string> parts = SplitParts(hex_strings, ',');

  for (const auto &part : parts) {
    std::string binary;
    if (!HexDecode(&binary, part)) {
      fprintf(stderr, "Bad hex string: %s\n", part.c_str());
      return ret;
    }

    ret.push_back(binary);
  }

  return ret;
}

static bssl::UniquePtr<STACK_OF(X509_NAME)> DecodeHexX509Names(
    const std::string &hex_names) {
  const std::vector<std::string> der_names = DecodeHexStrings(hex_names);
  bssl::UniquePtr<STACK_OF(X509_NAME)> ret(sk_X509_NAME_new_null());
  if (!ret) {
    return nullptr;
  }

  for (const auto &der_name : der_names) {
    const uint8_t *const data =
        reinterpret_cast<const uint8_t *>(der_name.data());
    const uint8_t *derp = data;
    bssl::UniquePtr<X509_NAME> name(
        d2i_X509_NAME(nullptr, &derp, der_name.size()));
    if (!name || derp != data + der_name.size()) {
      fprintf(stderr, "Failed to parse X509_NAME.\n");
      return nullptr;
    }

    if (!sk_X509_NAME_push(ret.get(), name.get())) {
      return nullptr;
    }
    name.release();
  }

  return ret;
}

static ssl_private_key_result_t AsyncPrivateKeySign(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
    uint16_t signature_algorithm, const uint8_t *in, size_t in_len) {
  TestState *test_state = GetTestState(ssl);
  if (!test_state->private_key_result.empty()) {
    fprintf(stderr, "AsyncPrivateKeySign called with operation pending.\n");
    abort();
  }

  // Determine the hash.
  const EVP_MD *md;
  switch (signature_algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
    case SSL_SIGN_ECDSA_SHA1:
      md = EVP_sha1();
      break;
    case SSL_SIGN_RSA_PKCS1_SHA256:
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
    case SSL_SIGN_RSA_PSS_SHA256:
      md = EVP_sha256();
      break;
    case SSL_SIGN_RSA_PKCS1_SHA384:
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
    case SSL_SIGN_RSA_PSS_SHA384:
      md = EVP_sha384();
      break;
    case SSL_SIGN_RSA_PKCS1_SHA512:
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
    case SSL_SIGN_RSA_PSS_SHA512:
      md = EVP_sha512();
      break;
    case SSL_SIGN_RSA_PKCS1_MD5_SHA1:
      md = EVP_md5_sha1();
      break;
    case SSL_SIGN_ED25519:
      md = nullptr;
      break;
    default:
      fprintf(stderr, "Unknown signature algorithm %04x.\n",
              signature_algorithm);
      return ssl_private_key_failure;
  }

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX *pctx;
  if (!EVP_DigestSignInit(ctx.get(), &pctx, md, nullptr,
                          test_state->private_key.get())) {
    return ssl_private_key_failure;
  }

  // Configure additional signature parameters.
  switch (signature_algorithm) {
    case SSL_SIGN_RSA_PSS_SHA256:
    case SSL_SIGN_RSA_PSS_SHA384:
    case SSL_SIGN_RSA_PSS_SHA512:
      if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
          !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx,
                                            -1 /* salt len = hash len */)) {
        return ssl_private_key_failure;
      }
  }

  // Write the signature into |test_state|.
  size_t len = 0;
  if (!EVP_DigestSign(ctx.get(), nullptr, &len, in, in_len)) {
    return ssl_private_key_failure;
  }
  test_state->private_key_result.resize(len);
  if (!EVP_DigestSign(ctx.get(), test_state->private_key_result.data(), &len,
                      in, in_len)) {
    return ssl_private_key_failure;
  }
  test_state->private_key_result.resize(len);

  // The signature will be released asynchronously in |AsyncPrivateKeyComplete|.
  return ssl_private_key_retry;
}

static ssl_private_key_result_t AsyncPrivateKeyDecrypt(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
    const uint8_t *in, size_t in_len) {
  TestState *test_state = GetTestState(ssl);
  if (!test_state->private_key_result.empty()) {
    fprintf(stderr,
            "AsyncPrivateKeyDecrypt called with operation pending.\n");
    abort();
  }

  RSA *rsa = EVP_PKEY_get0_RSA(test_state->private_key.get());
  if (rsa == NULL) {
    fprintf(stderr,
            "AsyncPrivateKeyDecrypt called with incorrect key type.\n");
    abort();
  }
  test_state->private_key_result.resize(RSA_size(rsa));
  if (!RSA_decrypt(rsa, out_len, test_state->private_key_result.data(),
                   RSA_size(rsa), in, in_len, RSA_NO_PADDING)) {
    return ssl_private_key_failure;
  }

  test_state->private_key_result.resize(*out_len);

  // The decryption will be released asynchronously in |AsyncPrivateComplete|.
  return ssl_private_key_retry;
}

static ssl_private_key_result_t AsyncPrivateKeyComplete(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out) {
  TestState *test_state = GetTestState(ssl);
  if (test_state->private_key_result.empty()) {
    fprintf(stderr,
            "AsyncPrivateKeyComplete called without operation pending.\n");
    abort();
  }

  if (test_state->private_key_retries < 2) {
    // Only return the decryption on the second attempt, to test both incomplete
    // |decrypt| and |decrypt_complete|.
    return ssl_private_key_retry;
  }

  if (max_out < test_state->private_key_result.size()) {
    fprintf(stderr, "Output buffer too small.\n");
    return ssl_private_key_failure;
  }
  OPENSSL_memcpy(out, test_state->private_key_result.data(),
                 test_state->private_key_result.size());
  *out_len = test_state->private_key_result.size();

  test_state->private_key_result.clear();
  test_state->private_key_retries = 0;
  return ssl_private_key_success;
}

static const SSL_PRIVATE_KEY_METHOD g_async_private_key_method = {
    AsyncPrivateKeySign,
    AsyncPrivateKeyDecrypt,
    AsyncPrivateKeyComplete,
};

template<typename T>
struct Free {
  void operator()(T *buf) {
    free(buf);
  }
};

static bool GetCertificate(SSL *ssl, bssl::UniquePtr<X509> *out_x509,
                           bssl::UniquePtr<STACK_OF(X509)> *out_chain,
                           bssl::UniquePtr<EVP_PKEY> *out_pkey) {
  const TestConfig *config = GetTestConfig(ssl);

  if (!config->signing_prefs.empty()) {
    std::vector<uint16_t> u16s(config->signing_prefs.begin(),
                               config->signing_prefs.end());
    if (!SSL_set_signing_algorithm_prefs(ssl, u16s.data(), u16s.size())) {
      return false;
    }
  }

  if (!config->key_file.empty()) {
    *out_pkey = LoadPrivateKey(config->key_file.c_str());
    if (!*out_pkey) {
      return false;
    }
  }
  if (!config->cert_file.empty() &&
      !LoadCertificate(out_x509, out_chain, config->cert_file.c_str())) {
    return false;
  }
  if (!config->ocsp_response.empty() &&
      !SSL_set_ocsp_response(ssl, (const uint8_t *)config->ocsp_response.data(),
                             config->ocsp_response.size())) {
    return false;
  }
  return true;
}

static bool InstallCertificate(SSL *ssl) {
  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<STACK_OF(X509)> chain;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &chain, &pkey)) {
    return false;
  }

  if (pkey) {
    TestState *test_state = GetTestState(ssl);
    const TestConfig *config = GetTestConfig(ssl);
    if (config->async) {
      test_state->private_key = std::move(pkey);
      SSL_set_private_key_method(ssl, &g_async_private_key_method);
    } else if (!SSL_use_PrivateKey(ssl, pkey.get())) {
      return false;
    }
  }

  if (x509 && !SSL_use_certificate(ssl, x509.get())) {
    return false;
  }

  if (sk_X509_num(chain.get()) > 0 &&
      !SSL_set1_chain(ssl, chain.get())) {
    return false;
  }

  return true;
}

static enum ssl_select_cert_result_t SelectCertificateCallback(
    const SSL_CLIENT_HELLO *client_hello) {
  const TestConfig *config = GetTestConfig(client_hello->ssl);
  GetTestState(client_hello->ssl)->early_callback_called = true;

  if (!config->expected_server_name.empty()) {
    const uint8_t *extension_data;
    size_t extension_len;
    CBS extension, server_name_list, host_name;
    uint8_t name_type;

    if (!SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_server_name, &extension_data,
            &extension_len)) {
      fprintf(stderr, "Could not find server_name extension.\n");
      return ssl_select_cert_error;
    }

    CBS_init(&extension, extension_data, extension_len);
    if (!CBS_get_u16_length_prefixed(&extension, &server_name_list) ||
        CBS_len(&extension) != 0 ||
        !CBS_get_u8(&server_name_list, &name_type) ||
        name_type != TLSEXT_NAMETYPE_host_name ||
        !CBS_get_u16_length_prefixed(&server_name_list, &host_name) ||
        CBS_len(&server_name_list) != 0) {
      fprintf(stderr, "Could not decode server_name extension.\n");
      return ssl_select_cert_error;
    }

    if (!CBS_mem_equal(&host_name,
                       (const uint8_t*)config->expected_server_name.data(),
                       config->expected_server_name.size())) {
      fprintf(stderr, "Server name mismatch.\n");
    }
  }

  if (config->fail_early_callback) {
    return ssl_select_cert_error;
  }

  // Install the certificate in the early callback.
  if (config->use_early_callback) {
    bool early_callback_ready =
        GetTestState(client_hello->ssl)->early_callback_ready;
    if (config->async && !early_callback_ready) {
      // Install the certificate asynchronously.
      return ssl_select_cert_retry;
    }
    if (!InstallCertificate(client_hello->ssl)) {
      return ssl_select_cert_error;
    }
  }
  return ssl_select_cert_success;
}

static bool CheckCertificateRequest(SSL *ssl) {
  const TestConfig *config = GetTestConfig(ssl);

  if (!config->expected_certificate_types.empty()) {
    const uint8_t *certificate_types;
    size_t certificate_types_len =
        SSL_get0_certificate_types(ssl, &certificate_types);
    if (certificate_types_len != config->expected_certificate_types.size() ||
        OPENSSL_memcmp(certificate_types,
                       config->expected_certificate_types.data(),
                       certificate_types_len) != 0) {
      fprintf(stderr, "certificate types mismatch\n");
      return false;
    }
  }

  if (!config->expected_client_ca_list.empty()) {
    bssl::UniquePtr<STACK_OF(X509_NAME)> expected =
        DecodeHexX509Names(config->expected_client_ca_list);
    const size_t num_expected = sk_X509_NAME_num(expected.get());

    const STACK_OF(X509_NAME) *received = SSL_get_client_CA_list(ssl);
    const size_t num_received = sk_X509_NAME_num(received);

    if (num_received != num_expected) {
      fprintf(stderr, "expected %u names in CertificateRequest but got %u\n",
              static_cast<unsigned>(num_expected),
              static_cast<unsigned>(num_received));
      return false;
    }

    for (size_t i = 0; i < num_received; i++) {
      if (X509_NAME_cmp(sk_X509_NAME_value(received, i),
                        sk_X509_NAME_value(expected.get(), i)) != 0) {
        fprintf(stderr, "names in CertificateRequest differ at index #%d\n",
                static_cast<unsigned>(i));
        return false;
      }
    }

    STACK_OF(CRYPTO_BUFFER) *buffers = SSL_get0_server_requested_CAs(ssl);
    if (sk_CRYPTO_BUFFER_num(buffers) != num_received) {
      fprintf(stderr,
              "Mismatch between SSL_get_server_requested_CAs and "
              "SSL_get_client_CA_list.\n");
      return false;
    }
  }

  return true;
}

static int ClientCertCallback(SSL *ssl, X509 **out_x509, EVP_PKEY **out_pkey) {
  if (!CheckCertificateRequest(ssl)) {
    return -1;
  }

  if (GetTestConfig(ssl)->async && !GetTestState(ssl)->cert_ready) {
    return -1;
  }

  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<STACK_OF(X509)> chain;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &chain, &pkey)) {
    return -1;
  }

  // Return zero for no certificate.
  if (!x509) {
    return 0;
  }

  // Chains and asynchronous private keys are not supported with client_cert_cb.
  *out_x509 = x509.release();
  *out_pkey = pkey.release();
  return 1;
}

static int CertCallback(SSL *ssl, void *arg) {
  const TestConfig *config = GetTestConfig(ssl);

  // Check the CertificateRequest metadata is as expected.
  if (!SSL_is_server(ssl) && !CheckCertificateRequest(ssl)) {
    return -1;
  }

  if (config->fail_cert_callback) {
    return 0;
  }

  // The certificate will be installed via other means.
  if (!config->async || config->use_early_callback) {
    return 1;
  }

  if (!GetTestState(ssl)->cert_ready) {
    return -1;
  }
  if (!InstallCertificate(ssl)) {
    return 0;
  }
  return 1;
}

static bool CheckVerifyCallback(SSL *ssl) {
  const TestConfig *config = GetTestConfig(ssl);
  if (!config->expected_ocsp_response.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_ocsp_response(ssl, &data, &len);
    if (len == 0) {
      fprintf(stderr, "OCSP response not available in verify callback\n");
      return false;
    }
  }

  if (GetTestState(ssl)->cert_verified) {
    fprintf(stderr, "Certificate verified twice.\n");
    return false;
  }

  return true;
}

static int CertVerifyCallback(X509_STORE_CTX *store_ctx, void *arg) {
  SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(store_ctx,
      SSL_get_ex_data_X509_STORE_CTX_idx());
  const TestConfig *config = GetTestConfig(ssl);
  if (!CheckVerifyCallback(ssl)) {
    return 0;
  }

  GetTestState(ssl)->cert_verified = true;
  if (config->verify_fail) {
    store_ctx->error = X509_V_ERR_APPLICATION_VERIFICATION;
    return 0;
  }

  return 1;
}

static ssl_verify_result_t CustomVerifyCallback(SSL *ssl, uint8_t *out_alert) {
  const TestConfig *config = GetTestConfig(ssl);
  if (!CheckVerifyCallback(ssl)) {
    return ssl_verify_invalid;
  }

  if (config->async && !GetTestState(ssl)->custom_verify_ready) {
    return ssl_verify_retry;
  }

  GetTestState(ssl)->cert_verified = true;
  if (config->verify_fail) {
    return ssl_verify_invalid;
  }

  return ssl_verify_ok;
}

static int NextProtosAdvertisedCallback(SSL *ssl, const uint8_t **out,
                                        unsigned int *out_len, void *arg) {
  const TestConfig *config = GetTestConfig(ssl);
  if (config->advertise_npn.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  *out = (const uint8_t*)config->advertise_npn.data();
  *out_len = config->advertise_npn.size();
  return SSL_TLSEXT_ERR_OK;
}

static int NextProtoSelectCallback(SSL* ssl, uint8_t** out, uint8_t* outlen,
                                   const uint8_t* in, unsigned inlen, void* arg) {
  const TestConfig *config = GetTestConfig(ssl);
  if (config->select_next_proto.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  *out = (uint8_t*)config->select_next_proto.data();
  *outlen = config->select_next_proto.size();
  return SSL_TLSEXT_ERR_OK;
}

static int AlpnSelectCallback(SSL* ssl, const uint8_t** out, uint8_t* outlen,
                              const uint8_t* in, unsigned inlen, void* arg) {
  if (GetTestState(ssl)->alpn_select_done) {
    fprintf(stderr, "AlpnSelectCallback called after completion.\n");
    exit(1);
  }

  GetTestState(ssl)->alpn_select_done = true;

  const TestConfig *config = GetTestConfig(ssl);
  if (config->decline_alpn) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  if (!config->expected_advertised_alpn.empty() &&
      (config->expected_advertised_alpn.size() != inlen ||
       OPENSSL_memcmp(config->expected_advertised_alpn.data(), in, inlen) !=
           0)) {
    fprintf(stderr, "bad ALPN select callback inputs\n");
    exit(1);
  }

  *out = (const uint8_t*)config->select_alpn.data();
  *outlen = config->select_alpn.size();
  return SSL_TLSEXT_ERR_OK;
}

static unsigned PskClientCallback(SSL *ssl, const char *hint,
                                  char *out_identity,
                                  unsigned max_identity_len,
                                  uint8_t *out_psk, unsigned max_psk_len) {
  const TestConfig *config = GetTestConfig(ssl);

  if (config->psk_identity.empty()) {
    if (hint != nullptr) {
      fprintf(stderr, "Server PSK hint was non-null.\n");
      return 0;
    }
  } else if (hint == nullptr ||
             strcmp(hint, config->psk_identity.c_str()) != 0) {
    fprintf(stderr, "Server PSK hint did not match.\n");
    return 0;
  }

  // Account for the trailing '\0' for the identity.
  if (config->psk_identity.size() >= max_identity_len ||
      config->psk.size() > max_psk_len) {
    fprintf(stderr, "PSK buffers too small\n");
    return 0;
  }

  BUF_strlcpy(out_identity, config->psk_identity.c_str(),
              max_identity_len);
  OPENSSL_memcpy(out_psk, config->psk.data(), config->psk.size());
  return config->psk.size();
}

static unsigned PskServerCallback(SSL *ssl, const char *identity,
                                  uint8_t *out_psk, unsigned max_psk_len) {
  const TestConfig *config = GetTestConfig(ssl);

  if (strcmp(identity, config->psk_identity.c_str()) != 0) {
    fprintf(stderr, "Client PSK identity did not match.\n");
    return 0;
  }

  if (config->psk.size() > max_psk_len) {
    fprintf(stderr, "PSK buffers too small\n");
    return 0;
  }

  OPENSSL_memcpy(out_psk, config->psk.data(), config->psk.size());
  return config->psk.size();
}

static timeval g_clock;

static void CurrentTimeCallback(const SSL *ssl, timeval *out_clock) {
  *out_clock = g_clock;
}

static void ChannelIdCallback(SSL *ssl, EVP_PKEY **out_pkey) {
  *out_pkey = GetTestState(ssl)->channel_id.release();
}

static SSL_SESSION *GetSessionCallback(SSL *ssl, const uint8_t *data, int len,
                                       int *copy) {
  TestState *async_state = GetTestState(ssl);
  if (async_state->session) {
    *copy = 0;
    return async_state->session.release();
  } else if (async_state->pending_session) {
    return SSL_magic_pending_session_ptr();
  } else {
    return NULL;
  }
}

static int DDoSCallback(const SSL_CLIENT_HELLO *client_hello) {
  const TestConfig *config = GetTestConfig(client_hello->ssl);
  static int callback_num = 0;

  callback_num++;
  if (config->fail_ddos_callback ||
      (config->fail_second_ddos_callback && callback_num == 2)) {
    return 0;
  }
  return 1;
}

static void InfoCallback(const SSL *ssl, int type, int val) {
  if (type == SSL_CB_HANDSHAKE_DONE) {
    if (GetTestConfig(ssl)->handshake_never_done) {
      fprintf(stderr, "Handshake unexpectedly completed.\n");
      // Abort before any expected error code is printed, to ensure the overall
      // test fails.
      abort();
    }
    // This callback is called when the handshake completes. |SSL_get_session|
    // must continue to work and |SSL_in_init| must return false.
    if (SSL_in_init(ssl) || SSL_get_session(ssl) == nullptr) {
      fprintf(stderr, "Invalid state for SSL_CB_HANDSHAKE_DONE.\n");
      abort();
    }
    GetTestState(ssl)->handshake_done = true;

    // Callbacks may be called again on a new handshake.
    GetTestState(ssl)->ticket_decrypt_done = false;
    GetTestState(ssl)->alpn_select_done = false;
  }
}

static int NewSessionCallback(SSL *ssl, SSL_SESSION *session) {
  // This callback is called as the handshake completes. |SSL_get_session|
  // must continue to work and, historically, |SSL_in_init| returned false at
  // this point.
  if (SSL_in_init(ssl) || SSL_get_session(ssl) == nullptr) {
    fprintf(stderr, "Invalid state for NewSessionCallback.\n");
    abort();
  }

  GetTestState(ssl)->got_new_session = true;
  GetTestState(ssl)->new_session.reset(session);
  return 1;
}

static int TicketKeyCallback(SSL *ssl, uint8_t *key_name, uint8_t *iv,
                             EVP_CIPHER_CTX *ctx, HMAC_CTX *hmac_ctx,
                             int encrypt) {
  if (!encrypt) {
    if (GetTestState(ssl)->ticket_decrypt_done) {
      fprintf(stderr, "TicketKeyCallback called after completion.\n");
      return -1;
    }

    GetTestState(ssl)->ticket_decrypt_done = true;
  }

  // This is just test code, so use the all-zeros key.
  static const uint8_t kZeros[16] = {0};

  if (encrypt) {
    OPENSSL_memcpy(key_name, kZeros, sizeof(kZeros));
    RAND_bytes(iv, 16);
  } else if (OPENSSL_memcmp(key_name, kZeros, 16) != 0) {
    return 0;
  }

  if (!HMAC_Init_ex(hmac_ctx, kZeros, sizeof(kZeros), EVP_sha256(), NULL) ||
      !EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL, kZeros, iv, encrypt)) {
    return -1;
  }

  if (!encrypt) {
    return GetTestConfig(ssl)->renew_ticket ? 2 : 1;
  }
  return 1;
}

// kCustomExtensionValue is the extension value that the custom extension
// callbacks will add.
static const uint16_t kCustomExtensionValue = 1234;
static void *const kCustomExtensionAddArg =
    reinterpret_cast<void *>(kCustomExtensionValue);
static void *const kCustomExtensionParseArg =
    reinterpret_cast<void *>(kCustomExtensionValue + 1);
static const char kCustomExtensionContents[] = "custom extension";

static int CustomExtensionAddCallback(SSL *ssl, unsigned extension_value,
                                      const uint8_t **out, size_t *out_len,
                                      int *out_alert_value, void *add_arg) {
  if (extension_value != kCustomExtensionValue ||
      add_arg != kCustomExtensionAddArg) {
    abort();
  }

  if (GetTestConfig(ssl)->custom_extension_skip) {
    return 0;
  }
  if (GetTestConfig(ssl)->custom_extension_fail_add) {
    return -1;
  }

  *out = reinterpret_cast<const uint8_t*>(kCustomExtensionContents);
  *out_len = sizeof(kCustomExtensionContents) - 1;

  return 1;
}

static void CustomExtensionFreeCallback(SSL *ssl, unsigned extension_value,
                                        const uint8_t *out, void *add_arg) {
  if (extension_value != kCustomExtensionValue ||
      add_arg != kCustomExtensionAddArg ||
      out != reinterpret_cast<const uint8_t *>(kCustomExtensionContents)) {
    abort();
  }
}

static int CustomExtensionParseCallback(SSL *ssl, unsigned extension_value,
                                        const uint8_t *contents,
                                        size_t contents_len,
                                        int *out_alert_value, void *parse_arg) {
  if (extension_value != kCustomExtensionValue ||
      parse_arg != kCustomExtensionParseArg) {
    abort();
  }

  if (contents_len != sizeof(kCustomExtensionContents) - 1 ||
      OPENSSL_memcmp(contents, kCustomExtensionContents, contents_len) != 0) {
    *out_alert_value = SSL_AD_DECODE_ERROR;
    return 0;
  }

  return 1;
}

static int ServerNameCallback(SSL *ssl, int *out_alert, void *arg) {
  // SNI must be accessible from the SNI callback.
  const TestConfig *config = GetTestConfig(ssl);
  const char *server_name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (server_name == nullptr ||
      std::string(server_name) != config->expected_server_name) {
    fprintf(stderr, "servername mismatch (got %s; want %s)\n", server_name,
            config->expected_server_name.c_str());
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  return SSL_TLSEXT_ERR_OK;
}

static void MessageCallback(int is_write, int version, int content_type,
                            const void *buf, size_t len, SSL *ssl, void *arg) {
  const uint8_t *buf_u8 = reinterpret_cast<const uint8_t *>(buf);
  const TestConfig *config = GetTestConfig(ssl);
  TestState *state = GetTestState(ssl);
  if (!state->msg_callback_ok) {
    return;
  }

  if (content_type == SSL3_RT_HEADER) {
    if (len !=
        (config->is_dtls ? DTLS1_RT_HEADER_LENGTH : SSL3_RT_HEADER_LENGTH)) {
      fprintf(stderr, "Incorrect length for record header: %zu\n", len);
      state->msg_callback_ok = false;
    }
    return;
  }

  state->msg_callback_text += is_write ? "write " : "read ";
  switch (content_type) {
    case 0:
      if (version != SSL2_VERSION) {
        fprintf(stderr, "Incorrect version for V2ClientHello: %x\n", version);
        state->msg_callback_ok = false;
        return;
      }
      state->msg_callback_text += "v2clienthello\n";
      return;

    case SSL3_RT_HANDSHAKE: {
      CBS cbs;
      CBS_init(&cbs, buf_u8, len);
      uint8_t type;
      uint32_t msg_len;
      if (!CBS_get_u8(&cbs, &type) ||
          // TODO(davidben): Reporting on entire messages would be more
          // consistent than fragments.
          (config->is_dtls &&
           !CBS_skip(&cbs, 3 /* total */ + 2 /* seq */ + 3 /* frag_off */)) ||
          !CBS_get_u24(&cbs, &msg_len) ||
          !CBS_skip(&cbs, msg_len) ||
          CBS_len(&cbs) != 0) {
        fprintf(stderr, "Could not parse handshake message.\n");
        state->msg_callback_ok = false;
        return;
      }
      char text[16];
      snprintf(text, sizeof(text), "hs %d\n", type);
      state->msg_callback_text += text;
      return;
    }

    case SSL3_RT_CHANGE_CIPHER_SPEC:
      if (len != 1 || buf_u8[0] != 1) {
        fprintf(stderr, "Invalid ChangeCipherSpec.\n");
        state->msg_callback_ok = false;
        return;
      }
      state->msg_callback_text += "ccs\n";
      return;

    case SSL3_RT_ALERT:
      if (len != 2) {
        fprintf(stderr, "Invalid alert.\n");
        state->msg_callback_ok = false;
        return;
      }
      char text[16];
      snprintf(text, sizeof(text), "alert %d %d\n", buf_u8[0], buf_u8[1]);
      state->msg_callback_text += text;
      return;

    default:
      fprintf(stderr, "Invalid content_type: %d\n", content_type);
      state->msg_callback_ok = false;
  }
}

// Connect returns a new socket connected to localhost on |port| or -1 on
// error.
static int Connect(uint16_t port) {
  for (int af : { AF_INET6, AF_INET }) {
    int sock = socket(af, SOCK_STREAM, 0);
    if (sock == -1) {
      PrintSocketError("socket");
      return -1;
    }
    int nodelay = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char*>(&nodelay), sizeof(nodelay)) != 0) {
      PrintSocketError("setsockopt");
      closesocket(sock);
      return -1;
    }

    sockaddr_storage ss;
    OPENSSL_memset(&ss, 0, sizeof(ss));
    ss.ss_family = af;
    socklen_t len = 0;

    if (af == AF_INET6) {
      sockaddr_in6 *sin6 = (sockaddr_in6 *) &ss;
      len = sizeof(*sin6);
      sin6->sin6_port = htons(port);
      if (!inet_pton(AF_INET6, "::1", &sin6->sin6_addr)) {
        PrintSocketError("inet_pton");
        closesocket(sock);
        return -1;
      }
    } else if (af == AF_INET) {
      sockaddr_in *sin = (sockaddr_in *) &ss;
      len = sizeof(*sin);
      sin->sin_port = htons(port);
      if (!inet_pton(AF_INET, "127.0.0.1", &sin->sin_addr)) {
        PrintSocketError("inet_pton");
        closesocket(sock);
        return -1;
      }
    }

    if (connect(sock, reinterpret_cast<const sockaddr*>(&ss), len) == 0) {
      return sock;
    }
    closesocket(sock);
  }

  PrintSocketError("connect");
  return -1;
}

class SocketCloser {
 public:
  explicit SocketCloser(int sock) : sock_(sock) {}
  ~SocketCloser() {
    // Half-close and drain the socket before releasing it. This seems to be
    // necessary for graceful shutdown on Windows. It will also avoid write
    // failures in the test runner.
#if defined(OPENSSL_WINDOWS)
    shutdown(sock_, SD_SEND);
#else
    shutdown(sock_, SHUT_WR);
#endif
    while (true) {
      char buf[1024];
      if (recv(sock_, buf, sizeof(buf), 0) <= 0) {
        break;
      }
    }
    closesocket(sock_);
  }

 private:
  const int sock_;
};

static void ssl_ctx_add_session(SSL_SESSION *session, void *void_param) {
  SSL_CTX *ctx = reinterpret_cast<SSL_CTX *>(void_param);
  bssl::UniquePtr<SSL_SESSION> new_session = bssl::SSL_SESSION_dup(
      session, SSL_SESSION_INCLUDE_NONAUTH | SSL_SESSION_INCLUDE_TICKET);
  if (new_session != nullptr) {
    SSL_CTX_add_session(ctx, new_session.get());
  }
}

static bssl::UniquePtr<SSL_CTX> SetupCtx(SSL_CTX *old_ctx,
                                         const TestConfig *config) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(
      config->is_dtls ? DTLS_method() : TLS_method()));
  if (!ssl_ctx) {
    return nullptr;
  }

  SSL_CTX_set0_buffer_pool(ssl_ctx.get(), g_pool);

  // Enable SSL 3.0 and TLS 1.3 for tests.
  if (!config->is_dtls &&
      (!SSL_CTX_set_min_proto_version(ssl_ctx.get(), SSL3_VERSION) ||
       !SSL_CTX_set_max_proto_version(ssl_ctx.get(), TLS1_3_VERSION))) {
    return nullptr;
  }

  std::string cipher_list = "ALL";
  if (!config->cipher.empty()) {
    cipher_list = config->cipher;
    SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
  }
  if (!SSL_CTX_set_strict_cipher_list(ssl_ctx.get(), cipher_list.c_str())) {
    return nullptr;
  }

  if (config->async && config->is_server) {
    // Disable the internal session cache. To test asynchronous session lookup,
    // we use an external session cache.
    SSL_CTX_set_session_cache_mode(
        ssl_ctx.get(), SSL_SESS_CACHE_BOTH | SSL_SESS_CACHE_NO_INTERNAL);
    SSL_CTX_sess_set_get_cb(ssl_ctx.get(), GetSessionCallback);
  } else {
    SSL_CTX_set_session_cache_mode(ssl_ctx.get(), SSL_SESS_CACHE_BOTH);
  }

  SSL_CTX_set_select_certificate_cb(ssl_ctx.get(), SelectCertificateCallback);

  if (config->use_old_client_cert_callback) {
    SSL_CTX_set_client_cert_cb(ssl_ctx.get(), ClientCertCallback);
  }

  SSL_CTX_set_next_protos_advertised_cb(
      ssl_ctx.get(), NextProtosAdvertisedCallback, NULL);
  if (!config->select_next_proto.empty()) {
    SSL_CTX_set_next_proto_select_cb(ssl_ctx.get(), NextProtoSelectCallback,
                                     NULL);
  }

  if (!config->select_alpn.empty() || config->decline_alpn) {
    SSL_CTX_set_alpn_select_cb(ssl_ctx.get(), AlpnSelectCallback, NULL);
  }

  SSL_CTX_set_channel_id_cb(ssl_ctx.get(), ChannelIdCallback);

  SSL_CTX_set_current_time_cb(ssl_ctx.get(), CurrentTimeCallback);

  SSL_CTX_set_info_callback(ssl_ctx.get(), InfoCallback);
  SSL_CTX_sess_set_new_cb(ssl_ctx.get(), NewSessionCallback);

  if (config->use_ticket_callback) {
    SSL_CTX_set_tlsext_ticket_key_cb(ssl_ctx.get(), TicketKeyCallback);
  }

  if (config->enable_client_custom_extension &&
      !SSL_CTX_add_client_custom_ext(
          ssl_ctx.get(), kCustomExtensionValue, CustomExtensionAddCallback,
          CustomExtensionFreeCallback, kCustomExtensionAddArg,
          CustomExtensionParseCallback, kCustomExtensionParseArg)) {
    return nullptr;
  }

  if (config->enable_server_custom_extension &&
      !SSL_CTX_add_server_custom_ext(
          ssl_ctx.get(), kCustomExtensionValue, CustomExtensionAddCallback,
          CustomExtensionFreeCallback, kCustomExtensionAddArg,
          CustomExtensionParseCallback, kCustomExtensionParseArg)) {
    return nullptr;
  }

  if (!config->use_custom_verify_callback) {
    SSL_CTX_set_cert_verify_callback(ssl_ctx.get(), CertVerifyCallback, NULL);
  }

  if (!config->signed_cert_timestamps.empty() &&
      !SSL_CTX_set_signed_cert_timestamp_list(
          ssl_ctx.get(), (const uint8_t *)config->signed_cert_timestamps.data(),
          config->signed_cert_timestamps.size())) {
    return nullptr;
  }

  if (!config->use_client_ca_list.empty()) {
    if (config->use_client_ca_list == "<NULL>") {
      SSL_CTX_set_client_CA_list(ssl_ctx.get(), nullptr);
    } else if (config->use_client_ca_list == "<EMPTY>") {
      bssl::UniquePtr<STACK_OF(X509_NAME)> names;
      SSL_CTX_set_client_CA_list(ssl_ctx.get(), names.release());
    } else {
      bssl::UniquePtr<STACK_OF(X509_NAME)> names =
          DecodeHexX509Names(config->use_client_ca_list);
      SSL_CTX_set_client_CA_list(ssl_ctx.get(), names.release());
    }
  }

  if (config->enable_grease) {
    SSL_CTX_set_grease_enabled(ssl_ctx.get(), 1);
  }

  if (!config->expected_server_name.empty()) {
    SSL_CTX_set_tlsext_servername_callback(ssl_ctx.get(), ServerNameCallback);
  }

  if (!config->ticket_key.empty() &&
      !SSL_CTX_set_tlsext_ticket_keys(ssl_ctx.get(), config->ticket_key.data(),
                                      config->ticket_key.size())) {
    return nullptr;
  }

  if (config->enable_early_data) {
    SSL_CTX_set_early_data_enabled(ssl_ctx.get(), 1);
  }

  SSL_CTX_set_tls13_variant(
      ssl_ctx.get(), static_cast<enum tls13_variant_t>(config->tls13_variant));

  if (config->allow_unknown_alpn_protos) {
    SSL_CTX_set_allow_unknown_alpn_protos(ssl_ctx.get(), 1);
  }

  if (config->enable_ed25519) {
    SSL_CTX_set_ed25519_enabled(ssl_ctx.get(), 1);
  }

  if (!config->verify_prefs.empty()) {
    std::vector<uint16_t> u16s(config->verify_prefs.begin(),
                               config->verify_prefs.end());
    if (!SSL_CTX_set_verify_algorithm_prefs(ssl_ctx.get(), u16s.data(),
                                            u16s.size())) {
      return nullptr;
    }
  }

  SSL_CTX_set_msg_callback(ssl_ctx.get(), MessageCallback);

  if (old_ctx) {
    uint8_t keys[48];
    if (!SSL_CTX_get_tlsext_ticket_keys(old_ctx, &keys, sizeof(keys)) ||
        !SSL_CTX_set_tlsext_ticket_keys(ssl_ctx.get(), keys, sizeof(keys))) {
      return nullptr;
    }
    lh_SSL_SESSION_doall_arg(old_ctx->sessions, ssl_ctx_add_session,
                             ssl_ctx.get());
  }

  return ssl_ctx;
}

// RetryAsync is called after a failed operation on |ssl| with return code
// |ret|. If the operation should be retried, it simulates one asynchronous
// event and returns true. Otherwise it returns false.
static bool RetryAsync(SSL *ssl, int ret) {
  // No error; don't retry.
  if (ret >= 0) {
    return false;
  }

  TestState *test_state = GetTestState(ssl);
  assert(GetTestConfig(ssl)->async);

  if (test_state->packeted_bio != nullptr &&
      PacketedBioAdvanceClock(test_state->packeted_bio)) {
    // The DTLS retransmit logic silently ignores write failures. So the test
    // may progress, allow writes through synchronously.
    AsyncBioEnforceWriteQuota(test_state->async_bio, false);
    int timeout_ret = DTLSv1_handle_timeout(ssl);
    AsyncBioEnforceWriteQuota(test_state->async_bio, true);

    if (timeout_ret < 0) {
      fprintf(stderr, "Error retransmitting.\n");
      return false;
    }
    return true;
  }

  // See if we needed to read or write more. If so, allow one byte through on
  // the appropriate end to maximally stress the state machine.
  switch (SSL_get_error(ssl, ret)) {
    case SSL_ERROR_WANT_READ:
      AsyncBioAllowRead(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_WRITE:
      AsyncBioAllowWrite(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_CHANNEL_ID_LOOKUP: {
      bssl::UniquePtr<EVP_PKEY> pkey =
          LoadPrivateKey(GetTestConfig(ssl)->send_channel_id);
      if (!pkey) {
        return false;
      }
      test_state->channel_id = std::move(pkey);
      return true;
    }
    case SSL_ERROR_WANT_X509_LOOKUP:
      test_state->cert_ready = true;
      return true;
    case SSL_ERROR_PENDING_SESSION:
      test_state->session = std::move(test_state->pending_session);
      return true;
    case SSL_ERROR_PENDING_CERTIFICATE:
      test_state->early_callback_ready = true;
      return true;
    case SSL_ERROR_WANT_PRIVATE_KEY_OPERATION:
      test_state->private_key_retries++;
      return true;
    case SSL_ERROR_WANT_CERTIFICATE_VERIFY:
      test_state->custom_verify_ready = true;
      return true;
    default:
      return false;
  }
}

// CheckIdempotentError runs |func|, an operation on |ssl|, ensuring that
// errors are idempotent.
static int CheckIdempotentError(const char *name, SSL *ssl,
                                std::function<int()> func) {
  int ret = func();
  int ssl_err = SSL_get_error(ssl, ret);
  uint32_t err = ERR_peek_error();
  if (ssl_err == SSL_ERROR_SSL || ssl_err == SSL_ERROR_ZERO_RETURN) {
    int ret2 = func();
    int ssl_err2 = SSL_get_error(ssl, ret2);
    uint32_t err2 = ERR_peek_error();
    if (ret != ret2 || ssl_err != ssl_err2 || err != err2) {
      fprintf(stderr, "Repeating %s did not replay the error.\n", name);
      char buf[256];
      ERR_error_string_n(err, buf, sizeof(buf));
      fprintf(stderr, "Wanted: %d %d %s\n", ret, ssl_err, buf);
      ERR_error_string_n(err2, buf, sizeof(buf));
      fprintf(stderr, "Got:    %d %d %s\n", ret2, ssl_err2, buf);
      // runner treats exit code 90 as always failing. Otherwise, it may
      // accidentally consider the result an expected protocol failure.
      exit(90);
    }
  }
  return ret;
}

// DoRead reads from |ssl|, resolving any asynchronous operations. It returns
// the result value of the final |SSL_read| call.
static int DoRead(SSL *ssl, uint8_t *out, size_t max_out) {
  const TestConfig *config = GetTestConfig(ssl);
  TestState *test_state = GetTestState(ssl);
  int ret;
  do {
    if (config->async) {
      // The DTLS retransmit logic silently ignores write failures. So the test
      // may progress, allow writes through synchronously. |SSL_read| may
      // trigger a retransmit, so disconnect the write quota.
      AsyncBioEnforceWriteQuota(test_state->async_bio, false);
    }
    ret = CheckIdempotentError("SSL_peek/SSL_read", ssl, [&]() -> int {
      return config->peek_then_read ? SSL_peek(ssl, out, max_out)
                                    : SSL_read(ssl, out, max_out);
    });
    if (config->async) {
      AsyncBioEnforceWriteQuota(test_state->async_bio, true);
    }

    // Run the exporter after each read. This is to test that the exporter fails
    // during a renegotiation.
    if (config->use_exporter_between_reads) {
      uint8_t buf;
      if (!SSL_export_keying_material(ssl, &buf, 1, NULL, 0, NULL, 0, 0)) {
        fprintf(stderr, "failed to export keying material\n");
        return -1;
      }
    }
  } while (config->async && RetryAsync(ssl, ret));

  if (config->peek_then_read && ret > 0) {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[static_cast<size_t>(ret)]);

    // SSL_peek should synchronously return the same data.
    int ret2 = SSL_peek(ssl, buf.get(), ret);
    if (ret2 != ret ||
        OPENSSL_memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "First and second SSL_peek did not match.\n");
      return -1;
    }

    // SSL_read should synchronously return the same data and consume it.
    ret2 = SSL_read(ssl, buf.get(), ret);
    if (ret2 != ret ||
        OPENSSL_memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "SSL_peek and SSL_read did not match.\n");
      return -1;
    }
  }

  return ret;
}

// WriteAll writes |in_len| bytes from |in| to |ssl|, resolving any asynchronous
// operations. It returns the result of the final |SSL_write| call.
static int WriteAll(SSL *ssl, const void *in_, size_t in_len) {
  const uint8_t *in = reinterpret_cast<const uint8_t *>(in_);
  const TestConfig *config = GetTestConfig(ssl);
  int ret;
  do {
    ret = SSL_write(ssl, in, in_len);
    if (ret > 0) {
      in += ret;
      in_len -= ret;
    }
  } while ((config->async && RetryAsync(ssl, ret)) || (ret > 0 && in_len > 0));
  return ret;
}

// DoShutdown calls |SSL_shutdown|, resolving any asynchronous operations. It
// returns the result of the final |SSL_shutdown| call.
static int DoShutdown(SSL *ssl) {
  const TestConfig *config = GetTestConfig(ssl);
  int ret;
  do {
    ret = SSL_shutdown(ssl);
  } while (config->async && RetryAsync(ssl, ret));
  return ret;
}

// DoSendFatalAlert calls |SSL_send_fatal_alert|, resolving any asynchronous
// operations. It returns the result of the final |SSL_send_fatal_alert| call.
static int DoSendFatalAlert(SSL *ssl, uint8_t alert) {
  const TestConfig *config = GetTestConfig(ssl);
  int ret;
  do {
    ret = SSL_send_fatal_alert(ssl, alert);
  } while (config->async && RetryAsync(ssl, ret));
  return ret;
}

static uint16_t GetProtocolVersion(const SSL *ssl) {
  uint16_t version = SSL_version(ssl);
  if (!SSL_is_dtls(ssl)) {
    return version;
  }
  return 0x0201 + ~version;
}

// CheckAuthProperties checks, after the initial handshake is completed or
// after a renegotiation, that authentication-related properties match |config|.
static bool CheckAuthProperties(SSL *ssl, bool is_resume,
                                const TestConfig *config) {
  if (!config->expected_ocsp_response.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_ocsp_response(ssl, &data, &len);
    if (config->expected_ocsp_response.size() != len ||
        OPENSSL_memcmp(config->expected_ocsp_response.data(), data, len) != 0) {
      fprintf(stderr, "OCSP response mismatch\n");
      return false;
    }
  }

  if (!config->expected_signed_cert_timestamps.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_signed_cert_timestamp_list(ssl, &data, &len);
    if (config->expected_signed_cert_timestamps.size() != len ||
        OPENSSL_memcmp(config->expected_signed_cert_timestamps.data(), data,
                       len) != 0) {
      fprintf(stderr, "SCT list mismatch\n");
      return false;
    }
  }

  if (config->expect_verify_result) {
    int expected_verify_result = config->verify_fail ?
      X509_V_ERR_APPLICATION_VERIFICATION :
      X509_V_OK;

    if (SSL_get_verify_result(ssl) != expected_verify_result) {
      fprintf(stderr, "Wrong certificate verification result\n");
      return false;
    }
  }

  if (!config->expect_peer_cert_file.empty()) {
    bssl::UniquePtr<X509> expect_leaf;
    bssl::UniquePtr<STACK_OF(X509)> expect_chain;
    if (!LoadCertificate(&expect_leaf, &expect_chain,
                         config->expect_peer_cert_file)) {
      return false;
    }

    // For historical reasons, clients report a chain with a leaf and servers
    // without.
    if (!config->is_server) {
      if (!sk_X509_insert(expect_chain.get(), expect_leaf.get(), 0)) {
        return false;
      }
      X509_up_ref(expect_leaf.get());  // sk_X509_push takes ownership.
    }

    bssl::UniquePtr<X509> leaf(SSL_get_peer_certificate(ssl));
    STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ssl);
    if (X509_cmp(leaf.get(), expect_leaf.get()) != 0) {
      fprintf(stderr, "Received a different leaf certificate than expected.\n");
      return false;
    }

    if (sk_X509_num(chain) != sk_X509_num(expect_chain.get())) {
      fprintf(stderr, "Received a chain of length %zu instead of %zu.\n",
              sk_X509_num(chain), sk_X509_num(expect_chain.get()));
      return false;
    }

    for (size_t i = 0; i < sk_X509_num(chain); i++) {
      if (X509_cmp(sk_X509_value(chain, i),
                   sk_X509_value(expect_chain.get(), i)) != 0) {
        fprintf(stderr, "Chain certificate %zu did not match.\n",
                i + 1);
        return false;
      }
    }
  }

  if (SSL_get_session(ssl)->peer_sha256_valid !=
      config->expect_sha256_client_cert) {
    fprintf(stderr,
            "Unexpected SHA-256 client cert state: expected:%d is_resume:%d.\n",
            config->expect_sha256_client_cert, is_resume);
    return false;
  }

  if (config->expect_sha256_client_cert &&
      SSL_get_session(ssl)->certs != nullptr) {
    fprintf(stderr, "Have both client cert and SHA-256 hash: is_resume:%d.\n",
            is_resume);
    return false;
  }

  return true;
}

// CheckHandshakeProperties checks, immediately after |ssl| completes its
// initial handshake (or False Starts), whether all the properties are
// consistent with the test configuration and invariants.
static bool CheckHandshakeProperties(SSL *ssl, bool is_resume,
                                     const TestConfig *config) {
  if (!CheckAuthProperties(ssl, is_resume, config)) {
    return false;
  }

  if (SSL_get_current_cipher(ssl) == nullptr) {
    fprintf(stderr, "null cipher after handshake\n");
    return false;
  }

  if (config->expect_version != 0 &&
      SSL_version(ssl) != config->expect_version) {
    fprintf(stderr, "want version %04x, got %04x\n", config->expect_version,
            SSL_version(ssl));
    return false;
  }

  bool expect_resume =
      is_resume && (!config->expect_session_miss || SSL_in_early_data(ssl));
  if (!!SSL_session_reused(ssl) != expect_resume) {
    fprintf(stderr, "session unexpectedly was%s reused\n",
            SSL_session_reused(ssl) ? "" : " not");
    return false;
  }

  bool expect_handshake_done =
      (is_resume || !config->false_start) && !SSL_in_early_data(ssl);
  if (expect_handshake_done != GetTestState(ssl)->handshake_done) {
    fprintf(stderr, "handshake was%s completed\n",
            GetTestState(ssl)->handshake_done ? "" : " not");
    return false;
  }

  if (expect_handshake_done && !config->is_server) {
    bool expect_new_session =
        !config->expect_no_session &&
        (!SSL_session_reused(ssl) || config->expect_ticket_renewal) &&
        // Session tickets are sent post-handshake in TLS 1.3.
        GetProtocolVersion(ssl) < TLS1_3_VERSION;
    if (expect_new_session != GetTestState(ssl)->got_new_session) {
      fprintf(stderr,
              "new session was%s cached, but we expected the opposite\n",
              GetTestState(ssl)->got_new_session ? "" : " not");
      return false;
    }
  }

  if (!is_resume) {
    if (config->expect_session_id && !GetTestState(ssl)->got_new_session) {
      fprintf(stderr, "session was not cached on the server.\n");
      return false;
    }
    if (config->expect_no_session_id && GetTestState(ssl)->got_new_session) {
      fprintf(stderr, "session was unexpectedly cached on the server.\n");
      return false;
    }
  }

  if (config->is_server && !GetTestState(ssl)->early_callback_called) {
    fprintf(stderr, "early callback not called\n");
    return false;
  }

  if (!config->expected_server_name.empty()) {
    const char *server_name =
        SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (server_name == nullptr ||
        server_name != config->expected_server_name) {
      fprintf(stderr, "servername mismatch (got %s; want %s)\n",
              server_name, config->expected_server_name.c_str());
      return false;
    }
  }

  if (!config->expected_next_proto.empty()) {
    const uint8_t *next_proto;
    unsigned next_proto_len;
    SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
    if (next_proto_len != config->expected_next_proto.size() ||
        OPENSSL_memcmp(next_proto, config->expected_next_proto.data(),
                       next_proto_len) != 0) {
      fprintf(stderr, "negotiated next proto mismatch\n");
      return false;
    }
  }

  if (!config->is_server) {
    const uint8_t *alpn_proto;
    unsigned alpn_proto_len;
    SSL_get0_alpn_selected(ssl, &alpn_proto, &alpn_proto_len);
    if (alpn_proto_len != config->expected_alpn.size() ||
        OPENSSL_memcmp(alpn_proto, config->expected_alpn.data(),
                       alpn_proto_len) != 0) {
      fprintf(stderr, "negotiated alpn proto mismatch\n");
      return false;
    }
  }

  if (!config->expected_channel_id.empty()) {
    uint8_t channel_id[64];
    if (!SSL_get_tls_channel_id(ssl, channel_id, sizeof(channel_id))) {
      fprintf(stderr, "no channel id negotiated\n");
      return false;
    }
    if (config->expected_channel_id.size() != 64 ||
        OPENSSL_memcmp(config->expected_channel_id.data(), channel_id, 64) !=
            0) {
      fprintf(stderr, "channel id mismatch\n");
      return false;
    }
  }

  if (config->expect_extended_master_secret && !SSL_get_extms_support(ssl)) {
    fprintf(stderr, "No EMS for connection when expected\n");
    return false;
  }

  if (config->expect_secure_renegotiation &&
      !SSL_get_secure_renegotiation_support(ssl)) {
    fprintf(stderr, "No secure renegotiation for connection when expected\n");
    return false;
  }

  if (config->expect_no_secure_renegotiation &&
      SSL_get_secure_renegotiation_support(ssl)) {
    fprintf(stderr,
            "Secure renegotiation unexpectedly negotiated for connection\n");
    return false;
  }

  if (config->expect_peer_signature_algorithm != 0 &&
      config->expect_peer_signature_algorithm !=
          SSL_get_peer_signature_algorithm(ssl)) {
    fprintf(stderr, "Peer signature algorithm was %04x, wanted %04x.\n",
            SSL_get_peer_signature_algorithm(ssl),
            config->expect_peer_signature_algorithm);
    return false;
  }

  if (config->expect_curve_id != 0) {
    uint16_t curve_id = SSL_get_curve_id(ssl);
    if (static_cast<uint16_t>(config->expect_curve_id) != curve_id) {
      fprintf(stderr, "curve_id was %04x, wanted %04x\n", curve_id,
              static_cast<uint16_t>(config->expect_curve_id));
      return false;
    }
  }

  uint16_t cipher_id =
      static_cast<uint16_t>(SSL_CIPHER_get_id(SSL_get_current_cipher(ssl)));
  if (config->expect_cipher_aes != 0 &&
      EVP_has_aes_hardware() &&
      static_cast<uint16_t>(config->expect_cipher_aes) != cipher_id) {
    fprintf(stderr, "Cipher ID was %04x, wanted %04x (has AES hardware)\n",
            cipher_id, static_cast<uint16_t>(config->expect_cipher_aes));
    return false;
  }

  if (config->expect_cipher_no_aes != 0 &&
      !EVP_has_aes_hardware() &&
      static_cast<uint16_t>(config->expect_cipher_no_aes) != cipher_id) {
    fprintf(stderr, "Cipher ID was %04x, wanted %04x (no AES hardware)\n",
            cipher_id, static_cast<uint16_t>(config->expect_cipher_no_aes));
    return false;
  }

  if (is_resume && !SSL_in_early_data(ssl)) {
    if ((config->expect_accept_early_data && !SSL_early_data_accepted(ssl)) ||
        (config->expect_reject_early_data && SSL_early_data_accepted(ssl))) {
      fprintf(stderr,
              "Early data was%s accepted, but we expected the opposite\n",
              SSL_early_data_accepted(ssl) ? "" : " not");
      return false;
    }
  }

  if (!config->psk.empty()) {
    if (SSL_get_peer_cert_chain(ssl) != nullptr) {
      fprintf(stderr, "Received peer certificate on a PSK cipher.\n");
      return false;
    }
  } else if (!config->is_server || config->require_any_client_certificate) {
    if (SSL_get_peer_cert_chain(ssl) == nullptr) {
      fprintf(stderr, "Received no peer certificate but expected one.\n");
      return false;
    }
  }

  if (is_resume && config->expect_ticket_age_skew != 0 &&
      SSL_get_ticket_age_skew(ssl) != config->expect_ticket_age_skew) {
    fprintf(stderr, "Ticket age skew was %" PRId32 ", wanted %d\n",
            SSL_get_ticket_age_skew(ssl), config->expect_ticket_age_skew);
    return false;
  }

  return true;
}

static bool WriteSettings(int i, const TestConfig *config,
                          const SSL_SESSION *session) {
  if (config->write_settings.empty()) {
    return true;
  }

  // Treat write_settings as a path prefix for each connection in the run.
  char buf[DECIMAL_SIZE(int)];
  snprintf(buf, sizeof(buf), "%d", i);
  std::string path = config->write_settings + buf;

  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 64)) {
    return false;
  }

  if (session != nullptr) {
    uint8_t *data;
    size_t len;
    if (!SSL_SESSION_to_bytes(session, &data, &len)) {
      return false;
    }
    bssl::UniquePtr<uint8_t> free_data(data);
    CBB child;
    if (!CBB_add_u16(cbb.get(), kSessionTag) ||
        !CBB_add_u24_length_prefixed(cbb.get(), &child) ||
        !CBB_add_bytes(&child, data, len) ||
        !CBB_flush(cbb.get())) {
      return false;
    }
  }

  if (config->is_server &&
      (config->require_any_client_certificate || config->verify_peer) &&
      !CBB_add_u16(cbb.get(), kRequestClientCert)) {
    return false;
  }

  if (config->tls13_variant != 0 &&
      (!CBB_add_u16(cbb.get(), kTLS13Variant) ||
       !CBB_add_u8(cbb.get(), static_cast<uint8_t>(config->tls13_variant)))) {
    return false;
  }

  uint8_t *settings;
  size_t settings_len;
  if (!CBB_add_u16(cbb.get(), kDataTag) ||
      !CBB_finish(cbb.get(), &settings, &settings_len)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_settings(settings);

  using ScopedFILE = std::unique_ptr<FILE, decltype(&fclose)>;
  ScopedFILE file(fopen(path.c_str(), "w"), fclose);
  if (!file) {
    return false;
  }

  return fwrite(settings, settings_len, 1, file.get()) == 1;
}

static bool DoExchange(bssl::UniquePtr<SSL_SESSION> *out_session, SSL *ssl,
                       const TestConfig *config, bool is_resume, bool is_retry);

// DoConnection tests an SSL connection against the peer. On success, it returns
// true and sets |*out_session| to the negotiated SSL session. If the test is a
// resumption attempt, |is_resume| is true and |session| is the session from the
// previous exchange.
static bool DoConnection(bssl::UniquePtr<SSL_SESSION> *out_session,
                         SSL_CTX *ssl_ctx, const TestConfig *config,
                         const TestConfig *retry_config, bool is_resume,
                         SSL_SESSION *session) {
  bssl::UniquePtr<SSL> ssl(SSL_new(ssl_ctx));
  if (!ssl) {
    return false;
  }

  if (!SetTestConfig(ssl.get(), config) ||
      !SetTestState(ssl.get(), std::unique_ptr<TestState>(new TestState))) {
    return false;
  }

  GetTestState(ssl.get())->is_resume = is_resume;

  if (config->fallback_scsv &&
      !SSL_set_mode(ssl.get(), SSL_MODE_SEND_FALLBACK_SCSV)) {
    return false;
  }
  // Install the certificate synchronously if nothing else will handle it.
  if (!config->use_early_callback &&
      !config->use_old_client_cert_callback &&
      !config->async &&
      !InstallCertificate(ssl.get())) {
    return false;
  }
  if (!config->use_old_client_cert_callback) {
    SSL_set_cert_cb(ssl.get(), CertCallback, nullptr);
  }
  int mode = SSL_VERIFY_NONE;
  if (config->require_any_client_certificate) {
    mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }
  if (config->verify_peer) {
    mode = SSL_VERIFY_PEER;
  }
  if (config->verify_peer_if_no_obc) {
    // Set SSL_VERIFY_FAIL_IF_NO_PEER_CERT so testing whether client
    // certificates were requested is easy.
    mode = SSL_VERIFY_PEER | SSL_VERIFY_PEER_IF_NO_OBC |
           SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }
  if (config->use_custom_verify_callback) {
    SSL_set_custom_verify(ssl.get(), mode, CustomVerifyCallback);
  } else if (mode != SSL_VERIFY_NONE) {
    SSL_set_verify(ssl.get(), mode, NULL);
  }
  if (config->false_start) {
    SSL_set_mode(ssl.get(), SSL_MODE_ENABLE_FALSE_START);
  }
  if (config->cbc_record_splitting) {
    SSL_set_mode(ssl.get(), SSL_MODE_CBC_RECORD_SPLITTING);
  }
  if (config->partial_write) {
    SSL_set_mode(ssl.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
  }
  if (config->no_tls13) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_3);
  }
  if (config->no_tls12) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_2);
  }
  if (config->no_tls11) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1_1);
  }
  if (config->no_tls1) {
    SSL_set_options(ssl.get(), SSL_OP_NO_TLSv1);
  }
  if (config->no_ssl3) {
    SSL_set_options(ssl.get(), SSL_OP_NO_SSLv3);
  }
  if (!config->expected_channel_id.empty() ||
      config->enable_channel_id) {
    SSL_set_tls_channel_id_enabled(ssl.get(), 1);
  }
  if (!config->send_channel_id.empty()) {
    SSL_set_tls_channel_id_enabled(ssl.get(), 1);
    if (!config->async) {
      // The async case will be supplied by |ChannelIdCallback|.
      bssl::UniquePtr<EVP_PKEY> pkey = LoadPrivateKey(config->send_channel_id);
      if (!pkey || !SSL_set1_tls_channel_id(ssl.get(), pkey.get())) {
        return false;
      }
    }
  }
  if (!config->host_name.empty() &&
      !SSL_set_tlsext_host_name(ssl.get(), config->host_name.c_str())) {
    return false;
  }
  if (!config->advertise_alpn.empty() &&
      SSL_set_alpn_protos(ssl.get(),
                          (const uint8_t *)config->advertise_alpn.data(),
                          config->advertise_alpn.size()) != 0) {
    return false;
  }
  if (!config->psk.empty()) {
    SSL_set_psk_client_callback(ssl.get(), PskClientCallback);
    SSL_set_psk_server_callback(ssl.get(), PskServerCallback);
  }
  if (!config->psk_identity.empty() &&
      !SSL_use_psk_identity_hint(ssl.get(), config->psk_identity.c_str())) {
    return false;
  }
  if (!config->srtp_profiles.empty() &&
      !SSL_set_srtp_profiles(ssl.get(), config->srtp_profiles.c_str())) {
    return false;
  }
  if (config->enable_ocsp_stapling) {
    SSL_enable_ocsp_stapling(ssl.get());
  }
  if (config->enable_signed_cert_timestamps) {
    SSL_enable_signed_cert_timestamps(ssl.get());
  }
  if (config->min_version != 0 &&
      !SSL_set_min_proto_version(ssl.get(), (uint16_t)config->min_version)) {
    return false;
  }
  if (config->max_version != 0 &&
      !SSL_set_max_proto_version(ssl.get(), (uint16_t)config->max_version)) {
    return false;
  }
  if (config->mtu != 0) {
    SSL_set_options(ssl.get(), SSL_OP_NO_QUERY_MTU);
    SSL_set_mtu(ssl.get(), config->mtu);
  }
  if (config->install_ddos_callback) {
    SSL_CTX_set_dos_protection_cb(ssl_ctx, DDoSCallback);
  }
  if (config->renegotiate_once) {
    SSL_set_renegotiate_mode(ssl.get(), ssl_renegotiate_once);
  }
  if (config->renegotiate_freely) {
    SSL_set_renegotiate_mode(ssl.get(), ssl_renegotiate_freely);
  }
  if (config->renegotiate_ignore) {
    SSL_set_renegotiate_mode(ssl.get(), ssl_renegotiate_ignore);
  }
  if (!config->check_close_notify) {
    SSL_set_quiet_shutdown(ssl.get(), 1);
  }
  if (config->p384_only) {
    int nid = NID_secp384r1;
    if (!SSL_set1_curves(ssl.get(), &nid, 1)) {
      return false;
    }
  }
  if (config->enable_all_curves) {
    static const int kAllCurves[] = {
        NID_secp224r1, NID_X9_62_prime256v1, NID_secp384r1,
        NID_secp521r1, NID_X25519,
    };
    if (!SSL_set1_curves(ssl.get(), kAllCurves,
                         OPENSSL_ARRAY_SIZE(kAllCurves))) {
      return false;
    }
  }
  if (config->initial_timeout_duration_ms > 0) {
    DTLSv1_set_initial_timeout_duration(ssl.get(),
                                        config->initial_timeout_duration_ms);
  }
  if (config->max_cert_list > 0) {
    SSL_set_max_cert_list(ssl.get(), config->max_cert_list);
  }
  if (config->retain_only_sha256_client_cert) {
    SSL_set_retain_only_sha256_of_client_certs(ssl.get(), 1);
  }
  if (config->max_send_fragment > 0) {
    SSL_set_max_send_fragment(ssl.get(), config->max_send_fragment);
  }

  int sock = Connect(config->port);
  if (sock == -1) {
    return false;
  }
  SocketCloser closer(sock);

  bssl::UniquePtr<BIO> bio(BIO_new_socket(sock, BIO_NOCLOSE));
  if (!bio) {
    return false;
  }
  if (config->is_dtls) {
    bssl::UniquePtr<BIO> packeted = PacketedBioCreate(&g_clock);
    if (!packeted) {
      return false;
    }
    GetTestState(ssl.get())->packeted_bio = packeted.get();
    BIO_push(packeted.get(), bio.release());
    bio = std::move(packeted);
  }
  if (config->async) {
    bssl::UniquePtr<BIO> async_scoped =
        config->is_dtls ? AsyncBioCreateDatagram() : AsyncBioCreate();
    if (!async_scoped) {
      return false;
    }
    BIO_push(async_scoped.get(), bio.release());
    GetTestState(ssl.get())->async_bio = async_scoped.get();
    bio = std::move(async_scoped);
  }
  SSL_set_bio(ssl.get(), bio.get(), bio.get());
  bio.release();  // SSL_set_bio takes ownership.

  if (session != NULL) {
    if (!config->is_server) {
      if (SSL_set_session(ssl.get(), session) != 1) {
        return false;
      }
    } else if (config->async) {
      // The internal session cache is disabled, so install the session
      // manually.
      SSL_SESSION_up_ref(session);
      GetTestState(ssl.get())->pending_session.reset(session);
    }
  }

  if (SSL_get_current_cipher(ssl.get()) != nullptr) {
    fprintf(stderr, "non-null cipher before handshake\n");
    return false;
  }

  if (config->is_server) {
    SSL_set_accept_state(ssl.get());
  } else {
    SSL_set_connect_state(ssl.get());
  }

  bool ret = DoExchange(out_session, ssl.get(), config, is_resume, false);
  if (!config->is_server && is_resume && config->expect_reject_early_data) {
    // We must have failed due to an early data rejection.
    if (ret) {
      fprintf(stderr, "0-RTT exchange unexpected succeeded.\n");
      return false;
    }
    if (SSL_get_error(ssl.get(), -1) != SSL_ERROR_EARLY_DATA_REJECTED) {
      fprintf(stderr,
              "SSL_get_error did not signal SSL_ERROR_EARLY_DATA_REJECTED.\n");
      return false;
    }

    // Before reseting, early state should still be available.
    if (!SSL_in_early_data(ssl.get()) ||
        !CheckHandshakeProperties(ssl.get(), is_resume, config)) {
      fprintf(stderr, "SSL_in_early_data returned false before reset.\n");
      return false;
    }

    // Reset the connection and try again at 1-RTT.
    SSL_reset_early_data_reject(ssl.get());

    // After reseting, the socket should report it is no longer in an early data
    // state.
    if (SSL_in_early_data(ssl.get())) {
      fprintf(stderr, "SSL_in_early_data returned true after reset.\n");
      return false;
    }

    if (!SetTestConfig(ssl.get(), retry_config)) {
      return false;
    }

    ret = DoExchange(out_session, ssl.get(), retry_config, is_resume, true);
  }

  if (!ret) {
    return false;
  }

  if (!GetTestState(ssl.get())->msg_callback_ok) {
    return false;
  }

  if (!config->expect_msg_callback.empty() &&
      GetTestState(ssl.get())->msg_callback_text !=
          config->expect_msg_callback) {
    fprintf(stderr, "Bad message callback trace. Wanted:\n%s\nGot:\n%s\n",
            config->expect_msg_callback.c_str(),
            GetTestState(ssl.get())->msg_callback_text.c_str());
    return false;
  }

  return true;
}

static bool DoExchange(bssl::UniquePtr<SSL_SESSION> *out_session, SSL *ssl,
                       const TestConfig *config, bool is_resume,
                       bool is_retry) {
  int ret;
  if (!config->implicit_handshake) {
    do {
      ret = CheckIdempotentError("SSL_do_handshake", ssl, [&]() -> int {
        return SSL_do_handshake(ssl);
      });
    } while (config->async && RetryAsync(ssl, ret));
    if (ret != 1 ||
        !CheckHandshakeProperties(ssl, is_resume, config)) {
      return false;
    }

    if (is_resume && !is_retry && !config->is_server &&
        config->expect_no_offer_early_data && SSL_in_early_data(ssl)) {
      fprintf(stderr, "Client unexpectedly offered early data.\n");
      return false;
    }

    if (config->handshake_twice) {
      do {
        ret = SSL_do_handshake(ssl);
      } while (config->async && RetryAsync(ssl, ret));
      if (ret != 1) {
        return false;
      }
    }

    // Skip the |config->async| logic as this should be a no-op.
    if (config->no_op_extra_handshake &&
        SSL_do_handshake(ssl) != 1) {
      fprintf(stderr, "Extra SSL_do_handshake was not a no-op.\n");
      return false;
    }

    // Reset the state to assert later that the callback isn't called in
    // renegotations.
    GetTestState(ssl)->got_new_session = false;
  }

  if (config->export_keying_material > 0) {
    std::vector<uint8_t> result(
        static_cast<size_t>(config->export_keying_material));
    if (!SSL_export_keying_material(
            ssl, result.data(), result.size(), config->export_label.data(),
            config->export_label.size(),
            reinterpret_cast<const uint8_t *>(config->export_context.data()),
            config->export_context.size(), config->use_export_context)) {
      fprintf(stderr, "failed to export keying material\n");
      return false;
    }
    if (WriteAll(ssl, result.data(), result.size()) < 0) {
      return false;
    }
  }

  if (config->tls_unique) {
    uint8_t tls_unique[16];
    size_t tls_unique_len;
    if (!SSL_get_tls_unique(ssl, tls_unique, &tls_unique_len,
                            sizeof(tls_unique))) {
      fprintf(stderr, "failed to get tls-unique\n");
      return false;
    }

    if (tls_unique_len != 12) {
      fprintf(stderr, "expected 12 bytes of tls-unique but got %u",
              static_cast<unsigned>(tls_unique_len));
      return false;
    }

    if (WriteAll(ssl, tls_unique, tls_unique_len) < 0) {
      return false;
    }
  }

  if (config->send_alert) {
    if (DoSendFatalAlert(ssl, SSL_AD_DECOMPRESSION_FAILURE) < 0) {
      return false;
    }
    return true;
  }

  if (config->write_different_record_sizes) {
    if (config->is_dtls) {
      fprintf(stderr, "write_different_record_sizes not supported for DTLS\n");
      return false;
    }
    // This mode writes a number of different record sizes in an attempt to
    // trip up the CBC record splitting code.
    static const size_t kBufLen = 32769;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[kBufLen]);
    OPENSSL_memset(buf.get(), 0x42, kBufLen);
    static const size_t kRecordSizes[] = {
        0, 1, 255, 256, 257, 16383, 16384, 16385, 32767, 32768, 32769};
    for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kRecordSizes); i++) {
      const size_t len = kRecordSizes[i];
      if (len > kBufLen) {
        fprintf(stderr, "Bad kRecordSizes value.\n");
        return false;
      }
      if (WriteAll(ssl, buf.get(), len) < 0) {
        return false;
      }
    }
  } else {
    static const char kInitialWrite[] = "hello";
    bool pending_initial_write = false;
    if (config->read_with_unfinished_write) {
      if (!config->async) {
        fprintf(stderr, "-read-with-unfinished-write requires -async.\n");
        return false;
      }

      // Let only one byte of the record through.
      AsyncBioAllowWrite(GetTestState(ssl)->async_bio, 1);
      int write_ret =
          SSL_write(ssl, kInitialWrite, strlen(kInitialWrite));
      if (SSL_get_error(ssl, write_ret) != SSL_ERROR_WANT_WRITE) {
        fprintf(stderr, "Failed to leave unfinished write.\n");
        return false;
      }
      pending_initial_write = true;
    } else if (config->shim_writes_first) {
      if (WriteAll(ssl, kInitialWrite, strlen(kInitialWrite)) < 0) {
        return false;
      }
    }
    if (!config->shim_shuts_down) {
      for (;;) {
        // Read only 512 bytes at a time in TLS to ensure records may be
        // returned in multiple reads.
        size_t read_size = config->is_dtls ? 16384 : 512;
        if (config->read_size > 0) {
          read_size = config->read_size;
        }
        std::unique_ptr<uint8_t[]> buf(new uint8_t[read_size]);

        int n = DoRead(ssl, buf.get(), read_size);
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_ZERO_RETURN ||
            (n == 0 && err == SSL_ERROR_SYSCALL)) {
          if (n != 0) {
            fprintf(stderr, "Invalid SSL_get_error output\n");
            return false;
          }
          // Stop on either clean or unclean shutdown.
          break;
        } else if (err != SSL_ERROR_NONE) {
          if (n > 0) {
            fprintf(stderr, "Invalid SSL_get_error output\n");
            return false;
          }
          return false;
        }
        // Successfully read data.
        if (n <= 0) {
          fprintf(stderr, "Invalid SSL_get_error output\n");
          return false;
        }

        if (!config->is_server && is_resume && !is_retry &&
            config->expect_reject_early_data) {
          fprintf(stderr,
                  "Unexpectedly received data instead of 0-RTT reject.\n");
          return false;
        }

        // After a successful read, with or without False Start, the handshake
        // must be complete unless we are doing early data.
        if (!GetTestState(ssl)->handshake_done &&
            !SSL_early_data_accepted(ssl)) {
          fprintf(stderr, "handshake was not completed after SSL_read\n");
          return false;
        }

        // Clear the initial write, if unfinished.
        if (pending_initial_write) {
          if (WriteAll(ssl, kInitialWrite, strlen(kInitialWrite)) < 0) {
            return false;
          }
          pending_initial_write = false;
        }

        for (int i = 0; i < n; i++) {
          buf[i] ^= 0xff;
        }
        if (WriteAll(ssl, buf.get(), n) < 0) {
          return false;
        }
      }
    }
  }

  if (!config->is_server && !config->false_start &&
      !config->implicit_handshake &&
      // Session tickets are sent post-handshake in TLS 1.3.
      GetProtocolVersion(ssl) < TLS1_3_VERSION &&
      GetTestState(ssl)->got_new_session) {
    fprintf(stderr, "new session was established after the handshake\n");
    return false;
  }

  if (GetProtocolVersion(ssl) >= TLS1_3_VERSION && !config->is_server) {
    bool expect_new_session =
        !config->expect_no_session && !config->shim_shuts_down;
    if (expect_new_session != GetTestState(ssl)->got_new_session) {
      fprintf(stderr,
              "new session was%s cached, but we expected the opposite\n",
              GetTestState(ssl)->got_new_session ? "" : " not");
      return false;
    }

    if (expect_new_session) {
      bool got_early_data =
          GetTestState(ssl)->new_session->ticket_max_early_data != 0;
      if (config->expect_ticket_supports_early_data != got_early_data) {
        fprintf(stderr,
                "new session did%s support early data, but we expected the "
                "opposite\n",
                got_early_data ? "" : " not");
        return false;
      }
    }
  }

  if (out_session) {
    *out_session = std::move(GetTestState(ssl)->new_session);
  }

  ret = DoShutdown(ssl);

  if (config->shim_shuts_down && config->check_close_notify) {
    // We initiate shutdown, so |SSL_shutdown| will return in two stages. First
    // it returns zero when our close_notify is sent, then one when the peer's
    // is received.
    if (ret != 0) {
      fprintf(stderr, "Unexpected SSL_shutdown result: %d != 0\n", ret);
      return false;
    }
    ret = DoShutdown(ssl);
  }

  if (ret != 1) {
    fprintf(stderr, "Unexpected SSL_shutdown result: %d != 1\n", ret);
    return false;
  }

  if (SSL_total_renegotiations(ssl) > 0) {
    if (!SSL_get_session(ssl)->not_resumable) {
      fprintf(stderr,
              "Renegotiations should never produce resumable sessions.\n");
      return false;
    }

    if (SSL_session_reused(ssl)) {
      fprintf(stderr, "Renegotiations should never resume sessions.\n");
      return false;
    }

    // Re-check authentication properties after a renegotiation. The reported
    // values should remain unchanged even if the server sent different SCT
    // lists.
    if (!CheckAuthProperties(ssl, is_resume, config)) {
      return false;
    }
  }

  if (SSL_total_renegotiations(ssl) != config->expect_total_renegotiations) {
    fprintf(stderr, "Expected %d renegotiations, got %d\n",
            config->expect_total_renegotiations, SSL_total_renegotiations(ssl));
    return false;
  }

  return true;
}

class StderrDelimiter {
 public:
  ~StderrDelimiter() { fprintf(stderr, "--- DONE ---\n"); }
};

int main(int argc, char **argv) {
  // To distinguish ASan's output from ours, add a trailing message to stderr.
  // Anything following this line will be considered an error.
  StderrDelimiter delimiter;

#if defined(OPENSSL_WINDOWS)
  // Initialize Winsock.
  WORD wsa_version = MAKEWORD(2, 2);
  WSADATA wsa_data;
  int wsa_err = WSAStartup(wsa_version, &wsa_data);
  if (wsa_err != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", wsa_err);
    return 1;
  }
  if (wsa_data.wVersion != wsa_version) {
    fprintf(stderr, "Didn't get expected version: %x\n", wsa_data.wVersion);
    return 1;
  }
#else
  signal(SIGPIPE, SIG_IGN);
#endif

  CRYPTO_library_init();
  g_config_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
  g_state_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, TestStateExFree);
  if (g_config_index < 0 || g_state_index < 0) {
    return 1;
  }

  TestConfig initial_config, resume_config, retry_config;
  if (!ParseConfig(argc - 1, argv + 1, &initial_config, &resume_config,
                   &retry_config)) {
    return Usage(argv[0]);
  }

  g_pool = CRYPTO_BUFFER_POOL_new();

  // Some code treats the zero time special, so initialize the clock to a
  // non-zero time.
  g_clock.tv_sec = 1234;
  g_clock.tv_usec = 1234;

  bssl::UniquePtr<SSL_CTX> ssl_ctx;

  bssl::UniquePtr<SSL_SESSION> session;
  for (int i = 0; i < initial_config.resume_count + 1; i++) {
    bool is_resume = i > 0;
    TestConfig *config = is_resume ? &resume_config : &initial_config;
    ssl_ctx = SetupCtx(ssl_ctx.get(), config);
    if (!ssl_ctx) {
      ERR_print_errors_fp(stderr);
      return 1;
    }

    if (is_resume && !initial_config.is_server && !session) {
      fprintf(stderr, "No session to offer.\n");
      return 1;
    }

    bssl::UniquePtr<SSL_SESSION> offer_session = std::move(session);
    if (!WriteSettings(i, config, offer_session.get())) {
      fprintf(stderr, "Error writing settings.\n");
      return 1;
    }
    if (!DoConnection(&session, ssl_ctx.get(), config, &retry_config, is_resume,
                      offer_session.get())) {
      fprintf(stderr, "Connection %d failed.\n", i + 1);
      ERR_print_errors_fp(stderr);
      return 1;
    }

    if (config->resumption_delay != 0) {
      g_clock.tv_sec += config->resumption_delay;
    }
  }

  return 0;
}
