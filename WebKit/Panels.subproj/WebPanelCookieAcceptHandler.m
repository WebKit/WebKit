/*
 IFPanelCookieAcceptHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <Cocoa/Cocoa.h>
#import <WebKit/IFPanelCookieAcceptHandler.h>
#import <WebKit/IFStandardPanels.h>
#import <WebFoundation/IFNSDictionaryExtensions.h>

static NSString *IFModalDialogPretendWindow = @"IFModalDialogPretendWindow";

@implementation IFPanelCookieAcceptHandler

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

// IFCookieAcceptHandler methods
-(BOOL)readyToStartCookieAcceptCheck:(IFCookieAcceptRequest *)request
{
    id window = [[IFStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = IFModalDialogPretendWindow;
    }

    return [windowToPanel objectForKey:window] == nil;
}

-(void)startCookieAcceptCheck:(IFCookieAcceptRequest *)request
{
    id window = [[IFStandardPanels sharedStandardPanels] frontmostWindowLoadingURL:[request url]];

    if (window == nil) {
        window = IFModalDialogPretendWindow;
    }

    if ([windowToPanel objectForKey:window] != nil) {
        [request cookieAcceptCheckDone:FALSE];
        return;
    }

    NSWindow *panel;

    if (window == IFModalDialogPretendWindow) {
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
	
	[requestToWindow _IF_setObject:window forUncopiedKey:request];
	[windowToPanel _IF_setObject:panel forUncopiedKey:window];
	[panel release];
	
        [self doneWithCheck:panel returnCode:[NSApp runModalForWindow:panel] contextInfo:request];
    } else {
	if ([[request cookies] count] == 1) {
	    NSBeginAlertSheet(@"Accept cookie?", @"Accept", @"Reject", nil, window, self, @selector(doneWithCheck:returnCode:contextInfo:), nil, request, @"Server \"%@\" has sent 1 cookie.", [[request url] host]);
	} else {
	    NSBeginAlertSheet(@"Accept cookies?", @"Accept", @"Reject", nil, window, self, @selector(doneWithCheck:returnCode:contextInfo:), nil, request, @"Server \"%@\" has sent %d cookies.", [[request url] host], [[request cookies] count]);
	}
	panel = [window attachedSheet];
	[requestToWindow _IF_setObject:window forUncopiedKey:request];
	[windowToPanel _IF_setObject:panel forUncopiedKey:window];
    }
}

-(void)cancelCookieAcceptCheck:(IFCookieAcceptRequest *)request
{
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        NSWindow *panel = [windowToPanel objectForKey:window];
        [panel close];
        if (window == IFModalDialogPretendWindow) {
            [NSApp stopModalWithCode:NSAlertAlternateReturn];
        } else {
            [NSApp endSheet:panel returnCode:NSAlertAlternateReturn];
        }
    }
}

- (void)doneWithCheck:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
    IFCookieAcceptRequest *request = (IFCookieAcceptRequest *)contextInfo;
    
    id window = [requestToWindow objectForKey:request];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [requestToWindow removeObjectForKey:request];
    }

    [request cookieAcceptCheckDone:(returnCode == NSAlertDefaultReturn)];
}

@end
