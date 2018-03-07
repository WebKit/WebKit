/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_SOCKETMONITOR_H_
#define P2P_CLIENT_SOCKETMONITOR_H_

#include <vector>

#include "p2p/base/jseptransport.h"  // for ConnectionInfos
#include "rtc_base/criticalsection.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/thread.h"

// TODO(pthatcher): Move these to connectionmonitor.h and
// connectionmonitor.cc, or just move them into channel.cc

namespace cricket {

class ConnectionStatsGetter {
 public:
  virtual ~ConnectionStatsGetter() {}
  virtual bool GetConnectionStats(ConnectionInfos* infos) = 0;
};

class ConnectionMonitor : public rtc::MessageHandler,
                          public sigslot::has_slots<> {
public:
  ConnectionMonitor(ConnectionStatsGetter* stats_getter,
                    rtc::Thread* network_thread,
                    rtc::Thread* monitoring_thread);
  ~ConnectionMonitor() override;

  void Start(int cms);
  void Stop();

  sigslot::signal2<ConnectionMonitor*,
                   const std::vector<ConnectionInfo>&> SignalUpdate;

 protected:
  void OnMessage(rtc::Message* message) override;

 private:
  void PollConnectionStats_w();

  std::vector<ConnectionInfo> connection_infos_;
  ConnectionStatsGetter* stats_getter_;
  rtc::Thread* network_thread_;
  rtc::Thread* monitoring_thread_;
  rtc::CriticalSection crit_;
  uint32_t rate_;
  bool monitoring_;
};

}  // namespace cricket

#endif  // P2P_CLIENT_SOCKETMONITOR_H_
