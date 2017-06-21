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
#include <vector>

#include "webrtc/base/platform_file.h"
#include "webrtc/config.h"

namespace webrtc {

// Forward declaration of storage class that is automatically generated from
// the protobuf file.
namespace rtclog {
class EventStream;

struct StreamConfig {
  uint32_t local_ssrc = 0;
  uint32_t remote_ssrc = 0;
  uint32_t rtx_ssrc = 0;
  std::string rsid;

  bool remb = false;
  std::vector<RtpExtension> rtp_extensions;

  RtcpMode rtcp_mode = RtcpMode::kReducedSize;

  struct Codec {
    Codec(const std::string& payload_name,
          int payload_type,
          int rtx_payload_type)
        : payload_name(payload_name),
          payload_type(payload_type),
          rtx_payload_type(rtx_payload_type) {}

    std::string payload_name;
    int payload_type;
    int rtx_payload_type;
  };
  std::vector<Codec> codecs;
};

}  // namespace rtclog

class Clock;
class RtcEventLogImpl;
struct AudioEncoderRuntimeConfig;

enum class MediaType;
enum class BandwidthUsage;

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
  static std::unique_ptr<RtcEventLog> Create(const Clock*) {
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

  // Logs configuration information for a video receive stream.
  virtual void LogVideoReceiveStreamConfig(
      const rtclog::StreamConfig& config) = 0;

  // Logs configuration information for a video send stream.
  virtual void LogVideoSendStreamConfig(const rtclog::StreamConfig& config) = 0;

  // Logs configuration information for an audio receive stream.
  virtual void LogAudioReceiveStreamConfig(
      const rtclog::StreamConfig& config) = 0;

  // Logs configuration information for an audio send stream.
  virtual void LogAudioSendStreamConfig(const rtclog::StreamConfig& config) = 0;

  // Logs the header of an incoming or outgoing RTP packet. packet_length
  // is the total length of the packet, including both header and payload.
  virtual void LogRtpHeader(PacketDirection direction,
                            const uint8_t* header,
                            size_t packet_length) = 0;

  // Same as above but used on the sender side to log packets that are part of
  // a probe cluster.
  virtual void LogRtpHeader(PacketDirection direction,
                            const uint8_t* header,
                            size_t packet_length,
                            int probe_cluster_id) = 0;

  // Logs an incoming or outgoing RTCP packet.
  virtual void LogRtcpPacket(PacketDirection direction,
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
      const AudioEncoderRuntimeConfig& config) = 0;

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
class RtcEventLogNullImpl : public RtcEventLog {
 public:
  bool StartLogging(const std::string& /* file_name */,
                    int64_t /* max_size_bytes */) override {
    return false;
  }
  bool StartLogging(rtc::PlatformFile /* platform_file */,
                    int64_t /* max_size_bytes */) override {
    return false;
  }
  void StopLogging() override {}
  void LogVideoReceiveStreamConfig(
      const rtclog::StreamConfig&) override {}
  void LogVideoSendStreamConfig(const rtclog::StreamConfig&) override {}
  void LogAudioReceiveStreamConfig(
      const rtclog::StreamConfig&) override {}
  void LogAudioSendStreamConfig(const rtclog::StreamConfig&) override {}
  void LogRtpHeader(PacketDirection,
                    const uint8_t*,
                    size_t) override {}
  void LogRtpHeader(PacketDirection,
                    const uint8_t*,
                    size_t,
                    int) override {}
  void LogRtcpPacket(PacketDirection,
                     const uint8_t*,
                     size_t) override {}
  void LogAudioPlayout(uint32_t) override {}
  void LogLossBasedBweUpdate(int32_t,
                             uint8_t,
                             int32_t) override {}
  void LogDelayBasedBweUpdate(int32_t,
                              BandwidthUsage) override {}
  void LogAudioNetworkAdaptation(
      const AudioEncoderRuntimeConfig&) override {}
  void LogProbeClusterCreated(int,
                              int,
                              int,
                              int) override{};
  void LogProbeResultSuccess(int, int) override{};
  void LogProbeResultFailure(int,
                             ProbeFailureReason) override{};
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
