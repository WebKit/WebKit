/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/rtc_event_log.h"

#include <limits>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/event.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/protobuf_utils.h"
#include "webrtc/base/swap_queue.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_helper_thread.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

#ifdef ENABLE_RTC_EVENT_LOG
// *.pb.h files are generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#endif
#endif

namespace webrtc {

#ifdef ENABLE_RTC_EVENT_LOG

class RtcEventLogImpl final : public RtcEventLog {
 public:
  RtcEventLogImpl();
  ~RtcEventLogImpl() override;

  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override;
  bool StartLogging(rtc::PlatformFile platform_file,
                    int64_t max_size_bytes) override;
  void StopLogging() override;
  void LogVideoReceiveStreamConfig(const rtclog::StreamConfig& config) override;
  void LogVideoSendStreamConfig(const rtclog::StreamConfig& config) override;
  void LogAudioReceiveStreamConfig(const rtclog::StreamConfig& config) override;
  void LogAudioSendStreamConfig(const rtclog::StreamConfig& config) override;
  void LogRtpHeader(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length) override;
  void LogRtpHeader(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id) override;
  void LogRtcpPacket(PacketDirection direction,
                     const uint8_t* packet,
                     size_t length) override;
  void LogAudioPlayout(uint32_t ssrc) override;
  void LogLossBasedBweUpdate(int32_t bitrate_bps,
                             uint8_t fraction_loss,
                             int32_t total_packets) override;
  void LogDelayBasedBweUpdate(int32_t bitrate_bps,
                              BandwidthUsage detector_state) override;
  void LogAudioNetworkAdaptation(
      const AudioEncoderRuntimeConfig& config) override;
  void LogProbeClusterCreated(int id,
                              int bitrate_bps,
                              int min_probes,
                              int min_bytes) override;
  void LogProbeResultSuccess(int id, int bitrate_bps) override;
  void LogProbeResultFailure(int id,
                             ProbeFailureReason failure_reason) override;

 private:
  void StoreEvent(std::unique_ptr<rtclog::Event>* event);
  void LogProbeResult(int id,
                      rtclog::BweProbeResult::ResultType result,
                      int bitrate_bps);

  // Message queue for passing control messages to the logging thread.
  SwapQueue<RtcEventLogHelperThread::ControlMessage> message_queue_;

  // Message queue for passing events to the logging thread.
  SwapQueue<std::unique_ptr<rtclog::Event> > event_queue_;

  RtcEventLogHelperThread helper_thread_;
  rtc::ThreadChecker thread_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

namespace {
// The functions in this namespace convert enums from the runtime format
// that the rest of the WebRtc project can use, to the corresponding
// serialized enum which is defined by the protobuf.

rtclog::VideoReceiveConfig_RtcpMode ConvertRtcpMode(RtcpMode rtcp_mode) {
  switch (rtcp_mode) {
    case RtcpMode::kCompound:
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
    case RtcpMode::kReducedSize:
      return rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE;
    case RtcpMode::kOff:
      RTC_NOTREACHED();
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
  }
  RTC_NOTREACHED();
  return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
}

rtclog::DelayBasedBweUpdate::DetectorState ConvertDetectorState(
    BandwidthUsage state) {
  switch (state) {
    case BandwidthUsage::kBwNormal:
      return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
    case BandwidthUsage::kBwUnderusing:
      return rtclog::DelayBasedBweUpdate::BWE_UNDERUSING;
    case BandwidthUsage::kBwOverusing:
      return rtclog::DelayBasedBweUpdate::BWE_OVERUSING;
  }
  RTC_NOTREACHED();
  return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
}

rtclog::BweProbeResult::ResultType ConvertProbeResultType(
    ProbeFailureReason failure_reason) {
  switch (failure_reason) {
    case kInvalidSendReceiveInterval:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_INTERVAL;
    case kInvalidSendReceiveRatio:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_RATIO;
    case kTimeout:
      return rtclog::BweProbeResult::TIMEOUT;
  }
  RTC_NOTREACHED();
  return rtclog::BweProbeResult::SUCCESS;
}

// The RTP and RTCP buffers reserve space for twice the expected number of
// sent packets because they also contain received packets.
static const int kEventsPerSecond = 1000;
static const int kControlMessagesPerSecond = 10;
}  // namespace

// RtcEventLogImpl member functions.
RtcEventLogImpl::RtcEventLogImpl()
    // Allocate buffers for roughly one second of history.
    : message_queue_(kControlMessagesPerSecond),
      event_queue_(kEventsPerSecond),
      helper_thread_(&message_queue_, &event_queue_),
      thread_checker_() {
  thread_checker_.DetachFromThread();
}

RtcEventLogImpl::~RtcEventLogImpl() {
  // The RtcEventLogHelperThread destructor closes the file
  // and waits for the thread to terminate.
}

bool RtcEventLogImpl::StartLogging(const std::string& file_name,
                                   int64_t max_size_bytes) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RtcEventLogHelperThread::ControlMessage message;
  message.message_type = RtcEventLogHelperThread::ControlMessage::START_FILE;
  message.max_size_bytes = max_size_bytes <= 0
                               ? std::numeric_limits<int64_t>::max()
                               : max_size_bytes;
  message.start_time = rtc::TimeMicros();
  message.stop_time = std::numeric_limits<int64_t>::max();
  message.file.reset(FileWrapper::Create());
  if (!message.file->OpenFile(file_name.c_str(), false)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    return false;
  }
  if (!message_queue_.Insert(&message)) {
    LOG(LS_ERROR) << "Message queue full. Can't start logging.";
    return false;
  }
  helper_thread_.SignalNewEvent();
  LOG(LS_INFO) << "Starting WebRTC event log.";
  return true;
}

bool RtcEventLogImpl::StartLogging(rtc::PlatformFile platform_file,
                                   int64_t max_size_bytes) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RtcEventLogHelperThread::ControlMessage message;
  message.message_type = RtcEventLogHelperThread::ControlMessage::START_FILE;
  message.max_size_bytes = max_size_bytes <= 0
                               ? std::numeric_limits<int64_t>::max()
                               : max_size_bytes;
  message.start_time = rtc::TimeMicros();
  message.stop_time = std::numeric_limits<int64_t>::max();
  message.file.reset(FileWrapper::Create());
  FILE* file_handle = rtc::FdopenPlatformFileForWriting(platform_file);
  if (!file_handle) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    // Even though we failed to open a FILE*, the platform_file is still open
    // and needs to be closed.
    if (!rtc::ClosePlatformFile(platform_file)) {
      LOG(LS_ERROR) << "Can't close file.";
    }
    return false;
  }
  if (!message.file->OpenFromFileHandle(file_handle)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    return false;
  }
  if (!message_queue_.Insert(&message)) {
    LOG(LS_ERROR) << "Message queue full. Can't start logging.";
    return false;
  }
  helper_thread_.SignalNewEvent();
  LOG(LS_INFO) << "Starting WebRTC event log.";
  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RtcEventLogHelperThread::ControlMessage message;
  message.message_type = RtcEventLogHelperThread::ControlMessage::STOP_FILE;
  message.stop_time = rtc::TimeMicros();
  while (!message_queue_.Insert(&message)) {
    // TODO(terelius): We would like to have a blocking Insert function in the
    // SwapQueue, but for the time being we will just clear any previous
    // messages.
    // Since StopLogging waits for the thread, it is essential that we don't
    // clear any STOP_FILE messages. To ensure that there is only one call at a
    // time, we require that all calls to StopLogging are made on the same
    // thread.
    LOG(LS_ERROR) << "Message queue full. Clearing queue to stop logging.";
    message_queue_.Clear();
  }
  LOG(LS_INFO) << "Stopping WebRTC event log.";
  helper_thread_.WaitForFileFinished();
}

void RtcEventLogImpl::LogVideoReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);

  rtclog::VideoReceiveConfig* receiver_config =
      event->mutable_video_receiver_config();
  receiver_config->set_remote_ssrc(config.remote_ssrc);
  receiver_config->set_local_ssrc(config.local_ssrc);

  // TODO(perkj): Add field for rsid.
  receiver_config->set_rtcp_mode(ConvertRtcpMode(config.rtcp_mode));
  receiver_config->set_remb(config.remb);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  for (const auto& d : config.codecs) {
    rtclog::DecoderConfig* decoder = receiver_config->add_decoders();
    decoder->set_name(d.payload_name);
    decoder->set_payload_type(d.payload_type);
    if (d.rtx_payload_type != 0) {
      rtclog::RtxMap* rtx = receiver_config->add_rtx_map();
      rtx->set_payload_type(d.payload_type);
      rtx->mutable_config()->set_rtx_ssrc(config.rtx_ssrc);
      rtx->mutable_config()->set_rtx_payload_type(d.rtx_payload_type);
    }
  }
  StoreEvent(&event);
}

void RtcEventLogImpl::LogVideoSendStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);

  rtclog::VideoSendConfig* sender_config = event->mutable_video_sender_config();

  // TODO(perkj): rtclog::VideoSendConfig should only contain one SSRC.
  sender_config->add_ssrcs(config.local_ssrc);
  if (config.rtx_ssrc != 0) {
    sender_config->add_rtx_ssrcs(config.rtx_ssrc);
  }

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  // TODO(perkj): rtclog::VideoSendConfig should contain many possible codec
  // configurations.
  for (const auto& codec : config.codecs) {
    sender_config->set_rtx_payload_type(codec.rtx_payload_type);
    rtclog::EncoderConfig* encoder = sender_config->mutable_encoder();
    encoder->set_name(codec.payload_name);
    encoder->set_payload_type(codec.payload_type);

    if (config.codecs.size() > 1) {
      LOG(WARNING) << "LogVideoSendStreamConfig currently only supports one "
                   << "codec. Logging codec :" << codec.payload_name;
      break;
    }
  }

  StoreEvent(&event);
}

void RtcEventLogImpl::LogAudioReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);

  rtclog::AudioReceiveConfig* receiver_config =
      event->mutable_audio_receiver_config();
  receiver_config->set_remote_ssrc(config.remote_ssrc);
  receiver_config->set_local_ssrc(config.local_ssrc);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }
  StoreEvent(&event);
}

void RtcEventLogImpl::LogAudioSendStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);

  rtclog::AudioSendConfig* sender_config = event->mutable_audio_sender_config();

  sender_config->set_ssrc(config.local_ssrc);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  StoreEvent(&event);
}

void RtcEventLogImpl::LogRtpHeader(PacketDirection direction,
                                   const uint8_t* header,
                                   size_t packet_length) {
  LogRtpHeader(direction, header, packet_length, PacedPacketInfo::kNotAProbe);
}

void RtcEventLogImpl::LogRtpHeader(PacketDirection direction,
                                   const uint8_t* header,
                                   size_t packet_length,
                                   int probe_cluster_id) {
  // Read header length (in bytes) from packet data.
  if (packet_length < 12u) {
    return;  // Don't read outside the packet.
  }
  const bool x = (header[0] & 0x10) != 0;
  const uint8_t cc = header[0] & 0x0f;
  size_t header_length = 12u + cc * 4u;

  if (x) {
    if (packet_length < 12u + cc * 4u + 4u) {
      return;  // Don't read outside the packet.
    }
    size_t x_len = ByteReader<uint16_t>::ReadBigEndian(header + 14 + cc * 4);
    header_length += (x_len + 1) * 4;
  }

  std::unique_ptr<rtclog::Event> rtp_event(new rtclog::Event());
  rtp_event->set_timestamp_us(rtc::TimeMicros());
  rtp_event->set_type(rtclog::Event::RTP_EVENT);
  rtp_event->mutable_rtp_packet()->set_incoming(direction == kIncomingPacket);
  rtp_event->mutable_rtp_packet()->set_packet_length(packet_length);
  rtp_event->mutable_rtp_packet()->set_header(header, header_length);
  if (probe_cluster_id != PacedPacketInfo::kNotAProbe)
    rtp_event->mutable_rtp_packet()->set_probe_cluster_id(probe_cluster_id);
  StoreEvent(&rtp_event);
}

void RtcEventLogImpl::LogRtcpPacket(PacketDirection direction,
                                    const uint8_t* packet,
                                    size_t length) {
  std::unique_ptr<rtclog::Event> rtcp_event(new rtclog::Event());
  rtcp_event->set_timestamp_us(rtc::TimeMicros());
  rtcp_event->set_type(rtclog::Event::RTCP_EVENT);
  rtcp_event->mutable_rtcp_packet()->set_incoming(direction == kIncomingPacket);

  rtcp::CommonHeader header;
  const uint8_t* block_begin = packet;
  const uint8_t* packet_end = packet + length;
  RTC_DCHECK(length <= IP_PACKET_SIZE);
  uint8_t buffer[IP_PACKET_SIZE];
  uint32_t buffer_length = 0;
  while (block_begin < packet_end) {
    if (!header.Parse(block_begin, packet_end - block_begin)) {
      break;  // Incorrect message header.
    }
    const uint8_t* next_block = header.NextPacket();
    uint32_t block_size = next_block - block_begin;
    switch (header.type()) {
      case rtcp::SenderReport::kPacketType:
      case rtcp::ReceiverReport::kPacketType:
      case rtcp::Bye::kPacketType:
      case rtcp::ExtendedJitterReport::kPacketType:
      case rtcp::Rtpfb::kPacketType:
      case rtcp::Psfb::kPacketType:
      case rtcp::ExtendedReports::kPacketType:
        // We log sender reports, receiver reports, bye messages
        // inter-arrival jitter, third-party loss reports, payload-specific
        // feedback and extended reports.
        memcpy(buffer + buffer_length, block_begin, block_size);
        buffer_length += block_size;
        break;
      case rtcp::Sdes::kPacketType:
      case rtcp::App::kPacketType:
      default:
        // We don't log sender descriptions, application defined messages
        // or message blocks of unknown type.
        break;
    }

    block_begin += block_size;
  }
  rtcp_event->mutable_rtcp_packet()->set_packet_data(buffer, buffer_length);
  StoreEvent(&rtcp_event);
}

void RtcEventLogImpl::LogAudioPlayout(uint32_t ssrc) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_PLAYOUT_EVENT);
  auto playout_event = event->mutable_audio_playout_event();
  playout_event->set_local_ssrc(ssrc);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogLossBasedBweUpdate(int32_t bitrate_bps,
                                            uint8_t fraction_loss,
                                            int32_t total_packets) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::LOSS_BASED_BWE_UPDATE);
  auto bwe_event = event->mutable_loss_based_bwe_update();
  bwe_event->set_bitrate_bps(bitrate_bps);
  bwe_event->set_fraction_loss(fraction_loss);
  bwe_event->set_total_packets(total_packets);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogDelayBasedBweUpdate(int32_t bitrate_bps,
                                             BandwidthUsage detector_state) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::DELAY_BASED_BWE_UPDATE);
  auto bwe_event = event->mutable_delay_based_bwe_update();
  bwe_event->set_bitrate_bps(bitrate_bps);
  bwe_event->set_detector_state(ConvertDetectorState(detector_state));
  StoreEvent(&event);
}

void RtcEventLogImpl::LogAudioNetworkAdaptation(
    const AudioEncoderRuntimeConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);
  auto audio_network_adaptation = event->mutable_audio_network_adaptation();
  if (config.bitrate_bps)
    audio_network_adaptation->set_bitrate_bps(*config.bitrate_bps);
  if (config.frame_length_ms)
    audio_network_adaptation->set_frame_length_ms(*config.frame_length_ms);
  if (config.uplink_packet_loss_fraction) {
    audio_network_adaptation->set_uplink_packet_loss_fraction(
        *config.uplink_packet_loss_fraction);
  }
  if (config.enable_fec)
    audio_network_adaptation->set_enable_fec(*config.enable_fec);
  if (config.enable_dtx)
    audio_network_adaptation->set_enable_dtx(*config.enable_dtx);
  if (config.num_channels)
    audio_network_adaptation->set_num_channels(*config.num_channels);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogProbeClusterCreated(int id,
                                             int bitrate_bps,
                                             int min_probes,
                                             int min_bytes) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT);

  auto probe_cluster = event->mutable_probe_cluster();
  probe_cluster->set_id(id);
  probe_cluster->set_bitrate_bps(bitrate_bps);
  probe_cluster->set_min_packets(min_probes);
  probe_cluster->set_min_bytes(min_bytes);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogProbeResultSuccess(int id, int bitrate_bps) {
  LogProbeResult(id, rtclog::BweProbeResult::SUCCESS, bitrate_bps);
}

void RtcEventLogImpl::LogProbeResultFailure(int id,
                                            ProbeFailureReason failure_reason) {
  rtclog::BweProbeResult::ResultType result =
      ConvertProbeResultType(failure_reason);
  LogProbeResult(id, result, -1);
}

void RtcEventLogImpl::LogProbeResult(int id,
                                     rtclog::BweProbeResult::ResultType result,
                                     int bitrate_bps) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::BWE_PROBE_RESULT_EVENT);

  auto probe_result = event->mutable_probe_result();
  probe_result->set_id(id);
  probe_result->set_result(result);
  if (result == rtclog::BweProbeResult::SUCCESS)
    probe_result->set_bitrate_bps(bitrate_bps);
  StoreEvent(&event);
}

void RtcEventLogImpl::StoreEvent(std::unique_ptr<rtclog::Event>* event) {
  RTC_DCHECK(event != nullptr);
  RTC_DCHECK(event->get() != nullptr);
  if (!event_queue_.Insert(event)) {
    LOG(LS_ERROR) << "WebRTC event log queue full. Dropping event.";
  }
  helper_thread_.SignalNewEvent();
}

bool RtcEventLog::ParseRtcEventLog(const std::string& file_name,
                                   rtclog::EventStream* result) {
  char tmp_buffer[1024];
  int bytes_read = 0;
  std::unique_ptr<FileWrapper> dump_file(FileWrapper::Create());
  if (!dump_file->OpenFile(file_name.c_str(), true)) {
    return false;
  }
  ProtoString dump_buffer;
  while ((bytes_read = dump_file->Read(tmp_buffer, sizeof(tmp_buffer))) > 0) {
    dump_buffer.append(tmp_buffer, bytes_read);
  }
  dump_file->CloseFile();
  return result->ParseFromString(dump_buffer);
}

#endif  // ENABLE_RTC_EVENT_LOG

// RtcEventLog member functions.
std::unique_ptr<RtcEventLog> RtcEventLog::Create() {
#ifdef ENABLE_RTC_EVENT_LOG
  return std::unique_ptr<RtcEventLog>(new RtcEventLogImpl());
#else
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
#endif  // ENABLE_RTC_EVENT_LOG
}

std::unique_ptr<RtcEventLog> RtcEventLog::CreateNull() {
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
}

}  // namespace webrtc
