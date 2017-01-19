/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_H_

#include <list>
#include <map>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {
namespace testing {
namespace bwe {

class Packet {
 public:
  enum Type { kMedia, kFeedback };

  Packet();
  Packet(int flow_id, int64_t send_time_us, size_t payload_size);
  virtual ~Packet();

  virtual bool operator<(const Packet& rhs) const;

  virtual int flow_id() const { return flow_id_; }
  virtual void set_send_time_us(int64_t send_time_us);
  virtual int64_t send_time_us() const { return send_time_us_; }
  virtual int64_t sender_timestamp_us() const { return sender_timestamp_us_; }
  virtual size_t payload_size() const { return payload_size_; }
  virtual Packet::Type GetPacketType() const = 0;
  virtual void set_sender_timestamp_us(int64_t sender_timestamp_us) {
    sender_timestamp_us_ = sender_timestamp_us;
  }
  virtual int64_t creation_time_ms() const {
    return (creation_time_us_ + 500) / 1000;
  }
  virtual int64_t sender_timestamp_ms() const {
    return (sender_timestamp_us_ + 500) / 1000;
  }
  virtual int64_t send_time_ms() const { return (send_time_us_ + 500) / 1000; }

 protected:
  int flow_id_;
  int64_t creation_time_us_;  // Time when the packet was created.
  int64_t send_time_us_;  // Time the packet left last processor touching it.
  int64_t sender_timestamp_us_;  // Time the packet left the Sender.
  size_t payload_size_;  // Size of the (non-existent, simulated) payload.
};

class MediaPacket : public Packet {
 public:
  MediaPacket();
  MediaPacket(int flow_id,
              int64_t send_time_us,
              size_t payload_size,
              uint16_t sequence_number);
  MediaPacket(int flow_id,
              int64_t send_time_us,
              size_t payload_size,
              const RTPHeader& header);
  MediaPacket(int64_t send_time_us, uint16_t sequence_number);

  virtual ~MediaPacket() {}

  int64_t GetAbsSendTimeInMs() const {
    int64_t timestamp = header_.extension.absoluteSendTime
                        << kAbsSendTimeInterArrivalUpshift;
    return 1000.0 * timestamp / static_cast<double>(1 << kInterArrivalShift);
  }
  void SetAbsSendTimeMs(int64_t abs_send_time_ms);
  const RTPHeader& header() const { return header_; }
  virtual Packet::Type GetPacketType() const { return kMedia; }
  uint16_t sequence_number() const { return header_.sequenceNumber; }

 private:
  static const int kAbsSendTimeFraction = 18;
  static const int kAbsSendTimeInterArrivalUpshift = 8;
  static const int kInterArrivalShift =
      kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;

  RTPHeader header_;
};

class FeedbackPacket : public Packet {
 public:
  FeedbackPacket(int flow_id,
                 int64_t this_send_time_us,
                 int64_t latest_send_time_ms)
      : Packet(flow_id, this_send_time_us, 0),
        latest_send_time_ms_(latest_send_time_ms) {}
  virtual ~FeedbackPacket() {}

  virtual Packet::Type GetPacketType() const { return kFeedback; }
  int64_t latest_send_time_ms() const { return latest_send_time_ms_; }

 private:
  int64_t latest_send_time_ms_;  // Time stamp for the latest sent FbPacket.
};

class RembFeedback : public FeedbackPacket {
 public:
  RembFeedback(int flow_id,
               int64_t send_time_us,
               int64_t latest_send_time_ms,
               uint32_t estimated_bps,
               RTCPReportBlock report_block);
  virtual ~RembFeedback() {}

  uint32_t estimated_bps() const { return estimated_bps_; }
  RTCPReportBlock report_block() const { return report_block_; }

 private:
  const uint32_t estimated_bps_;
  const RTCPReportBlock report_block_;
};

class SendSideBweFeedback : public FeedbackPacket {
 public:
  typedef std::map<uint16_t, int64_t> ArrivalTimesMap;
  SendSideBweFeedback(int flow_id,
                      int64_t send_time_us,
                      int64_t latest_send_time_ms,
                      const std::vector<PacketInfo>& packet_feedback_vector);
  virtual ~SendSideBweFeedback() {}

  const std::vector<PacketInfo>& packet_feedback_vector() const {
    return packet_feedback_vector_;
  }

 private:
  const std::vector<PacketInfo> packet_feedback_vector_;
};

class NadaFeedback : public FeedbackPacket {
 public:
  NadaFeedback(int flow_id,
               int64_t this_send_time_us,
               int64_t exp_smoothed_delay_ms,
               int64_t est_queuing_delay_signal_ms,
               int64_t congestion_signal,
               float derivative,
               float receiving_rate,
               int64_t latest_send_time_ms)
      : FeedbackPacket(flow_id, this_send_time_us, latest_send_time_ms),
        exp_smoothed_delay_ms_(exp_smoothed_delay_ms),
        est_queuing_delay_signal_ms_(est_queuing_delay_signal_ms),
        congestion_signal_(congestion_signal),
        derivative_(derivative),
        receiving_rate_(receiving_rate) {}
  virtual ~NadaFeedback() {}

  int64_t exp_smoothed_delay_ms() const { return exp_smoothed_delay_ms_; }
  int64_t est_queuing_delay_signal_ms() const {
    return est_queuing_delay_signal_ms_;
  }
  int64_t congestion_signal() const { return congestion_signal_; }
  float derivative() const { return derivative_; }
  float receiving_rate() const { return receiving_rate_; }

 private:
  int64_t exp_smoothed_delay_ms_;        // Referred as d_hat_n.
  int64_t est_queuing_delay_signal_ms_;  // Referred as d_tilde_n.
  int64_t congestion_signal_;            // Referred as x_n.
  float derivative_;                     // Referred as x'_n.
  float receiving_rate_;                 // Referred as R_r.
};

class TcpFeedback : public FeedbackPacket {
 public:
  TcpFeedback(int flow_id,
              int64_t send_time_us,
              int64_t latest_send_time_ms,
              const std::vector<uint16_t>& acked_packets)
      : FeedbackPacket(flow_id, send_time_us, latest_send_time_ms),
        acked_packets_(acked_packets) {}
  virtual ~TcpFeedback() {}

  const std::vector<uint16_t>& acked_packets() const { return acked_packets_; }

 private:
  const std::vector<uint16_t> acked_packets_;
};

typedef std::list<Packet*> Packets;
typedef std::list<Packet*>::iterator PacketsIt;
typedef std::list<Packet*>::const_iterator PacketsConstIt;

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_H_
