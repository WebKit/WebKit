/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDBroadcastSampleHandler.h"

#import <os/log.h>

#import "ARDExternalSampleCapturer.h"
#import "ARDSettingsModel.h"

#import "sdk/objc/api/logging/RTCCallbackLogger.h"
#import "sdk/objc/base/RTCLogging.h"

@implementation ARDBroadcastSampleHandler {
  ARDAppClient *_client;
  RTC_OBJC_TYPE(RTCCallbackLogger) * _callbackLogger;
}

@synthesize capturer = _capturer;

- (instancetype)init {
  if (self = [super init]) {
    _callbackLogger = [[RTC_OBJC_TYPE(RTCCallbackLogger) alloc] init];
    os_log_t rtc_os_log = os_log_create("com.google.AppRTCMobile", "RTCLog");
    [_callbackLogger start:^(NSString *logMessage) {
      os_log(rtc_os_log, "%{public}s", [logMessage cStringUsingEncoding:NSUTF8StringEncoding]);
    }];
  }
  return self;
}

- (void)broadcastStartedWithSetupInfo:(NSDictionary<NSString *, NSObject *> *)setupInfo {
  // User has requested to start the broadcast. Setup info from the UI extension can be supplied but
  // optional.
  ARDSettingsModel *settingsModel = [[ARDSettingsModel alloc] init];

  _client = [[ARDAppClient alloc] initWithDelegate:self];
  _client.broadcast = YES;

  NSString *roomName = nil;
  if (setupInfo[@"roomName"]) {
    roomName = (NSString *)setupInfo[@"roomName"];
  } else {
    u_int32_t randomRoomSuffix = arc4random_uniform(1000);
    roomName = [NSString stringWithFormat:@"broadcast_%d", randomRoomSuffix];
  }
  [_client connectToRoomWithId:roomName settings:settingsModel isLoopback:NO];
  RTCLog(@"Broadcast started.");
}

- (void)broadcastPaused {
  // User has requested to pause the broadcast. Samples will stop being delivered.
}

- (void)broadcastResumed {
  // User has requested to resume the broadcast. Samples delivery will resume.
}

- (void)broadcastFinished {
  // User has requested to finish the broadcast.
  [_client disconnect];
}

- (void)processSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   withType:(RPSampleBufferType)sampleBufferType {
  switch (sampleBufferType) {
    case RPSampleBufferTypeVideo:
      [self.capturer didCaptureSampleBuffer:sampleBuffer];
      break;
    case RPSampleBufferTypeAudioApp:
      break;
    case RPSampleBufferTypeAudioMic:
      break;
    default:
      break;
  }
}

#pragma mark - ARDAppClientDelegate

- (void)appClient:(ARDAppClient *)client didChangeState:(ARDAppClientState)state {
  switch (state) {
    case kARDAppClientStateConnected:
      RTCLog(@"Client connected.");
      break;
    case kARDAppClientStateConnecting:
      RTCLog("Client connecting.");
      break;
    case kARDAppClientStateDisconnected:
      RTCLog(@"Client disconnected.");
      break;
  }
}

- (void)appClient:(ARDAppClient *)client didChangeConnectionState:(RTCIceConnectionState)state {
  RTCLog(@"ICE state changed: %ld", (long)state);
}

- (void)appClient:(ARDAppClient *)client
    didCreateLocalCapturer:(RTC_OBJC_TYPE(RTCCameraVideoCapturer) *)localCapturer {
}

- (void)appClient:(ARDAppClient *)client
    didCreateLocalExternalSampleCapturer:(ARDExternalSampleCapturer *)externalSampleCapturer {
  self.capturer = externalSampleCapturer;
}

- (void)appClient:(ARDAppClient *)client
    didReceiveLocalVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)localVideoTrack {
}

- (void)appClient:(ARDAppClient *)client
    didReceiveRemoteVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)remoteVideoTrack {
}

- (void)appClient:(ARDAppClient *)client didGetStats:(RTC_OBJC_TYPE(RTCStatisticsReport) *)stats {
}

- (void)appClient:(ARDAppClient *)client didError:(NSError *)error {
  RTCLog(@"Error: %@", error);
}

@end
