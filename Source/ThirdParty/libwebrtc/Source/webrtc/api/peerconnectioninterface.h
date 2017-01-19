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
// Applications must use this interface to implement peerconnection.
// PeerConnectionFactory class provides factory methods to create
// peerconnection, mediastream and media tracks objects.
//
// The Following steps are needed to setup a typical call using Jsep.
// 1. Create a PeerConnectionFactoryInterface. Check constructors for more
// information about input parameters.
// 2. Create a PeerConnection object. Provide a configuration string which
// points either to stun or turn server to generate ICE candidates and provide
// an object that implements the PeerConnectionObserver interface.
// 3. Create local MediaStream and MediaTracks using the PeerConnectionFactory
// and add it to PeerConnection by calling AddStream.
// 4. Create an offer and serialize it and send it to the remote peer.
// 5. Once an ice candidate have been found PeerConnection will call the
// observer function OnIceCandidate. The candidates must also be serialized and
// sent to the remote peer.
// 6. Once an answer is received from the remote peer, call
// SetLocalSessionDescription with the offer and SetRemoteSessionDescription
// with the remote answer.
// 7. Once a remote candidate is received from the remote peer, provide it to
// the peerconnection by calling AddIceCandidate.


// The Receiver of a call can decide to accept or reject the call.
// This decision will be taken by the application not peerconnection.
// If application decides to accept the call
// 1. Create PeerConnectionFactoryInterface if it doesn't exist.
// 2. Create a new PeerConnection.
// 3. Provide the remote offer to the new PeerConnection object by calling
// SetRemoteSessionDescription.
// 4. Generate an answer to the remote offer by calling CreateAnswer and send it
// back to the remote peer.
// 5. Provide the local answer to the new PeerConnection by calling
// SetLocalSessionDescription with the answer.
// 6. Provide the remote ice candidates by calling AddIceCandidate.
// 7. Once a candidate have been found PeerConnection will call the observer
// function OnIceCandidate. Send these candidates to the remote peer.

#ifndef WEBRTC_API_PEERCONNECTIONINTERFACE_H_
#define WEBRTC_API_PEERCONNECTIONINTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/api/dtmfsenderinterface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/rtcstatscollector.h"
#include "webrtc/api/rtpreceiverinterface.h"
#include "webrtc/api/rtpsenderinterface.h"
#include "webrtc/api/statstypes.h"
#include "webrtc/api/umametrics.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/network.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/rtccertificategenerator.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/p2p/base/portallocator.h"

namespace rtc {
class SSLIdentity;
class Thread;
}

namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

namespace webrtc {
class AudioDeviceModule;
class MediaConstraintsInterface;

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

class MetricsObserverInterface : public rtc::RefCountInterface {
 public:

  // |type| is the type of the enum counter to be incremented. |counter|
  // is the particular counter in that type. |counter_max| is the next sequence
  // number after the highest counter.
  virtual void IncrementEnumCounter(PeerConnectionEnumCounterType type,
                                    int counter,
                                    int counter_max) {}

  // This is used to handle sparse counters like SSL cipher suites.
  // TODO(guoweis): Remove the implementation once the dependency's interface
  // definition is updated.
  virtual void IncrementSparseEnumCounter(PeerConnectionEnumCounterType type,
                                          int counter) {
    IncrementEnumCounter(type, counter, 0 /* Ignored */);
  }

  virtual void AddHistogramSample(PeerConnectionMetricsName type,
                                  int value) = 0;

 protected:
  virtual ~MetricsObserverInterface() {}
};

typedef MetricsObserverInterface UMAObserver;

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

  struct IceServer {
    // TODO(jbauch): Remove uri when all code using it has switched to urls.
    std::string uri;
    std::vector<std::string> urls;
    std::string username;
    std::string password;
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
    RTCConfiguration(RTCConfigurationType type) {
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
    // TODO(pthatcher): Rename this ice_transport_type, but update
    // Chromium at the same time.
    IceTransportsType type = kAll;
    // TODO(pthatcher): Rename this ice_servers, but update Chromium
    // at the same time.
    IceServers servers;
    BundlePolicy bundle_policy = kBundlePolicyBalanced;
    RtcpMuxPolicy rtcp_mux_policy = kRtcpMuxPolicyNegotiate;
    TcpCandidatePolicy tcp_candidate_policy = kTcpCandidatePolicyEnabled;
    CandidateNetworkPolicy candidate_network_policy =
        kCandidateNetworkPolicyAll;
    int audio_jitter_buffer_max_packets = kAudioJitterBufferMaxPackets;
    bool audio_jitter_buffer_fast_accelerate = false;
    int ice_connection_receiving_timeout = kUndefined;         // ms
    int ice_backup_candidate_pair_ping_interval = kUndefined;  // ms
    ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    bool prioritize_most_likely_ice_candidate_pairs = false;
    struct cricket::MediaConfig media_config;
    // Flags corresponding to values set by constraint flags.
    // rtc::Optional flags can be "missing", in which case the webrtc
    // default applies.
    bool disable_ipv6 = false;
    bool enable_rtp_data_channel = false;
    bool enable_quic = false;
    rtc::Optional<int> screencast_min_bitrate;
    rtc::Optional<bool> combined_audio_video_bwe;
    rtc::Optional<bool> enable_dtls_srtp;
    int ice_candidate_pool_size = 0;
    bool prune_turn_ports = false;
    // If set to true, this means the ICE transport should presume TURN-to-TURN
    // candidate pairs will succeed, even before a binding response is received.
    bool presume_writable_when_fully_relayed = false;
    // If true, "renomination" will be added to the ice options in the transport
    // description.
    bool enable_ice_renomination = false;
    // If true, ICE role is redetermined when peerconnection sets a local
    // transport description that indicates an ICE restart.
    bool redetermine_role_on_ice_restart = true;
  };

  struct RTCOfferAnswerOptions {
    static const int kUndefined = -1;
    static const int kMaxOfferToReceiveMedia = 1;

    // The default value for constraint offerToReceiveX:true.
    static const int kOfferToReceiveMediaTrue = 1;

    int offer_to_receive_video = kUndefined;
    int offer_to_receive_audio = kUndefined;
    bool voice_activity_detection = true;
    bool ice_restart = false;
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
  virtual bool AddStream(MediaStreamInterface* stream) = 0;

  // Remove a MediaStream from this PeerConnection.
  // Note that a SessionDescription negotiation is need before the
  // remote peer is notified.
  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  // TODO(deadbeef): Make the following two methods pure virtual once
  // implemented by all subclasses of PeerConnectionInterface.
  // Add a new MediaStreamTrack to be sent on this PeerConnection.
  // |streams| indicates which stream labels the track should be associated
  // with.
  virtual rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      MediaStreamTrackInterface* track,
      std::vector<MediaStreamInterface*> streams) {
    return nullptr;
  }

  // Remove an RtpSender from this PeerConnection.
  // Returns true on success.
  virtual bool RemoveTrack(RtpSenderInterface* sender) {
    return false;
  }

  // Returns pointer to the created DtmfSender on success.
  // Otherwise returns NULL.
  virtual rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) = 0;

  // TODO(deadbeef): Make these pure virtual once all subclasses implement them.
  // |kind| must be "audio" or "video".
  // |stream_id| is used to populate the msid attribute; if empty, one will
  // be generated automatically.
  virtual rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) {
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  virtual std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const {
    return std::vector<rtc::scoped_refptr<RtpSenderInterface>>();
  }

  virtual std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const {
    return std::vector<rtc::scoped_refptr<RtpReceiverInterface>>();
  }

  virtual bool GetStats(StatsObserver* observer,
                        MediaStreamTrackInterface* track,
                        StatsOutputLevel level) = 0;
  // Gets stats using the new stats collection API, see webrtc/api/stats/. These
  // will replace old stats collection API when the new API has matured enough.
  // TODO(hbos): Default implementation that does nothing only exists as to not
  // break third party projects. As soon as they have been updated this should
  // be changed to "= 0;".
  virtual void GetStats(RTCStatsCollectorCallback* callback) {}

  virtual rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) = 0;

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;

  // Create a new offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const MediaConstraintsInterface* constraints) {}

  // TODO(jiayl): remove the default impl and the old interface when chromium
  // code is updated.
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const RTCOfferAnswerOptions& options) {}

  // Create an answer to an offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const RTCOfferAnswerOptions& options) {}
  // Deprecated - use version above.
  // TODO(hta): Remove and remove default implementations when all callers
  // are updated.
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const MediaConstraintsInterface* constraints) {}

  // Sets the local session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  // Sets the remote session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc) = 0;
  // Restarts or updates the ICE Agent process of gathering local candidates
  // and pinging remote candidates.
  // TODO(deadbeef): Remove once Chrome is moved over to SetConfiguration.
  virtual bool UpdateIce(const IceServers& configuration,
                         const MediaConstraintsInterface* constraints) {
    return false;
  }
  virtual bool UpdateIce(const IceServers& configuration) { return false; }
  // Sets the PeerConnection's global configuration to |config|.
  // Any changes to STUN/TURN servers or ICE candidate policy will affect the
  // next gathering phase, and cause the next call to createOffer to generate
  // new ICE credentials. Note that the BUNDLE and RTCP-multiplexing policies
  // cannot be changed with this method.
  // TODO(deadbeef): Make this pure virtual once all Chrome subclasses of
  // PeerConnectionInterface implement it.
  virtual bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& config) {
    return false;
  }
  // Provides a remote candidate to the ICE Agent.
  // A copy of the |candidate| will be created and added to the remote
  // description. So the caller of this method still has the ownership of the
  // |candidate|.
  // TODO(ronghuawu): Consider to change this so that the AddIceCandidate will
  // take the ownership of the |candidate|.
  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;

  // Removes a group of remote candidates from the ICE agent.
  virtual bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) {
    return false;
  }

  virtual void RegisterUMAObserver(UMAObserver* observer) = 0;

  // Returns the current SignalingState.
  virtual SignalingState signaling_state() = 0;
  virtual IceConnectionState ice_connection_state() = 0;
  virtual IceGatheringState ice_gathering_state() = 0;

  // Starts RtcEventLog using existing file. Takes ownership of |file| and
  // passes it on to Call, which will take the ownership. If the
  // operation fails the file will be closed. The logging will stop
  // automatically after 10 minutes have passed, or when the StopRtcEventLog
  // function is called.
  // TODO(ivoc): Make this pure virtual when Chrome is updated.
  virtual bool StartRtcEventLog(rtc::PlatformFile file,
                                int64_t max_size_bytes) {
    return false;
  }

  // Stops logging the RtcEventLog.
  // TODO(ivoc): Make this pure virtual when Chrome is updated.
  virtual void StopRtcEventLog() {}

  // Terminates all media and closes the transport.
  virtual void Close() = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionInterface() {}
};

// PeerConnection callback interface. Application should implement these
// methods.
class PeerConnectionObserver {
 public:
  enum StateType {
    kSignalingState,
    kIceState,
  };

  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) = 0;

  // TODO(deadbeef): Once all subclasses override the scoped_refptr versions
  // of the below three methods, make them pure virtual and remove the raw
  // pointer version.

  // Triggered when media is received on a new stream from remote peer.
  virtual void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) {}
  // Deprecated; please use the version that uses a scoped_refptr.
  virtual void OnAddStream(MediaStreamInterface* stream) {}

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) {
  }
  // Deprecated; please use the version that uses a scoped_refptr.
  virtual void OnRemoveStream(MediaStreamInterface* stream) {}

  // Triggered when a remote peer opens a data channel.
  virtual void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel){};
  // Deprecated; please use the version that uses a scoped_refptr.
  virtual void OnDataChannel(DataChannelInterface* data_channel) {}

  // Triggered when renegotiation is needed. For example, an ICE restart
  // has begun.
  virtual void OnRenegotiationNeeded() = 0;

  // Called any time the IceConnectionState changes.
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
      const std::vector<cricket::Candidate>& candidates) {}

  // Called when the ICE connection receiving status changes.
  virtual void OnIceConnectionReceivingChange(bool receiving) {}

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

// PeerConnectionFactoryInterface is the factory interface use for creating
// PeerConnection, MediaStream and media tracks.
// PeerConnectionFactoryInterface will create required libjingle threads,
// socket and network manager factory classes for networking.
// If an application decides to provide its own threads and network
// implementation of these classes it should use the alternate
// CreatePeerConnectionFactory method which accepts threads as input and use the
// CreatePeerConnection version that takes a PortAllocator as an
// argument.
class PeerConnectionFactoryInterface : public rtc::RefCountInterface {
 public:
  class Options {
   public:
    Options()
        : disable_encryption(false),
          disable_sctp_data_channels(false),
          disable_network_monitor(false),
          network_ignore_mask(rtc::kDefaultNetworkIgnoreMask),
          ssl_max_version(rtc::SSL_PROTOCOL_DTLS_12),
          crypto_options(rtc::CryptoOptions::NoGcm()) {}
    bool disable_encryption;
    bool disable_sctp_data_channels;
    bool disable_network_monitor;

    // Sets the network types to ignore. For instance, calling this with
    // ADAPTER_TYPE_ETHERNET | ADAPTER_TYPE_LOOPBACK will ignore Ethernet and
    // loopback interfaces.
    int network_ignore_mask;

    // Sets the maximum supported protocol version. The highest version
    // supported by both ends will be used for the connection, i.e. if one
    // party supports DTLS 1.0 and the other DTLS 1.2, DTLS 1.0 will be used.
    rtc::SSLProtocolVersion ssl_max_version;

    // Sets crypto related options, e.g. enabled cipher suites.
    rtc::CryptoOptions crypto_options;
  };

  virtual void SetOptions(const Options& options) = 0;

  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) = 0;

  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) = 0;

  virtual rtc::scoped_refptr<MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) = 0;

  // Creates a AudioSourceInterface.
  // |constraints| decides audio processing settings but can be NULL.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) = 0;
  // Deprecated - use version above.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a VideoTrackSourceInterface. The new source take ownership of
  // |capturer|.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer) = 0;
  // A video source creator that allows selection of resolution and frame rate.
  // |constraints| decides video resolution and frame rate but can
  // be NULL.
  // In the NULL case, use the version above.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a new local VideoTrack. The same |source| can be used in several
  // tracks.
  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label,
      VideoTrackSourceInterface* source) = 0;

  // Creates an new AudioTrack. At the moment |source| can be NULL.
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

  // This function is deprecated and will be removed when Chrome is updated to
  // use the equivalent function on PeerConnectionInterface.
  // TODO(ivoc) Remove after Chrome is updated.
  virtual bool StartRtcEventLog(rtc::PlatformFile file,
                                int64_t max_size_bytes) = 0;
  // This function is deprecated and will be removed when Chrome is updated to
  // use the equivalent function on PeerConnectionInterface.
  // TODO(ivoc) Remove after Chrome is updated.
  virtual bool StartRtcEventLog(rtc::PlatformFile file) = 0;

  // This function is deprecated and will be removed when Chrome is updated to
  // use the equivalent function on PeerConnectionInterface.
  // TODO(ivoc) Remove after Chrome is updated.
  virtual void StopRtcEventLog() = 0;

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
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory();

// Create a new instance of PeerConnectionFactoryInterface.
//
// |network_thread|, |worker_thread| and |signaling_thread| are
// the only mandatory parameters.
//
// If non-null, ownership of |default_adm|, |encoder_factory| and
// |decoder_factory| are transferred to the returned factory.
rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

// Create a new instance of PeerConnectionFactoryInterface.
// Same thread is used as worker and network thread.
inline rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_and_network_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  return CreatePeerConnectionFactory(
      worker_and_network_thread, worker_and_network_thread, signaling_thread,
      default_adm, encoder_factory, decoder_factory);
}

}  // namespace webrtc

#endif  // WEBRTC_API_PEERCONNECTIONINTERFACE_H_
