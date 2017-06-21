/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_CONFERENCE_TRANSPORT_H_
#define WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_CONFERENCE_TRANSPORT_H_

#include <deque>
#include <map>
#include <memory>
#include <utility>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/test/auto_test/fakes/loudest_filter.h"

namespace webrtc {
namespace voetest {

static const size_t kMaxPacketSizeByte = 1500;

// This class is to simulate a conference call. There are two Voice Engines, one
// for local channels and the other for remote channels. There is a simulated
// reflector, which exchanges RTCP with local channels. For simplicity, it
// also uses the Voice Engine for remote channels. One can add streams by
// calling AddStream(), which creates a remote sender channel and a local
// receive channel. The remote sender channel plays a file as microphone in a
// looped fashion. Received streams are mixed and played.

class ConferenceTransport: public webrtc::Transport {
 public:
  ConferenceTransport();
  virtual ~ConferenceTransport();

  /* SetRtt()
   * Set RTT between local channels and reflector.
   *
   * Input:
   *   rtt_ms : RTT in milliseconds.
   */
  void SetRtt(unsigned int rtt_ms);

  /* AddStream()
   * Adds a stream in the conference.
   *
   * Input:
   *   file_name : name of the file to be added as microphone input.
   *   format    : format of the input file.
   *
   * Returns stream id.
   */
  unsigned int AddStream(std::string file_name, webrtc::FileFormats format);

  /* RemoveStream()
   * Removes a stream with specified ID from the conference.
   *
   * Input:
   *   id : stream id.
   *
   * Returns false if the specified stream does not exist, true if succeeds.
   */
  bool RemoveStream(unsigned int id);

  /* StartPlayout()
   * Starts playing out the stream with specified ID, using the default device.
   *
   * Input:
   *   id : stream id.
   *
   * Returns false if the specified stream does not exist, true if succeeds.
   */
  bool StartPlayout(unsigned int id);

  /* GetReceiverStatistics()
   * Gets RTCP statistics of the stream with specified ID.
   *
   * Input:
   *   id : stream id;
   *   stats : pointer to a CallStatistics to store the result.
   *
   * Returns false if the specified stream does not exist, true if succeeds.
   */
  bool GetReceiverStatistics(unsigned int id, webrtc::CallStatistics* stats);

  // Inherit from class webrtc::Transport.
  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override;
  bool SendRtcp(const uint8_t *data, size_t len) override;

 private:
  struct Packet {
    enum Type { Rtp, Rtcp, } type_;

    Packet() : len_(0) {}
    Packet(Type type, const void* data, size_t len, int64_t time_ms)
        : type_(type), len_(len), send_time_ms_(time_ms) {
      EXPECT_LE(len_, kMaxPacketSizeByte);
      memcpy(data_, data, len_);
    }

    uint8_t data_[kMaxPacketSizeByte];
    size_t len_;
    int64_t send_time_ms_;
  };

  static bool Run(void* transport) {
    return static_cast<ConferenceTransport*>(transport)->DispatchPackets();
  }

  int GetReceiverChannelForSsrc(unsigned int sender_ssrc) const;
  void StorePacket(Packet::Type type, const void* data, size_t len);
  void SendPacket(const Packet& packet);
  bool DispatchPackets();

  rtc::CriticalSection pq_crit_;
  rtc::CriticalSection stream_crit_;
  const std::unique_ptr<webrtc::EventWrapper> packet_event_;
  rtc::PlatformThread thread_;

  unsigned int rtt_ms_;
  unsigned int stream_count_;

  std::map<unsigned int, std::pair<int, int>> streams_ GUARDED_BY(stream_crit_);
  std::deque<Packet> packet_queue_ GUARDED_BY(pq_crit_);

  int local_sender_;  // Channel Id of local sender
  int reflector_;

  webrtc::VoiceEngine* local_voe_;
  webrtc::VoEBase* local_base_;
  webrtc::VoERTP_RTCP* local_rtp_rtcp_;
  webrtc::VoENetwork* local_network_;

  webrtc::VoiceEngine* remote_voe_;
  webrtc::VoEBase* remote_base_;
  webrtc::VoECodec* remote_codec_;
  webrtc::VoERTP_RTCP* remote_rtp_rtcp_;
  webrtc::VoENetwork* remote_network_;
  webrtc::VoEFile* remote_file_;

  LoudestFilter loudest_filter_;

  const std::unique_ptr<webrtc::RtpHeaderParser> rtp_header_parser_;
};

}  // namespace voetest
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_AUTO_TEST_FAKES_CONFERENCE_TRANSPORT_H_
