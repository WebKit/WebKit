/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEERCONNECTION_H_
#define PC_PEERCONNECTION_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "api/turncustomizer.h"
#include "pc/iceserverparsing.h"
#include "pc/peerconnectionfactory.h"
#include "pc/rtcstatscollector.h"
#include "pc/rtptransceiver.h"
#include "pc/statscollector.h"
#include "pc/streamcollection.h"
#include "pc/webrtcsessiondescriptionfactory.h"

namespace webrtc {

class MediaStreamObserver;
class VideoRtpReceiver;
class RtcEventLog;

// Statistics for all the transports of the session.
// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
struct SessionStats {
  std::map<std::string, std::string> proxy_to_transport;
  std::map<std::string, cricket::TransportStats> transport_stats;
};

struct ChannelNamePair {
  ChannelNamePair(const std::string& content_name,
                  const std::string& transport_name)
      : content_name(content_name), transport_name(transport_name) {}
  std::string content_name;
  std::string transport_name;
};

struct ChannelNamePairs {
  rtc::Optional<ChannelNamePair> voice;
  rtc::Optional<ChannelNamePair> video;
  rtc::Optional<ChannelNamePair> data;
};

// PeerConnection is the implementation of the PeerConnection object as defined
// by the PeerConnectionInterface API surface.
// The class currently is solely responsible for the following:
// - Managing the session state machine (signaling state).
// - Creating and initializing lower-level objects, like PortAllocator and
//   BaseChannels.
// - Owning and managing the life cycle of the RtpSender/RtpReceiver and track
//   objects.
// - Tracking the current and pending local/remote session descriptions.
// The class currently is jointly responsible for the following:
// - Parsing and interpreting SDP.
// - Generating offers and answers based on the current state.
// - The ICE state machine.
// - Generating stats.
class PeerConnection : public PeerConnectionInterface,
                       public DataChannelProviderInterface,
                       public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  explicit PeerConnection(PeerConnectionFactory* factory,
                          std::unique_ptr<RtcEventLog> event_log,
                          std::unique_ptr<Call> call);

  bool Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      MediaStreamTrackInterface* track,
      std::vector<MediaStreamInterface*> streams) override;
  bool RemoveTrack(RtpSenderInterface* sender) override;

  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init) override;

  // Gets the DTLS SSL certificate associated with the audio transport on the
  // remote side. This will become populated once the DTLS connection with the
  // peer has been completed, as indicated by the ICE connection state
  // transitioning to kIceConnectionCompleted.
  // Note that this will be removed once we implement RTCDtlsTransport which
  // has standardized method for getting this information.
  // See https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface
  std::unique_ptr<rtc::SSLCertificate> GetRemoteAudioSSLCertificate();

  rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) override;

  rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) override;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> GetTransceivers()
      const override;

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) override;
  bool GetStats(StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;
  void GetStats(RTCStatsCollectorCallback* callback) override;

  SignalingState signaling_state() override;

  IceConnectionState ice_connection_state() override;
  IceGatheringState ice_gathering_state() override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;
  const SessionDescriptionInterface* current_local_description() const override;
  const SessionDescriptionInterface* current_remote_description()
      const override;
  const SessionDescriptionInterface* pending_local_description() const override;
  const SessionDescriptionInterface* pending_remote_description()
      const override;

  // JSEP01
  // Deprecated, use version without constraints.
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const MediaConstraintsInterface* constraints) override;
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  // Deprecated, use version without constraints.
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints) override;
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override;
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override;
  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer)
      override;
  PeerConnectionInterface::RTCConfiguration GetConfiguration() override;
  bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      RTCError* error) override;
  bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration) override {
    return SetConfiguration(configuration, nullptr);
  }
  bool AddIceCandidate(const IceCandidateInterface* candidate) override;
  bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) override;

  void RegisterUMAObserver(UMAObserver* observer) override;

  RTCError SetBitrate(const BitrateParameters& bitrate) override;

  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override;

  void SetAudioPlayout(bool playout) override;
  void SetAudioRecording(bool recording) override;

  RTC_DEPRECATED bool StartRtcEventLog(rtc::PlatformFile file,
                                       int64_t max_size_bytes) override;
  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                        int64_t output_period_ms) override;
  void StopRtcEventLog() override;

  void Close() override;

  sigslot::signal1<DataChannel*> SignalDataChannelCreated;

  // Virtual for unit tests.
  virtual const std::vector<rtc::scoped_refptr<DataChannel>>&
  sctp_data_channels() const {
    return sctp_data_channels_;
  }

  rtc::Thread* network_thread() const { return factory_->network_thread(); }
  rtc::Thread* worker_thread() const { return factory_->worker_thread(); }
  rtc::Thread* signaling_thread() const { return factory_->signaling_thread(); }

  // The SDP session ID as defined by RFC 3264.
  virtual const std::string& session_id() const { return session_id_; }

  // Returns true if we were the initial offerer.
  bool initial_offerer() const { return initial_offerer_ && *initial_offerer_; }

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  // The parameterless version creates |ChannelNamePairs| from |voice_channel|,
  // |video_channel| and |voice_channel| if available - this requires it to be
  // called on the signaling thread - and invokes the other |GetStats|. The
  // other |GetStats| can be invoked on any thread; if not invoked on the
  // network thread a thread hop will happen.
  std::unique_ptr<SessionStats> GetSessionStats_s();
  virtual std::unique_ptr<SessionStats> GetSessionStats(
      const ChannelNamePairs& channel_name_pairs);

  // virtual so it can be mocked in unit tests
  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate);
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name);

  virtual Call::Stats GetCallStats();

  // Exposed for stats collecting.
  // TODO(steveanton): Switch callers to use the plural form and remove these.
  virtual cricket::VoiceChannel* voice_channel() const {
    return static_cast<cricket::VoiceChannel*>(
        GetAudioTransceiver()->internal()->channel());
  }
  virtual cricket::VideoChannel* video_channel() const {
    return static_cast<cricket::VideoChannel*>(
        GetVideoTransceiver()->internal()->channel());
  }

  // Only valid when using deprecated RTP data channels.
  virtual cricket::RtpDataChannel* rtp_data_channel() {
    return rtp_data_channel_;
  }
  virtual rtc::Optional<std::string> sctp_content_name() const {
    return sctp_content_name_;
  }
  virtual rtc::Optional<std::string> sctp_transport_name() const {
    return sctp_transport_name_;
  }

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id);
  virtual bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id);

  // Returns true if there was an ICE restart initiated by the remote offer.
  bool IceRestartPending(const std::string& content_name) const;

  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password). If the transport has been deleted as a result of
  // bundling, returns false.
  bool NeedsIceRestart(const std::string& content_name) const;

  // Get SSL role for an arbitrary m= section (handles bundling correctly).
  // TODO(deadbeef): This is only used internally by the session description
  // factory, it shouldn't really be public).
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role);

  enum Error {
    ERROR_NONE = 0,       // no error
    ERROR_CONTENT = 1,    // channel errors in SetLocalContent/SetRemoteContent
    ERROR_TRANSPORT = 2,  // transport error of some kind
  };

 protected:
  ~PeerConnection() override;

 private:
  class SetRemoteDescriptionObserverAdapter;
  friend class SetRemoteDescriptionObserverAdapter;

  struct RtpSenderInfo {
    RtpSenderInfo() : first_ssrc(0) {}
    RtpSenderInfo(const std::string& stream_label,
                  const std::string sender_id,
                  uint32_t ssrc)
        : stream_label(stream_label), sender_id(sender_id), first_ssrc(ssrc) {}
    bool operator==(const RtpSenderInfo& other) {
      return this->stream_label == other.stream_label &&
             this->sender_id == other.sender_id &&
             this->first_ssrc == other.first_ssrc;
    }
    std::string stream_label;
    std::string sender_id;
    // An RtpSender can have many SSRCs. The first one is used as a sort of ID
    // for communicating with the lower layers.
    uint32_t first_ssrc;
  };

  // Implements MessageHandler.
  void OnMessage(rtc::Message* msg) override;

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  GetSendersInternal() const;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  GetReceiversInternal() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAudioTransceiver() const;
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetVideoTransceiver() const;

  void CreateAudioReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info);

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info);
  rtc::scoped_refptr<RtpReceiverInterface> RemoveAndStopReceiver(
      const RtpSenderInfo& remote_sender_info);

  // May be called either by AddStream/RemoveStream, or when a track is
  // added/removed from a stream previously added via AddStream.
  void AddAudioTrack(AudioTrackInterface* track, MediaStreamInterface* stream);
  void RemoveAudioTrack(AudioTrackInterface* track,
                        MediaStreamInterface* stream);
  void AddVideoTrack(VideoTrackInterface* track, MediaStreamInterface* stream);
  void RemoveVideoTrack(VideoTrackInterface* track,
                        MediaStreamInterface* stream);

  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init);

  void SetIceConnectionState(IceConnectionState new_state);
  // Called any time the IceGatheringState changes
  void OnIceGatheringChange(IceGatheringState new_state);
  // New ICE candidate has been gathered.
  void OnIceCandidate(std::unique_ptr<IceCandidateInterface> candidate);
  // Some local ICE candidates have been removed.
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);

  // Update the state, signaling if necessary.
  void ChangeSignalingState(SignalingState signaling_state);

  // Signals from MediaStreamObserver.
  void OnAudioTrackAdded(AudioTrackInterface* track,
                         MediaStreamInterface* stream);
  void OnAudioTrackRemoved(AudioTrackInterface* track,
                           MediaStreamInterface* stream);
  void OnVideoTrackAdded(VideoTrackInterface* track,
                         MediaStreamInterface* stream);
  void OnVideoTrackRemoved(VideoTrackInterface* track,
                           MediaStreamInterface* stream);

  void PostSetSessionDescriptionSuccess(
      SetSessionDescriptionObserver* observer);
  void PostSetSessionDescriptionFailure(SetSessionDescriptionObserver* observer,
                                        const std::string& error);
  void PostCreateSessionDescriptionFailure(
      CreateSessionDescriptionObserver* observer,
      const std::string& error);

  bool IsClosed() const {
    return signaling_state_ == PeerConnectionInterface::kClosed;
  }

  // Returns a MediaSessionOptions struct with options decided by |options|,
  // the local MediaStreams and DataChannels.
  void GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  void GetOptionsForAnswer(const RTCOfferAnswerOptions& options,
                           cricket::MediaSessionOptions* session_options);

  // Generates MediaDescriptionOptions for the |session_opts| based on existing
  // local description or remote description.
  void GenerateMediaDescriptionOptions(
      const SessionDescriptionInterface* session_desc,
      RtpTransceiverDirection audio_direction,
      RtpTransceiverDirection video_direction,
      rtc::Optional<size_t>* audio_index,
      rtc::Optional<size_t>* video_index,
      rtc::Optional<size_t>* data_index,
      cricket::MediaSessionOptions* session_options);

  // Remove all local and remote senders of type |media_type|.
  // Called when a media type is rejected (m-line set to port 0).
  void RemoveSenders(cricket::MediaType media_type);

  // Makes sure a MediaStreamTrack is created for each StreamParam in |streams|,
  // and existing MediaStreamTracks are removed if there is no corresponding
  // StreamParam. If |default_track_needed| is true, a default MediaStreamTrack
  // is created if it doesn't exist; if false, it's removed if it exists.
  // |media_type| is the type of the |streams| and can be either audio or video.
  // If a new MediaStream is created it is added to |new_streams|.
  void UpdateRemoteSendersList(
      const std::vector<cricket::StreamParams>& streams,
      bool default_track_needed,
      cricket::MediaType media_type,
      StreamCollection* new_streams);

  // Triggered when a remote sender has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers CreateAudioReceiver or CreateVideoReceiver.
  void OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                           cricket::MediaType media_type);

  // Triggered when a remote sender has been removed from a remote session
  // description. It removes the remote sender with id |sender_id| from a remote
  // MediaStream and triggers DestroyAudioReceiver or DestroyVideoReceiver.
  void OnRemoteSenderRemoved(const RtpSenderInfo& sender_info,
                             cricket::MediaType media_type);

  // Finds remote MediaStreams without any tracks and removes them from
  // |remote_streams_| and notifies the observer that the MediaStreams no longer
  // exist.
  void UpdateEndedRemoteMediaStreams();

  // Loops through the vector of |streams| and finds added and removed
  // StreamParams since last time this method was called.
  // For each new or removed StreamParam, OnLocalSenderSeen or
  // OnLocalSenderRemoved is invoked.
  void UpdateLocalSenders(const std::vector<cricket::StreamParams>& streams,
                          cricket::MediaType media_type);

  // Triggered when a local sender has been seen for the first time in a local
  // session description.
  // This method triggers CreateAudioSender or CreateVideoSender if the rtp
  // streams in the local SessionDescription can be mapped to a MediaStreamTrack
  // in a MediaStream in |local_streams_|
  void OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                          cricket::MediaType media_type);

  // Triggered when a local sender has been removed from a local session
  // description.
  // This method triggers DestroyAudioSender or DestroyVideoSender if a stream
  // has been removed from the local SessionDescription and the stream can be
  // mapped to a MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalSenderRemoved(const RtpSenderInfo& sender_info,
                            cricket::MediaType media_type);

  void UpdateLocalRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateRemoteRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateClosingRtpDataChannels(
      const std::vector<std::string>& active_channels,
      bool is_local_update);
  void CreateRemoteRtpDataChannel(const std::string& label,
                                  uint32_t remote_ssrc);

  // Creates channel and adds it to the collection of DataChannels that will
  // be offered in a SessionDescription.
  rtc::scoped_refptr<DataChannel> InternalCreateDataChannel(
      const std::string& label,
      const InternalDataChannelInit* config);

  // Checks if any data channel has been added.
  bool HasDataChannels() const;

  void AllocateSctpSids(rtc::SSLRole role);
  void OnSctpDataChannelClosed(DataChannel* channel);

  void OnDataChannelDestroyed();
  // Called when a valid data channel OPEN message is received.
  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config);

  // Returns true if the PeerConnection is configured to use Unified Plan
  // semantics for creating offers/answers and setting local/remote
  // descriptions. If this is true the RtpTransceiver API will also be available
  // to the user. If this is false, Plan B semantics are assumed.
  // TODO(bugs.webrtc.org/8530): Flip the default to be Unified Plan once
  // sufficient time has passed.
  bool IsUnifiedPlan() const {
    return configuration_.sdp_semantics == SdpSemantics::kUnifiedPlan;
  }

  // Is there an RtpSender of the given type?
  bool HasRtpSender(cricket::MediaType type) const;

  // Return the RtpSender with the given track attached.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderForTrack(MediaStreamTrackInterface* track) const;

  // Return the RtpSender with the given id, or null if none exists.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderById(const std::string& sender_id) const;

  // Return the RtpReceiver with the given id, or null if none exists.
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  FindReceiverById(const std::string& receiver_id) const;

  std::vector<RtpSenderInfo>* GetRemoteSenderInfos(
      cricket::MediaType media_type);
  std::vector<RtpSenderInfo>* GetLocalSenderInfos(
      cricket::MediaType media_type);
  const RtpSenderInfo* FindSenderInfo(const std::vector<RtpSenderInfo>& infos,
                                      const std::string& stream_label,
                                      const std::string sender_id) const;

  // Returns the specified SCTP DataChannel in sctp_data_channels_,
  // or nullptr if not found.
  DataChannel* FindDataChannelBySid(int sid) const;

  // Called when first configuring the port allocator.
  bool InitializePortAllocator_n(const RTCConfiguration& configuration);
  // Called when SetConfiguration is called to apply the supported subset
  // of the configuration on the network thread.
  bool ReconfigurePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportsType type,
      int candidate_pool_size,
      bool prune_turn_ports,
      webrtc::TurnCustomizer* turn_customizer);

  // Starts output of an RTC event log to the given output object.
  // This function should only be called from the worker thread.
  bool StartRtcEventLog_w(std::unique_ptr<RtcEventLogOutput> output,
                          int64_t output_period_ms);

  // Stops recording an RTC event log.
  // This function should only be called from the worker thread.
  void StopRtcEventLog_w();

  // Ensures the configuration doesn't have any parameters with invalid values,
  // or values that conflict with other parameters.
  //
  // Returns RTCError::OK() if there are no issues.
  RTCError ValidateConfiguration(const RTCConfiguration& config) const;

  cricket::ChannelManager* channel_manager() const;
  MetricsObserverInterface* metrics_observer() const;

  // Indicates the type of SessionDescription in a call to SetLocalDescription
  // and SetRemoteDescription.
  enum Action {
    kOffer,
    kPrAnswer,
    kAnswer,
  };

  // Returns the last error in the session. See the enum above for details.
  Error error() const { return error_; }
  const std::string& error_desc() const { return error_desc_; }

  cricket::BaseChannel* GetChannel(const std::string& content_name);

  // Get current SSL role used by SCTP's underlying transport.
  bool GetSctpSslRole(rtc::SSLRole* role);

  // Validates and takes ownership of the description, setting it as the current
  // or pending description (depending on the description's action) if it is
  // valid. Also updates ice role, candidates, creates and destroys channels.
  bool SetCurrentOrPendingLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      std::string* err_desc);
  bool SetCurrentOrPendingRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      std::string* err_desc);

  cricket::IceConfig ParseIceConfig(
      const PeerConnectionInterface::RTCConfiguration& config) const;

  // Implements DataChannelProviderInterface.
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result) override;
  bool ConnectDataChannel(DataChannel* webrtc_data_channel) override;
  void DisconnectDataChannel(DataChannel* webrtc_data_channel) override;
  void AddSctpDataStream(int sid) override;
  void RemoveSctpDataStream(int sid) override;
  bool ReadyToSendData() const override;

  cricket::DataChannelType data_channel_type() const;

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  void OnDtlsSrtpSetupFailure(cricket::BaseChannel*, bool rtcp);

  cricket::TransportController* transport_controller() const {
    return transport_controller_.get();
  }

  // Return all managed, non-null channels.
  std::vector<cricket::BaseChannel*> Channels() const;

  // Non-const versions of local_description()/remote_description(), for use
  // internally.
  SessionDescriptionInterface* mutable_local_description() {
    return pending_local_description_ ? pending_local_description_.get()
                                      : current_local_description_.get();
  }
  SessionDescriptionInterface* mutable_remote_description() {
    return pending_remote_description_ ? pending_remote_description_.get()
                                       : current_remote_description_.get();
  }

  // Updates the error state, signaling if necessary.
  void SetError(Error error, const std::string& error_desc);

  bool UpdateSessionState(Action action,
                          cricket::ContentSource source,
                          std::string* err_desc);
  Action GetAction(const std::string& type);
  // Push the media parts of the local or remote session description
  // down to all of the channels.
  bool PushdownMediaDescription(cricket::ContentAction action,
                                cricket::ContentSource source,
                                std::string* error_desc);
  bool PushdownSctpParameters_n(cricket::ContentSource source);

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

  std::unique_ptr<SessionStats> GetSessionStats_n(
      const ChannelNamePairs& channel_name_pairs);

  bool CreateSctpTransport_n(const std::string& content_name,
                             const std::string& transport_name);
  // For bundling.
  void ChangeSctpTransport_n(const std::string& transport_name);
  void DestroySctpTransport_n();
  // SctpTransport signal handlers. Needed to marshal signals from the network
  // to signaling thread.
  void OnSctpTransportReadyToSendData_n();
  // This may be called with "false" if the direction of the m= section causes
  // us to tear down the SCTP connection.
  void OnSctpTransportReadyToSendData_s(bool ready);
  void OnSctpTransportDataReceived_n(const cricket::ReceiveDataParams& params,
                                     const rtc::CopyOnWriteBuffer& payload);
  // Beyond just firing the signal to the signaling thread, listens to SCTP
  // CONTROL messages on unused SIDs and processes them as OPEN messages.
  void OnSctpTransportDataReceived_s(const cricket::ReceiveDataParams& params,
                                     const rtc::CopyOnWriteBuffer& payload);
  void OnSctpStreamClosedRemotely_n(int sid);

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

  // Returns true if SRTP (either using DTLS-SRTP or SDES) is required by
  // this session.
  bool SrtpRequired() const;

  // TransportController signal handlers.
  void OnTransportControllerConnectionState(cricket::IceConnectionState state);
  void OnTransportControllerGatheringState(cricket::IceGatheringState state);
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerDtlsHandshakeError(rtc::SSLHandshakeError error);

  std::string GetSessionErrorMsg();

  // Invoked when TransportController connection completion is signaled.
  // Reports stats for all transports in use.
  void ReportTransportStats();

  // Gather the usage of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats);

  void OnSentPacket_w(const rtc::SentPacket& sent_packet);

  const std::string GetTransportName(const std::string& content_name);

  void DestroyRtcpTransport_n(const std::string& transport_name);
  void RemoveAndDestroyVideoChannel(cricket::VideoChannel* video_channel);
  void DestroyVideoChannel(cricket::VideoChannel* video_channel);
  void RemoveAndDestroyVoiceChannel(cricket::VoiceChannel* voice_channel);
  void DestroyVoiceChannel(cricket::VoiceChannel* voice_channel);
  void DestroyDataChannel();

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  rtc::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_ = nullptr;
  UMAObserver* uma_observer_ = nullptr;

  // The EventLog needs to outlive |call_| (and any other object that uses it).
  std::unique_ptr<RtcEventLog> event_log_;

  SignalingState signaling_state_ = kStable;
  IceConnectionState ice_connection_state_ = kIceConnectionNew;
  IceGatheringState ice_gathering_state_ = kIceGatheringNew;
  PeerConnectionInterface::RTCConfiguration configuration_;

  std::unique_ptr<cricket::PortAllocator> port_allocator_;

  // One PeerConnection has only one RTCP CNAME.
  // https://tools.ietf.org/html/draft-ietf-rtcweb-rtp-usage-26#section-4.9
  std::string rtcp_cname_;

  // Streams added via AddStream.
  rtc::scoped_refptr<StreamCollection> local_streams_;
  // Streams created as a result of SetRemoteDescription.
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<std::unique_ptr<MediaStreamObserver>> stream_observers_;

  // These lists store sender info seen in local/remote descriptions.
  std::vector<RtpSenderInfo> remote_audio_sender_infos_;
  std::vector<RtpSenderInfo> remote_video_sender_infos_;
  std::vector<RtpSenderInfo> local_audio_sender_infos_;
  std::vector<RtpSenderInfo> local_video_sender_infos_;

  SctpSidAllocator sid_allocator_;
  // label -> DataChannel
  std::map<std::string, rtc::scoped_refptr<DataChannel>> rtp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_to_free_;

  bool remote_peer_supports_msid_ = false;

  std::unique_ptr<Call> call_;
  std::unique_ptr<StatsCollector> stats_;  // A pointer is passed to senders_
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_;

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      transceivers_;

  Error error_ = ERROR_NONE;
  std::string error_desc_;

  std::string session_id_;
  rtc::Optional<bool> initial_offerer_;

  std::unique_ptr<cricket::TransportController> transport_controller_;
  std::unique_ptr<cricket::SctpTransportInternalFactory> sctp_factory_;
  // |rtp_data_channel_| is used if in RTP data channel mode, |sctp_transport_|
  // when using SCTP.
  cricket::RtpDataChannel* rtp_data_channel_ = nullptr;

  std::unique_ptr<cricket::SctpTransportInternal> sctp_transport_;
  // |sctp_transport_name_| keeps track of what DTLS transport the SCTP
  // transport is using (which can change due to bundling).
  rtc::Optional<std::string> sctp_transport_name_;
  // |sctp_content_name_| is the content name (MID) in SDP.
  rtc::Optional<std::string> sctp_content_name_;
  // Value cached on signaling thread. Only updated when SctpReadyToSendData
  // fires on the signaling thread.
  bool sctp_ready_to_send_data_ = false;
  // Same as signals provided by SctpTransport, but these are guaranteed to
  // fire on the signaling thread, whereas SctpTransport fires on the networking
  // thread.
  // |sctp_invoker_| is used so that any signals queued on the signaling thread
  // from the network thread are immediately discarded if the SctpTransport is
  // destroyed (due to m= section being rejected).
  // TODO(deadbeef): Use a proxy object to ensure that method calls/signals
  // are marshalled to the right thread. Could almost use proxy.h for this,
  // but it doesn't have a mechanism for marshalling sigslot::signals
  std::unique_ptr<rtc::AsyncInvoker> sctp_invoker_;
  sigslot::signal1<bool> SignalSctpReadyToSendData;
  sigslot::signal2<const cricket::ReceiveDataParams&,
                   const rtc::CopyOnWriteBuffer&>
      SignalSctpDataReceived;
  sigslot::signal1<int> SignalSctpStreamClosedRemotely;

  std::unique_ptr<SessionDescriptionInterface> current_local_description_;
  std::unique_ptr<SessionDescriptionInterface> pending_local_description_;
  std::unique_ptr<SessionDescriptionInterface> current_remote_description_;
  std::unique_ptr<SessionDescriptionInterface> pending_remote_description_;
  bool dtls_enabled_ = false;
  // Specifies which kind of data channel is allowed. This is controlled
  // by the chrome command-line flag and constraints:
  // 1. If chrome command-line switch 'enable-sctp-data-channels' is enabled,
  // constraint kEnableDtlsSrtp is true, and constaint kEnableRtpDataChannels is
  // not set or false, SCTP is allowed (DCT_SCTP);
  // 2. If constraint kEnableRtpDataChannels is true, RTP is allowed (DCT_RTP);
  // 3. If both 1&2 are false, data channel is not allowed (DCT_NONE).
  cricket::DataChannelType data_channel_type_ = cricket::DCT_NONE;
  // List of content names for which the remote side triggered an ICE restart.
  std::set<std::string> pending_ice_restarts_;

  std::unique_ptr<WebRtcSessionDescriptionFactory> webrtc_session_desc_factory_;

  // Member variables for caching global options.
  cricket::AudioOptions audio_options_;
  cricket::VideoOptions video_options_;
};

}  // namespace webrtc

#endif  // PC_PEERCONNECTION_H_
