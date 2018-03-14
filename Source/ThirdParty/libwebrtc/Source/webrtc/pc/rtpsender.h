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
#include "rtc_base/basictypes.h"
#include "rtc_base/criticalsection.h"
// Adding 'nogncheck' to disable the gn include headers check to support modular
// WebRTC build targets.
#include "media/base/audiosource.h"  // nogncheck
#include "pc/dtmfsender.h"
#include "pc/statscollector.h"

namespace webrtc {

// Internal interface used by PeerConnection.
class RtpSenderInternal : public RtpSenderInterface {
 public:
  // Used to set the SSRC of the sender, once a local description has been set.
  // If |ssrc| is 0, this indiates that the sender should disconnect from the
  // underlying transport (this occurs if the sender isn't seen in a local
  // description).
  virtual void SetSsrc(uint32_t ssrc) = 0;

  // TODO(steveanton): With Unified Plan, a track/RTCRTPSender can be part of
  // multiple streams (or no stream at all). Replace these singular methods with
  // their corresponding plural methods.
  // Until these are removed, RtpSenders must have exactly one stream.
  virtual void set_stream_id(const std::string& stream_id) = 0;
  virtual std::string stream_id() const = 0;
  virtual void set_stream_ids(const std::vector<std::string>& stream_ids) = 0;

  virtual void Stop() = 0;
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

  // Construct an AudioRtpSender with a null track, a single, randomly generated
  // stream label, and a randomly generated ID.
  AudioRtpSender(rtc::Thread* worker_thread, StatsCollector* stats);

  // Construct an AudioRtpSender with the given track and stream labels. The
  // sender ID will be set to the track's ID.
  AudioRtpSender(rtc::Thread* worker_thread,
                 rtc::scoped_refptr<AudioTrackInterface> track,
                 const std::vector<std::string>& stream_labels,
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

  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;

  // RtpSenderInternal implementation.
  void SetSsrc(uint32_t ssrc) override;

  void set_stream_id(const std::string& stream_id) override {
    stream_ids_ = {stream_id};
  }
  std::string stream_id() const override { return stream_ids_[0]; }
  void set_stream_ids(const std::vector<std::string>& stream_ids) override {
    stream_ids_ = stream_ids;
  }

  void Stop() override;

  int AttachmentId() const override { return attachment_id_; }

  // Does not take ownership.
  // Should call SetMediaChannel(nullptr) before |media_channel| is destroyed.
  void SetMediaChannel(cricket::VoiceMediaChannel* media_channel) {
    media_channel_ = media_channel;
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
  // TODO(steveanton): Until more Unified Plan work is done, this can only have
  // exactly one element.
  std::vector<std::string> stream_ids_;
  cricket::VoiceMediaChannel* media_channel_ = nullptr;
  StatsCollector* stats_;
  rtc::scoped_refptr<AudioTrackInterface> track_;
  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender_proxy_;
  uint32_t ssrc_ = 0;
  bool cached_track_enabled_ = false;
  bool stopped_ = false;

  // Used to pass the data callback from the |track_| to the other end of
  // cricket::AudioSource.
  std::unique_ptr<LocalAudioSinkAdapter> sink_adapter_;
  int attachment_id_ = 0;
};

class VideoRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInternal> {
 public:
  // Construct a VideoRtpSender with a null track, a single, randomly generated
  // stream label, and a randomly generated ID.
  explicit VideoRtpSender(rtc::Thread* worker_thread);

  // Construct a VideoRtpSender with the given track and stream labels. The
  // sender ID will be set to the track's ID.
  VideoRtpSender(rtc::Thread* worker_thread,
                 rtc::scoped_refptr<VideoTrackInterface> track,
                 const std::vector<std::string>& stream_labels);

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

  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;

  // RtpSenderInternal implementation.
  void SetSsrc(uint32_t ssrc) override;

  void set_stream_id(const std::string& stream_id) override {
    stream_ids_ = {stream_id};
  }
  std::string stream_id() const override { return stream_ids_[0]; }
  void set_stream_ids(const std::vector<std::string>& stream_ids) override {
    stream_ids_ = stream_ids;
  }

  void Stop() override;
  int AttachmentId() const override { return attachment_id_; }

  // Does not take ownership.
  // Should call SetMediaChannel(nullptr) before |media_channel| is destroyed.
  void SetMediaChannel(cricket::VideoMediaChannel* media_channel) {
    media_channel_ = media_channel;
  }

 private:
  bool can_send_track() const { return track_ && ssrc_; }
  // Helper function to construct options for
  // VideoProviderInterface::SetVideoSend.
  void SetVideoSend();
  // Helper function to call SetVideoSend with "stop sending" parameters.
  void ClearVideoSend();

  rtc::Thread* worker_thread_;
  const std::string id_;
  // TODO(steveanton): Until more Unified Plan work is done, this can only have
  // exactly one element.
  std::vector<std::string> stream_ids_;
  cricket::VideoMediaChannel* media_channel_ = nullptr;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  uint32_t ssrc_ = 0;
  bool cached_track_enabled_ = false;
  VideoTrackInterface::ContentHint cached_track_content_hint_ =
      VideoTrackInterface::ContentHint::kNone;
  bool stopped_ = false;
  int attachment_id_ = 0;
};

}  // namespace webrtc

#endif  // PC_RTPSENDER_H_
