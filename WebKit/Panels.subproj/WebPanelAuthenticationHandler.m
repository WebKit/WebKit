/*
 WebPanelAuthenticationHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <WebKit/WebPanelAuthenticationHandler.h>
#import <WebKit/WebAuthenticationPanel.h>
#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/NSURLConnectionAuthenticationChallenge.h>
#import <WebFoundation/NSURLDownloadAuthenticationChallenge.h>

static NSString *WebModalDialogPretendWindow = @"WebModalDialogPretendWindow";

@implementation WebPanelAuthenticationHandler

WebPanelAuthenticationHandler *sharedHandler;

+ (id)sharedHandler
{
    if (sharedHandler == nil) {
	sharedHandler = [[self alloc] init];
    }

    return sharedHandler;
}

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

-(void)startAuthentication:(NSURLAuthenticationChallenge *)challenge window:(NSWindow *)w
{
    ASSERT([challenge isKindOfClass:[NSURLConnectionAuthenticationChallenge class]] ||
	   [challenge isKindOfClass:[NSURLDownloadAuthenticationChallenge class]]);

    if ([w attachedSheet] != nil) {
	w = nil;
    }

    id window = w ? (id)w : (id)WebModalDialogPretendWindow;

    if ([windowToPanel objectForKey:window] != nil) {
	if ([challenge isKindOfClass:[NSURLConnectionAuthenticationChallenge class]]) {
	    NSURLConnectionAuthenticationChallenge *connectionChallenge = (NSURLConnectionAuthenticationChallenge *)challenge;
	    [[connectionChallenge connection] cancel];
	} else if ([challenge isKindOfClass:[NSURLDownloadAuthenticationChallenge class]]) {
	    NSURLDownloadAuthenticationChallenge *downloadChallenge = (NSURLDownloadAuthenticationChallenge *)challenge;
	    [[downloadChallenge download] cancel];
	} else {
	    ASSERT_NOT_REACHED();
	}
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

-(void)_authenticationDoneWithChallenge:(NSURLConnectionAuthenticationChallenge *)challenge result:(NSURLCredential *)credential
{
    id window = [challengeToWindow objectForKey:challenge];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [challengeToWindow removeObjectForKey:challenge];
    }

    if ([challenge isKindOfClass:[NSURLConnectionAuthenticationChallenge class]]) {
	NSURLConnectionAuthenticationChallenge *connectionChallenge = (NSURLConnectionAuthenticationChallenge *)challenge;

	if (credential == nil) {
	    [[connectionChallenge connection] cancel];
	} else {
	    [[connectionChallenge connection] useCredential:credential forAuthenticationChallenge:connectionChallenge];
	}
    } else if ([challenge isKindOfClass:[NSURLDownloadAuthenticationChallenge class]]) {
	NSURLDownloadAuthenticationChallenge *downloadChallenge = (NSURLDownloadAuthenticationChallenge *)challenge;

	if (credential == nil) {
	    [[downloadChallenge download] cancel];
	} else {
	    [[downloadChallenge download] useCredential:credential forAuthenticationChallenge:downloadChallenge];
	}
    } else {
	ASSERT_NOT_REACHED();
    }
}

@end
