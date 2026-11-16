// Copyright 2018 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_TEST_STATE
#define HEADER_TEST_STATE

#include <openssl/base.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mock_quic_transport.h"

struct TestState {
  // async_bio is async BIO which pauses reads and writes.
  BIO *async_bio = nullptr;
  // packeted_bio is the packeted BIO which simulates read timeouts.
  BIO *packeted_bio = nullptr;
  std::unique_ptr<MockQuicTransport> quic_transport;
  bool cert_ready = false;
  bssl::UniquePtr<SSL_SESSION> session;
  bssl::UniquePtr<SSL_SESSION> pending_session;
  bool early_callback_called = false;
  bool handshake_done = false;
  // private_key is the underlying private key used when testing custom keys.
  bssl::UniquePtr<EVP_PKEY> private_key;
  // When private key methods are used, whether the private key was used.
  bool used_private_key = false;
  std::vector<uint8_t> private_key_result;
  // private_key_retries is the number of times an asynchronous private key
  // operation has been retried.
  unsigned private_key_retries = 0;
  bool got_new_session = false;
  bssl::UniquePtr<SSL_SESSION> new_session;
  bool async_ticket_decrypt_ready = false;
  bool ticket_decrypt_done = false;
  bool alpn_select_done = false;
  bool early_callback_ready = false;
  bool custom_verify_ready = false;
  std::string msg_callback_text;
  bool msg_callback_ok = true;
  // cert_verified is true if certificate verification has been driven to
  // completion. This tests that the callback is not called again after this.
  bool cert_verified = false;
  int explicit_renegotiates = 0;
  std::function<bool(const SSL_CLIENT_HELLO*)> get_handshake_hints_cb;
  int last_message_received = -1;
  int selected_credential = -1;
};

bool SetTestState(SSL *ssl, std::unique_ptr<TestState> state);

TestState *GetTestState(const SSL *ssl);

struct timeval *GetClock();

void AdvanceClock(unsigned seconds);

void CopySessions(SSL_CTX *dest, const SSL_CTX *src);

#endif  // HEADER_TEST_STATE
