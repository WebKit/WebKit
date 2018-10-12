/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_
#define VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"

#include "api/video_codecs/video_codec.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/contributing_sources.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {

class NackModule;
class PacketRouter;
class ProcessThread;
class ReceiveStatistics;
class ReceiveStatisticsProxy;
class RtcpRttStats;
class RtpPacketReceived;
class Transport;
class UlpfecReceiver;

class RtpVideoStreamReceiver : public RecoveredPacketReceiver,
                               public RtpPacketSinkInterface,
                               public VCMFrameTypeCallback,
                               public VCMPacketRequestCallback,
                               public video_coding::OnReceivedFrameCallback,
                               public video_coding::OnCompleteFrameCallback {
 public:
  RtpVideoStreamReceiver(
      Transport* transport,
      RtcpRttStats* rtt_stats,
      PacketRouter* packet_router,
      const VideoReceiveStream::Config* config,
      ReceiveStatistics* rtp_receive_statistics,
      ReceiveStatisticsProxy* receive_stats_proxy,
      ProcessThread* process_thread,
      NackSender* nack_sender,
      KeyFrameRequestSender* keyframe_request_sender,
      video_coding::OnCompleteFrameCallback* complete_frame_callback);
  ~RtpVideoStreamReceiver() override;

  void AddReceiveCodec(const VideoCodec& video_codec,
                       const std::map<std::string, std::string>& codec_params);

  void StartReceive();
  void StopReceive();

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  absl::optional<Syncable::Info> GetSyncInfo() const;

  bool DeliverRtcp(const uint8_t* rtcp_packet, size_t rtcp_packet_length);

  void FrameContinuous(int64_t seq_num);

  void FrameDecoded(int64_t seq_num);

  void SignalNetworkState(NetworkState state);

  // Returns number of different frames seen in the packet buffer.
  int GetUniqueFramesSeen() const;

  // Implements RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // TODO(philipel): Stop using VCMPacket in the new jitter buffer and then
  //                 remove this function.
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                size_t payload_size,
                                const WebRtcRTPHeader* rtp_header);
  int32_t OnReceivedPayloadData(
      const uint8_t* payload_data,
      size_t payload_size,
      const WebRtcRTPHeader* rtp_header,
      const absl::optional<RtpGenericFrameDescriptor>& generic_descriptor);

  // Implements RecoveredPacketReceiver.
  void OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;

  // Implements VCMFrameTypeCallback.
  int32_t RequestKeyFrame() override;

  bool IsUlpfecEnabled() const;
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
      std::unique_ptr<video_coding::EncodedFrame> frame) override;

  // Called by VideoReceiveStream when stats are updated.
  void UpdateRtt(int64_t max_rtt_ms);

  absl::optional<int64_t> LastReceivedPacketMs() const;
  absl::optional<int64_t> LastReceivedKeyframePacketMs() const;

  // RtpDemuxer only forwards a given RTP packet to one sink. However, some
  // sinks, such as FlexFEC, might wish to be informed of all of the packets
  // a given sink receives (or any set of sinks). They may do so by registering
  // themselves as secondary sinks.
  void AddSecondarySink(RtpPacketSinkInterface* sink);
  void RemoveSecondarySink(const RtpPacketSinkInterface* sink);

  std::vector<webrtc::RtpSource> GetSources() const;

 private:
  // Entry point doing non-stats work for a received packet. Called
  // for the same packet both before and after RED decapsulation.
  void ReceivePacket(const RtpPacketReceived& packet);
  // Parses and handles RED headers.
  // This function assumes that it's being called from only one thread.
  void ParseAndHandleEncapsulatingHeader(const RtpPacketReceived& packet);
  void NotifyReceiverOfEmptyPacket(uint16_t seq_num);
  void UpdateHistograms();
  bool IsRedEnabled() const;
  void InsertSpsPpsIntoTracker(uint8_t payload_type);

  Clock* const clock_;
  // Ownership of this object lies with VideoReceiveStream, which owns |this|.
  const VideoReceiveStream::Config& config_;
  PacketRouter* const packet_router_;
  ProcessThread* const process_thread_;

  RemoteNtpTimeEstimator ntp_estimator_;

  RtpHeaderExtensionMap rtp_header_extensions_;
  ReceiveStatistics* const rtp_receive_statistics_;
  std::unique_ptr<UlpfecReceiver> ulpfec_receiver_;

  rtc::SequencedTaskChecker worker_task_checker_;
  bool receiving_ RTC_GUARDED_BY(worker_task_checker_);
  int64_t last_packet_log_ms_ RTC_GUARDED_BY(worker_task_checker_);

  const std::unique_ptr<RtpRtcp> rtp_rtcp_;

  // Members for the new jitter buffer experiment.
  video_coding::OnCompleteFrameCallback* complete_frame_callback_;
  KeyFrameRequestSender* keyframe_request_sender_;
  std::unique_ptr<NackModule> nack_module_;
  rtc::scoped_refptr<video_coding::PacketBuffer> packet_buffer_;
  std::unique_ptr<video_coding::RtpFrameReferenceFinder> reference_finder_;
  rtc::CriticalSection last_seq_num_cs_;
  std::map<int64_t, uint16_t> last_seq_num_for_pic_id_
      RTC_GUARDED_BY(last_seq_num_cs_);
  video_coding::H264SpsPpsTracker tracker_;

  std::map<uint8_t, VideoCodecType> pt_codec_type_;
  // TODO(johan): Remove pt_codec_params_ once
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=6883 is resolved.
  // Maps a payload type to a map of out-of-band supplied codec parameters.
  std::map<uint8_t, std::map<std::string, std::string>> pt_codec_params_;
  int16_t last_payload_type_ = -1;

  bool has_received_frame_;

  std::vector<RtpPacketSinkInterface*> secondary_sinks_
      RTC_GUARDED_BY(worker_task_checker_);

  // Info for GetSources and GetSyncInfo is updated on network or worker thread,
  // queried on the worker thread.
  rtc::CriticalSection rtp_sources_lock_;
  ContributingSources contributing_sources_ RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(rtp_sources_lock_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(rtp_sources_lock_);
};

}  // namespace webrtc

#endif  // VIDEO_RTP_VIDEO_STREAM_RECEIVER_H_
