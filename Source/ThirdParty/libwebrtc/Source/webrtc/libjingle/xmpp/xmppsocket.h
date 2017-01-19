/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPSOCKET_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPSOCKET_H_

#include "webrtc/libjingle/xmpp/asyncsocket.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/sigslot.h"

// The below define selects the SSLStreamAdapter implementation for
// SSL, as opposed to the SSLAdapter socket adapter.
// #define USE_SSLSTREAM

namespace rtc {
  class StreamInterface;
  class SocketAddress;
};
extern rtc::AsyncSocket* cricket_socket_;

namespace buzz {

class XmppSocket : public buzz::AsyncSocket, public sigslot::has_slots<> {
public:
  XmppSocket(buzz::TlsOptions tls);
  ~XmppSocket();

  virtual buzz::AsyncSocket::State state();
  virtual buzz::AsyncSocket::Error error();
  virtual int GetError();

  virtual bool Connect(const rtc::SocketAddress& addr);
  virtual bool Read(char * data, size_t len, size_t* len_read);
  virtual bool Write(const char * data, size_t len);
  virtual bool Close();
  virtual bool StartTls(const std::string & domainname);

  sigslot::signal1<int> SignalCloseEvent;

private:
  void CreateCricketSocket(int family);
#ifndef USE_SSLSTREAM
  void OnReadEvent(rtc::AsyncSocket * socket);
  void OnWriteEvent(rtc::AsyncSocket * socket);
  void OnConnectEvent(rtc::AsyncSocket * socket);
  void OnCloseEvent(rtc::AsyncSocket * socket, int error);
#else  // USE_SSLSTREAM
  void OnEvent(rtc::StreamInterface* stream, int events, int err);
#endif  // USE_SSLSTREAM

  rtc::AsyncSocket * cricket_socket_;
#ifdef USE_SSLSTREAM
  rtc::StreamInterface *stream_;
#endif  // USE_SSLSTREAM
  buzz::AsyncSocket::State state_;
  rtc::Buffer buffer_;
  buzz::TlsOptions tls_;
};

}  // namespace buzz

#endif // WEBRTC_LIBJINGLE_XMPP_XMPPSOCKET_H_

