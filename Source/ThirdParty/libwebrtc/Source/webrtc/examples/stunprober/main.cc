/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/stunprober/stun_prober.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

using stunprober::AsyncCallback;
using stunprober::StunProber;

ABSL_FLAG(int,
          interval,
          10,
          "Interval of consecutive stun pings in milliseconds");
ABSL_FLAG(bool,
          shared_socket,
          false,
          "Share socket mode for different remote IPs");
ABSL_FLAG(int,
          pings_per_ip,
          10,
          "Number of consecutive stun pings to send for each IP");
ABSL_FLAG(int,
          timeout,
          1000,
          "Milliseconds of wait after the last ping sent before exiting");
ABSL_FLAG(
    std::string,
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
    RTC_LOG(LS_WARNING) << "Results are inconclusive.";
    return;
  }

  RTC_LOG(LS_INFO) << "Shared Socket Mode: " << stats.shared_socket_mode;
  RTC_LOG(LS_INFO) << "Requests sent: " << stats.num_request_sent;
  RTC_LOG(LS_INFO) << "Responses received: " << stats.num_response_received;
  RTC_LOG(LS_INFO) << "Target interval (ns): "
                   << stats.target_request_interval_ns;
  RTC_LOG(LS_INFO) << "Actual interval (ns): "
                   << stats.actual_request_interval_ns;
  RTC_LOG(LS_INFO) << "NAT Type: " << PrintNatType(stats.nat_type);
  RTC_LOG(LS_INFO) << "Host IP: " << stats.host_ip;
  RTC_LOG(LS_INFO) << "Server-reflexive ips: ";
  for (auto& ip : stats.srflx_addrs) {
    RTC_LOG(LS_INFO) << "\t" << ip;
  }

  RTC_LOG(LS_INFO) << "Success Precent: " << stats.success_percent;
  RTC_LOG(LS_INFO) << "Response Latency:" << stats.average_rtt_ms;
}

void StopTrial(rtc::Thread* thread, StunProber* prober, int result) {
  thread->Quit();
  if (prober) {
    RTC_LOG(LS_INFO) << "Result: " << result;
    if (result == StunProber::SUCCESS) {
      PrintStats(prober);
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  std::vector<rtc::SocketAddress> server_addresses;
  std::istringstream servers(absl::GetFlag(FLAGS_servers));
  std::string server;
  while (getline(servers, server, ',')) {
    rtc::SocketAddress addr;
    if (!addr.FromString(server)) {
      RTC_LOG(LS_ERROR) << "Parsing " << server << " failed.";
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
  prober->Start(server_addresses, absl::GetFlag(FLAGS_shared_socket),
                absl::GetFlag(FLAGS_interval),
                absl::GetFlag(FLAGS_pings_per_ip), absl::GetFlag(FLAGS_timeout),
                AsyncCallback(finish_callback));
  thread->Run();
  delete prober;
  return 0;
}
