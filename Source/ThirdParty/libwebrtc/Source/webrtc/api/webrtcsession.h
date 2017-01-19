/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_WEBRTCSESSION_H_
#define WEBRTC_API_WEBRTCSESSION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "webrtc/api/datachannel.h"
#include "webrtc/api/dtmfsender.h"
#include "webrtc/api/mediacontroller.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/statstypes.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/pc/mediasession.h"

#ifdef HAVE_QUIC
#include "webrtc/api/quicdatatransport.h"
#endif  // HAVE_QUIC

namespace cricket {

class ChannelManager;
class DataChannel;
class StatsReport;
class VideoChannel;
class VoiceChannel;

#ifdef HAVE_QUIC
class QuicTransportChannel;
#endif  // HAVE_QUIC

}  // namespace cricket

namespace webrtc {

class IceRestartAnswerLatch;
class JsepIceCandidate;
class MediaStreamSignaling;
class WebRtcSessionDescriptionFactory;

extern const char kBundleWithoutRtcpMux[];
extern const char kCreateChannelFailed[];
extern const char kInvalidCandidates[];
extern const char kInvalidSdp[];
extern const char kMlineMismatch[];
extern const char kPushDownTDFailed[];
extern const char kSdpWithoutDtlsFingerprint[];
extern const char kSdpWithoutSdesCrypto[];
extern const char kSdpWithoutIceUfragPwd[];
extern const char kSdpWithoutSdesAndDtlsDisabled[];
extern const char kSessionError[];
extern const char kSessionErrorDesc[];
extern const char kDtlsSetupFailureRtp[];
extern const char kDtlsSetupFailureRtcp[];
extern const char kEnableBundleFailed[];

// Maximum number of received video streams that will be processed by webrtc
// even if they are not signalled beforehand.
extern const int kMaxUnsignalledRecvStreams;

// ICE state callback interface.
class IceObserver {
 public:
  IceObserver() {}
  // Called any time the IceConnectionState changes
  // TODO(honghaiz): Change the name to OnIceConnectionStateChange so as to
  // conform to the w3c standard.
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}
  // Called any time the IceGatheringState changes
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {}
  // New Ice candidate have been found.
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;

  // Some local ICE candidates have been removed.
  virtual void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) = 0;

  // Called whenever the state changes between receiving and not receiving.
  virtual void OnIceConnectionReceivingChange(bool receiving) {}

 protected:
  ~IceObserver() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(IceObserver);
};

// Statistics for all the transports of the session.
typedef std::map<std::string, cricket::TransportStats> TransportStatsMap;
typedef std::map<std::string, std::string> ProxyTransportMap;

// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
struct SessionStats {
  ProxyTransportMap proxy_to_transport;
  TransportStatsMap transport_stats;
};

// A WebRtcSession manages general session state. This includes negotiation
// of both the application-level and network-level protocols:  the former
// defines what will be sent and the latter defines how it will be sent.  Each
// network-level protocol is represented by a Transport object.  Each Transport
// participates in the network-level negotiation.  The individual streams of
// packets are represented by TransportChannels.  The application-level protocol
// is represented by SessionDecription objects.
class WebRtcSession :

    public DtmfProviderInterface,
    public DataChannelProviderInterface,
    public sigslot::has_slots<> {
 public:
  enum State {
    STATE_INIT = 0,
    STATE_SENTOFFER,         // Sent offer, waiting for answer.
    STATE_RECEIVEDOFFER,     // Received an offer. Need to send answer.
    STATE_SENTPRANSWER,      // Sent provisional answer. Need to send answer.
    STATE_RECEIVEDPRANSWER,  // Received provisional answer, waiting for answer.
    STATE_INPROGRESS,        // Offer/answer exchange completed.
    STATE_CLOSED,            // Close() was called.
  };

  enum Error {
    ERROR_NONE = 0,       // no error
    ERROR_CONTENT = 1,    // channel errors in SetLocalContent/SetRemoteContent
    ERROR_TRANSPORT = 2,  // transport error of some kind
  };

  WebRtcSession(
      webrtc::MediaControllerInterface* media_controller,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread,
      rtc::Thread* signaling_thread,
      cricket::PortAllocator* port_allocator,
      std::unique_ptr<cricket::TransportController> transport_controller);
  virtual ~WebRtcSession();

  // These are const to allow them to be called from const methods.
  rtc::Thread* network_thread() const { return network_thread_; }
  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* signaling_thread() const { return signaling_thread_; }

  // The ID of this session.
  const std::string& id() const { return sid_; }

  bool Initialize(
      const PeerConnectionFactoryInterface::Options& options,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      const PeerConnectionInterface::RTCConfiguration& rtc_configuration);
  // Deletes the voice, video and data channel and changes the session state
  // to STATE_CLOSED.
  void Close();

  // Returns true if we were the initial offerer.
  bool initial_offerer() const { return initial_offerer_; }

  // Returns the current state of the session. See the enum above for details.
  // Each time the state changes, we will fire this signal.
  State state() const { return state_; }
  sigslot::signal2<WebRtcSession*, State> SignalState;

  // Returns the last error in the session. See the enum above for details.
  Error error() const { return error_; }
  const std::string& error_desc() const { return error_desc_; }

  void RegisterIceObserver(IceObserver* observer) {
    ice_observer_ = observer;
  }

  virtual cricket::VoiceChannel* voice_channel() {
    return voice_channel_.get();
  }
  virtual cricket::VideoChannel* video_channel() {
    return video_channel_.get();
  }
  virtual cricket::DataChannel* data_channel() {
    return data_channel_.get();
  }

  cricket::BaseChannel* GetChannel(const std::string& content_name);

  void SetSdesPolicy(cricket::SecurePolicy secure_policy);
  cricket::SecurePolicy SdesPolicy() const;

  // Get current ssl role from transport.
  bool GetSslRole(const std::string& transport_name, rtc::SSLRole* role);

  // Get current SSL role for this channel's transport.
  // If |transport| is null, returns false.
  bool GetSslRole(const cricket::BaseChannel* channel, rtc::SSLRole* role);

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      const cricket::MediaSessionOptions& session_options);
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const cricket::MediaSessionOptions& session_options);
  // The ownership of |desc| will be transferred after this call.
  bool SetLocalDescription(SessionDescriptionInterface* desc,
                           std::string* err_desc);
  // The ownership of |desc| will be transferred after this call.
  bool SetRemoteDescription(SessionDescriptionInterface* desc,
                            std::string* err_desc);
  bool ProcessIceMessage(const IceCandidateInterface* ice_candidate);

  bool RemoveRemoteIceCandidates(
      const std::vector<cricket::Candidate>& candidates);

  cricket::IceConfig ParseIceConfig(
      const PeerConnectionInterface::RTCConfiguration& config) const;

  void SetIceConfig(const cricket::IceConfig& ice_config);

  // Start gathering candidates for any new transports, or transports doing an
  // ICE restart.
  void MaybeStartGathering();

  const SessionDescriptionInterface* local_description() const {
    return local_desc_.get();
  }
  const SessionDescriptionInterface* remote_description() const {
    return remote_desc_.get();
  }

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id);
  virtual bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id);

  // Implements DtmfProviderInterface.
  bool CanInsertDtmf(const std::string& track_id) override;
  bool InsertDtmf(const std::string& track_id,
                  int code, int duration) override;
  sigslot::signal0<>* GetOnDestroyedSignal() override;

  // Implements DataChannelProviderInterface.
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result) override;
  bool ConnectDataChannel(DataChannel* webrtc_data_channel) override;
  void DisconnectDataChannel(DataChannel* webrtc_data_channel) override;
  void AddSctpDataStream(int sid) override;
  void RemoveSctpDataStream(int sid) override;
  bool ReadyToSendData() const override;

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  virtual bool GetTransportStats(SessionStats* stats);

  // Get stats for a specific channel
  bool GetChannelTransportStats(cricket::BaseChannel* ch, SessionStats* stats);

  // virtual so it can be mocked in unit tests
  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate);

  // Caller owns returned certificate
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name);

  cricket::DataChannelType data_channel_type() const;

  bool IceRestartPending(const std::string& content_name) const;

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  void OnDtlsSetupFailure(cricket::BaseChannel*, bool rtcp);

  // For unit test.
  bool waiting_for_certificate_for_testing() const;
  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate_for_testing();

  void set_metrics_observer(
      webrtc::MetricsObserverInterface* metrics_observer) {
    metrics_observer_ = metrics_observer;
    transport_controller_->SetMetricsObserver(metrics_observer);
  }

  // Called when voice_channel_, video_channel_ and data_channel_ are created
  // and destroyed. As a result of, for example, setting a new description.
  sigslot::signal0<> SignalVoiceChannelCreated;
  sigslot::signal0<> SignalVoiceChannelDestroyed;
  sigslot::signal0<> SignalVideoChannelCreated;
  sigslot::signal0<> SignalVideoChannelDestroyed;
  sigslot::signal0<> SignalDataChannelCreated;
  sigslot::signal0<> SignalDataChannelDestroyed;
  // Called when the whole session is destroyed.
  sigslot::signal0<> SignalDestroyed;

  // Called when a valid data channel OPEN message is received.
  // std::string represents the data channel label.
  sigslot::signal2<const std::string&, const InternalDataChannelInit&>
      SignalDataChannelOpenMessage;
#ifdef HAVE_QUIC
  QuicDataTransport* quic_data_transport() {
    return quic_data_transport_.get();
  }
#endif  // HAVE_QUIC

 private:
  // Indicates the type of SessionDescription in a call to SetLocalDescription
  // and SetRemoteDescription.
  enum Action {
    kOffer,
    kPrAnswer,
    kAnswer,
  };

  // Log session state.
  void LogState(State old_state, State new_state);

  // Updates the state, signaling if necessary.
  virtual void SetState(State state);

  // Updates the error state, signaling if necessary.
  // TODO(ronghuawu): remove the SetError method that doesn't take |error_desc|.
  virtual void SetError(Error error, const std::string& error_desc);

  bool UpdateSessionState(Action action, cricket::ContentSource source,
                          std::string* err_desc);
  static Action GetAction(const std::string& type);
  // Push the media parts of the local or remote session description
  // down to all of the channels.
  bool PushdownMediaDescription(cricket::ContentAction action,
                                cricket::ContentSource source,
                                std::string* error_desc);

  bool PushdownTransportDescription(cricket::ContentSource source,
                                    cricket::ContentAction action,
                                    std::string* error_desc);

  // Helper methods to push local and remote transport descriptions.
  bool PushdownLocalTransportDescription(
      const cricket::SessionDescription* sdesc,
      cricket::ContentAction action,
      std::string* error_desc);
  bool PushdownRemoteTransportDescription(
      const cricket::SessionDescription* sdesc,
      cricket::ContentAction action,
      std::string* error_desc);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(
      const cricket::SessionDescription* description,
      const std::string& content_name,
      cricket::TransportDescription* info);

  // Returns the name of the transport channel when BUNDLE is enabled, or
  // nullptr if the channel is not part of any bundle.
  const std::string* GetBundleTransportName(
      const cricket::ContentInfo* content,
      const cricket::ContentGroup* bundle);

  // Cause all the BaseChannels in the bundle group to have the same
  // transport channel.
  bool EnableBundle(const cricket::ContentGroup& bundle);

  // Enables media channels to allow sending of media.
  void EnableChannels();
  // Returns the media index for a local ice candidate given the content name.
  // Returns false if the local session description does not have a media
  // content called  |content_name|.
  bool GetLocalCandidateMediaIndex(const std::string& content_name,
                                   int* sdp_mline_index);
  // Uses all remote candidates in |remote_desc| in this session.
  bool UseCandidatesInSessionDescription(
      const SessionDescriptionInterface* remote_desc);
  // Uses |candidate| in this session.
  bool UseCandidate(const IceCandidateInterface* candidate);
  // Deletes the corresponding channel of contents that don't exist in |desc|.
  // |desc| can be null. This means that all channels are deleted.
  void RemoveUnusedChannels(const cricket::SessionDescription* desc);

  // Allocates media channels based on the |desc|. If |desc| doesn't have
  // the BUNDLE option, this method will disable BUNDLE in PortAllocator.
  // This method will also delete any existing media channels before creating.
  bool CreateChannels(const cricket::SessionDescription* desc);

  // Helper methods to create media channels.
  bool CreateVoiceChannel(const cricket::ContentInfo* content,
                          const std::string* bundle_transport);
  bool CreateVideoChannel(const cricket::ContentInfo* content,
                          const std::string* bundle_transport);
  bool CreateDataChannel(const cricket::ContentInfo* content,
                         const std::string* bundle_transport);

  // Listens to SCTP CONTROL messages on unused SIDs and process them as OPEN
  // messages.
  void OnDataChannelMessageReceived(cricket::DataChannel* channel,
                                    const cricket::ReceiveDataParams& params,
                                    const rtc::CopyOnWriteBuffer& payload);

  std::string BadStateErrMsg(State state);
  void SetIceConnectionState(PeerConnectionInterface::IceConnectionState state);
  void SetIceConnectionReceiving(bool receiving);

  bool ValidateBundleSettings(const cricket::SessionDescription* desc);
  bool HasRtcpMuxEnabled(const cricket::ContentInfo* content);
  // Below methods are helper methods which verifies SDP.
  bool ValidateSessionDescription(const SessionDescriptionInterface* sdesc,
                                  cricket::ContentSource source,
                                  std::string* err_desc);

  // Check if a call to SetLocalDescription is acceptable with |action|.
  bool ExpectSetLocalDescription(Action action);
  // Check if a call to SetRemoteDescription is acceptable with |action|.
  bool ExpectSetRemoteDescription(Action action);
  // Verifies a=setup attribute as per RFC 5763.
  bool ValidateDtlsSetupAttribute(const cricket::SessionDescription* desc,
                                  Action action);

  // Returns true if we are ready to push down the remote candidate.
  // |remote_desc| is the new remote description, or NULL if the current remote
  // description should be used. Output |valid| is true if the candidate media
  // index is valid.
  bool ReadyToUseRemoteCandidate(const IceCandidateInterface* candidate,
                                 const SessionDescriptionInterface* remote_desc,
                                 bool* valid);

  void OnTransportControllerConnectionState(cricket::IceConnectionState state);
  void OnTransportControllerReceiving(bool receiving);
  void OnTransportControllerGatheringState(cricket::IceGatheringState state);
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);

  std::string GetSessionErrorMsg();

  // Invoked when TransportController connection completion is signaled.
  // Reports stats for all transports in use.
  void ReportTransportStats();

  // Gather the usage of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats);

  void OnSentPacket_w(const rtc::SentPacket& sent_packet);

  const std::string GetTransportName(const std::string& content_name);

  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const signaling_thread_;

  State state_ = STATE_INIT;
  Error error_ = ERROR_NONE;
  std::string error_desc_;

  const std::string sid_;
  bool initial_offerer_ = false;

  std::unique_ptr<cricket::TransportController> transport_controller_;
  MediaControllerInterface* media_controller_;
  std::unique_ptr<cricket::VoiceChannel> voice_channel_;
  std::unique_ptr<cricket::VideoChannel> video_channel_;
  std::unique_ptr<cricket::DataChannel> data_channel_;
  cricket::ChannelManager* channel_manager_;
  IceObserver* ice_observer_;
  PeerConnectionInterface::IceConnectionState ice_connection_state_;
  bool ice_connection_receiving_;
  std::unique_ptr<SessionDescriptionInterface> local_desc_;
  std::unique_ptr<SessionDescriptionInterface> remote_desc_;
  // If the remote peer is using a older version of implementation.
  bool older_version_remote_peer_;
  bool dtls_enabled_;
  // Specifies which kind of data channel is allowed. This is controlled
  // by the chrome command-line flag and constraints:
  // 1. If chrome command-line switch 'enable-sctp-data-channels' is enabled,
  // constraint kEnableDtlsSrtp is true, and constaint kEnableRtpDataChannels is
  // not set or false, SCTP is allowed (DCT_SCTP);
  // 2. If constraint kEnableRtpDataChannels is true, RTP is allowed (DCT_RTP);
  // 3. If both 1&2 are false, data channel is not allowed (DCT_NONE).
  // The data channel type could be DCT_QUIC if the QUIC data channel is
  // enabled.
  cricket::DataChannelType data_channel_type_;
  // List of content names for which the remote side triggered an ICE restart.
  std::set<std::string> pending_ice_restarts_;

  std::unique_ptr<WebRtcSessionDescriptionFactory> webrtc_session_desc_factory_;

  // Member variables for caching global options.
  cricket::AudioOptions audio_options_;
  cricket::VideoOptions video_options_;
  MetricsObserverInterface* metrics_observer_;

  // Declares the bundle policy for the WebRTCSession.
  PeerConnectionInterface::BundlePolicy bundle_policy_;

  // Declares the RTCP mux policy for the WebRTCSession.
  PeerConnectionInterface::RtcpMuxPolicy rtcp_mux_policy_;

  bool received_first_video_packet_ = false;
  bool received_first_audio_packet_ = false;

#ifdef HAVE_QUIC
  std::unique_ptr<QuicDataTransport> quic_data_transport_;
#endif  // HAVE_QUIC

  RTC_DISALLOW_COPY_AND_ASSIGN(WebRtcSession);
};
}  // namespace webrtc

#endif  // WEBRTC_API_WEBRTCSESSION_H_
