/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>
#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/flags.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/network.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/stunprober/stunprober.h"

using stunprober::StunProber;
using stunprober::AsyncCallback;

DEFINE_bool(help, false, "Prints this message");
DEFINE_int(interval, 10, "Interval of consecutive stun pings in milliseconds");
DEFINE_bool(shared_socket, false, "Share socket mode for different remote IPs");
DEFINE_int(pings_per_ip,
           10,
           "Number of consecutive stun pings to send for each IP");
DEFINE_int(timeout,
           1000,
           "Milliseconds of wait after the last ping sent before exiting");
DEFINE_string(
    servers,
    "stun.l.google.com:19302,stun1.l.google.com:19302,stun2.l.google.com:19302",
    "Comma separated STUN server addresses with ports");

namespace {

const char* PrintNatType(stunprober::NatType type) {
  switch (type) {
    case stunprober::NATTYPE_NONE:
      return "Not behind a NAT";
    case stunprober::NATTYPE_UNKNOWN:
      return "Unknown NAT type";
    case stunprober::NATTYPE_SYMMETRIC:
      return "Symmetric NAT";
    case stunprober::NATTYPE_NON_SYMMETRIC:
      return "Non-Symmetric NAT";
    default:
      return "Invalid";
  }
}

void PrintStats(StunProber* prober) {
  StunProber::Stats stats;
  if (!prober->GetStats(&stats)) {
    LOG(LS_WARNING) << "Results are inconclusive.";
    return;
  }

  LOG(LS_INFO) << "Shared Socket Mode: " << stats.shared_socket_mode;
  LOG(LS_INFO) << "Requests sent: " << stats.num_request_sent;
  LOG(LS_INFO) << "Responses received: " << stats.num_response_received;
  LOG(LS_INFO) << "Target interval (ns): " << stats.target_request_interval_ns;
  LOG(LS_INFO) << "Actual interval (ns): " << stats.actual_request_interval_ns;
  LOG(LS_INFO) << "NAT Type: " << PrintNatType(stats.nat_type);
  LOG(LS_INFO) << "Host IP: " << stats.host_ip;
  LOG(LS_INFO) << "Server-reflexive ips: ";
  for (auto& ip : stats.srflx_addrs) {
    LOG(LS_INFO) << "\t" << ip;
  }

  LOG(LS_INFO) << "Success Precent: " << stats.success_percent;
  LOG(LS_INFO) << "Response Latency:" << stats.average_rtt_ms;
}

void StopTrial(rtc::Thread* thread, StunProber* prober, int result) {
  thread->Quit();
  if (prober) {
    LOG(LS_INFO) << "Result: " << result;
    if (result == StunProber::SUCCESS) {
      PrintStats(prober);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  std::vector<rtc::SocketAddress> server_addresses;
  std::istringstream servers(FLAG_servers);
  std::string server;
  while (getline(servers, server, ',')) {
    rtc::SocketAddress addr;
    if (!addr.FromString(server)) {
      LOG(LS_ERROR) << "Parsing " << server << " failed.";
      return -1;
    }
    server_addresses.push_back(addr);
  }

  rtc::InitializeSSL();
  rtc::InitRandom(rtc::Time32());
  rtc::Thread* thread = rtc::ThreadManager::Instance()->WrapCurrentThread();
  std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory(
      new rtc::BasicPacketSocketFactory());
  std::unique_ptr<rtc::BasicNetworkManager> network_manager(
      new rtc::BasicNetworkManager());
  rtc::NetworkManager::NetworkList networks;
  network_manager->GetNetworks(&networks);
  StunProber* prober =
      new StunProber(socket_factory.get(), rtc::Thread::Current(), networks);
  auto finish_callback = [thread](StunProber* prober, int result) {
    StopTrial(thread, prober, result);
  };
  prober->Start(server_addresses, FLAG_shared_socket, FLAG_interval,
                FLAG_pings_per_ip, FLAG_timeout,
                AsyncCallback(finish_callback));
  thread->Run();
  delete prober;
  return 0;
}
