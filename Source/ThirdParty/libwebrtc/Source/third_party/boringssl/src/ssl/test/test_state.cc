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

#include "test_state.h"

#include <openssl/ssl.h>

#include "../../crypto/internal.h"
#include "../internal.h"

using namespace bssl;

static CRYPTO_once_t g_once = CRYPTO_ONCE_INIT;
static int g_state_index = 0;
// Some code treats the zero time special, so initialize the clock to a
// non-zero time.
static timeval g_clock = { 1234, 1234 };

static void TestStateExFree(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                            int index, long argl, void *argp) {
  delete ((TestState *)ptr);
}

static bool InitGlobals() {
  CRYPTO_once(&g_once, [] {
    g_state_index =
        SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, TestStateExFree);
  });
  return g_state_index >= 0;
}

struct timeval *GetClock() {
  return &g_clock;
}

void AdvanceClock(unsigned seconds) {
  g_clock.tv_sec += seconds;
}

bool SetTestState(SSL *ssl, std::unique_ptr<TestState> state) {
  if (!InitGlobals()) {
    return false;
  }
  // `SSL_set_ex_data` takes ownership of `state` only on success.
  if (SSL_set_ex_data(ssl, g_state_index, state.get()) == 1) {
    state.release();
    return true;
  }
  return false;
}

TestState *GetTestState(const SSL *ssl) {
  if (!InitGlobals()) {
    return nullptr;
  }
  return static_cast<TestState *>(SSL_get_ex_data(ssl, g_state_index));
}

static void ssl_ctx_add_session(SSL_SESSION *session, void *void_param) {
  SSL_CTX *ctx = reinterpret_cast<SSL_CTX *>(void_param);
  // Make a copy of the session.
  // TODO(crbug.com/527997772): Remove this copy.
  uint8_t *data;
  size_t len;
  if (!SSL_SESSION_to_bytes(session, &data, &len)) {
    fprintf(stderr, "Error copying SSL_SESSION.\n");
  }
  UniquePtr<uint8_t> free_data(data);
  UniquePtr<SSL_SESSION> new_session(SSL_SESSION_from_bytes(data, len, ctx));
  if (new_session != nullptr) {
    SSL_CTX_add_session(ctx, new_session.get());
  }
}

void CopySessions(SSL_CTX *dst, const SSL_CTX *src) {
  lh_SSL_SESSION_doall_arg(FromOpaque(src)->sessions, ssl_ctx_add_session, dst);
}
