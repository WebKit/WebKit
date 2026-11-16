/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDJoinResponse+Internal.h"

#import "ARDSignalingMessage.h"
#import "ARDUtilities.h"
#import "RTCIceServer+JSON.h"

static NSString const *kARDJoinResultKey = @"result";
static NSString const *kARDJoinResultParamsKey = @"params";
static NSString const *kARDJoinInitiatorKey = @"is_initiator";
static NSString const *kARDJoinRoomIdKey = @"room_id";
static NSString const *kARDJoinClientIdKey = @"client_id";
static NSString const *kARDJoinMessagesKey = @"messages";
static NSString const *kARDJoinWebSocketURLKey = @"wss_url";
static NSString const *kARDJoinWebSocketRestURLKey = @"wss_post_url";

@implementation ARDJoinResponse

@synthesize result = _result;
@synthesize isInitiator = _isInitiator;
@synthesize roomId = _roomId;
@synthesize clientId = _clientId;
@synthesize messages = _messages;
@synthesize webSocketURL = _webSocketURL;
@synthesize webSocketRestURL = _webSocketRestURL;

+ (ARDJoinResponse *)responseFromJSONData:(NSData *)data {
  NSDictionary *responseJSON = [NSDictionary dictionaryWithJSONData:data];
  if (!responseJSON) {
    return nil;
  }
  ARDJoinResponse *response = [[ARDJoinResponse alloc] init];
  NSString *resultString = responseJSON[kARDJoinResultKey];
  response.result = [[self class] resultTypeFromString:resultString];
  NSDictionary *params = responseJSON[kARDJoinResultParamsKey];

  response.isInitiator = [params[kARDJoinInitiatorKey] boolValue];
  response.roomId = params[kARDJoinRoomIdKey];
  response.clientId = params[kARDJoinClientIdKey];

  // Parse messages.
  NSArray *messages = params[kARDJoinMessagesKey];
  NSMutableArray *signalingMessages =
      [NSMutableArray arrayWithCapacity:messages.count];
  for (NSString *message in messages) {
    ARDSignalingMessage *signalingMessage =
        [ARDSignalingMessage messageFromJSONString:message];
    [signalingMessages addObject:signalingMessage];
  }
  response.messages = signalingMessages;

  // Parse websocket urls.
  NSString *webSocketURLString = params[kARDJoinWebSocketURLKey];
  response.webSocketURL = [NSURL URLWithString:webSocketURLString];
  NSString *webSocketRestURLString = params[kARDJoinWebSocketRestURLKey];
  response.webSocketRestURL = [NSURL URLWithString:webSocketRestURLString];

  return response;
}

#pragma mark - Private

+ (ARDJoinResultType)resultTypeFromString:(NSString *)resultString {
  ARDJoinResultType result = kARDJoinResultTypeUnknown;
  if ([resultString isEqualToString:@"SUCCESS"]) {
    result = kARDJoinResultTypeSuccess;
  } else if ([resultString isEqualToString:@"FULL"]) {
    result = kARDJoinResultTypeFull;
  }
  return result;
}

@end
