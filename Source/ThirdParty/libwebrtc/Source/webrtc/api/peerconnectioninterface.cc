/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/peerconnectioninterface.h"

namespace webrtc {

PeerConnectionInterface::IceServer::IceServer() = default;
PeerConnectionInterface::IceServer::IceServer(const IceServer& rhs) = default;
PeerConnectionInterface::IceServer::~IceServer() = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration() = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration(
    const RTCConfiguration& rhs) = default;

PeerConnectionInterface::RTCConfiguration::RTCConfiguration(
    RTCConfigurationType type) {
  if (type == RTCConfigurationType::kAggressive) {
    // These parameters are also defined in Java and IOS configurations,
    // so their values may be overwritten by the Java or IOS configuration.
    bundle_policy = kBundlePolicyMaxBundle;
    rtcp_mux_policy = kRtcpMuxPolicyRequire;
    ice_connection_receiving_timeout = kAggressiveIceConnectionReceivingTimeout;

    // These parameters are not defined in Java or IOS configuration,
    // so their values will not be overwritten.
    enable_ice_renomination = true;
    redetermine_role_on_ice_restart = false;
  }
}

PeerConnectionInterface::RTCConfiguration::~RTCConfiguration() = default;

RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnectionInterface::AddTrack(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented");
}

bool PeerConnectionInterface::RemoveTrack(RtpSenderInterface* sender) {
  return RemoveTrackNew(sender).ok();
}

RTCError PeerConnectionInterface::RemoveTrackNew(
    rtc::scoped_refptr<RtpSenderInterface> sender) {
  return RTCError(RemoveTrack(sender) ? RTCErrorType::NONE
                                      : RTCErrorType::INTERNAL_ERROR);
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(cricket::MediaType media_type) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::AddTransceiver(cricket::MediaType media_type,
                                        const RtpTransceiverInit& init) {
  return RTCError(RTCErrorType::INTERNAL_ERROR, "not implemented");
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnectionInterface::CreateSender(
    const std::string& kind,
    const std::string& stream_id) {
  return rtc::scoped_refptr<RtpSenderInterface>();
}

std::vector<rtc::scoped_refptr<RtpSenderInterface>>
PeerConnectionInterface::GetSenders() const {
  return std::vector<rtc::scoped_refptr<RtpSenderInterface>>();
}

std::vector<rtc::scoped_refptr<RtpReceiverInterface>>
PeerConnectionInterface::GetReceivers() const {
  return std::vector<rtc::scoped_refptr<RtpReceiverInterface>>();
}

std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
PeerConnectionInterface::GetTransceivers() const {
  return std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>();
}

const SessionDescriptionInterface*
PeerConnectionInterface::current_local_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::current_remote_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::pending_local_description() const {
  return nullptr;
}

const SessionDescriptionInterface*
PeerConnectionInterface::pending_remote_description() const {
  return nullptr;
}

PeerConnectionInterface::RTCConfiguration
PeerConnectionInterface::GetConfiguration() {
  return PeerConnectionInterface::RTCConfiguration();
}

bool PeerConnectionInterface::SetConfiguration(
    const PeerConnectionInterface::RTCConfiguration& config,
    RTCError* error) {
  return false;
}

bool PeerConnectionInterface::SetConfiguration(
    const PeerConnectionInterface::RTCConfiguration& config) {
  return false;
}

bool PeerConnectionInterface::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  return false;
}

RTCError PeerConnectionInterface::SetBitrate(const BitrateSettings& bitrate) {
  BitrateParameters bitrate_parameters;
  bitrate_parameters.min_bitrate_bps = bitrate.min_bitrate_bps;
  bitrate_parameters.current_bitrate_bps = bitrate.start_bitrate_bps;
  bitrate_parameters.max_bitrate_bps = bitrate.max_bitrate_bps;
  return SetBitrate(bitrate_parameters);
}

RTCError PeerConnectionInterface::SetBitrate(
    const BitrateParameters& bitrate_parameters) {
  BitrateSettings bitrate;
  bitrate.min_bitrate_bps = bitrate_parameters.min_bitrate_bps;
  bitrate.start_bitrate_bps = bitrate_parameters.current_bitrate_bps;
  bitrate.max_bitrate_bps = bitrate_parameters.max_bitrate_bps;
  return SetBitrate(bitrate);
}

PeerConnectionInterface::PeerConnectionState
PeerConnectionInterface::peer_connection_state() {
  return PeerConnectionInterface::PeerConnectionState::kNew;
}

bool PeerConnectionInterface::StartRtcEventLog(rtc::PlatformFile file,
                                               int64_t max_size_bytes) {
  return false;
}

bool PeerConnectionInterface::StartRtcEventLog(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  return false;
}

PeerConnectionInterface::BitrateParameters::BitrateParameters() = default;

PeerConnectionInterface::BitrateParameters::~BitrateParameters() = default;

PeerConnectionDependencies::PeerConnectionDependencies(
    PeerConnectionObserver* observer_in)
    : observer(observer_in) {}

PeerConnectionDependencies::PeerConnectionDependencies(
    PeerConnectionDependencies&&) = default;

PeerConnectionDependencies::~PeerConnectionDependencies() = default;

PeerConnectionFactoryDependencies::PeerConnectionFactoryDependencies() =
    default;

PeerConnectionFactoryDependencies::PeerConnectionFactoryDependencies(
    PeerConnectionFactoryDependencies&&) = default;

PeerConnectionFactoryDependencies::~PeerConnectionFactoryDependencies() =
    default;

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactoryInterface::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  return nullptr;
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactoryInterface::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies dependencies) {
  return nullptr;
}

RtpCapabilities PeerConnectionFactoryInterface::GetRtpSenderCapabilities(
    cricket::MediaType kind) const {
  return {};
}

RtpCapabilities PeerConnectionFactoryInterface::GetRtpReceiverCapabilities(
    cricket::MediaType kind) const {
  return {};
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactoryInterface::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer) {
  return nullptr;
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactoryInterface::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer,
    const MediaConstraintsInterface* constraints) {
  return nullptr;
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactoryInterface::CreateVideoSource(
    cricket::VideoCapturer* capturer) {
  return CreateVideoSource(std::unique_ptr<cricket::VideoCapturer>(capturer));
}

}  // namespace webrtc
