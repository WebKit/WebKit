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

#include <openssl/base.h>

#include <stdio.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include "internal.h"
#include "transport_common.h"


static const struct argument kArguments[] = {
    {
     "-connect", kRequiredArgument,
     "The hostname and port of the server to connect to, e.g. foo.com:443",
    },
    {
     "-cipher", kOptionalArgument,
     "An OpenSSL-style cipher suite string that configures the offered ciphers",
    },
    {
     "-max-version", kOptionalArgument,
     "The maximum acceptable protocol version",
    },
    {
     "-min-version", kOptionalArgument,
     "The minimum acceptable protocol version",
    },
    {
     "-server-name", kOptionalArgument,
     "The server name to advertise",
    },
    {
     "-select-next-proto", kOptionalArgument,
     "An NPN protocol to select if the server supports NPN",
    },
    {
     "-alpn-protos", kOptionalArgument,
     "A comma-separated list of ALPN protocols to advertise",
    },
    {
     "-fallback-scsv", kBooleanArgument,
     "Enable FALLBACK_SCSV",
    },
    {
     "-ocsp-stapling", kBooleanArgument,
     "Advertise support for OCSP stabling",
    },
    {
     "-signed-certificate-timestamps", kBooleanArgument,
     "Advertise support for signed certificate timestamps",
    },
    {
     "-channel-id-key", kOptionalArgument,
     "The key to use for signing a channel ID",
    },
    {
     "-false-start", kBooleanArgument,
     "Enable False Start",
    },
    { "-session-in", kOptionalArgument,
      "A file containing a session to resume.",
    },
    { "-session-out", kOptionalArgument,
      "A file to write the negotiated session to.",
    },
    {
      "-key", kOptionalArgument,
      "Private-key file to use (default is no client certificate)",
    },
    {
      "-starttls", kOptionalArgument,
      "A STARTTLS mini-protocol to run before the TLS handshake. Supported"
      " values: 'smtp'",
    },
    {
     "-grease", kBooleanArgument,
     "Enable GREASE",
    },
    {
     "", kOptionalArgument, "",
    },
};

static bssl::UniquePtr<EVP_PKEY> LoadPrivateKey(const std::string &file) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_file()));
  if (!bio || !BIO_read_filename(bio.get(), file.c_str())) {
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> pkey(PEM_read_bio_PrivateKey(bio.get(), nullptr,
                                 nullptr, nullptr));
  return pkey;
}

static int NextProtoSelectCallback(SSL* ssl, uint8_t** out, uint8_t* outlen,
                                   const uint8_t* in, unsigned inlen, void* arg) {
  *out = reinterpret_cast<uint8_t *>(arg);
  *outlen = strlen(reinterpret_cast<const char *>(arg));
  return SSL_TLSEXT_ERR_OK;
}

static FILE *g_keylog_file = nullptr;

static void KeyLogCallback(const SSL *ssl, const char *line) {
  fprintf(g_keylog_file, "%s\n", line);
  fflush(g_keylog_file);
}

static bssl::UniquePtr<BIO> session_out;

static int NewSessionCallback(SSL *ssl, SSL_SESSION *session) {
  if (session_out) {
    if (!PEM_write_bio_SSL_SESSION(session_out.get(), session) ||
        BIO_flush(session_out.get()) <= 0) {
      fprintf(stderr, "Error while saving session:\n");
      ERR_print_errors_cb(PrintErrorCallback, stderr);
      return 0;
    }
  }

  return 0;
}

bool Client(const std::vector<std::string> &args) {
  if (!InitSocketLibrary()) {
    return false;
  }

  std::map<std::string, std::string> args_map;

  if (!ParseKeyValueArguments(&args_map, args, kArguments)) {
    PrintUsage(kArguments);
    return false;
  }

  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(SSLv23_client_method()));

  const char *keylog_file = getenv("SSLKEYLOGFILE");
  if (keylog_file) {
    g_keylog_file = fopen(keylog_file, "a");
    if (g_keylog_file == nullptr) {
      perror("fopen");
      return false;
    }
    SSL_CTX_set_keylog_callback(ctx.get(), KeyLogCallback);
  }

  if (args_map.count("-cipher") != 0 &&
      !SSL_CTX_set_cipher_list(ctx.get(), args_map["-cipher"].c_str())) {
    fprintf(stderr, "Failed setting cipher list\n");
    return false;
  }

  if (args_map.count("-max-version") != 0) {
    uint16_t version;
    if (!VersionFromString(&version, args_map["-max-version"])) {
      fprintf(stderr, "Unknown protocol version: '%s'\n",
              args_map["-max-version"].c_str());
      return false;
    }
    if (!SSL_CTX_set_max_proto_version(ctx.get(), version)) {
      return false;
    }
  }

  if (args_map.count("-min-version") != 0) {
    uint16_t version;
    if (!VersionFromString(&version, args_map["-min-version"])) {
      fprintf(stderr, "Unknown protocol version: '%s'\n",
              args_map["-min-version"].c_str());
      return false;
    }
    if (!SSL_CTX_set_min_proto_version(ctx.get(), version)) {
      return false;
    }
  }

  if (args_map.count("-select-next-proto") != 0) {
    const std::string &proto = args_map["-select-next-proto"];
    if (proto.size() > 255) {
      fprintf(stderr, "Bad NPN protocol: '%s'\n", proto.c_str());
      return false;
    }
    // |SSL_CTX_set_next_proto_select_cb| is not const-correct.
    SSL_CTX_set_next_proto_select_cb(ctx.get(), NextProtoSelectCallback,
                                     const_cast<char *>(proto.c_str()));
  }

  if (args_map.count("-alpn-protos") != 0) {
    const std::string &alpn_protos = args_map["-alpn-protos"];
    std::vector<uint8_t> wire;
    size_t i = 0;
    while (i <= alpn_protos.size()) {
      size_t j = alpn_protos.find(',', i);
      if (j == std::string::npos) {
        j = alpn_protos.size();
      }
      size_t len = j - i;
      if (len > 255) {
        fprintf(stderr, "Invalid ALPN protocols: '%s'\n", alpn_protos.c_str());
        return false;
      }
      wire.push_back(static_cast<uint8_t>(len));
      wire.resize(wire.size() + len);
      memcpy(wire.data() + wire.size() - len, alpn_protos.data() + i, len);
      i = j + 1;
    }
    if (SSL_CTX_set_alpn_protos(ctx.get(), wire.data(), wire.size()) != 0) {
      return false;
    }
  }

  if (args_map.count("-fallback-scsv") != 0) {
    SSL_CTX_set_mode(ctx.get(), SSL_MODE_SEND_FALLBACK_SCSV);
  }

  if (args_map.count("-ocsp-stapling") != 0) {
    SSL_CTX_enable_ocsp_stapling(ctx.get());
  }

  if (args_map.count("-signed-certificate-timestamps") != 0) {
    SSL_CTX_enable_signed_cert_timestamps(ctx.get());
  }

  if (args_map.count("-channel-id-key") != 0) {
    bssl::UniquePtr<EVP_PKEY> pkey =
        LoadPrivateKey(args_map["-channel-id-key"]);
    if (!pkey || !SSL_CTX_set1_tls_channel_id(ctx.get(), pkey.get())) {
      return false;
    }
  }

  if (args_map.count("-false-start") != 0) {
    SSL_CTX_set_mode(ctx.get(), SSL_MODE_ENABLE_FALSE_START);
  }

  if (args_map.count("-key") != 0) {
    const std::string &key = args_map["-key"];
    if (!SSL_CTX_use_PrivateKey_file(ctx.get(), key.c_str(), SSL_FILETYPE_PEM)) {
      fprintf(stderr, "Failed to load private key: %s\n", key.c_str());
      return false;
    }
    if (!SSL_CTX_use_certificate_chain_file(ctx.get(), key.c_str())) {
      fprintf(stderr, "Failed to load cert chain: %s\n", key.c_str());
      return false;
    }
  }

  if (args_map.count("-session-out") != 0) {
    session_out.reset(BIO_new_file(args_map["-session-out"].c_str(), "wb"));
    if (!session_out) {
      fprintf(stderr, "Error while opening %s:\n",
              args_map["-session-out"].c_str());
      ERR_print_errors_cb(PrintErrorCallback, stderr);
      return false;
    }
    SSL_CTX_set_session_cache_mode(ctx.get(), SSL_SESS_CACHE_CLIENT);
    SSL_CTX_sess_set_new_cb(ctx.get(), NewSessionCallback);
  }

  if (args_map.count("-grease") != 0) {
    SSL_CTX_set_grease_enabled(ctx.get(), 1);
  }

  int sock = -1;
  if (!Connect(&sock, args_map["-connect"])) {
    return false;
  }

  if (args_map.count("-starttls") != 0) {
    const std::string& starttls = args_map["-starttls"];
    if (starttls == "smtp") {
      if (!DoSMTPStartTLS(sock)) {
        return false;
      }
    } else {
      fprintf(stderr, "Unknown value for -starttls: %s\n", starttls.c_str());
      return false;
    }
  }

  bssl::UniquePtr<BIO> bio(BIO_new_socket(sock, BIO_CLOSE));
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));

  if (args_map.count("-server-name") != 0) {
    SSL_set_tlsext_host_name(ssl.get(), args_map["-server-name"].c_str());
  }

  if (args_map.count("-session-in") != 0) {
    bssl::UniquePtr<BIO> in(BIO_new_file(args_map["-session-in"].c_str(),
                                         "rb"));
    if (!in) {
      fprintf(stderr, "Error reading session\n");
      ERR_print_errors_cb(PrintErrorCallback, stderr);
      return false;
    }
    bssl::UniquePtr<SSL_SESSION> session(PEM_read_bio_SSL_SESSION(in.get(),
                                         nullptr, nullptr, nullptr));
    if (!session) {
      fprintf(stderr, "Error reading session\n");
      ERR_print_errors_cb(PrintErrorCallback, stderr);
      return false;
    }
    SSL_set_session(ssl.get(), session.get());
  }

  SSL_set_bio(ssl.get(), bio.get(), bio.get());
  bio.release();

  int ret = SSL_connect(ssl.get());
  if (ret != 1) {
    int ssl_err = SSL_get_error(ssl.get(), ret);
    fprintf(stderr, "Error while connecting: %d\n", ssl_err);
    ERR_print_errors_cb(PrintErrorCallback, stderr);
    return false;
  }

  fprintf(stderr, "Connected.\n");
  PrintConnectionInfo(ssl.get());

  bool ok = TransferData(ssl.get(), sock);

  return ok;
}
