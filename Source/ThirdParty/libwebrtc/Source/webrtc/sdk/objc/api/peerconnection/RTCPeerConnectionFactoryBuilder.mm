/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactoryBuilder.h"
#import "RTCPeerConnectionFactory+Native.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

@implementation RTCPeerConnectionFactoryBuilder {
  std::unique_ptr<webrtc::VideoEncoderFactory> _videoEncoderFactory;
  std::unique_ptr<webrtc::VideoDecoderFactory> _videoDecoderFactory;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> _audioEncoderFactory;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> _audioDecoderFactory;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _audioDeviceModule;
  rtc::scoped_refptr<webrtc::AudioProcessing> _audioProcessingModule;
}

+ (RTCPeerConnectionFactoryBuilder *)builder {
  return [[RTCPeerConnectionFactoryBuilder alloc] init];
}

- (RTCPeerConnectionFactory *)createPeerConnectionFactory {
  RTCPeerConnectionFactory *factory = [RTCPeerConnectionFactory alloc];
  return [factory initWithNativeAudioEncoderFactory:_audioEncoderFactory
                          nativeAudioDecoderFactory:_audioDecoderFactory
                          nativeVideoEncoderFactory:std::move(_videoEncoderFactory)
                          nativeVideoDecoderFactory:std::move(_videoDecoderFactory)
                                  audioDeviceModule:_audioDeviceModule
                              audioProcessingModule:_audioProcessingModule];
}

- (void)setVideoEncoderFactory:(std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory {
  _videoEncoderFactory = std::move(videoEncoderFactory);
}

- (void)setVideoDecoderFactory:(std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory {
  _videoDecoderFactory = std::move(videoDecoderFactory);
}

- (void)setAudioEncoderFactory:
        (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory {
  _audioEncoderFactory = audioEncoderFactory;
}

- (void)setAudioDecoderFactory:
        (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory {
  _audioDecoderFactory = audioDecoderFactory;
}

- (void)setAudioDeviceModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule {
  _audioDeviceModule = audioDeviceModule;
}

- (void)setAudioProcessingModule:
        (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule {
  _audioProcessingModule = audioProcessingModule;
}

@end
