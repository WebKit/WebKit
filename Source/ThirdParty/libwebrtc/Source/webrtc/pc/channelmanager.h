/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_CHANNELMANAGER_H_
#define WEBRTC_PC_CHANNELMANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/fileutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/mediaengine.h"
#include "webrtc/pc/voicechannel.h"

namespace cricket {

class VoiceChannel;

// ChannelManager allows the MediaEngine to run on a separate thread, and takes
// care of marshalling calls between threads. It also creates and keeps track of
// voice and video channels; by doing so, it can temporarily pause all the
// channels when a new audio or video device is chosen. The voice and video
// channels are stored in separate vectors, to easily allow operations on just
// voice or just video channels.
// ChannelManager also allows the application to discover what devices it has
// using device manager.
class ChannelManager {
 public:
  // For testing purposes. Allows the media engine and data media
  // engine and dev manager to be mocks.
  ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                 std::unique_ptr<DataEngineInterface> dme,
                 rtc::Thread* worker_and_network);
  // Same as above, but gives an easier default DataEngine.
  ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                 rtc::Thread* worker,
                 rtc::Thread* network);
  ~ChannelManager();

  // Accessors for the worker thread, allowing it to be set after construction,
  // but before Init. set_worker_thread will return false if called after Init.
  rtc::Thread* worker_thread() const { return worker_thread_; }
  bool set_worker_thread(rtc::Thread* thread) {
    if (initialized_) {
      return false;
    }
    worker_thread_ = thread;
    return true;
  }
  rtc::Thread* network_thread() const { return network_thread_; }
  bool set_network_thread(rtc::Thread* thread) {
    if (initialized_) {
      return false;
    }
    network_thread_ = thread;
    return true;
  }

  MediaEngineInterface* media_engine() { return media_engine_.get(); }

  // Retrieves the list of supported audio & video codec types.
  // Can be called before starting the media engine.
  void GetSupportedAudioSendCodecs(std::vector<AudioCodec>* codecs) const;
  void GetSupportedAudioReceiveCodecs(std::vector<AudioCodec>* codecs) const;
  void GetSupportedAudioRtpHeaderExtensions(RtpHeaderExtensions* ext) const;
  void GetSupportedVideoCodecs(std::vector<VideoCodec>* codecs) const;
  void GetSupportedVideoRtpHeaderExtensions(RtpHeaderExtensions* ext) const;
  void GetSupportedDataCodecs(std::vector<DataCodec>* codecs) const;

  // Indicates whether the media engine is started.
  bool initialized() const { return initialized_; }
  // Starts up the media engine.
  bool Init();
  // Shuts down the media engine.
  void Terminate();

  // The operations below all occur on the worker thread.
  // Creates a voice channel, to be associated with the specified session.
  VoiceChannel* CreateVoiceChannel(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_transport,
      DtlsTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const AudioOptions& options);
  // Version of the above that takes PacketTransportInternal.
  VoiceChannel* CreateVoiceChannel(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      rtc::PacketTransportInternal* rtp_transport,
      rtc::PacketTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const AudioOptions& options);
  // Destroys a voice channel created with the Create API.
  void DestroyVoiceChannel(VoiceChannel* voice_channel);
  // Creates a video channel, synced with the specified voice channel, and
  // associated with the specified session.
  VideoChannel* CreateVideoChannel(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_transport,
      DtlsTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const VideoOptions& options);
  // Version of the above that takes PacketTransportInternal.
  VideoChannel* CreateVideoChannel(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      rtc::PacketTransportInternal* rtp_transport,
      rtc::PacketTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const VideoOptions& options);
  // Destroys a video channel created with the Create API.
  void DestroyVideoChannel(VideoChannel* video_channel);
  RtpDataChannel* CreateRtpDataChannel(
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_transport,
      DtlsTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required);
  // Destroys a data channel created with the Create API.
  void DestroyRtpDataChannel(RtpDataChannel* data_channel);

  // Indicates whether any channels exist.
  bool has_channels() const {
    return (!voice_channels_.empty() || !video_channels_.empty());
  }

  // RTX will be enabled/disabled in engines that support it. The supporting
  // engines will start offering an RTX codec. Must be called before Init().
  bool SetVideoRtxEnabled(bool enable);

  // Starts/stops the local microphone and enables polling of the input level.
  bool capturing() const { return capturing_; }

  // The operations below occur on the main thread.

  // Starts AEC dump using existing file, with a specified maximum file size in
  // bytes. When the limit is reached, logging will stop and the file will be
  // closed. If max_size_bytes is set to <= 0, no limit will be used.
  bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes);

  // Stops recording AEC dump.
  void StopAecDump();

 private:
  typedef std::vector<VoiceChannel*> VoiceChannels;
  typedef std::vector<VideoChannel*> VideoChannels;
  typedef std::vector<RtpDataChannel*> RtpDataChannels;

  void Construct(std::unique_ptr<MediaEngineInterface> me,
                 std::unique_ptr<DataEngineInterface> dme,
                 rtc::Thread* worker_thread,
                 rtc::Thread* network_thread);
  bool InitMediaEngine_w();
  void DestructorDeletes_w();
  void Terminate_w();
  VoiceChannel* CreateVoiceChannel_w(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_dtls_transport,
      DtlsTransportInternal* rtcp_dtls_transport,
      rtc::PacketTransportInternal* rtp_packet_transport,
      rtc::PacketTransportInternal* rtcp_packet_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const AudioOptions& options);
  void DestroyVoiceChannel_w(VoiceChannel* voice_channel);
  VideoChannel* CreateVideoChannel_w(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_dtls_transport,
      DtlsTransportInternal* rtcp_dtls_transport,
      rtc::PacketTransportInternal* rtp_packet_transport,
      rtc::PacketTransportInternal* rtcp_packet_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required,
      const VideoOptions& options);
  void DestroyVideoChannel_w(VideoChannel* video_channel);
  RtpDataChannel* CreateRtpDataChannel_w(
      const cricket::MediaConfig& media_config,
      DtlsTransportInternal* rtp_transport,
      DtlsTransportInternal* rtcp_transport,
      rtc::Thread* signaling_thread,
      const std::string& content_name,
      bool srtp_required);
  void DestroyRtpDataChannel_w(RtpDataChannel* data_channel);

  std::unique_ptr<MediaEngineInterface> media_engine_;
  std::unique_ptr<DataEngineInterface> data_media_engine_;
  bool initialized_;
  rtc::Thread* main_thread_;
  rtc::Thread* worker_thread_;
  rtc::Thread* network_thread_;

  VoiceChannels voice_channels_;
  VideoChannels video_channels_;
  RtpDataChannels data_channels_;

  bool enable_rtx_;
  rtc::CryptoOptions crypto_options_;

  bool capturing_;
};

}  // namespace cricket

#endif  // WEBRTC_PC_CHANNELMANAGER_H_
