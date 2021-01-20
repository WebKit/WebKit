/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection+Private.h"

#import "RTCConfiguration+Private.h"
#import "RTCDataChannel+Private.h"
#import "RTCIceCandidate+Private.h"
#import "RTCLegacyStatsReport+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"
#import "RTCRtpTransceiver+Private.h"
#import "RTCSessionDescription+Private.h"
#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include <memory>

#include "api/jsep_ice_candidate.h"
#include "api/rtc_event_log_output_file.h"
#include "api/transport/media/media_transport_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

NSString * const kRTCPeerConnectionErrorDomain =
    @"org.webrtc.RTCPeerConnection";
int const kRTCPeerConnnectionSessionDescriptionError = -1;

namespace webrtc {

class CreateSessionDescriptionObserverAdapter
    : public CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserverAdapter(
      void (^completionHandler)(RTCSessionDescription *sessionDescription,
                                NSError *error)) {
    completion_handler_ = completionHandler;
  }

  ~CreateSessionDescriptionObserverAdapter() override { completion_handler_ = nil; }

  void OnSuccess(SessionDescriptionInterface *desc) override {
    RTC_DCHECK(completion_handler_);
    std::unique_ptr<webrtc::SessionDescriptionInterface> description =
        std::unique_ptr<webrtc::SessionDescriptionInterface>(desc);
    RTCSessionDescription* session =
        [[RTCSessionDescription alloc] initWithNativeDescription:
            description.get()];
    completion_handler_(session, nil);
    completion_handler_ = nil;
  }

  void OnFailure(RTCError error) override {
    RTC_DCHECK(completion_handler_);
    // TODO(hta): Add handling of error.type()
    NSString *str = [NSString stringForStdString:error.message()];
    NSError* err =
        [NSError errorWithDomain:kRTCPeerConnectionErrorDomain
                            code:kRTCPeerConnnectionSessionDescriptionError
                        userInfo:@{ NSLocalizedDescriptionKey : str }];
    completion_handler_(nil, err);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)
      (RTCSessionDescription *sessionDescription, NSError *error);
};

class SetSessionDescriptionObserverAdapter :
    public SetSessionDescriptionObserver {
 public:
  SetSessionDescriptionObserverAdapter(void (^completionHandler)
      (NSError *error)) {
    completion_handler_ = completionHandler;
  }

  ~SetSessionDescriptionObserverAdapter() override { completion_handler_ = nil; }

  void OnSuccess() override {
    RTC_DCHECK(completion_handler_);
    completion_handler_(nil);
    completion_handler_ = nil;
  }

  void OnFailure(RTCError error) override {
    RTC_DCHECK(completion_handler_);
    // TODO(hta): Add handling of error.type()
    NSString *str = [NSString stringForStdString:error.message()];
    NSError* err =
        [NSError errorWithDomain:kRTCPeerConnectionErrorDomain
                            code:kRTCPeerConnnectionSessionDescriptionError
                        userInfo:@{ NSLocalizedDescriptionKey : str }];
    completion_handler_(err);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)(NSError *error);
};

PeerConnectionDelegateAdapter::PeerConnectionDelegateAdapter(
    RTCPeerConnection *peerConnection) {
  peer_connection_ = peerConnection;
}

PeerConnectionDelegateAdapter::~PeerConnectionDelegateAdapter() {
  peer_connection_ = nil;
}

void PeerConnectionDelegateAdapter::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  RTCSignalingState state =
      [[RTCPeerConnection class] signalingStateForNativeState:new_state];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                   didChangeSignalingState:state];
}

void PeerConnectionDelegateAdapter::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  RTCPeerConnection *peer_connection = peer_connection_;
  RTCMediaStream *mediaStream =
      [[RTCMediaStream alloc] initWithFactory:peer_connection.factory nativeMediaStream:stream];
  [peer_connection.delegate peerConnection:peer_connection
                              didAddStream:mediaStream];
}

void PeerConnectionDelegateAdapter::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  RTCPeerConnection *peer_connection = peer_connection_;
  RTCMediaStream *mediaStream =
      [[RTCMediaStream alloc] initWithFactory:peer_connection.factory nativeMediaStream:stream];

  [peer_connection.delegate peerConnection:peer_connection
                           didRemoveStream:mediaStream];
}

void PeerConnectionDelegateAdapter::OnTrack(
    rtc::scoped_refptr<RtpTransceiverInterface> nativeTransceiver) {
  RTCPeerConnection *peer_connection = peer_connection_;
  RTCRtpTransceiver *transceiver =
      [[RTCRtpTransceiver alloc] initWithFactory:peer_connection.factory
                            nativeRtpTransceiver:nativeTransceiver];
  if ([peer_connection.delegate
          respondsToSelector:@selector(peerConnection:didStartReceivingOnTransceiver:)]) {
    [peer_connection.delegate peerConnection:peer_connection
              didStartReceivingOnTransceiver:transceiver];
  }
}

void PeerConnectionDelegateAdapter::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> data_channel) {
  RTCPeerConnection *peer_connection = peer_connection_;
  RTCDataChannel *dataChannel = [[RTCDataChannel alloc] initWithFactory:peer_connection.factory
                                                      nativeDataChannel:data_channel];
  [peer_connection.delegate peerConnection:peer_connection
                        didOpenDataChannel:dataChannel];
}

void PeerConnectionDelegateAdapter::OnRenegotiationNeeded() {
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnectionShouldNegotiate:peer_connection];
}

void PeerConnectionDelegateAdapter::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  RTCIceConnectionState state = [RTCPeerConnection iceConnectionStateForNativeState:new_state];
  [peer_connection_.delegate peerConnection:peer_connection_ didChangeIceConnectionState:state];
}

void PeerConnectionDelegateAdapter::OnStandardizedIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  if ([peer_connection_.delegate
          respondsToSelector:@selector(peerConnection:didChangeStandardizedIceConnectionState:)]) {
    RTCIceConnectionState state = [RTCPeerConnection iceConnectionStateForNativeState:new_state];
    [peer_connection_.delegate peerConnection:peer_connection_
        didChangeStandardizedIceConnectionState:state];
  }
}

void PeerConnectionDelegateAdapter::OnConnectionChange(
    PeerConnectionInterface::PeerConnectionState new_state) {
  if ([peer_connection_.delegate
          respondsToSelector:@selector(peerConnection:didChangeConnectionState:)]) {
    RTCPeerConnectionState state = [RTCPeerConnection connectionStateForNativeState:new_state];
    [peer_connection_.delegate peerConnection:peer_connection_ didChangeConnectionState:state];
  }
}

void PeerConnectionDelegateAdapter::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  RTCIceGatheringState state =
      [[RTCPeerConnection class] iceGatheringStateForNativeState:new_state];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                didChangeIceGatheringState:state];
}

void PeerConnectionDelegateAdapter::OnIceCandidate(
    const IceCandidateInterface *candidate) {
  RTCIceCandidate *iceCandidate =
      [[RTCIceCandidate alloc] initWithNativeCandidate:candidate];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                   didGenerateIceCandidate:iceCandidate];
}

void PeerConnectionDelegateAdapter::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  NSMutableArray* ice_candidates =
      [NSMutableArray arrayWithCapacity:candidates.size()];
  for (const auto& candidate : candidates) {
    std::unique_ptr<JsepIceCandidate> candidate_wrapper(
        new JsepIceCandidate(candidate.transport_name(), -1, candidate));
    RTCIceCandidate* ice_candidate = [[RTCIceCandidate alloc]
        initWithNativeCandidate:candidate_wrapper.get()];
    [ice_candidates addObject:ice_candidate];
  }
  RTCPeerConnection* peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                    didRemoveIceCandidates:ice_candidates];
}

void PeerConnectionDelegateAdapter::OnIceSelectedCandidatePairChanged(
    const cricket::CandidatePairChangeEvent &event) {
  const auto &selected_pair = event.selected_candidate_pair;
  auto local_candidate_wrapper = std::make_unique<JsepIceCandidate>(
      selected_pair.local_candidate().transport_name(), -1, selected_pair.local_candidate());
  RTCIceCandidate *local_candidate =
      [[RTCIceCandidate alloc] initWithNativeCandidate:local_candidate_wrapper.release()];
  auto remote_candidate_wrapper = std::make_unique<JsepIceCandidate>(
      selected_pair.remote_candidate().transport_name(), -1, selected_pair.remote_candidate());
  RTCIceCandidate *remote_candidate =
      [[RTCIceCandidate alloc] initWithNativeCandidate:remote_candidate_wrapper.release()];
  RTCPeerConnection *peer_connection = peer_connection_;
  NSString *nsstr_reason = [NSString stringForStdString:event.reason];
  if ([peer_connection.delegate
          respondsToSelector:@selector
          (peerConnection:didChangeLocalCandidate:remoteCandidate:lastReceivedMs:changeReason:)]) {
    [peer_connection.delegate peerConnection:peer_connection
                     didChangeLocalCandidate:local_candidate
                             remoteCandidate:remote_candidate
                              lastReceivedMs:event.last_data_received_ms
                                changeReason:nsstr_reason];
  }
}

void PeerConnectionDelegateAdapter::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>> &streams) {
  RTCPeerConnection *peer_connection = peer_connection_;
  if ([peer_connection.delegate respondsToSelector:@selector(peerConnection:
                                                             didAddReceiver:streams:)]) {
    NSMutableArray *mediaStreams = [NSMutableArray arrayWithCapacity:streams.size()];
    for (const auto &nativeStream : streams) {
      RTCMediaStream *mediaStream = [[RTCMediaStream alloc] initWithFactory:peer_connection.factory
                                                          nativeMediaStream:nativeStream];
      [mediaStreams addObject:mediaStream];
    }
    RTCRtpReceiver *rtpReceiver = [[RTCRtpReceiver alloc] initWithFactory:peer_connection.factory
                                                        nativeRtpReceiver:receiver];

    [peer_connection.delegate peerConnection:peer_connection
                              didAddReceiver:rtpReceiver
                                     streams:mediaStreams];
  }
}

void PeerConnectionDelegateAdapter::OnRemoveTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver) {
  RTCPeerConnection *peer_connection = peer_connection_;
  if ([peer_connection.delegate respondsToSelector:@selector(peerConnection:didRemoveReceiver:)]) {
    RTCRtpReceiver *rtpReceiver = [[RTCRtpReceiver alloc] initWithFactory:peer_connection.factory
                                                        nativeRtpReceiver:receiver];
    [peer_connection.delegate peerConnection:peer_connection didRemoveReceiver:rtpReceiver];
  }
}

}  // namespace webrtc

@implementation RTCPeerConnection {
  RTCPeerConnectionFactory *_factory;
  NSMutableArray<RTCMediaStream *> *_localStreams;
  std::unique_ptr<webrtc::PeerConnectionDelegateAdapter> _observer;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
  std::unique_ptr<webrtc::MediaConstraints> _nativeConstraints;
  BOOL _hasStartedRtcEventLog;
}

@synthesize delegate = _delegate;
@synthesize factory = _factory;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                  configuration:(RTCConfiguration *)configuration
                    constraints:(RTCMediaConstraints *)constraints
                       delegate:(id<RTCPeerConnectionDelegate>)delegate {
  NSParameterAssert(factory);
  std::unique_ptr<webrtc::PeerConnectionDependencies> dependencies =
      std::make_unique<webrtc::PeerConnectionDependencies>(nullptr);
  return [self initWithDependencies:factory
                      configuration:configuration
                        constraints:constraints
                       dependencies:std::move(dependencies)
                           delegate:delegate];
}

- (instancetype)initWithDependencies:(RTCPeerConnectionFactory *)factory
                       configuration:(RTCConfiguration *)configuration
                         constraints:(RTCMediaConstraints *)constraints
                        dependencies:
                            (std::unique_ptr<webrtc::PeerConnectionDependencies>)dependencies
                            delegate:(id<RTCPeerConnectionDelegate>)delegate {
  NSParameterAssert(factory);
  NSParameterAssert(dependencies.get());
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration> config(
      [configuration createNativeConfiguration]);
  if (!config) {
    return nil;
  }
  if (self = [super init]) {
    _observer.reset(new webrtc::PeerConnectionDelegateAdapter(self));
    _nativeConstraints = constraints.nativeConstraints;
    CopyConstraintsIntoRtcConfiguration(_nativeConstraints.get(), config.get());

    webrtc::PeerConnectionDependencies deps = std::move(*dependencies.release());
    deps.observer = _observer.get();
    _peerConnection = factory.nativeFactory->CreatePeerConnection(*config, std::move(deps));

    if (!_peerConnection) {
      return nil;
    }
    _factory = factory;
    _localStreams = [[NSMutableArray alloc] init];
    _delegate = delegate;
  }
  return self;
}

- (NSArray<RTCMediaStream *> *)localStreams {
  return [_localStreams copy];
}

- (RTCSessionDescription *)localDescription {
  const webrtc::SessionDescriptionInterface *description =
      _peerConnection->local_description();
  return description ?
      [[RTCSessionDescription alloc] initWithNativeDescription:description]
          : nil;
}

- (RTCSessionDescription *)remoteDescription {
  const webrtc::SessionDescriptionInterface *description =
      _peerConnection->remote_description();
  return description ?
      [[RTCSessionDescription alloc] initWithNativeDescription:description]
          : nil;
}

- (RTCSignalingState)signalingState {
  return [[self class]
      signalingStateForNativeState:_peerConnection->signaling_state()];
}

- (RTCIceConnectionState)iceConnectionState {
  return [[self class] iceConnectionStateForNativeState:
      _peerConnection->ice_connection_state()];
}

- (RTCPeerConnectionState)connectionState {
  return [[self class] connectionStateForNativeState:_peerConnection->peer_connection_state()];
}

- (RTCIceGatheringState)iceGatheringState {
  return [[self class] iceGatheringStateForNativeState:
      _peerConnection->ice_gathering_state()];
}

- (BOOL)setConfiguration:(RTCConfiguration *)configuration {
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration> config(
      [configuration createNativeConfiguration]);
  if (!config) {
    return NO;
  }
  CopyConstraintsIntoRtcConfiguration(_nativeConstraints.get(),
                                      config.get());
  return _peerConnection->SetConfiguration(*config).ok();
}

- (RTCConfiguration *)configuration {
  webrtc::PeerConnectionInterface::RTCConfiguration config =
    _peerConnection->GetConfiguration();
  return [[RTCConfiguration alloc] initWithNativeConfiguration:config];
}

- (void)close {
  _peerConnection->Close();
}

- (void)addIceCandidate:(RTCIceCandidate *)candidate {
  std::unique_ptr<const webrtc::IceCandidateInterface> iceCandidate(
      candidate.nativeCandidate);
  _peerConnection->AddIceCandidate(iceCandidate.get());
}

- (void)removeIceCandidates:(NSArray<RTCIceCandidate *> *)iceCandidates {
  std::vector<cricket::Candidate> candidates;
  for (RTCIceCandidate *iceCandidate in iceCandidates) {
    std::unique_ptr<const webrtc::IceCandidateInterface> candidate(
        iceCandidate.nativeCandidate);
    if (candidate) {
      candidates.push_back(candidate->candidate());
      // Need to fill the transport name from the sdp_mid.
      candidates.back().set_transport_name(candidate->sdp_mid());
    }
  }
  if (!candidates.empty()) {
    _peerConnection->RemoveIceCandidates(candidates);
  }
}

- (void)addStream:(RTCMediaStream *)stream {
  if (!_peerConnection->AddStream(stream.nativeMediaStream)) {
    RTCLogError(@"Failed to add stream: %@", stream);
    return;
  }
  [_localStreams addObject:stream];
}

- (void)removeStream:(RTCMediaStream *)stream {
  _peerConnection->RemoveStream(stream.nativeMediaStream);
  [_localStreams removeObject:stream];
}

- (RTCRtpSender *)addTrack:(RTCMediaStreamTrack *)track streamIds:(NSArray<NSString *> *)streamIds {
  std::vector<std::string> nativeStreamIds;
  for (NSString *streamId in streamIds) {
    nativeStreamIds.push_back([streamId UTF8String]);
  }
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>> nativeSenderOrError =
      _peerConnection->AddTrack(track.nativeTrack, nativeStreamIds);
  if (!nativeSenderOrError.ok()) {
    RTCLogError(@"Failed to add track %@: %s", track, nativeSenderOrError.error().message());
    return nil;
  }
  return [[RTCRtpSender alloc] initWithFactory:self.factory
                               nativeRtpSender:nativeSenderOrError.MoveValue()];
}

- (BOOL)removeTrack:(RTCRtpSender *)sender {
  bool result = _peerConnection->RemoveTrack(sender.nativeRtpSender);
  if (!result) {
    RTCLogError(@"Failed to remote track %@", sender);
  }
  return result;
}

- (RTCRtpTransceiver *)addTransceiverWithTrack:(RTCMediaStreamTrack *)track {
  return [self addTransceiverWithTrack:track init:[[RTCRtpTransceiverInit alloc] init]];
}

- (RTCRtpTransceiver *)addTransceiverWithTrack:(RTCMediaStreamTrack *)track
                                          init:(RTCRtpTransceiverInit *)init {
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> nativeTransceiverOrError =
      _peerConnection->AddTransceiver(track.nativeTrack, init.nativeInit);
  if (!nativeTransceiverOrError.ok()) {
    RTCLogError(
        @"Failed to add transceiver %@: %s", track, nativeTransceiverOrError.error().message());
    return nil;
  }
  return [[RTCRtpTransceiver alloc] initWithFactory:self.factory
                               nativeRtpTransceiver:nativeTransceiverOrError.MoveValue()];
}

- (RTCRtpTransceiver *)addTransceiverOfType:(RTCRtpMediaType)mediaType {
  return [self addTransceiverOfType:mediaType init:[[RTCRtpTransceiverInit alloc] init]];
}

- (RTCRtpTransceiver *)addTransceiverOfType:(RTCRtpMediaType)mediaType
                                       init:(RTCRtpTransceiverInit *)init {
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> nativeTransceiverOrError =
      _peerConnection->AddTransceiver([RTCRtpReceiver nativeMediaTypeForMediaType:mediaType],
                                      init.nativeInit);
  if (!nativeTransceiverOrError.ok()) {
    RTCLogError(@"Failed to add transceiver %@: %s",
                [RTCRtpReceiver stringForMediaType:mediaType],
                nativeTransceiverOrError.error().message());
    return nil;
  }
  return [[RTCRtpTransceiver alloc] initWithFactory:self.factory
                               nativeRtpTransceiver:nativeTransceiverOrError.MoveValue()];
}

- (void)offerForConstraints:(RTCMediaConstraints *)constraints
          completionHandler:
    (void (^)(RTCSessionDescription *sessionDescription,
              NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  CopyConstraintsIntoOfferAnswerOptions(constraints.nativeConstraints.get(), &options);

  _peerConnection->CreateOffer(observer, options);
}

- (void)answerForConstraints:(RTCMediaConstraints *)constraints
           completionHandler:
    (void (^)(RTCSessionDescription *sessionDescription,
              NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  CopyConstraintsIntoOfferAnswerOptions(constraints.nativeConstraints.get(), &options);

  _peerConnection->CreateAnswer(observer, options);
}

- (void)setLocalDescription:(RTCSessionDescription *)sdp
          completionHandler:(void (^)(NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserverAdapter> observer(
      new rtc::RefCountedObject<webrtc::SetSessionDescriptionObserverAdapter>(
          completionHandler));
  _peerConnection->SetLocalDescription(observer, sdp.nativeDescription);
}

- (void)setRemoteDescription:(RTCSessionDescription *)sdp
           completionHandler:(void (^)(NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserverAdapter> observer(
      new rtc::RefCountedObject<webrtc::SetSessionDescriptionObserverAdapter>(
          completionHandler));
  _peerConnection->SetRemoteDescription(observer, sdp.nativeDescription);
}

- (BOOL)setBweMinBitrateBps:(nullable NSNumber *)minBitrateBps
          currentBitrateBps:(nullable NSNumber *)currentBitrateBps
              maxBitrateBps:(nullable NSNumber *)maxBitrateBps {
  webrtc::PeerConnectionInterface::BitrateParameters params;
  if (minBitrateBps != nil) {
    params.min_bitrate_bps = absl::optional<int>(minBitrateBps.intValue);
  }
  if (currentBitrateBps != nil) {
    params.current_bitrate_bps = absl::optional<int>(currentBitrateBps.intValue);
  }
  if (maxBitrateBps != nil) {
    params.max_bitrate_bps = absl::optional<int>(maxBitrateBps.intValue);
  }
  return _peerConnection->SetBitrate(params).ok();
}

- (BOOL)startRtcEventLogWithFilePath:(NSString *)filePath
                      maxSizeInBytes:(int64_t)maxSizeInBytes {
  RTC_DCHECK(filePath.length);
  RTC_DCHECK_GT(maxSizeInBytes, 0);
  RTC_DCHECK(!_hasStartedRtcEventLog);
  if (_hasStartedRtcEventLog) {
    RTCLogError(@"Event logging already started.");
    return NO;
  }
  FILE *f = fopen(filePath.UTF8String, "wb");
  if (!f) {
    RTCLogError(@"Error opening file: %@. Error: %d", filePath, errno);
    return NO;
  }
  // TODO(eladalon): It would be better to not allow negative values into PC.
  const size_t max_size = (maxSizeInBytes < 0) ? webrtc::RtcEventLog::kUnlimitedOutput :
                                                 rtc::saturated_cast<size_t>(maxSizeInBytes);

  _hasStartedRtcEventLog = _peerConnection->StartRtcEventLog(
      std::make_unique<webrtc::RtcEventLogOutputFile>(f, max_size));
  return _hasStartedRtcEventLog;
}

- (void)stopRtcEventLog {
  _peerConnection->StopRtcEventLog();
  _hasStartedRtcEventLog = NO;
}

- (RTCRtpSender *)senderWithKind:(NSString *)kind
                        streamId:(NSString *)streamId {
  std::string nativeKind = [NSString stdStringForString:kind];
  std::string nativeStreamId = [NSString stdStringForString:streamId];
  rtc::scoped_refptr<webrtc::RtpSenderInterface> nativeSender(
      _peerConnection->CreateSender(nativeKind, nativeStreamId));
  return nativeSender ?
      [[RTCRtpSender alloc] initWithFactory:self.factory nativeRtpSender:nativeSender] :
      nil;
}

- (NSArray<RTCRtpSender *> *)senders {
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> nativeSenders(
      _peerConnection->GetSenders());
  NSMutableArray *senders = [[NSMutableArray alloc] init];
  for (const auto &nativeSender : nativeSenders) {
    RTCRtpSender *sender =
        [[RTCRtpSender alloc] initWithFactory:self.factory nativeRtpSender:nativeSender];
    [senders addObject:sender];
  }
  return senders;
}

- (NSArray<RTCRtpReceiver *> *)receivers {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> nativeReceivers(
      _peerConnection->GetReceivers());
  NSMutableArray *receivers = [[NSMutableArray alloc] init];
  for (const auto &nativeReceiver : nativeReceivers) {
    RTCRtpReceiver *receiver =
        [[RTCRtpReceiver alloc] initWithFactory:self.factory nativeRtpReceiver:nativeReceiver];
    [receivers addObject:receiver];
  }
  return receivers;
}

- (NSArray<RTCRtpTransceiver *> *)transceivers {
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> nativeTransceivers(
      _peerConnection->GetTransceivers());
  NSMutableArray *transceivers = [[NSMutableArray alloc] init];
  for (const auto &nativeTransceiver : nativeTransceivers) {
    RTCRtpTransceiver *transceiver = [[RTCRtpTransceiver alloc] initWithFactory:self.factory
                                                           nativeRtpTransceiver:nativeTransceiver];
    [transceivers addObject:transceiver];
  }
  return transceivers;
}

#pragma mark - Private

+ (webrtc::PeerConnectionInterface::SignalingState)nativeSignalingStateForState:
    (RTCSignalingState)state {
  switch (state) {
    case RTCSignalingStateStable:
      return webrtc::PeerConnectionInterface::kStable;
    case RTCSignalingStateHaveLocalOffer:
      return webrtc::PeerConnectionInterface::kHaveLocalOffer;
    case RTCSignalingStateHaveLocalPrAnswer:
      return webrtc::PeerConnectionInterface::kHaveLocalPrAnswer;
    case RTCSignalingStateHaveRemoteOffer:
      return webrtc::PeerConnectionInterface::kHaveRemoteOffer;
    case RTCSignalingStateHaveRemotePrAnswer:
      return webrtc::PeerConnectionInterface::kHaveRemotePrAnswer;
    case RTCSignalingStateClosed:
      return webrtc::PeerConnectionInterface::kClosed;
  }
}

+ (RTCSignalingState)signalingStateForNativeState:
    (webrtc::PeerConnectionInterface::SignalingState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kStable:
      return RTCSignalingStateStable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
      return RTCSignalingStateHaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
      return RTCSignalingStateHaveLocalPrAnswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
      return RTCSignalingStateHaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
      return RTCSignalingStateHaveRemotePrAnswer;
    case webrtc::PeerConnectionInterface::kClosed:
      return RTCSignalingStateClosed;
  }
}

+ (NSString *)stringForSignalingState:(RTCSignalingState)state {
  switch (state) {
    case RTCSignalingStateStable:
      return @"STABLE";
    case RTCSignalingStateHaveLocalOffer:
      return @"HAVE_LOCAL_OFFER";
    case RTCSignalingStateHaveLocalPrAnswer:
      return @"HAVE_LOCAL_PRANSWER";
    case RTCSignalingStateHaveRemoteOffer:
      return @"HAVE_REMOTE_OFFER";
    case RTCSignalingStateHaveRemotePrAnswer:
      return @"HAVE_REMOTE_PRANSWER";
    case RTCSignalingStateClosed:
      return @"CLOSED";
  }
}

+ (webrtc::PeerConnectionInterface::PeerConnectionState)nativeConnectionStateForState:
        (RTCPeerConnectionState)state {
  switch (state) {
    case RTCPeerConnectionStateNew:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kNew;
    case RTCPeerConnectionStateConnecting:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kConnecting;
    case RTCPeerConnectionStateConnected:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kConnected;
    case RTCPeerConnectionStateFailed:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kFailed;
    case RTCPeerConnectionStateDisconnected:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected;
    case RTCPeerConnectionStateClosed:
      return webrtc::PeerConnectionInterface::PeerConnectionState::kClosed;
  }
}

+ (RTCPeerConnectionState)connectionStateForNativeState:
        (webrtc::PeerConnectionInterface::PeerConnectionState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::PeerConnectionState::kNew:
      return RTCPeerConnectionStateNew;
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnecting:
      return RTCPeerConnectionStateConnecting;
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnected:
      return RTCPeerConnectionStateConnected;
    case webrtc::PeerConnectionInterface::PeerConnectionState::kFailed:
      return RTCPeerConnectionStateFailed;
    case webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected:
      return RTCPeerConnectionStateDisconnected;
    case webrtc::PeerConnectionInterface::PeerConnectionState::kClosed:
      return RTCPeerConnectionStateClosed;
  }
}

+ (NSString *)stringForConnectionState:(RTCPeerConnectionState)state {
  switch (state) {
    case RTCPeerConnectionStateNew:
      return @"NEW";
    case RTCPeerConnectionStateConnecting:
      return @"CONNECTING";
    case RTCPeerConnectionStateConnected:
      return @"CONNECTED";
    case RTCPeerConnectionStateFailed:
      return @"FAILED";
    case RTCPeerConnectionStateDisconnected:
      return @"DISCONNECTED";
    case RTCPeerConnectionStateClosed:
      return @"CLOSED";
  }
}

+ (webrtc::PeerConnectionInterface::IceConnectionState)
    nativeIceConnectionStateForState:(RTCIceConnectionState)state {
  switch (state) {
    case RTCIceConnectionStateNew:
      return webrtc::PeerConnectionInterface::kIceConnectionNew;
    case RTCIceConnectionStateChecking:
      return webrtc::PeerConnectionInterface::kIceConnectionChecking;
    case RTCIceConnectionStateConnected:
      return webrtc::PeerConnectionInterface::kIceConnectionConnected;
    case RTCIceConnectionStateCompleted:
      return webrtc::PeerConnectionInterface::kIceConnectionCompleted;
    case RTCIceConnectionStateFailed:
      return webrtc::PeerConnectionInterface::kIceConnectionFailed;
    case RTCIceConnectionStateDisconnected:
      return webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
    case RTCIceConnectionStateClosed:
      return webrtc::PeerConnectionInterface::kIceConnectionClosed;
    case RTCIceConnectionStateCount:
      return webrtc::PeerConnectionInterface::kIceConnectionMax;
  }
}

+ (RTCIceConnectionState)iceConnectionStateForNativeState:
    (webrtc::PeerConnectionInterface::IceConnectionState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return RTCIceConnectionStateNew;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return RTCIceConnectionStateChecking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return RTCIceConnectionStateConnected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return RTCIceConnectionStateCompleted;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return RTCIceConnectionStateFailed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return RTCIceConnectionStateDisconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return RTCIceConnectionStateClosed;
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
      return RTCIceConnectionStateCount;
  }
}

+ (NSString *)stringForIceConnectionState:(RTCIceConnectionState)state {
  switch (state) {
    case RTCIceConnectionStateNew:
      return @"NEW";
    case RTCIceConnectionStateChecking:
      return @"CHECKING";
    case RTCIceConnectionStateConnected:
      return @"CONNECTED";
    case RTCIceConnectionStateCompleted:
      return @"COMPLETED";
    case RTCIceConnectionStateFailed:
      return @"FAILED";
    case RTCIceConnectionStateDisconnected:
      return @"DISCONNECTED";
    case RTCIceConnectionStateClosed:
      return @"CLOSED";
    case RTCIceConnectionStateCount:
      return @"COUNT";
  }
}

+ (webrtc::PeerConnectionInterface::IceGatheringState)
    nativeIceGatheringStateForState:(RTCIceGatheringState)state {
  switch (state) {
    case RTCIceGatheringStateNew:
      return webrtc::PeerConnectionInterface::kIceGatheringNew;
    case RTCIceGatheringStateGathering:
      return webrtc::PeerConnectionInterface::kIceGatheringGathering;
    case RTCIceGatheringStateComplete:
      return webrtc::PeerConnectionInterface::kIceGatheringComplete;
  }
}

+ (RTCIceGatheringState)iceGatheringStateForNativeState:
    (webrtc::PeerConnectionInterface::IceGatheringState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return RTCIceGatheringStateNew;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return RTCIceGatheringStateGathering;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return RTCIceGatheringStateComplete;
  }
}

+ (NSString *)stringForIceGatheringState:(RTCIceGatheringState)state {
  switch (state) {
    case RTCIceGatheringStateNew:
      return @"NEW";
    case RTCIceGatheringStateGathering:
      return @"GATHERING";
    case RTCIceGatheringStateComplete:
      return @"COMPLETE";
  }
}

+ (webrtc::PeerConnectionInterface::StatsOutputLevel)
    nativeStatsOutputLevelForLevel:(RTCStatsOutputLevel)level {
  switch (level) {
    case RTCStatsOutputLevelStandard:
      return webrtc::PeerConnectionInterface::kStatsOutputLevelStandard;
    case RTCStatsOutputLevelDebug:
      return webrtc::PeerConnectionInterface::kStatsOutputLevelDebug;
  }
}

- (rtc::scoped_refptr<webrtc::PeerConnectionInterface>)nativePeerConnection {
  return _peerConnection;
}

@end
