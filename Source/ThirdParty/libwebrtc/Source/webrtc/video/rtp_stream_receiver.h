/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RTP_STREAM_RECEIVER_H_
#define WEBRTC_VIDEO_RTP_STREAM_RECEIVER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/h264_sps_pps_tracker.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/packet_buffer.h"
#include "webrtc/modules/video_coding/rtp_frame_reference_finder.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {

class NackModule;
class PacedSender;
class PacketRouter;
class ProcessThread;
class ReceiveStatistics;
class ReceiveStatisticsProxy;
class RemoteBitrateEstimator;
class RemoteNtpTimeEstimator;
class RtcpRttStats;
class RtpHeaderParser;
class RTPPayloadRegistry;
class RtpReceiver;
class Transport;
class UlpfecReceiver;
class VCMTiming;
class VieRemb;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

class RtpStreamReceiver : public RtpData,
                          public RtpFeedback,
                          public VCMFrameTypeCallback,
                          public VCMPacketRequestCallback,
                          public video_coding::OnReceivedFrameCallback,
                          public video_coding::OnCompleteFrameCallback,
                          public CallStatsObserver {
 public:
  RtpStreamReceiver(
      vcm::VideoReceiver* video_receiver,
      RemoteBitrateEstimator* remote_bitrate_estimator,
      Transport* transport,
      RtcpRttStats* rtt_stats,
      PacedSender* paced_sender,
      PacketRouter* packet_router,
      VieRemb* remb,
      const VideoReceiveStream::Config* config,
      ReceiveStatisticsProxy* receive_stats_proxy,
      ProcessThread* process_thread,
      RateLimiter* retransmission_rate_limiter,
      NackSender* nack_sender,
      KeyFrameRequestSender* keyframe_request_sender,
      video_coding::OnCompleteFrameCallback* complete_frame_callback,
      VCMTiming* timing);
  ~RtpStreamReceiver();

  bool SetReceiveCodec(const VideoCodec& video_codec);

  uint32_t GetRemoteSsrc() const;
  int GetCsrcs(uint32_t* csrcs) const;

  RtpReceiver* GetRtpReceiver() const;
  RtpRtcp* rtp_rtcp() const { return rtp_rtcp_.get(); }

  void StartReceive();
  void StopReceive();

  bool DeliverRtp(const uint8_t* rtp_packet,
                  size_t rtp_packet_length,
                  const PacketTime& packet_time);
  bool DeliverRtcp(const uint8_t* rtcp_packet, size_t rtcp_packet_length);

  void FrameContinuous(uint16_t seq_num);

  void FrameDecoded(uint16_t seq_num);

  void SignalNetworkState(NetworkState state);

  // Implements RtpData.
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                size_t payload_size,
                                const WebRtcRTPHeader* rtp_header) override;
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;

  // Implements RtpFeedback.
  int32_t OnInitializeDecoder(int8_t payload_type,
                              const char payload_name[RTP_PAYLOAD_NAME_SIZE],
                              int frequency,
                              size_t channels,
                              uint32_t rate) override;
  void OnIncomingSSRCChanged(uint32_t ssrc) override;
  void OnIncomingCSRCChanged(uint32_t CSRC, bool added) override {}

  // Implements VCMFrameTypeCallback.
  int32_t RequestKeyFrame() override;
  int32_t SliceLossIndicationRequest(const uint64_t picture_id) override;

  bool IsFecEnabled() const;
  bool IsRetransmissionsEnabled() const;
  // Don't use, still experimental.
  void RequestPacketRetransmit(const std::vector<uint16_t>& sequence_numbers);

  // Implements VCMPacketRequestCallback.
  int32_t ResendPackets(const uint16_t* sequenceNumbers,
                        uint16_t length) override;

  // Implements OnReceivedFrameCallback.
  void OnReceivedFrame(
      std::unique_ptr<video_coding::RtpFrameObject> frame) override;

  // Implements OnCompleteFrameCallback.
  void OnCompleteFrame(
      std::unique_ptr<video_coding::FrameObject> frame) override;

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

 private:
  bool ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header,
                     bool in_order);
  // Parses and handles for instance RTX and RED headers.
  // This function assumes that it's being called from only one thread.
  bool ParseAndHandleEncapsulatingHeader(const uint8_t* packet,
                                         size_t packet_length,
                                         const RTPHeader& header);
  void NotifyReceiverOfFecPacket(const RTPHeader& header);
  bool IsPacketInOrder(const RTPHeader& header) const;
  bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;
  void UpdateHistograms();
  void EnableReceiveRtpHeaderExtension(const std::string& extension, int id);
  bool IsRedEnabled() const;

  Clock* const clock_;
  // Ownership of this object lies with VideoReceiveStream, which owns |this|.
  const VideoReceiveStream::Config& config_;
  vcm::VideoReceiver* const video_receiver_;
  RemoteBitrateEstimator* const remote_bitrate_estimator_;
  PacketRouter* const packet_router_;
  VieRemb* const remb_;
  ProcessThread* const process_thread_;

  RemoteNtpTimeEstimator ntp_estimator_;
  RTPPayloadRegistry rtp_payload_registry_;

  const std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
  const std::unique_ptr<RtpReceiver> rtp_receiver_;
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<UlpfecReceiver> ulpfec_receiver_;

  rtc::CriticalSection receive_cs_;
  bool receiving_ GUARDED_BY(receive_cs_);
  uint8_t restored_packet_[IP_PACKET_SIZE] GUARDED_BY(receive_cs_);
  bool restored_packet_in_use_ GUARDED_BY(receive_cs_);
  int64_t last_packet_log_ms_ GUARDED_BY(receive_cs_);

  const std::unique_ptr<RtpRtcp> rtp_rtcp_;

  // Members for the new jitter buffer experiment.
  bool jitter_buffer_experiment_;
  video_coding::OnCompleteFrameCallback* complete_frame_callback_;
  KeyFrameRequestSender* keyframe_request_sender_;
  VCMTiming* timing_;
  std::unique_ptr<NackModule> nack_module_;
  rtc::scoped_refptr<video_coding::PacketBuffer> packet_buffer_;
  std::unique_ptr<video_coding::RtpFrameReferenceFinder> reference_finder_;
  rtc::CriticalSection last_seq_num_cs_;
  std::map<uint16_t, uint16_t, DescendingSeqNumComp<uint16_t>>
      last_seq_num_for_pic_id_ GUARDED_BY(last_seq_num_cs_);
  video_coding::H264SpsPpsTracker tracker_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RTP_STREAM_RECEIVER_H_
