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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <memory>

#include <openssl/bytestring.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "handshake_util.h"
#include "test_config.h"
#include "test_state.h"

using namespace bssl;

namespace {

ssize_t read_eintr(int fd, void *out, size_t len) {
  ssize_t ret;
  do {
    ret = read(fd, out, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

ssize_t write_eintr(int fd, const void *in, size_t len) {
  ssize_t ret;
  do {
    ret = write(fd, in, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

bool GenerateHandshakeHint(const TestConfig *config,
                           bssl::Span<const uint8_t> request, int control) {
  // The handshake hint contains the ClientHello and the capabilities string.
  CBS cbs = request;
  CBS client_hello, capabilities;
  if (!CBS_get_u24_length_prefixed(&cbs, &client_hello) ||
      !CBS_get_u24_length_prefixed(&cbs, &capabilities) ||  //
      CBS_len(&cbs) != 0) {
    fprintf(stderr, "Handshaker: Could not parse hint request\n");
    return false;
  }

  UniquePtr<SSL_CTX> ctx = config->SetupCtx(/*old_ctx=*/nullptr);
  if (!ctx) {
    return false;
  }

  UniquePtr<SSL> ssl = config->NewSSL(ctx.get(), /*session=*/nullptr,
                                      std::make_unique<TestState>());
  if (!ssl) {
    fprintf(stderr, "Error creating SSL object in handshaker.\n");
    ERR_print_errors_fp(stderr);
    return false;
  }

  // TODO(davidben): When split handshakes is replaced, move this into `NewSSL`.
  assert(config->is_server);
  SSL_set_accept_state(ssl.get());

  if (!SSL_request_handshake_hints(
          ssl.get(), CBS_data(&client_hello), CBS_len(&client_hello),
          CBS_data(&capabilities), CBS_len(&capabilities))) {
    fprintf(stderr, "Handshaker: SSL_request_handshake_hints failed\n");
    return false;
  }

  int ret = 0;
  do {
    ret = CheckIdempotentError("SSL_do_handshake", ssl.get(),
                               [&] { return SSL_do_handshake(ssl.get()); });
  } while (RetryAsync(ssl.get(), ret));

  if (ret > 0) {
    fprintf(stderr, "Handshaker: handshake unexpectedly succeeded.\n");
    return false;
  }

  if (SSL_get_error(ssl.get(), ret) != SSL_ERROR_HANDSHAKE_HINTS_READY) {
    // Errors here may be expected if the test is testing a failing case. The
    // shim should continue executing without a hint, so we report an error
    // "successfully". This allows the shim to distinguish this from the other
    // unexpected error cases.
    //
    // We intentionally avoid printing the error in this case, to avoid mixing
    // up test expectations with errors from the shim.
    char msg = kControlMsgError;
    if (write_eintr(control, &msg, 1) == -1) {
      return false;
    }
    return true;
  }

  bssl::ScopedCBB hints;
  if (!CBB_init(hints.get(), 256) ||
      !SSL_serialize_handshake_hints(ssl.get(), hints.get())) {
    fprintf(stderr, "Handshaker: failed to serialize handshake hints\n");
    return false;
  }

  char msg = kControlMsgDone;
  if (write_eintr(control, &msg, 1) == -1 ||
      write_eintr(control, CBB_data(hints.get()), CBB_len(hints.get())) == -1) {
    perror("write");
    return false;
  }

  return true;
}

int SignalUnimplemented() {
  const char msg = kControlMsgUnimplemented;
  if (write_eintr(kFdControl, &msg, 1) != 1) {
    return 2;
  }
  return 1;
}

int SignalError() {
  const char msg = kControlMsgError;
  if (write_eintr(kFdControl, &msg, 1) != 1) {
    return 2;
  }
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  // Read the request before parsing the configuration. This ensures that
  // flag-parsing errors are signaled at a reliable point in time. read() will
  // return the entire message in one go, because it's a datagram socket.
  constexpr size_t kBufSize = 1024 * 1024;
  std::vector<uint8_t> request(kBufSize);
  ssize_t len = read_eintr(kFdControl, request.data(), request.size());
  if (len == -1) {
    perror("read");
    return 2;
  }
  request.resize(static_cast<size_t>(len));

  TestConfig initial_config, resume_config, retry_config;
  if (!ParseConfig(argc - 1, argv + 1, /*is_shim=*/false, &initial_config,
                   &resume_config, &retry_config)) {
    return SignalUnimplemented();
  }
  const TestConfig *config =
      initial_config.handshaker_resume ? &resume_config : &initial_config;
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  if (initial_config.fuzzer_mode) {
    CRYPTO_set_fuzzer_mode(1);
  }
  if (initial_config.handshaker_resume) {
    // If the PRNG returns exactly the same values when trying to resume then a
    // "random" session ID will happen to exactly match the session ID
    // "randomly" generated on the initial connection. The client will thus
    // incorrectly believe that the server is resuming.
    uint8_t byte;
    RAND_bytes(&byte, 1);
  }
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

  if (!config->handshake_hints) {
    // Historically omitting -handshake-hints ran the split handshakes mode.
    fprintf(stderr, "Handshaker missing -handshake-hints flag.");
    return SignalError();
  }
  if (!GenerateHandshakeHint(config, request, kFdControl)) {
    return SignalError();
  }
  return 0;
}
