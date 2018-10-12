/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/peerconnection/server/data_socket.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif

#include "examples/peerconnection/server/utils.h"

static const char kHeaderTerminator[] = "\r\n\r\n";
static const int kHeaderTerminatorLength = sizeof(kHeaderTerminator) - 1;

// static
const char DataSocket::kCrossOriginAllowHeaders[] =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Credentials: true\r\n"
    "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, "
    "Content-Length, Connection, Cache-Control\r\n"
    "Access-Control-Expose-Headers: Content-Length, X-Peer-Id\r\n";

#if defined(WIN32)
class WinsockInitializer {
  static WinsockInitializer singleton;

  WinsockInitializer() {
    WSADATA data;
    WSAStartup(MAKEWORD(1, 0), &data);
  }

 public:
  ~WinsockInitializer() { WSACleanup(); }
};
WinsockInitializer WinsockInitializer::singleton;
#endif

//
// SocketBase
//

bool SocketBase::Create() {
  assert(!valid());
  socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
  return valid();
}

void SocketBase::Close() {
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
}

//
// DataSocket
//

std::string DataSocket::request_arguments() const {
  size_t args = request_path_.find('?');
  if (args != std::string::npos)
    return request_path_.substr(args + 1);
  return "";
}

bool DataSocket::PathEquals(const char* path) const {
  assert(path);
  size_t args = request_path_.find('?');
  if (args != std::string::npos)
    return request_path_.substr(0, args).compare(path) == 0;
  return request_path_.compare(path) == 0;
}

bool DataSocket::OnDataAvailable(bool* close_socket) {
  assert(valid());
  char buffer[0xfff] = {0};
  int bytes = recv(socket_, buffer, sizeof(buffer), 0);
  if (bytes == SOCKET_ERROR || bytes == 0) {
    *close_socket = true;
    return false;
  }

  *close_socket = false;

  bool ret = true;
  if (headers_received()) {
    if (method_ != POST) {
      // unexpectedly received data.
      ret = false;
    } else {
      data_.append(buffer, bytes);
    }
  } else {
    request_headers_.append(buffer, bytes);
    size_t found = request_headers_.find(kHeaderTerminator);
    if (found != std::string::npos) {
      data_ = request_headers_.substr(found + kHeaderTerminatorLength);
      request_headers_.resize(found + kHeaderTerminatorLength);
      ret = ParseHeaders();
    }
  }
  return ret;
}

bool DataSocket::Send(const std::string& data) const {
  return send(socket_, data.data(), static_cast<int>(data.length()), 0) !=
         SOCKET_ERROR;
}

bool DataSocket::Send(const std::string& status,
                      bool connection_close,
                      const std::string& content_type,
                      const std::string& extra_headers,
                      const std::string& data) const {
  assert(valid());
  assert(!status.empty());
  std::string buffer("HTTP/1.1 " + status + "\r\n");

  buffer +=
      "Server: PeerConnectionTestServer/0.1\r\n"
      "Cache-Control: no-cache\r\n";

  if (connection_close)
    buffer += "Connection: close\r\n";

  if (!content_type.empty())
    buffer += "Content-Type: " + content_type + "\r\n";

  buffer +=
      "Content-Length: " + int2str(static_cast<int>(data.size())) + "\r\n";

  if (!extra_headers.empty()) {
    buffer += extra_headers;
    // Extra headers are assumed to have a separator per header.
  }

  buffer += kCrossOriginAllowHeaders;

  buffer += "\r\n";
  buffer += data;

  return Send(buffer);
}

void DataSocket::Clear() {
  method_ = INVALID;
  content_length_ = 0;
  content_type_.clear();
  request_path_.clear();
  request_headers_.clear();
  data_.clear();
}

bool DataSocket::ParseHeaders() {
  assert(!request_headers_.empty());
  assert(method_ == INVALID);
  size_t i = request_headers_.find("\r\n");
  if (i == std::string::npos)
    return false;

  if (!ParseMethodAndPath(request_headers_.data(), i))
    return false;

  assert(method_ != INVALID);
  assert(!request_path_.empty());

  if (method_ == POST) {
    const char* headers = request_headers_.data() + i + 2;
    size_t len = request_headers_.length() - i - 2;
    if (!ParseContentLengthAndType(headers, len))
      return false;
  }

  return true;
}

bool DataSocket::ParseMethodAndPath(const char* begin, size_t len) {
  struct {
    const char* method_name;
    size_t method_name_len;
    RequestMethod id;
  } supported_methods[] = {
      {"GET", 3, GET}, {"POST", 4, POST}, {"OPTIONS", 7, OPTIONS},
  };

  const char* path = NULL;
  for (size_t i = 0; i < ARRAYSIZE(supported_methods); ++i) {
    if (len > supported_methods[i].method_name_len &&
        isspace(begin[supported_methods[i].method_name_len]) &&
        strncmp(begin, supported_methods[i].method_name,
                supported_methods[i].method_name_len) == 0) {
      method_ = supported_methods[i].id;
      path = begin + supported_methods[i].method_name_len;
      break;
    }
  }

  const char* end = begin + len;
  if (!path || path >= end)
    return false;

  ++path;
  begin = path;
  while (!isspace(*path) && path < end)
    ++path;

  request_path_.assign(begin, path - begin);

  return true;
}

bool DataSocket::ParseContentLengthAndType(const char* headers, size_t length) {
  assert(content_length_ == 0);
  assert(content_type_.empty());

  const char* end = headers + length;
  while (headers && headers < end) {
    if (!isspace(headers[0])) {
      static const char kContentLength[] = "Content-Length:";
      static const char kContentType[] = "Content-Type:";
      if ((headers + ARRAYSIZE(kContentLength)) < end &&
          strncmp(headers, kContentLength, ARRAYSIZE(kContentLength) - 1) ==
              0) {
        headers += ARRAYSIZE(kContentLength) - 1;
        while (headers[0] == ' ')
          ++headers;
        content_length_ = atoi(headers);
      } else if ((headers + ARRAYSIZE(kContentType)) < end &&
                 strncmp(headers, kContentType, ARRAYSIZE(kContentType) - 1) ==
                     0) {
        headers += ARRAYSIZE(kContentType) - 1;
        while (headers[0] == ' ')
          ++headers;
        const char* type_end = strstr(headers, "\r\n");
        if (type_end == NULL)
          type_end = end;
        content_type_.assign(headers, type_end);
      }
    } else {
      ++headers;
    }
    headers = strstr(headers, "\r\n");
    if (headers)
      headers += 2;
  }

  return !content_type_.empty() && content_length_ != 0;
}

//
// ListeningSocket
//

bool ListeningSocket::Listen(unsigned short port) {
  assert(valid());
  int enabled = 1;
  setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&enabled), sizeof(enabled));
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(socket_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) ==
      SOCKET_ERROR) {
    printf("bind failed\n");
    return false;
  }
  return listen(socket_, 5) != SOCKET_ERROR;
}

DataSocket* ListeningSocket::Accept() const {
  assert(valid());
  struct sockaddr_in addr = {0};
  socklen_t size = sizeof(addr);
  NativeSocket client =
      accept(socket_, reinterpret_cast<sockaddr*>(&addr), &size);
  if (client == INVALID_SOCKET)
    return NULL;

  return new DataSocket(client);
}
