// Copyright 2014 The BoringSSL Authors
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

#include "packeted_bio.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <openssl/bio.h>
#include <openssl/mem.h>
#include <openssl/span.h>
#include <openssl/ssl.h>

#include "../../crypto/internal.h"


using namespace bssl;

namespace {

constexpr uint8_t kOpcodePacket = 'P';
constexpr uint8_t kOpcodeTimeout = 'T';
constexpr uint8_t kOpcodeTimeoutAck = 't';
constexpr uint8_t kOpcodeMTU = 'M';
constexpr uint8_t kOpcodeExpectNextTimeout = 'E';
constexpr uint8_t kOpcodeUpdateTimeout = 'U';

struct PacketedBio {
  PacketedBio(timeval *clock_arg, SSL *ssl_arg)
      : clock(clock_arg), ssl(ssl_arg) {}
  timeval *clock = nullptr;
  SSL *ssl = nullptr;
  std::function<bool(void)> interrupt;
};

static int PacketedBioMethodType() {
  static int type = [] {
    int idx = BIO_get_new_index();
    BSSL_CHECK(idx > 0);
    return idx | BIO_TYPE_FILTER;
  }();
  return type;
}

PacketedBio *GetData(BIO *bio) {
  if (BIO_method_type(bio) != PacketedBioMethodType()) {
    return nullptr;
  }
  return static_cast<PacketedBio *>(BIO_get_data(bio));
}

// ReadAll reads |len| bytes from |bio| into |out|. It returns 1 on success and
// 0 or -1 on error.
static int ReadAll(BIO *bio, bssl::Span<uint8_t> out) {
  while (!out.empty()) {
    int ret = BIO_read(bio, out.data(),
                       static_cast<int>(std::min(out.size(), size_t{INT_MAX})));
    if (ret <= 0) {
      return ret;
    }
    out = out.subspan(ret);
  }
  return 1;
}

static int PacketedWrite(BIO *bio, const char *in, int inl) {
  BIO *next = BIO_next(bio);
  if (next == nullptr) {
    return -1;
  }

  BIO_clear_retry_flags(bio);

  // Write the header.
  uint8_t header[5];
  header[0] = kOpcodePacket;
  CRYPTO_store_u32_be(header + 1, inl);
  int ret = BIO_write(next, header, sizeof(header));
  if (ret <= 0) {
    BIO_copy_next_retry(bio);
    return ret;
  }

  // Write the buffer.
  ret = BIO_write(next, in, inl);
  if (ret < 0 || (inl > 0 && ret == 0)) {
    BIO_copy_next_retry(bio);
    return ret;
  }
  assert(ret == inl);
  return ret;
}

static int PacketedRead(BIO *bio, char *out, int outl) {
  PacketedBio *data = GetData(bio);
  BIO *next = BIO_next(bio);
  if (next == nullptr) {
    return -1;
  }

  BIO_clear_retry_flags(bio);

  if (data->interrupt) {
    BIO_set_retry_read(bio);
    return -1;
  }

  // Read the opcode.
  uint8_t opcode;
  int ret = ReadAll(next, bssl::Span(&opcode, 1));
  if (ret <= 0) {
    BIO_copy_next_retry(bio);
    return ret;
  }

  switch (opcode) {
    case kOpcodeTimeout: {
      // Process the timeout.
      uint8_t buf[8];
      ret = ReadAll(next, buf);
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }
      uint64_t timeout = CRYPTO_load_u64_be(buf);
      timeout /= 1000;  // Convert nanoseconds to microseconds.
      data->interrupt = [=] { return PacketedBioAdvanceClock(bio, timeout); };

      // Send an ACK to the peer.
      ret = BIO_write(next, &kOpcodeTimeoutAck, 1);
      if (ret <= 0) {
        return ret;
      }
      assert(ret == 1);
      BIO_set_retry_read(bio);
      return -1;
    }

    case kOpcodeMTU: {
      uint8_t buf[4];
      ret = ReadAll(next, buf);
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }
      uint32_t mtu = CRYPTO_load_u32_be(buf);
      data->interrupt = [=]() -> bool { return SSL_set_mtu(data->ssl, mtu); };
      BIO_set_retry_read(bio);
      return -1;
    }

    case kOpcodeExpectNextTimeout: {
      uint8_t buf[8];
      ret = ReadAll(next, buf);
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }
      uint64_t expected_ns = CRYPTO_load_u64_be(buf);
      data->interrupt = [=]() -> bool {
        timeval timeout;
        bool has_timeout = DTLSv1_get_timeout(data->ssl, &timeout);
        if (expected_ns == UINT64_MAX) {
          if (has_timeout) {
            fprintf(stderr,
                    "Expected no timeout, but got %" PRIu64 ".%06" PRIu64
                    "s.\n",
                    static_cast<uint64_t>(timeout.tv_sec),
                    static_cast<uint64_t>(timeout.tv_usec));
            return false;
          }
        } else {
          // Convert nanoseconds to microseconds.
          uint64_t expected_usec = expected_ns / 1000;
          // Split into seconds and fractional part.
          uint64_t expected_sec = expected_usec / 1000000;
          expected_usec %= 1000000;
          if (!has_timeout) {
            fprintf(stderr,
                    "Expected timeout of %" PRIu64 ".%06" PRIu64
                    "s, but got none.\n",
                    expected_sec, expected_usec);
            return false;
          }
          if (static_cast<uint64_t>(timeout.tv_sec) != expected_sec ||
              static_cast<uint64_t>(timeout.tv_usec) != expected_usec) {
            fprintf(stderr,
                    "Expected timeout of %" PRIu64 ".%06" PRIu64
                    "s, but got %" PRIu64 ".%06" PRIu64 "s.\n",
                    expected_sec, expected_usec,
                    static_cast<uint64_t>(timeout.tv_sec),
                    static_cast<uint64_t>(timeout.tv_usec));
            return false;
          }
        }
        return true;
      };

      BIO_set_retry_read(bio);
      return -1;
    }

    case kOpcodeUpdateTimeout: {
      uint8_t buf[sizeof(uint32_t)];
      ret = ReadAll(next, buf);
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }
      uint32_t duration_ms = CRYPTO_load_u32_be(buf);
      data->interrupt = [=] {
        DTLSv1_set_initial_timeout_duration(data->ssl, duration_ms);
        // A real caller is expected to immediately check the new timeout and
        // call |DTLSv1_handle_timeout| if the timeout is now expired. We do not
        // this automatically, so that the test can send ExpectNextTimeout(0)
        // first.
        return true;
      };
      BIO_set_retry_read(bio);
      return -1;
    }

    case kOpcodePacket: {
      // Read the length prefix.
      uint8_t len_bytes[4];
      ret = ReadAll(next, len_bytes);
      if (ret <= 0) {
        BIO_copy_next_retry(bio);
        return ret;
      }

      std::vector<uint8_t> buf(CRYPTO_load_u32_be(len_bytes), 0);
      ret = ReadAll(next, bssl::Span(buf));
      if (ret <= 0) {
        fprintf(stderr, "Packeted BIO was truncated\n");
        return -1;
      }

      if (static_cast<size_t>(outl) > buf.size()) {
        outl = static_cast<int>(buf.size());
      }
      OPENSSL_memcpy(out, buf.data(), outl);
      return outl;
    }
    default:
      fprintf(stderr, "Unknown opcode, %u\n", opcode);
      return -1;
  }
}

static long PacketedCtrl(BIO *bio, int cmd, long num, void *ptr) {
  BIO *next = BIO_next(bio);
  if (next == nullptr) {
    return 0;
  }

  BIO_clear_retry_flags(bio);
  long ret = BIO_ctrl(next, cmd, num, ptr);
  BIO_copy_next_retry(bio);
  return ret;
}

static int PacketedFree(BIO *bio) {
  if (bio == nullptr) {
    return 0;
  }

  delete GetData(bio);
  return 1;
}

static long PacketedCallbackCtrl(BIO *bio, int cmd, BIO_info_cb *fp) {
  BIO *next = BIO_next(bio);
  if (next == nullptr) {
    return 0;
  }
  return BIO_callback_ctrl(next, cmd, fp);
}

static const BIO_METHOD *PacketedBioMethod() {
  static const BIO_METHOD *method = [] {
    BIO_METHOD *ret = BIO_meth_new(PacketedBioMethodType(), "packeted bio");
    BSSL_CHECK(ret);
    BSSL_CHECK(BIO_meth_set_write(ret, PacketedWrite));
    BSSL_CHECK(BIO_meth_set_read(ret, PacketedRead));
    BSSL_CHECK(BIO_meth_set_ctrl(ret, PacketedCtrl));
    BSSL_CHECK(BIO_meth_set_destroy(ret, PacketedFree));
    BSSL_CHECK(BIO_meth_set_callback_ctrl(ret, PacketedCallbackCtrl));
    return ret;
  }();
  return method;
}

}  // namespace

bssl::UniquePtr<BIO> PacketedBioCreate(timeval *clock, SSL *ssl) {
  bssl::UniquePtr<BIO> bio(BIO_new(PacketedBioMethod()));
  if (!bio) {
    return nullptr;
  }
  BIO_set_data(bio.get(), new PacketedBio(clock, ssl));
  BIO_set_init(bio.get(), 1);
  return bio;
}

bool PacketedBioHasInterrupt(BIO *bio) {
  PacketedBio *data = GetData(bio);
  return data != nullptr && data->interrupt;
}

bool PacketedBioHandleInterrupt(BIO *bio) {
  PacketedBio *data = GetData(bio);
  if (data == nullptr || !data->interrupt) {
    return false;
  }
  return std::exchange(data->interrupt, {})();
}

bool PacketedBioAdvanceClock(BIO *bio, uint64_t microseconds) {
  PacketedBio *data = GetData(bio);
  if (data == nullptr) {
    return false;
  }

  data->clock->tv_sec += microseconds / 1000000;
  data->clock->tv_usec += microseconds % 1000000;
  if (data->clock->tv_usec >= 1000000) {
    data->clock->tv_usec -= 1000000;
    data->clock->tv_sec++;
  }

  // If DTLSv1_handle_timeout returns 1 (successfully handled timer), 0 (no
  // timers), or SSL_ERROR_WANT_WRITE (handled timeout but transport wasn't
  // ready), retry the operation.
  int ret = DTLSv1_handle_timeout(data->ssl);
  return ret >= 0 || SSL_get_error(data->ssl, ret) == SSL_ERROR_WANT_WRITE;
}
