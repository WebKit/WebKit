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

#include <string>
#include <vector>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if !defined(OPENSSL_WINDOWS)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <io.h>
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <winsock2.h>
#include <ws2tcpip.h>
OPENSSL_MSVC_PRAGMA(warning(pop))

typedef int ssize_t;
OPENSSL_MSVC_PRAGMA(comment(lib, "Ws2_32.lib"))
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "../crypto/internal.h"
#include "internal.h"
#include "transport_common.h"


#if !defined(OPENSSL_WINDOWS)
static int closesocket(int sock) {
  return close(sock);
}
#endif

bool InitSocketLibrary() {
#if defined(OPENSSL_WINDOWS)
  WSADATA wsaData;
  int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (err != 0) {
    fprintf(stderr, "WSAStartup failed with error %d\n", err);
    return false;
  }
#endif
  return true;
}

// Connect sets |*out_sock| to be a socket connected to the destination given
// in |hostname_and_port|, which should be of the form "www.example.com:123".
// It returns true on success and false otherwise.
bool Connect(int *out_sock, const std::string &hostname_and_port) {
  size_t colon_offset = hostname_and_port.find_last_of(':');
  const size_t bracket_offset = hostname_and_port.find_last_of(']');
  std::string hostname, port;

  // An IPv6 literal may have colons internally, guarded by square brackets.
  if (bracket_offset != std::string::npos &&
      colon_offset != std::string::npos && bracket_offset > colon_offset) {
    colon_offset = std::string::npos;
  }

  if (colon_offset == std::string::npos) {
    hostname = hostname_and_port;
    port = "443";
  } else {
    hostname = hostname_and_port.substr(0, colon_offset);
    port = hostname_and_port.substr(colon_offset + 1);
  }

  // Handle IPv6 literals.
  if (hostname.size() >= 2 && hostname[0] == '[' &&
      hostname[hostname.size() - 1] == ']') {
    hostname = hostname.substr(1, hostname.size() - 2);
  }

  struct addrinfo hint, *result;
  OPENSSL_memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;

  int ret = getaddrinfo(hostname.c_str(), port.c_str(), &hint, &result);
  if (ret != 0) {
    fprintf(stderr, "getaddrinfo returned: %s\n", gai_strerror(ret));
    return false;
  }

  bool ok = false;
  char buf[256];

  *out_sock =
      socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (*out_sock < 0) {
    perror("socket");
    goto out;
  }

  switch (result->ai_family) {
    case AF_INET: {
      struct sockaddr_in *sin =
          reinterpret_cast<struct sockaddr_in *>(result->ai_addr);
      fprintf(stderr, "Connecting to %s:%d\n",
              inet_ntop(result->ai_family, &sin->sin_addr, buf, sizeof(buf)),
              ntohs(sin->sin_port));
      break;
    }
    case AF_INET6: {
      struct sockaddr_in6 *sin6 =
          reinterpret_cast<struct sockaddr_in6 *>(result->ai_addr);
      fprintf(stderr, "Connecting to [%s]:%d\n",
              inet_ntop(result->ai_family, &sin6->sin6_addr, buf, sizeof(buf)),
              ntohs(sin6->sin6_port));
      break;
    }
  }

  if (connect(*out_sock, result->ai_addr, result->ai_addrlen) != 0) {
    perror("connect");
    goto out;
  }
  ok = true;

out:
  freeaddrinfo(result);
  return ok;
}

bool Accept(int *out_sock, const std::string &port) {
  struct sockaddr_in6 addr, cli_addr;
  socklen_t cli_addr_len = sizeof(cli_addr);
  OPENSSL_memset(&addr, 0, sizeof(addr));

  addr.sin6_family = AF_INET6;
  addr.sin6_addr = IN6ADDR_ANY_INIT;
  addr.sin6_port = htons(atoi(port.c_str()));

  bool ok = false;
#if defined(OPENSSL_WINDOWS)
  const BOOL enable = TRUE;
#else
  const int enable = 1;
#endif
  int server_sock = -1;

  server_sock =
      socket(addr.sin6_family, SOCK_STREAM, 0);
  if (server_sock < 0) {
    perror("socket");
    goto out;
  }

  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable,
                 sizeof(enable)) < 0) {
    perror("setsockopt");
    goto out;
  }

  if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    perror("connect");
    goto out;
  }
  listen(server_sock, 1);
  *out_sock = accept(server_sock, (struct sockaddr*)&cli_addr, &cli_addr_len);

  ok = true;

out:
  closesocket(server_sock);
  return ok;
}

bool VersionFromString(uint16_t *out_version, const std::string &version) {
  if (version == "ssl3") {
    *out_version = SSL3_VERSION;
    return true;
  } else if (version == "tls1" || version == "tls1.0") {
    *out_version = TLS1_VERSION;
    return true;
  } else if (version == "tls1.1") {
    *out_version = TLS1_1_VERSION;
    return true;
  } else if (version == "tls1.2") {
    *out_version = TLS1_2_VERSION;
    return true;
  } else if (version == "tls1.3") {
    *out_version = TLS1_3_VERSION;
    return true;
  }
  return false;
}

static const char *SignatureAlgorithmToString(uint16_t version, uint16_t sigalg) {
  const bool is_tls12 = version == TLS1_2_VERSION || version == DTLS1_2_VERSION;
  switch (sigalg) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return "rsa_pkcs1_sha1";
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return "rsa_pkcs1_sha256";
    case SSL_SIGN_RSA_PKCS1_SHA384:
      return "rsa_pkcs1_sha384";
    case SSL_SIGN_RSA_PKCS1_SHA512:
      return "rsa_pkcs1_sha512";
    case SSL_SIGN_ECDSA_SHA1:
      return "ecdsa_sha1";
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      return is_tls12 ? "ecdsa_sha256" : "ecdsa_secp256r1_sha256";
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      return is_tls12 ? "ecdsa_sha384" : "ecdsa_secp384r1_sha384";
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      return is_tls12 ? "ecdsa_sha512" : "ecdsa_secp521r1_sha512";
    case SSL_SIGN_RSA_PSS_SHA256:
      return "rsa_pss_sha256";
    case SSL_SIGN_RSA_PSS_SHA384:
      return "rsa_pss_sha384";
    case SSL_SIGN_RSA_PSS_SHA512:
      return "rsa_pss_sha512";
    case SSL_SIGN_ED25519:
      return "ed25519";
    default:
      return "(unknown)";
  }
}

void PrintConnectionInfo(const SSL *ssl) {
  const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);

  fprintf(stderr, "  Version: %s\n", SSL_get_version(ssl));
  fprintf(stderr, "  Resumed session: %s\n",
          SSL_session_reused(ssl) ? "yes" : "no");
  fprintf(stderr, "  Cipher: %s\n", SSL_CIPHER_get_name(cipher));
  uint16_t curve = SSL_get_curve_id(ssl);
  if (curve != 0) {
    fprintf(stderr, "  ECDHE curve: %s\n", SSL_get_curve_name(curve));
  }
  uint16_t sigalg = SSL_get_peer_signature_algorithm(ssl);
  if (sigalg != 0) {
    fprintf(stderr, "  Signature algorithm: %s\n",
            SignatureAlgorithmToString(SSL_version(ssl), sigalg));
  }
  fprintf(stderr, "  Secure renegotiation: %s\n",
          SSL_get_secure_renegotiation_support(ssl) ? "yes" : "no");
  fprintf(stderr, "  Extended master secret: %s\n",
          SSL_get_extms_support(ssl) ? "yes" : "no");

  const uint8_t *next_proto;
  unsigned next_proto_len;
  SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
  fprintf(stderr, "  Next protocol negotiated: %.*s\n", next_proto_len,
          next_proto);

  const uint8_t *alpn;
  unsigned alpn_len;
  SSL_get0_alpn_selected(ssl, &alpn, &alpn_len);
  fprintf(stderr, "  ALPN protocol: %.*s\n", alpn_len, alpn);

  const char *host_name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (host_name != nullptr && SSL_is_server(ssl)) {
    fprintf(stderr, "  Client sent SNI: %s\n", host_name);
  }

  if (!SSL_is_server(ssl)) {
    const uint8_t *ocsp_staple;
    size_t ocsp_staple_len;
    SSL_get0_ocsp_response(ssl, &ocsp_staple, &ocsp_staple_len);
    fprintf(stderr, "  OCSP staple: %s\n", ocsp_staple_len > 0 ? "yes" : "no");

    const uint8_t *sct_list;
    size_t sct_list_len;
    SSL_get0_signed_cert_timestamp_list(ssl, &sct_list, &sct_list_len);
    fprintf(stderr, "  SCT list: %s\n", sct_list_len > 0 ? "yes" : "no");
  }

  fprintf(stderr, "  Early data: %s\n",
          SSL_early_data_accepted(ssl) ? "yes" : "no");

  // Print the server cert subject and issuer names.
  bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(ssl));
  if (peer != nullptr) {
    fprintf(stderr, "  Cert subject: ");
    X509_NAME_print_ex_fp(stderr, X509_get_subject_name(peer.get()), 0,
                          XN_FLAG_ONELINE);
    fprintf(stderr, "\n  Cert issuer: ");
    X509_NAME_print_ex_fp(stderr, X509_get_issuer_name(peer.get()), 0,
                          XN_FLAG_ONELINE);
    fprintf(stderr, "\n");
  }
}

bool SocketSetNonBlocking(int sock, bool is_non_blocking) {
  bool ok;

#if defined(OPENSSL_WINDOWS)
  u_long arg = is_non_blocking;
  ok = 0 == ioctlsocket(sock, FIONBIO, &arg);
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  if (is_non_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  ok = 0 == fcntl(sock, F_SETFL, flags);
#endif
  if (!ok) {
    fprintf(stderr, "Failed to set socket non-blocking.\n");
  }
  return ok;
}

// PrintErrorCallback is a callback function from OpenSSL's
// |ERR_print_errors_cb| that writes errors to a given |FILE*|.
int PrintErrorCallback(const char *str, size_t len, void *ctx) {
  fwrite(str, len, 1, reinterpret_cast<FILE*>(ctx));
  return 1;
}

bool TransferData(SSL *ssl, int sock) {
  bool stdin_open = true;

  fd_set read_fds;
  FD_ZERO(&read_fds);

  if (!SocketSetNonBlocking(sock, true)) {
    return false;
  }

  for (;;) {
    if (stdin_open) {
      FD_SET(0, &read_fds);
    }
    FD_SET(sock, &read_fds);

    int ret = select(sock + 1, &read_fds, NULL, NULL, NULL);
    if (ret <= 0) {
      perror("select");
      return false;
    }

    if (FD_ISSET(0, &read_fds)) {
      uint8_t buffer[512];
      ssize_t n;

      do {
        n = BORINGSSL_READ(0, buffer, sizeof(buffer));
      } while (n == -1 && errno == EINTR);

      if (n == 0) {
        FD_CLR(0, &read_fds);
        stdin_open = false;
#if !defined(OPENSSL_WINDOWS)
        shutdown(sock, SHUT_WR);
#else
        shutdown(sock, SD_SEND);
#endif
        continue;
      } else if (n < 0) {
        perror("read from stdin");
        return false;
      }

      if (!SocketSetNonBlocking(sock, false)) {
        return false;
      }
      int ssl_ret = SSL_write(ssl, buffer, n);
      if (!SocketSetNonBlocking(sock, true)) {
        return false;
      }

      if (ssl_ret <= 0) {
        int ssl_err = SSL_get_error(ssl, ssl_ret);
        fprintf(stderr, "Error while writing: %d\n", ssl_err);
        ERR_print_errors_cb(PrintErrorCallback, stderr);
        return false;
      } else if (ssl_ret != n) {
        fprintf(stderr, "Short write from SSL_write.\n");
        return false;
      }
    }

    if (FD_ISSET(sock, &read_fds)) {
      uint8_t buffer[512];
      int ssl_ret = SSL_read(ssl, buffer, sizeof(buffer));

      if (ssl_ret < 0) {
        int ssl_err = SSL_get_error(ssl, ssl_ret);
        if (ssl_err == SSL_ERROR_WANT_READ) {
          continue;
        }
        fprintf(stderr, "Error while reading: %d\n", ssl_err);
        ERR_print_errors_cb(PrintErrorCallback, stderr);
        return false;
      } else if (ssl_ret == 0) {
        return true;
      }

      ssize_t n;
      do {
        n = BORINGSSL_WRITE(1, buffer, ssl_ret);
      } while (n == -1 && errno == EINTR);

      if (n != ssl_ret) {
        fprintf(stderr, "Short write to stderr.\n");
        return false;
      }
    }
  }
}

// SocketLineReader wraps a small buffer around a socket for line-orientated
// protocols.
class SocketLineReader {
 public:
  explicit SocketLineReader(int sock) : sock_(sock) {}

  // Next reads a '\n'- or '\r\n'-terminated line from the socket and, on
  // success, sets |*out_line| to it and returns true. Otherwise it returns
  // false.
  bool Next(std::string *out_line) {
    for (;;) {
      for (size_t i = 0; i < buf_len_; i++) {
        if (buf_[i] != '\n') {
          continue;
        }

        size_t length = i;
        if (i > 0 && buf_[i - 1] == '\r') {
          length--;
        }

        out_line->assign(buf_, length);
        buf_len_ -= i + 1;
        OPENSSL_memmove(buf_, &buf_[i + 1], buf_len_);

        return true;
      }

      if (buf_len_ == sizeof(buf_)) {
        fprintf(stderr, "Received line too long!\n");
        return false;
      }

      ssize_t n;
      do {
        n = recv(sock_, &buf_[buf_len_], sizeof(buf_) - buf_len_, 0);
      } while (n == -1 && errno == EINTR);

      if (n < 0) {
        fprintf(stderr, "Read error from socket\n");
        return false;
      }

      buf_len_ += n;
    }
  }

  // ReadSMTPReply reads one or more lines that make up an SMTP reply. On
  // success, it sets |*out_code| to the reply's code (e.g. 250) and
  // |*out_content| to the body of the reply (e.g. "OK") and returns true.
  // Otherwise it returns false.
  //
  // See https://tools.ietf.org/html/rfc821#page-48
  bool ReadSMTPReply(unsigned *out_code, std::string *out_content) {
    out_content->clear();

    // kMaxLines is the maximum number of lines that we'll accept in an SMTP
    // reply.
    static const unsigned kMaxLines = 512;
    for (unsigned i = 0; i < kMaxLines; i++) {
      std::string line;
      if (!Next(&line)) {
        return false;
      }

      if (line.size() < 4) {
        fprintf(stderr, "Short line from SMTP server: %s\n", line.c_str());
        return false;
      }

      const std::string code_str = line.substr(0, 3);
      char *endptr;
      const unsigned long code = strtoul(code_str.c_str(), &endptr, 10);
      if (*endptr || code > UINT_MAX) {
        fprintf(stderr, "Failed to parse code from line: %s\n", line.c_str());
        return false;
      }

      if (i == 0) {
        *out_code = code;
      } else if (code != *out_code) {
        fprintf(stderr,
                "Reply code varied within a single reply: was %u, now %u\n",
                *out_code, static_cast<unsigned>(code));
        return false;
      }

      if (line[3] == ' ') {
        // End of reply.
        *out_content += line.substr(4, std::string::npos);
        return true;
      } else if (line[3] == '-') {
        // Another line of reply will follow this one.
        *out_content += line.substr(4, std::string::npos);
        out_content->push_back('\n');
      } else {
        fprintf(stderr, "Bad character after code in SMTP reply: %s\n",
                line.c_str());
        return false;
      }
    }

    fprintf(stderr, "Rejected SMTP reply of more then %u lines\n", kMaxLines);
    return false;
  }

 private:
  const int sock_;
  char buf_[512];
  size_t buf_len_ = 0;
};

// SendAll writes |data_len| bytes from |data| to |sock|. It returns true on
// success and false otherwise.
static bool SendAll(int sock, const char *data, size_t data_len) {
  size_t done = 0;

  while (done < data_len) {
    ssize_t n;
    do {
      n = send(sock, &data[done], data_len - done, 0);
    } while (n == -1 && errno == EINTR);

    if (n < 0) {
      fprintf(stderr, "Error while writing to socket\n");
      return false;
    }

    done += n;
  }

  return true;
}

bool DoSMTPStartTLS(int sock) {
  SocketLineReader line_reader(sock);

  unsigned code_220 = 0;
  std::string reply_220;
  if (!line_reader.ReadSMTPReply(&code_220, &reply_220)) {
    return false;
  }

  if (code_220 != 220) {
    fprintf(stderr, "Expected 220 line from SMTP server but got code %u\n",
            code_220);
    return false;
  }

  static const char kHelloLine[] = "EHLO BoringSSL\r\n";
  if (!SendAll(sock, kHelloLine, sizeof(kHelloLine) - 1)) {
    return false;
  }

  unsigned code_250 = 0;
  std::string reply_250;
  if (!line_reader.ReadSMTPReply(&code_250, &reply_250)) {
    return false;
  }

  if (code_250 != 250) {
    fprintf(stderr, "Expected 250 line after EHLO but got code %u\n", code_250);
    return false;
  }

  // https://tools.ietf.org/html/rfc1869#section-4.3
  if (("\n" + reply_250 + "\n").find("\nSTARTTLS\n") == std::string::npos) {
    fprintf(stderr, "Server does not support STARTTLS\n");
    return false;
  }

  static const char kSTARTTLSLine[] = "STARTTLS\r\n";
  if (!SendAll(sock, kSTARTTLSLine, sizeof(kSTARTTLSLine) - 1)) {
    return false;
  }

  if (!line_reader.ReadSMTPReply(&code_220, &reply_220)) {
    return false;
  }

  if (code_220 != 220) {
    fprintf(
        stderr,
        "Expected 220 line from SMTP server after STARTTLS, but got code %u\n",
        code_220);
    return false;
  }

  return true;
}
