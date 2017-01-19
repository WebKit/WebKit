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
#include "webrtc/base/swap_queue.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_helper_thread.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"

#ifdef ENABLE_RTC_EVENT_LOG
// Files generated at build-time by the protobuf compiler.
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
  explicit RtcEventLogImpl(const Clock* clock);
  ~RtcEventLogImpl() override;

  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override;
  bool StartLogging(rtc::PlatformFile platform_file,
                    int64_t max_size_bytes) override;
  void StopLogging() override;
  void LogVideoReceiveStreamConfig(
      const VideoReceiveStream::Config& config) override;
  void LogVideoSendStreamConfig(const VideoSendStream::Config& config) override;
  void LogAudioReceiveStreamConfig(
      const AudioReceiveStream::Config& config) override;
  void LogAudioSendStreamConfig(const AudioSendStream::Config& config) override;
  void LogRtpHeader(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length) override;
  void LogRtcpPacket(PacketDirection direction,
                     MediaType media_type,
                     const uint8_t* packet,
                     size_t length) override;
  void LogAudioPlayout(uint32_t ssrc) override;
  void LogBwePacketLossEvent(int32_t bitrate,
                             uint8_t fraction_loss,
                             int32_t total_packets) override;

 private:
  void StoreEvent(std::unique_ptr<rtclog::Event>* event);

  // Message queue for passing control messages to the logging thread.
  SwapQueue<RtcEventLogHelperThread::ControlMessage> message_queue_;

  // Message queue for passing events to the logging thread.
  SwapQueue<std::unique_ptr<rtclog::Event> > event_queue_;

  const Clock* const clock_;

  RtcEventLogHelperThread helper_thread_;
  rtc::ThreadChecker thread_checker_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcEventLogImpl);
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

rtclog::MediaType ConvertMediaType(MediaType media_type) {
  switch (media_type) {
    case MediaType::ANY:
      return rtclog::MediaType::ANY;
    case MediaType::AUDIO:
      return rtclog::MediaType::AUDIO;
    case MediaType::VIDEO:
      return rtclog::MediaType::VIDEO;
    case MediaType::DATA:
      return rtclog::MediaType::DATA;
  }
  RTC_NOTREACHED();
  return rtclog::ANY;
}

// The RTP and RTCP buffers reserve space for twice the expected number of
// sent packets because they also contain received packets.
static const int kEventsPerSecond = 1000;
static const int kControlMessagesPerSecond = 10;
}  // namespace

// RtcEventLogImpl member functions.
RtcEventLogImpl::RtcEventLogImpl(const Clock* clock)
    // Allocate buffers for roughly one second of history.
    : message_queue_(kControlMessagesPerSecond),
      event_queue_(kEventsPerSecond),
      clock_(clock),
      helper_thread_(&message_queue_, &event_queue_, clock),
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
  message.start_time = clock_->TimeInMicroseconds();
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
  message.start_time = clock_->TimeInMicroseconds();
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
  message.stop_time = clock_->TimeInMicroseconds();
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
    const VideoReceiveStream::Config& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);

  rtclog::VideoReceiveConfig* receiver_config =
      event->mutable_video_receiver_config();
  receiver_config->set_remote_ssrc(config.rtp.remote_ssrc);
  receiver_config->set_local_ssrc(config.rtp.local_ssrc);

  receiver_config->set_rtcp_mode(ConvertRtcpMode(config.rtp.rtcp_mode));
  receiver_config->set_remb(config.rtp.remb);

  for (const auto& kv : config.rtp.rtx) {
    rtclog::RtxMap* rtx = receiver_config->add_rtx_map();
    rtx->set_payload_type(kv.first);
    rtx->mutable_config()->set_rtx_ssrc(kv.second.ssrc);
    rtx->mutable_config()->set_rtx_payload_type(kv.second.payload_type);
  }

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  for (const auto& d : config.decoders) {
    rtclog::DecoderConfig* decoder = receiver_config->add_decoders();
    decoder->set_name(d.payload_name);
    decoder->set_payload_type(d.payload_type);
  }
  StoreEvent(&event);
}

void RtcEventLogImpl::LogVideoSendStreamConfig(
    const VideoSendStream::Config& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);

  rtclog::VideoSendConfig* sender_config = event->mutable_video_sender_config();

  for (const auto& ssrc : config.rtp.ssrcs) {
    sender_config->add_ssrcs(ssrc);
  }

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  for (const auto& rtx_ssrc : config.rtp.rtx.ssrcs) {
    sender_config->add_rtx_ssrcs(rtx_ssrc);
  }
  sender_config->set_rtx_payload_type(config.rtp.rtx.payload_type);

  rtclog::EncoderConfig* encoder = sender_config->mutable_encoder();
  encoder->set_name(config.encoder_settings.payload_name);
  encoder->set_payload_type(config.encoder_settings.payload_type);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogAudioReceiveStreamConfig(
    const AudioReceiveStream::Config& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);

  rtclog::AudioReceiveConfig* receiver_config =
      event->mutable_audio_receiver_config();
  receiver_config->set_remote_ssrc(config.rtp.remote_ssrc);
  receiver_config->set_local_ssrc(config.rtp.local_ssrc);

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }
  StoreEvent(&event);
}

void RtcEventLogImpl::LogAudioSendStreamConfig(
    const AudioSendStream::Config& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);

  rtclog::AudioSendConfig* sender_config = event->mutable_audio_sender_config();

  sender_config->set_ssrc(config.rtp.ssrc);

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  StoreEvent(&event);
}

void RtcEventLogImpl::LogRtpHeader(PacketDirection direction,
                                   MediaType media_type,
                                   const uint8_t* header,
                                   size_t packet_length) {
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
  rtp_event->set_timestamp_us(clock_->TimeInMicroseconds());
  rtp_event->set_type(rtclog::Event::RTP_EVENT);
  rtp_event->mutable_rtp_packet()->set_incoming(direction == kIncomingPacket);
  rtp_event->mutable_rtp_packet()->set_type(ConvertMediaType(media_type));
  rtp_event->mutable_rtp_packet()->set_packet_length(packet_length);
  rtp_event->mutable_rtp_packet()->set_header(header, header_length);
  StoreEvent(&rtp_event);
}

void RtcEventLogImpl::LogRtcpPacket(PacketDirection direction,
                                    MediaType media_type,
                                    const uint8_t* packet,
                                    size_t length) {
  std::unique_ptr<rtclog::Event> rtcp_event(new rtclog::Event());
  rtcp_event->set_timestamp_us(clock_->TimeInMicroseconds());
  rtcp_event->set_type(rtclog::Event::RTCP_EVENT);
  rtcp_event->mutable_rtcp_packet()->set_incoming(direction == kIncomingPacket);
  rtcp_event->mutable_rtcp_packet()->set_type(ConvertMediaType(media_type));

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
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::AUDIO_PLAYOUT_EVENT);
  auto playout_event = event->mutable_audio_playout_event();
  playout_event->set_local_ssrc(ssrc);
  StoreEvent(&event);
}

void RtcEventLogImpl::LogBwePacketLossEvent(int32_t bitrate,
                                            uint8_t fraction_loss,
                                            int32_t total_packets) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(clock_->TimeInMicroseconds());
  event->set_type(rtclog::Event::BWE_PACKET_LOSS_EVENT);
  auto bwe_event = event->mutable_bwe_packet_loss_event();
  bwe_event->set_bitrate(bitrate);
  bwe_event->set_fraction_loss(fraction_loss);
  bwe_event->set_total_packets(total_packets);
  StoreEvent(&event);
}

void RtcEventLogImpl::StoreEvent(std::unique_ptr<rtclog::Event>* event) {
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
  std::string dump_buffer;
  while ((bytes_read = dump_file->Read(tmp_buffer, sizeof(tmp_buffer))) > 0) {
    dump_buffer.append(tmp_buffer, bytes_read);
  }
  dump_file->CloseFile();
  return result->ParseFromString(dump_buffer);
}

#endif  // ENABLE_RTC_EVENT_LOG

bool RtcEventLogNullImpl::StartLogging(rtc::PlatformFile platform_file,
                                       int64_t max_size_bytes) {
  // The platform_file is open and needs to be closed.
  if (!rtc::ClosePlatformFile(platform_file)) {
    LOG(LS_ERROR) << "Can't close file.";
  }
  return false;
}

// RtcEventLog member functions.
std::unique_ptr<RtcEventLog> RtcEventLog::Create(const Clock* clock) {
#ifdef ENABLE_RTC_EVENT_LOG
  return std::unique_ptr<RtcEventLog>(new RtcEventLogImpl(clock));
#else
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
#endif  // ENABLE_RTC_EVENT_LOG
}

std::unique_ptr<RtcEventLog> RtcEventLog::CreateNull() {
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
}

}  // namespace webrtc
