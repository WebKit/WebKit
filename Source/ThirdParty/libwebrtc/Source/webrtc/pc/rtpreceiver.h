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
#include "rtc_base/basictypes.h"

namespace webrtc {

// Internal class used by PeerConnection.
class RtpReceiverInternal : public RtpReceiverInterface {
 public:
  virtual void Stop() = 0;

  // This SSRC is used as an identifier for the receiver between the API layer
  // and the WebRtcVideoEngine, WebRtcVoiceEngine layer.
  virtual uint32_t ssrc() const = 0;

  // Call this to notify the RtpReceiver when the first packet has been received
  // on the corresponding channel.
  virtual void NotifyFirstPacketReceived() = 0;

  // Set the associated remote media streams for this receiver. The remote track
  // will be removed from any streams that are no longer present and added to
  // any new streams.
  virtual void SetStreams(
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) = 0;
};

class AudioRtpReceiver : public ObserverInterface,
                         public AudioSourceInterface::AudioObserver,
                         public rtc::RefCountedObject<RtpReceiverInternal> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  // TODO(deadbeef): Use rtc::Optional, or have another constructor that
  // doesn't take an SSRC, and make this one DCHECK(ssrc != 0).
  AudioRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& receiver_id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
      uint32_t ssrc,
      cricket::VoiceMediaChannel* media_channel);
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

  // RtpReceiverInternal implementation.
  void Stop() override;
  uint32_t ssrc() const override { return ssrc_; }
  void NotifyFirstPacketReceived() override;
  void SetStreams(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  // Does not take ownership.
  // Should call SetMediaChannel(nullptr) before |media_channel| is destroyed.
  void SetMediaChannel(cricket::VoiceMediaChannel* media_channel);

  std::vector<RtpSource> GetSources() const override;
  int AttachmentId() const override { return attachment_id_; }

 private:
  void Reconfigure();
  bool SetOutputVolume(double volume);

  rtc::Thread* const worker_thread_;
  const std::string id_;
  const uint32_t ssrc_;
  cricket::VoiceMediaChannel* media_channel_ = nullptr;
  const rtc::scoped_refptr<AudioTrackInterface> track_;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_;
  bool cached_track_enabled_;
  double cached_volume_ = 1;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
  int attachment_id_ = 0;
};

class VideoRtpReceiver : public rtc::RefCountedObject<RtpReceiverInternal> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  VideoRtpReceiver(
      rtc::Thread* worker_thread,
      const std::string& receiver_id,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
      uint32_t ssrc,
      cricket::VideoMediaChannel* media_channel);

  virtual ~VideoRtpReceiver();

  rtc::scoped_refptr<VideoTrackInterface> video_track() const {
    return track_.get();
  }

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }
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

  // RtpReceiverInternal implementation.
  void Stop() override;
  uint32_t ssrc() const override { return ssrc_; }
  void NotifyFirstPacketReceived() override;
  void SetStreams(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  // Does not take ownership.
  // Should call SetMediaChannel(nullptr) before |media_channel| is destroyed.
  void SetMediaChannel(cricket::VideoMediaChannel* media_channel);

 private:
  bool SetSink(rtc::VideoSinkInterface<VideoFrame>* sink);

  rtc::Thread* const worker_thread_;
  const std::string id_;
  uint32_t ssrc_;
  cricket::VideoMediaChannel* media_channel_ = nullptr;
  // |broadcaster_| is needed since the decoder can only handle one sink.
  // It might be better if the decoder can handle multiple sinks and consider
  // the VideoSinkWants.
  rtc::VideoBroadcaster broadcaster_;
  // |source_| is held here to be able to change the state of the source when
  // the VideoRtpReceiver is stopped.
  rtc::scoped_refptr<VideoTrackSource> source_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
  int attachment_id_ = 0;
};

}  // namespace webrtc

#endif  // PC_RTPRECEIVER_H_
