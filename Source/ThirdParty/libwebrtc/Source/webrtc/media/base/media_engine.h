/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_ENGINE_H_
#define MEDIA_BASE_MEDIA_ENGINE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/audio/audio_device.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/audio_state.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/stream_params.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {

class AudioMixer;
class Call;

// Checks that the scalability_mode value of each encoding is supported by at
// least one video codec of the list. If the list is empty, no check is done.
RTCError CheckScalabilityModeValues(const RtpParameters& new_parameters,
                                    std::span<const Codec> send_codecs,
                                    std::optional<Codec> send_codec);

// Checks the parameters have valid and supported values, and checks parameters
// with CheckScalabilityModeValues().
RTCError CheckRtpParametersValues(const RtpParameters& new_parameters,
                                  std::span<const Codec> send_codecs,
                                  std::optional<Codec> send_codec,
                                  const FieldTrialsView& field_trials);

// Checks that the immutable values have not changed in new_parameters and
// checks all parameters with CheckRtpParametersValues().
RTCError CheckRtpParametersInvalidModificationAndValues(
    const RtpParameters& old_parameters,
    const RtpParameters& new_parameters,
    std::span<const Codec> send_codecs,
    std::optional<Codec> send_codec,
    const FieldTrialsView& field_trials);

// Checks that the immutable values have not changed in new_parameters and
// checks parameters (except SVC) with CheckRtpParametersValues(). It should
// usually be paired with a call to CheckScalabilityModeValues().
RTCError CheckRtpParametersInvalidModificationAndValues(
    const RtpParameters& old_parameters,
    const RtpParameters& new_parameters,
    const FieldTrialsView& field_trials);

class RtpHeaderExtensionQueryInterface {
 public:
  virtual ~RtpHeaderExtensionQueryInterface() = default;

  // Returns a vector of RtpHeaderExtensionCapability, whose direction is
  // kStopped if the extension is stopped (not used) by default.
  virtual std::vector<RtpHeaderExtensionCapability> GetRtpHeaderExtensions(
      const FieldTrialsView* field_trials) const = 0;
};

// Interface for creating voice media channels.
//
// Methods on this interface only perform thread-safe initialization and do not
// mutate engine-level state. Consequently, it is safe to call these methods
// from any thread, including the signaling thread.
class VoiceChannelFactoryInterface {
 public:
  virtual ~VoiceChannelFactoryInterface() = default;

  // Safe to be called from the signaling thread.
  // The `options` parameter configures stream/channel-specific settings (e.g.,
  // jitter buffer, ANA). Global options (like AEC, AGC, NS) should be
  // configured directly at the engine level via ApplyGlobalOptions.
  virtual std::unique_ptr<VoiceMediaSendChannelInterface> CreateSendChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const CryptoOptions& crypto_options,
      absl::AnyInvocable<void()> parameters_changed_callback = nullptr) = 0;

  // Safe to be called from the signaling thread.
  // The `options` parameter configures stream/channel-specific settings (e.g.,
  // jitter buffer). Global options (like AEC, AGC, NS) should be configured
  // directly at the engine level via ApplyGlobalOptions.
  virtual std::unique_ptr<VoiceMediaReceiveChannelInterface>
  CreateReceiveChannel(const Environment& env,
                       Call* call,
                       const MediaConfig& config,
                       const AudioOptions& options,
                       const CryptoOptions& crypto_options) = 0;
};

// Interface for creating video media channels.
//
// Methods on this interface only perform thread-safe initialization and do not
// mutate engine-level state. Consequently, it is safe to call these methods
// from any thread, including the signaling thread.
class VideoChannelFactoryInterface {
 public:
  virtual ~VideoChannelFactoryInterface() = default;

  // Safe to be called from the signaling thread.
  virtual std::unique_ptr<VideoMediaSendChannelInterface> CreateSendChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const CryptoOptions& crypto_options,
      VideoBitrateAllocatorFactory* video_bitrate_allocator_factory,
      VideoMediaSendChannelInterface::EncoderSwitchRequestCallback
          video_encoder_switch_request_callback,
      absl::AnyInvocable<void()> parameters_changed_callback) = 0;

  // Safe to be called from the signaling thread.
  virtual std::unique_ptr<VideoMediaReceiveChannelInterface>
  CreateReceiveChannel(const Environment& env,
                       Call* call,
                       const MediaConfig& config,
                       const CryptoOptions& crypto_options) = 0;
};

class VoiceEngineInterface : public RtpHeaderExtensionQueryInterface,
                             public VoiceChannelFactoryInterface {
 public:
  VoiceEngineInterface() = default;
  ~VoiceEngineInterface() override = default;

  VoiceEngineInterface(const VoiceEngineInterface&) = delete;
  VoiceEngineInterface& operator=(const VoiceEngineInterface&) = delete;

  // Initialization
  // Starts the engine.
  virtual void Init() = 0;
  // Stops the engine.
  virtual void Terminate() = 0;
  // Applies global options (like APM settings) to the engine.
  virtual void ApplyGlobalOptions(const AudioOptions& options) = 0;

  // TODO(solenberg): Remove once VoE API refactoring is done.
  virtual scoped_refptr<AudioState> GetAudioState() const = 0;

  // VoiceChannelFactoryInterface overrides.
  std::unique_ptr<VoiceMediaSendChannelInterface> CreateSendChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const CryptoOptions& crypto_options,
      absl::AnyInvocable<void()> parameters_changed_callback =
          nullptr) override = 0;

  std::unique_ptr<VoiceMediaReceiveChannelInterface> CreateReceiveChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const CryptoOptions& crypto_options) override = 0;

  // Legacy: Retrieve list of supported codecs.
  // + protection codecs, and assigns PT numbers that may have to be
  // reassigned.
  // TODO: https://issues.webrtc.org/360058654 - remove when all users updated.
  virtual const std::vector<Codec>& LegacySendCodecs() const = 0;
  virtual const std::vector<Codec>& LegacyRecvCodecs() const = 0;

  virtual const scoped_refptr<AudioEncoderFactory>& encoder_factory() const = 0;
  virtual const scoped_refptr<AudioDecoderFactory>& decoder_factory() const = 0;

  // Starts AEC dump using existing file, a maximum file size in bytes can be
  // specified. Logging is stopped just before the size limit is exceeded.
  // If max_size_bytes is set to a value <= 0, no limit will be used.
  virtual bool StartAecDump(FileWrapper file, int64_t max_size_bytes) = 0;

  // Stops recording AEC dump.
  virtual void StopAecDump() = 0;

  virtual std::optional<AudioDeviceModule::Stats> GetAudioDeviceStats() = 0;

  // Returns true if the engine handles built-in codecs like DTMF and CN
  // automatically.
  virtual bool NeedsAuxiliaryCodecsAdded() const { return false; }
};

class VideoEngineInterface : public RtpHeaderExtensionQueryInterface,
                             public VideoChannelFactoryInterface {
 public:
  VideoEngineInterface() = default;
  ~VideoEngineInterface() override = default;

  VideoEngineInterface(const VideoEngineInterface&) = delete;
  VideoEngineInterface& operator=(const VideoEngineInterface&) = delete;

  // VideoChannelFactoryInterface overrides.
  std::unique_ptr<VideoMediaSendChannelInterface> CreateSendChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const CryptoOptions& crypto_options,
      VideoBitrateAllocatorFactory* video_bitrate_allocator_factory,
      VideoMediaSendChannelInterface::EncoderSwitchRequestCallback
          video_encoder_switch_request_callback,
      absl::AnyInvocable<void()> parameters_changed_callback) override = 0;

  std::unique_ptr<VideoMediaReceiveChannelInterface> CreateReceiveChannel(
      const Environment& env,
      Call* call,
      const MediaConfig& config,
      const CryptoOptions& crypto_options) override = 0;

  // Legacy: Retrieve list of supported codecs.
  // + protection codecs, and assigns PT numbers that may have to be
  // reassigned.
  // This functionality is being moved to the CodecVendor class.
  // TODO: https://issues.webrtc.org/360058654 - deprecate and remove.
  virtual std::vector<Codec> LegacySendCodecs() const = 0;
  virtual std::vector<Codec> LegacyRecvCodecs() const = 0;
  // As above, but if include_rtx is false, don't include RTX codecs.
  virtual std::vector<Codec> LegacySendCodecs(bool include_rtx) const = 0;
  virtual std::vector<Codec> LegacyRecvCodecs(bool include_rtx) const = 0;

  virtual VideoEncoderFactory* encoder_factory() const = 0;
  virtual VideoDecoderFactory* decoder_factory() const = 0;

  virtual std::vector<SdpVideoFormat> GetSupportedFormats(
      bool is_decoder) const = 0;

  // Returns true if the engine handles built-in codecs like RTX, RED, FEC
  // automatically.
  virtual bool NeedsAuxiliaryCodecsAdded() const { return false; }
};

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  virtual ~MediaEngineInterface() {}

  // Init . Needs to be called on the worker thread.
  virtual void Init() = 0;
  // Terminate. Needs to be called on the worker thread.
  virtual void Terminate() = 0;

  virtual VoiceEngineInterface& voice() = 0;
  virtual VideoEngineInterface& video() = 0;
  virtual const VoiceEngineInterface& voice() const = 0;
  virtual const VideoEngineInterface& video() const = 0;
};

// CompositeMediaEngine constructs a MediaEngine from separate
// voice and video engine classes.
// Optionally owns a FieldTrialsView trials map.
class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine(std::unique_ptr<FieldTrialsView> trials,
                       std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  CompositeMediaEngine(std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  ~CompositeMediaEngine() override;

  void Init() override;
  void Terminate() override;

  VoiceEngineInterface& voice() override;
  VideoEngineInterface& video() override;
  const VoiceEngineInterface& voice() const override;
  const VideoEngineInterface& video() const override;

 private:
  const std::unique_ptr<FieldTrialsView> trials_;
  const std::unique_ptr<VoiceEngineInterface> voice_engine_;
  const std::unique_ptr<VideoEngineInterface> video_engine_;
};

RtpParameters CreateRtpParametersWithOneEncoding();
RtpParameters CreateRtpParametersWithEncodings(StreamParams sp);

// Returns a vector of RTP extensions as visible from RtpSender/Receiver
// GetCapabilities(). The returned vector only shows what will definitely be
// offered by default, i.e. the list of extensions returned from
// GetRtpHeaderExtensions() that are not kStopped.
std::vector<RtpHeaderExtensionCapability>
GetDefaultEnabledRtpHeaderCapabilities(
    const RtpHeaderExtensionQueryInterface& query_interface,
    const FieldTrialsView* field_trials);

}  //  namespace webrtc


#endif  // MEDIA_BASE_MEDIA_ENGINE_H_
