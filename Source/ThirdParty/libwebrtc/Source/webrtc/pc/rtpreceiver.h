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

#ifndef WEBRTC_PC_RTPRECEIVER_H_
#define WEBRTC_PC_RTPRECEIVER_H_

#include <stdint.h>

#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/rtpreceiverinterface.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/videobroadcaster.h"
#include "webrtc/pc/channel.h"
#include "webrtc/pc/remoteaudiosource.h"
#include "webrtc/pc/videotracksource.h"

namespace webrtc {

// Internal class used by PeerConnection.
class RtpReceiverInternal : public RtpReceiverInterface {
 public:
  virtual void Stop() = 0;
  // This SSRC is used as an identifier for the receiver between the API layer
  // and the WebRtcVideoEngine2, WebRtcVoiceEngine layer.
  virtual uint32_t ssrc() const = 0;
};

class AudioRtpReceiver : public ObserverInterface,
                         public AudioSourceInterface::AudioObserver,
                         public rtc::RefCountedObject<RtpReceiverInternal>,
                         public sigslot::has_slots<> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  // TODO(deadbeef): Use rtc::Optional, or have another constructor that
  // doesn't take an SSRC, and make this one DCHECK(ssrc != 0).
  AudioRtpReceiver(const std::string& track_id,
                   uint32_t ssrc,
                   cricket::VoiceChannel* channel);

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

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }

  std::string id() const override { return id_; }

  RtpParameters GetParameters() const override;
  bool SetParameters(const RtpParameters& parameters) override;

  // RtpReceiverInternal implementation.
  void Stop() override;
  uint32_t ssrc() const override { return ssrc_; }

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  // Does not take ownership.
  // Should call SetChannel(nullptr) before |channel| is destroyed.
  void SetChannel(cricket::VoiceChannel* channel);

 private:
  void Reconfigure();
  void OnFirstPacketReceived(cricket::BaseChannel* channel);

  const std::string id_;
  const uint32_t ssrc_;
  cricket::VoiceChannel* channel_;
  const rtc::scoped_refptr<AudioTrackInterface> track_;
  bool cached_track_enabled_;
  double cached_volume_ = 1;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
};

class VideoRtpReceiver : public rtc::RefCountedObject<RtpReceiverInternal>,
                         public sigslot::has_slots<> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees.
  VideoRtpReceiver(const std::string& track_id,
                   rtc::Thread* worker_thread,
                   uint32_t ssrc,
                   cricket::VideoChannel* channel);

  virtual ~VideoRtpReceiver();

  rtc::scoped_refptr<VideoTrackInterface> video_track() const {
    return track_.get();
  }

  // RtpReceiverInterface implementation
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
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

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  // Does not take ownership.
  // Should call SetChannel(nullptr) before |channel| is destroyed.
  void SetChannel(cricket::VideoChannel* channel);

 private:
  void OnFirstPacketReceived(cricket::BaseChannel* channel);

  std::string id_;
  uint32_t ssrc_;
  cricket::VideoChannel* channel_;
  // |broadcaster_| is needed since the decoder can only handle one sink.
  // It might be better if the decoder can handle multiple sinks and consider
  // the VideoSinkWants.
  rtc::VideoBroadcaster broadcaster_;
  // |source_| is held here to be able to change the state of the source when
  // the VideoRtpReceiver is stopped.
  rtc::scoped_refptr<VideoTrackSource> source_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  bool stopped_ = false;
  RtpReceiverObserverInterface* observer_ = nullptr;
  bool received_first_packet_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_RTPRECEIVER_H_
