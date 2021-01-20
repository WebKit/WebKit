/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <XCTest/XCTest.h>

#include "api/task_queue/default_task_queue_factory.h"

#import "sdk/objc/components/audio/RTCAudioSession+Private.h"
#import "sdk/objc/native/api/audio_device_module.h"
#import "sdk/objc/native/src/audio/audio_device_ios.h"

@interface RTCAudioDeviceTests : XCTestCase {
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _audioDeviceModule;
  std::unique_ptr<webrtc::ios_adm::AudioDeviceIOS> _audio_device;
}

@property(nonatomic) RTCAudioSession *audioSession;

@end

@implementation RTCAudioDeviceTests

@synthesize audioSession = _audioSession;

- (void)setUp {
  [super setUp];

  _audioDeviceModule = webrtc::CreateAudioDeviceModule();
  _audio_device.reset(new webrtc::ios_adm::AudioDeviceIOS());
  self.audioSession = [RTCAudioSession sharedInstance];

  NSError *error = nil;
  [self.audioSession lockForConfiguration];
  [self.audioSession setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:0 error:&error];
  XCTAssertNil(error);

  [self.audioSession setMode:AVAudioSessionModeVoiceChat error:&error];
  XCTAssertNil(error);

  [self.audioSession setActive:YES error:&error];
  XCTAssertNil(error);

  [self.audioSession unlockForConfiguration];
}

- (void)tearDown {
  _audio_device->Terminate();
  _audio_device.reset(nullptr);
  _audioDeviceModule = nullptr;
  [self.audioSession notifyDidEndInterruptionWithShouldResumeSession:NO];

  [super tearDown];
}

// Verifies that the AudioDeviceIOS is_interrupted_ flag is reset correctly
// after an iOS AVAudioSessionInterruptionTypeEnded notification event.
// AudioDeviceIOS listens to RTCAudioSession interrupted notifications by:
// - In AudioDeviceIOS.InitPlayOrRecord registers its audio_session_observer_
//   callback with RTCAudioSession's delegate list.
// - When RTCAudioSession receives an iOS audio interrupted notification, it
//   passes the notification to callbacks in its delegate list which sets
//   AudioDeviceIOS's is_interrupted_ flag to true.
// - When AudioDeviceIOS.ShutdownPlayOrRecord is called, its
//   audio_session_observer_ callback is removed from RTCAudioSessions's
//   delegate list.
//   So if RTCAudioSession receives an iOS end audio interruption notification,
//   AudioDeviceIOS is not notified as its callback is not in RTCAudioSession's
//   delegate list. This causes AudioDeviceIOS's is_interrupted_ flag to be in
//   the wrong (true) state and the audio session will ignore audio changes.
// As RTCAudioSession keeps its own interrupted state, the fix is to initialize
// AudioDeviceIOS's is_interrupted_ flag to RTCAudioSession's isInterrupted
// flag in AudioDeviceIOS.InitPlayOrRecord.
- (void)testInterruptedAudioSession {
  XCTAssertTrue(self.audioSession.isActive);
  XCTAssertTrue([self.audioSession.category isEqual:AVAudioSessionCategoryPlayAndRecord] ||
                [self.audioSession.category isEqual:AVAudioSessionCategoryPlayback]);
  XCTAssertEqual(AVAudioSessionModeVoiceChat, self.audioSession.mode);

  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  std::unique_ptr<webrtc::AudioDeviceBuffer> audio_buffer;
  audio_buffer.reset(new webrtc::AudioDeviceBuffer(task_queue_factory.get()));
  _audio_device->AttachAudioBuffer(audio_buffer.get());
  XCTAssertEqual(webrtc::AudioDeviceGeneric::InitStatus::OK, _audio_device->Init());
  XCTAssertEqual(0, _audio_device->InitPlayout());
  XCTAssertEqual(0, _audio_device->StartPlayout());

  // Force interruption.
  [self.audioSession notifyDidBeginInterruption];

  // Wait for notification to propagate.
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();
  XCTAssertTrue(_audio_device->IsInterrupted());

  // Force it for testing.
  _audio_device->StopPlayout();

  [self.audioSession notifyDidEndInterruptionWithShouldResumeSession:YES];
  // Wait for notification to propagate.
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();
  XCTAssertTrue(_audio_device->IsInterrupted());

  _audio_device->Init();
  _audio_device->InitPlayout();
  XCTAssertFalse(_audio_device->IsInterrupted());
}

@end
