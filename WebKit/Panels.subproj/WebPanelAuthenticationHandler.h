/*
 IFPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/IFAuthenticationManager.h>


@interface IFPanelAuthenticationHandler : NSObject <IFAuthenticationHandler>
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *requestToWindow;
}

// IFAuthenticationHandler methods
-(BOOL)readyToStartAuthentication:(IFAuthenticationRequest *)request;
-(void)startAuthentication:(IFAuthenticationRequest *)request;
-(void)cancelAuthentication:(IFAuthenticationRequest *)request;


@end
