/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_

#include <memory>
#include <string>

#include "webrtc/base/platform_file.h"
#include "webrtc/call/audio_receive_stream.h"
#include "webrtc/call/audio_send_stream.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

// Forward declaration of storage class that is automatically generated from
// the protobuf file.
namespace rtclog {
class EventStream;
}  // namespace rtclog

class Clock;
class RtcEventLogImpl;

enum class MediaType;

enum PacketDirection { kIncomingPacket = 0, kOutgoingPacket };
enum ProbeFailureReason {
  kInvalidSendReceiveInterval,
  kInvalidSendReceiveRatio,
  kTimeout
};

class RtcEventLog {
 public:
  virtual ~RtcEventLog() {}

  // Factory method to create an RtcEventLog object.
  static std::unique_ptr<RtcEventLog> Create();
  // TODO(nisse): webrtc::Clock is deprecated. Delete this method and
  // above forward declaration of Clock when
  // webrtc/system_wrappers/include/clock.h is deleted.
  static std::unique_ptr<RtcEventLog> Create(const Clock* clock) {
    return Create();
  }

  // Create an RtcEventLog object that does nothing.
  static std::unique_ptr<RtcEventLog> CreateNull();

  // Starts logging a maximum of max_size_bytes bytes to the specified file.
  // If the file already exists it will be overwritten.
  // If max_size_bytes <= 0, logging will be active until StopLogging is called.
  // The function has no effect and returns false if we can't start a new log
  // e.g. because we are already logging or the file cannot be opened.
  virtual bool StartLogging(const std::string& file_name,
                            int64_t max_size_bytes) = 0;

  // Same as above. The RtcEventLog takes ownership of the file if the call
  // is successful, i.e. if it returns true.
  virtual bool StartLogging(rtc::PlatformFile platform_file,
                            int64_t max_size_bytes) = 0;

  // Deprecated. Pass an explicit file size limit.
  bool StartLogging(const std::string& file_name) {
    return StartLogging(file_name, 10000000);
  }

  // Deprecated. Pass an explicit file size limit.
  bool StartLogging(rtc::PlatformFile platform_file) {
    return StartLogging(platform_file, 10000000);
  }

  // Stops logging to file and waits until the thread has finished.
  virtual void StopLogging() = 0;

  // Logs configuration information for webrtc::VideoReceiveStream.
  virtual void LogVideoReceiveStreamConfig(
      const webrtc::VideoReceiveStream::Config& config) = 0;

  // Logs configuration information for webrtc::VideoSendStream.
  virtual void LogVideoSendStreamConfig(
      const webrtc::VideoSendStream::Config& config) = 0;

  // Logs configuration information for webrtc::AudioReceiveStream.
  virtual void LogAudioReceiveStreamConfig(
      const webrtc::AudioReceiveStream::Config& config) = 0;

  // Logs configuration information for webrtc::AudioSendStream.
  virtual void LogAudioSendStreamConfig(
      const webrtc::AudioSendStream::Config& config) = 0;

  // Logs the header of an incoming or outgoing RTP packet. packet_length
  // is the total length of the packet, including both header and payload.
  virtual void LogRtpHeader(PacketDirection direction,
                            MediaType media_type,
                            const uint8_t* header,
                            size_t packet_length) = 0;

  // Same as above but used on the sender side to log packets that are part of
  // a probe cluster.
  virtual void LogRtpHeader(PacketDirection direction,
                            MediaType media_type,
                            const uint8_t* header,
                            size_t packet_length,
                            int probe_cluster_id) = 0;

  // Logs an incoming or outgoing RTCP packet.
  virtual void LogRtcpPacket(PacketDirection direction,
                             MediaType media_type,
                             const uint8_t* packet,
                             size_t length) = 0;

  // Logs an audio playout event.
  virtual void LogAudioPlayout(uint32_t ssrc) = 0;

  // Logs a bitrate update from the bandwidth estimator based on packet loss.
  virtual void LogLossBasedBweUpdate(int32_t bitrate_bps,
                                     uint8_t fraction_loss,
                                     int32_t total_packets) = 0;

  // Logs a bitrate update from the bandwidth estimator based on delay changes.
  virtual void LogDelayBasedBweUpdate(int32_t bitrate_bps,
                                      BandwidthUsage detector_state) = 0;

  // Logs audio encoder re-configuration driven by audio network adaptor.
  virtual void LogAudioNetworkAdaptation(
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config) = 0;

  // Logs when a probe cluster is created.
  virtual void LogProbeClusterCreated(int id,
                                      int bitrate_bps,
                                      int min_probes,
                                      int min_bytes) = 0;

  // Logs the result of a successful probing attempt.
  virtual void LogProbeResultSuccess(int id, int bitrate_bps) = 0;

  // Logs the result of an unsuccessful probing attempt.
  virtual void LogProbeResultFailure(int id,
                                     ProbeFailureReason failure_reason) = 0;

  // Reads an RtcEventLog file and returns true when reading was successful.
  // The result is stored in the given EventStream object.
  // The order of the events in the EventStream is implementation defined.
  // The current implementation writes a LOG_START event, then the old
  // configurations, then the remaining events in timestamp order and finally
  // a LOG_END event. However, this might change without further notice.
  // TODO(terelius): Change result type to a vector?
  static bool ParseRtcEventLog(const std::string& file_name,
                               rtclog::EventStream* result);
};

// No-op implementation is used if flag is not set, or in tests.
class RtcEventLogNullImpl final : public RtcEventLog {
 public:
  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override {
    return false;
  }
  bool StartLogging(rtc::PlatformFile platform_file,
                    int64_t max_size_bytes) override;
  void StopLogging() override {}
  void LogVideoReceiveStreamConfig(
      const VideoReceiveStream::Config& config) override {}
  void LogVideoSendStreamConfig(
      const VideoSendStream::Config& config) override {}
  void LogAudioReceiveStreamConfig(
      const AudioReceiveStream::Config& config) override {}
  void LogAudioSendStreamConfig(
      const AudioSendStream::Config& config) override {}
  void LogRtpHeader(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length) override {}
  void LogRtpHeader(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id) override {}
  void LogRtcpPacket(PacketDirection direction,
                     MediaType media_type,
                     const uint8_t* packet,
                     size_t length) override {}
  void LogAudioPlayout(uint32_t ssrc) override {}
  void LogLossBasedBweUpdate(int32_t bitrate_bps,
                             uint8_t fraction_loss,
                             int32_t total_packets) override {}
  void LogDelayBasedBweUpdate(int32_t bitrate_bps,
                              BandwidthUsage detector_state) override {}
  void LogAudioNetworkAdaptation(
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config) override {}
  void LogProbeClusterCreated(int id,
                              int bitrate_bps,
                              int min_probes,
                              int min_bytes) override{};
  void LogProbeResultSuccess(int id, int bitrate_bps) override{};
  void LogProbeResultFailure(int id,
                             ProbeFailureReason failure_reason) override{};
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
