/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_CHANNEL_H_
#define WEBRTC_PC_CHANNEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/api/call/audio_sink.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/network.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/window.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/mediaengine.h"
#include "webrtc/media/base/streamparams.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videosourceinterface.h"
#include "webrtc/p2p/base/dtlstransportinternal.h"
#include "webrtc/p2p/base/packettransportinternal.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/p2p/client/socketmonitor.h"
#include "webrtc/pc/audiomonitor.h"
#include "webrtc/pc/bundlefilter.h"
#include "webrtc/pc/mediamonitor.h"
#include "webrtc/pc/mediasession.h"
#include "webrtc/pc/rtcpmuxfilter.h"
#include "webrtc/pc/srtpfilter.h"

namespace webrtc {
class AudioSinkInterface;
}  // namespace webrtc

namespace cricket {

struct CryptoParams;
class MediaContentDescription;

// BaseChannel contains logic common to voice and video, including enable,
// marshaling calls to a worker and network threads, and connection and media
// monitors.
//
// BaseChannel assumes signaling and other threads are allowed to make
// synchronous calls to the worker thread, the worker thread makes synchronous
// calls only to the network thread, and the network thread can't be blocked by
// other threads.
// All methods with _n suffix must be called on network thread,
//     methods with _w suffix on worker thread
// and methods with _s suffix on signaling thread.
// Network and worker threads may be the same thread.
//
// WARNING! SUBCLASSES MUST CALL Deinit() IN THEIR DESTRUCTORS!
// This is required to avoid a data race between the destructor modifying the
// vtable, and the media channel's thread using BaseChannel as the
// NetworkInterface.

class BaseChannel
    : public rtc::MessageHandler, public sigslot::has_slots<>,
      public MediaChannel::NetworkInterface,
      public ConnectionStatsGetter {
 public:
  // If |srtp_required| is true, the channel will not send or receive any
  // RTP/RTCP packets without using SRTP (either using SDES or DTLS-SRTP).
  BaseChannel(rtc::Thread* worker_thread,
              rtc::Thread* network_thread,
              rtc::Thread* signaling_thread,
              MediaChannel* channel,
              const std::string& content_name,
              bool rtcp_mux_required,
              bool srtp_required);
  virtual ~BaseChannel();
  bool Init_w(DtlsTransportInternal* rtp_dtls_transport,
              DtlsTransportInternal* rtcp_dtls_transport,
              rtc::PacketTransportInternal* rtp_packet_transport,
              rtc::PacketTransportInternal* rtcp_packet_transport);
  // Deinit may be called multiple times and is simply ignored if it's already
  // done.
  void Deinit();

  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  const std::string& content_name() const { return content_name_; }
  // TODO(deadbeef): This is redundant; remove this.
  const std::string& transport_name() const { return transport_name_; }
  bool enabled() const { return enabled_; }

  // This function returns true if we are using SRTP.
  bool secure() const { return srtp_filter_.IsActive(); }
  // The following function returns true if we are using
  // DTLS-based keying. If you turned off SRTP later, however
  // you could have secure() == false and dtls_secure() == true.
  bool secure_dtls() const { return dtls_keyed_; }

  bool writable() const { return writable_; }

  // Set the transport(s), and update writability and "ready-to-send" state.
  // |rtp_transport| must be non-null.
  // |rtcp_transport| must be supplied if NeedsRtcpTransport() is true (meaning
  // RTCP muxing is not fully active yet).
  // |rtp_transport| and |rtcp_transport| must share the same transport name as
  // well.
  // Can not start with "rtc::PacketTransportInternal" and switch to
  // "DtlsTransportInternal", or vice-versa.
  void SetTransports(DtlsTransportInternal* rtp_dtls_transport,
                     DtlsTransportInternal* rtcp_dtls_transport);
  void SetTransports(rtc::PacketTransportInternal* rtp_packet_transport,
                     rtc::PacketTransportInternal* rtcp_packet_transport);
  bool PushdownLocalDescription(const SessionDescription* local_desc,
                                ContentAction action,
                                std::string* error_desc);
  bool PushdownRemoteDescription(const SessionDescription* remote_desc,
                                ContentAction action,
                                std::string* error_desc);
  // Channel control
  bool SetLocalContent(const MediaContentDescription* content,
                       ContentAction action,
                       std::string* error_desc);
  bool SetRemoteContent(const MediaContentDescription* content,
                        ContentAction action,
                        std::string* error_desc);

  bool Enable(bool enable);

  // Multiplexing
  bool AddRecvStream(const StreamParams& sp);
  bool RemoveRecvStream(uint32_t ssrc);
  bool AddSendStream(const StreamParams& sp);
  bool RemoveSendStream(uint32_t ssrc);

  // Monitoring
  void StartConnectionMonitor(int cms);
  void StopConnectionMonitor();
  // For ConnectionStatsGetter, used by ConnectionMonitor
  bool GetConnectionStats(ConnectionInfos* infos) override;

  BundleFilter* bundle_filter() { return &bundle_filter_; }

  const std::vector<StreamParams>& local_streams() const {
    return local_streams_;
  }
  const std::vector<StreamParams>& remote_streams() const {
    return remote_streams_;
  }

  sigslot::signal2<BaseChannel*, bool> SignalDtlsSrtpSetupFailure;
  void SignalDtlsSrtpSetupFailure_n(bool rtcp);
  void SignalDtlsSrtpSetupFailure_s(bool rtcp);

  // Used for latency measurements.
  sigslot::signal1<BaseChannel*> SignalFirstPacketReceived;

  // Forward SignalSentPacket to worker thread.
  sigslot::signal1<const rtc::SentPacket&> SignalSentPacket;

  // Emitted whenever rtcp-mux is fully negotiated and the rtcp-transport can
  // be destroyed.
  // Fired on the network thread.
  sigslot::signal1<const std::string&> SignalRtcpMuxFullyActive;

  // Only public for unit tests.  Otherwise, consider private.
  DtlsTransportInternal* rtp_dtls_transport() const {
    return rtp_dtls_transport_;
  }
  DtlsTransportInternal* rtcp_dtls_transport() const {
    return rtcp_dtls_transport_;
  }

  bool NeedsRtcpTransport();

  // Made public for easier testing.
  //
  // Updates "ready to send" for an individual channel, and informs the media
  // channel that the transport is ready to send if each channel (in use) is
  // ready to send. This is more specific than just "writable"; it means the
  // last send didn't return ENOTCONN.
  //
  // This should be called whenever a channel's ready-to-send state changes,
  // or when RTCP muxing becomes active/inactive.
  void SetTransportChannelReadyToSend(bool rtcp, bool ready);

  // Only public for unit tests.  Otherwise, consider protected.
  int SetOption(SocketType type, rtc::Socket::Option o, int val)
      override;
  int SetOption_n(SocketType type, rtc::Socket::Option o, int val);

  SrtpFilter* srtp_filter() { return &srtp_filter_; }

  virtual cricket::MediaType media_type() = 0;

  bool SetCryptoOptions(const rtc::CryptoOptions& crypto_options);

  // This function returns true if we require SRTP for call setup.
  bool srtp_required_for_testing() const { return srtp_required_; }

 protected:
  virtual MediaChannel* media_channel() const { return media_channel_; }

  void SetTransports_n(DtlsTransportInternal* rtp_dtls_transport,
                       DtlsTransportInternal* rtcp_dtls_transport,
                       rtc::PacketTransportInternal* rtp_packet_transport,
                       rtc::PacketTransportInternal* rtcp_packet_transport);

  // This does not update writability or "ready-to-send" state; it just
  // disconnects from the old channel and connects to the new one.
  void SetTransport_n(bool rtcp,
                      DtlsTransportInternal* new_dtls_transport,
                      rtc::PacketTransportInternal* new_packet_transport);

  bool was_ever_writable() const { return was_ever_writable_; }
  void set_local_content_direction(MediaContentDirection direction) {
    local_content_direction_ = direction;
  }
  void set_remote_content_direction(MediaContentDirection direction) {
    remote_content_direction_ = direction;
  }
  // These methods verify that:
  // * The required content description directions have been set.
  // * The channel is enabled.
  // * And for sending:
  //   - The SRTP filter is active if it's needed.
  //   - The transport has been writable before, meaning it should be at least
  //     possible to succeed in sending a packet.
  //
  // When any of these properties change, UpdateMediaSendRecvState_w should be
  // called.
  bool IsReadyToReceiveMedia_w() const;
  bool IsReadyToSendMedia_w() const;
  rtc::Thread* signaling_thread() { return signaling_thread_; }

  void ConnectToDtlsTransport(DtlsTransportInternal* transport);
  void DisconnectFromDtlsTransport(DtlsTransportInternal* transport);
  void ConnectToPacketTransport(rtc::PacketTransportInternal* transport);
  void DisconnectFromPacketTransport(rtc::PacketTransportInternal* transport);

  void FlushRtcpMessages_n();

  // NetworkInterface implementation, called by MediaEngine
  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options) override;
  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options) override;

  // From TransportChannel
  void OnWritableState(rtc::PacketTransportInternal* transport);
  virtual void OnPacketRead(rtc::PacketTransportInternal* transport,
                            const char* data,
                            size_t len,
                            const rtc::PacketTime& packet_time,
                            int flags);
  void OnReadyToSend(rtc::PacketTransportInternal* transport);

  void OnDtlsState(DtlsTransportInternal* transport, DtlsTransportState state);

  void OnSelectedCandidatePairChanged(
      IceTransportInternal* ice_transport,
      CandidatePairInterface* selected_candidate_pair,
      int last_sent_packet_id,
      bool ready_to_send);

  bool PacketIsRtcp(const rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len);
  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options);

  bool WantsPacket(bool rtcp, const rtc::CopyOnWriteBuffer* packet);
  void HandlePacket(bool rtcp, rtc::CopyOnWriteBuffer* packet,
                    const rtc::PacketTime& packet_time);
  void OnPacketReceived(bool rtcp,
                        const rtc::CopyOnWriteBuffer& packet,
                        const rtc::PacketTime& packet_time);

  void EnableMedia_w();
  void DisableMedia_w();

  // Performs actions if the RTP/RTCP writable state changed. This should
  // be called whenever a channel's writable state changes or when RTCP muxing
  // becomes active/inactive.
  void UpdateWritableState_n();
  void ChannelWritable_n();
  void ChannelNotWritable_n();

  bool AddRecvStream_w(const StreamParams& sp);
  bool RemoveRecvStream_w(uint32_t ssrc);
  bool AddSendStream_w(const StreamParams& sp);
  bool RemoveSendStream_w(uint32_t ssrc);
  bool ShouldSetupDtlsSrtp_n() const;
  // Do the DTLS key expansion and impose it on the SRTP/SRTCP filters.
  // |rtcp_channel| indicates whether to set up the RTP or RTCP filter.
  bool SetupDtlsSrtp_n(bool rtcp);
  void MaybeSetupDtlsSrtp_n();
  // Set the DTLS-SRTP cipher policy on this channel as appropriate.
  bool SetDtlsSrtpCryptoSuites_n(DtlsTransportInternal* transport, bool rtcp);

  // Should be called whenever the conditions for
  // IsReadyToReceiveMedia/IsReadyToSendMedia are satisfied (or unsatisfied).
  // Updates the send/recv state of the media channel.
  void UpdateMediaSendRecvState();
  virtual void UpdateMediaSendRecvState_w() = 0;

  // Gets the content info appropriate to the channel (audio or video).
  virtual const ContentInfo* GetFirstContent(
      const SessionDescription* sdesc) = 0;
  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            ContentAction action,
                            std::string* error_desc);
  bool UpdateRemoteStreams_w(const std::vector<StreamParams>& streams,
                             ContentAction action,
                             std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc) = 0;
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc) = 0;
  bool SetRtpTransportParameters(const MediaContentDescription* content,
                                 ContentAction action,
                                 ContentSource src,
                                 std::string* error_desc);
  bool SetRtpTransportParameters_n(const MediaContentDescription* content,
                                   ContentAction action,
                                   ContentSource src,
                                   std::string* error_desc);

  // Helper method to get RTP Absoulute SendTime extension header id if
  // present in remote supported extensions list.
  void MaybeCacheRtpAbsSendTimeHeaderExtension_w(
      const std::vector<webrtc::RtpExtension>& extensions);

  bool CheckSrtpConfig_n(const std::vector<CryptoParams>& cryptos,
                         bool* dtls,
                         std::string* error_desc);
  bool SetSrtp_n(const std::vector<CryptoParams>& params,
                 ContentAction action,
                 ContentSource src,
                 std::string* error_desc);
  bool SetRtcpMux_n(bool enable,
                    ContentAction action,
                    ContentSource src,
                    std::string* error_desc);

  // From MessageHandler
  void OnMessage(rtc::Message* pmsg) override;

  const rtc::CryptoOptions& crypto_options() const {
    return crypto_options_;
  }

  // Handled in derived classes
  // Get the SRTP crypto suites to use for RTP media
  virtual void GetSrtpCryptoSuites_n(std::vector<int>* crypto_suites) const = 0;
  virtual void OnConnectionMonitorUpdate(ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) = 0;

  // Helper function for invoking bool-returning methods on the worker thread.
  template <class FunctorT>
  bool InvokeOnWorker(const rtc::Location& posted_from,
                      const FunctorT& functor) {
    return worker_thread_->Invoke<bool>(posted_from, functor);
  }

 private:
  bool InitNetwork_n(DtlsTransportInternal* rtp_dtls_transport,
                     DtlsTransportInternal* rtcp_dtls_transport,
                     rtc::PacketTransportInternal* rtp_packet_transport,
                     rtc::PacketTransportInternal* rtcp_packet_transport);
  void DisconnectTransportChannels_n();
  void SignalSentPacket_n(rtc::PacketTransportInternal* transport,
                          const rtc::SentPacket& sent_packet);
  void SignalSentPacket_w(const rtc::SentPacket& sent_packet);
  bool IsReadyToSendMedia_n() const;
  void CacheRtpAbsSendTimeHeaderExtension_n(int rtp_abs_sendtime_extn_id);
  int GetTransportOverheadPerPacket() const;
  void UpdateTransportOverhead();

  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const signaling_thread_;
  rtc::AsyncInvoker invoker_;

  const std::string content_name_;
  std::unique_ptr<ConnectionMonitor> connection_monitor_;

  // Won't be set when using raw packet transports. SDP-specific thing.
  std::string transport_name_;
  // True if RTCP-multiplexing is required. In other words, no standalone RTCP
  // transport will ever be used for this channel.
  const bool rtcp_mux_required_;

  // Separate DTLS/non-DTLS pointers to support using BaseChannel without DTLS.
  // Temporary measure until more refactoring is done.
  // If non-null, "X_dtls_transport_" will always equal "X_packet_transport_".
  DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;
  rtc::PacketTransportInternal* rtp_packet_transport_ = nullptr;
  rtc::PacketTransportInternal* rtcp_packet_transport_ = nullptr;
  std::vector<std::pair<rtc::Socket::Option, int> > socket_options_;
  std::vector<std::pair<rtc::Socket::Option, int> > rtcp_socket_options_;
  SrtpFilter srtp_filter_;
  RtcpMuxFilter rtcp_mux_filter_;
  BundleFilter bundle_filter_;
  bool rtp_ready_to_send_ = false;
  bool rtcp_ready_to_send_ = false;
  bool writable_ = false;
  bool was_ever_writable_ = false;
  bool has_received_packet_ = false;
  bool dtls_keyed_ = false;
  const bool srtp_required_ = true;
  rtc::CryptoOptions crypto_options_;
  int rtp_abs_sendtime_extn_id_ = -1;

  // MediaChannel related members that should be accessed from the worker
  // thread.
  MediaChannel* const media_channel_;
  // Currently the |enabled_| flag is accessed from the signaling thread as
  // well, but it can be changed only when signaling thread does a synchronous
  // call to the worker thread, so it should be safe.
  bool enabled_ = false;
  std::vector<StreamParams> local_streams_;
  std::vector<StreamParams> remote_streams_;
  MediaContentDirection local_content_direction_ = MD_INACTIVE;
  MediaContentDirection remote_content_direction_ = MD_INACTIVE;
  CandidatePairInterface* selected_candidate_pair_;
};

// VoiceChannel is a specialization that adds support for early media, DTMF,
// and input/output level monitoring.
class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(rtc::Thread* worker_thread,
               rtc::Thread* network_thread,
               rtc::Thread* signaling_thread,
               MediaEngineInterface* media_engine,
               VoiceMediaChannel* channel,
               const std::string& content_name,
               bool rtcp_mux_required,
               bool srtp_required);
  ~VoiceChannel();

  // Configure sending media on the stream with SSRC |ssrc|
  // If there is only one sending stream SSRC 0 can be used.
  bool SetAudioSend(uint32_t ssrc,
                    bool enable,
                    const AudioOptions* options,
                    AudioSource* source);

  // downcasts a MediaChannel
  VoiceMediaChannel* media_channel() const override {
    return static_cast<VoiceMediaChannel*>(BaseChannel::media_channel());
  }

  void SetEarlyMedia(bool enable);
  // This signal is emitted when we have gone a period of time without
  // receiving early media. When received, a UI should start playing its
  // own ringing sound
  sigslot::signal1<VoiceChannel*> SignalEarlyMediaTimeout;

  // Returns if the telephone-event has been negotiated.
  bool CanInsertDtmf();
  // Send and/or play a DTMF |event| according to the |flags|.
  // The DTMF out-of-band signal will be used on sending.
  // The |ssrc| should be either 0 or a valid send stream ssrc.
  // The valid value for the |event| are 0 which corresponding to DTMF
  // event 0-9, *, #, A-D.
  bool InsertDtmf(uint32_t ssrc, int event_code, int duration);
  bool SetOutputVolume(uint32_t ssrc, double volume);
  void SetRawAudioSink(uint32_t ssrc,
                       std::unique_ptr<webrtc::AudioSinkInterface> sink);
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const;
  bool SetRtpSendParameters(uint32_t ssrc,
                            const webrtc::RtpParameters& parameters);
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const;
  bool SetRtpReceiveParameters(uint32_t ssrc,
                               const webrtc::RtpParameters& parameters);

  // Get statistics about the current media session.
  bool GetStats(VoiceMediaInfo* stats);

  // Monitoring functions
  sigslot::signal2<VoiceChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VoiceChannel*, const VoiceMediaInfo&> SignalMediaMonitor;

  void StartAudioMonitor(int cms);
  void StopAudioMonitor();
  bool IsAudioMonitorRunning() const;
  sigslot::signal2<VoiceChannel*, const AudioInfo&> SignalAudioMonitor;

  int GetInputLevel_w();
  int GetOutputLevel_w();
  void GetActiveStreams_w(AudioInfo::StreamList* actives);
  webrtc::RtpParameters GetRtpSendParameters_w(uint32_t ssrc) const;
  bool SetRtpSendParameters_w(uint32_t ssrc, webrtc::RtpParameters parameters);
  webrtc::RtpParameters GetRtpReceiveParameters_w(uint32_t ssrc) const;
  bool SetRtpReceiveParameters_w(uint32_t ssrc,
                                 webrtc::RtpParameters parameters);
  cricket::MediaType media_type() override { return cricket::MEDIA_TYPE_AUDIO; }

 private:
  // overrides from BaseChannel
  void OnPacketRead(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len,
                    const rtc::PacketTime& packet_time,
                    int flags) override;
  void UpdateMediaSendRecvState_w() override;
  const ContentInfo* GetFirstContent(const SessionDescription* sdesc) override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         ContentAction action,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          ContentAction action,
                          std::string* error_desc) override;
  void HandleEarlyMediaTimeout();
  bool InsertDtmf_w(uint32_t ssrc, int event, int duration);
  bool SetOutputVolume_w(uint32_t ssrc, double volume);
  bool GetStats_w(VoiceMediaInfo* stats);

  void OnMessage(rtc::Message* pmsg) override;
  void GetSrtpCryptoSuites_n(std::vector<int>* crypto_suites) const override;
  void OnConnectionMonitorUpdate(
      ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) override;
  void OnMediaMonitorUpdate(VoiceMediaChannel* media_channel,
                            const VoiceMediaInfo& info);
  void OnAudioMonitorUpdate(AudioMonitor* monitor, const AudioInfo& info);

  static const int kEarlyMediaTimeout = 1000;
  MediaEngineInterface* media_engine_;
  bool received_media_;
  std::unique_ptr<VoiceMediaMonitor> media_monitor_;
  std::unique_ptr<AudioMonitor> audio_monitor_;

  // Last AudioSendParameters sent down to the media_channel() via
  // SetSendParameters.
  AudioSendParameters last_send_params_;
  // Last AudioRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  AudioRecvParameters last_recv_params_;
};

// VideoChannel is a specialization for video.
class VideoChannel : public BaseChannel {
 public:
  VideoChannel(rtc::Thread* worker_thread,
               rtc::Thread* network_thread,
               rtc::Thread* signaling_thread,
               VideoMediaChannel* channel,
               const std::string& content_name,
               bool rtcp_mux_required,
               bool srtp_required);
  ~VideoChannel();

  // downcasts a MediaChannel
  VideoMediaChannel* media_channel() const override {
    return static_cast<VideoMediaChannel*>(BaseChannel::media_channel());
  }

  bool SetSink(uint32_t ssrc,
               rtc::VideoSinkInterface<webrtc::VideoFrame>* sink);
  // Get statistics about the current media session.
  bool GetStats(VideoMediaInfo* stats);

  sigslot::signal2<VideoChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VideoChannel*, const VideoMediaInfo&> SignalMediaMonitor;

  // Register a source and set options.
  // The |ssrc| must correspond to a registered send stream.
  bool SetVideoSend(uint32_t ssrc,
                    bool enable,
                    const VideoOptions* options,
                    rtc::VideoSourceInterface<webrtc::VideoFrame>* source);
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const;
  bool SetRtpSendParameters(uint32_t ssrc,
                            const webrtc::RtpParameters& parameters);
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const;
  bool SetRtpReceiveParameters(uint32_t ssrc,
                               const webrtc::RtpParameters& parameters);
  cricket::MediaType media_type() override { return cricket::MEDIA_TYPE_VIDEO; }

 private:
  // overrides from BaseChannel
  void UpdateMediaSendRecvState_w() override;
  const ContentInfo* GetFirstContent(const SessionDescription* sdesc) override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         ContentAction action,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          ContentAction action,
                          std::string* error_desc) override;
  bool GetStats_w(VideoMediaInfo* stats);
  webrtc::RtpParameters GetRtpSendParameters_w(uint32_t ssrc) const;
  bool SetRtpSendParameters_w(uint32_t ssrc, webrtc::RtpParameters parameters);
  webrtc::RtpParameters GetRtpReceiveParameters_w(uint32_t ssrc) const;
  bool SetRtpReceiveParameters_w(uint32_t ssrc,
                                 webrtc::RtpParameters parameters);

  void OnMessage(rtc::Message* pmsg) override;
  void GetSrtpCryptoSuites_n(std::vector<int>* crypto_suites) const override;
  void OnConnectionMonitorUpdate(
      ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) override;
  void OnMediaMonitorUpdate(VideoMediaChannel* media_channel,
                            const VideoMediaInfo& info);

  std::unique_ptr<VideoMediaMonitor> media_monitor_;

  // Last VideoSendParameters sent down to the media_channel() via
  // SetSendParameters.
  VideoSendParameters last_send_params_;
  // Last VideoRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  VideoRecvParameters last_recv_params_;
};

// RtpDataChannel is a specialization for data.
class RtpDataChannel : public BaseChannel {
 public:
  RtpDataChannel(rtc::Thread* worker_thread,
                 rtc::Thread* network_thread,
                 rtc::Thread* signaling_thread,
                 DataMediaChannel* channel,
                 const std::string& content_name,
                 bool rtcp_mux_required,
                 bool srtp_required);
  ~RtpDataChannel();
  bool Init_w(DtlsTransportInternal* rtp_dtls_transport,
              DtlsTransportInternal* rtcp_dtls_transport,
              rtc::PacketTransportInternal* rtp_packet_transport,
              rtc::PacketTransportInternal* rtcp_packet_transport);

  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result);

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();

  // Should be called on the signaling thread only.
  bool ready_to_send_data() const {
    return ready_to_send_data_;
  }

  sigslot::signal2<RtpDataChannel*, const DataMediaInfo&> SignalMediaMonitor;
  sigslot::signal2<RtpDataChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  sigslot::signal2<const ReceiveDataParams&, const rtc::CopyOnWriteBuffer&>
      SignalDataReceived;
  // Signal for notifying when the channel becomes ready to send data.
  // That occurs when the channel is enabled, the transport is writable,
  // both local and remote descriptions are set, and the channel is unblocked.
  sigslot::signal1<bool> SignalReadyToSendData;
  cricket::MediaType media_type() override { return cricket::MEDIA_TYPE_DATA; }

 protected:
  // downcasts a MediaChannel.
  DataMediaChannel* media_channel() const override {
    return static_cast<DataMediaChannel*>(BaseChannel::media_channel());
  }

 private:
  struct SendDataMessageData : public rtc::MessageData {
    SendDataMessageData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer* payload,
                        SendDataResult* result)
        : params(params),
          payload(payload),
          result(result),
          succeeded(false) {
    }

    const SendDataParams& params;
    const rtc::CopyOnWriteBuffer* payload;
    SendDataResult* result;
    bool succeeded;
  };

  struct DataReceivedMessageData : public rtc::MessageData {
    // We copy the data because the data will become invalid after we
    // handle DataMediaChannel::SignalDataReceived but before we fire
    // SignalDataReceived.
    DataReceivedMessageData(
        const ReceiveDataParams& params, const char* data, size_t len)
        : params(params),
          payload(data, len) {
    }
    const ReceiveDataParams params;
    const rtc::CopyOnWriteBuffer payload;
  };

  typedef rtc::TypedMessageData<bool> DataChannelReadyToSendMessageData;

  // overrides from BaseChannel
  const ContentInfo* GetFirstContent(const SessionDescription* sdesc) override;
  // Checks that data channel type is RTP.
  bool CheckDataChannelTypeFromContent(const DataContentDescription* content,
                                       std::string* error_desc);
  bool SetLocalContent_w(const MediaContentDescription* content,
                         ContentAction action,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          ContentAction action,
                          std::string* error_desc) override;
  void UpdateMediaSendRecvState_w() override;

  void OnMessage(rtc::Message* pmsg) override;
  void GetSrtpCryptoSuites_n(std::vector<int>* crypto_suites) const override;
  void OnConnectionMonitorUpdate(
      ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) override;
  void OnMediaMonitorUpdate(DataMediaChannel* media_channel,
                            const DataMediaInfo& info);
  void OnDataReceived(
      const ReceiveDataParams& params, const char* data, size_t len);
  void OnDataChannelError(uint32_t ssrc, DataMediaChannel::Error error);
  void OnDataChannelReadyToSend(bool writable);

  std::unique_ptr<DataMediaMonitor> media_monitor_;
  bool ready_to_send_data_ = false;

  // Last DataSendParameters sent down to the media_channel() via
  // SetSendParameters.
  DataSendParameters last_send_params_;
  // Last DataRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  DataRecvParameters last_recv_params_;
};

}  // namespace cricket

#endif  // WEBRTC_PC_CHANNEL_H_
