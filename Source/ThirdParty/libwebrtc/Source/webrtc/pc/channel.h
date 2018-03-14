/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_H_
#define PC_CHANNEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/call/audio_sink.h"
#include "api/jsep.h"
#include "api/rtpreceiverinterface.h"
#include "api/videosinkinterface.h"
#include "api/videosourceinterface.h"
#include "media/base/mediachannel.h"
#include "media/base/mediaengine.h"
#include "media/base/streamparams.h"
#include "p2p/base/dtlstransportinternal.h"
#include "p2p/base/packettransportinternal.h"
#include "p2p/client/socketmonitor.h"
#include "pc/audiomonitor.h"
#include "pc/dtlssrtptransport.h"
#include "pc/mediamonitor.h"
#include "pc/mediasession.h"
#include "pc/rtcpmuxfilter.h"
#include "pc/rtptransport.h"
#include "pc/srtpfilter.h"
#include "pc/srtptransport.h"
#include "pc/transportcontroller.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/asyncudpsocket.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/network.h"
#include "rtc_base/sigslot.h"

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
              std::unique_ptr<MediaChannel> media_channel,
              const std::string& content_name,
              bool rtcp_mux_required,
              bool srtp_required);
  virtual ~BaseChannel();
  // TODO(zhihuang): Remove this once the RtpTransport can be shared between
  // BaseChannels.
  void Init_w(DtlsTransportInternal* rtp_dtls_transport,
              DtlsTransportInternal* rtcp_dtls_transport,
              rtc::PacketTransportInternal* rtp_packet_transport,
              rtc::PacketTransportInternal* rtcp_packet_transport);
  void Init_w(webrtc::RtpTransportInternal* rtp_transport);

  // Deinit may be called multiple times and is simply ignored if it's already
  // done.
  void Deinit();

  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  const std::string& content_name() const { return content_name_; }
  // TODO(deadbeef): This is redundant; remove this.
  const std::string& transport_name() const { return transport_name_; }
  bool enabled() const { return enabled_; }

  // This function returns true if we are using SDES.
  bool sdes_active() const {
    return sdes_transport_ && sdes_negotiator_.IsActive();
  }
  // The following function returns true if we are using DTLS-based keying.
  bool dtls_active() const {
    return dtls_srtp_transport_ && dtls_srtp_transport_->IsActive();
  }
  // This function returns true if using SRTP (DTLS-based keying or SDES).
  bool srtp_active() const { return sdes_active() || dtls_active(); }

  bool writable() const { return writable_; }

  // Set an RTP level transport which could be an RtpTransport without
  // encryption, an SrtpTransport for SDES or a DtlsSrtpTransport for DTLS-SRTP.
  // This can be called from any thread and it hops to the network thread
  // internally. It would replace the |SetTransports| and its variants.
  void SetRtpTransport(webrtc::RtpTransportInternal* rtp_transport);

  // Set the transport(s), and update writability and "ready-to-send" state.
  // |rtp_transport| must be non-null.
  // |rtcp_transport| must be supplied if NeedsRtcpTransport() is true (meaning
  // RTCP muxing is not fully active yet).
  // |rtp_transport| and |rtcp_transport| must share the same transport name as
  // well.
  // Can not start with "rtc::PacketTransportInternal" and switch to
  // "DtlsTransportInternal", or vice-versa.
  // TODO(zhihuang): Remove these two once the RtpTransport can be shared
  // between BaseChannels.
  void SetTransports(DtlsTransportInternal* rtp_dtls_transport,
                     DtlsTransportInternal* rtcp_dtls_transport);
  void SetTransports(rtc::PacketTransportInternal* rtp_packet_transport,
                     rtc::PacketTransportInternal* rtcp_packet_transport);
  // Channel control
  bool SetLocalContent(const MediaContentDescription* content,
                       webrtc::SdpType type,
                       std::string* error_desc);
  bool SetRemoteContent(const MediaContentDescription* content,
                        webrtc::SdpType type,
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

  // From RtpTransport - public for testing only
  void OnTransportReadyToSend(bool ready);

  // Only public for unit tests.  Otherwise, consider protected.
  int SetOption(SocketType type, rtc::Socket::Option o, int val)
      override;
  int SetOption_n(SocketType type, rtc::Socket::Option o, int val);

  virtual cricket::MediaType media_type() = 0;

  // Public for testing.
  // TODO(zstein): Remove this once channels register themselves with
  // an RtpTransport in a more explicit way.
  bool HandlesPayloadType(int payload_type) const;

  // Used by the RTCStatsCollector tests to set the transport name without
  // creating RtpTransports.
  void set_transport_name_for_testing(const std::string& transport_name) {
    transport_name_ = transport_name;
  }

 protected:
  virtual MediaChannel* media_channel() const { return media_channel_.get(); }

  void SetTransports_n(DtlsTransportInternal* rtp_dtls_transport,
                       DtlsTransportInternal* rtcp_dtls_transport,
                       rtc::PacketTransportInternal* rtp_packet_transport,
                       rtc::PacketTransportInternal* rtcp_packet_transport);

  // This does not update writability or "ready-to-send" state; it just
  // disconnects from the old channel and connects to the new one.
  // TODO(zhihuang): Remove this once the RtpTransport can be shared between
  // BaseChannels.
  void SetTransport_n(bool rtcp,
                      DtlsTransportInternal* new_dtls_transport,
                      rtc::PacketTransportInternal* new_packet_transport);

  bool was_ever_writable() const { return was_ever_writable_; }
  void set_local_content_direction(webrtc::RtpTransceiverDirection direction) {
    local_content_direction_ = direction;
  }
  void set_remote_content_direction(webrtc::RtpTransceiverDirection direction) {
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

  void FlushRtcpMessages_n();

  // NetworkInterface implementation, called by MediaEngine
  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options) override;
  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options) override;

  // From RtpTransportInternal
  void OnWritableState(bool writable);

  void OnNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute> network_route);

  bool PacketIsRtcp(const rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len);
  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options);

  bool WantsPacket(bool rtcp, const rtc::CopyOnWriteBuffer* packet);
  void HandlePacket(bool rtcp, rtc::CopyOnWriteBuffer* packet,
                    const rtc::PacketTime& packet_time);
  // TODO(zstein): packet can be const once the RtpTransport handles protection.
  virtual void OnPacketReceived(bool rtcp,
                                rtc::CopyOnWriteBuffer* packet,
                                const rtc::PacketTime& packet_time);
  void ProcessPacket(bool rtcp,
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

  // Should be called whenever the conditions for
  // IsReadyToReceiveMedia/IsReadyToSendMedia are satisfied (or unsatisfied).
  // Updates the send/recv state of the media channel.
  void UpdateMediaSendRecvState();
  virtual void UpdateMediaSendRecvState_w() = 0;

  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            webrtc::SdpType type,
                            std::string* error_desc);
  bool UpdateRemoteStreams_w(const std::vector<StreamParams>& streams,
                             webrtc::SdpType type,
                             std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 webrtc::SdpType type,
                                 std::string* error_desc) = 0;
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  webrtc::SdpType type,
                                  std::string* error_desc) = 0;
  bool SetRtpTransportParameters(const MediaContentDescription* content,
                                 webrtc::SdpType type,
                                 ContentSource src,
                                 const RtpHeaderExtensions& extensions,
                                 std::string* error_desc);
  bool SetRtpTransportParameters_n(
      const MediaContentDescription* content,
      webrtc::SdpType type,
      ContentSource src,
      const std::vector<int>& encrypted_extension_ids,
      std::string* error_desc);

  // Return a list of RTP header extensions with the non-encrypted extensions
  // removed depending on the current crypto_options_ and only if both the
  // non-encrypted and encrypted extension is present for the same URI.
  RtpHeaderExtensions GetFilteredRtpHeaderExtensions(
      const RtpHeaderExtensions& extensions);

  // Helper method to get RTP Absoulute SendTime extension header id if
  // present in remote supported extensions list.
  void MaybeCacheRtpAbsSendTimeHeaderExtension_w(
      const std::vector<webrtc::RtpExtension>& extensions);

  bool CheckSrtpConfig_n(const std::vector<CryptoParams>& cryptos,
                         bool* dtls,
                         std::string* error_desc);
  bool SetSrtp_n(const std::vector<CryptoParams>& params,
                 webrtc::SdpType type,
                 ContentSource src,
                 const std::vector<int>& encrypted_extension_ids,
                 std::string* error_desc);
  bool SetRtcpMux_n(bool enable,
                    webrtc::SdpType type,
                    ContentSource src,
                    std::string* error_desc);

  // From MessageHandler
  void OnMessage(rtc::Message* pmsg) override;

  // Handled in derived classes
  virtual void OnConnectionMonitorUpdate(ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) = 0;

  // Helper function template for invoking methods on the worker thread.
  template <class T, class FunctorT>
  T InvokeOnWorker(const rtc::Location& posted_from, const FunctorT& functor) {
    return worker_thread_->Invoke<T>(posted_from, functor);
  }

  void AddHandledPayloadType(int payload_type);

 private:
  void ConnectToRtpTransport();
  void DisconnectFromRtpTransport();
  void SignalSentPacket_n(const rtc::SentPacket& sent_packet);
  void SignalSentPacket_w(const rtc::SentPacket& sent_packet);
  bool IsReadyToSendMedia_n() const;
  void CacheRtpAbsSendTimeHeaderExtension_n(int rtp_abs_sendtime_extn_id);
  // Wraps the existing RtpTransport in an SrtpTransport.
  void EnableSdes_n();

  // Wraps the existing RtpTransport in a new SrtpTransport and wraps that in a
  // new DtlsSrtpTransport.
  void EnableDtlsSrtp_n();

  // Update the encrypted header extension IDs when setting the local/remote
  // description and use them later together with other crypto parameters from
  // DtlsTransport. If DTLS-SRTP is enabled, it also update the encrypted header
  // extension IDs for DtlsSrtpTransport.
  void UpdateEncryptedHeaderExtensionIds(cricket::ContentSource source,
                                         const std::vector<int>& extension_ids);

  // Permanently enable RTCP muxing. Set null RTCP PacketTransport for
  // BaseChannel and RtpTransport. If using DTLS-SRTP, set null DtlsTransport
  // for DtlsSrtpTransport.
  void ActivateRtcpMux();

  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const signaling_thread_;
  rtc::AsyncInvoker invoker_;

  const std::string content_name_;
  std::unique_ptr<ConnectionMonitor> connection_monitor_;

  // Won't be set when using raw packet transports. SDP-specific thing.
  std::string transport_name_;

  const bool rtcp_mux_required_;

  // Separate DTLS/non-DTLS pointers to support using BaseChannel without DTLS.
  // Temporary measure until more refactoring is done.
  // If non-null, "X_dtls_transport_" will always equal "X_packet_transport_".
  DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;

  webrtc::RtpTransportInternal* rtp_transport_ = nullptr;
  // Only one of these transports is non-null at a time. One for DTLS-SRTP, one
  // for SDES and one for unencrypted RTP.
  std::unique_ptr<webrtc::SrtpTransport> sdes_transport_;
  std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport_;
  std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport_;

  std::vector<std::pair<rtc::Socket::Option, int> > socket_options_;
  std::vector<std::pair<rtc::Socket::Option, int> > rtcp_socket_options_;
  SrtpFilter sdes_negotiator_;
  RtcpMuxFilter rtcp_mux_filter_;
  bool writable_ = false;
  bool was_ever_writable_ = false;
  bool has_received_packet_ = false;
  const bool srtp_required_ = true;

  // MediaChannel related members that should be accessed from the worker
  // thread.
  std::unique_ptr<MediaChannel> media_channel_;
  // Currently the |enabled_| flag is accessed from the signaling thread as
  // well, but it can be changed only when signaling thread does a synchronous
  // call to the worker thread, so it should be safe.
  bool enabled_ = false;
  std::vector<StreamParams> local_streams_;
  std::vector<StreamParams> remote_streams_;
  webrtc::RtpTransceiverDirection local_content_direction_ =
      webrtc::RtpTransceiverDirection::kInactive;
  webrtc::RtpTransceiverDirection remote_content_direction_ =
      webrtc::RtpTransceiverDirection::kInactive;

  // The cached encrypted header extension IDs.
  rtc::Optional<std::vector<int>> cached_send_extension_ids_;
  rtc::Optional<std::vector<int>> cached_recv_extension_ids_;
};

// VoiceChannel is a specialization that adds support for early media, DTMF,
// and input/output level monitoring.
class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(rtc::Thread* worker_thread,
               rtc::Thread* network_thread,
               rtc::Thread* signaling_thread,
               MediaEngineInterface* media_engine,
               std::unique_ptr<VoiceMediaChannel> channel,
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
  cricket::MediaType media_type() override { return cricket::MEDIA_TYPE_AUDIO; }

 private:
  // overrides from BaseChannel
  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void UpdateMediaSendRecvState_w() override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;
  void HandleEarlyMediaTimeout();

  void OnMessage(rtc::Message* pmsg) override;
  void OnConnectionMonitorUpdate(
      ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) override;
  void OnMediaMonitorUpdate(VoiceMediaChannel* media_channel,
                            const VoiceMediaInfo& info);
  void OnAudioMonitorUpdate(AudioMonitor* monitor, const AudioInfo& info);

  static const int kEarlyMediaTimeout = 1000;
  MediaEngineInterface* media_engine_;
  bool received_media_ = false;
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
               std::unique_ptr<VideoMediaChannel> media_channel,
               const std::string& content_name,
               bool rtcp_mux_required,
               bool srtp_required);
  ~VideoChannel();

  // downcasts a MediaChannel
  VideoMediaChannel* media_channel() const override {
    return static_cast<VideoMediaChannel*>(BaseChannel::media_channel());
  }

  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info);
  // Get statistics about the current media session.
  bool GetStats(VideoMediaInfo* stats);

  sigslot::signal2<VideoChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VideoChannel*, const VideoMediaInfo&> SignalMediaMonitor;

  cricket::MediaType media_type() override { return cricket::MEDIA_TYPE_VIDEO; }

 private:
  // overrides from BaseChannel
  void UpdateMediaSendRecvState_w() override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;
  bool GetStats_w(VideoMediaInfo* stats);

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
                 std::unique_ptr<DataMediaChannel> channel,
                 const std::string& content_name,
                 bool rtcp_mux_required,
                 bool srtp_required);
  ~RtpDataChannel();
  // TODO(zhihuang): Remove this once the RtpTransport can be shared between
  // BaseChannels.
  void Init_w(DtlsTransportInternal* rtp_dtls_transport,
              DtlsTransportInternal* rtcp_dtls_transport,
              rtc::PacketTransportInternal* rtp_packet_transport,
              rtc::PacketTransportInternal* rtcp_packet_transport);
  void Init_w(webrtc::RtpTransportInternal* rtp_transport);

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
  // Checks that data channel type is RTP.
  bool CheckDataChannelTypeFromContent(const DataContentDescription* content,
                                       std::string* error_desc);
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;
  void UpdateMediaSendRecvState_w() override;

  void OnMessage(rtc::Message* pmsg) override;
  void OnConnectionMonitorUpdate(
      ConnectionMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) override;
  void OnMediaMonitorUpdate(DataMediaChannel* media_channel,
                            const DataMediaInfo& info);
  void OnDataReceived(
      const ReceiveDataParams& params, const char* data, size_t len);
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

#endif  // PC_CHANNEL_H_
