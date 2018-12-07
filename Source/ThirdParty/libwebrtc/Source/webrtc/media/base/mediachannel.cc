/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/mediachannel.h"

namespace cricket {

VideoOptions::VideoOptions() = default;
VideoOptions::~VideoOptions() = default;

MediaChannel::MediaChannel(const MediaConfig& config)
    : enable_dscp_(config.enable_dscp) {}

MediaChannel::MediaChannel() : enable_dscp_(false) {}

MediaChannel::~MediaChannel() {}

void MediaChannel::SetInterface(
    NetworkInterface* iface,
    webrtc::MediaTransportInterface* media_transport) {
  rtc::CritScope cs(&network_interface_crit_);
  network_interface_ = iface;
  media_transport_ = media_transport;
  UpdateDscp();
}

int MediaChannel::GetRtpSendTimeExtnId() const {
  return -1;
}

rtc::DiffServCodePoint MediaChannel::PreferredDscp() const {
  return rtc::DSCP_DEFAULT;
}

void MediaChannel::SetFrameEncryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
  // Placeholder should be pure virtual once internal supports it.
}

void MediaChannel::SetFrameDecryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  // Placeholder should be pure virtual once internal supports it.
}

MediaSenderInfo::MediaSenderInfo() = default;
MediaSenderInfo::~MediaSenderInfo() = default;

MediaReceiverInfo::MediaReceiverInfo() = default;
MediaReceiverInfo::~MediaReceiverInfo() = default;

VoiceSenderInfo::VoiceSenderInfo() = default;
VoiceSenderInfo::~VoiceSenderInfo() = default;

VoiceReceiverInfo::VoiceReceiverInfo() = default;
VoiceReceiverInfo::~VoiceReceiverInfo() = default;

VideoSenderInfo::VideoSenderInfo() = default;
VideoSenderInfo::~VideoSenderInfo() = default;

VideoReceiverInfo::VideoReceiverInfo() = default;
VideoReceiverInfo::~VideoReceiverInfo() = default;

VoiceMediaInfo::VoiceMediaInfo() = default;
VoiceMediaInfo::~VoiceMediaInfo() = default;

VideoMediaInfo::VideoMediaInfo() = default;
VideoMediaInfo::~VideoMediaInfo() = default;

DataMediaInfo::DataMediaInfo() = default;
DataMediaInfo::~DataMediaInfo() = default;

AudioSendParameters::AudioSendParameters() = default;
AudioSendParameters::~AudioSendParameters() = default;

std::map<std::string, std::string> AudioSendParameters::ToStringMap() const {
  auto params = RtpSendParameters<AudioCodec>::ToStringMap();
  params["options"] = options.ToString();
  return params;
}

cricket::MediaType VoiceMediaChannel::media_type() const {
  return cricket::MediaType::MEDIA_TYPE_AUDIO;
}

VideoSendParameters::VideoSendParameters() = default;
VideoSendParameters::~VideoSendParameters() = default;

std::map<std::string, std::string> VideoSendParameters::ToStringMap() const {
  auto params = RtpSendParameters<VideoCodec>::ToStringMap();
  params["conference_mode"] = (conference_mode ? "yes" : "no");
  return params;
}

cricket::MediaType VideoMediaChannel::media_type() const {
  return cricket::MediaType::MEDIA_TYPE_VIDEO;
}

DataMediaChannel::DataMediaChannel() = default;
DataMediaChannel::DataMediaChannel(const MediaConfig& config)
    : MediaChannel(config) {}
DataMediaChannel::~DataMediaChannel() = default;

cricket::MediaType DataMediaChannel::media_type() const {
  return cricket::MediaType::MEDIA_TYPE_DATA;
}

bool DataMediaChannel::GetStats(DataMediaInfo* info) {
  return true;
}

}  // namespace cricket
