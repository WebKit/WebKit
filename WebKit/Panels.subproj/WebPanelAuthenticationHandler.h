/*
 WebPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/WebAuthenticationManager.h>


@interface WebPanelAuthenticationHandler : NSObject <WebAuthenticationHandler>
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *requestToWindow;
}

// WebAuthenticationHandler methods
-(BOOL)readyToStartAuthentication:(WebAuthenticationRequest *)request;
-(void)startAuthentication:(WebAuthenticationRequest *)request;
-(void)cancelAuthentication:(WebAuthenticationRequest *)request;


@end
