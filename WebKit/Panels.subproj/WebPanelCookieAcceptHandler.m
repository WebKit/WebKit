/*
 WebPanelCookieAcceptHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <Cocoa/Cocoa.h>
#import <WebKit/WebPanelCookieAcceptHandler.h>
#import <WebKit/WebStandardPanels.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

static NSString *WebModalDialogPretendWindow = @"WebModalDialogPretendWindow";

@implementation WebPanelCookieAcceptHandler

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

// WebCookieAcceptHandler methods
-(BOOL)readyToStartCookieAcceptCheck:(WebCookieAcceptRequest *)request
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    return [windowToPanel objectForKey:window] == nil;
}

-(void)startCookieAcceptCheck:(WebCookieAcceptRequest *)request
{
    id window = [[WebStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = WebModalDialogPretendWindow;
    }

    if ([windowToPanel objectForKey:window] != nil) {
        [request cookieAcceptCheckDone:FALSE];
        return;
    }

    NSWindow *panel;

    if (window == WebModalDialogPretendWindow) {
	if ([[request cookies] count] == 1) {
	    panel = NSGetAlertPanel(@"Accept cookie?",
				    @"Server \"%@\" has sent 1 cookie.",
				    @"Accept", @"Reject", nil,
				    [[request url] host]);
	} else {
	    panel = NSGetAlertPanel(@"Accept cookies?",
				    @"Server \"%@\" has sent %d cookies.",
				    @"Accept", @"Reject", nil,
				    [[request url] host], [[request cookies] count]);
	}
	
	[requestToWindow _web_setObject:window forUncopiedKey:request];
	[windowToPanel _web_setObject:panel forUncopiedKey:window];
	[panel release];
	
        [self doneWithCheck:panel returnCode:[NSApp runModalForWindow:panel] contextInfo:request];
    } else {
	if ([[request cookies] count] == 1) {
	    NSBeginAlertSheet(@"Accept cookie?", @"Accept", @"Reject", nil, window, self, @selector(doneWithCheck:returnCode:contextInfo:), nil, request, @"Server \"%@\" has sent 1 cookie.", [[request url] host]);
	} else {
	    NSBeginAlertSheet(@"Accept cookies?", @"Accept", @"Reject", nil, window, self, @selector(doneWithCheck:returnCode:contextInfo:), nil, request, @"Server \"%@\" has sent %d cookies.", [[request url] host], [[request cookies] count]);
	}
	panel = [window attachedSheet];
	[requestToWindow _web_setObject:window forUncopiedKey:request];
	[windowToPanel _web_setObject:panel forUncopiedKey:window];
    }
}

-(void)cancelCookieAcceptCheck:(WebCookieAcceptRequest *)request
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        NSWindow *panel = [windowToPanel objectForKey:window];
        [panel close];
        if (window == WebModalDialogPretendWindow) {
            [NSApp stopModalWithCode:NSAlertAlternateReturn];
        } else {
            [NSApp endSheet:panel returnCode:NSAlertAlternateReturn];
        }
    }
}

- (void)doneWithCheck:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
    WebCookieAcceptRequest *request = (WebCookieAcceptRequest *)contextInfo;
    
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [requestToWindow removeObjectForKey:request];
    }

    [request cookieAcceptCheckDone:(returnCode == NSAlertDefaultReturn)];
}

@end
