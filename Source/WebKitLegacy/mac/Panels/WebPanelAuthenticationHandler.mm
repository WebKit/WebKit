/*
 * Copyright (C) 2005 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !PLATFORM(IOS_FAMILY)

#import "WebPanelAuthenticationHandler.h"

#import <Foundation/NSURLAuthenticationChallenge.h>
#import <WebKitLegacy/WebAuthenticationPanel.h>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

static NSString * const WebModalDialogPretendWindow = @"WebModalDialogPretendWindow";

@implementation WebPanelAuthenticationHandler {
    RetainPtr<NSMapTable> windowToPanel;
    RetainPtr<NSMapTable> challengeToWindow;
    RetainPtr<NSMapTable> windowToChallengeQueue;
}

+ (id)sharedHandler
{
    static NeverDestroyed<RetainPtr<WebPanelAuthenticationHandler>> sharedHandler = adoptNS([[self alloc] init]);
    return sharedHandler.get();
}

-(id)init
{
    self = [super init];
    if (self != nil) {
        windowToPanel = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0]);
        challengeToWindow = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0]);
        windowToChallengeQueue = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0]);
    }

    return self;
}

-(void)enqueueChallenge:(NSURLAuthenticationChallenge *)challenge forWindow:(id)window
{
    RetainPtr<NSMutableArray> queue = [windowToChallengeQueue objectForKey:window];
    if (!queue) {
        queue = adoptNS([[NSMutableArray alloc] init]);
        [windowToChallengeQueue setObject:queue forKey:window];
    }
    [queue addObject:challenge];
}

-(void)tryNextChallengeForWindow:(id)window
{
    RetainPtr<NSMutableArray> queue = [windowToChallengeQueue objectForKey:window];
    if (!queue)
        return;

    RetainPtr<NSURLAuthenticationChallenge> challenge = [queue objectAtIndex:0];
    [queue removeObjectAtIndex:0];
    if (![queue count])
        [windowToChallengeQueue removeObjectForKey:window];

    RetainPtr latestCredential = [protect([NSURLCredentialStorage sharedCredentialStorage]) defaultCredentialForProtectionSpace:protect([challenge protectionSpace])];

    if ([latestCredential hasPassword]) {
        [protect([challenge sender]) useCredential:latestCredential forAuthenticationChallenge:challenge];
        return;
    }

    [self startAuthentication:challenge window:(window == WebModalDialogPretendWindow ? nil : window)];
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
    // cancel loading instead, since this case is fairly
    // unlikely (how would you be loading a page if you had an error
    // sheet up?)
    if ([w attachedSheet] != nil) {
        [protect([challenge sender]) cancelAuthenticationChallenge:challenge];
        return;
    }

    RetainPtr panel = adoptNS([[WebAuthenticationPanel alloc] initWithCallback:self selector:@selector(_authenticationDoneWithChallenge:result:)]);
    [challengeToWindow setObject:window forKey:challenge];
    [windowToPanel setObject:panel forKey:window];

    if (window == WebModalDialogPretendWindow)
        [panel runAsModalDialogWithChallenge:challenge];
    else
        [panel runAsSheetOnWindow:window withChallenge:challenge];
}

-(void)cancelAuthentication:(NSURLAuthenticationChallenge *)challenge
{
    RetainPtr<id> window = [challengeToWindow objectForKey:challenge];
    if (!window)
        return;

    [protect([windowToPanel objectForKey:window]) cancel:self];
}

-(void)_authenticationDoneWithChallenge:(NSURLAuthenticationChallenge *)challenge result:(NSURLCredential *)credential
{
    RetainPtr<id> window = [challengeToWindow objectForKey:challenge];
    if (window) {
        [windowToPanel removeObjectForKey:window];
        [challengeToWindow removeObjectForKey:challenge];
    }

    if (credential == nil)
        [protect([challenge sender]) continueWithoutCredentialForAuthenticationChallenge:challenge];
    else
        [protect([challenge sender]) useCredential:credential forAuthenticationChallenge:challenge];

    [self tryNextChallengeForWindow:window];
}

@end

#endif // !PLATFORM(IOS_FAMILY)
