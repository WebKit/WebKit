/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnectionFactory.h"

#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class AudioDeviceModule;
class AudioEncoderFactory;
class AudioDecoderFactory;
class VideoEncoderFactory;
class VideoDecoderFactory;
class AudioProcessing;

}  // namespace webrtc

NS_ASSUME_NONNULL_BEGIN

@interface RTCPeerConnectionFactoryBuilder : NSObject

+ (RTCPeerConnectionFactoryBuilder *)builder;

- (RTCPeerConnectionFactory *)createPeerConnectionFactory;

- (void)setVideoEncoderFactory:(std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory;

- (void)setVideoDecoderFactory:(std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory;

- (void)setAudioEncoderFactory:(rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory;

- (void)setAudioDecoderFactory:(rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory;

- (void)setAudioDeviceModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule;

- (void)setAudioProcessingModule:(rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule;

@end

NS_ASSUME_NONNULL_END
