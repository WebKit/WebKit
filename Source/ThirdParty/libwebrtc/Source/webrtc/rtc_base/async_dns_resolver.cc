/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_dns_resolver.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/async_dns_resolver.h"
#include "api/make_ref_counted.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_POSIX)
#include <netdb.h>
#endif

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include <dispatch/dispatch.h>
#endif

namespace webrtc {

namespace {

int ResolveHostname(absl::string_view hostname,
                    int family,
                    std::vector<IPAddress>& addresses) {
  addresses.clear();
  struct addrinfo* result = nullptr;
  struct addrinfo hints = {.ai_flags = AI_ADDRCONFIG, .ai_family = family};
  // `family` here will almost always be AF_UNSPEC, because `family` comes from
  // AsyncResolver::addr_.family(), which comes from a SocketAddress constructed
  // with a hostname. When a SocketAddress is constructed with a hostname, its
  // family is AF_UNSPEC. However, if someday in the future we construct
  // a SocketAddress with both a hostname and a family other than AF_UNSPEC,
  // then it would be possible to get a specific family value here.

  // The behavior of AF_UNSPEC is roughly "get both ipv4 and ipv6", as
  // documented by the various operating systems:
  // Linux: http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
  // Windows: https://msdn.microsoft.com/en-us/library/windows/desktop/
  // ms738520(v=vs.85).aspx
  // Mac: https://developer.apple.com/legacy/library/documentation/Darwin/
  // Reference/ManPages/man3/getaddrinfo.3.html
  // Android (source code, not documentation):
  // https://android.googlesource.com/platform/bionic/+/
  // 7e0bfb511e85834d7c6cb9631206b62f82701d60/libc/netbsd/net/getaddrinfo.c#1657
  int ret =
      getaddrinfo(std::string(hostname).c_str(), nullptr, &hints, &result);
  if (ret != 0) {
    return ret;
  }
  struct addrinfo* cursor = result;
  for (; cursor; cursor = cursor->ai_next) {
    if (family == AF_UNSPEC || cursor->ai_family == family) {
      IPAddress ip;
      if (IPFromAddrInfo(cursor, &ip)) {
        addresses.push_back(ip);
      }
    }
  }
  freeaddrinfo(result);
  return 0;
}

// Special task posting for Mac/iOS
#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
void GlobalGcdRunTask(void* context) {
  std::unique_ptr<absl::AnyInvocable<void() &&>> task(
      static_cast<absl::AnyInvocable<void() &&>*>(context));
  std::move (*task)();
}

// Post a task into the system-defined global concurrent queue.
void PostTaskToGlobalQueue(
    std::unique_ptr<absl::AnyInvocable<void() &&>> task) {
  dispatch_async_f(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
      task.release(), &GlobalGcdRunTask);
}
#endif  // defined(WEBRTC_MAC) || defined(WEBRTC_IOS)

}  // namespace

class AsyncDnsResolver::State : public RefCountedBase {
 public:
  enum class Status {
    kActive,    // Running request, or able to be passed one
    kFinished,  // Request has finished processing
    kDead       // The owning AsyncDnsResolver has been deleted
  };
  static scoped_refptr<AsyncDnsResolver::State> Create() {
    return make_ref_counted<AsyncDnsResolver::State>();
  }

  // Execute the passed function if the state is Active.
  void Finish(absl::AnyInvocable<void()> function) {
    MutexLock lock(&mutex_);
    if (status_ != Status::kActive) {
      return;
    }
    status_ = Status::kFinished;
    function();
  }
  void Kill() {
    MutexLock lock(&mutex_);
    status_ = Status::kDead;
  }

 private:
  Mutex mutex_;
  Status status_ RTC_GUARDED_BY(mutex_) = Status::kActive;
};

AsyncDnsResolver::AsyncDnsResolver() : state_(State::Create()) {}

AsyncDnsResolver::~AsyncDnsResolver() {
  state_->Kill();
}

void AsyncDnsResolver::Start(const SocketAddress& addr,
                             absl::AnyInvocable<void()> callback) {
  Start(addr, addr.family(), std::move(callback));
}

// Start address resolution of the hostname in `addr` matching `family`.
void AsyncDnsResolver::Start(const SocketAddress& addr,
                             int family,
                             absl::AnyInvocable<void()> callback) {
  RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
  result_.addr_ = addr;
  callback_ = std::move(callback);
  auto thread_function = [this, addr, family, flag = safety_.flag(),
                          caller_task_queue = TaskQueueBase::Current(),
                          state = state_] {
    std::vector<IPAddress> addresses;
    int error = ResolveHostname(addr.hostname(), family, addresses);
    // We assume that the caller task queue is still around if the
    // AsyncDnsResolver has not been destroyed.
    state->Finish([this, error, flag, caller_task_queue,
                   addresses = std::move(addresses)]() mutable {
      caller_task_queue->PostTask(
          SafeTask(flag, [this, error, addresses = std::move(addresses)]() {
            RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
            result_.addresses_ = addresses;
            result_.error_ = error;
            callback_();
          }));
    });
  };
#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  PostTaskToGlobalQueue(
      std::make_unique<absl::AnyInvocable<void() &&>>(thread_function));
#else
  PlatformThread::SpawnDetached(std::move(thread_function), "AsyncResolver");
#endif
}

const AsyncDnsResolverResult& AsyncDnsResolver::result() const {
  return result_;
}

bool AsyncDnsResolverResultImpl::GetResolvedAddress(int family,
                                                    SocketAddress* addr) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(addr);
  if (error_ != 0 || addresses_.empty())
    return false;

  *addr = addr_;
  for (const auto& address : addresses_) {
    if (family == address.family()) {
      addr->SetResolvedIP(address);
      return true;
    }
  }
  return false;
}

int AsyncDnsResolverResultImpl::GetError() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return error_;
}

}  // namespace webrtc
