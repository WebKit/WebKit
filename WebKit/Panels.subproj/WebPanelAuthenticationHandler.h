/*
 WebPanelAuthenticationHandler.h

 Copyright 2002 Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <WebFoundation/NSURLCredentialStorage.h>


@interface WebPanelAuthenticationHandler : NSObject <WebAuthenticationHandler>
{
    NSMutableDictionary *windowToPanel;
    NSMutableDictionary *requestToWindow;
}

@end
