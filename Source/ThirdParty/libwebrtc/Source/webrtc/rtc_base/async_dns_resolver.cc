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

#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/async_dns_resolver.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/string_utils.h"  // IWYU pragma: keep
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
#if defined(WEBRTC_WIN)
// Special support for the Windows specific addrinfo type.
bool IPFromAddrInfo(PADDRINFOEXW info, IPAddress* out) {
  if (!info || !info->ai_addr ||
      (info->ai_addr->sa_family != AF_INET &&
       info->ai_addr->sa_family != AF_INET6)) {
    return false;
  }
  if (info->ai_addr->sa_family == AF_INET) {
    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(info->ai_addr);
    *out = IPAddress(addr->sin_addr);
  } else {
    RTC_DCHECK_EQ(info->ai_addr->sa_family, AF_INET6);
    sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(info->ai_addr);
    *out = IPAddress(addr->sin6_addr);
  }
  return true;
}
#endif

template <typename AddrInfo>
std::vector<IPAddress> AddressesFromAddrInfo(AddrInfo* addr, int family) {
  std::vector<IPAddress> addresses;
  for (AddrInfo* cursor = addr; cursor; cursor = cursor->ai_next) {
    if (family == AF_UNSPEC || cursor->ai_family == family) {
      IPAddress ip;
      if (IPFromAddrInfo(cursor, &ip)) {
        addresses.push_back(ip);
      }
    }
  }
  return addresses;
}

#if !defined(WEBRTC_WIN)
int ResolveHostname(absl::string_view hostname,
                    int family,
                    std::vector<IPAddress>& addresses) {
  RTC_DCHECK(addresses.empty());
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
  int ret = getaddrinfo(hostname.data(), nullptr, &hints, &result);
  if (ret != 0) {
    return ret;
  }
  addresses = AddressesFromAddrInfo(result, family);
  freeaddrinfo(result);
  return 0;
}
#endif  // !defined(WEBRTC_WIN)

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

class AsyncDnsResolver::StateImpl {
 public:
  static scoped_refptr<AsyncDnsResolver::State> Create() {
    return make_ref_counted<AsyncDnsResolver::State>();
  }

  // Execute the passed function if the state is Active.
  void PostToCallbackTaskQueue(absl::AnyInvocable<void() &&> function) {
    MutexLock lock(&mutex_);
    if (!task_queue_) {
      return;
    }
    task_queue_->PostTask(std::move(function));
  }

  void Cancel() {
    MutexLock lock(&mutex_);
    task_queue_ = nullptr;
  }

 private:
  Mutex mutex_;
  TaskQueueBase* task_queue_ RTC_GUARDED_BY(mutex_) = TaskQueueBase::Current();
};

AsyncDnsResolver::AsyncDnsResolver() = default;

AsyncDnsResolver::~AsyncDnsResolver() {
  if (state_) {
#if defined(WEBRTC_WIN)
    RTC_DCHECK(cancel_);
    GetAddrInfoExCancel(&cancel_);
#endif
    state_->Cancel();
  }
#if defined(WEBRTC_WIN)
  if (!worker_.empty()) {
    // The wait operation has been cancelled, this should be fast.
    worker_.Finalize();
  }
  if (addr_info_) {
    FreeAddrInfoExW(addr_info_);
  }
  if (ol_.hEvent) {
    ::CloseHandle(ol_.hEvent);
  }
#endif
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
  RTC_CHECK(!state_);
  state_ = State::Create();
  result_.addr_ = addr;
  callback_ = std::move(callback);

#if defined(WEBRTC_WIN)
  RTC_DCHECK(!ol_.hEvent);
  RTC_DCHECK(!addr_info_);
  RTC_DCHECK(!cancel_);
  RTC_DCHECK(worker_.empty());
  // Start the async name resolution on this thread. It may complete directly
  // or it may proceed asynchronously. In the async case, we'll spawn a thread
  // that waits for completion. We must use the unicode version (`W`) of
  // GetAddrInfoEx since the ANSI version is not supported.
  std::wstring hostname = ToUtf16(addr.hostname());
  ol_.hEvent = CreateEvent(nullptr, true, false, nullptr);
  ADDRINFOEXW hints = {.ai_flags = AI_ADDRCONFIG, .ai_family = family};
  int ret = GetAddrInfoExW(hostname.c_str(), nullptr, NS_ALL, nullptr, &hints,
                           &addr_info_, nullptr, &ol_, nullptr, &cancel_);

  // Check if the operation is done, continues asynchronously, or failed.
  if (ret == ERROR_IO_PENDING) {  // Async.
    auto on_complete = SafeTask(safety_.flag(), [this, family]() mutable {
      RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
      WSAGetOverlappedResult(0, &ol_, nullptr, false, nullptr);
      result_.error_ = static_cast<int>(ol_.Internal);
      if (result_.error_ == ERROR_SUCCESS) {
        result_.addresses_ = AddressesFromAddrInfo(addr_info_, family);
      }
      state_ = nullptr;
      std::move(callback_)();
    });

    absl::AnyInvocable<void() &&> thread_function =
        [done = ol_.hEvent, state = state_,
         on_complete = std::move(on_complete)]() mutable {
          WaitForSingleObject(done, INFINITE);
          state->PostToCallbackTaskQueue(std::move(on_complete));
        };
    worker_ = PlatformThread::SpawnJoinable(std::move(thread_function),
                                            "AsyncResolver");
  } else {  // Failed or succeeded.
    if (ret == ERROR_SUCCESS) {
      result_.addresses_ = AddressesFromAddrInfo(addr_info_, family);
    }
    result_.error_ = ret;
    state_->PostToCallbackTaskQueue(SafeTask(safety_.flag(), [this]() {
      RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
      state_ = nullptr;
      std::move(callback_)();
    }));
  }
#else
  absl::AnyInvocable<void() &&> thread_function =
      [this, addr, family, flag = safety_.flag(), state = state_]() {
        std::vector<IPAddress> addresses;
        int error = ResolveHostname(addr.hostname(), family, addresses);
        state->PostToCallbackTaskQueue(
            SafeTask(flag, [this, error, addresses = std::move(addresses)]() {
              RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
              state_ = nullptr;
              result_.addresses_ = addresses;
              result_.error_ = error;
              std::move(callback_)();
            }));
      };

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  PostTaskToGlobalQueue(std::make_unique<absl::AnyInvocable<void() &&>>(
      std::move(thread_function)));
#else
  PlatformThread::SpawnDetached(std::move(thread_function), "AsyncResolver");
#endif
#endif  // defined(WEBRTC_WIN)
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
