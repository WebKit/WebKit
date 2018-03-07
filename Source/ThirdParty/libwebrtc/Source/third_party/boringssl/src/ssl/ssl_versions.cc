/* Copyright (c) 2017, Google Inc.
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

#include <openssl/ssl.h>

#include <assert.h>

#include <openssl/bytestring.h>
#include <openssl/err.h>

#include "internal.h"
#include "../crypto/internal.h"


namespace bssl {

bool ssl_protocol_version_from_wire(uint16_t *out, uint16_t version) {
  switch (version) {
    case SSL3_VERSION:
    case TLS1_VERSION:
    case TLS1_1_VERSION:
    case TLS1_2_VERSION:
      *out = version;
      return true;

    case TLS1_3_DRAFT_VERSION:
    case TLS1_3_DRAFT21_VERSION:
    case TLS1_3_DRAFT22_VERSION:
    case TLS1_3_EXPERIMENT_VERSION:
    case TLS1_3_EXPERIMENT2_VERSION:
    case TLS1_3_EXPERIMENT3_VERSION:
      *out = TLS1_3_VERSION;
      return true;

    case DTLS1_VERSION:
      // DTLS 1.0 is analogous to TLS 1.1, not TLS 1.0.
      *out = TLS1_1_VERSION;
      return true;

    case DTLS1_2_VERSION:
      *out = TLS1_2_VERSION;
      return true;

    default:
      return false;
  }
}

// The follow arrays are the supported versions for TLS and DTLS, in order of
// decreasing preference.

static const uint16_t kTLSVersions[] = {
    TLS1_3_DRAFT22_VERSION,
    TLS1_3_EXPERIMENT3_VERSION,
    TLS1_3_EXPERIMENT2_VERSION,
    TLS1_3_EXPERIMENT_VERSION,
    TLS1_3_DRAFT_VERSION,
    TLS1_3_DRAFT21_VERSION,
    TLS1_2_VERSION,
    TLS1_1_VERSION,
    TLS1_VERSION,
    SSL3_VERSION,
};

static const uint16_t kDTLSVersions[] = {
    DTLS1_2_VERSION,
    DTLS1_VERSION,
};

static void get_method_versions(const SSL_PROTOCOL_METHOD *method,
                                const uint16_t **out, size_t *out_num) {
  if (method->is_dtls) {
    *out = kDTLSVersions;
    *out_num = OPENSSL_ARRAY_SIZE(kDTLSVersions);
  } else {
    *out = kTLSVersions;
    *out_num = OPENSSL_ARRAY_SIZE(kTLSVersions);
  }
}

static bool method_supports_version(const SSL_PROTOCOL_METHOD *method,
                                    uint16_t version) {
  const uint16_t *versions;
  size_t num_versions;
  get_method_versions(method, &versions, &num_versions);
  for (size_t i = 0; i < num_versions; i++) {
    if (versions[i] == version) {
      return true;
    }
  }
  return false;
}

// The following functions map between API versions and wire versions. The
// public API works on wire versions, except that TLS 1.3 draft versions all
// appear as TLS 1.3. This will get collapsed back down when TLS 1.3 is
// finalized.

static const char *ssl_version_to_string(uint16_t version) {
  switch (version) {
    case TLS1_3_DRAFT_VERSION:
    case TLS1_3_DRAFT21_VERSION:
    case TLS1_3_DRAFT22_VERSION:
    case TLS1_3_EXPERIMENT_VERSION:
    case TLS1_3_EXPERIMENT2_VERSION:
    case TLS1_3_EXPERIMENT3_VERSION:
      return "TLSv1.3";

    case TLS1_2_VERSION:
      return "TLSv1.2";

    case TLS1_1_VERSION:
      return "TLSv1.1";

    case TLS1_VERSION:
      return "TLSv1";

    case SSL3_VERSION:
      return "SSLv3";

    case DTLS1_VERSION:
      return "DTLSv1";

    case DTLS1_2_VERSION:
      return "DTLSv1.2";

    default:
      return "unknown";
  }
}

static uint16_t wire_version_to_api(uint16_t version) {
  switch (version) {
    // Report TLS 1.3 draft versions as TLS 1.3 in the public API.
    case TLS1_3_DRAFT_VERSION:
    case TLS1_3_DRAFT21_VERSION:
    case TLS1_3_DRAFT22_VERSION:
    case TLS1_3_EXPERIMENT_VERSION:
    case TLS1_3_EXPERIMENT2_VERSION:
    case TLS1_3_EXPERIMENT3_VERSION:
      return TLS1_3_VERSION;
    default:
      return version;
  }
}

// api_version_to_wire maps |version| to some representative wire version. In
// particular, it picks an arbitrary TLS 1.3 representative. This should only be
// used in context where that does not matter.
static bool api_version_to_wire(uint16_t *out, uint16_t version) {
  if (version == TLS1_3_DRAFT_VERSION ||
      version == TLS1_3_DRAFT21_VERSION ||
      version == TLS1_3_DRAFT22_VERSION ||
      version == TLS1_3_EXPERIMENT_VERSION ||
      version == TLS1_3_EXPERIMENT2_VERSION ||
      version == TLS1_3_EXPERIMENT3_VERSION) {
    return false;
  }
  if (version == TLS1_3_VERSION) {
    version = TLS1_3_DRAFT_VERSION;
  }

  // Check it is a real protocol version.
  uint16_t unused;
  if (!ssl_protocol_version_from_wire(&unused, version)) {
    return false;
  }

  *out = version;
  return true;
}

static bool set_version_bound(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                              uint16_t version) {
  if (!api_version_to_wire(&version, version) ||
      !method_supports_version(method, version) ||
      !ssl_protocol_version_from_wire(out, version)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_SSL_VERSION);
    return false;
  }

  return true;
}

static bool set_min_version(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                            uint16_t version) {
  // Zero is interpreted as the default minimum version.
  if (version == 0) {
    // SSL 3.0 is disabled by default and TLS 1.0 does not exist in DTLS.
    *out = method->is_dtls ? TLS1_1_VERSION : TLS1_VERSION;
    return true;
  }

  return set_version_bound(method, out, version);
}

static bool set_max_version(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                            uint16_t version) {
  // Zero is interpreted as the default maximum version.
  if (version == 0) {
    *out = TLS1_2_VERSION;
    return true;
  }

  return set_version_bound(method, out, version);
}

const struct {
  uint16_t version;
  uint32_t flag;
} kProtocolVersions[] = {
    {SSL3_VERSION, SSL_OP_NO_SSLv3},
    {TLS1_VERSION, SSL_OP_NO_TLSv1},
    {TLS1_1_VERSION, SSL_OP_NO_TLSv1_1},
    {TLS1_2_VERSION, SSL_OP_NO_TLSv1_2},
    {TLS1_3_VERSION, SSL_OP_NO_TLSv1_3},
};

bool ssl_get_version_range(const SSL *ssl, uint16_t *out_min_version,
                           uint16_t *out_max_version) {
  // For historical reasons, |SSL_OP_NO_DTLSv1| aliases |SSL_OP_NO_TLSv1|, but
  // DTLS 1.0 should be mapped to TLS 1.1.
  uint32_t options = ssl->options;
  if (SSL_is_dtls(ssl)) {
    options &= ~SSL_OP_NO_TLSv1_1;
    if (options & SSL_OP_NO_DTLSv1) {
      options |= SSL_OP_NO_TLSv1_1;
    }
  }

  uint16_t min_version = ssl->conf_min_version;
  uint16_t max_version = ssl->conf_max_version;

  // OpenSSL's API for controlling versions entails blacklisting individual
  // protocols. This has two problems. First, on the client, the protocol can
  // only express a contiguous range of versions. Second, a library consumer
  // trying to set a maximum version cannot disable protocol versions that get
  // added in a future version of the library.
  //
  // To account for both of these, OpenSSL interprets the client-side bitmask
  // as a min/max range by picking the lowest contiguous non-empty range of
  // enabled protocols. Note that this means it is impossible to set a maximum
  // version of the higest supported TLS version in a future-proof way.
  bool any_enabled = false;
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kProtocolVersions); i++) {
    // Only look at the versions already enabled.
    if (min_version > kProtocolVersions[i].version) {
      continue;
    }
    if (max_version < kProtocolVersions[i].version) {
      break;
    }

    if (!(options & kProtocolVersions[i].flag)) {
      // The minimum version is the first enabled version.
      if (!any_enabled) {
        any_enabled = true;
        min_version = kProtocolVersions[i].version;
      }
      continue;
    }

    // If there is a disabled version after the first enabled one, all versions
    // after it are implicitly disabled.
    if (any_enabled) {
      max_version = kProtocolVersions[i-1].version;
      break;
    }
  }

  if (!any_enabled) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SUPPORTED_VERSIONS_ENABLED);
    return false;
  }

  *out_min_version = min_version;
  *out_max_version = max_version;
  return true;
}

static uint16_t ssl_version(const SSL *ssl) {
  // In early data, we report the predicted version.
  if (SSL_in_early_data(ssl) && !ssl->server) {
    return ssl->s3->hs->early_session->ssl_version;
  }
  return ssl->version;
}

uint16_t ssl_protocol_version(const SSL *ssl) {
  assert(ssl->s3->have_version);
  uint16_t version;
  if (!ssl_protocol_version_from_wire(&version, ssl->version)) {
    // |ssl->version| will always be set to a valid version.
    assert(0);
    return 0;
  }

  return version;
}

bool ssl_supports_version(SSL_HANDSHAKE *hs, uint16_t version) {
  SSL *const ssl = hs->ssl;
  uint16_t protocol_version;
  if (!method_supports_version(ssl->method, version) ||
      !ssl_protocol_version_from_wire(&protocol_version, version) ||
      hs->min_version > protocol_version ||
      protocol_version > hs->max_version) {
    return false;
  }

  // TLS 1.3 variants must additionally match |tls13_variant|.
  if (protocol_version != TLS1_3_VERSION ||
      (ssl->tls13_variant == tls13_experiment &&
       version == TLS1_3_EXPERIMENT_VERSION) ||
      (ssl->tls13_variant == tls13_experiment2 &&
       version == TLS1_3_EXPERIMENT2_VERSION) ||
      (ssl->tls13_variant == tls13_experiment3 &&
       version == TLS1_3_EXPERIMENT3_VERSION) ||
      (ssl->tls13_variant == tls13_draft21 &&
       version == TLS1_3_DRAFT21_VERSION) ||
      (ssl->tls13_variant == tls13_draft22 &&
       version == TLS1_3_DRAFT22_VERSION) ||
      (ssl->tls13_variant == tls13_default &&
       version == TLS1_3_DRAFT_VERSION)) {
    return true;
  }

  // The server, when not configured at |tls13_default|, should additionally
  // enable all variants, except draft-21 which is implemented solely for QUIC
  // interop testing and will not be deployed, and draft-22 which will be
  // enabled once the draft is finalized and ready to be deployed in Chrome.
  // Currently, this is to implement the draft-18 vs. experiments field trials.
  // In the future, this will be to transition cleanly to a final draft-22
  // which hopefully includes the deployability fixes.
  if (ssl->server &&
      ssl->tls13_variant != tls13_default &&
      version != TLS1_3_DRAFT21_VERSION &&
      version != TLS1_3_DRAFT22_VERSION) {
    return true;
  }

  return false;
}

bool ssl_add_supported_versions(SSL_HANDSHAKE *hs, CBB *cbb) {
  const uint16_t *versions;
  size_t num_versions;
  get_method_versions(hs->ssl->method, &versions, &num_versions);
  for (size_t i = 0; i < num_versions; i++) {
    if (ssl_supports_version(hs, versions[i]) &&
        !CBB_add_u16(cbb, versions[i])) {
      return false;
    }
  }
  return true;
}

bool ssl_negotiate_version(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                           uint16_t *out_version, const CBS *peer_versions) {
  const uint16_t *versions;
  size_t num_versions;
  get_method_versions(hs->ssl->method, &versions, &num_versions);
  for (size_t i = 0; i < num_versions; i++) {
    if (!ssl_supports_version(hs, versions[i])) {
      continue;
    }

    CBS copy = *peer_versions;
    while (CBS_len(&copy) != 0) {
      uint16_t version;
      if (!CBS_get_u16(&copy, &version)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        *out_alert = SSL_AD_DECODE_ERROR;
        return false;
      }

      if (version == versions[i]) {
        *out_version = version;
        return true;
      }
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_PROTOCOL);
  *out_alert = SSL_AD_PROTOCOL_VERSION;
  return false;
}

bool ssl_is_draft21(uint16_t version) {
  return version == TLS1_3_DRAFT21_VERSION || version == TLS1_3_DRAFT22_VERSION;
}

bool ssl_is_draft22(uint16_t version) {
  return version == TLS1_3_DRAFT22_VERSION;
}

bool ssl_is_resumption_experiment(uint16_t version) {
  return version == TLS1_3_EXPERIMENT_VERSION ||
         version == TLS1_3_EXPERIMENT2_VERSION ||
         version == TLS1_3_EXPERIMENT3_VERSION ||
         version == TLS1_3_DRAFT22_VERSION;
}

bool ssl_is_resumption_variant(enum tls13_variant_t variant) {
  return variant == tls13_experiment || variant == tls13_experiment2 ||
         variant == tls13_experiment3 || variant == tls13_draft22;
}

bool ssl_is_resumption_client_ccs_experiment(uint16_t version) {
  return version == TLS1_3_EXPERIMENT_VERSION ||
         version == TLS1_3_EXPERIMENT2_VERSION ||
         version == TLS1_3_DRAFT22_VERSION;
}

bool ssl_is_resumption_record_version_experiment(uint16_t version) {
  return version == TLS1_3_EXPERIMENT2_VERSION ||
         version == TLS1_3_EXPERIMENT3_VERSION ||
         version == TLS1_3_DRAFT22_VERSION;
}

}  // namespace bssl

using namespace bssl;

int SSL_CTX_set_min_proto_version(SSL_CTX *ctx, uint16_t version) {
  return set_min_version(ctx->method, &ctx->conf_min_version, version);
}

int SSL_CTX_set_max_proto_version(SSL_CTX *ctx, uint16_t version) {
  return set_max_version(ctx->method, &ctx->conf_max_version, version);
}

int SSL_set_min_proto_version(SSL *ssl, uint16_t version) {
  return set_min_version(ssl->method, &ssl->conf_min_version, version);
}

int SSL_set_max_proto_version(SSL *ssl, uint16_t version) {
  return set_max_version(ssl->method, &ssl->conf_max_version, version);
}

int SSL_version(const SSL *ssl) {
  return wire_version_to_api(ssl_version(ssl));
}

const char *SSL_get_version(const SSL *ssl) {
  return ssl_version_to_string(ssl_version(ssl));
}

const char *SSL_SESSION_get_version(const SSL_SESSION *session) {
  return ssl_version_to_string(session->ssl_version);
}

uint16_t SSL_SESSION_get_protocol_version(const SSL_SESSION *session) {
  return wire_version_to_api(session->ssl_version);
}

int SSL_SESSION_set_protocol_version(SSL_SESSION *session, uint16_t version) {
  // This picks a representative TLS 1.3 version, but this API should only be
  // used on unit test sessions anyway.
  return api_version_to_wire(&session->ssl_version, version);
}
