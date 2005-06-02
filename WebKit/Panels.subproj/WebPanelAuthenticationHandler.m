/*
 WebPanelAuthenticationHandler.m

 Copyright 2002 Apple, Inc. All rights reserved.
 */


#import <WebKit/WebPanelAuthenticationHandler.h>

#import <Foundation/NSURLAuthenticationChallenge.h>
#import <WebKit/WebAuthenticationPanel.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebNSDictionaryExtras.h>

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
	windowToChallengeQueue = [[NSMutableDictionary alloc] init];
    }
    return self;
}

-(void)dealloc
{
    [windowToPanel release];
    [challengeToWindow release];    
    [windowToChallengeQueue release];    
    [super dealloc];
}

-(void)enqueueChallenge:(NSURLAuthenticationChallenge *)challenge forWindow:(id)window
{
    NSMutableArray *queue = [windowToChallengeQueue objectForKey:window];
    if (queue == nil) {
	queue = [[NSMutableArray alloc] init];
	[windowToChallengeQueue _webkit_setObject:queue forUncopiedKey:window];
	[queue release];
    }
    [queue addObject:challenge];
}

-(void)tryNextChallengeForWindow:(id)window
{
    NSMutableArray *queue = [windowToChallengeQueue objectForKey:window];
    if (queue == nil) {
	return;
    }

    NSURLAuthenticationChallenge *challenge = [[queue objectAtIndex:0] retain];
    [queue removeObjectAtIndex:0];
    if ([queue count] == 0) {
	[windowToChallengeQueue removeObjectForKey:window];
    }

    NSURLCredential *latestCredential = [[NSURLCredentialStorage sharedCredentialStorage] defaultCredentialForProtectionSpace:[challenge protectionSpace]];

    if ([latestCredential hasPassword]) {
	[[challenge sender] useCredential:latestCredential forAuthenticationChallenge:challenge];
	[challenge release];
	return;
    }
								    
    [self startAuthentication:challenge window:(window == WebModalDialogPretendWindow ? nil : window)];
    [challenge release];
}


-(void)startAuthentication:(NSURLAuthenticationChallenge *)challenge window:(NSWindow *)w
{
    id window = w ? (id)w : (id)WebModalDialogPretendWindow;

    if ([windowToPanel objectForKey:window] != nil) {
	[self enqueueChallenge:challenge forWindow:window];
        return;
    }

    // In this case, we have an attached sheet that's not one of our
    // authentication panels, so enqueing is not an option. Just
    // cancel authentication instead, since this case is fairly
    // unlikely (how would you be loading a page if you had an error
    // sheet up?)
    if ([w attachedSheet] != nil) {
	[[challenge sender] cancelAuthenticationChallenge:challenge];
	return;
    }

    WebAuthenticationPanel *panel = [[WebAuthenticationPanel alloc] initWithCallback:self selector:@selector(_authenticationDoneWithChallenge:result:)];
    [challengeToWindow _webkit_setObject:window forUncopiedKey:challenge];
    [windowToPanel _webkit_setObject:panel forUncopiedKey:window];
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
    [window retain];
    if (window != nil) {
        [windowToPanel removeObjectForKey:window];
        [challengeToWindow removeObjectForKey:challenge];
    }

    if (credential == nil) {
	[[challenge sender] cancelAuthenticationChallenge:challenge];
    } else {
	[[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
    }

    [self tryNextChallengeForWindow:window];
    [window release];
}

@end
