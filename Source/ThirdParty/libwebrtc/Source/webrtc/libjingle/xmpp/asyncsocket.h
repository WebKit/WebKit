/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_ASYNCSOCKET_H_
#define WEBRTC_LIBJINGLE_XMPP_ASYNCSOCKET_H_

#include <string>

#include "webrtc/base/sigslot.h"

namespace rtc {
  class SocketAddress;
}

namespace buzz {

class AsyncSocket {
public:
  enum State {
    STATE_CLOSED = 0,      //!< Socket is not open.
    STATE_CLOSING,         //!< Socket is closing but can have buffered data
    STATE_CONNECTING,      //!< In the process of
    STATE_OPEN,            //!< Socket is connected
#if defined(FEATURE_ENABLE_SSL)
    STATE_TLS_CONNECTING,  //!< Establishing TLS connection
    STATE_TLS_OPEN,        //!< TLS connected
#endif
  };

  enum Error {
    ERROR_NONE = 0,         //!< No error
    ERROR_WINSOCK,          //!< Winsock error
    ERROR_DNS,              //!< Couldn't resolve host name
    ERROR_WRONGSTATE,       //!< Call made while socket is in the wrong state
#if defined(FEATURE_ENABLE_SSL)
    ERROR_SSL,              //!< Something went wrong with OpenSSL
#endif
  };

  virtual ~AsyncSocket() {}
  virtual State state() = 0;
  virtual Error error() = 0;
  virtual int GetError() = 0;    // winsock error code

  virtual bool Connect(const rtc::SocketAddress& addr) = 0;
  virtual bool Read(char * data, size_t len, size_t* len_read) = 0;
  virtual bool Write(const char * data, size_t len) = 0;
  virtual bool Close() = 0;
#if defined(FEATURE_ENABLE_SSL)
  // We allow matching any passed domain.  This allows us to avoid
  // handling the valuable certificates for logins into proxies.  If
  // both names are passed as empty, we do not require a match.
  virtual bool StartTls(const std::string & domainname) = 0;
#endif

  sigslot::signal0<> SignalConnected;
  sigslot::signal0<> SignalSSLConnected;
  sigslot::signal0<> SignalClosed;
  sigslot::signal0<> SignalRead;
  sigslot::signal0<> SignalError;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_ASYNCSOCKET_H_
