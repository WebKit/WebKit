/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>
#import <QuartzCore/CoreAnimation.h>

#include "webrtc/base/gunit.h"
#include "webrtc/base/ssladapter.h"

#import "WebRTC/RTCMediaConstraints.h"
#import "WebRTC/RTCPeerConnectionFactory.h"
#import "WebRTC/RTCSessionDescription.h"

#import "ARDAppClient+Internal.h"
#import "ARDJoinResponse+Internal.h"
#import "ARDMessageResponse+Internal.h"
#import "ARDSDPUtils.h"

static NSString *kARDAppClientTestsDomain = @"org.webrtc.ARDAppClientTests";
static NSInteger kARDAppClientTestsExpectationTimeoutError = 100;

// These classes mimic XCTest APIs, to make eventual conversion to XCTest
// easier. Conversion will happen once XCTest is supported well on build bots.
@interface ARDTestExpectation : NSObject

@property(nonatomic, readonly) NSString *description;
@property(nonatomic, readonly) BOOL isFulfilled;

- (instancetype)initWithDescription:(NSString *)description;
- (void)fulfill;

@end

@implementation ARDTestExpectation

@synthesize description = _description;
@synthesize isFulfilled = _isFulfilled;

- (instancetype)initWithDescription:(NSString *)description {
  if (self = [super init]) {
    _description = description;
  }
  return self;
}

- (void)fulfill {
  _isFulfilled = YES;
}

@end

@interface ARDTestCase : NSObject

- (ARDTestExpectation *)expectationWithDescription:(NSString *)description;
- (void)waitForExpectationsWithTimeout:(NSTimeInterval)timeout
                               handler:(void (^)(NSError *error))handler;

@end

@implementation ARDTestCase {
  NSMutableArray *_expectations;
}

- (instancetype)init {
  if (self = [super init]) {
   _expectations = [NSMutableArray array];
  }
  return self;
}

- (ARDTestExpectation *)expectationWithDescription:(NSString *)description {
  ARDTestExpectation *expectation =
      [[ARDTestExpectation alloc] initWithDescription:description];
  [_expectations addObject:expectation];
  return expectation;
}

- (void)waitForExpectationsWithTimeout:(NSTimeInterval)timeout
                               handler:(void (^)(NSError *error))handler {
  CFTimeInterval startTime = CACurrentMediaTime();
  NSError *error = nil;
  while (![self areExpectationsFulfilled]) {
    CFTimeInterval duration = CACurrentMediaTime() - startTime;
    if (duration > timeout) {
      error = [NSError errorWithDomain:kARDAppClientTestsDomain
                                  code:kARDAppClientTestsExpectationTimeoutError
                              userInfo:@{}];
      break;
    }
    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
  }
  handler(error);
}

- (BOOL)areExpectationsFulfilled {
  for (ARDTestExpectation *expectation in _expectations) {
    if (!expectation.isFulfilled) {
      return NO;
    }
  }
  return YES;
}

@end

@interface ARDAppClientTest : ARDTestCase
@end

@implementation ARDAppClientTest

#pragma mark - Mock helpers

- (id)mockRoomServerClientForRoomId:(NSString *)roomId
                           clientId:(NSString *)clientId
                        isInitiator:(BOOL)isInitiator
                           messages:(NSArray *)messages
                     messageHandler:
    (void (^)(ARDSignalingMessage *))messageHandler {
  id mockRoomServerClient =
      [OCMockObject mockForProtocol:@protocol(ARDRoomServerClient)];

  // Successful join response.
  ARDJoinResponse *joinResponse = [[ARDJoinResponse alloc] init];
  joinResponse.result = kARDJoinResultTypeSuccess;
  joinResponse.roomId = roomId;
  joinResponse.clientId = clientId;
  joinResponse.isInitiator = isInitiator;
  joinResponse.messages = messages;

  // Successful message response.
  ARDMessageResponse *messageResponse = [[ARDMessageResponse alloc] init];
  messageResponse.result = kARDMessageResultTypeSuccess;

  // Return join response from above on join.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained void (^completionHandler)(ARDJoinResponse *response,
                                                  NSError *error);
    [invocation getArgument:&completionHandler atIndex:4];
    completionHandler(joinResponse, nil);
  }] joinRoomWithRoomId:roomId isLoopback:NO completionHandler:[OCMArg any]];

  // Return message response from above on join.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained ARDSignalingMessage *message;
    __unsafe_unretained void (^completionHandler)(ARDMessageResponse *response,
                                                  NSError *error);
    [invocation getArgument:&message atIndex:2];
    [invocation getArgument:&completionHandler atIndex:5];
    messageHandler(message);
    completionHandler(messageResponse, nil);
  }] sendMessage:[OCMArg any]
            forRoomId:roomId
             clientId:clientId
    completionHandler:[OCMArg any]];

  // Do nothing on leave.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained void (^completionHandler)(NSError *error);
    [invocation getArgument:&completionHandler atIndex:4];
    if (completionHandler) {
      completionHandler(nil);
    }
  }] leaveRoomWithRoomId:roomId
                clientId:clientId
       completionHandler:[OCMArg any]];

  return mockRoomServerClient;
}

- (id)mockSignalingChannelForRoomId:(NSString *)roomId
                           clientId:(NSString *)clientId
                     messageHandler:
    (void (^)(ARDSignalingMessage *message))messageHandler {
  id mockSignalingChannel =
      [OCMockObject niceMockForProtocol:@protocol(ARDSignalingChannel)];
  [[mockSignalingChannel stub] registerForRoomId:roomId clientId:clientId];
  [[[mockSignalingChannel stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained ARDSignalingMessage *message;
    [invocation getArgument:&message atIndex:2];
    messageHandler(message);
  }] sendMessage:[OCMArg any]];
  return mockSignalingChannel;
}

- (id)mockTURNClient {
  id mockTURNClient =
      [OCMockObject mockForProtocol:@protocol(ARDTURNClient)];
  [[[mockTURNClient stub] andDo:^(NSInvocation *invocation) {
    // Don't return anything in TURN response.
    __unsafe_unretained void (^completionHandler)(NSArray *turnServers,
                                                  NSError *error);
    [invocation getArgument:&completionHandler atIndex:2];
    completionHandler([NSArray array], nil);
  }] requestServersWithCompletionHandler:[OCMArg any]];
  return mockTURNClient;
}

- (ARDAppClient *)createAppClientForRoomId:(NSString *)roomId
                                  clientId:(NSString *)clientId
                               isInitiator:(BOOL)isInitiator
                                  messages:(NSArray *)messages
                            messageHandler:
    (void (^)(ARDSignalingMessage *message))messageHandler
                          connectedHandler:(void (^)(void))connectedHandler
                    localVideoTrackHandler:(void (^)(void))localVideoTrackHandler {
  id turnClient = [self mockTURNClient];
  id signalingChannel = [self mockSignalingChannelForRoomId:roomId
                                                   clientId:clientId
                                             messageHandler:messageHandler];
  id roomServerClient =
      [self mockRoomServerClientForRoomId:roomId
                                 clientId:clientId
                              isInitiator:isInitiator
                                 messages:messages
                           messageHandler:messageHandler];
  id delegate =
      [OCMockObject niceMockForProtocol:@protocol(ARDAppClientDelegate)];
  [[[delegate stub] andDo:^(NSInvocation *invocation) {
    connectedHandler();
  }] appClient:[OCMArg any]
      didChangeConnectionState:RTCIceConnectionStateConnected];
  [[[delegate stub] andDo:^(NSInvocation *invocation) {
    localVideoTrackHandler();
  }] appClient:[OCMArg any]
      didReceiveLocalVideoTrack:[OCMArg any]];

  return [[ARDAppClient alloc] initWithRoomServerClient:roomServerClient
                                       signalingChannel:signalingChannel
                                             turnClient:turnClient
                                               delegate:delegate];
}

// Tests that an ICE connection is established between two ARDAppClient objects
// where one is set up as a caller and the other the answerer. Network
// components are mocked out and messages are relayed directly from object to
// object. It's expected that both clients reach the
// RTCIceConnectionStateConnected state within a reasonable amount of time.
- (void)testSession {
  // Need block arguments here because we're setting up a callbacks before we
  // create the clients.
  ARDAppClient *caller = nil;
  ARDAppClient *answerer = nil;
  __block __weak ARDAppClient *weakCaller = nil;
  __block __weak ARDAppClient *weakAnswerer = nil;
  NSString *roomId = @"testRoom";
  NSString *callerId = @"testCallerId";
  NSString *answererId = @"testAnswererId";

  ARDTestExpectation *callerConnectionExpectation =
      [self expectationWithDescription:@"Caller PC connected."];
  ARDTestExpectation *answererConnectionExpectation =
      [self expectationWithDescription:@"Answerer PC connected."];

  caller = [self createAppClientForRoomId:roomId
                                 clientId:callerId
                              isInitiator:YES
                                 messages:[NSArray array]
                           messageHandler:^(ARDSignalingMessage *message) {
    ARDAppClient *strongAnswerer = weakAnswerer;
    [strongAnswerer channel:strongAnswerer.channel didReceiveMessage:message];
  }                      connectedHandler:^{
    [callerConnectionExpectation fulfill];
  }                localVideoTrackHandler:^{
  }];
  // TODO(tkchin): Figure out why DTLS-SRTP constraint causes thread assertion
  // crash in Debug.
  caller.defaultPeerConnectionConstraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:nil
                                            optionalConstraints:nil];
  weakCaller = caller;

  answerer = [self createAppClientForRoomId:roomId
                                   clientId:answererId
                                isInitiator:NO
                                   messages:[NSArray array]
                             messageHandler:^(ARDSignalingMessage *message) {
    ARDAppClient *strongCaller = weakCaller;
    [strongCaller channel:strongCaller.channel didReceiveMessage:message];
  }                        connectedHandler:^{
    [answererConnectionExpectation fulfill];
  }                  localVideoTrackHandler:^{
  }];
  // TODO(tkchin): Figure out why DTLS-SRTP constraint causes thread assertion
  // crash in Debug.
  answerer.defaultPeerConnectionConstraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:nil
                                            optionalConstraints:nil];
  weakAnswerer = answerer;

  // Kick off connection.
  [caller connectToRoomWithId:roomId
                   isLoopback:NO
                  isAudioOnly:NO
            shouldMakeAecDump:NO
        shouldUseLevelControl:NO];
  [answerer connectToRoomWithId:roomId
                     isLoopback:NO
                    isAudioOnly:NO
              shouldMakeAecDump:NO
          shouldUseLevelControl:NO];
  [self waitForExpectationsWithTimeout:20 handler:^(NSError *error) {
    if (error) {
      EXPECT_TRUE(0);
    }
  }];
}

// Test to see that we get a local video connection
// Note this will currently pass even when no camera is connected as a local
// video track is created regardless (Perhaps there should be a test for that...)
- (void)testSessionShouldGetLocalVideoTrackCallback {
  ARDAppClient *caller = nil;
  NSString *roomId = @"testRoom";
  NSString *callerId = @"testCallerId";

  ARDTestExpectation *localVideoTrackExpectation =
      [self expectationWithDescription:@"Caller got local video."];

  caller = [self createAppClientForRoomId:roomId
                                 clientId:callerId
                              isInitiator:YES
                                 messages:[NSArray array]
                           messageHandler:^(ARDSignalingMessage *message) {
  }                      connectedHandler:^{
  }                localVideoTrackHandler:^{
    [localVideoTrackExpectation fulfill];
  }];
  caller.defaultPeerConnectionConstraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:nil
                                          optionalConstraints:nil];

  // Kick off connection.
  [caller connectToRoomWithId:roomId
                   isLoopback:NO
                  isAudioOnly:NO
            shouldMakeAecDump:NO
        shouldUseLevelControl:NO];
  [self waitForExpectationsWithTimeout:20 handler:^(NSError *error) {
    if (error) {
      EXPECT_TRUE(0);
    }
  }];
}

@end

@interface ARDSDPUtilsTest : ARDTestCase
- (void)testPreferVideoCodec;
@end

@implementation ARDSDPUtilsTest

- (void)testPreferVideoCodec {
  NSString *sdp = @("m=video 9 RTP/SAVPF 100 116 117 96 120\n"
                    "a=rtpmap:120 H264/90000\n");
  NSString *expectedSdp = @("m=video 9 RTP/SAVPF 120 100 116 117 96\n"
                            "a=rtpmap:120 H264/90000\n");
  RTCSessionDescription* desc =
      [[RTCSessionDescription alloc] initWithType:RTCSdpTypeOffer sdp:sdp];
  RTCSessionDescription *h264Desc =
      [ARDSDPUtils descriptionForDescription:desc
                         preferredVideoCodec:@"H264"];
  EXPECT_TRUE([h264Desc.description rangeOfString:expectedSdp].location != NSNotFound);
}

@end

class SignalingTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    rtc::InitializeSSL();
  }
  static void TearDownTestCase() {
    rtc::CleanupSSL();
  }
};

TEST_F(SignalingTest, SessionTest) {
  @autoreleasepool {
    ARDAppClientTest *test = [[ARDAppClientTest alloc] init];
    [test testSession];
  }
}

#if !TARGET_IPHONE_SIMULATOR
// Expected fail on iOS Simulator due to no camera support
TEST_F(SignalingTest, SessionLocalVideoCallbackTest) {
  @autoreleasepool {
    ARDAppClientTest *test = [[ARDAppClientTest alloc] init];
    [test testSessionShouldGetLocalVideoTrackCallback];
  }
}
#endif

TEST_F(SignalingTest, SDPTest) {
  @autoreleasepool {
    ARDSDPUtilsTest *test = [[ARDSDPUtilsTest alloc] init];
    [test testPreferVideoCodec];
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
