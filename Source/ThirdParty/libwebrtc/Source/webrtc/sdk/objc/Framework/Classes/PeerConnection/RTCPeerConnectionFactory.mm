/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory+Native.h"
#import "RTCPeerConnectionFactory+Private.h"

#import "NSString+StdString.h"
#import "RTCAVFoundationVideoSource+Private.h"
#import "RTCAudioSource+Private.h"
#import "RTCAudioTrack+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCPeerConnection+Private.h"
#import "RTCVideoSource+Private.h"
#import "RTCVideoTrack+Private.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#ifndef HAVE_NO_MEDIA
#include "VideoToolbox/objc_video_decoder_factory.h"
#include "VideoToolbox/objc_video_encoder_factory.h"
#import "WebRTC/RTCVideoCodecH264.h"
// The no-media version PeerConnectionFactory doesn't depend on these files, but the gn check tool
// is not smart enough to take the #ifdef into account.
#include "api/audio_codecs/builtin_audio_decoder_factory.h"     // nogncheck
#include "api/audio_codecs/builtin_audio_encoder_factory.h"     // nogncheck
#include "modules/audio_device/include/audio_device.h"          // nogncheck
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck
#endif

#include "Video/objcvideotracksource.h"
#include "api/videosourceproxy.h"
// Adding the nogncheck to disable the including header check.
// The no-media version PeerConnectionFactory doesn't depend on media related
// C++ target.
// TODO(zhihuang): Remove nogncheck once MediaEngineInterface is moved to C++
// API layer.
#include "media/engine/webrtcmediaengine.h"  // nogncheck

@implementation RTCPeerConnectionFactory {
  std::unique_ptr<rtc::Thread> _networkThread;
  std::unique_ptr<rtc::Thread> _workerThread;
  std::unique_ptr<rtc::Thread> _signalingThread;
  BOOL _hasStartedAecDump;
}

@synthesize nativeFactory = _nativeFactory;

- (instancetype)init {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  return [self initWithNativeAudioEncoderFactory:webrtc::CreateBuiltinAudioEncoderFactory()
                       nativeAudioDecoderFactory:webrtc::CreateBuiltinAudioDecoderFactory()
                 legacyNativeVideoEncoderFactory:new webrtc::ObjCVideoEncoderFactory(
                                                     [[RTCVideoEncoderFactoryH264 alloc] init])
                 legacyNativeVideoDecoderFactory:new webrtc::ObjCVideoDecoderFactory(
                                                     [[RTCVideoDecoderFactoryH264 alloc] init])
                               audioDeviceModule:nullptr];

#endif
}

- (instancetype)initWithEncoderFactory:(nullable id<RTCVideoEncoderFactory>)encoderFactory
                        decoderFactory:(nullable id<RTCVideoDecoderFactory>)decoderFactory {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  std::unique_ptr<webrtc::VideoEncoderFactory> native_encoder_factory;
  std::unique_ptr<webrtc::VideoDecoderFactory> native_decoder_factory;
  if (encoderFactory) {
    native_encoder_factory.reset(new webrtc::ObjCVideoEncoderFactory(encoderFactory));
  }
  if (decoderFactory) {
    native_decoder_factory.reset(new webrtc::ObjCVideoDecoderFactory(decoderFactory));
  }
  return [self initWithNativeAudioEncoderFactory:webrtc::CreateBuiltinAudioEncoderFactory()
                       nativeAudioDecoderFactory:webrtc::CreateBuiltinAudioDecoderFactory()
                       nativeVideoEncoderFactory:std::move(native_encoder_factory)
                       nativeVideoDecoderFactory:std::move(native_decoder_factory)
                               audioDeviceModule:nullptr
                           audioProcessingModule:nullptr];
#endif
}

- (instancetype)initNative {
  if (self = [super init]) {
    _networkThread = rtc::Thread::CreateWithSocketServer();
    BOOL result = _networkThread->Start();
    NSAssert(result, @"Failed to start network thread.");

    _workerThread = rtc::Thread::Create();
    result = _workerThread->Start();
    NSAssert(result, @"Failed to start worker thread.");

    _signalingThread = rtc::Thread::Create();
    result = _signalingThread->Start();
    NSAssert(result, @"Failed to start signaling thread.");
  }
  return self;
}

- (instancetype)initWithNoMedia {
  if (self = [self initNative]) {
    _nativeFactory = webrtc::CreateModularPeerConnectionFactory(
        _networkThread.get(),
        _workerThread.get(),
        _signalingThread.get(),
        std::unique_ptr<cricket::MediaEngineInterface>(),
        std::unique_ptr<webrtc::CallFactoryInterface>(),
        std::unique_ptr<webrtc::RtcEventLogFactoryInterface>());
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
}

- (instancetype)initWithNativeAudioEncoderFactory:
                    (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
                        nativeAudioDecoderFactory:
                            (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
                        nativeVideoEncoderFactory:
                            (std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory
                        nativeVideoDecoderFactory:
                            (std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory
                                audioDeviceModule:
                                    (nullable webrtc::AudioDeviceModule *)audioDeviceModule
                            audioProcessingModule:
                                (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  if (self = [self initNative]) {
    _nativeFactory = webrtc::CreatePeerConnectionFactory(_networkThread.get(),
                                                         _workerThread.get(),
                                                         _signalingThread.get(),
                                                         audioDeviceModule,
                                                         audioEncoderFactory,
                                                         audioDecoderFactory,
                                                         std::move(videoEncoderFactory),
                                                         std::move(videoDecoderFactory),
                                                         nullptr,  // audio mixer
                                                         audioProcessingModule);
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
#endif
}

- (instancetype)
    initWithNativeAudioEncoderFactory:
        (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
            nativeAudioDecoderFactory:
                (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
      legacyNativeVideoEncoderFactory:(cricket::WebRtcVideoEncoderFactory *)videoEncoderFactory
      legacyNativeVideoDecoderFactory:(cricket::WebRtcVideoDecoderFactory *)videoDecoderFactory
                    audioDeviceModule:(nullable webrtc::AudioDeviceModule *)audioDeviceModule {
#ifdef HAVE_NO_MEDIA
  return [self initWithNoMedia];
#else
  if (self = [self initNative]) {
    _nativeFactory = webrtc::CreatePeerConnectionFactory(_networkThread.get(),
                                                         _workerThread.get(),
                                                         _signalingThread.get(),
                                                         audioDeviceModule,
                                                         audioEncoderFactory,
                                                         audioDecoderFactory,
                                                         videoEncoderFactory,
                                                         videoDecoderFactory);
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
#endif
}

- (RTCAudioSource *)audioSourceWithConstraints:(nullable RTCMediaConstraints *)constraints {
  std::unique_ptr<webrtc::MediaConstraints> nativeConstraints;
  if (constraints) {
    nativeConstraints = constraints.nativeConstraints;
  }
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      _nativeFactory->CreateAudioSource(nativeConstraints.get());
  return [[RTCAudioSource alloc] initWithNativeAudioSource:source];
}

- (RTCAudioTrack *)audioTrackWithTrackId:(NSString *)trackId {
  RTCAudioSource *audioSource = [self audioSourceWithConstraints:nil];
  return [self audioTrackWithSource:audioSource trackId:trackId];
}

- (RTCAudioTrack *)audioTrackWithSource:(RTCAudioSource *)source
                                trackId:(NSString *)trackId {
  return [[RTCAudioTrack alloc] initWithFactory:self
                                         source:source
                                        trackId:trackId];
}

- (RTCAVFoundationVideoSource *)avFoundationVideoSourceWithConstraints:
    (nullable RTCMediaConstraints *)constraints {
#ifdef HAVE_NO_MEDIA
  return nil;
#else
  return [[RTCAVFoundationVideoSource alloc] initWithFactory:self constraints:constraints];
#endif
}

- (RTCVideoSource *)videoSource {
  rtc::scoped_refptr<webrtc::ObjcVideoTrackSource> objcVideoTrackSource(
      new rtc::RefCountedObject<webrtc::ObjcVideoTrackSource>());
  return [[RTCVideoSource alloc]
      initWithNativeVideoSource:webrtc::VideoTrackSourceProxy::Create(_signalingThread.get(),
                                                                      _workerThread.get(),
                                                                      objcVideoTrackSource)];
}

- (RTCVideoTrack *)videoTrackWithSource:(RTCVideoSource *)source
                                trackId:(NSString *)trackId {
  return [[RTCVideoTrack alloc] initWithFactory:self
                                         source:source
                                        trackId:trackId];
}

- (RTCMediaStream *)mediaStreamWithStreamId:(NSString *)streamId {
  return [[RTCMediaStream alloc] initWithFactory:self
                                        streamId:streamId];
}

- (RTCPeerConnection *)peerConnectionWithConfiguration:
    (RTCConfiguration *)configuration
                                           constraints:
    (RTCMediaConstraints *)constraints
                                              delegate:
    (nullable id<RTCPeerConnectionDelegate>)delegate {
  return [[RTCPeerConnection alloc] initWithFactory:self
                                      configuration:configuration
                                        constraints:constraints
                                           delegate:delegate];
}

- (BOOL)startAecDumpWithFilePath:(NSString *)filePath
                  maxSizeInBytes:(int64_t)maxSizeInBytes {
  RTC_DCHECK(filePath.length);
  RTC_DCHECK_GT(maxSizeInBytes, 0);

  if (_hasStartedAecDump) {
    RTCLogError(@"Aec dump already started.");
    return NO;
  }
  int fd = open(filePath.UTF8String, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    RTCLogError(@"Error opening file: %@. Error: %d", filePath, errno);
    return NO;
  }
  _hasStartedAecDump = _nativeFactory->StartAecDump(fd, maxSizeInBytes);
  return _hasStartedAecDump;
}

- (void)stopAecDump {
  _nativeFactory->StopAecDump();
  _hasStartedAecDump = NO;
}

@end
