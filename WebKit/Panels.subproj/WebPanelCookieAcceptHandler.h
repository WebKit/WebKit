/*
 WebPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/WebCookieManager.h>

@class NSWindow;

@interface WebPanelCookieAcceptHandler : NSObject  <WebCookieAcceptHandler>
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *requestToWindow;
}

- (void)doneWithCheck:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;

// WebCookieAcceptHandler methods
-(BOOL)readyToStartCookieAcceptCheck:(WebCookieAcceptRequest *)request;
-(void)startCookieAcceptCheck:(WebCookieAcceptRequest *)request;
-(void)cancelCookieAcceptCheck:(WebCookieAcceptRequest *)request;

@end
