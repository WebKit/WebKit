/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SDP_OFFER_ANSWER_H_
#define PC_SDP_OFFER_ANSWER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/jsep_ice_candidate.h"
#include "api/peer_connection_interface.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/turn_customizer.h"
#include "pc/data_channel_controller.h"
#include "pc/ice_server_parsing.h"
#include "pc/jsep_transport_controller.h"
#include "pc/peer_connection_factory.h"
#include "pc/peer_connection_internal.h"
#include "pc/rtc_stats_collector.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transceiver.h"
#include "pc/sctp_transport.h"
#include "pc/stats_collector.h"
#include "pc/stream_collection.h"
#include "pc/webrtc_session_description_factory.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/operations_chain.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/unique_id_generator.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class MediaStreamObserver;
class PeerConnection;
class VideoRtpReceiver;
class RtcEventLog;

// SdpOfferAnswerHandler is a component
// of the PeerConnection object as defined
// by the PeerConnectionInterface API surface.
// The class is responsible for the following:
// - Parsing and interpreting SDP.
// - Generating offers and answers based on the current state.
// This class lives on the signaling thread.
class SdpOfferAnswerHandler {
 public:
  explicit SdpOfferAnswerHandler(PeerConnection* pc);
  ~SdpOfferAnswerHandler();

  void SetSessionDescFactory(
      std::unique_ptr<WebRtcSessionDescriptionFactory> factory) {
    RTC_DCHECK_RUN_ON(signaling_thread());
    webrtc_session_desc_factory_ = std::move(factory);
  }
  void ResetSessionDescFactory() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    webrtc_session_desc_factory_.reset();
  }
  const WebRtcSessionDescriptionFactory* webrtc_session_desc_factory() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return webrtc_session_desc_factory_.get();
  }

  // Change signaling state to Closed, and perform appropriate actions.
  void Close();

  // Called as part of destroying the owning PeerConnection.
  void PrepareForShutdown();

  PeerConnectionInterface::SignalingState signaling_state() const;

  const SessionDescriptionInterface* local_description() const;
  const SessionDescriptionInterface* remote_description() const;
  const SessionDescriptionInterface* current_local_description() const;
  const SessionDescriptionInterface* current_remote_description() const;
  const SessionDescriptionInterface* pending_local_description() const;
  const SessionDescriptionInterface* pending_remote_description() const;

  void RestartIce();

  // JSEP01
  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  void CreateAnswer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);

  void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc);
  void SetLocalDescription(SetSessionDescriptionObserver* observer);

  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer);
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc);

  PeerConnectionInterface::RTCConfiguration GetConfiguration();
  RTCError SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration);
  bool AddIceCandidate(const IceCandidateInterface* candidate);
  void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                       std::function<void(RTCError)> callback);
  bool RemoveIceCandidates(const std::vector<cricket::Candidate>& candidates);
  // Adds a locally generated candidate to the local description.
  void AddLocalIceCandidate(const JsepIceCandidate* candidate);
  void RemoveLocalIceCandidates(
      const std::vector<cricket::Candidate>& candidates);
  bool ShouldFireNegotiationNeededEvent(uint32_t event_id);

  absl::optional<bool> is_caller();
  bool HasNewIceCredentials();
  bool IceRestartPending(const std::string& content_name) const;
  void UpdateNegotiationNeeded();
  void SetHavePendingRtpDataChannel() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    have_pending_rtp_data_channel_ = true;
  }

  // Returns the media section in the given session description that is
  // associated with the RtpTransceiver. Returns null if none found or this
  // RtpTransceiver is not associated. Logic varies depending on the
  // SdpSemantics specified in the configuration.
  const cricket::ContentInfo* FindMediaSectionForTransceiver(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      const SessionDescriptionInterface* sdesc) const;

 private:
  class ImplicitCreateSessionDescriptionObserver;
  friend class ImplicitCreateSessionDescriptionObserver;
  class SetSessionDescriptionObserverAdapter;
  friend class SetSessionDescriptionObserverAdapter;

  // Represents the [[LocalIceCredentialsToReplace]] internal slot in the spec.
  // It makes the next CreateOffer() produce new ICE credentials even if
  // RTCOfferAnswerOptions::ice_restart is false.
  // https://w3c.github.io/webrtc-pc/#dfn-localufragstoreplace
  // TODO(hbos): When JsepTransportController/JsepTransport supports rollback,
  // move this type of logic to JsepTransportController/JsepTransport.
  class LocalIceCredentialsToReplace;

  rtc::Thread* signaling_thread() const;
  // Non-const versions of local_description()/remote_description(), for use
  // internally.
  SessionDescriptionInterface* mutable_local_description()
      RTC_RUN_ON(signaling_thread()) {
    return pending_local_description_ ? pending_local_description_.get()
                                      : current_local_description_.get();
  }
  SessionDescriptionInterface* mutable_remote_description()
      RTC_RUN_ON(signaling_thread()) {
    return pending_remote_description_ ? pending_remote_description_.get()
                                       : current_remote_description_.get();
  }

  // Synchronous implementations of SetLocalDescription/SetRemoteDescription
  // that return an RTCError instead of invoking a callback.
  RTCError ApplyLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc);
  RTCError ApplyRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc);

  // Implementation of the offer/answer exchange operations. These are chained
  // onto the |operations_chain_| when the public CreateOffer(), CreateAnswer(),
  // SetLocalDescription() and SetRemoteDescription() methods are invoked.
  void DoCreateOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer);
  void DoCreateAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer);
  void DoSetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void DoSetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer);

  // Update the state, signaling if necessary.
  void ChangeSignalingState(
      PeerConnectionInterface::SignalingState signaling_state);

  RTCError UpdateSessionState(SdpType type,
                              cricket::ContentSource source,
                              const cricket::SessionDescription* description);

  bool IsUnifiedPlan() const RTC_RUN_ON(signaling_thread());

  // | desc_type | is the type of the description that caused the rollback.
  RTCError Rollback(SdpType desc_type);
  void OnOperationsChainEmpty();

  // Runs the algorithm **set the associated remote streams** specified in
  // https://w3c.github.io/webrtc-pc/#set-associated-remote-streams.
  void SetAssociatedRemoteStreams(
      rtc::scoped_refptr<RtpReceiverInternal> receiver,
      const std::vector<std::string>& stream_ids,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* added_streams,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  bool CheckIfNegotiationIsNeeded();
  void GenerateNegotiationNeededEvent();
  // Helper method which verifies SDP.
  RTCError ValidateSessionDescription(const SessionDescriptionInterface* sdesc,
                                      cricket::ContentSource source)
      RTC_RUN_ON(signaling_thread());

  // Updates the local RtpTransceivers according to the JSEP rules. Called as
  // part of setting the local/remote description.
  RTCError UpdateTransceiversAndDataChannels(
      cricket::ContentSource source,
      const SessionDescriptionInterface& new_session,
      const SessionDescriptionInterface* old_local_description,
      const SessionDescriptionInterface* old_remote_description);

  // Associate the given transceiver according to the JSEP rules.
  RTCErrorOr<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  AssociateTransceiver(cricket::ContentSource source,
                       SdpType type,
                       size_t mline_index,
                       const cricket::ContentInfo& content,
                       const cricket::ContentInfo* old_local_content,
                       const cricket::ContentInfo* old_remote_content)
      RTC_RUN_ON(signaling_thread());

  // If the BUNDLE policy is max-bundle, then we know for sure that all
  // transports will be bundled from the start. This method returns the BUNDLE
  // group if that's the case, or null if BUNDLE will be negotiated later. An
  // error is returned if max-bundle is specified but the session description
  // does not have a BUNDLE group.
  RTCErrorOr<const cricket::ContentGroup*> GetEarlyBundleGroup(
      const cricket::SessionDescription& desc) const
      RTC_RUN_ON(signaling_thread());

  // Either creates or destroys the transceiver's BaseChannel according to the
  // given media section.
  RTCError UpdateTransceiverChannel(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      const cricket::ContentInfo& content,
      const cricket::ContentGroup* bundle_group) RTC_RUN_ON(signaling_thread());

  // Either creates or destroys the local data channel according to the given
  // media section.
  RTCError UpdateDataChannel(cricket::ContentSource source,
                             const cricket::ContentInfo& content,
                             const cricket::ContentGroup* bundle_group)
      RTC_RUN_ON(signaling_thread());
  // Check if a call to SetLocalDescription is acceptable with a session
  // description of the given type.
  bool ExpectSetLocalDescription(SdpType type);
  // Check if a call to SetRemoteDescription is acceptable with a session
  // description of the given type.
  bool ExpectSetRemoteDescription(SdpType type);

  // The offer/answer machinery assumes the media section MID is present and
  // unique. To support legacy end points that do not supply a=mid lines, this
  // method will modify the session description to add MIDs generated according
  // to the SDP semantics.
  void FillInMissingRemoteMids(cricket::SessionDescription* remote_description);

  // Returns an RtpTransciever, if available, that can be used to receive the
  // given media type according to JSEP rules.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindAvailableTransceiverToReceive(cricket::MediaType media_type) const;

  // Returns a MediaSessionOptions struct with options decided by |options|,
  // the local MediaStreams and DataChannels.
  void GetOptionsForOffer(const PeerConnectionInterface::RTCOfferAnswerOptions&
                              offer_answer_options,
                          cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());
  void GetOptionsForUnifiedPlanOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  void GetOptionsForAnswer(const PeerConnectionInterface::RTCOfferAnswerOptions&
                               offer_answer_options,
                           cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());
  void GetOptionsForUnifiedPlanAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());

  // ===================================================================

  PeerConnection* const pc_;

  std::unique_ptr<WebRtcSessionDescriptionFactory> webrtc_session_desc_factory_
      RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<SessionDescriptionInterface> current_local_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> pending_local_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> current_remote_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> pending_remote_description_
      RTC_GUARDED_BY(signaling_thread());

  PeerConnectionInterface::SignalingState signaling_state_
      RTC_GUARDED_BY(signaling_thread()) = PeerConnectionInterface::kStable;

  // Whether this peer is the caller. Set when the local description is applied.
  absl::optional<bool> is_caller_ RTC_GUARDED_BY(signaling_thread());

  // The operations chain is used by the offer/answer exchange methods to ensure
  // they are executed in the right order. For example, if
  // SetRemoteDescription() is invoked while CreateOffer() is still pending, the
  // SRD operation will not start until CreateOffer() has completed. See
  // https://w3c.github.io/webrtc-pc/#dfn-operations-chain.
  rtc::scoped_refptr<rtc::OperationsChain> operations_chain_
      RTC_GUARDED_BY(signaling_thread());

  // List of content names for which the remote side triggered an ICE restart.
  std::set<std::string> pending_ice_restarts_
      RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<LocalIceCredentialsToReplace>
      local_ice_credentials_to_replace_ RTC_GUARDED_BY(signaling_thread());

  bool remote_peer_supports_msid_ RTC_GUARDED_BY(signaling_thread()) = false;
  bool is_negotiation_needed_ RTC_GUARDED_BY(signaling_thread()) = false;
  uint32_t negotiation_needed_event_id_ = 0;
  bool update_negotiation_needed_on_empty_chain_
      RTC_GUARDED_BY(signaling_thread()) = false;

  // In Unified Plan, if we encounter remote SDP that does not contain an a=msid
  // line we create and use a stream with a random ID for our receivers. This is
  // to support legacy endpoints that do not support the a=msid attribute (as
  // opposed to streamless tracks with "a=msid:-").
  rtc::scoped_refptr<MediaStreamInterface> missing_msid_default_stream_
      RTC_GUARDED_BY(signaling_thread());

  // Used when rolling back RTP data channels.
  bool have_pending_rtp_data_channel_ RTC_GUARDED_BY(signaling_thread()) =
      false;

  rtc::WeakPtrFactory<SdpOfferAnswerHandler> weak_ptr_factory_
      RTC_GUARDED_BY(signaling_thread());
};

}  // namespace webrtc

#endif  // PC_SDP_OFFER_ANSWER_H_
