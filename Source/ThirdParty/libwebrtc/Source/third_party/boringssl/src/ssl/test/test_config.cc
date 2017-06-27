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

#include "test_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include <openssl/base64.h>

namespace {

template <typename T>
struct Flag {
  const char *flag;
  T TestConfig::*member;
};

// FindField looks for the flag in |flags| that matches |flag|. If one is found,
// it returns a pointer to the corresponding field in |config|. Otherwise, it
// returns NULL.
template<typename T, size_t N>
T *FindField(TestConfig *config, const Flag<T> (&flags)[N], const char *flag) {
  for (size_t i = 0; i < N; i++) {
    if (strcmp(flag, flags[i].flag) == 0) {
      return &(config->*(flags[i].member));
    }
  }
  return NULL;
}

const Flag<bool> kBoolFlags[] = {
  { "-server", &TestConfig::is_server },
  { "-dtls", &TestConfig::is_dtls },
  { "-fallback-scsv", &TestConfig::fallback_scsv },
  { "-require-any-client-certificate",
    &TestConfig::require_any_client_certificate },
  { "-false-start", &TestConfig::false_start },
  { "-async", &TestConfig::async },
  { "-write-different-record-sizes",
    &TestConfig::write_different_record_sizes },
  { "-cbc-record-splitting", &TestConfig::cbc_record_splitting },
  { "-partial-write", &TestConfig::partial_write },
  { "-no-tls13", &TestConfig::no_tls13 },
  { "-no-tls12", &TestConfig::no_tls12 },
  { "-no-tls11", &TestConfig::no_tls11 },
  { "-no-tls1", &TestConfig::no_tls1 },
  { "-no-ssl3", &TestConfig::no_ssl3 },
  { "-enable-channel-id", &TestConfig::enable_channel_id },
  { "-shim-writes-first", &TestConfig::shim_writes_first },
  { "-expect-session-miss", &TestConfig::expect_session_miss },
  { "-decline-alpn", &TestConfig::decline_alpn },
  { "-expect-extended-master-secret",
    &TestConfig::expect_extended_master_secret },
  { "-enable-ocsp-stapling", &TestConfig::enable_ocsp_stapling },
  { "-enable-signed-cert-timestamps",
    &TestConfig::enable_signed_cert_timestamps },
  { "-implicit-handshake", &TestConfig::implicit_handshake },
  { "-use-early-callback", &TestConfig::use_early_callback },
  { "-fail-early-callback", &TestConfig::fail_early_callback },
  { "-install-ddos-callback", &TestConfig::install_ddos_callback },
  { "-fail-ddos-callback", &TestConfig::fail_ddos_callback },
  { "-fail-second-ddos-callback", &TestConfig::fail_second_ddos_callback },
  { "-fail-cert-callback", &TestConfig::fail_cert_callback },
  { "-handshake-never-done", &TestConfig::handshake_never_done },
  { "-use-export-context", &TestConfig::use_export_context },
  { "-tls-unique", &TestConfig::tls_unique },
  { "-expect-ticket-renewal", &TestConfig::expect_ticket_renewal },
  { "-expect-no-session", &TestConfig::expect_no_session },
  { "-expect-early-data-info", &TestConfig::expect_early_data_info },
  { "-use-ticket-callback", &TestConfig::use_ticket_callback },
  { "-renew-ticket", &TestConfig::renew_ticket },
  { "-enable-early-data", &TestConfig::enable_early_data },
  { "-enable-client-custom-extension",
    &TestConfig::enable_client_custom_extension },
  { "-enable-server-custom-extension",
    &TestConfig::enable_server_custom_extension },
  { "-custom-extension-skip", &TestConfig::custom_extension_skip },
  { "-custom-extension-fail-add", &TestConfig::custom_extension_fail_add },
  { "-check-close-notify", &TestConfig::check_close_notify },
  { "-shim-shuts-down", &TestConfig::shim_shuts_down },
  { "-verify-fail", &TestConfig::verify_fail },
  { "-verify-peer", &TestConfig::verify_peer },
  { "-expect-verify-result", &TestConfig::expect_verify_result },
  { "-renegotiate-once", &TestConfig::renegotiate_once },
  { "-renegotiate-freely", &TestConfig::renegotiate_freely },
  { "-renegotiate-ignore", &TestConfig::renegotiate_ignore },
  { "-p384-only", &TestConfig::p384_only },
  { "-enable-all-curves", &TestConfig::enable_all_curves },
  { "-use-old-client-cert-callback",
    &TestConfig::use_old_client_cert_callback },
  { "-send-alert", &TestConfig::send_alert },
  { "-peek-then-read", &TestConfig::peek_then_read },
  { "-enable-grease", &TestConfig::enable_grease },
  { "-use-exporter-between-reads", &TestConfig::use_exporter_between_reads },
  { "-retain-only-sha256-client-cert-initial",
    &TestConfig::retain_only_sha256_client_cert_initial },
  { "-retain-only-sha256-client-cert-resume",
    &TestConfig::retain_only_sha256_client_cert_resume },
  { "-expect-sha256-client-cert-initial",
    &TestConfig::expect_sha256_client_cert_initial },
  { "-expect-sha256-client-cert-resume",
    &TestConfig::expect_sha256_client_cert_resume },
  { "-read-with-unfinished-write", &TestConfig::read_with_unfinished_write },
  { "-expect-secure-renegotiation",
    &TestConfig::expect_secure_renegotiation },
  { "-expect-no-secure-renegotiation",
    &TestConfig::expect_no_secure_renegotiation },
  { "-expect-session-id", &TestConfig::expect_session_id },
  { "-expect-no-session-id", &TestConfig::expect_no_session_id },
  { "-expect-accept-early-data", &TestConfig::expect_accept_early_data },
  { "-expect-reject-early-data", &TestConfig::expect_reject_early_data },
  { "-no-op-extra-handshake", &TestConfig::no_op_extra_handshake },
  { "-handshake-twice", &TestConfig::handshake_twice },
  { "-allow-unknown-alpn-protos", &TestConfig::allow_unknown_alpn_protos },
  { "-enable-ed25519", &TestConfig::enable_ed25519 },
};

const Flag<std::string> kStringFlags[] = {
  { "-digest-prefs", &TestConfig::digest_prefs },
  { "-key-file", &TestConfig::key_file },
  { "-cert-file", &TestConfig::cert_file },
  { "-expect-server-name", &TestConfig::expected_server_name },
  { "-advertise-npn", &TestConfig::advertise_npn },
  { "-expect-next-proto", &TestConfig::expected_next_proto },
  { "-select-next-proto", &TestConfig::select_next_proto },
  { "-send-channel-id", &TestConfig::send_channel_id },
  { "-host-name", &TestConfig::host_name },
  { "-advertise-alpn", &TestConfig::advertise_alpn },
  { "-expect-alpn", &TestConfig::expected_alpn },
  { "-expect-advertised-alpn", &TestConfig::expected_advertised_alpn },
  { "-select-alpn", &TestConfig::select_alpn },
  { "-psk", &TestConfig::psk },
  { "-psk-identity", &TestConfig::psk_identity },
  { "-srtp-profiles", &TestConfig::srtp_profiles },
  { "-cipher", &TestConfig::cipher },
  { "-export-label", &TestConfig::export_label },
  { "-export-context", &TestConfig::export_context },
  { "-expect-peer-cert-file", &TestConfig::expect_peer_cert_file },
  { "-use-client-ca-list", &TestConfig::use_client_ca_list },
  { "-expect-client-ca-list", &TestConfig::expected_client_ca_list },
};

const Flag<std::string> kBase64Flags[] = {
  { "-expect-certificate-types", &TestConfig::expected_certificate_types },
  { "-expect-channel-id", &TestConfig::expected_channel_id },
  { "-expect-ocsp-response", &TestConfig::expected_ocsp_response },
  { "-expect-signed-cert-timestamps",
    &TestConfig::expected_signed_cert_timestamps },
  { "-ocsp-response", &TestConfig::ocsp_response },
  { "-signed-cert-timestamps", &TestConfig::signed_cert_timestamps },
  { "-ticket-key", &TestConfig::ticket_key },
};

const Flag<int> kIntFlags[] = {
  { "-port", &TestConfig::port },
  { "-resume-count", &TestConfig::resume_count },
  { "-min-version", &TestConfig::min_version },
  { "-max-version", &TestConfig::max_version },
  { "-mtu", &TestConfig::mtu },
  { "-export-keying-material", &TestConfig::export_keying_material },
  { "-expect-total-renegotiations", &TestConfig::expect_total_renegotiations },
  { "-expect-peer-signature-algorithm",
    &TestConfig::expect_peer_signature_algorithm },
  { "-expect-curve-id", &TestConfig::expect_curve_id },
  { "-initial-timeout-duration-ms", &TestConfig::initial_timeout_duration_ms },
  { "-max-cert-list", &TestConfig::max_cert_list },
  { "-expect-cipher-aes", &TestConfig::expect_cipher_aes },
  { "-expect-cipher-no-aes", &TestConfig::expect_cipher_no_aes },
  { "-resumption-delay", &TestConfig::resumption_delay },
  { "-max-send-fragment", &TestConfig::max_send_fragment },
  { "-read-size", &TestConfig::read_size },
  { "-expect-ticket-age-skew", &TestConfig::expect_ticket_age_skew },
};

const Flag<std::vector<int>> kIntVectorFlags[] = {
  { "-signing-prefs", &TestConfig::signing_prefs },
  { "-verify-prefs", &TestConfig::verify_prefs },
};

const char kInit[] = "-on-initial";
const char kResume[] = "-on-resume";

}  // namespace

bool ParseConfig(int argc, char **argv, bool is_resume,
                 TestConfig *out_config) {
  for (int i = 0; i < argc; i++) {
    bool skip = false;
    char *flag = argv[i];
    const char *prefix = is_resume ? kResume : kInit;
    const char *opposite = is_resume ? kInit : kResume;
    if (strncmp(flag, prefix, strlen(prefix)) == 0) {
      flag = flag + strlen(prefix);
      for (int j = 0; j < argc; j++) {
        if (strcmp(argv[j], flag) == 0) {
          fprintf(stderr, "Can't use default and prefixed arguments: %s\n",
                  flag);
          return false;
        }
      }
    } else if (strncmp(flag, opposite, strlen(opposite)) == 0) {
      flag = flag + strlen(opposite);
      skip = true;
    }

    bool *bool_field = FindField(out_config, kBoolFlags, flag);
    if (bool_field != NULL) {
      if (!skip) {
        *bool_field = true;
      }
      continue;
    }

    std::string *string_field = FindField(out_config, kStringFlags, flag);
    if (string_field != NULL) {
      i++;
      if (i >= argc) {
        fprintf(stderr, "Missing parameter\n");
        return false;
      }
      if (!skip) {
        string_field->assign(argv[i]);
      }
      continue;
    }

    std::string *base64_field = FindField(out_config, kBase64Flags, flag);
    if (base64_field != NULL) {
      i++;
      if (i >= argc) {
        fprintf(stderr, "Missing parameter\n");
        return false;
      }
      size_t len;
      if (!EVP_DecodedLength(&len, strlen(argv[i]))) {
        fprintf(stderr, "Invalid base64: %s\n", argv[i]);
        return false;
      }
      std::unique_ptr<uint8_t[]> decoded(new uint8_t[len]);
      if (!EVP_DecodeBase64(decoded.get(), &len, len,
                            reinterpret_cast<const uint8_t *>(argv[i]),
                            strlen(argv[i]))) {
        fprintf(stderr, "Invalid base64: %s\n", argv[i]);
        return false;
      }
      if (!skip) {
        base64_field->assign(reinterpret_cast<const char *>(decoded.get()),
                             len);
      }
      continue;
    }

    int *int_field = FindField(out_config, kIntFlags, flag);
    if (int_field) {
      i++;
      if (i >= argc) {
        fprintf(stderr, "Missing parameter\n");
        return false;
      }
      if (!skip) {
        *int_field = atoi(argv[i]);
      }
      continue;
    }

    std::vector<int> *int_vector_field =
        FindField(out_config, kIntVectorFlags, flag);
    if (int_vector_field) {
      i++;
      if (i >= argc) {
        fprintf(stderr, "Missing parameter\n");
        return false;
      }

      // Each instance of the flag adds to the list.
      if (!skip) {
        int_vector_field->push_back(atoi(argv[i]));
      }
      continue;
    }

    fprintf(stderr, "Unknown argument: %s\n", flag);
    return false;
  }

  return true;
}
