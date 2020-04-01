/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NET_HELPERS_H_
#define RTC_BASE_NET_HELPERS_H_

#if defined(WEBRTC_POSIX)
#include <sys/socket.h>
#elif WEBRTC_WIN
#include <winsock2.h>  // NOLINT
#endif

#include <vector>

#include "rtc_base/async_resolver_interface.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/signal_thread.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

// AsyncResolver will perform async DNS resolution, signaling the result on
// the SignalDone from AsyncResolverInterface when the operation completes.
class RTC_EXPORT AsyncResolver : public SignalThread,
                                 public AsyncResolverInterface {
 public:
  AsyncResolver();
  ~AsyncResolver() override;

  void Start(const SocketAddress& addr) override;
  bool GetResolvedAddress(int family, SocketAddress* addr) const override;
  int GetError() const override;
  void Destroy(bool wait) override;

  const std::vector<IPAddress>& addresses() const { return addresses_; }
  void set_error(int error) { error_ = error; }

 protected:
  void DoWork() override;
  void OnWorkDone() override;

 private:
  SocketAddress addr_;
  std::vector<IPAddress> addresses_;
  int error_;
};

// rtc namespaced wrappers for inet_ntop and inet_pton so we can avoid
// the windows-native versions of these.
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size);
int inet_pton(int af, const char* src, void* dst);

bool HasIPv4Enabled();
bool HasIPv6Enabled();
}  // namespace rtc

#endif  // RTC_BASE_NET_HELPERS_H_
