/*
 IFPanelAuthenticationHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <WebKit/IFPanelAuthenticationHandler.h>
#import <WebKit/IFAuthenticationPanel.h>
#import <WebKit/IFStandardPanels.h>
#import <WebFoundation/IFNSDictionaryExtensions.h>

static NSString *IFModalDialogPretendWindow = @"IFModalDialogPretendWindow";

@implementation IFPanelAuthenticationHandler

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

// IFAuthenticationHandler methods
-(BOOL)readyToStartAuthentication:(IFAuthenticationRequest *)request
{
    id window = [[IFStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = IFModalDialogPretendWindow;
    }

    return [windowToPanel objectForKey:window] == nil;
}

-(void)startAuthentication:(IFAuthenticationRequest *)request
{
    id window = [[IFStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = IFModalDialogPretendWindow;
    }

    if ([windowToPanel objectForKey:window] != nil) {
        [request authenticationDone:nil];
        return;
    }

    IFAuthenticationPanel *panel = [[IFAuthenticationPanel alloc] initWithCallback:self selector:@selector(_authenticationDoneWithRequest:result:)];
    [requestToWindow _IF_setObject:window forUncopiedKey:request];
    [windowToPanel _IF_setObject:panel forUncopiedKey:window];
    [panel release];
    
    if (window == IFModalDialogPretendWindow) {
        [panel runAsModalDialogWithRequest:request];
    } else {
        [panel runAsSheetOnWindow:window withRequest:request];
    }
}

-(void)cancelAuthentication:(IFAuthenticationRequest *)request
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        IFAuthenticationPanel *panel = [windowToPanel objectForKey:window];
        [panel cancel:self];
    }
}

-(void)_authenticationDoneWithRequest:(IFAuthenticationRequest *)request result:(IFAuthenticationResult *)result
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [requestToWindow removeObjectForKey:request];
    }

    [request authenticationDone:result];
}

@end
