/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDWebSocketChannel.h"

#import <WebRTC/RTCLogging.h>
#import "SRWebSocket.h"

#import "ARDSignalingMessage.h"
#import "ARDUtilities.h"

// TODO(tkchin): move these to a configuration object.
static NSString const *kARDWSSMessageErrorKey = @"error";
static NSString const *kARDWSSMessagePayloadKey = @"msg";

@interface ARDWebSocketChannel () <SRWebSocketDelegate>
@end

@implementation ARDWebSocketChannel {
  NSURL *_url;
  NSURL *_restURL;
  SRWebSocket *_socket;
}

@synthesize delegate = _delegate;
@synthesize state = _state;
@synthesize roomId = _roomId;
@synthesize clientId = _clientId;

- (instancetype)initWithURL:(NSURL *)url
                    restURL:(NSURL *)restURL
                   delegate:(id<ARDSignalingChannelDelegate>)delegate {
  if (self = [super init]) {
    _url = url;
    _restURL = restURL;
    _delegate = delegate;
    _socket = [[SRWebSocket alloc] initWithURL:url];
    _socket.delegate = self;
    RTCLog(@"Opening WebSocket.");
    [_socket open];
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)setState:(ARDSignalingChannelState)state {
  if (_state == state) {
    return;
  }
  _state = state;
  [_delegate channel:self didChangeState:_state];
}

- (void)registerForRoomId:(NSString *)roomId
                 clientId:(NSString *)clientId {
  NSParameterAssert(roomId.length);
  NSParameterAssert(clientId.length);
  _roomId = roomId;
  _clientId = clientId;
  if (_state == kARDSignalingChannelStateOpen) {
    [self registerWithCollider];
  }
}

- (void)sendMessage:(ARDSignalingMessage *)message {
  NSParameterAssert(_clientId.length);
  NSParameterAssert(_roomId.length);
  NSData *data = [message JSONData];
  if (_state == kARDSignalingChannelStateRegistered) {
    NSString *payload =
        [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    NSDictionary *message = @{
      @"cmd": @"send",
      @"msg": payload,
    };
    NSData *messageJSONObject =
        [NSJSONSerialization dataWithJSONObject:message
                                        options:NSJSONWritingPrettyPrinted
                                          error:nil];
    NSString *messageString =
        [[NSString alloc] initWithData:messageJSONObject
                              encoding:NSUTF8StringEncoding];
    RTCLog(@"C->WSS: %@", messageString);
    [_socket send:messageString];
  } else {
    NSString *dataString =
        [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    RTCLog(@"C->WSS POST: %@", dataString);
    NSString *urlString =
        [NSString stringWithFormat:@"%@/%@/%@",
            [_restURL absoluteString], _roomId, _clientId];
    NSURL *url = [NSURL URLWithString:urlString];
    [NSURLConnection sendAsyncPostToURL:url
                               withData:data
                      completionHandler:nil];
  }
}

- (void)disconnect {
  if (_state == kARDSignalingChannelStateClosed ||
      _state == kARDSignalingChannelStateError) {
    return;
  }
  [_socket close];
  RTCLog(@"C->WSS DELETE rid:%@ cid:%@", _roomId, _clientId);
  NSString *urlString =
      [NSString stringWithFormat:@"%@/%@/%@",
          [_restURL absoluteString], _roomId, _clientId];
  NSURL *url = [NSURL URLWithString:urlString];
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = @"DELETE";
  request.HTTPBody = nil;
  [NSURLConnection sendAsyncRequest:request completionHandler:nil];
}

#pragma mark - SRWebSocketDelegate

- (void)webSocketDidOpen:(SRWebSocket *)webSocket {
  RTCLog(@"WebSocket connection opened.");
  self.state = kARDSignalingChannelStateOpen;
  if (_roomId.length && _clientId.length) {
    [self registerWithCollider];
  }
}

- (void)webSocket:(SRWebSocket *)webSocket didReceiveMessage:(id)message {
  NSString *messageString = message;
  NSData *messageData = [messageString dataUsingEncoding:NSUTF8StringEncoding];
  id jsonObject = [NSJSONSerialization JSONObjectWithData:messageData
                                                  options:0
                                                    error:nil];
  if (![jsonObject isKindOfClass:[NSDictionary class]]) {
    RTCLogError(@"Unexpected message: %@", jsonObject);
    return;
  }
  NSDictionary *wssMessage = jsonObject;
  NSString *errorString = wssMessage[kARDWSSMessageErrorKey];
  if (errorString.length) {
    RTCLogError(@"WSS error: %@", errorString);
    return;
  }
  NSString *payload = wssMessage[kARDWSSMessagePayloadKey];
  ARDSignalingMessage *signalingMessage =
      [ARDSignalingMessage messageFromJSONString:payload];
  RTCLog(@"WSS->C: %@", payload);
  [_delegate channel:self didReceiveMessage:signalingMessage];
}

- (void)webSocket:(SRWebSocket *)webSocket didFailWithError:(NSError *)error {
  RTCLogError(@"WebSocket error: %@", error);
  self.state = kARDSignalingChannelStateError;
}

- (void)webSocket:(SRWebSocket *)webSocket
    didCloseWithCode:(NSInteger)code
              reason:(NSString *)reason
            wasClean:(BOOL)wasClean {
  RTCLog(@"WebSocket closed with code: %ld reason:%@ wasClean:%d",
      (long)code, reason, wasClean);
  NSParameterAssert(_state != kARDSignalingChannelStateError);
  self.state = kARDSignalingChannelStateClosed;
}

#pragma mark - Private

- (void)registerWithCollider {
  if (_state == kARDSignalingChannelStateRegistered) {
    return;
  }
  NSParameterAssert(_roomId.length);
  NSParameterAssert(_clientId.length);
  NSDictionary *registerMessage = @{
    @"cmd": @"register",
    @"roomid" : _roomId,
    @"clientid" : _clientId,
  };
  NSData *message =
      [NSJSONSerialization dataWithJSONObject:registerMessage
                                      options:NSJSONWritingPrettyPrinted
                                        error:nil];
  NSString *messageString =
      [[NSString alloc] initWithData:message encoding:NSUTF8StringEncoding];
  RTCLog(@"Registering on WSS for rid:%@ cid:%@", _roomId, _clientId);
  // Registration can fail if server rejects it. For example, if the room is
  // full.
  [_socket send:messageString];
  self.state = kARDSignalingChannelStateRegistered;
}

@end

@interface ARDLoopbackWebSocketChannel () <ARDSignalingChannelDelegate>
@end

@implementation ARDLoopbackWebSocketChannel

- (instancetype)initWithURL:(NSURL *)url restURL:(NSURL *)restURL {
  return [super initWithURL:url restURL:restURL delegate:self];
}

#pragma mark - ARDSignalingChannelDelegate

- (void)channel:(id<ARDSignalingChannel>)channel
    didReceiveMessage:(ARDSignalingMessage *)message {
  switch (message.type) {
    case kARDSignalingMessageTypeOffer: {
      // Change message to answer, send back to server.
      ARDSessionDescriptionMessage *sdpMessage =
          (ARDSessionDescriptionMessage *)message;
      RTCSessionDescription *description = sdpMessage.sessionDescription;
      NSString *dsc = description.sdp;
      dsc = [dsc stringByReplacingOccurrencesOfString:@"offer"
                                           withString:@"answer"];
      RTCSessionDescription *answerDescription =
          [[RTCSessionDescription alloc] initWithType:RTCSdpTypeAnswer sdp:dsc];
      ARDSignalingMessage *answer =
          [[ARDSessionDescriptionMessage alloc]
               initWithDescription:answerDescription];
      [self sendMessage:answer];
      break;
    }
    case kARDSignalingMessageTypeAnswer:
      // Should not receive answer in loopback scenario.
      break;
    case kARDSignalingMessageTypeCandidate:
    case kARDSignalingMessageTypeCandidateRemoval:
      // Send back to server.
      [self sendMessage:message];
      break;
    case kARDSignalingMessageTypeBye:
      // Nothing to do.
      return;
  }
}

- (void)channel:(id<ARDSignalingChannel>)channel
    didChangeState:(ARDSignalingChannelState)state {
}

@end

