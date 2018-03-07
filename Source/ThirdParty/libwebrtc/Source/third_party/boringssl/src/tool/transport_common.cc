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

static void SplitHostPort(std::string *out_hostname, std::string *out_port,
                          const std::string &hostname_and_port) {
  size_t colon_offset = hostname_and_port.find_last_of(':');
  const size_t bracket_offset = hostname_and_port.find_last_of(']');
  std::string hostname, port;

  // An IPv6 literal may have colons internally, guarded by square brackets.
  if (bracket_offset != std::string::npos &&
      colon_offset != std::string::npos && bracket_offset > colon_offset) {
    colon_offset = std::string::npos;
  }

  if (colon_offset == std::string::npos) {
    *out_hostname = hostname_and_port;
    *out_port = "443";
  } else {
    *out_hostname = hostname_and_port.substr(0, colon_offset);
    *out_port = hostname_and_port.substr(colon_offset + 1);
  }
}

// Connect sets |*out_sock| to be a socket connected to the destination given
// in |hostname_and_port|, which should be of the form "www.example.com:123".
// It returns true on success and false otherwise.
bool Connect(int *out_sock, const std::string &hostname_and_port) {
  std::string hostname, port;
  SplitHostPort(&hostname, &port, hostname_and_port);

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

Listener::~Listener() {
  if (server_sock_ >= 0) {
    closesocket(server_sock_);
  }
}

bool Listener::Init(const std::string &port) {
  if (server_sock_ >= 0) {
    return false;
  }

  struct sockaddr_in6 addr;
  OPENSSL_memset(&addr, 0, sizeof(addr));

  addr.sin6_family = AF_INET6;
  // Windows' IN6ADDR_ANY_INIT does not have enough curly braces for clang-cl
  // (https://crbug.com/772108), while other platforms like NaCl are missing
  // in6addr_any, so use a mix of both.
#if defined(OPENSSL_WINDOWS)
  addr.sin6_addr = in6addr_any;
#else
  addr.sin6_addr = IN6ADDR_ANY_INIT;
#endif
  addr.sin6_port = htons(atoi(port.c_str()));

#if defined(OPENSSL_WINDOWS)
  const BOOL enable = TRUE;
#else
  const int enable = 1;
#endif

  server_sock_ = socket(addr.sin6_family, SOCK_STREAM, 0);
  if (server_sock_ < 0) {
    perror("socket");
    return false;
  }

  if (setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable,
                 sizeof(enable)) < 0) {
    perror("setsockopt");
    return false;
  }

  if (bind(server_sock_, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("connect");
    return false;
  }

  listen(server_sock_, SOMAXCONN);
  return true;
}

bool Listener::Accept(int *out_sock) {
  struct sockaddr_in6 addr;
  socklen_t addr_len = sizeof(addr);
  *out_sock = accept(server_sock_, (struct sockaddr *)&addr, &addr_len);
  return *out_sock >= 0;
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

void PrintConnectionInfo(BIO *bio, const SSL *ssl) {
  const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);

  BIO_printf(bio, "  Version: %s\n", SSL_get_version(ssl));
  BIO_printf(bio, "  Resumed session: %s\n",
             SSL_session_reused(ssl) ? "yes" : "no");
  BIO_printf(bio, "  Cipher: %s\n", SSL_CIPHER_standard_name(cipher));
  uint16_t curve = SSL_get_curve_id(ssl);
  if (curve != 0) {
    BIO_printf(bio, "  ECDHE curve: %s\n", SSL_get_curve_name(curve));
  }
  uint16_t sigalg = SSL_get_peer_signature_algorithm(ssl);
  if (sigalg != 0) {
    BIO_printf(bio, "  Signature algorithm: %s\n",
               SSL_get_signature_algorithm_name(
                   sigalg, SSL_version(ssl) != TLS1_2_VERSION));
  }
  BIO_printf(bio, "  Secure renegotiation: %s\n",
             SSL_get_secure_renegotiation_support(ssl) ? "yes" : "no");
  BIO_printf(bio, "  Extended master secret: %s\n",
             SSL_get_extms_support(ssl) ? "yes" : "no");

  const uint8_t *next_proto;
  unsigned next_proto_len;
  SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
  BIO_printf(bio, "  Next protocol negotiated: %.*s\n", next_proto_len,
             next_proto);

  const uint8_t *alpn;
  unsigned alpn_len;
  SSL_get0_alpn_selected(ssl, &alpn, &alpn_len);
  BIO_printf(bio, "  ALPN protocol: %.*s\n", alpn_len, alpn);

  const char *host_name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (host_name != nullptr && SSL_is_server(ssl)) {
    BIO_printf(bio, "  Client sent SNI: %s\n", host_name);
  }

  if (!SSL_is_server(ssl)) {
    const uint8_t *ocsp_staple;
    size_t ocsp_staple_len;
    SSL_get0_ocsp_response(ssl, &ocsp_staple, &ocsp_staple_len);
    BIO_printf(bio, "  OCSP staple: %s\n", ocsp_staple_len > 0 ? "yes" : "no");

    const uint8_t *sct_list;
    size_t sct_list_len;
    SSL_get0_signed_cert_timestamp_list(ssl, &sct_list, &sct_list_len);
    BIO_printf(bio, "  SCT list: %s\n", sct_list_len > 0 ? "yes" : "no");
  }

  BIO_printf(
      bio, "  Early data: %s\n",
      (SSL_early_data_accepted(ssl) || SSL_in_early_data(ssl)) ? "yes" : "no");

  // Print the server cert subject and issuer names.
  bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(ssl));
  if (peer != nullptr) {
    BIO_printf(bio, "  Cert subject: ");
    X509_NAME_print_ex(bio, X509_get_subject_name(peer.get()), 0,
                       XN_FLAG_ONELINE);
    BIO_printf(bio, "\n  Cert issuer: ");
    X509_NAME_print_ex(bio, X509_get_issuer_name(peer.get()), 0,
                       XN_FLAG_ONELINE);
    BIO_printf(bio, "\n");
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

static bool SocketSelect(int sock, bool stdin_open, bool *socket_ready,
                         bool *stdin_ready) {
#if !defined(OPENSSL_WINDOWS)
  fd_set read_fds;
  FD_ZERO(&read_fds);
  if (stdin_open) {
    FD_SET(0, &read_fds);
  }
  FD_SET(sock, &read_fds);
  if (select(sock + 1, &read_fds, NULL, NULL, NULL) <= 0) {
    perror("select");
    return false;
  }

  if (FD_ISSET(0, &read_fds)) {
    *stdin_ready = true;
  }
  if (FD_ISSET(sock, &read_fds)) {
    *socket_ready = true;
  }

  return true;
#else
  WSAEVENT socket_handle = WSACreateEvent();
  if (socket_handle == WSA_INVALID_EVENT ||
      WSAEventSelect(sock, socket_handle, FD_READ) != 0) {
    WSACloseEvent(socket_handle);
    return false;
  }

  HANDLE read_fds[2];
  read_fds[0] = socket_handle;
  read_fds[1] = GetStdHandle(STD_INPUT_HANDLE);

  switch (
      WaitForMultipleObjects(stdin_open ? 2 : 1, read_fds, FALSE, INFINITE)) {
    case WAIT_OBJECT_0 + 0:
      *socket_ready = true;
      break;
    case WAIT_OBJECT_0 + 1:
      *stdin_ready = true;
      break;
    case WAIT_TIMEOUT:
      break;
    default:
      WSACloseEvent(socket_handle);
      return false;
  }

  WSACloseEvent(socket_handle);
  return true;
#endif
}

// PrintErrorCallback is a callback function from OpenSSL's
// |ERR_print_errors_cb| that writes errors to a given |FILE*|.
int PrintErrorCallback(const char *str, size_t len, void *ctx) {
  fwrite(str, len, 1, reinterpret_cast<FILE*>(ctx));
  return 1;
}

bool TransferData(SSL *ssl, int sock) {
  if (!SocketSetNonBlocking(sock, true)) {
    return false;
  }

  bool stdin_open = true;
  for (;;) {
    bool socket_ready = false;
    bool stdin_ready = false;
    if (!SocketSelect(sock, stdin_open, &socket_ready, &stdin_ready)) {
      return false;
    }

    if (stdin_ready) {
      uint8_t buffer[512];
      ssize_t n;

      do {
        n = BORINGSSL_READ(0, buffer, sizeof(buffer));
      } while (n == -1 && errno == EINTR);

      if (n == 0) {
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

      // On Windows, SocketSelect ends up setting sock to non-blocking.
#if !defined(OPENSSL_WINDOWS)
      if (!SocketSetNonBlocking(sock, false)) {
        return false;
      }
#endif
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

    if (socket_ready) {
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

bool DoHTTPTunnel(int sock, const std::string &hostname_and_port) {
  std::string hostname, port;
  SplitHostPort(&hostname, &port, hostname_and_port);

  fprintf(stderr, "Establishing HTTP tunnel to %s:%s.\n", hostname.c_str(),
          port.c_str());
  char buf[1024];
  snprintf(buf, sizeof(buf), "CONNECT %s:%s HTTP/1.0\r\n\r\n", hostname.c_str(),
           port.c_str());
  if (!SendAll(sock, buf, strlen(buf))) {
    return false;
  }

  SocketLineReader line_reader(sock);

  // Read until an empty line, signaling the end of the HTTP response.
  std::string line;
  for (;;) {
    if (!line_reader.Next(&line)) {
      return false;
    }
    if (line.empty()) {
      return true;
    }
    fprintf(stderr, "%s\n", line.c_str());
  }
}
