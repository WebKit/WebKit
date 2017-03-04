/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_SOCKETADAPTERS_H_
#define WEBRTC_BASE_SOCKETADAPTERS_H_

#include <map>
#include <string>

#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/cryptstring.h"
#include "webrtc/base/logging.h"

namespace rtc {

class ByteBufferReader;
class ByteBufferWriter;

///////////////////////////////////////////////////////////////////////////////

// Implements a socket adapter that can buffer and process data internally,
// as in the case of connecting to a proxy, where you must speak the proxy
// protocol before commencing normal socket behavior.
class BufferedReadAdapter : public AsyncSocketAdapter {
 public:
  BufferedReadAdapter(AsyncSocket* socket, size_t buffer_size);
  ~BufferedReadAdapter() override;

  int Send(const void* pv, size_t cb) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;

 protected:
  int DirectSend(const void* pv, size_t cb) {
    return AsyncSocketAdapter::Send(pv, cb);
  }

  void BufferInput(bool on = true);
  virtual void ProcessInput(char* data, size_t* len) = 0;

  void OnReadEvent(AsyncSocket* socket) override;

 private:
  char * buffer_;
  size_t buffer_size_, data_len_;
  bool buffering_;
  RTC_DISALLOW_COPY_AND_ASSIGN(BufferedReadAdapter);
};

///////////////////////////////////////////////////////////////////////////////

// Interface for implementing proxy server sockets.
class AsyncProxyServerSocket : public BufferedReadAdapter {
 public:
  AsyncProxyServerSocket(AsyncSocket* socket, size_t buffer_size);
  ~AsyncProxyServerSocket() override;
  sigslot::signal2<AsyncProxyServerSocket*,
                   const SocketAddress&>  SignalConnectRequest;
  virtual void SendConnectResult(int err, const SocketAddress& addr) = 0;
};

///////////////////////////////////////////////////////////////////////////////

// Implements a socket adapter that performs the client side of a
// fake SSL handshake. Used for "ssltcp" P2P functionality.
class AsyncSSLSocket : public BufferedReadAdapter {
 public:
  explicit AsyncSSLSocket(AsyncSocket* socket);

  int Connect(const SocketAddress& addr) override;

 protected:
  void OnConnectEvent(AsyncSocket* socket) override;
  void ProcessInput(char* data, size_t* len) override;
  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncSSLSocket);
};

// Implements a socket adapter that performs the server side of a
// fake SSL handshake. Used when implementing a relay server that does "ssltcp".
class AsyncSSLServerSocket : public BufferedReadAdapter {
 public:
  explicit AsyncSSLServerSocket(AsyncSocket* socket);

 protected:
  void ProcessInput(char* data, size_t* len) override;
  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncSSLServerSocket);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_SOCKETADAPTERS_H_
