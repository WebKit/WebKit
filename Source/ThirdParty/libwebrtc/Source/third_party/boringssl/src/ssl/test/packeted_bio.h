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

#ifndef HEADER_PACKETED_BIO
#define HEADER_PACKETED_BIO

#include <functional>

#include <openssl/base.h>
#include <openssl/bio.h>

#if defined(OPENSSL_WINDOWS)
#include <winsock2.h>
#else
#include <sys/time.h>
#endif


// PacketedBioCreate creates a filter BIO which implements a reliable in-order
// blocking datagram socket. It uses the value of |*clock| as the clock.
//
// During a |BIO_read|, the peer may interrupt the filter BIO to perform
// operations on |ssl|, such as handling timeouts or updating the MTU. In this
// case, the |BIO_read| operation will fail with a retryable error, which should
// be surfaced from |ssl| as |SSL_ERROR_WANT_READ|. The caller must then call
// |PacketedBioHasInterrupt| and |PacketedBioHandleInterrupt| to handle the
// interrupt.
//
// Pending operations are deferred so that they are not triggered reentrantly in
// the middle of an operation on |ssl|.
bssl::UniquePtr<BIO> PacketedBioCreate(timeval *clock, SSL *ssl);

// PacketedBioHasInterrupt returns whether |bio| has a pending interrupt. If it
// returns true, the caller must call |PacketedBioHandleInterrupt| to handle it.
bool PacketedBioHasInterrupt(BIO *bio);

// PacketedBioHandleInterrupt handles the pending interrupt on |bio|. It returns
// true on success, in which case the caller should retry the operation, and
// false on error.
bool PacketedBioHandleInterrupt(BIO *bio);

// PacketedBioAdvanceClock advances the clock by |microseconds| and handles the
// timeout on the |SSL| object. It returns true on success and false on error.
bool PacketedBioAdvanceClock(BIO *bio, uint64_t microseconds);

#endif  // HEADER_PACKETED_BIO
