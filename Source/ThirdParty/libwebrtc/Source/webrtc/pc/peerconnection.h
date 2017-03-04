/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_PEERCONNECTION_H_
#define WEBRTC_PC_PEERCONNECTION_H_

#include <string>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/pc/peerconnectionfactory.h"
#include "webrtc/pc/rtcstatscollector.h"
#include "webrtc/pc/rtpreceiver.h"
#include "webrtc/pc/rtpsender.h"
#include "webrtc/pc/statscollector.h"
#include "webrtc/pc/streamcollection.h"
#include "webrtc/pc/webrtcsession.h"

namespace webrtc {

class MediaStreamObserver;
class VideoRtpReceiver;
class RtcEventLog;

// Populates |session_options| from |rtc_options|, and returns true if options
// are valid.
// |session_options|->transport_options map entries must exist in order for
// them to be populated from |rtc_options|.
bool ExtractMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    bool is_offer,
    cricket::MediaSessionOptions* session_options);

// Populates |session_options| from |constraints|, and returns true if all
// mandatory constraints are satisfied.
// Assumes that |session_options|->transport_options map entries exist.
// Will also set defaults if corresponding constraints are not present:
// recv_audio=true, recv_video=true, bundle_enabled=true.
// Other fields will be left with existing values.
//
// Deprecated. Will be removed once callers that use constraints are gone.
// TODO(hta): Remove when callers are gone.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5617
bool ParseConstraintsForAnswer(const MediaConstraintsInterface* constraints,
                               cricket::MediaSessionOptions* session_options);

// Parses the URLs for each server in |servers| to build |stun_servers| and
// |turn_servers|. Can return SYNTAX_ERROR if the URL is malformed, or
// INVALID_PARAMETER if a TURN server is missing |username| or |password|.
RTCErrorType ParseIceServers(
    const PeerConnectionInterface::IceServers& servers,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers);

// PeerConnection implements the PeerConnectionInterface interface.
// It uses WebRtcSession to implement the PeerConnection functionality.
class PeerConnection : public PeerConnectionInterface,
                       public IceObserver,
                       public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
  explicit PeerConnection(PeerConnectionFactory* factory);

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

  virtual WebRtcSession* session() { return session_.get(); }

  rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) override;

  rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) override;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
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

  bool StartRtcEventLog(rtc::PlatformFile file,
                        int64_t max_size_bytes) override;
  void StopRtcEventLog() override;

  void Close() override;

  sigslot::signal1<DataChannel*> SignalDataChannelCreated;

  // Virtual for unit tests.
  virtual const std::vector<rtc::scoped_refptr<DataChannel>>&
  sctp_data_channels() const {
    return sctp_data_channels_;
  }

 protected:
  ~PeerConnection() override;

 private:
  struct TrackInfo {
    TrackInfo() : ssrc(0) {}
    TrackInfo(const std::string& stream_label,
              const std::string track_id,
              uint32_t ssrc)
        : stream_label(stream_label), track_id(track_id), ssrc(ssrc) {}
    bool operator==(const TrackInfo& other) {
      return this->stream_label == other.stream_label &&
             this->track_id == other.track_id && this->ssrc == other.ssrc;
    }
    std::string stream_label;
    std::string track_id;
    uint32_t ssrc;
  };
  typedef std::vector<TrackInfo> TrackInfos;

  // Implements MessageHandler.
  void OnMessage(rtc::Message* msg) override;

  void CreateAudioReceiver(MediaStreamInterface* stream,
                           const std::string& track_id,
                           uint32_t ssrc);

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const std::string& track_id,
                           uint32_t ssrc);
  void DestroyReceiver(const std::string& track_id);
  void DestroyAudioSender(MediaStreamInterface* stream,
                          AudioTrackInterface* audio_track,
                          uint32_t ssrc);
  void DestroyVideoSender(MediaStreamInterface* stream,
                          VideoTrackInterface* video_track);

  // Implements IceObserver
  void OnIceConnectionChange(IceConnectionState new_state) override;
  void OnIceGatheringChange(IceGatheringState new_state) override;
  void OnIceCandidate(const IceCandidateInterface* candidate) override;
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override;
  void OnIceConnectionReceivingChange(bool receiving) override;

  // Signals from WebRtcSession.
  void OnSessionStateChange(WebRtcSession* session, WebRtcSession::State state);
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

  rtc::Thread* signaling_thread() const {
    return factory_->signaling_thread();
  }

  rtc::Thread* network_thread() const { return factory_->network_thread(); }

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
  virtual bool GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  // Deprecated, use version without constraints.
  virtual bool GetOptionsForAnswer(
      const MediaConstraintsInterface* constraints,
      cricket::MediaSessionOptions* session_options);
  virtual bool GetOptionsForAnswer(
      const RTCOfferAnswerOptions& options,
      cricket::MediaSessionOptions* session_options);

  void InitializeOptionsForAnswer(
      cricket::MediaSessionOptions* session_options);

  // Helper function for options processing.
  // Deprecated.
  virtual void FinishOptionsForAnswer(
      cricket::MediaSessionOptions* session_options);

  // Remove all local and remote tracks of type |media_type|.
  // Called when a media type is rejected (m-line set to port 0).
  void RemoveTracks(cricket::MediaType media_type);

  // Makes sure a MediaStreamTrack is created for each StreamParam in |streams|,
  // and existing MediaStreamTracks are removed if there is no corresponding
  // StreamParam. If |default_track_needed| is true, a default MediaStreamTrack
  // is created if it doesn't exist; if false, it's removed if it exists.
  // |media_type| is the type of the |streams| and can be either audio or video.
  // If a new MediaStream is created it is added to |new_streams|.
  void UpdateRemoteStreamsList(
      const std::vector<cricket::StreamParams>& streams,
      bool default_track_needed,
      cricket::MediaType media_type,
      StreamCollection* new_streams);

  // Triggered when a remote track has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers CreateAudioReceiver or CreateVideoReceiver.
  void OnRemoteTrackSeen(const std::string& stream_label,
                         const std::string& track_id,
                         uint32_t ssrc,
                         cricket::MediaType media_type);

  // Triggered when a remote track has been removed from a remote session
  // description. It removes the remote track with id |track_id| from a remote
  // MediaStream and triggers DestroyAudioReceiver or DestroyVideoReceiver.
  void OnRemoteTrackRemoved(const std::string& stream_label,
                            const std::string& track_id,
                            cricket::MediaType media_type);

  // Finds remote MediaStreams without any tracks and removes them from
  // |remote_streams_| and notifies the observer that the MediaStreams no longer
  // exist.
  void UpdateEndedRemoteMediaStreams();

  // Loops through the vector of |streams| and finds added and removed
  // StreamParams since last time this method was called.
  // For each new or removed StreamParam, OnLocalTrackSeen or
  // OnLocalTrackRemoved is invoked.
  void UpdateLocalTracks(const std::vector<cricket::StreamParams>& streams,
                         cricket::MediaType media_type);

  // Triggered when a local track has been seen for the first time in a local
  // session description.
  // This method triggers CreateAudioSender or CreateVideoSender if the rtp
  // streams in the local SessionDescription can be mapped to a MediaStreamTrack
  // in a MediaStream in |local_streams_|
  void OnLocalTrackSeen(const std::string& stream_label,
                        const std::string& track_id,
                        uint32_t ssrc,
                        cricket::MediaType media_type);

  // Triggered when a local track has been removed from a local session
  // description.
  // This method triggers DestroyAudioSender or DestroyVideoSender if a stream
  // has been removed from the local SessionDescription and the stream can be
  // mapped to a MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalTrackRemoved(const std::string& stream_label,
                           const std::string& track_id,
                           uint32_t ssrc,
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

  // Notifications from WebRtcSession relating to BaseChannels.
  void OnVoiceChannelCreated();
  void OnVoiceChannelDestroyed();
  void OnVideoChannelCreated();
  void OnVideoChannelDestroyed();
  void OnDataChannelCreated();
  void OnDataChannelDestroyed();
  // Called when the cricket::DataChannel receives a message indicating that a
  // webrtc::DataChannel should be opened.
  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config);

  RtpSenderInternal* FindSenderById(const std::string& id);

  std::vector<rtc::scoped_refptr<
      RtpSenderProxyWithInternal<RtpSenderInternal>>>::iterator
  FindSenderForTrack(MediaStreamTrackInterface* track);
  std::vector<rtc::scoped_refptr<
      RtpReceiverProxyWithInternal<RtpReceiverInternal>>>::iterator
  FindReceiverForTrack(const std::string& track_id);

  TrackInfos* GetRemoteTracks(cricket::MediaType media_type);
  TrackInfos* GetLocalTracks(cricket::MediaType media_type);
  const TrackInfo* FindTrackInfo(const TrackInfos& infos,
                                 const std::string& stream_label,
                                 const std::string track_id) const;

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
      bool prune_turn_ports);

  // Starts recording an Rtc EventLog using the supplied platform file.
  // This function should only be called from the worker thread.
  bool StartRtcEventLog_w(rtc::PlatformFile file, int64_t max_size_bytes);
  // Starts recording an Rtc EventLog using the supplied platform file.
  // This function should only be called from the worker thread.
  void StopRtcEventLog_w();

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  rtc::scoped_refptr<PeerConnectionFactory> factory_;
  PeerConnectionObserver* observer_;
  UMAObserver* uma_observer_;
  SignalingState signaling_state_;
  IceConnectionState ice_connection_state_;
  IceGatheringState ice_gathering_state_;
  PeerConnectionInterface::RTCConfiguration configuration_;

  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  // The EventLog needs to outlive the media controller.
  std::unique_ptr<RtcEventLog> event_log_;
  std::unique_ptr<MediaControllerInterface> media_controller_;

  // One PeerConnection has only one RTCP CNAME.
  // https://tools.ietf.org/html/draft-ietf-rtcweb-rtp-usage-26#section-4.9
  std::string rtcp_cname_;

  // Streams added via AddStream.
  rtc::scoped_refptr<StreamCollection> local_streams_;
  // Streams created as a result of SetRemoteDescription.
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<std::unique_ptr<MediaStreamObserver>> stream_observers_;

  // These lists store track info seen in local/remote descriptions.
  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  SctpSidAllocator sid_allocator_;
  // label -> DataChannel
  std::map<std::string, rtc::scoped_refptr<DataChannel>> rtp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_;
  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_to_free_;

  bool remote_peer_supports_msid_ = false;

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      senders_;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      receivers_;
  std::unique_ptr<WebRtcSession> session_;
  std::unique_ptr<StatsCollector> stats_;
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_PEERCONNECTION_H_
