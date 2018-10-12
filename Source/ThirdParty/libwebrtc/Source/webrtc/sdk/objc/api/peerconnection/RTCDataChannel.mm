/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDataChannel+Private.h"

#import "helpers/NSString+StdString.h"

#include <memory>

namespace webrtc {

class DataChannelDelegateAdapter : public DataChannelObserver {
 public:
  DataChannelDelegateAdapter(RTCDataChannel *channel) { channel_ = channel; }

  void OnStateChange() override {
    [channel_.delegate dataChannelDidChangeState:channel_];
  }

  void OnMessage(const DataBuffer& buffer) override {
    RTCDataBuffer *data_buffer =
        [[RTCDataBuffer alloc] initWithNativeBuffer:buffer];
    [channel_.delegate dataChannel:channel_
       didReceiveMessageWithBuffer:data_buffer];
  }

  void OnBufferedAmountChange(uint64_t previousAmount) override {
    id<RTCDataChannelDelegate> delegate = channel_.delegate;
    SEL sel = @selector(dataChannel:didChangeBufferedAmount:);
    if ([delegate respondsToSelector:sel]) {
      [delegate dataChannel:channel_ didChangeBufferedAmount:previousAmount];
    }
  }

 private:
  __weak RTCDataChannel *channel_;
};
}


@implementation RTCDataBuffer {
  std::unique_ptr<webrtc::DataBuffer> _dataBuffer;
}

- (instancetype)initWithData:(NSData *)data isBinary:(BOOL)isBinary {
  NSParameterAssert(data);
  if (self = [super init]) {
    rtc::CopyOnWriteBuffer buffer(
        reinterpret_cast<const uint8_t*>(data.bytes), data.length);
    _dataBuffer.reset(new webrtc::DataBuffer(buffer, isBinary));
  }
  return self;
}

- (NSData *)data {
  return [NSData dataWithBytes:_dataBuffer->data.data()
                        length:_dataBuffer->data.size()];
}

- (BOOL)isBinary {
  return _dataBuffer->binary;
}

#pragma mark - Private

- (instancetype)initWithNativeBuffer:(const webrtc::DataBuffer&)nativeBuffer {
  if (self = [super init]) {
    _dataBuffer.reset(new webrtc::DataBuffer(nativeBuffer));
  }
  return self;
}

- (const webrtc::DataBuffer *)nativeDataBuffer {
  return _dataBuffer.get();
}

@end


@implementation RTCDataChannel {
  RTCPeerConnectionFactory *_factory;
  rtc::scoped_refptr<webrtc::DataChannelInterface> _nativeDataChannel;
  std::unique_ptr<webrtc::DataChannelDelegateAdapter> _observer;
  BOOL _isObserverRegistered;
}

@synthesize delegate = _delegate;

- (void)dealloc {
  // Handles unregistering the observer properly. We need to do this because
  // there may still be other references to the underlying data channel.
  _nativeDataChannel->UnregisterObserver();
}

- (NSString *)label {
  return [NSString stringForStdString:_nativeDataChannel->label()];
}

- (BOOL)isReliable {
  return _nativeDataChannel->reliable();
}

- (BOOL)isOrdered {
  return _nativeDataChannel->ordered();
}

- (NSUInteger)maxRetransmitTime {
  return self.maxPacketLifeTime;
}

- (uint16_t)maxPacketLifeTime {
  return _nativeDataChannel->maxRetransmitTime();
}

- (uint16_t)maxRetransmits {
  return _nativeDataChannel->maxRetransmits();
}

- (NSString *)protocol {
  return [NSString stringForStdString:_nativeDataChannel->protocol()];
}

- (BOOL)isNegotiated {
  return _nativeDataChannel->negotiated();
}

- (NSInteger)streamId {
  return self.channelId;
}

- (int)channelId {
  return _nativeDataChannel->id();
}

- (RTCDataChannelState)readyState {
  return [[self class] dataChannelStateForNativeState:
      _nativeDataChannel->state()];
}

- (uint64_t)bufferedAmount {
  return _nativeDataChannel->buffered_amount();
}

- (void)close {
  _nativeDataChannel->Close();
}

- (BOOL)sendData:(RTCDataBuffer *)data {
  return _nativeDataChannel->Send(*data.nativeDataBuffer);
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCDataChannel:\n%ld\n%@\n%@",
                                    (long)self.channelId,
                                    self.label,
                                    [[self class]
                                        stringForState:self.readyState]];
}

#pragma mark - Private

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
              nativeDataChannel:
                  (rtc::scoped_refptr<webrtc::DataChannelInterface>)nativeDataChannel {
  NSParameterAssert(nativeDataChannel);
  if (self = [super init]) {
    _factory = factory;
    _nativeDataChannel = nativeDataChannel;
    _observer.reset(new webrtc::DataChannelDelegateAdapter(self));
    _nativeDataChannel->RegisterObserver(_observer.get());
  }
  return self;
}

+ (webrtc::DataChannelInterface::DataState)
    nativeDataChannelStateForState:(RTCDataChannelState)state {
  switch (state) {
    case RTCDataChannelStateConnecting:
      return webrtc::DataChannelInterface::DataState::kConnecting;
    case RTCDataChannelStateOpen:
      return webrtc::DataChannelInterface::DataState::kOpen;
    case RTCDataChannelStateClosing:
      return webrtc::DataChannelInterface::DataState::kClosing;
    case RTCDataChannelStateClosed:
      return webrtc::DataChannelInterface::DataState::kClosed;
  }
}

+ (RTCDataChannelState)dataChannelStateForNativeState:
    (webrtc::DataChannelInterface::DataState)nativeState {
  switch (nativeState) {
    case webrtc::DataChannelInterface::DataState::kConnecting:
      return RTCDataChannelStateConnecting;
    case webrtc::DataChannelInterface::DataState::kOpen:
      return RTCDataChannelStateOpen;
    case webrtc::DataChannelInterface::DataState::kClosing:
      return RTCDataChannelStateClosing;
    case webrtc::DataChannelInterface::DataState::kClosed:
      return RTCDataChannelStateClosed;
  }
}

+ (NSString *)stringForState:(RTCDataChannelState)state {
  switch (state) {
    case RTCDataChannelStateConnecting:
      return @"Connecting";
    case RTCDataChannelStateOpen:
      return @"Open";
    case RTCDataChannelStateClosing:
      return @"Closing";
    case RTCDataChannelStateClosed:
      return @"Closed";
  }
}

@end
