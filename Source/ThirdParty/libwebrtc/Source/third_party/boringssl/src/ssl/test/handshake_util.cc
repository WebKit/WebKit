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

#include "handshake_util.h"

#include <assert.h>
#if defined(HANDSHAKER_SUPPORTED)
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <functional>
#include <map>
#include <vector>

#include "async_bio.h"
#include "packeted_bio.h"
#include "test_config.h"
#include "test_state.h"

#include <openssl/bytestring.h>
#include <openssl/ssl.h>

using namespace bssl;

bool RetryAsync(SSL *ssl, int ret) {
  const TestConfig *config = GetTestConfig(ssl);
  TestState *test_state = GetTestState(ssl);
  if (ret >= 0) {
    return false;
  }

  int ssl_err = SSL_get_error(ssl, ret);
  if (ssl_err == SSL_ERROR_WANT_RENEGOTIATE && config->renegotiate_explicit) {
    test_state->explicit_renegotiates++;
    return SSL_renegotiate(ssl);
  }

  if (test_state->quic_transport && ssl_err == SSL_ERROR_WANT_READ) {
    return test_state->quic_transport->ReadHandshake();
  }

  if (!config->async) {
    // Only asynchronous tests should trigger other retries.
    return false;
  }

  if (test_state->packeted_bio != nullptr &&
      PacketedBioHasInterrupt(test_state->packeted_bio)) {
    return PacketedBioHandleInterrupt(test_state->packeted_bio);
  }

  // See if we needed to read or write more. If so, allow one byte through on
  // the appropriate end to maximally stress the state machine.
  switch (ssl_err) {
    case SSL_ERROR_WANT_READ:
      AsyncBioAllowRead(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_WRITE:
      AsyncBioAllowWrite(test_state->async_bio, 1);
      return true;
    case SSL_ERROR_WANT_X509_LOOKUP:
      test_state->cert_ready = true;
      return true;
    case SSL_ERROR_PENDING_SESSION:
      test_state->session = std::move(test_state->pending_session);
      return true;
    case SSL_ERROR_PENDING_CERTIFICATE:
      test_state->early_callback_ready = true;
      return true;
    case SSL_ERROR_WANT_PRIVATE_KEY_OPERATION:
      test_state->private_key_retries++;
      if (config->private_key_delay_ms != 0 &&
          test_state->private_key_retries == 1) {
        // The first time around, simulate the private key operation taking a
        // long time to run.
        if (test_state->packeted_bio == nullptr) {
          fprintf(stderr, "-private-key-delay-ms requires DTLS.\n");
          return false;
        }
        if (!PacketedBioAdvanceClock(test_state->packeted_bio,
                                     config->private_key_delay_ms * 1000)) {
          return false;
        }
      }
      return true;
    case SSL_ERROR_WANT_CERTIFICATE_VERIFY:
      test_state->custom_verify_ready = true;
      return true;
    case SSL_ERROR_PENDING_TICKET:
      test_state->async_ticket_decrypt_ready = true;
      return true;
    default:
      return false;
  }
}

int CheckIdempotentError(const char *name, SSL *ssl,
                         std::function<int()> func) {
  int ret = func();
  int ssl_err = SSL_get_error(ssl, ret);
  uint32_t err = ERR_peek_error();
  if (ssl_err == SSL_ERROR_SSL || ssl_err == SSL_ERROR_ZERO_RETURN) {
    int ret2 = func();
    int ssl_err2 = SSL_get_error(ssl, ret2);
    uint32_t err2 = ERR_peek_error();
    if (ret != ret2 || ssl_err != ssl_err2 || err != err2) {
      fprintf(stderr, "Repeating %s did not replay the error.\n", name);
      char buf[256];
      ERR_error_string_n(err, buf, sizeof(buf));
      fprintf(stderr, "Wanted: %d %d %s\n", ret, ssl_err, buf);
      ERR_error_string_n(err2, buf, sizeof(buf));
      fprintf(stderr, "Got:    %d %d %s\n", ret2, ssl_err2, buf);
      // runner treats kExitCodeMustFail as always failing. Otherwise, it may
      // accidentally consider the result an expected protocol failure.
      exit(kExitCodeMustFail);
    }
  }
  return ret;
}

#if defined(HANDSHAKER_SUPPORTED)

static ssize_t read_eintr(int fd, void *out, size_t len) {
  ssize_t ret;
  do {
    ret = read(fd, out, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

static ssize_t write_eintr(int fd, const void *in, size_t len) {
  ssize_t ret;
  do {
    ret = write(fd, in, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

static ssize_t waitpid_eintr(pid_t pid, int *wstatus, int options) {
  pid_t ret;
  do {
    ret = waitpid(pid, wstatus, options);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

class ScopedFD {
 public:
  ScopedFD() : fd_(-1) {}
  explicit ScopedFD(int fd) : fd_(fd) {}
  ~ScopedFD() { Reset(); }

  ScopedFD(ScopedFD &&other) { *this = std::move(other); }
  ScopedFD &operator=(ScopedFD &&other) {
    Reset(other.fd_);
    other.fd_ = -1;
    return *this;
  }

  int fd() const { return fd_; }

  void Reset(int fd = -1) {
    if (fd_ >= 0) {
      close(fd_);
    }
    fd_ = fd;
  }

 private:
  int fd_;
};

class ScopedProcess {
 public:
  ScopedProcess() : pid_(-1) {}
  ~ScopedProcess() { Reset(); }

  ScopedProcess(ScopedProcess &&other) { *this = std::move(other); }
  ScopedProcess &operator=(ScopedProcess &&other) {
    Reset(other.pid_);
    other.pid_ = -1;
    return *this;
  }

  pid_t pid() const { return pid_; }

  void Reset(pid_t pid = -1) {
    if (pid_ >= 0) {
      kill(pid_, SIGTERM);
      int unused;
      Wait(&unused);
    }
    pid_ = pid;
  }

  bool Wait(int *out_status) {
    if (pid_ < 0) {
      return false;
    }
    if (waitpid_eintr(pid_, out_status, 0) != pid_) {
      return false;
    }
    pid_ = -1;
    return true;
  }

 private:
  pid_t pid_;
};

class FileActionsDestroyer {
 public:
  explicit FileActionsDestroyer(posix_spawn_file_actions_t *actions)
      : actions_(actions) {}
  ~FileActionsDestroyer() { posix_spawn_file_actions_destroy(actions_); }
  FileActionsDestroyer(const FileActionsDestroyer &) = delete;
  FileActionsDestroyer &operator=(const FileActionsDestroyer &) = delete;

 private:
  posix_spawn_file_actions_t *actions_;
};

// StartHandshaker starts the handshaker process and, on success, returns a
// handle to the process in `*out`. It sets `*out_control` to a control pipe to
// the process. `map_fds` maps from desired fd number in the child process to
// the source fd in the calling process. `close_fds` is the list of additional
// fds to close, which may overlap with `map_fds`. Other than stdin, stdout, and
// stderr, the status of fds not listed in either set is undefined.
static bool StartHandshaker(ScopedProcess *out, ScopedFD *out_control,
                            const TestConfig *config, bool is_resume,
                            std::map<int, int> map_fds,
                            std::vector<int> close_fds) {
  if (config->handshaker_path.empty()) {
    fprintf(stderr, "no -handshaker-path specified\n");
    return false;
  }
  struct stat dummy;
  if (stat(config->handshaker_path.c_str(), &dummy) == -1) {
    perror(config->handshaker_path.c_str());
    return false;
  }

  std::vector<const char *> args;
  args.push_back(config->handshaker_path.c_str());
  static const char kResumeFlag[] = "-handshaker-resume";
  if (is_resume) {
    args.push_back(kResumeFlag);
  }
  // config->handshaker_args omits argv[0].
  for (const char *arg : config->handshaker_args) {
    args.push_back(arg);
  }
  args.push_back(nullptr);

  // A datagram socket guarantees that writes are all-or-nothing.
  int control[2];
  if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, control) != 0) {
    perror("socketpair");
    return false;
  }
  ScopedFD scoped_control0(control[0]), scoped_control1(control[1]);
  close_fds.push_back(control[0]);
  map_fds[kFdControl] = control[1];

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    return false;
  }
  FileActionsDestroyer actions_destroyer(&actions);
  for (int fd : close_fds) {
    if (posix_spawn_file_actions_addclose(&actions, fd) != 0) {
      return false;
    }
  }
  if (!map_fds.empty()) {
    int max_fd = STDERR_FILENO;
    for (const auto &pair : map_fds) {
      max_fd = std::max(max_fd, pair.first);
      max_fd = std::max(max_fd, pair.second);
    }
    // `map_fds` may contain cycles, so make a copy of all the source fds.
    // `posix_spawn` can only use `dup2`, not `dup`, so we assume `max_fd` is
    // the last fd we care about inheriting. `temp_fds` maps from fd number in
    // the parent process to a temporary fd number in the child process.
    std::map<int, int> temp_fds;
    int next_fd = max_fd + 1;
    for (const auto &pair : map_fds) {
      if (temp_fds.count(pair.second)) {
        continue;
      }
      temp_fds[pair.second] = next_fd;
      if (posix_spawn_file_actions_adddup2(&actions, pair.second, next_fd) !=
          0 ||
          posix_spawn_file_actions_addclose(&actions, pair.second) != 0) {
        return false;
      }
      next_fd++;
    }
    for (const auto &pair : map_fds) {
      if (posix_spawn_file_actions_adddup2(&actions, temp_fds[pair.second],
                                           pair.first) != 0) {
        return false;
      }
    }
    // Clean up temporary fds.
    for (int fd = max_fd + 1; fd < next_fd; fd++) {
      if (posix_spawn_file_actions_addclose(&actions, fd) != 0) {
        return false;
      }
    }
  }

  fflush(stdout);
  fflush(stderr);

  // MSan doesn't know that `posix_spawn` initializes its output, so initialize
  // it to -1.
  pid_t pid = -1;
  if (posix_spawn(&pid, args[0], &actions, nullptr,
                  const_cast<char *const *>(args.data()), environ) != 0) {
    return false;
  }

  out->Reset(pid);
  *out_control = std::move(scoped_control0);
  return true;
}

static bool RequestHandshakeHint(const TestConfig *config, bool is_resume,
                                 Span<const uint8_t> input, bool *out_has_hints,
                                 std::vector<uint8_t> *out_hints) {
  ScopedProcess handshaker;
  ScopedFD control;
  if (!StartHandshaker(&handshaker, &control, config, is_resume, {}, {})) {
    return false;
  }

  if (write_eintr(control.fd(), input.data(), input.size()) == -1) {
    perror("write");
    return false;
  }

  char msg;
  if (read_eintr(control.fd(), &msg, 1) != 1) {
    perror("read");
    return false;
  }

  switch (msg) {
    case kControlMsgDone: {
      constexpr size_t kBufSize = 1024 * 1024;
      out_hints->resize(kBufSize);
      ssize_t len =
          read_eintr(control.fd(), out_hints->data(), out_hints->size());
      if (len == -1) {
        perror("read");
        return false;
      }
      out_hints->resize(len);
      *out_has_hints = true;
      break;
    }
    case kControlMsgError:
      *out_has_hints = false;
      break;
    case kControlMsgUnimplemented:
      exit(kExitCodeUnimplemented);
    default:
      fprintf(stderr, "Unknown control message from handshaker: %c\n", msg);
      return false;
  }

  int wstatus;
  if (!handshaker.Wait(&wstatus)) {
    perror("waitpid");
    return false;
  }
  if (wstatus) {
    fprintf(stderr, "handshaker exited irregularly\n");
    return false;
  }

  return true;
}

bool GetHandshakeHint(SSL *ssl, SettingsWriter *writer, bool is_resume,
                      const SSL_CLIENT_HELLO *client_hello) {
  ScopedCBB input;
  CBB child;
  if (!CBB_init(input.get(), client_hello->client_hello_len + 256) ||
      !CBB_add_u24_length_prefixed(input.get(), &child) ||
      !CBB_add_bytes(&child, client_hello->client_hello,
                     client_hello->client_hello_len) ||
      !CBB_add_u24_length_prefixed(input.get(), &child) ||
      !SSL_serialize_capabilities(ssl, &child) ||  //
      !CBB_flush(input.get())) {
    return false;
  }

  bool has_hints;
  std::vector<uint8_t> hints;
  const TestConfig *config = GetTestConfig(ssl);
  if (!RequestHandshakeHint(config, is_resume,
                            Span(CBB_data(input.get()), CBB_len(input.get())),
                            &has_hints, &hints)) {
    return false;
  }
  if (!has_hints) {
    // The handshaker successfully communicated to us that its handshake failed
    // before getting hints. Continue with the handshake on the shim side (and
    // presumably eventually report the error there).
    return true;
  }
  return writer->WriteHintTrace(bssl::Span(client_hello->client_hello,
                                           client_hello->client_hello_len),
                                hints) &&
         SSL_set_handshake_hints(ssl, hints.data(), hints.size());
}

#endif  // defined(HANDSHAKER_SUPPORTED)
