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
        challengeToWindow = [[NSMutableDictionary alloc] init];
    }
    return self;
}

-(void)dealloc
{
    [windowToPanel release];
    [challengeToWindow release];    
    [super dealloc];
}

// WebAuthenticationHandler methods
-(BOOL)isReadyToStartAuthentication:(NSURLAuthenticationChallenge *)challenge
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[[challenge protectionSpace] URL]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    return [windowToPanel objectForKey:window] == nil;
}

-(void)startAuthentication:(NSURLAuthenticationChallenge *)challenge
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[[challenge protectionSpace] URL]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    if ([windowToPanel objectForKey:window] != nil) {
        [challenge cancel];
        return;
    }

    WebAuthenticationPanel *panel = [[WebAuthenticationPanel alloc] initWithCallback:self selector:@selector(_authenticationDoneWithChallenge:result:)];
    [challengeToWindow _web_setObject:window forUncopiedKey:challenge];
    [windowToPanel _web_setObject:panel forUncopiedKey:window];
    [panel release];
    
    if (window == WebModalDialogPretendWindow) {
        [panel runAsModalDialogWithChallenge:challenge];
    } else {
        [panel runAsSheetOnWindow:window withChallenge:challenge];
    }
}

-(void)cancelAuthentication:(NSURLAuthenticationChallenge *)challenge
{
    id window = [challengeToWindow objectForKey:challenge];
    if (window != nil) {
        WebAuthenticationPanel *panel = [windowToPanel objectForKey:window];
        [panel cancel:self];
    }
}

-(void)_authenticationDoneWithChallenge:(NSURLAuthenticationChallenge *)challenge result:(NSURLCredential *)credential
{
    id window = [challengeToWindow objectForKey:challenge];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [challengeToWindow removeObjectForKey:challenge];
    }

    [challenge useCredential:credential];
}

@end
