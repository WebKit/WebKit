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

#import "NSString+StdString.h"
#import "RTCDataChannel+Private.h"
#import "RTCDataChannelConfiguration+Private.h"

@implementation RTCPeerConnection (DataChannel)

- (RTCDataChannel *)dataChannelForLabel:(NSString *)label
                          configuration:
    (RTCDataChannelConfiguration *)configuration {
  std::string labelString = [NSString stdStringForString:label];
  const webrtc::DataChannelInit nativeInit =
      configuration.nativeDataChannelInit;
  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel =
      self.nativePeerConnection->CreateDataChannel(labelString,
                                                   &nativeInit);
  if (!dataChannel) {
    return nil;
  }
  return [[RTCDataChannel alloc] initWithNativeDataChannel:dataChannel];
}

@end
