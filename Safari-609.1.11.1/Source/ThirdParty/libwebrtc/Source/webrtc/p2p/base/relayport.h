/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_RELAYPORT_H_
#define P2P_BASE_RELAYPORT_H_

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "p2p/base/port.h"
#include "p2p/base/stunrequest.h"

namespace cricket {

class RelayEntry;
class RelayConnection;

// Communicates using an allocated port on the relay server. For each
// remote candidate that we try to send data to a RelayEntry instance
// is created. The RelayEntry will try to reach the remote destination
// by connecting to all available server addresses in a pre defined
// order with a small delay in between. When a connection is
// successful all other connection attempts are aborted.
class RelayPort : public Port {
 public:
  typedef std::pair<rtc::Socket::Option, int> OptionValue;

  // RelayPort doesn't yet do anything fancy in the ctor.
  static std::unique_ptr<RelayPort> Create(rtc::Thread* thread,
                                           rtc::PacketSocketFactory* factory,
                                           rtc::Network* network,
                                           uint16_t min_port,
                                           uint16_t max_port,
                                           const std::string& username,
                                           const std::string& password) {
    // Using `new` to access a non-public constructor.
    return absl::WrapUnique(new RelayPort(thread, factory, network, min_port,
                                          max_port, username, password));
  }
  ~RelayPort() override;

  void AddServerAddress(const ProtocolAddress& addr);
  void AddExternalAddress(const ProtocolAddress& addr);

  const std::vector<OptionValue>& options() const { return options_; }
  bool HasMagicCookie(const char* data, size_t size);

  void PrepareAddress() override;
  Connection* CreateConnection(const Candidate& address,
                               CandidateOrigin origin) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override;
  bool SupportsProtocol(const std::string& protocol) const override;
  ProtocolType GetProtocol() const override;

  const ProtocolAddress* ServerAddress(size_t index) const;
  bool IsReady() { return ready_; }

  // Used for testing.
  sigslot::signal1<const ProtocolAddress*> SignalConnectFailure;
  sigslot::signal1<const ProtocolAddress*> SignalSoftTimeout;

 protected:
  RelayPort(rtc::Thread* thread,
            rtc::PacketSocketFactory* factory,
            rtc::Network*,
            uint16_t min_port,
            uint16_t max_port,
            const std::string& username,
            const std::string& password);
  bool Init();

  void SetReady();

  int SendTo(const void* data,
             size_t size,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;

  // Dispatches the given packet to the port or connection as appropriate.
  void OnReadPacket(const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    ProtocolType proto,
                    int64_t packet_time_us);

  // The OnSentPacket callback is left empty here since they are handled by
  // RelayEntry.
  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override {}

 private:
  friend class RelayEntry;

  std::deque<ProtocolAddress> server_addr_;
  std::vector<ProtocolAddress> external_addr_;
  bool ready_;
  std::vector<RelayEntry*> entries_;
  std::vector<OptionValue> options_;
  int error_;
};

}  // namespace cricket

#endif  // P2P_BASE_RELAYPORT_H_
