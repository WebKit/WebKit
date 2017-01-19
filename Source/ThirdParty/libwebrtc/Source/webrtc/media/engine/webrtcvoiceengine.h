/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVOICEENGINE_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVOICEENGINE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/call/audio_state.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/networkroute.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call.h"
#include "webrtc/config.h"
#include "webrtc/media/base/rtputils.h"
#include "webrtc/media/engine/webrtccommon.h"
#include "webrtc/media/engine/webrtcvoe.h"
#include "webrtc/pc/channel.h"

namespace cricket {

class AudioDeviceModule;
class AudioSource;
class VoEWrapper;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine final : public webrtc::TraceCallback  {
  friend class WebRtcVoiceMediaChannel;
 public:
  // Exposed for the WVoE/MC unit test.
  static bool ToCodecInst(const AudioCodec& in, webrtc::CodecInst* out);

  WebRtcVoiceEngine(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory);
  // Dependency injection for testing.
  WebRtcVoiceEngine(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
      VoEWrapper* voe_wrapper);
  ~WebRtcVoiceEngine() override;

  rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const;
  VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const AudioOptions& options);

  int GetInputLevel();

  const std::vector<AudioCodec>& send_codecs() const;
  const std::vector<AudioCodec>& recv_codecs() const;
  RtpCapabilities GetCapabilities() const;

  // For tracking WebRtc channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by WebRtcVoiceMediaChannel.
  void RegisterChannel(WebRtcVoiceMediaChannel* channel);
  void UnregisterChannel(WebRtcVoiceMediaChannel* channel);

  // Called by WebRtcVoiceMediaChannel to set a gain offset from
  // the default AGC target level.
  bool AdjustAgcLevel(int delta);

  VoEWrapper* voe() { return voe_wrapper_.get(); }
  int GetLastEngineError();

  // Starts AEC dump using an existing file. A maximum file size in bytes can be
  // specified. When the maximum file size is reached, logging is stopped and
  // the file is closed. If max_size_bytes is set to <= 0, no limit will be
  // used.
  bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes);

  // Stops AEC dump.
  void StopAecDump();

 private:
  // Every option that is "set" will be applied. Every option not "set" will be
  // ignored. This allows us to selectively turn on and off different options
  // easily at any time.
  bool ApplyOptions(const AudioOptions& options);
  void SetDefaultDevices();

  // webrtc::TraceCallback:
  void Print(webrtc::TraceLevel level, const char* trace, int length) override;

  void StartAecDump(const std::string& filename);
  int CreateVoEChannel();
  webrtc::AudioDeviceModule* adm();
  webrtc::AudioProcessing* apm();

  AudioCodecs CollectRecvCodecs() const;

  rtc::ThreadChecker signal_thread_checker_;
  rtc::ThreadChecker worker_thread_checker_;

  // The audio device manager.
  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm_;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory_;
  // Reference to the APM, owned by VoE.
  webrtc::AudioProcessing* apm_ = nullptr;
  // The primary instance of WebRtc VoiceEngine.
  std::unique_ptr<VoEWrapper> voe_wrapper_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::vector<AudioCodec> send_codecs_;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<WebRtcVoiceMediaChannel*> channels_;
  webrtc::VoEBase::ChannelConfig channel_config_;
  bool is_dumping_aec_ = false;

  webrtc::AgcConfig default_agc_config_;
  // Cache received extended_filter_aec, delay_agnostic_aec, experimental_ns
  // level controller, and intelligibility_enhancer values, and apply them
  // in case they are missing in the audio options. We need to do this because
  // SetExtraOptions() will revert to defaults for options which are not
  // provided.
  rtc::Optional<bool> extended_filter_aec_;
  rtc::Optional<bool> delay_agnostic_aec_;
  rtc::Optional<bool> experimental_ns_;
  rtc::Optional<bool> intelligibility_enhancer_;
  rtc::Optional<bool> level_control_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcVoiceEngine);
};

// WebRtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRtc Voice Engine.
class WebRtcVoiceMediaChannel final : public VoiceMediaChannel,
                                      public webrtc::Transport {
 public:
  WebRtcVoiceMediaChannel(WebRtcVoiceEngine* engine,
                          const MediaConfig& config,
                          const AudioOptions& options,
                          webrtc::Call* call);
  ~WebRtcVoiceMediaChannel() override;

  const AudioOptions& options() const { return options_; }

  rtc::DiffServCodePoint PreferredDscp() const override;

  bool SetSendParameters(const AudioSendParameters& params) override;
  bool SetRecvParameters(const AudioRecvParameters& params) override;
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override;
  bool SetRtpSendParameters(uint32_t ssrc,
                            const webrtc::RtpParameters& parameters) override;
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const override;
  bool SetRtpReceiveParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) override;

  void SetPlayout(bool playout) override;
  void SetSend(bool send) override;
  bool SetAudioSend(uint32_t ssrc,
                    bool enable,
                    const AudioOptions* options,
                    AudioSource* source) override;
  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32_t ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool RemoveRecvStream(uint32_t ssrc) override;
  bool GetActiveStreams(AudioInfo::StreamList* actives) override;
  int GetOutputLevel() override;
  int GetTimeSinceLastTyping() override;
  void SetTypingDetectionParameters(int time_window,
                                    int cost_per_typing,
                                    int reporting_threshold,
                                    int penalty_decay,
                                    int type_event_delay) override;
  bool SetOutputVolume(uint32_t ssrc, double volume) override;

  bool CanInsertDtmf() override;
  bool InsertDtmf(uint32_t ssrc, int event, int duration) override;

  void OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;
  void OnReadyToSend(bool ready) override;
  void OnTransportOverheadChanged(int transport_overhead_per_packet) override;
  bool GetStats(VoiceMediaInfo* info) override;

  void SetRawAudioSink(
      uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink) override;

  // implements Transport interface
  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override {
    rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
    rtc::PacketOptions rtc_options;
    rtc_options.packet_id = options.packet_id;
    return VoiceMediaChannel::SendPacket(&packet, rtc_options);
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
    return VoiceMediaChannel::SendRtcp(&packet, rtc::PacketOptions());
  }

  int GetReceiveChannelId(uint32_t ssrc) const;
  int GetSendChannelId(uint32_t ssrc) const;

 private:
  bool SetOptions(const AudioOptions& options);
  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs);
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs);
  bool SetLocalSource(uint32_t ssrc, AudioSource* source);
  bool MuteStream(uint32_t ssrc, bool mute);

  WebRtcVoiceEngine* engine() { return engine_; }
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  int GetOutputLevel(int channel);
  void ChangePlayout(bool playout);
  int CreateVoEChannel();
  bool DeleteVoEChannel(int channel);
  bool IsDefaultRecvStream(uint32_t ssrc) {
    return default_recv_ssrc_ == static_cast<int64_t>(ssrc);
  }
  bool SetMaxSendBitrate(int bps);
  bool ValidateRtpParameters(const webrtc::RtpParameters& parameters);
  void SetupRecording();

  rtc::ThreadChecker worker_thread_checker_;

  WebRtcVoiceEngine* const engine_ = nullptr;
  std::vector<AudioCodec> send_codecs_;
  std::vector<AudioCodec> recv_codecs_;
  int max_send_bitrate_bps_ = 0;
  AudioOptions options_;
  rtc::Optional<int> dtmf_payload_type_;
  bool desired_playout_ = false;
  bool recv_transport_cc_enabled_ = false;
  bool recv_nack_enabled_ = false;
  bool playout_ = false;
  bool send_ = false;
  webrtc::Call* const call_ = nullptr;

  // SSRC of unsignalled receive stream, or -1 if there isn't one.
  int64_t default_recv_ssrc_ = -1;
  // Volume for unsignalled stream, which may be set before the stream exists.
  double default_recv_volume_ = 1.0;
  // Sink for unsignalled stream, which may be set before the stream exists.
  std::unique_ptr<webrtc::AudioSinkInterface> default_sink_;
  // Default SSRC to use for RTCP receiver reports in case of no signaled
  // send streams. See: https://code.google.com/p/webrtc/issues/detail?id=4740
  // and https://code.google.com/p/chromium/issues/detail?id=547661
  uint32_t receiver_reports_ssrc_ = 0xFA17FA17u;

  class WebRtcAudioSendStream;
  std::map<uint32_t, WebRtcAudioSendStream*> send_streams_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  class WebRtcAudioReceiveStream;
  std::map<uint32_t, WebRtcAudioReceiveStream*> recv_streams_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;

  webrtc::AudioSendStream::Config::SendCodecSpec send_codec_spec_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcVoiceMediaChannel);
};
}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVOICEENGINE_H_
