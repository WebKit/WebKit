/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/common_audio/smoothing_filter.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "webrtc/modules/audio_coding/codecs/opus/opus_interface.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

namespace webrtc {

class RtcEventLog;

struct CodecInst;

class AudioEncoderOpus final : public AudioEncoder {
 public:
  enum ApplicationMode {
    kVoip = 0,
    kAudio = 1,
  };

  struct Config {
    Config();
    Config(const Config&);
    ~Config();
    Config& operator=(const Config&);

    bool IsOk() const;
    int GetBitrateBps() const;
    // Returns empty if the current bitrate falls within the hysteresis window,
    // defined by complexity_threshold_bps +/- complexity_threshold_window_bps.
    // Otherwise, returns the current complexity depending on whether the
    // current bitrate is above or below complexity_threshold_bps.
    rtc::Optional<int> GetNewComplexity() const;

    int frame_size_ms = 20;
    size_t num_channels = 1;
    int payload_type = 120;
    ApplicationMode application = kVoip;
    rtc::Optional<int> bitrate_bps;  // Unset means to use default value.
    bool fec_enabled = false;
    int max_playback_rate_hz = 48000;
    int complexity = kDefaultComplexity;
    // This value may change in the struct's constructor.
    int low_rate_complexity = kDefaultComplexity;
    // low_rate_complexity is used when the bitrate is below this threshold.
    int complexity_threshold_bps = 12500;
    int complexity_threshold_window_bps = 1500;
    bool dtx_enabled = false;
    std::vector<int> supported_frame_lengths_ms;
    const Clock* clock = Clock::GetRealTimeClock();
    int uplink_bandwidth_update_interval_ms = 200;

   private:
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS) || defined(WEBRTC_ARCH_ARM)
    // If we are on Android, iOS and/or ARM, use a lower complexity setting as
    // default, to save encoder complexity.
    static const int kDefaultComplexity = 5;
#else
    static const int kDefaultComplexity = 9;
#endif
  };

  using AudioNetworkAdaptorCreator =
      std::function<std::unique_ptr<AudioNetworkAdaptor>(const std::string&,
                                                         RtcEventLog*,
                                                         const Clock*)>;
  AudioEncoderOpus(
      const Config& config,
      AudioNetworkAdaptorCreator&& audio_network_adaptor_creator = nullptr,
      std::unique_ptr<SmoothingFilter> bitrate_smoother = nullptr);

  explicit AudioEncoderOpus(const CodecInst& codec_inst);

  ~AudioEncoderOpus() override;

  int SampleRateHz() const override;
  size_t NumChannels() const override;
  size_t Num10MsFramesInNextPacket() const override;
  size_t Max10MsFramesInAPacket() const override;
  int GetTargetBitrate() const override;

  void Reset() override;
  bool SetFec(bool enable) override;

  // Set Opus DTX. Once enabled, Opus stops transmission, when it detects voice
  // being inactive. During that, it still sends 2 packets (one for content, one
  // for signaling) about every 400 ms.
  bool SetDtx(bool enable) override;
  bool GetDtx() const override;

  bool SetApplication(Application application) override;
  void SetMaxPlaybackRate(int frequency_hz) override;
  bool EnableAudioNetworkAdaptor(const std::string& config_string,
                                 RtcEventLog* event_log,
                                 const Clock* clock) override;
  void DisableAudioNetworkAdaptor() override;
  void OnReceivedUplinkPacketLossFraction(
      float uplink_packet_loss_fraction) override;
  void OnReceivedUplinkBandwidth(
      int target_audio_bitrate_bps,
      rtc::Optional<int64_t> probing_interval_ms) override;
  void OnReceivedRtt(int rtt_ms) override;
  void OnReceivedOverhead(size_t overhead_bytes_per_packet) override;
  void SetReceiverFrameLengthRange(int min_frame_length_ms,
                                   int max_frame_length_ms) override;
  rtc::ArrayView<const int> supported_frame_lengths_ms() const {
    return config_.supported_frame_lengths_ms;
  }

  // Getters for testing.
  float packet_loss_rate() const { return packet_loss_rate_; }
  ApplicationMode application() const { return config_.application; }
  bool fec_enabled() const { return config_.fec_enabled; }
  size_t num_channels_to_encode() const { return num_channels_to_encode_; }
  int next_frame_length_ms() const { return next_frame_length_ms_; }

 protected:
  EncodedInfo EncodeImpl(uint32_t rtp_timestamp,
                         rtc::ArrayView<const int16_t> audio,
                         rtc::Buffer* encoded) override;

 private:
  class PacketLossFractionSmoother;

  size_t Num10msFramesPerPacket() const;
  size_t SamplesPer10msFrame() const;
  size_t SufficientOutputBufferSize() const;
  bool RecreateEncoderInstance(const Config& config);
  void SetFrameLength(int frame_length_ms);
  void SetNumChannelsToEncode(size_t num_channels_to_encode);
  void SetProjectedPacketLossRate(float fraction);

  // TODO(minyue): remove "override" when we can deprecate
  // |AudioEncoder::SetTargetBitrate|.
  void SetTargetBitrate(int target_bps) override;

  void ApplyAudioNetworkAdaptor();
  std::unique_ptr<AudioNetworkAdaptor> DefaultAudioNetworkAdaptorCreator(
      const std::string& config_string,
      RtcEventLog* event_log,
      const Clock* clock) const;

  void MaybeUpdateUplinkBandwidth();

  Config config_;
  const bool send_side_bwe_with_overhead_;
  float packet_loss_rate_;
  std::vector<int16_t> input_buffer_;
  OpusEncInst* inst_;
  uint32_t first_timestamp_in_buffer_;
  size_t num_channels_to_encode_;
  int next_frame_length_ms_;
  int complexity_;
  std::unique_ptr<PacketLossFractionSmoother> packet_loss_fraction_smoother_;
  AudioNetworkAdaptorCreator audio_network_adaptor_creator_;
  std::unique_ptr<AudioNetworkAdaptor> audio_network_adaptor_;
  rtc::Optional<size_t> overhead_bytes_per_packet_;
  const std::unique_ptr<SmoothingFilter> bitrate_smoother_;
  rtc::Optional<int64_t> bitrate_smoother_last_update_time_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioEncoderOpus);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_
