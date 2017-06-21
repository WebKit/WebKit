/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_ORTC_RTPTRANSPORTCONTROLLERADAPTER_H_
#define WEBRTC_ORTC_RTPTRANSPORTCONTROLLERADAPTER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "webrtc/api/ortc/ortcrtpreceiverinterface.h"
#include "webrtc/api/ortc/ortcrtpsenderinterface.h"
#include "webrtc/api/ortc/rtptransportcontrollerinterface.h"
#include "webrtc/api/ortc/srtptransportinterface.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/mediachannel.h"  // For MediaConfig.
#include "webrtc/pc/channelmanager.h"

namespace webrtc {

class RtpTransportAdapter;
class OrtcRtpSenderAdapter;
class OrtcRtpReceiverAdapter;

// Implementation of RtpTransportControllerInterface. Wraps a Call,
// a VoiceChannel and VideoChannel, and maintains a list of dependent RTP
// transports.
//
// When used along with an RtpSenderAdapter or RtpReceiverAdapter, the
// sender/receiver passes its parameters along to this class, which turns them
// into cricket:: media descriptions (the interface used by BaseChannel).
//
// Due to the fact that BaseChannel has different subclasses for audio/video,
// the actual BaseChannel object is not created until an RtpSender/RtpReceiver
// needs them.
//
// All methods should be called on the signaling thread.
//
// TODO(deadbeef): When BaseChannel is split apart into separate
// "RtpSender"/"RtpTransceiver"/"RtpSender"/"RtpReceiver" objects, this adapter
// object can be replaced by a "real" one.
class RtpTransportControllerAdapter : public RtpTransportControllerInterface,
                                      public sigslot::has_slots<> {
 public:
  // Creates a proxy that will call "public interface" methods on the correct
  // thread.
  //
  // Doesn't take ownership of any objects passed in.
  //
  // |channel_manager| must not be null.
  static std::unique_ptr<RtpTransportControllerInterface> CreateProxied(
      const cricket::MediaConfig& config,
      cricket::ChannelManager* channel_manager,
      webrtc::RtcEventLog* event_log,
      rtc::Thread* signaling_thread,
      rtc::Thread* worker_thread);

  ~RtpTransportControllerAdapter() override;

  // RtpTransportControllerInterface implementation.
  std::vector<RtpTransportInterface*> GetTransports() const override;

  // These methods are used by OrtcFactory to create RtpTransports, RtpSenders
  // and RtpReceivers using this controller. Called "CreateProxied" because
  // these methods return proxies that will safely call methods on the correct
  // thread.
  RTCErrorOr<std::unique_ptr<RtpTransportInterface>> CreateProxiedRtpTransport(
      const RtcpParameters& rtcp_parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp);

  RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>
  CreateProxiedSrtpTransport(const RtcpParameters& rtcp_parameters,
                             PacketTransportInterface* rtp,
                             PacketTransportInterface* rtcp);

  // |transport_proxy| needs to be a proxy to a transport because the
  // application may call GetTransport() on the returned sender or receiver,
  // and expects it to return a thread-safe transport proxy.
  RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>> CreateProxiedRtpSender(
      cricket::MediaType kind,
      RtpTransportInterface* transport_proxy);
  RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>>
  CreateProxiedRtpReceiver(cricket::MediaType kind,
                           RtpTransportInterface* transport_proxy);

  // Methods used internally by other "adapter" classes.
  rtc::Thread* signaling_thread() const { return signaling_thread_; }
  rtc::Thread* worker_thread() const { return worker_thread_; }

  RTCError SetRtcpParameters(const RtcpParameters& parameters,
                             RtpTransportInterface* inner_transport);

  cricket::VoiceChannel* voice_channel() { return voice_channel_; }
  cricket::VideoChannel* video_channel() { return video_channel_; }

  // |primary_ssrc| out parameter is filled with either
  // |parameters.encodings[0].ssrc|, or a generated SSRC if that's left unset.
  RTCError ValidateAndApplyAudioSenderParameters(
      const RtpParameters& parameters,
      uint32_t* primary_ssrc);
  RTCError ValidateAndApplyVideoSenderParameters(
      const RtpParameters& parameters,
      uint32_t* primary_ssrc);
  RTCError ValidateAndApplyAudioReceiverParameters(
      const RtpParameters& parameters);
  RTCError ValidateAndApplyVideoReceiverParameters(
      const RtpParameters& parameters);

 protected:
  RtpTransportControllerAdapter* GetInternal() override { return this; }

 private:
  // Only expected to be called by RtpTransportControllerAdapter::CreateProxied.
  RtpTransportControllerAdapter(const cricket::MediaConfig& config,
                                cricket::ChannelManager* channel_manager,
                                webrtc::RtcEventLog* event_log,
                                rtc::Thread* signaling_thread,
                                rtc::Thread* worker_thread);
  void Init_w();
  void Close_w();

  // These return an error if another of the same type of object is already
  // attached, or if |transport_proxy| can't be used with the sender/receiver
  // due to the limitation that the sender/receiver of the same media type must
  // use the same transport.
  RTCError AttachAudioSender(OrtcRtpSenderAdapter* sender,
                             RtpTransportInterface* inner_transport);
  RTCError AttachVideoSender(OrtcRtpSenderAdapter* sender,
                             RtpTransportInterface* inner_transport);
  RTCError AttachAudioReceiver(OrtcRtpReceiverAdapter* receiver,
                               RtpTransportInterface* inner_transport);
  RTCError AttachVideoReceiver(OrtcRtpReceiverAdapter* receiver,
                               RtpTransportInterface* inner_transport);

  void OnRtpTransportDestroyed(RtpTransportAdapter* transport);

  void OnAudioSenderDestroyed();
  void OnVideoSenderDestroyed();
  void OnAudioReceiverDestroyed();
  void OnVideoReceiverDestroyed();

  void CreateVoiceChannel();
  void CreateVideoChannel();
  void DestroyVoiceChannel();
  void DestroyVideoChannel();

  void CopyRtcpParametersToDescriptions(
      const RtcpParameters& params,
      cricket::MediaContentDescription* local,
      cricket::MediaContentDescription* remote);

  // Helper function to generate an SSRC that doesn't match one in any of the
  // "content description" structs, or in |new_ssrcs| (which is needed since
  // multiple SSRCs may be generated in one go).
  uint32_t GenerateUnusedSsrc(std::set<uint32_t>* new_ssrcs) const;

  // |description| is the matching description where existing SSRCs can be
  // found.
  //
  // This is a member function because it may need to generate SSRCs that don't
  // match existing ones, which is more than ToStreamParamsVec does.
  RTCErrorOr<cricket::StreamParamsVec> MakeSendStreamParamsVec(
      std::vector<RtpEncodingParameters> encodings,
      const std::string& cname,
      const cricket::MediaContentDescription& description) const;

  // If the |rtp_transport| is a SrtpTransport, set the cryptos of the
  // audio/video content descriptions.
  RTCError MaybeSetCryptos(
      RtpTransportInterface* rtp_transport,
      cricket::MediaContentDescription* local_description,
      cricket::MediaContentDescription* remote_description);

  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  // |transport_proxies_| and |inner_audio_transport_|/|inner_audio_transport_|
  // are somewhat redundant, but the latter are only set when
  // RtpSenders/RtpReceivers are attached to the transport.
  std::vector<RtpTransportInterface*> transport_proxies_;
  RtpTransportInterface* inner_audio_transport_ = nullptr;
  RtpTransportInterface* inner_video_transport_ = nullptr;
  const cricket::MediaConfig media_config_;
  cricket::ChannelManager* channel_manager_;
  webrtc::RtcEventLog* event_log_;
  std::unique_ptr<Call> call_;

  // BaseChannel takes content descriptions as input, so we store them here
  // such that they can be updated when a new RtpSenderAdapter/
  // RtpReceiverAdapter attaches itself.
  cricket::AudioContentDescription local_audio_description_;
  cricket::AudioContentDescription remote_audio_description_;
  cricket::VideoContentDescription local_video_description_;
  cricket::VideoContentDescription remote_video_description_;
  cricket::VoiceChannel* voice_channel_ = nullptr;
  cricket::VideoChannel* video_channel_ = nullptr;
  bool have_audio_sender_ = false;
  bool have_video_sender_ = false;
  bool have_audio_receiver_ = false;
  bool have_video_receiver_ = false;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtpTransportControllerAdapter);
};

}  // namespace webrtc

#endif  // WEBRTC_ORTC_RTPTRANSPORTCONTROLLERADAPTER_H_
