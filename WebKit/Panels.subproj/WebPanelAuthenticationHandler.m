/*
 WebPanelAuthenticationHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <WebKit/WebPanelAuthenticationHandler.h>
#import <WebKit/WebAuthenticationPanel.h>
#import <WebKit/WebStandardPanels.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

static NSString *WebModalDialogPretendWindow = @"WebModalDialogPretendWindow";

@implementation WebPanelAuthenticationHandler

-(id)init
{
    self = [super init];
    if (self != nil) {
        windowToPanel = [[NSMutableDictionary alloc] init];
        requestToWindow = [[NSMutableDictionary alloc] init];
    }
    return self;
}

-(void)dealloc
{
    [windowToPanel release];
    [requestToWindow release];    
    [super dealloc];
}

// WebAuthenticationHandler methods
-(BOOL)readyToStartAuthentication:(WebAuthenticationRequest *)request
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    return [windowToPanel objectForKey:window] == nil;
}

-(void)startAuthentication:(WebAuthenticationRequest *)request
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    if ([windowToPanel objectForKey:window] != nil) {
        [request authenticationDone:nil];
        return;
    }

    WebAuthenticationPanel *panel = [[WebAuthenticationPanel alloc] initWithCallback:self selector:@selector(_authenticationDoneWithRequest:result:)];
    [requestToWindow _web_setObject:window forUncopiedKey:request];
    [windowToPanel _web_setObject:panel forUncopiedKey:window];
    [panel release];
    
    if (window == WebModalDialogPretendWindow) {
        [panel runAsModalDialogWithRequest:request];
    } else {
        [panel runAsSheetOnWindow:window withRequest:request];
    }
}

-(void)cancelAuthentication:(WebAuthenticationRequest *)request
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        WebAuthenticationPanel *panel = [windowToPanel objectForKey:window];
        [panel cancel:self];
    }
}

-(void)_authenticationDoneWithRequest:(WebAuthenticationRequest *)request result:(WebAuthenticationResult *)result
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [requestToWindow removeObjectForKey:request];
    }

    [request authenticationDone:result];
}

@end
