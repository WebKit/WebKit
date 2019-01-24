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

#include "api/media_transport_interface.h"
#include "api/peerconnectioninterface.h"
#include "api/turncustomizer.h"
#include "pc/iceserverparsing.h"
#include "pc/jseptransportcontroller.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectioninternal.h"
#include "pc/rtcstatscollector.h"
#include "pc/rtptransceiver.h"
#include "pc/statscollector.h"
#include "pc/streamcollection.h"
#include "pc/webrtcsessiondescriptionfactory.h"

namespace webrtc {

class MediaStreamObserver;
class VideoRtpReceiver;
class RtcEventLog;

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
class PeerConnection : public PeerConnectionInternal,
                       public DataChannelProviderInterface,
                       public DataChannelSink,
                       public JsepTransportController::Observer,
                       public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  enum class UsageEvent : int {
    TURN_SERVER_ADDED = 0x01,
    STUN_SERVER_ADDED = 0x02,
    DATA_ADDED = 0x04,
    AUDIO_ADDED = 0x08,
    VIDEO_ADDED = 0x10,
    SET_LOCAL_DESCRIPTION_CALLED = 0x20,
    SET_REMOTE_DESCRIPTION_CALLED = 0x40,
    CANDIDATE_COLLECTED = 0x80,
    REMOTE_CANDIDATE_ADDED = 0x100,
    ICE_STATE_CONNECTED = 0x200,
    CLOSE_CALLED = 0x400,
    PRIVATE_CANDIDATE_COLLECTED = 0x800,
    MAX_VALUE = 0x1000,
  };

  explicit PeerConnection(PeerConnectionFactory* factory,
                          std::unique_ptr<RtcEventLog> event_log,
                          std::unique_ptr<Call> call);

  bool Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override;
  bool RemoveTrack(RtpSenderInterface* sender) override;
  RTCError RemoveTrackNew(
      rtc::scoped_refptr<RtpSenderInterface> sender) override;

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

  // Version of the above method that returns the full certificate chain.
  std::unique_ptr<rtc::SSLCertChain> GetRemoteAudioSSLCertChain();

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
  // WARNING: LEGACY. See peerconnectioninterface.h
  bool GetStats(StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;
  // Spec-complaint GetStats(). See peerconnectioninterface.h
  void GetStats(RTCStatsCollectorCallback* callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpSenderInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpReceiverInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void ClearStatsCache() override;

  SignalingState signaling_state() override;

  IceConnectionState ice_connection_state() override;
  IceConnectionState standardized_ice_connection_state();
  PeerConnectionState peer_connection_state() override;
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
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
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

  RTCError SetBitrate(const BitrateSettings& bitrate) override;

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

  // PeerConnectionInternal implementation.
  rtc::Thread* network_thread() const override {
    return factory_->network_thread();
  }
  rtc::Thread* worker_thread() const override {
    return factory_->worker_thread();
  }
  rtc::Thread* signaling_thread() const override {
    return factory_->signaling_thread();
  }

  std::string session_id() const override { return session_id_; }

  bool initial_offerer() const override {
    return transport_controller_ && transport_controller_->initial_offerer();
  }

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversInternal() const override {
    return transceivers_;
  }

  absl::string_view GetLocalTrackIdBySsrc(uint32_t ssrc) override;
  absl::string_view GetRemoteTrackIdBySsrc(uint32_t ssrc) override;

  sigslot::signal1<DataChannel*>& SignalDataChannelCreated() override {
    return SignalDataChannelCreated_;
  }

  cricket::RtpDataChannel* rtp_data_channel() const override {
    return rtp_data_channel_;
  }

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels()
      const override {
    return sctp_data_channels_;
  }

  absl::optional<std::string> sctp_content_name() const override {
    return sctp_mid_;
  }

  absl::optional<std::string> sctp_transport_name() const override;

  cricket::CandidateStatsList GetPooledCandidateStats() const override;
  std::map<std::string, std::string> GetTransportNamesByMid() const override;
  std::map<std::string, cricket::TransportStats> GetTransportStatsByNames(
      const std::set<std::string>& transport_names) override;
  Call::Stats GetCallStats() override;

  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override;
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& transport_name) override;
  bool IceRestartPending(const std::string& content_name) const override;
  bool NeedsIceRestart(const std::string& content_name) const override;
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role) override;

  void ReturnHistogramVeryQuicklyForTesting() {
    return_histogram_very_quickly_ = true;
  }
  void RequestUsagePatternReportForTesting();

 protected:
  ~PeerConnection() override;

 private:
  class SetRemoteDescriptionObserverAdapter;
  friend class SetRemoteDescriptionObserverAdapter;

  struct RtpSenderInfo {
    RtpSenderInfo() : first_ssrc(0) {}
    RtpSenderInfo(const std::string& stream_id,
                  const std::string sender_id,
                  uint32_t ssrc)
        : stream_id(stream_id), sender_id(sender_id), first_ssrc(ssrc) {}
    bool operator==(const RtpSenderInfo& other) {
      return this->stream_id == other.stream_id &&
             this->sender_id == other.sender_id &&
             this->first_ssrc == other.first_ssrc;
    }
    std::string stream_id;
    std::string sender_id;
    // An RtpSender can have many SSRCs. The first one is used as a sort of ID
    // for communicating with the lower layers.
    uint32_t first_ssrc;
  };

  // Implements MessageHandler.
  void OnMessage(rtc::Message* msg) override;

  // Plan B helpers for getting the voice/video media channels for the single
  // audio/video transceiver, if it exists.
  cricket::VoiceMediaChannel* voice_media_channel() const;
  cricket::VideoMediaChannel* video_media_channel() const;

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  GetSendersInternal() const;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  GetReceiversInternal() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAudioTransceiver() const;
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetVideoTransceiver() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetFirstAudioTransceiver() const;

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

  // AddTrack implementation when Unified Plan is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackUnifiedPlan(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids);
  // AddTrack implementation when Plan B is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackPlanB(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids);

  // Returns the first RtpTransceiver suitable for a newly added track, if such
  // transceiver is available.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindFirstTransceiverForAddedTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track);

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindTransceiverBySender(rtc::scoped_refptr<RtpSenderInterface> sender);

  // Internal implementation for AddTransceiver family of methods. If
  // |fire_callback| is set, fires OnRenegotiationNeeded callback if successful.
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init,
      bool fire_callback = true);

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  CreateSender(cricket::MediaType media_type,
               const std::string& id,
               rtc::scoped_refptr<MediaStreamTrackInterface> track,
               const std::vector<std::string>& stream_ids,
               const std::vector<RtpEncodingParameters>& send_encodings);

  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  CreateReceiver(cricket::MediaType media_type, const std::string& receiver_id);

  // Create a new RtpTransceiver of the given type and add it to the list of
  // transceivers.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  CreateAndAddTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  void SetIceConnectionState(IceConnectionState new_state);
  void SetStandardizedIceConnectionState(
      PeerConnectionInterface::IceConnectionState new_state);
  void SetConnectionState(
      PeerConnectionInterface::PeerConnectionState new_state);

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
                                        RTCError&& error);
  void PostCreateSessionDescriptionFailure(
      CreateSessionDescriptionObserver* observer,
      RTCError error);

  // Synchronous implementations of SetLocalDescription/SetRemoteDescription
  // that return an RTCError instead of invoking a callback.
  RTCError ApplyLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc);
  RTCError ApplyRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc);

  // Updates the local RtpTransceivers according to the JSEP rules. Called as
  // part of setting the local/remote description.
  RTCError UpdateTransceiversAndDataChannels(
      cricket::ContentSource source,
      const SessionDescriptionInterface& new_session,
      const SessionDescriptionInterface* old_local_description,
      const SessionDescriptionInterface* old_remote_description);

  // Either creates or destroys the transceiver's BaseChannel according to the
  // given media section.
  RTCError UpdateTransceiverChannel(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      const cricket::ContentInfo& content,
      const cricket::ContentGroup* bundle_group);

  // Either creates or destroys the local data channel according to the given
  // media section.
  RTCError UpdateDataChannel(cricket::ContentSource source,
                             const cricket::ContentInfo& content,
                             const cricket::ContentGroup* bundle_group);

  // Associate the given transceiver according to the JSEP rules.
  RTCErrorOr<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  AssociateTransceiver(cricket::ContentSource source,
                       SdpType type,
                       size_t mline_index,
                       const cricket::ContentInfo& content,
                       const cricket::ContentInfo* old_local_content,
                       const cricket::ContentInfo* old_remote_content);

  // Returns the RtpTransceiver, if found, that is associated to the given MID.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAssociatedTransceiver(const std::string& mid) const;

  // Returns the RtpTransceiver, if found, that was assigned to the given mline
  // index in CreateOffer.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetTransceiverByMLineIndex(size_t mline_index) const;

  // Returns an RtpTransciever, if available, that can be used to receive the
  // given media type according to JSEP rules.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindAvailableTransceiverToReceive(cricket::MediaType media_type) const;

  // Returns the media section in the given session description that is
  // associated with the RtpTransceiver. Returns null if none found or this
  // RtpTransceiver is not associated. Logic varies depending on the
  // SdpSemantics specified in the configuration.
  const cricket::ContentInfo* FindMediaSectionForTransceiver(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      const SessionDescriptionInterface* sdesc) const;

  // Runs the algorithm **set the associated remote streams** specified in
  // https://w3c.github.io/webrtc-pc/#set-associated-remote-streams.
  void SetAssociatedRemoteStreams(
      rtc::scoped_refptr<RtpReceiverInternal> receiver,
      const std::vector<std::string>& stream_ids,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* added_streams,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  // Runs the algorithm **process the removal of a remote track** specified in
  // the WebRTC specification.
  // This method will update the following lists:
  // |remove_list| is the list of transceivers for which the receiving track is
  //     being removed.
  // |removed_streams| is the list of streams which no longer have a receiving
  //     track so should be removed.
  // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
  void ProcessRemovalOfRemoteTrack(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>* remove_list,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  void RemoveRemoteStreamsIfEmpty(
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
          remote_streams,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  void OnNegotiationNeeded();

  bool IsClosed() const {
    return signaling_state_ == PeerConnectionInterface::kClosed;
  }

  // Returns a MediaSessionOptions struct with options decided by |options|,
  // the local MediaStreams and DataChannels.
  void GetOptionsForOffer(const PeerConnectionInterface::RTCOfferAnswerOptions&
                              offer_answer_options,
                          cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options);
  void GetOptionsForUnifiedPlanOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options);

  RTCError HandleLegacyOfferOptions(const RTCOfferAnswerOptions& options);
  void RemoveRecvDirectionFromReceivingTransceiversOfType(
      cricket::MediaType media_type);
  void AddUpToOneReceivingTransceiverOfType(cricket::MediaType media_type);
  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetReceivingTransceiversOfType(cricket::MediaType media_type);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  void GetOptionsForAnswer(const RTCOfferAnswerOptions& offer_answer_options,
                           cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options);
  void GetOptionsForUnifiedPlanAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options);

  // Generates MediaDescriptionOptions for the |session_opts| based on existing
  // local description or remote description.
  void GenerateMediaDescriptionOptions(
      const SessionDescriptionInterface* session_desc,
      RtpTransceiverDirection audio_direction,
      RtpTransceiverDirection video_direction,
      absl::optional<size_t>* audio_index,
      absl::optional<size_t>* video_index,
      absl::optional<size_t>* data_index,
      cricket::MediaSessionOptions* session_options);

  // Generates the active MediaDescriptionOptions for the local data channel
  // given the specified MID.
  cricket::MediaDescriptionOptions GetMediaDescriptionOptionsForActiveData(
      const std::string& mid) const;

  // Generates the rejected MediaDescriptionOptions for the local data channel
  // given the specified MID.
  cricket::MediaDescriptionOptions GetMediaDescriptionOptionsForRejectedData(
      const std::string& mid) const;

  // Returns the MID for the data section associated with either the
  // RtpDataChannel or SCTP data channel, if it has been set. If no data
  // channels are configured this will return nullopt.
  absl::optional<std::string> GetDataMid() const;

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
  // Parses and handles open messages.  Returns true if the message is an open
  // message, false otherwise.
  bool HandleOpenMessage_s(const cricket::ReceiveDataParams& params,
                           const rtc::CopyOnWriteBuffer& buffer)
      RTC_RUN_ON(signaling_thread());

  // Returns true if the PeerConnection is configured to use Unified Plan
  // semantics for creating offers/answers and setting local/remote
  // descriptions. If this is true the RtpTransceiver API will also be available
  // to the user. If this is false, Plan B semantics are assumed.
  // TODO(bugs.webrtc.org/8530): Flip the default to be Unified Plan once
  // sufficient time has passed.
  bool IsUnifiedPlan() const {
    return configuration_.sdp_semantics == SdpSemantics::kUnifiedPlan;
  }

  // The offer/answer machinery assumes the media section MID is present and
  // unique. To support legacy end points that do not supply a=mid lines, this
  // method will modify the session description to add MIDs generated according
  // to the SDP semantics.
  void FillInMissingRemoteMids(cricket::SessionDescription* remote_description);

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
                                      const std::string& stream_id,
                                      const std::string sender_id) const;

  // Returns the specified SCTP DataChannel in sctp_data_channels_,
  // or nullptr if not found.
  DataChannel* FindDataChannelBySid(int sid) const;

  // Called when first configuring the port allocator.
  bool InitializePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      const RTCConfiguration& configuration);
  // Called when SetConfiguration is called to apply the supported subset
  // of the configuration on the network thread.
  bool ReconfigurePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportsType type,
      int candidate_pool_size,
      bool prune_turn_ports,
      webrtc::TurnCustomizer* turn_customizer,
      absl::optional<int> stun_candidate_keepalive_interval);

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

  enum class SessionError {
    kNone,       // No error.
    kContent,    // Error in BaseChannel SetLocalContent/SetRemoteContent.
    kTransport,  // Error from the underlying transport.
  };

  // Returns the last error in the session. See the enum above for details.
  SessionError session_error() const { return session_error_; }
  const std::string& session_error_desc() const { return session_error_desc_; }

  cricket::ChannelInterface* GetChannel(const std::string& content_name);

  // Get current SSL role used by SCTP's underlying transport.
  bool GetSctpSslRole(rtc::SSLRole* role);

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

  // Implements DataChannelSink.
  void OnDataReceived(int channel_id,
                      DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override;
  void OnChannelClosing(int channel_id) override;
  void OnChannelClosed(int channel_id) override;

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  void OnDtlsSrtpSetupFailure(cricket::BaseChannel*, bool rtcp);

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
  void SetSessionError(SessionError error, const std::string& error_desc);

  RTCError UpdateSessionState(SdpType type,
                              cricket::ContentSource source,
                              const cricket::SessionDescription* description);
  // Push the media parts of the local or remote session description
  // down to all of the channels.
  RTCError PushdownMediaDescription(SdpType type,
                                    cricket::ContentSource source);
  bool PushdownSctpParameters_n(cricket::ContentSource source);

  RTCError PushdownTransportDescription(cricket::ContentSource source,
                                        SdpType type);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(
      const cricket::SessionDescription* description,
      const std::string& content_name,
      cricket::TransportDescription* info);

  // Enables media channels to allow sending of media.
  // This enables media to flow on all configured audio/video channels and the
  // RtpDataChannel.
  void EnableSending();

  // Destroys all BaseChannels and destroys the SCTP data channel, if present.
  void DestroyAllChannels();

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
  RTCError CreateChannels(const cricket::SessionDescription& desc);

  // If the BUNDLE policy is max-bundle, then we know for sure that all
  // transports will be bundled from the start. This method returns the BUNDLE
  // group if that's the case, or null if BUNDLE will be negotiated later. An
  // error is returned if max-bundle is specified but the session description
  // does not have a BUNDLE group.
  RTCErrorOr<const cricket::ContentGroup*> GetEarlyBundleGroup(
      const cricket::SessionDescription& desc) const;

  // Helper methods to create media channels.
  cricket::VoiceChannel* CreateVoiceChannel(const std::string& mid);
  cricket::VideoChannel* CreateVideoChannel(const std::string& mid);
  bool CreateDataChannel(const std::string& mid);

  bool CreateSctpTransport_n(const std::string& mid);
  // For bundling.
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
  void OnSctpClosingProcedureStartedRemotely_n(int sid);
  void OnSctpClosingProcedureComplete_n(int sid);

  bool SetupMediaTransportForDataChannels_n(const std::string& mid)
      RTC_RUN_ON(network_thread());
  void OnMediaTransportStateChanged_n() RTC_RUN_ON(network_thread());
  void TeardownMediaTransportForDataChannels_n() RTC_RUN_ON(network_thread());

  bool ValidateBundleSettings(const cricket::SessionDescription* desc);
  bool HasRtcpMuxEnabled(const cricket::ContentInfo* content);
  // Below methods are helper methods which verifies SDP.
  RTCError ValidateSessionDescription(const SessionDescriptionInterface* sdesc,
                                      cricket::ContentSource source);

  // Check if a call to SetLocalDescription is acceptable with a session
  // description of the given type.
  bool ExpectSetLocalDescription(SdpType type);
  // Check if a call to SetRemoteDescription is acceptable with a session
  // description of the given type.
  bool ExpectSetRemoteDescription(SdpType type);
  // Verifies a=setup attribute as per RFC 5763.
  bool ValidateDtlsSetupAttribute(const cricket::SessionDescription* desc,
                                  SdpType type);

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

  // JsepTransportController signal handlers.
  void OnTransportControllerConnectionState(cricket::IceConnectionState state);
  void OnTransportControllerGatheringState(cricket::IceGatheringState state);
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates);
  void OnTransportControllerDtlsHandshakeError(rtc::SSLHandshakeError error);

  const char* SessionErrorToString(SessionError error) const;
  std::string GetSessionErrorMsg();

  // Report the UMA metric SdpFormatReceived for the given remote offer.
  void ReportSdpFormatReceived(const SessionDescriptionInterface& remote_offer);

  // Report inferred negotiated SDP semantics from a local/remote answer to the
  // UMA observer.
  void ReportNegotiatedSdpSemantics(const SessionDescriptionInterface& answer);

  // Invoked when TransportController connection completion is signaled.
  // Reports stats for all transports in use.
  void ReportTransportStats();

  // Gather the usage of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats,
                               const std::set<cricket::MediaType>& media_types);

  void NoteUsageEvent(UsageEvent event);
  void ReportUsagePattern() const;

  void OnSentPacket_w(const rtc::SentPacket& sent_packet);

  const std::string GetTransportName(const std::string& content_name);

  // Destroys and clears the BaseChannel associated with the given transceiver,
  // if such channel is set.
  void DestroyTransceiverChannel(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver);

  // Destroys the RTP data channel and/or the SCTP data channel and clears it.
  void DestroyDataChannel();

  // Destroys the given ChannelInterface.
  // The channel cannot be accessed after this method is called.
  void DestroyChannelInterface(cricket::ChannelInterface* channel);

  // JsepTransportController::Observer override.
  //
  // Called by |transport_controller_| when processing transport information
  // from a session description, and the mapping from m= sections to transports
  // changed (as a result of BUNDLE negotiation, or m= sections being
  // rejected).
  bool OnTransportChanged(const std::string& mid,
                          RtpTransportInternal* rtp_transport,
                          cricket::DtlsTransportInternal* dtls_transport,
                          MediaTransportInterface* media_transport) override;

  // Returns the observer. Will crash on CHECK if the observer is removed.
  PeerConnectionObserver* Observer() const;

  // Returns the CryptoOptions for this PeerConnection. This will always
  // return the RTCConfiguration.crypto_options if set and will only default
  // back to the PeerConnectionFactory settings if nothing was set.
  CryptoOptions GetCryptoOptions();

  // Returns rtp transport, result can not be nullptr.
  RtpTransportInternal* GetRtpTransport(const std::string& mid) {
    auto rtp_transport = transport_controller_->GetRtpTransport(mid);
    RTC_DCHECK(rtp_transport);
    return rtp_transport;
  }

  // Returns media transport, if PeerConnection was created with configuration
  // to use media transport. Otherwise returns nullptr.
  MediaTransportInterface* GetMediaTransport(const std::string& mid) {
    auto media_transport = transport_controller_->GetMediaTransport(mid);
    RTC_DCHECK((configuration_.use_media_transport ||
                configuration_.use_media_transport_for_data_channels) ==
               (media_transport != nullptr))
        << "configuration_.use_media_transport="
        << configuration_.use_media_transport
        << ", configuration_.use_media_transport_for_data_channels="
        << configuration_.use_media_transport_for_data_channels
        << ", (media_transport != nullptr)=" << (media_transport != nullptr);
    return media_transport;
  }

  sigslot::signal1<DataChannel*> SignalDataChannelCreated_;

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  rtc::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_ = nullptr;

  // The EventLog needs to outlive |call_| (and any other object that uses it).
  std::unique_ptr<RtcEventLog> event_log_;

  SignalingState signaling_state_ = kStable;
  IceConnectionState ice_connection_state_ = kIceConnectionNew;
  PeerConnectionInterface::IceConnectionState
      standardized_ice_connection_state_ = kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState connection_state_ =
      PeerConnectionState::kNew;

  IceGatheringState ice_gathering_state_ = kIceGatheringNew;
  PeerConnectionInterface::RTCConfiguration configuration_;

  // TODO(zstein): |async_resolver_factory_| can currently be nullptr if it
  // is not injected. It should be required once chromium supplies it.
  std::unique_ptr<AsyncResolverFactory> async_resolver_factory_;
  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier_;
  int port_allocator_flags_ = 0;

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
  // In Unified Plan, if we encounter remote SDP that does not contain an a=msid
  // line we create and use a stream with a random ID for our receivers. This is
  // to support legacy endpoints that do not support the a=msid attribute (as
  // opposed to streamless tracks with "a=msid:-").
  rtc::scoped_refptr<MediaStreamInterface> missing_msid_default_stream_;
  // MIDs that have been seen either by SetLocalDescription or
  // SetRemoteDescription over the life of the PeerConnection.
  std::set<std::string> seen_mids_;

  SessionError session_error_ = SessionError::kNone;
  std::string session_error_desc_;

  std::string session_id_;

  std::unique_ptr<JsepTransportController> transport_controller_;
  std::unique_ptr<cricket::SctpTransportInternalFactory> sctp_factory_;
  // |rtp_data_channel_| is used if in RTP data channel mode, |sctp_transport_|
  // when using SCTP.
  cricket::RtpDataChannel* rtp_data_channel_ = nullptr;

  std::unique_ptr<cricket::SctpTransportInternal> sctp_transport_;
  // |sctp_mid_| is the content name (MID) in SDP.
  absl::optional<std::string> sctp_mid_;
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
  sigslot::signal1<int> SignalSctpClosingProcedureStartedRemotely;
  sigslot::signal1<int> SignalSctpClosingProcedureComplete;

  // Whether this peer is the caller. Set when the local description is applied.
  absl::optional<bool> is_caller_ RTC_GUARDED_BY(signaling_thread());

  // Content name (MID) for media transport data channels in SDP.
  absl::optional<std::string> media_transport_data_mid_;

  // Media transport used for data channels.  Thread-safe.
  MediaTransportInterface* media_transport_ = nullptr;

  // Cached value of whether the media transport is ready to send.
  bool media_transport_ready_to_send_data_ RTC_GUARDED_BY(signaling_thread()) =
      false;

  // Used to invoke media transport signals on the signaling thread.
  std::unique_ptr<rtc::AsyncInvoker> media_transport_invoker_;

  // Identical to the signals for SCTP, but from media transport:
  sigslot::signal1<bool> SignalMediaTransportWritable_s
      RTC_GUARDED_BY(signaling_thread());
  sigslot::signal2<const cricket::ReceiveDataParams&,
                   const rtc::CopyOnWriteBuffer&>
      SignalMediaTransportReceivedData_s RTC_GUARDED_BY(signaling_thread());
  sigslot::signal1<int> SignalMediaTransportChannelClosing_s
      RTC_GUARDED_BY(signaling_thread());
  sigslot::signal1<int> SignalMediaTransportChannelClosed_s
      RTC_GUARDED_BY(signaling_thread());

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

  int usage_event_accumulator_ = 0;
  bool return_histogram_very_quickly_ = false;
};

}  // namespace webrtc

#endif  // PC_PEERCONNECTION_H_
