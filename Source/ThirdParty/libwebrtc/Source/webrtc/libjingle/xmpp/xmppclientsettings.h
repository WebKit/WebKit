/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPCLIENTSETTINGS_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPCLIENTSETTINGS_H_

#include "webrtc/p2p/base/port.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/cryptstring.h"

namespace buzz {

class XmppUserSettings {
 public:
  XmppUserSettings()
    : use_tls_(buzz::TLS_DISABLED),
      allow_plain_(false) {
  }

  void set_user(const std::string& user) { user_ = user; }
  void set_host(const std::string& host) { host_ = host; }
  void set_pass(const rtc::CryptString& pass) { pass_ = pass; }
  void set_auth_token(const std::string& mechanism,
                      const std::string& token) {
    auth_mechanism_ = mechanism;
    auth_token_ = token;
  }
  void set_resource(const std::string& resource) { resource_ = resource; }
  void set_use_tls(const TlsOptions use_tls) { use_tls_ = use_tls; }
  void set_allow_plain(bool f) { allow_plain_ = f; }
  void set_test_server_domain(const std::string& test_server_domain) {
    test_server_domain_ = test_server_domain;
  }
  void set_token_service(const std::string& token_service) {
    token_service_ = token_service;
  }

  const std::string& user() const { return user_; }
  const std::string& host() const { return host_; }
  const rtc::CryptString& pass() const { return pass_; }
  const std::string& auth_mechanism() const { return auth_mechanism_; }
  const std::string& auth_token() const { return auth_token_; }
  const std::string& resource() const { return resource_; }
  TlsOptions use_tls() const { return use_tls_; }
  bool allow_plain() const { return allow_plain_; }
  const std::string& test_server_domain() const { return test_server_domain_; }
  const std::string& token_service() const { return token_service_; }

 private:
  std::string user_;
  std::string host_;
  rtc::CryptString pass_;
  std::string auth_mechanism_;
  std::string auth_token_;
  std::string resource_;
  TlsOptions use_tls_;
  bool allow_plain_;
  std::string test_server_domain_;
  std::string token_service_;
};

class XmppClientSettings : public XmppUserSettings {
 public:
  XmppClientSettings()
    : protocol_(cricket::PROTO_TCP),
      proxy_(rtc::PROXY_NONE),
      proxy_port_(80),
      use_proxy_auth_(false) {
  }

  void set_server(const rtc::SocketAddress& server) {
      server_ = server;
  }
  void set_protocol(cricket::ProtocolType protocol) { protocol_ = protocol; }
  void set_proxy(rtc::ProxyType f) { proxy_ = f; }
  void set_proxy_host(const std::string& host) { proxy_host_ = host; }
  void set_proxy_port(int port) { proxy_port_ = port; };
  void set_use_proxy_auth(bool f) { use_proxy_auth_ = f; }
  void set_proxy_user(const std::string& user) { proxy_user_ = user; }
  void set_proxy_pass(const rtc::CryptString& pass) { proxy_pass_ = pass; }

  const rtc::SocketAddress& server() const { return server_; }
  cricket::ProtocolType protocol() const { return protocol_; }
  rtc::ProxyType proxy() const { return proxy_; }
  const std::string& proxy_host() const { return proxy_host_; }
  int proxy_port() const { return proxy_port_; }
  bool use_proxy_auth() const { return use_proxy_auth_; }
  const std::string& proxy_user() const { return proxy_user_; }
  const rtc::CryptString& proxy_pass() const { return proxy_pass_; }

 private:
  rtc::SocketAddress server_;
  cricket::ProtocolType protocol_;
  rtc::ProxyType proxy_;
  std::string proxy_host_;
  int proxy_port_;
  bool use_proxy_auth_;
  std::string proxy_user_;
  rtc::CryptString proxy_pass_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPCLIENT_H_
