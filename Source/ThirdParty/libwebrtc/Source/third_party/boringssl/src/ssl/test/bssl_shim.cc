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

#include <openssl/bio.h>
#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <memory>
#include <string>
#include <vector>

#include "../../crypto/internal.h"
#include "async_bio.h"
#include "packeted_bio.h"
#include "test_config.h"

namespace bssl {

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

static bssl::UniquePtr<X509> LoadCertificate(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  return bssl::UniquePtr<X509>(PEM_read_bio_X509(bio.get(), NULL, NULL, NULL));
}

static bssl::UniquePtr<EVP_PKEY> LoadPrivateKey(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), NULL, NULL, NULL));
}

static int AsyncPrivateKeyType(SSL *ssl) {
  EVP_PKEY *key = GetTestState(ssl)->private_key.get();
  switch (EVP_PKEY_id(key)) {
    case EVP_PKEY_RSA:
      return NID_rsaEncryption;
    case EVP_PKEY_EC:
      return EC_GROUP_get_curve_name(
          EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(key)));
    default:
      return NID_undef;
  }
}

static size_t AsyncPrivateKeyMaxSignatureLen(SSL *ssl) {
  return EVP_PKEY_size(GetTestState(ssl)->private_key.get());
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
    default:
      fprintf(stderr, "Unknown signature algorithm %04x.\n",
              signature_algorithm);
      return ssl_private_key_failure;
  }

  ScopedEVP_MD_CTX ctx;
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
  if (!EVP_DigestSignUpdate(ctx.get(), in, in_len) ||
      !EVP_DigestSignFinal(ctx.get(), nullptr, &len)) {
    return ssl_private_key_failure;
  }
  test_state->private_key_result.resize(len);
  if (!EVP_DigestSignFinal(ctx.get(), test_state->private_key_result.data(),
                           &len)) {
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
  memcpy(out, test_state->private_key_result.data(),
         test_state->private_key_result.size());
  *out_len = test_state->private_key_result.size();

  test_state->private_key_result.clear();
  test_state->private_key_retries = 0;
  return ssl_private_key_success;
}

static const SSL_PRIVATE_KEY_METHOD g_async_private_key_method = {
    AsyncPrivateKeyType,
    AsyncPrivateKeyMaxSignatureLen,
    AsyncPrivateKeySign,
    nullptr /* sign_digest */,
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
                           bssl::UniquePtr<EVP_PKEY> *out_pkey) {
  const TestConfig *config = GetTestConfig(ssl);

  if (!config->digest_prefs.empty()) {
    std::unique_ptr<char, Free<char>> digest_prefs(
        strdup(config->digest_prefs.c_str()));
    std::vector<int> digest_list;

    for (;;) {
      char *token =
          strtok(digest_list.empty() ? digest_prefs.get() : nullptr, ",");
      if (token == nullptr) {
        break;
      }

      digest_list.push_back(EVP_MD_type(EVP_get_digestbyname(token)));
    }

    if (!SSL_set_private_key_digest_prefs(ssl, digest_list.data(),
                                          digest_list.size())) {
      return false;
    }
  }

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
  if (!config->cert_file.empty()) {
    *out_x509 = LoadCertificate(config->cert_file.c_str());
    if (!*out_x509) {
      return false;
    }
  }
  if (!config->ocsp_response.empty() &&
      !SSL_CTX_set_ocsp_response(ssl->ctx,
                                 (const uint8_t *)config->ocsp_response.data(),
                                 config->ocsp_response.size())) {
    return false;
  }
  return true;
}

static bool InstallCertificate(SSL *ssl) {
  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &pkey)) {
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

  return true;
}

static int SelectCertificateCallback(const struct ssl_early_callback_ctx *ctx) {
  const TestConfig *config = GetTestConfig(ctx->ssl);
  GetTestState(ctx->ssl)->early_callback_called = true;

  if (!config->expected_server_name.empty()) {
    const uint8_t *extension_data;
    size_t extension_len;
    CBS extension, server_name_list, host_name;
    uint8_t name_type;

    if (!SSL_early_callback_ctx_extension_get(ctx, TLSEXT_TYPE_server_name,
                                              &extension_data,
                                              &extension_len)) {
      fprintf(stderr, "Could not find server_name extension.\n");
      return -1;
    }

    CBS_init(&extension, extension_data, extension_len);
    if (!CBS_get_u16_length_prefixed(&extension, &server_name_list) ||
        CBS_len(&extension) != 0 ||
        !CBS_get_u8(&server_name_list, &name_type) ||
        name_type != TLSEXT_NAMETYPE_host_name ||
        !CBS_get_u16_length_prefixed(&server_name_list, &host_name) ||
        CBS_len(&server_name_list) != 0) {
      fprintf(stderr, "Could not decode server_name extension.\n");
      return -1;
    }

    if (!CBS_mem_equal(&host_name,
                       (const uint8_t*)config->expected_server_name.data(),
                       config->expected_server_name.size())) {
      fprintf(stderr, "Server name mismatch.\n");
    }
  }

  if (config->fail_early_callback) {
    return -1;
  }

  // Install the certificate in the early callback.
  if (config->use_early_callback) {
    if (config->async) {
      // Install the certificate asynchronously.
      return 0;
    }
    if (!InstallCertificate(ctx->ssl)) {
      return -1;
    }
  }
  return 1;
}

static int ClientCertCallback(SSL *ssl, X509 **out_x509, EVP_PKEY **out_pkey) {
  if (GetTestConfig(ssl)->async && !GetTestState(ssl)->cert_ready) {
    return -1;
  }

  bssl::UniquePtr<X509> x509;
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!GetCertificate(ssl, &x509, &pkey)) {
    return -1;
  }

  // Return zero for no certificate.
  if (!x509) {
    return 0;
  }

  // Asynchronous private keys are not supported with client_cert_cb.
  *out_x509 = x509.release();
  *out_pkey = pkey.release();
  return 1;
}

static int VerifySucceed(X509_STORE_CTX *store_ctx, void *arg) {
  SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(store_ctx,
      SSL_get_ex_data_X509_STORE_CTX_idx());
  const TestConfig *config = GetTestConfig(ssl);

  if (!config->expected_ocsp_response.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_ocsp_response(ssl, &data, &len);
    if (len == 0) {
      fprintf(stderr, "OCSP response not available in verify callback\n");
      return 0;
    }
  }

  return 1;
}

static int VerifyFail(X509_STORE_CTX *store_ctx, void *arg) {
  store_ctx->error = X509_V_ERR_APPLICATION_VERIFICATION;
  return 0;
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
       memcmp(config->expected_advertised_alpn.data(),
              in, inlen) != 0)) {
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
  memcpy(out_psk, config->psk.data(), config->psk.size());
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

  memcpy(out_psk, config->psk.data(), config->psk.size());
  return config->psk.size();
}

static void CurrentTimeCallback(const SSL *ssl, timeval *out_clock) {
  *out_clock = PacketedBioGetClock(GetTestState(ssl)->packeted_bio);
}

static void ChannelIdCallback(SSL *ssl, EVP_PKEY **out_pkey) {
  *out_pkey = GetTestState(ssl)->channel_id.release();
}

static int CertCallback(SSL *ssl, void *arg) {
  const TestConfig *config = GetTestConfig(ssl);

  // Check the CertificateRequest metadata is as expected.
  //
  // TODO(davidben): Test |SSL_get_client_CA_list|.
  if (!SSL_is_server(ssl) &&
      !config->expected_certificate_types.empty()) {
    const uint8_t *certificate_types;
    size_t certificate_types_len =
        SSL_get0_certificate_types(ssl, &certificate_types);
    if (certificate_types_len != config->expected_certificate_types.size() ||
        memcmp(certificate_types,
               config->expected_certificate_types.data(),
               certificate_types_len) != 0) {
      fprintf(stderr, "certificate types mismatch\n");
      return 0;
    }
  }

  // The certificate will be installed via other means.
  if (!config->async || config->use_early_callback ||
      config->use_old_client_cert_callback) {
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

static SSL_SESSION *GetSessionCallback(SSL *ssl, uint8_t *data, int len,
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

static int DDoSCallback(const struct ssl_early_callback_ctx *early_context) {
  const TestConfig *config = GetTestConfig(early_context->ssl);
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
    GetTestState(ssl)->handshake_done = true;

    // Callbacks may be called again on a new handshake.
    GetTestState(ssl)->ticket_decrypt_done = false;
    GetTestState(ssl)->alpn_select_done = false;
  }
}

static int NewSessionCallback(SSL *ssl, SSL_SESSION *session) {
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
    memcpy(key_name, kZeros, sizeof(kZeros));
    RAND_bytes(iv, 16);
  } else if (memcmp(key_name, kZeros, 16) != 0) {
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
      memcmp(contents, kCustomExtensionContents, contents_len) != 0) {
    *out_alert_value = SSL_AD_DECODE_ERROR;
    return 0;
  }

  return 1;
}

// Connect returns a new socket connected to localhost on |port| or -1 on
// error.
static int Connect(uint16_t port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
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
  sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  if (!inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr)) {
    PrintSocketError("inet_pton");
    closesocket(sock);
    return -1;
  }
  if (connect(sock, reinterpret_cast<const sockaddr*>(&sin),
              sizeof(sin)) != 0) {
    PrintSocketError("connect");
    closesocket(sock);
    return -1;
  }
  return sock;
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

static bssl::UniquePtr<SSL_CTX> SetupCtx(const TestConfig *config) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(
      config->is_dtls ? DTLS_method() : TLS_method()));
  if (!ssl_ctx) {
    return nullptr;
  }

  // Enable TLS 1.3 for tests.
  if (!config->is_dtls &&
      !SSL_CTX_set_max_proto_version(ssl_ctx.get(), TLS1_3_VERSION)) {
    return nullptr;
  }

  std::string cipher_list = "ALL";
  if (!config->cipher.empty()) {
    cipher_list = config->cipher;
    SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
  }
  if (!SSL_CTX_set_cipher_list(ssl_ctx.get(), cipher_list.c_str())) {
    return nullptr;
  }

  if (!config->cipher_tls10.empty() &&
      !SSL_CTX_set_cipher_list_tls10(ssl_ctx.get(),
                                     config->cipher_tls10.c_str())) {
    return nullptr;
  }
  if (!config->cipher_tls11.empty() &&
      !SSL_CTX_set_cipher_list_tls11(ssl_ctx.get(),
                                     config->cipher_tls11.c_str())) {
    return nullptr;
  }

  bssl::UniquePtr<DH> dh(DH_get_2048_256(NULL));
  if (!dh) {
    return nullptr;
  }

  if (config->use_sparse_dh_prime) {
    // This prime number is 2^1024 + 643 – a value just above a power of two.
    // Because of its form, values modulo it are essentially certain to be one
    // byte shorter. This is used to test padding of these values.
    if (BN_hex2bn(
            &dh->p,
            "1000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000028"
            "3") == 0 ||
        !BN_set_word(dh->g, 2)) {
      return nullptr;
    }
    BN_free(dh->q);
    dh->q = NULL;
    dh->priv_length = 0;
  }

  if (!SSL_CTX_set_tmp_dh(ssl_ctx.get(), dh.get())) {
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

  SSL_CTX_enable_tls_channel_id(ssl_ctx.get());
  SSL_CTX_set_channel_id_cb(ssl_ctx.get(), ChannelIdCallback);

  if (config->is_dtls) {
    SSL_CTX_set_current_time_cb(ssl_ctx.get(), CurrentTimeCallback);
  }

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

  if (config->verify_fail) {
    SSL_CTX_set_cert_verify_callback(ssl_ctx.get(), VerifyFail, NULL);
  } else {
    SSL_CTX_set_cert_verify_callback(ssl_ctx.get(), VerifySucceed, NULL);
  }

  if (!config->signed_cert_timestamps.empty() &&
      !SSL_CTX_set_signed_cert_timestamp_list(
          ssl_ctx.get(), (const uint8_t *)config->signed_cert_timestamps.data(),
          config->signed_cert_timestamps.size())) {
    return nullptr;
  }

  if (config->use_null_client_ca_list) {
    SSL_CTX_set_client_CA_list(ssl_ctx.get(), nullptr);
  }

  if (config->enable_grease) {
    SSL_CTX_set_grease_enabled(ssl_ctx.get(), 1);
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
      // The handshake will resume without a second call to the early callback.
      return InstallCertificate(ssl);
    case SSL_ERROR_WANT_PRIVATE_KEY_OPERATION:
      test_state->private_key_retries++;
      return true;
    default:
      return false;
  }
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
    ret = config->peek_then_read ? SSL_peek(ssl, out, max_out)
                                 : SSL_read(ssl, out, max_out);
    if (config->async) {
      AsyncBioEnforceWriteQuota(test_state->async_bio, true);
    }
  } while (config->async && RetryAsync(ssl, ret));

  if (config->peek_then_read && ret > 0) {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[static_cast<size_t>(ret)]);

    // SSL_peek should synchronously return the same data.
    int ret2 = SSL_peek(ssl, buf.get(), ret);
    if (ret2 != ret ||
        memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "First and second SSL_peek did not match.\n");
      return -1;
    }

    // SSL_read should synchronously return the same data and consume it.
    ret2 = SSL_read(ssl, buf.get(), ret);
    if (ret2 != ret ||
        memcmp(buf.get(), out, ret) != 0) {
      fprintf(stderr, "SSL_peek and SSL_read did not match.\n");
      return -1;
    }
  }

  return ret;
}

// WriteAll writes |in_len| bytes from |in| to |ssl|, resolving any asynchronous
// operations. It returns the result of the final |SSL_write| call.
static int WriteAll(SSL *ssl, const uint8_t *in, size_t in_len) {
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

// CheckHandshakeProperties checks, immediately after |ssl| completes its
// initial handshake (or False Starts), whether all the properties are
// consistent with the test configuration and invariants.
static bool CheckHandshakeProperties(SSL *ssl, bool is_resume) {
  const TestConfig *config = GetTestConfig(ssl);

  if (SSL_get_current_cipher(ssl) == nullptr) {
    fprintf(stderr, "null cipher after handshake\n");
    return false;
  }

  if (is_resume &&
      (!!SSL_session_reused(ssl) == config->expect_session_miss)) {
    fprintf(stderr, "session was%s reused\n",
            SSL_session_reused(ssl) ? "" : " not");
    return false;
  }

  bool expect_handshake_done = is_resume || !config->false_start;
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

  if (config->is_server && !GetTestState(ssl)->early_callback_called) {
    fprintf(stderr, "early callback not called\n");
    return false;
  }

  if (!config->expected_server_name.empty()) {
    const char *server_name =
        SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (server_name != config->expected_server_name) {
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
        memcmp(next_proto, config->expected_next_proto.data(),
               next_proto_len) != 0) {
      fprintf(stderr, "negotiated next proto mismatch\n");
      return false;
    }
  }

  if (!config->expected_alpn.empty()) {
    const uint8_t *alpn_proto;
    unsigned alpn_proto_len;
    SSL_get0_alpn_selected(ssl, &alpn_proto, &alpn_proto_len);
    if (alpn_proto_len != config->expected_alpn.size() ||
        memcmp(alpn_proto, config->expected_alpn.data(),
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
        memcmp(config->expected_channel_id.data(),
               channel_id, 64) != 0) {
      fprintf(stderr, "channel id mismatch\n");
      return false;
    }
  }

  if (config->expect_extended_master_secret) {
    if (!SSL_get_extms_support(ssl)) {
      fprintf(stderr, "No EMS for connection when expected");
      return false;
    }
  }

  if (!config->expected_ocsp_response.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_ocsp_response(ssl, &data, &len);
    if (config->expected_ocsp_response.size() != len ||
        memcmp(config->expected_ocsp_response.data(), data, len) != 0) {
      fprintf(stderr, "OCSP response mismatch\n");
      return false;
    }
  }

  if (!config->expected_signed_cert_timestamps.empty()) {
    const uint8_t *data;
    size_t len;
    SSL_get0_signed_cert_timestamp_list(ssl, &data, &len);
    if (config->expected_signed_cert_timestamps.size() != len ||
        memcmp(config->expected_signed_cert_timestamps.data(),
               data, len) != 0) {
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

  if (config->expect_dhe_group_size != 0) {
    unsigned dhe_group_size = SSL_get_dhe_group_size(ssl);
    if (static_cast<unsigned>(config->expect_dhe_group_size) !=
        dhe_group_size) {
      fprintf(stderr, "dhe_group_size was %u, wanted %d\n", dhe_group_size,
              config->expect_dhe_group_size);
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

  return true;
}

// DoExchange runs a test SSL exchange against the peer. On success, it returns
// true and sets |*out_session| to the negotiated SSL session. If the test is a
// resumption attempt, |is_resume| is true and |session| is the session from the
// previous exchange.
static bool DoExchange(bssl::UniquePtr<SSL_SESSION> *out_session,
                       SSL_CTX *ssl_ctx, const TestConfig *config,
                       bool is_resume, SSL_SESSION *session) {
  bssl::UniquePtr<SSL> ssl(SSL_new(ssl_ctx));
  if (!ssl) {
    return false;
  }

  if (!SetTestConfig(ssl.get(), config) ||
      !SetTestState(ssl.get(), std::unique_ptr<TestState>(new TestState))) {
    return false;
  }

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
  SSL_set_cert_cb(ssl.get(), CertCallback, nullptr);
  if (config->require_any_client_certificate) {
    SSL_set_verify(ssl.get(), SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                   NULL);
  }
  if (config->verify_peer) {
    SSL_set_verify(ssl.get(), SSL_VERIFY_PEER, NULL);
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
    SSL_enable_tls_channel_id(ssl.get());
  }
  if (!config->send_channel_id.empty()) {
    SSL_enable_tls_channel_id(ssl.get());
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
  if (config->enable_ocsp_stapling &&
      !SSL_enable_ocsp_stapling(ssl.get())) {
    return false;
  }
  if (config->enable_signed_cert_timestamps &&
      !SSL_enable_signed_cert_timestamps(ssl.get())) {
    return false;
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
  if (config->disable_npn) {
    SSL_set_options(ssl.get(), SSL_OP_DISABLE_NPN);
  }
  if (config->p384_only) {
    int nid = NID_secp384r1;
    if (!SSL_set1_curves(ssl.get(), &nid, 1)) {
      return false;
    }
  }
  if (config->enable_all_curves) {
    static const int kAllCurves[] = {
      NID_X9_62_prime256v1, NID_secp384r1, NID_secp521r1, NID_X25519,
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
    bssl::UniquePtr<BIO> packeted = PacketedBioCreate(!config->async);
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

  int ret;
  if (config->implicit_handshake) {
    if (config->is_server) {
      SSL_set_accept_state(ssl.get());
    } else {
      SSL_set_connect_state(ssl.get());
    }
  } else {
    do {
      if (config->is_server) {
        ret = SSL_accept(ssl.get());
      } else {
        ret = SSL_connect(ssl.get());
      }
    } while (config->async && RetryAsync(ssl.get(), ret));
    if (ret != 1 ||
        !CheckHandshakeProperties(ssl.get(), is_resume)) {
      return false;
    }

    // Reset the state to assert later that the callback isn't called in
    // renegotations.
    GetTestState(ssl.get())->got_new_session = false;
  }

  if (config->export_keying_material > 0) {
    std::vector<uint8_t> result(
        static_cast<size_t>(config->export_keying_material));
    if (!SSL_export_keying_material(
            ssl.get(), result.data(), result.size(),
            config->export_label.data(), config->export_label.size(),
            reinterpret_cast<const uint8_t*>(config->export_context.data()),
            config->export_context.size(), config->use_export_context)) {
      fprintf(stderr, "failed to export keying material\n");
      return false;
    }
    if (WriteAll(ssl.get(), result.data(), result.size()) < 0) {
      return false;
    }
  }

  if (config->tls_unique) {
    uint8_t tls_unique[16];
    size_t tls_unique_len;
    if (!SSL_get_tls_unique(ssl.get(), tls_unique, &tls_unique_len,
                            sizeof(tls_unique))) {
      fprintf(stderr, "failed to get tls-unique\n");
      return false;
    }

    if (tls_unique_len != 12) {
      fprintf(stderr, "expected 12 bytes of tls-unique but got %u",
              static_cast<unsigned>(tls_unique_len));
      return false;
    }

    if (WriteAll(ssl.get(), tls_unique, tls_unique_len) < 0) {
      return false;
    }
  }

  if (config->send_alert) {
    if (DoSendFatalAlert(ssl.get(), SSL_AD_DECOMPRESSION_FAILURE) < 0) {
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
    memset(buf.get(), 0x42, kBufLen);
    static const size_t kRecordSizes[] = {
        0, 1, 255, 256, 257, 16383, 16384, 16385, 32767, 32768, 32769};
    for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kRecordSizes); i++) {
      const size_t len = kRecordSizes[i];
      if (len > kBufLen) {
        fprintf(stderr, "Bad kRecordSizes value.\n");
        return false;
      }
      if (WriteAll(ssl.get(), buf.get(), len) < 0) {
        return false;
      }
    }
  } else {
    if (config->shim_writes_first) {
      if (WriteAll(ssl.get(), reinterpret_cast<const uint8_t *>("hello"),
                   5) < 0) {
        return false;
      }
    }
    if (!config->shim_shuts_down) {
      for (;;) {
        static const size_t kBufLen = 16384;
        std::unique_ptr<uint8_t[]> buf(new uint8_t[kBufLen]);

        // Read only 512 bytes at a time in TLS to ensure records may be
        // returned in multiple reads.
        int n = DoRead(ssl.get(), buf.get(), config->is_dtls ? kBufLen : 512);
        int err = SSL_get_error(ssl.get(), n);
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

        // After a successful read, with or without False Start, the handshake
        // must be complete.
        if (!GetTestState(ssl.get())->handshake_done) {
          fprintf(stderr, "handshake was not completed after SSL_read\n");
          return false;
        }

        for (int i = 0; i < n; i++) {
          buf[i] ^= 0xff;
        }
        if (WriteAll(ssl.get(), buf.get(), n) < 0) {
          return false;
        }
      }
    }
  }

  if (!config->is_server && !config->false_start &&
      !config->implicit_handshake &&
      // Session tickets are sent post-handshake in TLS 1.3.
      GetProtocolVersion(ssl.get()) < TLS1_3_VERSION &&
      GetTestState(ssl.get())->got_new_session) {
    fprintf(stderr, "new session was established after the handshake\n");
    return false;
  }

  if (GetProtocolVersion(ssl.get()) >= TLS1_3_VERSION && !config->is_server) {
    bool expect_new_session =
        !config->expect_no_session && !config->shim_shuts_down;
    if (expect_new_session != GetTestState(ssl.get())->got_new_session) {
      fprintf(stderr,
              "new session was%s cached, but we expected the opposite\n",
              GetTestState(ssl.get())->got_new_session ? "" : " not");
      return false;
    }
  }

  if (out_session) {
    *out_session = std::move(GetTestState(ssl.get())->new_session);
  }

  ret = DoShutdown(ssl.get());

  if (config->shim_shuts_down && config->check_close_notify) {
    // We initiate shutdown, so |SSL_shutdown| will return in two stages. First
    // it returns zero when our close_notify is sent, then one when the peer's
    // is received.
    if (ret != 0) {
      fprintf(stderr, "Unexpected SSL_shutdown result: %d != 0\n", ret);
      return false;
    }
    ret = DoShutdown(ssl.get());
  }

  if (ret != 1) {
    fprintf(stderr, "Unexpected SSL_shutdown result: %d != 1\n", ret);
    return false;
  }

  if (SSL_total_renegotiations(ssl.get()) !=
      config->expect_total_renegotiations) {
    fprintf(stderr, "Expected %d renegotiations, got %d\n",
            config->expect_total_renegotiations,
            SSL_total_renegotiations(ssl.get()));
    return false;
  }

  return true;
}

class StderrDelimiter {
 public:
  ~StderrDelimiter() { fprintf(stderr, "--- DONE ---\n"); }
};

static int Main(int argc, char **argv) {
  // To distinguish ASan's output from ours, add a trailing message to stderr.
  // Anything following this line will be considered an error.
  StderrDelimiter delimiter;

#if defined(OPENSSL_WINDOWS)
  /* Initialize Winsock. */
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

  TestConfig config;
  if (!ParseConfig(argc - 1, argv + 1, &config)) {
    return Usage(argv[0]);
  }

  bssl::UniquePtr<SSL_CTX> ssl_ctx = SetupCtx(&config);
  if (!ssl_ctx) {
    ERR_print_errors_fp(stderr);
    return 1;
  }

  bssl::UniquePtr<SSL_SESSION> session;
  for (int i = 0; i < config.resume_count + 1; i++) {
    bool is_resume = i > 0;
    if (is_resume && !config.is_server && !session) {
      fprintf(stderr, "No session to offer.\n");
      return 1;
    }

    bssl::UniquePtr<SSL_SESSION> offer_session = std::move(session);
    if (!DoExchange(&session, ssl_ctx.get(), &config, is_resume,
                    offer_session.get())) {
      fprintf(stderr, "Connection %d failed.\n", i + 1);
      ERR_print_errors_fp(stderr);
      return 1;
    }
  }

  return 0;
}

}  // namespace bssl

int main(int argc, char **argv) {
  return bssl::Main(argc, argv);
}
