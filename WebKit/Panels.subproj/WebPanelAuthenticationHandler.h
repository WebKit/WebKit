/*
 WebPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/NSURLCredentialStorage.h>

@class NSURLConnectionAuthenticationChallenge;

@interface WebPanelAuthenticationHandler : NSObject
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *challengeToWindow;
}

+ (id)sharedHandler;
- (void)startAuthentication:(NSURLConnectionAuthenticationChallenge *)challenge window:(NSWindow *)w;
- (void)cancelAuthentication:(NSURLConnectionAuthenticationChallenge *)challenge;

@end
