/*
 WebPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <Foundation/NSURLCredentialStorage.h>

@class NSURLAuthenticationChallenge;

@interface WebPanelAuthenticationHandler : NSObject
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *challengeToWindow;
    NSMutableDictionary *windowToChallengeQueue;
}

+ (id)sharedHandler;
- (void)startAuthentication:(NSURLAuthenticationChallenge *)challenge window:(NSWindow *)w;
- (void)cancelAuthentication:(NSURLAuthenticationChallenge *)challenge;

@end
