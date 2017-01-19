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

#include "webrtc/api/mediacontroller.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/device.h"
#include "webrtc/media/base/hybriddataengine.h"
#include "webrtc/media/base/rtpdataengine.h"
#ifdef HAVE_SCTP
#include "webrtc/media/sctp/sctpdataengine.h"
#endif
#include "webrtc/pc/srtpfilter.h"

namespace cricket {


using rtc::Bind;

static DataEngineInterface* ConstructDataEngine() {
#ifdef HAVE_SCTP
  return new HybridDataEngine(new RtpDataEngine(), new SctpDataEngine());
#else
  return new RtpDataEngine();
#endif
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               rtc::Thread* thread) {
  Construct(me, dme, thread, thread);
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  Construct(me, ConstructDataEngine(), worker_thread, network_thread);
}

void ChannelManager::Construct(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  media_engine_.reset(me);
  data_media_engine_.reset(dme);
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
#if defined(ENABLE_EXTERNAL_AUTH)
  if (crypto_options_.enable_gcm_crypto_suites) {
    // TODO(jbauch): Re-enable once https://crbug.com/628400 is resolved.
    crypto_options_.enable_gcm_crypto_suites = false;
    LOG(LS_WARNING) << "GCM ciphers are not supported with " <<
        "ENABLE_EXTERNAL_AUTH and will be disabled.";
  }
#endif
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

  std::vector<VideoCodec>::const_iterator it;
  for (it = media_engine_->video_codecs().begin();
      it != media_engine_->video_codecs().end(); ++it) {
    if (!enable_rtx_ && _stricmp(kRtxCodecName, it->name.c_str()) == 0) {
      continue;
    }
    codecs->push_back(*it);
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
  ASSERT(!initialized_);
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
  ASSERT(initialized_);
  return initialized_;
}

bool ChannelManager::InitMediaEngine_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  return media_engine_->Init();
}

void ChannelManager::Terminate() {
  ASSERT(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(RTC_FROM_HERE,
                               Bind(&ChannelManager::Terminate_w, this));
  initialized_ = false;
}

void ChannelManager::DestructorDeletes_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  media_engine_.reset(NULL);
}

void ChannelManager::Terminate_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
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
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      RTC_FROM_HERE, Bind(&ChannelManager::CreateVoiceChannel_w, this,
                          media_controller, transport_controller, content_name,
                          bundle_transport_name, rtcp, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    const AudioOptions& options) {
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  ASSERT(nullptr != media_controller);
  VoiceMediaChannel* media_channel = media_engine_->CreateChannel(
      media_controller->call_w(), media_controller->config(), options);
  if (!media_channel)
    return nullptr;

  VoiceChannel* voice_channel =
      new VoiceChannel(worker_thread_, network_thread_, media_engine_.get(),
                       media_channel, transport_controller, content_name, rtcp);
  voice_channel->SetCryptoOptions(crypto_options_);
  if (!voice_channel->Init_w(bundle_transport_name)) {
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
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  VoiceChannels::iterator it = std::find(voice_channels_.begin(),
      voice_channels_.end(), voice_channel);
  ASSERT(it != voice_channels_.end());
  if (it == voice_channels_.end())
    return;
  voice_channels_.erase(it);
  delete voice_channel;
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      RTC_FROM_HERE, Bind(&ChannelManager::CreateVideoChannel_w, this,
                          media_controller, transport_controller, content_name,
                          bundle_transport_name, rtcp, options));
}

VideoChannel* ChannelManager::CreateVideoChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    const VideoOptions& options) {
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  ASSERT(nullptr != media_controller);
  VideoMediaChannel* media_channel = media_engine_->CreateVideoChannel(
      media_controller->call_w(), media_controller->config(), options);
  if (media_channel == NULL) {
    return NULL;
  }

  VideoChannel* video_channel =
      new VideoChannel(worker_thread_, network_thread_, media_channel,
                       transport_controller, content_name, rtcp);
  video_channel->SetCryptoOptions(crypto_options_);
  if (!video_channel->Init_w(bundle_transport_name)) {
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
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  VideoChannels::iterator it = std::find(video_channels_.begin(),
      video_channels_.end(), video_channel);
  ASSERT(it != video_channels_.end());
  if (it == video_channels_.end())
    return;

  video_channels_.erase(it);
  delete video_channel;
}

DataChannel* ChannelManager::CreateDataChannel(
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    DataChannelType channel_type) {
  return worker_thread_->Invoke<DataChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateDataChannel_w, this, transport_controller,
           content_name, bundle_transport_name, rtcp, channel_type));
}

DataChannel* ChannelManager::CreateDataChannel_w(
    TransportController* transport_controller,
    const std::string& content_name,
    const std::string* bundle_transport_name,
    bool rtcp,
    DataChannelType data_channel_type) {
  // This is ok to alloc from a thread other than the worker thread.
  ASSERT(initialized_);
  DataMediaChannel* media_channel = data_media_engine_->CreateChannel(
      data_channel_type);
  if (!media_channel) {
    LOG(LS_WARNING) << "Failed to create data channel of type "
                    << data_channel_type;
    return NULL;
  }

  DataChannel* data_channel =
      new DataChannel(worker_thread_, network_thread_, media_channel,
                      transport_controller, content_name, rtcp);
  data_channel->SetCryptoOptions(crypto_options_);
  if (!data_channel->Init_w(bundle_transport_name)) {
    LOG(LS_WARNING) << "Failed to init data channel.";
    delete data_channel;
    return NULL;
  }
  data_channels_.push_back(data_channel);
  return data_channel;
}

void ChannelManager::DestroyDataChannel(DataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyDataChannel");
  if (data_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyDataChannel_w, this, data_channel));
  }
}

void ChannelManager::DestroyDataChannel_w(DataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyDataChannel_w");
  // Destroy data channel.
  ASSERT(initialized_);
  DataChannels::iterator it = std::find(data_channels_.begin(),
      data_channels_.end(), data_channel);
  ASSERT(it != data_channels_.end());
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
