/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCAudioSource;
@class RTCAudioTrack;
@class RTCConfiguration;
@class RTCMediaConstraints;
@class RTCMediaStream;
@class RTCPeerConnection;
@class RTCVideoSource;
@class RTCVideoTrack;
@class RTCPeerConnectionFactoryOptions;
@protocol RTCPeerConnectionDelegate;
@protocol RTCVideoDecoderFactory;
@protocol RTCVideoEncoderFactory;

RTC_OBJC_EXPORT
@interface RTCPeerConnectionFactory : NSObject

/* Initialize object with default H264 video encoder/decoder factories */
- (instancetype)init;

/* Initialize object with injectable video encoder/decoder factories */
- (instancetype)initWithEncoderFactory:(nullable id<RTCVideoEncoderFactory>)encoderFactory
                        decoderFactory:(nullable id<RTCVideoDecoderFactory>)decoderFactory;

/** Initialize an RTCAudioSource with constraints. */
- (RTCAudioSource *)audioSourceWithConstraints:(nullable RTCMediaConstraints *)constraints;

/** Initialize an RTCAudioTrack with an id. Convenience ctor to use an audio source with no
 *  constraints.
 */
- (RTCAudioTrack *)audioTrackWithTrackId:(NSString *)trackId;

/** Initialize an RTCAudioTrack with a source and an id. */
- (RTCAudioTrack *)audioTrackWithSource:(RTCAudioSource *)source trackId:(NSString *)trackId;

/** Initialize a generic RTCVideoSource. The RTCVideoSource should be passed to a RTCVideoCapturer
 *  implementation, e.g. RTCCameraVideoCapturer, in order to produce frames.
 */
- (RTCVideoSource *)videoSource;

/** Initialize an RTCVideoTrack with a source and an id. */
- (RTCVideoTrack *)videoTrackWithSource:(RTCVideoSource *)source trackId:(NSString *)trackId;

/** Initialize an RTCMediaStream with an id. */
- (RTCMediaStream *)mediaStreamWithStreamId:(NSString *)streamId;

/** Initialize an RTCPeerConnection with a configuration, constraints, and
 *  delegate.
 */
- (RTCPeerConnection *)peerConnectionWithConfiguration:(RTCConfiguration *)configuration
                                           constraints:(RTCMediaConstraints *)constraints
                                              delegate:
                                                  (nullable id<RTCPeerConnectionDelegate>)delegate;

/** Set the options to be used for subsequently created RTCPeerConnections */
- (void)setOptions:(nonnull RTCPeerConnectionFactoryOptions *)options;

/** Start an AecDump recording. This API call will likely change in the future. */
- (BOOL)startAecDumpWithFilePath:(NSString *)filePath maxSizeInBytes:(int64_t)maxSizeInBytes;

/* Stop an active AecDump recording */
- (void)stopAecDump;

@end

NS_ASSUME_NONNULL_END
