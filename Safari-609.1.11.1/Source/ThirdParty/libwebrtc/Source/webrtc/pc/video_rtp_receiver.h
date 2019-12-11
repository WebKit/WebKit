/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEO_RTP_RECEIVER_H_
#define PC_VIDEO_RTP_RECEIVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/media_channel.h"
#include "media/base/video_broadcaster.h"
#include "pc/jitter_buffer_delay_interface.h"
#include "pc/rtp_receiver.h"
#include "pc/video_track_source.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

namespace webrtc {

class VideoRtpReceiver : public rtc::RefCountedObject<RtpReceiverInternal> {
 public:
  // An SSRC of 0 will create a receiver that will match the first SSRC it
  // sees. Must be called on signaling thread.
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
  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const override {
    return dtls_transport_;
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
  void set_transport(
      rtc::scoped_refptr<DtlsTransportInterface> dtls_transport) override {
    dtls_transport_ = dtls_transport;
  }
  void SetStreams(const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetObserver(RtpReceiverObserverInterface* observer) override;

  void SetJitterBufferMinimumDelay(
      absl::optional<double> delay_seconds) override;

  void SetMediaChannel(cricket::MediaChannel* media_channel) override;

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
  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport_;
  // Allows to thread safely change jitter buffer delay. Handles caching cases
  // if |SetJitterBufferMinimumDelay| is called before start.
  rtc::scoped_refptr<JitterBufferDelayInterface> delay_;
};

}  // namespace webrtc

#endif  // PC_VIDEO_RTP_RECEIVER_H_
