/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the PeerConnection interface as defined in
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#peer-to-peer-connections.
//
// The PeerConnectionFactory class provides factory methods to create
// PeerConnection, MediaStream and MediaStreamTrack objects.
//
// The following steps are needed to setup a typical call using WebRTC:
//
// 1. Create a PeerConnectionFactoryInterface. Check constructors for more
// information about input parameters.
//
// 2. Create a PeerConnection object. Provide a configuration struct which
// points to STUN and/or TURN servers used to generate ICE candidates, and
// provide an object that implements the PeerConnectionObserver interface,
// which is used to receive callbacks from the PeerConnection.
//
// 3. Create local MediaStreamTracks using the PeerConnectionFactory and add
// them to PeerConnection by calling AddTrack (or legacy method, AddStream).
//
// 4. Create an offer, call SetLocalDescription with it, serialize it, and send
// it to the remote peer
//
// 5. Once an ICE candidate has been gathered, the PeerConnection will call the
// observer function OnIceCandidate. The candidates must also be serialized and
// sent to the remote peer.
//
// 6. Once an answer is received from the remote peer, call
// SetRemoteDescription with the remote answer.
//
// 7. Once a remote candidate is received from the remote peer, provide it to
// the PeerConnection by calling AddIceCandidate.
//
// The receiver of a call (assuming the application is "call"-based) can decide
// to accept or reject the call; this decision will be taken by the application,
// not the PeerConnection.
//
// If the application decides to accept the call, it should:
//
// 1. Create PeerConnectionFactoryInterface if it doesn't exist.
//
// 2. Create a new PeerConnection.
//
// 3. Provide the remote offer to the new PeerConnection object by calling
// SetRemoteDescription.
//
// 4. Generate an answer to the remote offer by calling CreateAnswer and send it
// back to the remote peer.
//
// 5. Provide the local answer to the new PeerConnection by calling
// SetLocalDescription with the answer.
//
// 6. Provide the remote ICE candidates by calling AddIceCandidate.
//
// 7. Once a candidate has been gathered, the PeerConnection will call the
// observer function OnIceCandidate. Send these candidates to the remote peer.

#ifndef API_PEERCONNECTIONINTERFACE_H_
#define API_PEERCONNECTIONINTERFACE_H_

// TODO(sakal): Remove this define after migration to virtual PeerConnection
// observer is complete.
#define VIRTUAL_PEERCONNECTION_OBSERVER_DESTRUCTOR

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/datachannelinterface.h"
#include "api/dtmfsenderinterface.h"
#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/rtcerror.h"
#include "api/rtceventlogoutput.h"
#include "api/rtpreceiverinterface.h"
#include "api/rtpsenderinterface.h"
#include "api/rtptransceiverinterface.h"
#include "api/setremotedescriptionobserverinterface.h"
#include "api/stats/rtcstatscollectorcallback.h"
#include "api/statstypes.h"
#include "api/turncustomizer.h"
#include "api/umametrics.h"
#include "call/callfactoryinterface.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "media/base/mediachannel.h"
#include "media/base/videocapturer.h"
#include "p2p/base/portallocator.h"
#include "rtc_base/network.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/rtccertificategenerator.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/sslstreamadapter.h"

namespace rtc {
class SSLIdentity;
class Thread;
}

namespace cricket {
class MediaEngineInterface;
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

namespace webrtc {
class AudioDeviceModule;
class AudioMixer;
class CallFactoryInterface;
class MediaConstraintsInterface;
class VideoDecoderFactory;
class VideoEncoderFactory;

// MediaStream container interface.
class StreamCollectionInterface : public rtc::RefCountInterface {
 public:
  // TODO(ronghuawu): Update the function names to c++ style, e.g. find -> Find.
  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;
  virtual MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) = 0;
  virtual MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~StreamCollectionInterface() {}
};

class StatsObserver : public rtc::RefCountInterface {
 public:
  virtual void OnComplete(const StatsReports& reports) = 0;

 protected:
  virtual ~StatsObserver() {}
};

// For now, kDefault is interpreted as kPlanB.
// TODO(bugs.webrtc.org/8530): Switch default to kUnifiedPlan.
enum class SdpSemantics { kDefault, kPlanB, kUnifiedPlan };

class PeerConnectionInterface : public rtc::RefCountInterface {
 public:
  // See http://dev.w3.org/2011/webrtc/editor/webrtc.html#state-definitions .
  enum SignalingState {
    kStable,
    kHaveLocalOffer,
    kHaveLocalPrAnswer,
    kHaveRemoteOffer,
    kHaveRemotePrAnswer,
    kClosed,
  };

  enum IceGatheringState {
    kIceGatheringNew,
    kIceGatheringGathering,
    kIceGatheringComplete
  };

  enum IceConnectionState {
    kIceConnectionNew,
    kIceConnectionChecking,
    kIceConnectionConnected,
    kIceConnectionCompleted,
    kIceConnectionFailed,
    kIceConnectionDisconnected,
    kIceConnectionClosed,
    kIceConnectionMax,
  };

  // TLS certificate policy.
  enum TlsCertPolicy {
    // For TLS based protocols, ensure the connection is secure by not
    // circumventing certificate validation.
    kTlsCertPolicySecure,
    // For TLS based protocols, disregard security completely by skipping
    // certificate validation. This is insecure and should never be used unless
    // security is irrelevant in that particular context.
    kTlsCertPolicyInsecureNoCheck,
  };

  struct IceServer {
    // TODO(jbauch): Remove uri when all code using it has switched to urls.
    // List of URIs associated with this server. Valid formats are described
    // in RFC7064 and RFC7065, and more may be added in the future. The "host"
    // part of the URI may contain either an IP address or a hostname.
    std::string uri;
    std::vector<std::string> urls;
    std::string username;
    std::string password;
    TlsCertPolicy tls_cert_policy = kTlsCertPolicySecure;
    // If the URIs in |urls| only contain IP addresses, this field can be used
    // to indicate the hostname, which may be necessary for TLS (using the SNI
    // extension). If |urls| itself contains the hostname, this isn't
    // necessary.
    std::string hostname;
    // List of protocols to be used in the TLS ALPN extension.
    std::vector<std::string> tls_alpn_protocols;
    // List of elliptic curves to be used in the TLS elliptic curves extension.
    std::vector<std::string> tls_elliptic_curves;

    bool operator==(const IceServer& o) const {
      return uri == o.uri && urls == o.urls && username == o.username &&
             password == o.password && tls_cert_policy == o.tls_cert_policy &&
             hostname == o.hostname &&
             tls_alpn_protocols == o.tls_alpn_protocols &&
             tls_elliptic_curves == o.tls_elliptic_curves;
    }
    bool operator!=(const IceServer& o) const { return !(*this == o); }
  };
  typedef std::vector<IceServer> IceServers;

  enum IceTransportsType {
    // TODO(pthatcher): Rename these kTransporTypeXXX, but update
    // Chromium at the same time.
    kNone,
    kRelay,
    kNoHost,
    kAll
  };

  // https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-08#section-4.1.1
  enum BundlePolicy {
    kBundlePolicyBalanced,
    kBundlePolicyMaxBundle,
    kBundlePolicyMaxCompat
  };

  // https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-09#section-4.1.1
  enum RtcpMuxPolicy {
    kRtcpMuxPolicyNegotiate,
    kRtcpMuxPolicyRequire,
  };

  enum TcpCandidatePolicy {
    kTcpCandidatePolicyEnabled,
    kTcpCandidatePolicyDisabled
  };

  enum CandidateNetworkPolicy {
    kCandidateNetworkPolicyAll,
    kCandidateNetworkPolicyLowCost
  };

  enum ContinualGatheringPolicy {
    GATHER_ONCE,
    GATHER_CONTINUALLY
  };

  enum class RTCConfigurationType {
    // A configuration that is safer to use, despite not having the best
    // performance. Currently this is the default configuration.
    kSafe,
    // An aggressive configuration that has better performance, although it
    // may be riskier and may need extra support in the application.
    kAggressive
  };

  // TODO(hbos): Change into class with private data and public getters.
  // TODO(nisse): In particular, accessing fields directly from an
  // application is brittle, since the organization mirrors the
  // organization of the implementation, which isn't stable. So we
  // need getters and setters at least for fields which applications
  // are interested in.
  struct RTCConfiguration {
    // This struct is subject to reorganization, both for naming
    // consistency, and to group settings to match where they are used
    // in the implementation. To do that, we need getter and setter
    // methods for all settings which are of interest to applications,
    // Chrome in particular.

    RTCConfiguration() = default;
    explicit RTCConfiguration(RTCConfigurationType type) {
      if (type == RTCConfigurationType::kAggressive) {
        // These parameters are also defined in Java and IOS configurations,
        // so their values may be overwritten by the Java or IOS configuration.
        bundle_policy = kBundlePolicyMaxBundle;
        rtcp_mux_policy = kRtcpMuxPolicyRequire;
        ice_connection_receiving_timeout =
            kAggressiveIceConnectionReceivingTimeout;

        // These parameters are not defined in Java or IOS configuration,
        // so their values will not be overwritten.
        enable_ice_renomination = true;
        redetermine_role_on_ice_restart = false;
      }
    }

    bool operator==(const RTCConfiguration& o) const;
    bool operator!=(const RTCConfiguration& o) const;

    bool dscp() { return media_config.enable_dscp; }
    void set_dscp(bool enable) { media_config.enable_dscp = enable; }

    // TODO(nisse): The corresponding flag in MediaConfig and
    // elsewhere should be renamed enable_cpu_adaptation.
    bool cpu_adaptation() {
      return media_config.video.enable_cpu_overuse_detection;
    }
    void set_cpu_adaptation(bool enable) {
      media_config.video.enable_cpu_overuse_detection = enable;
    }

    bool suspend_below_min_bitrate() {
      return media_config.video.suspend_below_min_bitrate;
    }
    void set_suspend_below_min_bitrate(bool enable) {
      media_config.video.suspend_below_min_bitrate = enable;
    }

    // TODO(nisse): The negation in the corresponding MediaConfig
    // attribute is inconsistent, and it should be renamed at some
    // point.
    bool prerenderer_smoothing() {
      return !media_config.video.disable_prerenderer_smoothing;
    }
    void set_prerenderer_smoothing(bool enable) {
      media_config.video.disable_prerenderer_smoothing = !enable;
    }

    static const int kUndefined = -1;
    // Default maximum number of packets in the audio jitter buffer.
    static const int kAudioJitterBufferMaxPackets = 50;
    // ICE connection receiving timeout for aggressive configuration.
    static const int kAggressiveIceConnectionReceivingTimeout = 1000;

    ////////////////////////////////////////////////////////////////////////
    // The below few fields mirror the standard RTCConfiguration dictionary:
    // https://www.w3.org/TR/webrtc/#rtcconfiguration-dictionary
    ////////////////////////////////////////////////////////////////////////

    // TODO(pthatcher): Rename this ice_servers, but update Chromium
    // at the same time.
    IceServers servers;
    // TODO(pthatcher): Rename this ice_transport_type, but update
    // Chromium at the same time.
    IceTransportsType type = kAll;
    BundlePolicy bundle_policy = kBundlePolicyBalanced;
    RtcpMuxPolicy rtcp_mux_policy = kRtcpMuxPolicyRequire;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    int ice_candidate_pool_size = 0;

    //////////////////////////////////////////////////////////////////////////
    // The below fields correspond to constraints from the deprecated
    // constraints interface for constructing a PeerConnection.
    //
    // rtc::Optional fields can be "missing", in which case the implementation
    // default will be used.
    //////////////////////////////////////////////////////////////////////////

    // If set to true, don't gather IPv6 ICE candidates.
    // TODO(deadbeef): Remove this? IPv6 support has long stopped being
    // experimental
    bool disable_ipv6 = false;

    // If set to true, don't gather IPv6 ICE candidates on Wi-Fi.
    // Only intended to be used on specific devices. Certain phones disable IPv6
    // when the screen is turned off and it would be better to just disable the
    // IPv6 ICE candidates on Wi-Fi in those cases.
    bool disable_ipv6_on_wifi = false;

    // By default, the PeerConnection will use a limited number of IPv6 network
    // interfaces, in order to avoid too many ICE candidate pairs being created
    // and delaying ICE completion.
    //
    // Can be set to INT_MAX to effectively disable the limit.
    int max_ipv6_networks = cricket::kDefaultMaxIPv6Networks;

    // If set to true, use RTP data channels instead of SCTP.
    // TODO(deadbeef): Remove this. We no longer commit to supporting RTP data
    // channels, though some applications are still working on moving off of
    // them.
    bool enable_rtp_data_channel = false;

    // Minimum bitrate at which screencast video tracks will be encoded at.
    // This means adding padding bits up to this bitrate, which can help
    // when switching from a static scene to one with motion.
    rtc::Optional<int> screencast_min_bitrate;

    // Use new combined audio/video bandwidth estimation?
    rtc::Optional<bool> combined_audio_video_bwe;

    // Can be used to disable DTLS-SRTP. This should never be done, but can be
    // useful for testing purposes, for example in setting up a loopback call
    // with a single PeerConnection.
    rtc::Optional<bool> enable_dtls_srtp;

    /////////////////////////////////////////////////
    // The below fields are not part of the standard.
    /////////////////////////////////////////////////

    // Can be used to disable TCP candidate generation.
    TcpCandidatePolicy tcp_candidate_policy = kTcpCandidatePolicyEnabled;

    // Can be used to avoid gathering candidates for a "higher cost" network,
    // if a lower cost one exists. For example, if both Wi-Fi and cellular
    // interfaces are available, this could be used to avoid using the cellular
    // interface.
    CandidateNetworkPolicy candidate_network_policy =
        kCandidateNetworkPolicyAll;

    // The maximum number of packets that can be stored in the NetEq audio
    // jitter buffer. Can be reduced to lower tolerated audio latency.
    int audio_jitter_buffer_max_packets = kAudioJitterBufferMaxPackets;

    // Whether to use the NetEq "fast mode" which will accelerate audio quicker
    // if it falls behind.
    bool audio_jitter_buffer_fast_accelerate = false;

    // Timeout in milliseconds before an ICE candidate pair is considered to be
    // "not receiving", after which a lower priority candidate pair may be
    // selected.
    int ice_connection_receiving_timeout = kUndefined;

    // Interval in milliseconds at which an ICE "backup" candidate pair will be
    // pinged. This is a candidate pair which is not actively in use, but may
    // be switched to if the active candidate pair becomes unusable.
    //
    // This is relevant mainly to Wi-Fi/cell handoff; the application may not
    // want this backup cellular candidate pair pinged frequently, since it
    // consumes data/battery.
    int ice_backup_candidate_pair_ping_interval = kUndefined;

    // Can be used to enable continual gathering, which means new candidates
    // will be gathered as network interfaces change. Note that if continual
    // gathering is used, the candidate removal API should also be used, to
    // avoid an ever-growing list of candidates.
    ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;

    // If set to true, candidate pairs will be pinged in order of most likely
    // to work (which means using a TURN server, generally), rather than in
    // standard priority order.
    bool prioritize_most_likely_ice_candidate_pairs = false;

    struct cricket::MediaConfig media_config;

    // If set to true, only one preferred TURN allocation will be used per
    // network interface. UDP is preferred over TCP and IPv6 over IPv4. This
    // can be used to cut down on the number of candidate pairings.
    bool prune_turn_ports = false;

    // If set to true, this means the ICE transport should presume TURN-to-TURN
    // candidate pairs will succeed, even before a binding response is received.
    // This can be used to optimize the initial connection time, since the DTLS
    // handshake can begin immediately.
    bool presume_writable_when_fully_relayed = false;

    // If true, "renomination" will be added to the ice options in the transport
    // description.
    // See: https://tools.ietf.org/html/draft-thatcher-ice-renomination-00
    bool enable_ice_renomination = false;

    // If true, the ICE role is re-determined when the PeerConnection sets a
    // local transport description that indicates an ICE restart.
    //
    // This is standard RFC5245 ICE behavior, but causes unnecessary role
    // thrashing, so an application may wish to avoid it. This role
    // re-determining was removed in ICEbis (ICE v2).
    bool redetermine_role_on_ice_restart = true;

    // If set, the min interval (max rate) at which we will send ICE checks
    // (STUN pings), in milliseconds.
    rtc::Optional<int> ice_check_min_interval;

    // ICE Periodic Regathering
    // If set, WebRTC will periodically create and propose candidates without
    // starting a new ICE generation. The regathering happens continuously with
    // interval specified in milliseconds by the uniform distribution [a, b].
    rtc::Optional<rtc::IntervalRange> ice_regather_interval_range;

    // Optional TurnCustomizer.
    // With this class one can modify outgoing TURN messages.
    // The object passed in must remain valid until PeerConnection::Close() is
    // called.
    webrtc::TurnCustomizer* turn_customizer = nullptr;

    // Configure the SDP semantics used by this PeerConnection. Note that the
    // WebRTC 1.0 specification requires kUnifiedPlan semantics. The
    // RtpTransceiver API is only available with kUnifiedPlan semantics.
    //
    // kPlanB will cause PeerConnection to create offers and answers with at
    // most one audio and one video m= section with multiple RtpSenders and
    // RtpReceivers specified as multiple a=ssrc lines within the section. This
    // will also cause PeerConnection to reject offers/answers with multiple m=
    // sections of the same media type.
    //
    // kUnifiedPlan will cause PeerConnection to create offers and answers with
    // multiple m= sections where each m= section maps to one RtpSender and one
    // RtpReceiver (an RtpTransceiver), either both audio or both video. Plan B
    // style offers or answers will be rejected in calls to SetLocalDescription
    // or SetRemoteDescription.
    //
    // For users who only send at most one audio and one video track, this
    // choice does not matter and should be left as kDefault.
    //
    // For users who wish to send multiple audio/video streams and need to stay
    // interoperable with legacy WebRTC implementations, specify kPlanB.
    //
    // For users who wish to send multiple audio/video streams and/or wish to
    // use the new RtpTransceiver API, specify kUnifiedPlan.
    //
    // TODO(steveanton): Implement support for kUnifiedPlan.
    SdpSemantics sdp_semantics = SdpSemantics::kDefault;

    //
    // Don't forget to update operator== if adding something.
    //
  };

  // See: https://www.w3.org/TR/webrtc/#idl-def-rtcofferansweroptions
  struct RTCOfferAnswerOptions {
    static const int kUndefined = -1;
    static const int kMaxOfferToReceiveMedia = 1;

    // The default value for constraint offerToReceiveX:true.
    static const int kOfferToReceiveMediaTrue = 1;

    // These have been removed from the standard in favor of the "transceiver"
    // API, but given that we don't support that API, we still have them here.
    //
    // offer_to_receive_X set to 1 will cause a media description to be
    // generated in the offer, even if no tracks of that type have been added.
    // Values greater than 1 are treated the same.
    //
    // If set to 0, the generated directional attribute will not include the
    // "recv" direction (meaning it will be "sendonly" or "inactive".
    int offer_to_receive_video = kUndefined;
    int offer_to_receive_audio = kUndefined;

    bool voice_activity_detection = true;
    bool ice_restart = false;

    // If true, will offer to BUNDLE audio/video/data together. Not to be
    // confused with RTCP mux (multiplexing RTP and RTCP together).
    bool use_rtp_mux = true;

    RTCOfferAnswerOptions() = default;

    RTCOfferAnswerOptions(int offer_to_receive_video,
                          int offer_to_receive_audio,
                          bool voice_activity_detection,
                          bool ice_restart,
                          bool use_rtp_mux)
        : offer_to_receive_video(offer_to_receive_video),
          offer_to_receive_audio(offer_to_receive_audio),
          voice_activity_detection(voice_activity_detection),
          ice_restart(ice_restart),
          use_rtp_mux(use_rtp_mux) {}
  };

  // Used by GetStats to decide which stats to include in the stats reports.
  // |kStatsOutputLevelStandard| includes the standard stats for Javascript API;
  // |kStatsOutputLevelDebug| includes both the standard stats and additional
  // stats for debugging purposes.
  enum StatsOutputLevel {
    kStatsOutputLevelStandard,
    kStatsOutputLevelDebug,
  };

  // Accessor methods to active local streams.
  virtual rtc::scoped_refptr<StreamCollectionInterface>
      local_streams() = 0;

  // Accessor methods to remote streams.
  virtual rtc::scoped_refptr<StreamCollectionInterface>
      remote_streams() = 0;

  // Add a new MediaStream to be sent on this PeerConnection.
  // Note that a SessionDescription negotiation is needed before the
  // remote peer can receive the stream.
  //
  // This has been removed from the standard in favor of a track-based API. So,
  // this is equivalent to simply calling AddTrack for each track within the
  // stream, with the one difference that if "stream->AddTrack(...)" is called
  // later, the PeerConnection will automatically pick up the new track. Though
  // this functionality will be deprecated in the future.
  virtual bool AddStream(MediaStreamInterface* stream) = 0;

  // Remove a MediaStream from this PeerConnection.
  // Note that a SessionDescription negotiation is needed before the
  // remote peer is notified.
  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  // Add a new MediaStreamTrack to be sent on this PeerConnection, and return
  // the newly created RtpSender. The RtpSender will be associated with the
  // streams specified in the |stream_labels| list.
  //
  // Errors:
  // - INVALID_PARAMETER: |track| is null, has a kind other than audio or video,
  //       or a sender already exists for the track.
  // - INVALID_STATE: The PeerConnection is closed.
  // TODO(steveanton): Remove default implementation once downstream
  // implementations have been updated.
  virtual RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface>,
      const std::vector<std::string>&) {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented");
  }
  // |streams| indicates which stream labels the track should be associated
  // with.
  // TODO(steveanton): Remove this overload once callers have moved to the
  // signature with stream labels.
  virtual rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      MediaStreamTrackInterface* track,
      std::vector<MediaStreamInterface*> streams) = 0;

  // Remove an RtpSender from this PeerConnection.
  // Returns true on success.
  virtual bool RemoveTrack(RtpSenderInterface* sender) = 0;

  // AddTransceiver creates a new RtpTransceiver and adds it to the set of
  // transceivers. Adding a transceiver will cause future calls to CreateOffer
  // to add a media description for the corresponding transceiver.
  //
  // The initial value of |mid| in the returned transceiver is null. Setting a
  // new session description may change it to a non-null value.
  //
  // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-addtransceiver
  //
  // Optionally, an RtpTransceiverInit structure can be specified to configure
  // the transceiver from construction. If not specified, the transceiver will
  // default to having a direction of kSendRecv and not be part of any streams.
  //
  // These methods are only available when Unified Plan is enabled (see
  // RTCConfiguration).
  //
  // Common errors:
  // - INTERNAL_ERROR: The configuration does not have Unified Plan enabled.
  // TODO(steveanton): Make these pure virtual once downstream projects have
  // updated.

  // Adds a transceiver with a sender set to transmit the given track. The kind
  // of the transceiver (and sender/receiver) will be derived from the kind of
  // the track.
  // Errors:
  // - INVALID_PARAMETER: |track| is null.
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(rtc::scoped_refptr<MediaStreamTrackInterface>) {
    return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
  }
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(rtc::scoped_refptr<MediaStreamTrackInterface>,
                 const RtpTransceiverInit&) {
    return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
  }

  // Adds a transceiver with the given kind. Can either be MEDIA_TYPE_AUDIO or
  // MEDIA_TYPE_VIDEO.
  // Errors:
  // - INVALID_PARAMETER: |media_type| is not MEDIA_TYPE_AUDIO or
  //                      MEDIA_TYPE_VIDEO.
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(cricket::MediaType) {
    return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
  }
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(cricket::MediaType,
                 const RtpTransceiverInit&) {
    return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
  }

  // Returns pointer to a DtmfSender on success. Otherwise returns null.
  //
  // This API is no longer part of the standard; instead DtmfSenders are
  // obtained from RtpSenders. Which is what the implementation does; it finds
  // an RtpSender for |track| and just returns its DtmfSender.
  virtual rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) = 0;

  // TODO(deadbeef): Make these pure virtual once all subclasses implement them.

  // Creates a sender without a track. Can be used for "early media"/"warmup"
  // use cases, where the application may want to negotiate video attributes
  // before a track is available to send.
  //
  // The standard way to do this would be through "addTransceiver", but we
  // don't support that API yet.
  //
  // |kind| must be "audio" or "video".
  //
  // |stream_id| is used to populate the msid attribute; if empty, one will
  // be generated automatically.
  virtual rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& /* kind */,
      const std::string& /* stream_id */) {
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  // Get all RtpSenders, created either through AddStream, AddTrack, or
  // CreateSender. Note that these are "Plan B SDP" RtpSenders, not "Unified
  // Plan SDP" RtpSenders, which means that all senders of a specific media
  // type share the same media description.
  virtual std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const {
    return std::vector<rtc::scoped_refptr<RtpSenderInterface>>();
  }

  // Get all RtpReceivers, created when a remote description is applied.
  // Note that these are "Plan B SDP" RtpReceivers, not "Unified Plan SDP"
  // RtpReceivers, which means that all receivers of a specific media type
  // share the same media description.
  //
  // It is also possible to have a media description with no associated
  // RtpReceivers, if the directional attribute does not indicate that the
  // remote peer is sending any media.
  virtual std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const {
    return std::vector<rtc::scoped_refptr<RtpReceiverInterface>>();
  }

  // Get all RtpTransceivers, created either through AddTransceiver, AddTrack or
  // by a remote description applied with SetRemoteDescription.
  // Note: This method is only available when Unified Plan is enabled (see
  // RTCConfiguration).
  virtual std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
  GetTransceivers() const {
    return {};
  }

  virtual bool GetStats(StatsObserver* observer,
                        MediaStreamTrackInterface* track,
                        StatsOutputLevel level) = 0;
  // Gets stats using the new stats collection API, see webrtc/api/stats/. These
  // will replace old stats collection API when the new API has matured enough.
  // TODO(hbos): Default implementation that does nothing only exists as to not
  // break third party projects. As soon as they have been updated this should
  // be changed to "= 0;".
  virtual void GetStats(RTCStatsCollectorCallback*) {}
  // Clear cached stats in the rtcstatscollector.
  // Exposed for testing while waiting for automatic cache clear to work.
  // https://bugs.webrtc.org/8693
  virtual void ClearStatsCache() {}

  // Create a data channel with the provided config, or default config if none
  // is provided. Note that an offer/answer negotiation is still necessary
  // before the data channel can be used.
  //
  // Also, calling CreateDataChannel is the only way to get a data "m=" section
  // in SDP, so it should be done before CreateOffer is called, if the
  // application plans to use data channels.
  virtual rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) = 0;

  // Returns the more recently applied description; "pending" if it exists, and
  // otherwise "current". See below.
  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;

  // A "current" description the one currently negotiated from a complete
  // offer/answer exchange.
  virtual const SessionDescriptionInterface* current_local_description() const {
    return nullptr;
  }
  virtual const SessionDescriptionInterface* current_remote_description()
      const {
    return nullptr;
  }

  // A "pending" description is one that's part of an incomplete offer/answer
  // exchange (thus, either an offer or a pranswer). Once the offer/answer
  // exchange is finished, the "pending" description will become "current".
  virtual const SessionDescriptionInterface* pending_local_description() const {
    return nullptr;
  }
  virtual const SessionDescriptionInterface* pending_remote_description()
      const {
    return nullptr;
  }

  // Create a new offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateOffer(CreateSessionDescriptionObserver*,
                           const MediaConstraintsInterface*) {}

  // TODO(jiayl): remove the default impl and the old interface when chromium
  // code is updated.
  virtual void CreateOffer(CreateSessionDescriptionObserver*,
                           const RTCOfferAnswerOptions&) {}

  // Create an answer to an offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateAnswer(CreateSessionDescriptionObserver*,
                            const RTCOfferAnswerOptions&) {}
  // Deprecated - use version above.
  // TODO(hta): Remove and remove default implementations when all callers
  // are updated.
  virtual void CreateAnswer(CreateSessionDescriptionObserver*,
                            const MediaConstraintsInterface*) {}

  // Sets the local session description.
  // The PeerConnection takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  // TODO(deadbeef): Change |desc| to be a unique_ptr, to make it clear
  // that this method always takes ownership of it.
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  // Sets the remote session description.
  // The PeerConnection takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  // TODO(hbos): Remove when Chrome implements the new signature.
  virtual void SetRemoteDescription(SetSessionDescriptionObserver*,
                                    SessionDescriptionInterface*) {}
  // TODO(hbos): Make pure virtual when Chrome has updated its signature.
  virtual void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface>,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface>) {}
  // Deprecated; Replaced by SetConfiguration.
  // TODO(deadbeef): Remove once Chrome is moved over to SetConfiguration.
  virtual bool UpdateIce(const IceServers&,
                         const MediaConstraintsInterface*) {
    return false;
  }
  virtual bool UpdateIce(const IceServers&) { return false; }

  // TODO(deadbeef): Make this pure virtual once all Chrome subclasses of
  // PeerConnectionInterface implement it.
  virtual PeerConnectionInterface::RTCConfiguration GetConfiguration() {
    return PeerConnectionInterface::RTCConfiguration();
  }

  // Sets the PeerConnection's global configuration to |config|.
  //
  // The members of |config| that may be changed are |type|, |servers|,
  // |ice_candidate_pool_size| and |prune_turn_ports| (though the candidate
  // pool size can't be changed after the first call to SetLocalDescription).
  // Note that this means the BUNDLE and RTCP-multiplexing policies cannot be
  // changed with this method.
  //
  // Any changes to STUN/TURN servers or ICE candidate policy will affect the
  // next gathering phase, and cause the next call to createOffer to generate
  // new ICE credentials, as described in JSEP. This also occurs when
  // |prune_turn_ports| changes, for the same reasoning.
  //
  // If an error occurs, returns false and populates |error| if non-null:
  // - INVALID_MODIFICATION if |config| contains a modified parameter other
  //   than one of the parameters listed above.
  // - INVALID_RANGE if |ice_candidate_pool_size| is out of range.
  // - SYNTAX_ERROR if parsing an ICE server URL failed.
  // - INVALID_PARAMETER if a TURN server is missing |username| or |password|.
  // - INTERNAL_ERROR if an unexpected error occurred.
  //
  // TODO(deadbeef): Make this pure virtual once all Chrome subclasses of
  // PeerConnectionInterface implement it.
  virtual bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration&,
      RTCError*) {
    return false;
  }
  // Version without error output param for backwards compatibility.
  // TODO(deadbeef): Remove once chromium is updated.
  virtual bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration&) {
    return false;
  }

  // Provides a remote candidate to the ICE Agent.
  // A copy of the |candidate| will be created and added to the remote
  // description. So the caller of this method still has the ownership of the
  // |candidate|.
  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;

  // Removes a group of remote candidates from the ICE agent. Needed mainly for
  // continual gathering, to avoid an ever-growing list of candidates as
  // networks come and go.
  virtual bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>&) {
    return false;
  }

  // Register a metric observer (used by chromium). It's reference counted, and
  // this method takes a reference. RegisterUMAObserver(nullptr) will release
  // the reference.
  // TODO(deadbeef): Take argument as scoped_refptr?
  virtual void RegisterUMAObserver(UMAObserver* observer) = 0;

  // 0 <= min <= current <= max should hold for set parameters.
  struct BitrateParameters {
    rtc::Optional<int> min_bitrate_bps;
    rtc::Optional<int> current_bitrate_bps;
    rtc::Optional<int> max_bitrate_bps;
  };

  // SetBitrate limits the bandwidth allocated for all RTP streams sent by
  // this PeerConnection. Other limitations might affect these limits and
  // are respected (for example "b=AS" in SDP).
  //
  // Setting |current_bitrate_bps| will reset the current bitrate estimate
  // to the provided value.
  virtual RTCError SetBitrate(const BitrateParameters& bitrate) = 0;

  // Sets current strategy. If not set default WebRTC allocator will be used.
  // May be changed during an active session. The strategy
  // ownership is passed with std::unique_ptr
  // TODO(alexnarest): Make this pure virtual when tests will be updated
  virtual void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>) {}

  // Enable/disable playout of received audio streams. Enabled by default. Note
  // that even if playout is enabled, streams will only be played out if the
  // appropriate SDP is also applied. Setting |playout| to false will stop
  // playout of the underlying audio device but starts a task which will poll
  // for audio data every 10ms to ensure that audio processing happens and the
  // audio statistics are updated.
  // TODO(henrika): deprecate and remove this.
  virtual void SetAudioPlayout(bool /* playout */) {}

  // Enable/disable recording of transmitted audio streams. Enabled by default.
  // Note that even if recording is enabled, streams will only be recorded if
  // the appropriate SDP is also applied.
  // TODO(henrika): deprecate and remove this.
  virtual void SetAudioRecording(bool /* recording */) {}

  // Returns the current SignalingState.
  virtual SignalingState signaling_state() = 0;

  // Returns the aggregate state of all ICE *and* DTLS transports.
  // TODO(deadbeef): Implement "PeerConnectionState" according to the standard,
  // to aggregate ICE+DTLS state, and change the scope of IceConnectionState to
  // be just the ICE layer. See: crbug.com/webrtc/6145
  virtual IceConnectionState ice_connection_state() = 0;

  virtual IceGatheringState ice_gathering_state() = 0;

  // Starts RtcEventLog using existing file. Takes ownership of |file| and
  // passes it on to Call, which will take the ownership. If the
  // operation fails the file will be closed. The logging will stop
  // automatically after 10 minutes have passed, or when the StopRtcEventLog
  // function is called.
  // TODO(eladalon): Deprecate and remove this.
  virtual bool StartRtcEventLog(rtc::PlatformFile,
                                int64_t /* max_size_bytes */) {
    return false;
  }

  // Start RtcEventLog using an existing output-sink. Takes ownership of
  // |output| and passes it on to Call, which will take the ownership. If the
  // operation fails the output will be closed and deallocated. The event log
  // will send serialized events to the output object every |output_period_ms|.
  virtual bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput>,
                                int64_t /* output_period_ms */) {
    return false;
  }

  // Stops logging the RtcEventLog.
  // TODO(ivoc): Make this pure virtual when Chrome is updated.
  virtual void StopRtcEventLog() {}

  // Terminates all media, closes the transports, and in general releases any
  // resources used by the PeerConnection. This is an irreversible operation.
  //
  // Note that after this method completes, the PeerConnection will no longer
  // use the PeerConnectionObserver interface passed in on construction, and
  // thus the observer object can be safely destroyed.
  virtual void Close() = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionInterface() {}
};

// PeerConnection callback interface, used for RTCPeerConnection events.
// Application should implement these methods.
class PeerConnectionObserver {
 public:
  enum StateType {
    kSignalingState,
    kIceState,
  };

  virtual ~PeerConnectionObserver() = default;

  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) = 0;

  // TODO(deadbeef): Once all subclasses override the scoped_refptr versions
  // of the below three methods, make them pure virtual and remove the raw
  // pointer version.

  // Triggered when media is received on a new stream from remote peer.
  virtual void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) = 0;

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(
      rtc::scoped_refptr<MediaStreamInterface> stream) = 0;

  // Triggered when a remote peer opens a data channel.
  virtual void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) = 0;

  // Triggered when renegotiation is needed. For example, an ICE restart
  // has begun.
  virtual void OnRenegotiationNeeded() = 0;

  // Called any time the IceConnectionState changes.
  //
  // Note that our ICE states lag behind the standard slightly. The most
  // notable differences include the fact that "failed" occurs after 15
  // seconds, not 30, and this actually represents a combination ICE + DTLS
  // state, so it may be "failed" if DTLS fails while ICE succeeds.
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) = 0;

  // Called any time the IceGatheringState changes.
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) = 0;

  // A new ICE candidate has been gathered.
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;

  // Ice candidates have been removed.
  // TODO(honghaiz): Make this a pure virtual method when all its subclasses
  // implement it.
  virtual void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>&) {}

  // Called when the ICE connection receiving status changes.
  virtual void OnIceConnectionReceivingChange(bool) {}

  // This is called when a receiver and its track is created.
  // TODO(zhihuang): Make this pure virtual when all subclasses implement it.
  virtual void OnAddTrack(
      rtc::scoped_refptr<RtpReceiverInterface>,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&) {}

  // TODO(hbos,deadbeef): Add |OnAssociatedStreamsUpdated| with |receiver| and
  // |streams| as arguments. This should be called when an existing receiver its
  // associated streams updated. https://crbug.com/webrtc/8315
  // This may be blocked on supporting multiple streams per sender or else
  // this may count as the removal and addition of a track?
  // https://crbug.com/webrtc/7932

  // Called when a receiver is completely removed. This is current (Plan B SDP)
  // behavior that occurs when processing the removal of a remote track, and is
  // called when the receiver is removed and the track is muted. When Unified
  // Plan SDP is supported, transceivers can change direction (and receivers
  // stopped) but receivers are never removed.
  // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
  // TODO(hbos,deadbeef): When Unified Plan SDP is supported and receivers are
  // no longer removed, deprecate and remove this callback.
  // TODO(hbos,deadbeef): Make pure virtual when all subclasses implement it.
  virtual void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface>) {}
};

// PeerConnectionFactoryInterface is the factory interface used for creating
// PeerConnection, MediaStream and MediaStreamTrack objects.
//
// The simplest method for obtaiing one, CreatePeerConnectionFactory will
// create the required libjingle threads, socket and network manager factory
// classes for networking if none are provided, though it requires that the
// application runs a message loop on the thread that called the method (see
// explanation below)
//
// If an application decides to provide its own threads and/or implementation
// of networking classes, it should use the alternate
// CreatePeerConnectionFactory method which accepts threads as input, and use
// the CreatePeerConnection version that takes a PortAllocator as an argument.
class PeerConnectionFactoryInterface : public rtc::RefCountInterface {
 public:
  class Options {
   public:
    Options() : crypto_options(rtc::CryptoOptions::NoGcm()) {}

    // If set to true, created PeerConnections won't enforce any SRTP
    // requirement, allowing unsecured media. Should only be used for
    // testing/debugging.
    bool disable_encryption = false;

    // Deprecated. The only effect of setting this to true is that
    // CreateDataChannel will fail, which is not that useful.
    bool disable_sctp_data_channels = false;

    // If set to true, any platform-supported network monitoring capability
    // won't be used, and instead networks will only be updated via polling.
    //
    // This only has an effect if a PeerConnection is created with the default
    // PortAllocator implementation.
    bool disable_network_monitor = false;

    // Sets the network types to ignore. For instance, calling this with
    // ADAPTER_TYPE_ETHERNET | ADAPTER_TYPE_LOOPBACK will ignore Ethernet and
    // loopback interfaces.
    int network_ignore_mask = rtc::kDefaultNetworkIgnoreMask;

    // Sets the maximum supported protocol version. The highest version
    // supported by both ends will be used for the connection, i.e. if one
    // party supports DTLS 1.0 and the other DTLS 1.2, DTLS 1.0 will be used.
    rtc::SSLProtocolVersion ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;

    // Sets crypto related options, e.g. enabled cipher suites.
    rtc::CryptoOptions crypto_options;
  };

  // Set the options to be used for subsequently created PeerConnections.
  virtual void SetOptions(const Options& options) = 0;

  // |allocator| and |cert_generator| may be null, in which case default
  // implementations will be used.
  //
  // |observer| must not be null.
  //
  // Note that this method does not take ownership of |observer|; it's the
  // responsibility of the caller to delete it. It can be safely deleted after
  // Close has been called on the returned PeerConnection, which ensures no
  // more observer callbacks will be invoked.
  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) = 0;

  // Deprecated; should use RTCConfiguration for everything that previously
  // used constraints.
  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) = 0;

  virtual rtc::scoped_refptr<MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) = 0;

  // Creates an AudioSourceInterface.
  // |options| decides audio processing settings.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) = 0;
  // Deprecated - use version above.
  // Can use CopyConstraintsIntoAudioOptions to bridge the gap.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a VideoTrackSourceInterface from |capturer|.
  // TODO(deadbeef): We should aim to remove cricket::VideoCapturer from the
  // API. It's mainly used as a wrapper around webrtc's provided
  // platform-specific capturers, but these should be refactored to use
  // VideoTrackSourceInterface directly.
  // TODO(deadbeef): Make pure virtual once downstream mock PC factory classes
  // are updated.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      std::unique_ptr<cricket::VideoCapturer>) {
    return nullptr;
  }

  // A video source creator that allows selection of resolution and frame rate.
  // |constraints| decides video resolution and frame rate but can be null.
  // In the null case, use the version above.
  //
  // |constraints| is only used for the invocation of this method, and can
  // safely be destroyed afterwards.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      std::unique_ptr<cricket::VideoCapturer>,
      const MediaConstraintsInterface*) {
    return nullptr;
  }

  // Deprecated; please use the versions that take unique_ptrs above.
  // TODO(deadbeef): Remove these once safe to do so.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer) {
    return CreateVideoSource(std::unique_ptr<cricket::VideoCapturer>(capturer));
  }
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints) {
    return CreateVideoSource(std::unique_ptr<cricket::VideoCapturer>(capturer),
                             constraints);
  }

  // Creates a new local VideoTrack. The same |source| can be used in several
  // tracks.
  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label,
      VideoTrackSourceInterface* source) = 0;

  // Creates an new AudioTrack. At the moment |source| can be null.
  virtual rtc::scoped_refptr<AudioTrackInterface>
      CreateAudioTrack(const std::string& label,
                       AudioSourceInterface* source) = 0;

  // Starts AEC dump using existing file. Takes ownership of |file| and passes
  // it on to VoiceEngine (via other objects) immediately, which will take
  // the ownerhip. If the operation fails, the file will be closed.
  // A maximum file size in bytes can be specified. When the file size limit is
  // reached, logging is stopped automatically. If max_size_bytes is set to a
  // value <= 0, no limit will be used, and logging will continue until the
  // StopAecDump function is called.
  virtual bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes) = 0;

  // Stops logging the AEC dump.
  virtual void StopAecDump() = 0;

 protected:
  // Dtor and ctor protected as objects shouldn't be created or deleted via
  // this interface.
  PeerConnectionFactoryInterface() {}
  ~PeerConnectionFactoryInterface() {} // NOLINT
};

// Create a new instance of PeerConnectionFactoryInterface.
//
// This method relies on the thread it's called on as the "signaling thread"
// for the PeerConnectionFactory it creates.
//
// As such, if the current thread is not already running an rtc::Thread message
// loop, an application using this method must eventually either call
// rtc::Thread::Current()->Run(), or call
// rtc::Thread::Current()->ProcessMessages() within the application's own
// message loop.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory);

// Create a new instance of PeerConnectionFactoryInterface.
//
// |network_thread|, |worker_thread| and |signaling_thread| are
// the only mandatory parameters.
//
// If non-null, a reference is added to |default_adm|, and ownership of
// |video_encoder_factory| and |video_decoder_factory| is transferred to the
// returned factory.
// TODO(deadbeef): Use rtc::scoped_refptr<> and std::unique_ptr<> to make this
// ownership transfer and ref counting more obvious.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory);

// Create a new instance of PeerConnectionFactoryInterface with optional
// external audio mixed and audio processing modules.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
// If |audio_processing| is null, an internal audio processing module will be
// created and used.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing);

// Create a new instance of PeerConnectionFactoryInterface with optional video
// codec factories. These video factories represents all video codecs, i.e. no
// extra internal video codecs will be added.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing);

// Create a new instance of PeerConnectionFactoryInterface with external audio
// mixer.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer);

// Create a new instance of PeerConnectionFactoryInterface.
// Same thread is used as worker and network thread.
inline rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_and_network_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreatePeerConnectionFactory(
      worker_and_network_thread, worker_and_network_thread, signaling_thread,
      default_adm, audio_encoder_factory, audio_decoder_factory,
      video_encoder_factory, video_decoder_factory);
}

// This is a lower-level version of the CreatePeerConnectionFactory functions
// above. It's implemented in the "peerconnection" build target, whereas the
// above methods are only implemented in the broader "libjingle_peerconnection"
// build target, which pulls in the implementations of every module webrtc may
// use.
//
// If an application knows it will only require certain modules, it can reduce
// webrtc's impact on its binary size by depending only on the "peerconnection"
// target and the modules the application requires, using
// CreateModularPeerConnectionFactory instead of one of the
// CreatePeerConnectionFactory methods above. For example, if an application
// only uses WebRTC for audio, it can pass in null pointers for the
// video-specific interfaces, and omit the corresponding modules from its
// build.
//
// If |network_thread| or |worker_thread| are null, the PeerConnectionFactory
// will create the necessary thread internally. If |signaling_thread| is null,
// the PeerConnectionFactory will use the thread on which this method is called
// as the signaling thread, wrapping it in an rtc::Thread object if needed.
//
// If non-null, a reference is added to |default_adm|, and ownership of
// |video_encoder_factory| and |video_decoder_factory| is transferred to the
// returned factory.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
//
// TODO(deadbeef): Use rtc::scoped_refptr<> and std::unique_ptr<> to make this
// ownership transfer and ref counting more obvious.
//
// TODO(deadbeef): Encapsulate these modules in a struct, so that when a new
// module is inevitably exposed, we can just add a field to the struct instead
// of adding a whole new CreateModularPeerConnectionFactory overload.
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory);

}  // namespace webrtc

#endif  // API_PEERCONNECTIONINTERFACE_H_
