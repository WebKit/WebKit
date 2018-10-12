/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpTransceiver+Private.h"

#import "RTCRtpEncodingParameters+Private.h"
#import "RTCRtpParameters+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"
#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

@implementation RTCRtpTransceiverInit

@synthesize direction = _direction;
@synthesize streamIds = _streamIds;
@synthesize sendEncodings = _sendEncodings;

- (instancetype)init {
  if (self = [super init]) {
    _direction = RTCRtpTransceiverDirectionSendRecv;
  }
  return self;
}

- (webrtc::RtpTransceiverInit)nativeInit {
  webrtc::RtpTransceiverInit init;
  init.direction = [RTCRtpTransceiver nativeRtpTransceiverDirectionFromDirection:_direction];
  for (NSString *streamId in _streamIds) {
    init.stream_ids.push_back([streamId UTF8String]);
  }
  for (RTCRtpEncodingParameters *sendEncoding in _sendEncodings) {
    init.send_encodings.push_back(sendEncoding.nativeParameters);
  }
  return init;
}

@end

@implementation RTCRtpTransceiver {
  RTCPeerConnectionFactory *_factory;
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> _nativeRtpTransceiver;
}

- (RTCRtpMediaType)mediaType {
  return [RTCRtpReceiver mediaTypeForNativeMediaType:_nativeRtpTransceiver->media_type()];
}

- (NSString *)mid {
  if (_nativeRtpTransceiver->mid()) {
    return [NSString stringForStdString:*_nativeRtpTransceiver->mid()];
  } else {
    return nil;
  }
}

@synthesize sender = _sender;
@synthesize receiver = _receiver;

- (BOOL)isStopped {
  return _nativeRtpTransceiver->stopped();
}

- (RTCRtpTransceiverDirection)direction {
  return [RTCRtpTransceiver
      rtpTransceiverDirectionFromNativeDirection:_nativeRtpTransceiver->direction()];
}

- (void)setDirection:(RTCRtpTransceiverDirection)direction {
  _nativeRtpTransceiver->SetDirection(
      [RTCRtpTransceiver nativeRtpTransceiverDirectionFromDirection:direction]);
}

- (BOOL)currentDirection:(RTCRtpTransceiverDirection *)currentDirectionOut {
  if (_nativeRtpTransceiver->current_direction()) {
    *currentDirectionOut = [RTCRtpTransceiver
        rtpTransceiverDirectionFromNativeDirection:*_nativeRtpTransceiver->current_direction()];
    return YES;
  } else {
    return NO;
  }
}

- (void)stop {
  _nativeRtpTransceiver->Stop();
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"RTCRtpTransceiver {\n  sender: %@\n  receiver: %@\n}", _sender, _receiver];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }
  RTCRtpTransceiver *transceiver = (RTCRtpTransceiver *)object;
  return _nativeRtpTransceiver == transceiver.nativeRtpTransceiver;
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeRtpTransceiver.get();
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::RtpTransceiverInterface>)nativeRtpTransceiver {
  return _nativeRtpTransceiver;
}

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
           nativeRtpTransceiver:
               (rtc::scoped_refptr<webrtc::RtpTransceiverInterface>)nativeRtpTransceiver {
  NSParameterAssert(factory);
  NSParameterAssert(nativeRtpTransceiver);
  if (self = [super init]) {
    _factory = factory;
    _nativeRtpTransceiver = nativeRtpTransceiver;
    _sender = [[RTCRtpSender alloc] initWithFactory:_factory
                                    nativeRtpSender:nativeRtpTransceiver->sender()];
    _receiver = [[RTCRtpReceiver alloc] initWithFactory:_factory
                                      nativeRtpReceiver:nativeRtpTransceiver->receiver()];
    RTCLogInfo(@"RTCRtpTransceiver(%p): created transceiver: %@", self, self.description);
  }
  return self;
}

+ (webrtc::RtpTransceiverDirection)nativeRtpTransceiverDirectionFromDirection:
        (RTCRtpTransceiverDirection)direction {
  switch (direction) {
    case RTCRtpTransceiverDirectionSendRecv:
      return webrtc::RtpTransceiverDirection::kSendRecv;
    case RTCRtpTransceiverDirectionSendOnly:
      return webrtc::RtpTransceiverDirection::kSendOnly;
    case RTCRtpTransceiverDirectionRecvOnly:
      return webrtc::RtpTransceiverDirection::kRecvOnly;
    case RTCRtpTransceiverDirectionInactive:
      return webrtc::RtpTransceiverDirection::kInactive;
  }
}

+ (RTCRtpTransceiverDirection)rtpTransceiverDirectionFromNativeDirection:
        (webrtc::RtpTransceiverDirection)nativeDirection {
  switch (nativeDirection) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return RTCRtpTransceiverDirectionSendRecv;
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return RTCRtpTransceiverDirectionSendOnly;
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return RTCRtpTransceiverDirectionRecvOnly;
    case webrtc::RtpTransceiverDirection::kInactive:
      return RTCRtpTransceiverDirectionInactive;
  }
}

@end
