/*
 IFPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/IFCookieManager.h>

@class NSWindow;

@interface IFPanelCookieAcceptHandler : NSObject  <IFCookieAcceptHandler>
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *requestToWindow;
}

- (void)doneWithCheck:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;

// IFCookieAcceptHandler methods
-(BOOL)readyToStartCookieAcceptCheck:(IFCookieAcceptRequest *)request;
-(void)startCookieAcceptCheck:(IFCookieAcceptRequest *)request;
-(void)cancelCookieAcceptCheck:(IFCookieAcceptRequest *)request;

@end
