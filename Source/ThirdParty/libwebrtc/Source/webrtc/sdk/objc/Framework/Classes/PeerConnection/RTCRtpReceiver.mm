/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpReceiver+Private.h"

#import "NSString+StdString.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCRtpParameters+Private.h"
#import "WebRTC/RTCLogging.h"

#include "webrtc/api/mediastreaminterface.h"

namespace webrtc {

RtpReceiverDelegateAdapter::RtpReceiverDelegateAdapter(
    RTCRtpReceiver *receiver) {
  RTC_CHECK(receiver);
  receiver_ = receiver;
}

void RtpReceiverDelegateAdapter::OnFirstPacketReceived(
    cricket::MediaType media_type) {
  RTCRtpMediaType packet_media_type =
      [RTCRtpReceiver mediaTypeForNativeMediaType:media_type];
  RTCRtpReceiver *receiver = receiver_;
  [receiver.delegate rtpReceiver:receiver didReceiveFirstPacketForMediaType:packet_media_type];
}

}  // namespace webrtc

@implementation RTCRtpReceiver {
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> _nativeRtpReceiver;
  std::unique_ptr<webrtc::RtpReceiverDelegateAdapter> _observer;
}

@synthesize delegate = _delegate;

- (NSString *)receiverId {
  return [NSString stringForStdString:_nativeRtpReceiver->id()];
}

- (RTCRtpParameters *)parameters {
  return [[RTCRtpParameters alloc]
      initWithNativeParameters:_nativeRtpReceiver->GetParameters()];
}

- (void)setParameters:(RTCRtpParameters *)parameters {
  if (!_nativeRtpReceiver->SetParameters(parameters.nativeParameters)) {
    RTCLogError(@"RTCRtpReceiver(%p): Failed to set parameters: %@", self,
        parameters);
  }
}

- (RTCMediaStreamTrack *)track {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack(
    _nativeRtpReceiver->track());
  if (nativeTrack) {
    return [[RTCMediaStreamTrack alloc] initWithNativeTrack:nativeTrack];
  }
  return nil;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCRtpReceiver {\n  receiverId: %@\n}",
      self.receiverId];
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
  RTCRtpReceiver *receiver = (RTCRtpReceiver *)object;
  return _nativeRtpReceiver == receiver.nativeRtpReceiver;
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeRtpReceiver.get();
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::RtpReceiverInterface>)nativeRtpReceiver {
  return _nativeRtpReceiver;
}

- (instancetype)initWithNativeRtpReceiver:
    (rtc::scoped_refptr<webrtc::RtpReceiverInterface>)nativeRtpReceiver {
  if (self = [super init]) {
    _nativeRtpReceiver = nativeRtpReceiver;
    RTCLogInfo(
        @"RTCRtpReceiver(%p): created receiver: %@", self, self.description);
    _observer.reset(new webrtc::RtpReceiverDelegateAdapter(self));
    _nativeRtpReceiver->SetObserver(_observer.get());
  }
  return self;
}

+ (RTCRtpMediaType)mediaTypeForNativeMediaType:
    (cricket::MediaType)nativeMediaType {
  switch (nativeMediaType) {
    case cricket::MEDIA_TYPE_AUDIO:
      return RTCRtpMediaTypeAudio;
    case cricket::MEDIA_TYPE_VIDEO:
      return RTCRtpMediaTypeVideo;
    case cricket::MEDIA_TYPE_DATA:
      return RTCRtpMediaTypeData;
  }
}

@end
