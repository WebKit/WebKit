/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpSenderInterface.
// An RtpSender associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef PC_RTPSENDER_H_
#define PC_RTPSENDER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/mediastreaminterface.h"
#include "api/rtpsenderinterface.h"
#include "media/base/audiosource.h"
#include "media/base/mediachannel.h"
#include "pc/dtmfsender.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class StatsCollector;

bool UnimplementedRtpParameterHasValue(const RtpParameters& parameters);

// Internal interface used by PeerConnection.
class RtpSenderInternal : public RtpSenderInterface {
 public:
  // Sets the underlying MediaEngine channel associated with this RtpSender.
  // SetVoiceMediaChannel should be used for audio RtpSenders and
  // SetVideoMediaChannel should be used for video RtpSenders. Must call the
  // appropriate SetXxxMediaChannel(nullptr) before the media channel is
  // destroyed.
  virtual void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) = 0;
  virtual void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) = 0;

  // Used to set the SSRC of the sender, once a local description has been set.
  // If |ssrc| is 0, this indiates that the sender should disconnect from the
  // underlying transport (this occurs if the sender isn't seen in a local
  // description).
  virtual void SetSsrc(uint32_t ssrc) = 0;

  virtual void set_stream_ids(const std::vector<std::string>& stream_ids) = 0;
  virtual void set_init_send_encodings(
      const std::vector<RtpEncodingParameters>& init_send_encodings) = 0;

  virtual void Stop() = 0;

  // Returns an ID that changes every time SetTrack() is called, but
  // otherwise remains constant. Used to generate IDs for stats.
  // The special value zero means that no track is attached.
  virtual int AttachmentId() const = 0;
};

// LocalAudioSinkAdapter receives data callback as a sink to the local
// AudioTrack, and passes the data to the sink of AudioSource.
class LocalAudioSinkAdapter : public AudioTrackSinkInterface,
                              public cricket::AudioSource {
 public:
  LocalAudioSinkAdapter();
  virtual ~LocalAudioSinkAdapter();

 private:
  // AudioSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

  // cricket::AudioSource implementation.
  void SetSink(cricket::AudioSource::Sink* sink) override;

  cricket::AudioSource::Sink* sink_;
  // Critical section protecting |sink_|.
  rtc::CriticalSection lock_;
};

class AudioRtpSender : public DtmfProviderInterface,
                       public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInternal> {
 public:
  // StatsCollector provided so that Add/RemoveLocalAudioTrack can be called
  // at the appropriate times.

  // Construct an RtpSender for audio with the given sender ID.
  // The sender is initialized with no track to send and no associated streams.
  AudioRtpSender(rtc::Thread* worker_thread,
                 const std::string& id,
                 StatsCollector* stats);

  virtual ~AudioRtpSender();

  // DtmfSenderProvider implementation.
  bool CanInsertDtmf() override;
  bool InsertDtmf(int code, int duration) override;
  sigslot::signal0<>* GetOnDestroyedSignal() override;

  // ObserverInterface implementation.
  void OnChanged() override;

  // RtpSenderInterface implementation.
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_;
  }

  uint32_t ssrc() const override { return ssrc_; }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }

  std::string id() const override { return id_; }

  std::vector<std::string> stream_ids() const override { return stream_ids_; }

  RtpParameters GetParameters() override;
  RTCError SetParameters(const RtpParameters& parameters) override;

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;

  void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) override;

  rtc::scoped_refptr<FrameEncryptorInterface> GetFrameEncryptor()
      const override;

  // RtpSenderInternal implementation.
  void SetSsrc(uint32_t ssrc) override;

  void set_stream_ids(const std::vector<std::string>& stream_ids) override {
    stream_ids_ = stream_ids;
  }

  void set_init_send_encodings(
      const std::vector<RtpEncodingParameters>& init_send_encodings) override {
    init_parameters_.encodings = init_send_encodings;
  }
  std::vector<RtpEncodingParameters> init_send_encodings() const override {
    return init_parameters_.encodings;
  }

  void Stop() override;

  int AttachmentId() const override { return attachment_id_; }

  void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) override;

  void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) override {
    RTC_NOTREACHED();
  }

 private:
  // TODO(nisse): Since SSRC == 0 is technically valid, figure out
  // some other way to test if we have a valid SSRC.
  bool can_send_track() const { return track_ && ssrc_; }
  // Helper function to construct options for
  // AudioProviderInterface::SetAudioSend.
  void SetAudioSend();
  // Helper function to call SetAudioSend with "stop sending" parameters.
  void ClearAudioSend();

  sigslot::signal0<> SignalDestroyed;

  rtc::Thread* const worker_thread_;
  const std::string id_;
  std::vector<std::string> stream_ids_;
  RtpParameters init_parameters_;
  cricket::VoiceMediaChannel* media_channel_ = nullptr;
  StatsCollector* stats_ = nullptr;
  rtc::scoped_refptr<AudioTrackInterface> track_;
  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender_proxy_;
  absl::optional<std::string> last_transaction_id_;
  uint32_t ssrc_ = 0;
  bool cached_track_enabled_ = false;
  bool stopped_ = false;

  // Used to pass the data callback from the |track_| to the other end of
  // cricket::AudioSource.
  std::unique_ptr<LocalAudioSinkAdapter> sink_adapter_;
  int attachment_id_ = 0;
  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_;
};

class VideoRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInternal> {
 public:
  // Construct an RtpSender for video with the given sender ID.
  // The sender is initialized with no track to send and no associated streams.
  VideoRtpSender(rtc::Thread* worker_thread, const std::string& id);

  virtual ~VideoRtpSender();

  // ObserverInterface implementation
  void OnChanged() override;

  // RtpSenderInterface implementation
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_;
  }

  uint32_t ssrc() const override { return ssrc_; }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

  std::string id() const override { return id_; }

  std::vector<std::string> stream_ids() const override { return stream_ids_; }

  void set_init_send_encodings(
      const std::vector<RtpEncodingParameters>& init_send_encodings) override {
    init_parameters_.encodings = init_send_encodings;
  }
  std::vector<RtpEncodingParameters> init_send_encodings() const override {
    return init_parameters_.encodings;
  }

  RtpParameters GetParameters() override;
  RTCError SetParameters(const RtpParameters& parameters) override;

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;

  void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) override;

  rtc::scoped_refptr<FrameEncryptorInterface> GetFrameEncryptor()
      const override;

  // RtpSenderInternal implementation.
  void SetSsrc(uint32_t ssrc) override;

  void set_stream_ids(const std::vector<std::string>& stream_ids) override {
    stream_ids_ = stream_ids;
  }

  void Stop() override;
  int AttachmentId() const override { return attachment_id_; }

  void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) override {
    RTC_NOTREACHED();
  }

  void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) override;

 private:
  bool can_send_track() const { return track_ && ssrc_; }
  // Helper function to construct options for
  // VideoProviderInterface::SetVideoSend.
  void SetVideoSend();
  // Helper function to call SetVideoSend with "stop sending" parameters.
  void ClearVideoSend();

  rtc::Thread* worker_thread_;
  const std::string id_;
  std::vector<std::string> stream_ids_;
  RtpParameters init_parameters_;
  cricket::VideoMediaChannel* media_channel_ = nullptr;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  absl::optional<std::string> last_transaction_id_;
  uint32_t ssrc_ = 0;
  VideoTrackInterface::ContentHint cached_track_content_hint_ =
      VideoTrackInterface::ContentHint::kNone;
  bool stopped_ = false;
  int attachment_id_ = 0;
  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_;
};

}  // namespace webrtc

#endif  // PC_RTPSENDER_H_
