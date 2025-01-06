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

#import "RTCDataChannel+Private.h"
#import "RTCDataChannelConfiguration+Private.h"
#import "helpers/NSString+StdString.h"

@implementation RTCPeerConnection (DataChannel)

- (nullable RTCDataChannel *)dataChannelForLabel:(NSString *)label
                                   configuration:(RTCDataChannelConfiguration *)configuration {
  std::string labelString = [NSString stdStringForString:label];
  const webrtc::DataChannelInit nativeInit =
      configuration.nativeDataChannelInit;
  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel =
      self.nativePeerConnection->CreateDataChannel(labelString,
                                                   &nativeInit);
  if (!dataChannel) {
    return nil;
  }
  return [[RTCDataChannel alloc] initWithFactory:self.factory nativeDataChannel:dataChannel];
}

@end
