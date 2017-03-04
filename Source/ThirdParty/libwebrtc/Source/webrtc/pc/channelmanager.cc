/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/channelmanager.h"

#include <algorithm>

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/device.h"
#include "webrtc/media/base/rtpdataengine.h"
#include "webrtc/pc/srtpfilter.h"
#include "webrtc/pc/mediacontroller.h"

namespace cricket {


using rtc::Bind;

ChannelManager::ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                               std::unique_ptr<DataEngineInterface> dme,
                               rtc::Thread* thread) {
  Construct(std::move(me), std::move(dme), thread, thread);
}

ChannelManager::ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  Construct(std::move(me),
            std::unique_ptr<DataEngineInterface>(new RtpDataEngine()),
            worker_thread, network_thread);
}

void ChannelManager::Construct(std::unique_ptr<MediaEngineInterface> me,
                               std::unique_ptr<DataEngineInterface> dme,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  media_engine_ = std::move(me);
  data_media_engine_ = std::move(dme);
  initialized_ = false;
  main_thread_ = rtc::Thread::Current();
  worker_thread_ = worker_thread;
  network_thread_ = network_thread;
  capturing_ = false;
  enable_rtx_ = false;
  crypto_options_ = rtc::CryptoOptions::NoGcm();
}

ChannelManager::~ChannelManager() {
  if (initialized_) {
    Terminate();
    // If srtp is initialized (done by the Channel) then we must call
    // srtp_shutdown to free all crypto kernel lists. But we need to make sure
    // shutdown always called at the end, after channels are destroyed.
    // ChannelManager d'tor is always called last, it's safe place to call
    // shutdown.
    ShutdownSrtp();
  }
  // The media engine needs to be deleted on the worker thread for thread safe
  // destruction,
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE, Bind(&ChannelManager::DestructorDeletes_w, this));
}

bool ChannelManager::SetVideoRtxEnabled(bool enable) {
  // To be safe, this call is only allowed before initialization. Apps like
  // Flute only have a singleton ChannelManager and we don't want this flag to
  // be toggled between calls or when there's concurrent calls. We expect apps
  // to enable this at startup and retain that setting for the lifetime of the
  // app.
  if (!initialized_) {
    enable_rtx_ = enable;
    return true;
  } else {
    LOG(LS_WARNING) << "Cannot toggle rtx after initialization!";
    return false;
  }
}

bool ChannelManager::SetCryptoOptions(
    const rtc::CryptoOptions& crypto_options) {
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, Bind(
      &ChannelManager::SetCryptoOptions_w, this, crypto_options));
}

bool ChannelManager::SetCryptoOptions_w(
    const rtc::CryptoOptions& crypto_options) {
  if (!video_channels_.empty() || !voice_channels_.empty() ||
      !data_channels_.empty()) {
    LOG(LS_WARNING) << "Not changing crypto options in existing channels.";
  }
  crypto_options_ = crypto_options;
  return true;
}

void ChannelManager::GetSupportedAudioSendCodecs(
    std::vector<AudioCodec>* codecs) const {
  *codecs = media_engine_->audio_send_codecs();
}

void ChannelManager::GetSupportedAudioReceiveCodecs(
    std::vector<AudioCodec>* codecs) const {
  *codecs = media_engine_->audio_recv_codecs();
}

void ChannelManager::GetSupportedAudioRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  *ext = media_engine_->GetAudioCapabilities().header_extensions;
}

void ChannelManager::GetSupportedVideoCodecs(
    std::vector<VideoCodec>* codecs) const {
  codecs->clear();

  std::vector<VideoCodec> video_codecs = media_engine_->video_codecs();
  for (const auto& video_codec : video_codecs) {
    if (!enable_rtx_ &&
        _stricmp(kRtxCodecName, video_codec.name.c_str()) == 0) {
      continue;
    }
    codecs->push_back(video_codec);
  }
}

void ChannelManager::GetSupportedVideoRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  *ext = media_engine_->GetVideoCapabilities().header_extensions;
}

void ChannelManager::GetSupportedDataCodecs(
    std::vector<DataCodec>* codecs) const {
  *codecs = data_media_engine_->data_codecs();
}

bool ChannelManager::Init() {
  RTC_DCHECK(!initialized_);
  if (initialized_) {
    return false;
  }
  RTC_DCHECK(network_thread_);
  RTC_DCHECK(worker_thread_);
  if (!network_thread_->IsCurrent()) {
    // Do not allow invoking calls to other threads on the network thread.
    network_thread_->Invoke<bool>(
        RTC_FROM_HERE,
        rtc::Bind(&rtc::Thread::SetAllowBlockingCalls, network_thread_, false));
  }

  initialized_ = worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&ChannelManager::InitMediaEngine_w, this));
  RTC_DCHECK(initialized_);
  return initialized_;
}

bool ChannelManager::InitMediaEngine_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  return media_engine_->Init();
}

void ChannelManager::Terminate() {
  RTC_DCHECK(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(RTC_FROM_HERE,
                               Bind(&ChannelManager::Terminate_w, this));
  initialized_ = false;
}

void ChannelManager::DestructorDeletes_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  media_engine_.reset(NULL);
}

void ChannelManager::Terminate_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  // Need to destroy the voice/video channels
  while (!video_channels_.empty()) {
    DestroyVideoChannel_w(video_channels_.back());
  }
  while (!voice_channels_.empty()) {
    DestroyVoiceChannel_w(voice_channels_.back());
  }
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVoiceChannel_w, this, media_controller,
           rtp_transport, rtcp_transport, rtp_transport, rtcp_transport,
           signaling_thread, content_name, srtp_required, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    webrtc::MediaControllerInterface* media_controller,
    rtc::PacketTransportInternal* rtp_transport,
    rtc::PacketTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVoiceChannel_w, this, media_controller,
           nullptr, nullptr, rtp_transport, rtcp_transport, signaling_thread,
           content_name, srtp_required, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  RTC_DCHECK(nullptr != media_controller);

  VoiceMediaChannel* media_channel = media_engine_->CreateChannel(
      media_controller->call_w(), media_controller->config(), options);
  if (!media_channel)
    return nullptr;

  VoiceChannel* voice_channel =
      new VoiceChannel(worker_thread_, network_thread_, signaling_thread,
                       media_engine_.get(), media_channel, content_name,
                       rtcp_packet_transport == nullptr, srtp_required);
  voice_channel->SetCryptoOptions(crypto_options_);

  if (!voice_channel->Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                             rtp_packet_transport, rtcp_packet_transport)) {
    delete voice_channel;
    return nullptr;
  }
  voice_channels_.push_back(voice_channel);
  return voice_channel;
}

void ChannelManager::DestroyVoiceChannel(VoiceChannel* voice_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVoiceChannel");
  if (voice_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyVoiceChannel_w, this, voice_channel));
  }
}

void ChannelManager::DestroyVoiceChannel_w(VoiceChannel* voice_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVoiceChannel_w");
  // Destroy voice channel.
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  VoiceChannels::iterator it = std::find(voice_channels_.begin(),
      voice_channels_.end(), voice_channel);
  RTC_DCHECK(it != voice_channels_.end());
  if (it == voice_channels_.end())
    return;
  voice_channels_.erase(it);
  delete voice_channel;
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVideoChannel_w, this, media_controller,
           rtp_transport, rtcp_transport, rtp_transport, rtcp_transport,
           signaling_thread, content_name, srtp_required, options));
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::MediaControllerInterface* media_controller,
    rtc::PacketTransportInternal* rtp_transport,
    rtc::PacketTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVideoChannel_w, this, media_controller,
           nullptr, nullptr, rtp_transport, rtcp_transport, signaling_thread,
           content_name, srtp_required, options));
}

VideoChannel* ChannelManager::CreateVideoChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  RTC_DCHECK(nullptr != media_controller);
  VideoMediaChannel* media_channel = media_engine_->CreateVideoChannel(
      media_controller->call_w(), media_controller->config(), options);
  if (media_channel == NULL) {
    return NULL;
  }

  VideoChannel* video_channel = new VideoChannel(
      worker_thread_, network_thread_, signaling_thread, media_channel,
      content_name, rtcp_packet_transport == nullptr, srtp_required);
  video_channel->SetCryptoOptions(crypto_options_);
  if (!video_channel->Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                             rtp_packet_transport, rtcp_packet_transport)) {
    delete video_channel;
    return NULL;
  }
  video_channels_.push_back(video_channel);
  return video_channel;
}

void ChannelManager::DestroyVideoChannel(VideoChannel* video_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVideoChannel");
  if (video_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyVideoChannel_w, this, video_channel));
  }
}

void ChannelManager::DestroyVideoChannel_w(VideoChannel* video_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVideoChannel_w");
  // Destroy video channel.
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  VideoChannels::iterator it = std::find(video_channels_.begin(),
      video_channels_.end(), video_channel);
  RTC_DCHECK(it != video_channels_.end());
  if (it == video_channels_.end())
    return;

  video_channels_.erase(it);
  delete video_channel;
}

RtpDataChannel* ChannelManager::CreateRtpDataChannel(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required) {
  return worker_thread_->Invoke<RtpDataChannel*>(
      RTC_FROM_HERE, Bind(&ChannelManager::CreateRtpDataChannel_w, this,
                          media_controller, rtp_transport, rtcp_transport,
                          signaling_thread, content_name, srtp_required));
}

RtpDataChannel* ChannelManager::CreateRtpDataChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required) {
  // This is ok to alloc from a thread other than the worker thread.
  RTC_DCHECK(initialized_);
  MediaConfig config;
  if (media_controller) {
    config = media_controller->config();
  }
  DataMediaChannel* media_channel = data_media_engine_->CreateChannel(config);
  if (!media_channel) {
    LOG(LS_WARNING) << "Failed to create RTP data channel.";
    return nullptr;
  }

  RtpDataChannel* data_channel = new RtpDataChannel(
      worker_thread_, network_thread_, signaling_thread, media_channel,
      content_name, rtcp_transport == nullptr, srtp_required);
  data_channel->SetCryptoOptions(crypto_options_);
  if (!data_channel->Init_w(rtp_transport, rtcp_transport, rtp_transport,
                            rtcp_transport)) {
    LOG(LS_WARNING) << "Failed to init data channel.";
    delete data_channel;
    return nullptr;
  }
  data_channels_.push_back(data_channel);
  return data_channel;
}

void ChannelManager::DestroyRtpDataChannel(RtpDataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyRtpDataChannel");
  if (data_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyRtpDataChannel_w, this, data_channel));
  }
}

void ChannelManager::DestroyRtpDataChannel_w(RtpDataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyRtpDataChannel_w");
  // Destroy data channel.
  RTC_DCHECK(initialized_);
  RtpDataChannels::iterator it =
      std::find(data_channels_.begin(), data_channels_.end(), data_channel);
  RTC_DCHECK(it != data_channels_.end());
  if (it == data_channels_.end())
    return;

  data_channels_.erase(it);
  delete data_channel;
}

bool ChannelManager::StartAecDump(rtc::PlatformFile file,
                                  int64_t max_size_bytes) {
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&MediaEngineInterface::StartAecDump,
                          media_engine_.get(), file, max_size_bytes));
}

void ChannelManager::StopAecDump() {
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(&MediaEngineInterface::StopAecDump, media_engine_.get()));
}

}  // namespace cricket
