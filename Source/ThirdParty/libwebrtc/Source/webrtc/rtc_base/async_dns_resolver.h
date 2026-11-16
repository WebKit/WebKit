/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_ASYNC_DNS_RESOLVER_H_
#define RTC_BASE_ASYNC_DNS_RESOLVER_H_

#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/async_dns_resolver.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/platform_thread.h"  // IWYU pragma: keep
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_WIN)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace webrtc {
// This file contains a default implementation of
// AsyncDnsResolverInterface, for use when there is no need for special
// treatment.

class AsyncDnsResolverResultImpl : public AsyncDnsResolverResult {
 public:
  bool GetResolvedAddress(int family, SocketAddress* addr) const override;
  // Returns error from resolver.
  int GetError() const override;

 private:
  friend class AsyncDnsResolver;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  SocketAddress addr_ RTC_GUARDED_BY(sequence_checker_);
  std::vector<IPAddress> addresses_ RTC_GUARDED_BY(sequence_checker_);
  int error_ RTC_GUARDED_BY(sequence_checker_);
};

class RTC_EXPORT AsyncDnsResolver : public AsyncDnsResolverInterface {
 public:
  AsyncDnsResolver();
  ~AsyncDnsResolver() override;
  // Start address resolution of the hostname in `addr`.
  void Start(const SocketAddress& addr,
             absl::AnyInvocable<void()> callback) override;
  // Start address resolution of the hostname in `addr` matching `family`.
  void Start(const SocketAddress& addr,
             int family,
             absl::AnyInvocable<void()> callback) override;
  const AsyncDnsResolverResult& result() const override;

 private:
  class StateImpl;
  using State = FinalRefCountedObject<StateImpl>;
  scoped_refptr<State> state_;  // To check for the target task queue going away
  AsyncDnsResolverResultImpl result_;
  absl::AnyInvocable<void() &&> callback_;
#if defined(WEBRTC_WIN)
  OVERLAPPED ol_ = {};
  HANDLE cancel_ = nullptr;
  PADDRINFOEXW addr_info_ = nullptr;
  PlatformThread worker_;
#endif
  ScopedTaskSafety safety_;  // To check for client going away. Must be last.
};

}  // namespace webrtc
#endif  // RTC_BASE_ASYNC_DNS_RESOLVER_H_
