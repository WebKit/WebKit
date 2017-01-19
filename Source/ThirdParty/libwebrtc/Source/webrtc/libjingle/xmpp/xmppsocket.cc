/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "xmppsocket.h"

#include <errno.h>
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#ifdef FEATURE_ENABLE_SSL
#include "webrtc/base/ssladapter.h"
#endif

#ifdef USE_SSLSTREAM
#include "webrtc/base/socketstream.h"
#ifdef FEATURE_ENABLE_SSL
#include "webrtc/base/sslstreamadapter.h"
#endif  // FEATURE_ENABLE_SSL
#endif  // USE_SSLSTREAM

namespace buzz {

XmppSocket::XmppSocket(buzz::TlsOptions tls) : cricket_socket_(NULL),
                                               tls_(tls) {
  state_ = buzz::AsyncSocket::STATE_CLOSED;
}

void XmppSocket::CreateCricketSocket(int family) {
  rtc::Thread* pth = rtc::Thread::Current();
  if (family == AF_UNSPEC) {
    family = AF_INET;
  }
  rtc::AsyncSocket* socket =
      pth->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
#ifndef USE_SSLSTREAM
#ifdef FEATURE_ENABLE_SSL
  if (tls_ != buzz::TLS_DISABLED) {
    socket = rtc::SSLAdapter::Create(socket);
  }
#endif  // FEATURE_ENABLE_SSL
  cricket_socket_ = socket;
  cricket_socket_->SignalReadEvent.connect(this, &XmppSocket::OnReadEvent);
  cricket_socket_->SignalWriteEvent.connect(this, &XmppSocket::OnWriteEvent);
  cricket_socket_->SignalConnectEvent.connect(this,
                                              &XmppSocket::OnConnectEvent);
  cricket_socket_->SignalCloseEvent.connect(this, &XmppSocket::OnCloseEvent);
#else  // USE_SSLSTREAM
  cricket_socket_ = socket;
  stream_ = new rtc::SocketStream(cricket_socket_);
#ifdef FEATURE_ENABLE_SSL
  if (tls_ != buzz::TLS_DISABLED)
    stream_ = rtc::SSLStreamAdapter::Create(stream_);
#endif  // FEATURE_ENABLE_SSL
  stream_->SignalEvent.connect(this, &XmppSocket::OnEvent);
#endif  // USE_SSLSTREAM
}

XmppSocket::~XmppSocket() {
  Close();
#ifndef USE_SSLSTREAM
  delete cricket_socket_;
#else  // USE_SSLSTREAM
  delete stream_;
#endif  // USE_SSLSTREAM
}

#ifndef USE_SSLSTREAM
void XmppSocket::OnReadEvent(rtc::AsyncSocket * socket) {
  SignalRead();
}

void XmppSocket::OnWriteEvent(rtc::AsyncSocket * socket) {
  // Write bytes if there are any
  while (buffer_.size() > 0) {
    int written = cricket_socket_->Send(buffer_.data(), buffer_.size());
    if (written > 0) {
      ASSERT(static_cast<size_t>(written) <= buffer_.size());
      memmove(buffer_.data(), buffer_.data() + written,
          buffer_.size() - written);
      buffer_.SetSize(buffer_.size() - written);
      continue;
    }
    if (!cricket_socket_->IsBlocking())
      LOG(LS_ERROR) << "Send error: " << cricket_socket_->GetError();
    return;
  }
}

void XmppSocket::OnConnectEvent(rtc::AsyncSocket * socket) {
#if defined(FEATURE_ENABLE_SSL)
  if (state_ == buzz::AsyncSocket::STATE_TLS_CONNECTING) {
    state_ = buzz::AsyncSocket::STATE_TLS_OPEN;
    SignalSSLConnected();
    OnWriteEvent(cricket_socket_);
    return;
  }
#endif  // !defined(FEATURE_ENABLE_SSL)
  state_ = buzz::AsyncSocket::STATE_OPEN;
  SignalConnected();
}

void XmppSocket::OnCloseEvent(rtc::AsyncSocket * socket, int error) {
  SignalCloseEvent(error);
}

#else  // USE_SSLSTREAM

void XmppSocket::OnEvent(rtc::StreamInterface* stream,
                         int events, int err) {
  if ((events & rtc::SE_OPEN)) {
#if defined(FEATURE_ENABLE_SSL)
    if (state_ == buzz::AsyncSocket::STATE_TLS_CONNECTING) {
      state_ = buzz::AsyncSocket::STATE_TLS_OPEN;
      SignalSSLConnected();
      events |= rtc::SE_WRITE;
    } else
#endif
    {
      state_ = buzz::AsyncSocket::STATE_OPEN;
      SignalConnected();
    }
  }
  if ((events & rtc::SE_READ))
    SignalRead();
  if ((events & rtc::SE_WRITE)) {
    // Write bytes if there are any
    while (buffer_.size() > 0) {
      rtc::StreamResult result;
      size_t written;
      int error;
      result = stream_->Write(buffer_.data(), buffer_.size(),
                              &written, &error);
      if (result == rtc::SR_ERROR) {
        LOG(LS_ERROR) << "Send error: " << error;
        return;
      }
      if (result == rtc::SR_BLOCK)
        return;
      ASSERT(result == rtc::SR_SUCCESS);
      ASSERT(written > 0);
      ASSERT(written <= buffer_.size());
      memmove(buffer_.data(), buffer_.data() + written,
          buffer_.size() - written);
      buffer_.SetSize(buffer_.size() - written);
    }
  }
  if ((events & rtc::SE_CLOSE))
    SignalCloseEvent(err);
}
#endif  // USE_SSLSTREAM

buzz::AsyncSocket::State XmppSocket::state() {
  return state_;
}

buzz::AsyncSocket::Error XmppSocket::error() {
  return buzz::AsyncSocket::ERROR_NONE;
}

int XmppSocket::GetError() {
  return 0;
}

bool XmppSocket::Connect(const rtc::SocketAddress& addr) {
  if (cricket_socket_ == NULL) {
    CreateCricketSocket(addr.family());
  }
  if (cricket_socket_->Connect(addr) < 0) {
    return cricket_socket_->IsBlocking();
  }
  return true;
}

bool XmppSocket::Read(char * data, size_t len, size_t* len_read) {
#ifndef USE_SSLSTREAM
  int read = cricket_socket_->Recv(data, len, nullptr);
  if (read > 0) {
    *len_read = (size_t)read;
    return true;
  }
#else  // USE_SSLSTREAM
  rtc::StreamResult result = stream_->Read(data, len, len_read, NULL);
  if (result == rtc::SR_SUCCESS)
    return true;
#endif  // USE_SSLSTREAM
  return false;
}

bool XmppSocket::Write(const char * data, size_t len) {
  buffer_.AppendData(data, len);
#ifndef USE_SSLSTREAM
  OnWriteEvent(cricket_socket_);
#else  // USE_SSLSTREAM
  OnEvent(stream_, rtc::SE_WRITE, 0);
#endif  // USE_SSLSTREAM
  return true;
}

bool XmppSocket::Close() {
  if (state_ != buzz::AsyncSocket::STATE_OPEN)
    return false;
#ifndef USE_SSLSTREAM
  if (cricket_socket_->Close() == 0) {
    state_ = buzz::AsyncSocket::STATE_CLOSED;
    SignalClosed();
    return true;
  }
  return false;
#else  // USE_SSLSTREAM
  state_ = buzz::AsyncSocket::STATE_CLOSED;
  stream_->Close();
  SignalClosed();
  return true;
#endif  // USE_SSLSTREAM
}

bool XmppSocket::StartTls(const std::string & domainname) {
#if defined(FEATURE_ENABLE_SSL)
  if (tls_ == buzz::TLS_DISABLED)
    return false;
#ifndef USE_SSLSTREAM
  rtc::SSLAdapter* ssl_adapter =
    static_cast<rtc::SSLAdapter *>(cricket_socket_);
  if (ssl_adapter->StartSSL(domainname.c_str(), false) != 0)
    return false;
#else  // USE_SSLSTREAM
  rtc::SSLStreamAdapter* ssl_stream =
    static_cast<rtc::SSLStreamAdapter *>(stream_);
  if (ssl_stream->StartSSLWithServer(domainname.c_str()) != 0)
    return false;
#endif  // USE_SSLSTREAM
  state_ = buzz::AsyncSocket::STATE_TLS_CONNECTING;
  return true;
#else  // !defined(FEATURE_ENABLE_SSL)
  return false;
#endif  // !defined(FEATURE_ENABLE_SSL)
}

}  // namespace buzz

