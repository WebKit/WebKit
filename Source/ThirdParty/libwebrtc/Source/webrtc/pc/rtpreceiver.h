/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpReceiverInterface.
// An RtpReceiver associates a MediaStreamTrackInterface with an underlying
// transport (provided by cricket::VoiceChannel/cricket::VideoChannel)

#ifndef PC_RTPRECEIVER_H_
#define PC_RTPRECEIVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "api/mediastreaminterface.h"
#include "api/rtpreceiverinterface.h"
#include "media/base/videobroadcaster.h"
#include "pc/remoteaudiosource.h"
#include "pc/videotracksource.h"

namespace webrtc {

// Internal class used by PeerConnection.
class RtpReceiverInternal : public RtpReceiverInterface {
 public:
  virtual void Stop() = 0;

  // Sets the underlying MediaEngine channel associated with this RtpSender.
  // SetVoiceMediaChannel should be used for audio RtpSenders and
  // SetVideoMediaChannel should be used for video RtpSenders. Must call the
  // appropriate SetXxxMediaChannel(nullptr) before the media channel is
  // destroyed.
  virtual void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) = 0;
  virtual void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) = 0;

  // Configures the RtpReceiver with the underlying media channel, with the
  // given SSRC as the stream identifier. If |ssrc| is 0, the receiver will
  // receive packets on unsignaled SSRCs.
  virtual void SetupMediaChannel(uint32_t ssrc) = 0;

  // This SSRC is used as an identifier for the receiver between the API layer
  // and the WebRtcVideoEngine, WebRtcVoiceEngine layer.
  virtual uint32_t ssrc() const = 0;

  // Call this to notify the RtpReceiver when the first packet has been received
  // on the corresponding channel.
  virtual void NotifyFirstPacketReceived() = 0;

  // Set the associated remote media streams for this receiver. The remote track
  // will be removed from any streams that are no longer present and added to
  // any new streams.
  virtual void set_stream_ids(std::vector<std::string> stream_ids) = 0;
  // TODO(https://crbug.com/webrtc/9480): Remove SetStreams() in favor of
  // set_stream_ids() as soon as downstream projects are no longer dependent on
  // stream objects.
  virtual void SetStreams(
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) = 0;

  // Returns an ID that changes if the attached track changes, but
  // otherwise remains constant. Used to generate IDs for stats.
  // The special value zero means that no track is attached.
  virtual int AttachmentId() const = 0;
};

class AudioRtpReceiver : public ObserverInterface,
                         public AudioSourceInterface::AudioObserver,
                         public rtc::RefCountedObject<RtpReceiverInternal> {
 public:
  AudioRtpReceiver(rtc::Thread* worker_thread,
                   std::string receiver_id,
                   std::vector<std::string> stream_ids);
  // TODO(https://crbug.com/webrtc/9480): Remove this when streams() is removed.
  AudioRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& receiver_id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams);
  virtual ~AudioRtpReceiver();

  // ObserverInterface implementation
  void OnChanged() override;

  // AudioSourceInterface::AudioObserver implementation
  void OnSetVolume(double volume) override;

  rtc::scoped_refptr<AudioTrackInterface> audio_track() const {
    return track_.get();
  }

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }
  std::vector<std::string> stream_ids() const override;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams()
      const override {
    return streams_;
  }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }

  std::string id() const override { return id_; }

  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;

  void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) override;

  rtc::scoped_refptr<FrameDecryptorInterface> GetFrameDecryptor()
      const override;

  // RtpReceiverInternal implementation.
  void Stop() override;
  void SetupMediaChannel(uint32_t ssrc) override;
  uint32_t ssrc() const override { return ssrc_.value_or(0); }
  void NotifyFirstPacketReceived() override;
  void set_stream_ids(std::vector<std::string> stream_ids) override;
  void SetStreams(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;
  void SetObserver(RtpReceiverObserverInterface* observer) override;
  void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) override;

  void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) override {
    RTC_NOTREACHED();
  }

  std::vector<RtpSource> GetSources() const override;
  int AttachmentId() const override { return attachment_id_; }

 private:
  void Reconfigure();
  bool SetOutputVolume(double volume);

  rtc::Thread* const worker_thread_;
  const std::string id_;
  const rtc::scoped_refptr<RemoteAudioSource> source_;
  const rtc::scoped_refptr<AudioTrackInterface> track_;
  cricket::VoiceMediaChannel* media_channel_ = nullptr;
  absl::optional<uint32_t> ssrc_;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_;
  bool cached_track_enabled_;
  double cached_volume_ = 1;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
  int attachment_id_ = 0;
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
};

class VideoRtpReceiver : public rtc::RefCountedObject<RtpReceiverInternal> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  VideoRtpReceiver(rtc::Thread* worker_thread,
                   std::string receiver_id,
                   std::vector<std::string> streams_ids);
  // TODO(hbos): Remove this when streams() is removed.
  // https://crbug.com/webrtc/9480
  VideoRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& receiver_id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams);

  virtual ~VideoRtpReceiver();

  rtc::scoped_refptr<VideoTrackInterface> video_track() const {
    return track_.get();
  }

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }
  std::vector<std::string> stream_ids() const override;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams()
      const override {
    return streams_;
  }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

  std::string id() const override { return id_; }

  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;

  void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) override;

  rtc::scoped_refptr<FrameDecryptorInterface> GetFrameDecryptor()
      const override;

  // RtpReceiverInternal implementation.
  void Stop() override;
  void SetupMediaChannel(uint32_t ssrc) override;
  uint32_t ssrc() const override { return ssrc_.value_or(0); }
  void NotifyFirstPacketReceived() override;
  void set_stream_ids(std::vector<std::string> stream_ids) override;
  void SetStreams(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  void SetVoiceMediaChannel(
      cricket::VoiceMediaChannel* voice_media_channel) override {
    RTC_NOTREACHED();
  }

  void SetVideoMediaChannel(
      cricket::VideoMediaChannel* video_media_channel) override;

  int AttachmentId() const override { return attachment_id_; }

  std::vector<RtpSource> GetSources() const override;

 private:
  class VideoRtpTrackSource : public VideoTrackSource {
   public:
    VideoRtpTrackSource() : VideoTrackSource(true /* remote */) {}

    rtc::VideoSourceInterface<VideoFrame>* source() override {
      return &broadcaster_;
    }
    rtc::VideoSinkInterface<VideoFrame>* sink() { return &broadcaster_; }

   private:
    // |broadcaster_| is needed since the decoder can only handle one sink.
    // It might be better if the decoder can handle multiple sinks and consider
    // the VideoSinkWants.
    rtc::VideoBroadcaster broadcaster_;
  };

  bool SetSink(rtc::VideoSinkInterface<VideoFrame>* sink);

  rtc::Thread* const worker_thread_;
  const std::string id_;
  cricket::VideoMediaChannel* media_channel_ = nullptr;
  absl::optional<uint32_t> ssrc_;
  // |source_| is held here to be able to change the state of the source when
  // the VideoRtpReceiver is stopped.
  rtc::scoped_refptr<VideoRtpTrackSource> source_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
  int attachment_id_ = 0;
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
};

}  // namespace webrtc

#endif  // PC_RTPRECEIVER_H_
