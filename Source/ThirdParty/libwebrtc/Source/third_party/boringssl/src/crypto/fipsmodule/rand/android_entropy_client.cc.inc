// Copyright 2020 The BoringSSL Authors
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

#include <openssl/base.h>

#if defined(BORINGSSL_FIPS) && defined(OPENSSL_ANDROID)

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "internal.h"

BSSL_NAMESPACE_BEGIN

int bssl_get_seed_from_daemon(uint8_t *out_entropy, size_t *inout_entropy_len) {
  const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    return 0;
  }

  struct sockaddr_un sun;
  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  static const char kSocketPath[] = "/dev/socket/prng_seeder";
  static_assert(sizeof(kSocketPath) <= UNIX_PATH_MAX, "kSocketPath too long");
  OPENSSL_memcpy(sun.sun_path, kSocketPath, sizeof(kSocketPath));

  int ret = 0;
  size_t done = 0;
  if (connect(sock, (struct sockaddr *)&sun, sizeof(sun))) {
    goto out;
  }

  uint8_t buffer[BSSL_ENTROPY_DAEMON_RESPONSE_LEN];
  while (done < sizeof(buffer)) {
    ssize_t n;
    do {
      n = read(sock, buffer + done, sizeof(buffer) - done);
    } while (n == -1 && errno == EINTR);

    if (n < 1) {
      goto out;
    }
    done += n;
  }

  if (done != BSSL_ENTROPY_DAEMON_RESPONSE_LEN) {
    // The daemon should always write |BSSL_ENTROPY_DAEMON_RESPONSE_LEN| bytes
    // on every connection.
    goto out;
  }

  if (*inout_entropy_len > sizeof(buffer)) {
    *inout_entropy_len = sizeof(buffer);
  }
  OPENSSL_memcpy(out_entropy, buffer, *inout_entropy_len);
  ret = 1;

out:
  if (sock >= 0) {
    close(sock);
  }
  return ret;
}

BSSL_NAMESPACE_END

#endif  // FIPS && ANDROID
