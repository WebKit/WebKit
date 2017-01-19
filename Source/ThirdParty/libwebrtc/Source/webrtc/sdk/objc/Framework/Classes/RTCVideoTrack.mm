/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoTrack+Private.h"

#import "NSString+StdString.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCVideoRendererAdapter+Private.h"
#import "RTCVideoSource+Private.h"

@implementation RTCVideoTrack {
  NSMutableArray *_adapters;
}

@synthesize source = _source;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                         source:(RTCVideoSource *)source
                        trackId:(NSString *)trackId {
  NSParameterAssert(factory);
  NSParameterAssert(source);
  NSParameterAssert(trackId.length);
  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track =
      factory.nativeFactory->CreateVideoTrack(nativeId,
                                              source.nativeVideoSource);
  if ([self initWithNativeTrack:track type:RTCMediaStreamTrackTypeVideo]) {
    _source = source;
  }
  return self;
}

- (instancetype)initWithNativeTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeMediaTrack
                               type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(nativeMediaTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeVideo);
  if (self = [super initWithNativeTrack:nativeMediaTrack type:type]) {
    _adapters = [NSMutableArray array];
  }
  return self;
}

- (void)dealloc {
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    self.nativeVideoTrack->RemoveSink(adapter.nativeVideoRenderer);
  }
}

- (RTCVideoSource *)source {
  if (!_source) {
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source =
        self.nativeVideoTrack->GetSource();
    if (source) {
      _source = [[RTCVideoSource alloc] initWithNativeVideoSource:source.get()];
    }
  }
  return _source;
}

- (void)addRenderer:(id<RTCVideoRenderer>)renderer {
  // Make sure we don't have this renderer yet.
  for (RTCVideoRendererAdapter *adapter in _adapters) {
    if (adapter.videoRenderer == renderer) {
      NSAssert(NO, @"|renderer| is already attached to this track");
      return;
    }
  }
  // Create a wrapper that provides a native pointer for us.
  RTCVideoRendererAdapter* adapter =
      [[RTCVideoRendererAdapter alloc] initWithNativeRenderer:renderer];
  [_adapters addObject:adapter];
  self.nativeVideoTrack->AddOrUpdateSink(adapter.nativeVideoRenderer,
                                         rtc::VideoSinkWants());
}

- (void)removeRenderer:(id<RTCVideoRenderer>)renderer {
  __block NSUInteger indexToRemove = NSNotFound;
  [_adapters enumerateObjectsUsingBlock:^(RTCVideoRendererAdapter *adapter,
                                          NSUInteger idx,
                                          BOOL *stop) {
    if (adapter.videoRenderer == renderer) {
      indexToRemove = idx;
      *stop = YES;
    }
  }];
  if (indexToRemove == NSNotFound) {
    return;
  }
  RTCVideoRendererAdapter *adapterToRemove =
      [_adapters objectAtIndex:indexToRemove];
  self.nativeVideoTrack->RemoveSink(adapterToRemove.nativeVideoRenderer);
  [_adapters removeObjectAtIndex:indexToRemove];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::VideoTrackInterface>)nativeVideoTrack {
  return static_cast<webrtc::VideoTrackInterface *>(self.nativeTrack.get());
}

@end
