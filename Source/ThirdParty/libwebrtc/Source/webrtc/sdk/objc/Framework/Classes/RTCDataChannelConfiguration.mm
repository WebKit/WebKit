/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDataChannelConfiguration+Private.h"

#import "NSString+StdString.h"

@implementation RTCDataChannelConfiguration

@synthesize nativeDataChannelInit = _nativeDataChannelInit;

- (BOOL)isOrdered {
  return _nativeDataChannelInit.ordered;
}

- (void)setIsOrdered:(BOOL)isOrdered {
  _nativeDataChannelInit.ordered = isOrdered;
}

- (NSInteger)maxRetransmitTimeMs {
  return self.maxPacketLifeTime;
}

- (void)setMaxRetransmitTimeMs:(NSInteger)maxRetransmitTimeMs {
  self.maxPacketLifeTime = maxRetransmitTimeMs;
}

- (int)maxPacketLifeTime {
  return _nativeDataChannelInit.maxRetransmitTime;
}

- (void)setMaxPacketLifeTime:(int)maxPacketLifeTime {
  _nativeDataChannelInit.maxRetransmitTime = maxPacketLifeTime;
}

- (int)maxRetransmits {
  return _nativeDataChannelInit.maxRetransmits;
}

- (void)setMaxRetransmits:(int)maxRetransmits {
  _nativeDataChannelInit.maxRetransmits = maxRetransmits;
}

- (NSString *)protocol {
  return [NSString stringForStdString:_nativeDataChannelInit.protocol];
}

- (void)setProtocol:(NSString *)protocol {
  _nativeDataChannelInit.protocol = [NSString stdStringForString:protocol];
}

- (BOOL)isNegotiated {
  return _nativeDataChannelInit.negotiated;
}

- (void)setIsNegotiated:(BOOL)isNegotiated {
  _nativeDataChannelInit.negotiated = isNegotiated;
}

- (int)streamId {
  return self.channelId;
}

- (void)setStreamId:(int)streamId {
  self.channelId = streamId;
}

- (int)channelId {
  return _nativeDataChannelInit.id;
}

- (void)setChannelId:(int)channelId {
  _nativeDataChannelInit.id = channelId;
}

@end
