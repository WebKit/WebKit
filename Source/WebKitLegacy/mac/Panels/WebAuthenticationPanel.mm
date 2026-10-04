/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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

#import "WebAuthenticationPanel.h"

#import "WebLocalizableStringsInternal.h"
#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLCredential.h>
#import <Foundation/NSURLProtectionSpace.h>
#import <WebKitLegacy/WebKitNSStringExtras.h>
#import <WebKitLegacy/WebNSControlExtras.h>
#import <WebKitLegacy/WebNSURLExtras.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

#define WebAuthenticationPanelNibName @"WebAuthenticationPanel"

@implementation WebAuthenticationPanel {
    RetainPtr<id> callback;
    RetainPtr<NSURLAuthenticationChallenge> challenge;
}

-(id)initWithCallback:(id)cb selector:(SEL)sel
{
    self = [self init];
    if (self != nil) {
        callback = cb;
        selector = sel;
    }
    return self;
}


- (void)dealloc
{
    // Retaining the member just to release it would be pointless.
    SUPPRESS_UNRETAINED_ARG [panel release];

    [super dealloc];
}

// IB actions

- (IBAction)cancel:(id)sender
{
    // This is required because the body of this method is going to
    // remove all of the panel's remaining refs, which can cause a
    // crash later when finishing button hit tracking. So we make
    // sure it lives on a bit longer.
    RetainPtr { panel }.autorelease();

    // This is required as a workaround for AppKit issue 4118422
    [[self retain] autorelease];

    RetainPtr protectedPanel = panel;
    if (usingSheet)
        [protect([protectedPanel sheetParent]) endSheet:protectedPanel returnCode:NSModalResponseCancel];
    else {
        [protectedPanel orderOut:sender];
        [[NSApplication sharedApplication] stopModalWithCode:1];
    }
}

- (IBAction)logIn:(id)sender
{
    // This is required because the body of this method is going to
    // remove all of the panel's remaining refs, which can cause a
    // crash later when finishing button hit tracking. So we make
    // sure it lives on a bit longer.
    RetainPtr { panel }.autorelease();

    RetainPtr protectedPanel = panel;
    if (usingSheet)
        [protect([protectedPanel sheetParent]) endSheet:protectedPanel returnCode:NSModalResponseOK];
    else {
        [protectedPanel orderOut:sender];
        [[NSApplication sharedApplication] stopModalWithCode:0];
    }
}

- (BOOL)loadNib
{
    if (!nibLoaded) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([NSBundle loadNibNamed:WebAuthenticationPanelNibName owner:self]) {
ALLOW_DEPRECATED_DECLARATIONS_END
            nibLoaded = YES;
            [protect(imageView) setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
        } else {
            LOG_ERROR("couldn't load nib named '%@'", WebAuthenticationPanelNibName);
            return FALSE;
        }
    }
    return TRUE;
}

// Methods related to displaying the panel

-(void)setUpForChallenge:(NSURLAuthenticationChallenge *)chall
{
    [self loadNib];

    NSURLProtectionSpace *space = [chall protectionSpace];

    NSString *host;
    if (![space port])
        host = [[space host] _webkit_decodeHostName];
    else
        host = [NSString stringWithFormat:@"%@:%ld", [[space host] _webkit_decodeHostName], (long)[space port]];

    NSString *realm = [space realm];
    if (!realm)
        realm = @"";
    NSString *message;

    // Consider the realm name to be "simple" if it does not contain any whitespace or newline characters.
    // If the realm name is determined to be complex, we will use a slightly different sheet layout, designed
    // to keep a malicious realm name from spoofing the wording in the sheet text.
    BOOL realmNameIsSimple = [realm rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]].location == NSNotFound;

    if (![chall previousFailureCount]) {
        if ([space isProxy]) {
            message = [NSString stringWithFormat:UI_STRING_INTERNAL("To view this page, you must log in to the %@ proxy server %@.",
                                                           "prompt string in authentication panel"),
                [space proxyType], host];
        } else {
            if (realmNameIsSimple) {
                message = [NSString stringWithFormat:UI_STRING_INTERNAL("To view this page, you must log in to area “%@” on %@.",
                                                               "prompt string in authentication panel"), realm, host];
            } else {
                message = [NSString stringWithFormat:UI_STRING_INTERNAL("To view this page, you must log in to this area on %@:",
                                                               "prompt string in authentication panel"), host];
            }
        }
    } else {
        if ([space isProxy]) {
            message = [NSString stringWithFormat:UI_STRING_INTERNAL("The user name or password you entered for the %@ proxy server %@ was incorrect. Make sure you’re entering them correctly, and then try again.",
                                                           "prompt string in authentication panel"),
                [space proxyType], host];
        } else {
            if (realmNameIsSimple) {
                message = [NSString stringWithFormat:UI_STRING_INTERNAL("The user name or password you entered for area “%@” on %@ was incorrect. Make sure you’re entering them correctly, and then try again.",
                                                               "prompt string in authentication panel"), realm, host];
            } else {
                message = [NSString stringWithFormat:UI_STRING_INTERNAL("The user name or password you entered for this area on %@ was incorrect. Make sure you’re entering them correctly, and then try again.",
                                                               "prompt string in authentication panel"), host];
            }
        }
    }

    RetainPtr protectedSeparateRealmLabel = separateRealmLabel;
    RetainPtr protectedMainLabel = mainLabel;
    RetainPtr protectedSmallLabel = smallLabel;
    RetainPtr protectedPanel = panel;
    if (![space isProxy] && !realmNameIsSimple) {
        [protectedSeparateRealmLabel setHidden:NO];
        [protectedSeparateRealmLabel setStringValue:realm];
        [protectedSeparateRealmLabel setAutoresizingMask:NSViewMinYMargin];
        [protectedSeparateRealmLabel sizeToFitAndAdjustWindowHeight];
        [protectedSeparateRealmLabel setAutoresizingMask:NSViewMaxYMargin];
    } else {
        // In the proxy or "simple" realm name case, we need to hide the 'separateRealmLabel'
        // and move the rest of the contents up appropriately to fill the space.
        NSRect mainLabelFrame = [protectedMainLabel frame];
        NSRect realmFrame = [protectedSeparateRealmLabel frame];
        NSRect smallLabelFrame = [protectedSmallLabel frame];

        // Find the distance between the 'smallLabel' and the label above it, initially the 'separateRealmLabel'.
        // Then, find the current distance between 'smallLabel' and 'mainLabel'. The difference between
        // these two is how much shorter the panel needs to be after hiding the 'separateRealmLabel'.
        CGFloat smallLabelMargin = NSMinY(realmFrame) - NSMaxY(smallLabelFrame);
        CGFloat smallLabelToMainLabel = NSMinY(mainLabelFrame) - NSMaxY(smallLabelFrame);
        CGFloat deltaMargin = smallLabelToMainLabel - smallLabelMargin;

        [protectedSeparateRealmLabel setHidden:YES];
        NSRect windowFrame = [protectedPanel frame];
        windowFrame.size.height -= deltaMargin;
        [protectedPanel setFrame:windowFrame display:NO];
    }

    [protectedMainLabel setStringValue:message];
    [protectedMainLabel sizeToFitAndAdjustWindowHeight];

    if ([space receivesCredentialSecurely] || [[space protocol] _webkit_isCaseInsensitiveEqualToString:@"https"]) {
        [protectedSmallLabel setStringValue:
            protect(UI_STRING_INTERNAL("Your login information will be sent securely.",
                "message in authentication panel"))];
    } else {
        // Use this scary-sounding phrase only when using basic auth with non-https servers. In this case the password
        // could be sniffed by intercepting the network traffic.
        [protectedSmallLabel setStringValue:
            protect(UI_STRING_INTERNAL("Your password will be sent unencrypted.",
                "message in authentication panel"))];
    }

    RetainPtr protectedUsername = username;
    RetainPtr protectedPassword = password;
    if ([[chall proposedCredential] user] != nil) {
        [protectedUsername setStringValue:[[chall proposedCredential] user]];
        [protectedPanel setInitialFirstResponder:protectedPassword];
    } else {
        [protectedUsername setStringValue:@""];
        [protectedPassword setStringValue:@""];
        [protectedPanel setInitialFirstResponder:protectedUsername];
    }
}

- (void)runAsModalDialogWithChallenge:(NSURLAuthenticationChallenge *)chall
{
    [self setUpForChallenge:chall];

    usingSheet = FALSE;
    RetainPtr protectedChallenge = chall;
    RetainPtr<NSURLCredential> credential;

    if (![[NSApplication sharedApplication] runModalForWindow:protect(panel)])
        credential = adoptNS([[NSURLCredential alloc] initWithUser:protect([protect(username) stringValue]) password:protect([protect(password) stringValue]) persistence:([protect(remember) state] == NSControlStateValueOn) ? NSURLCredentialPersistencePermanent : NSURLCredentialPersistenceForSession]);

    [callback performSelector:selector withObject:protectedChallenge withObject:credential];
}

- (void)runAsSheetOnWindow:(NSWindow *)window withChallenge:(NSURLAuthenticationChallenge *)chall
{
    ASSERT(!usingSheet);

    [self setUpForChallenge:chall];

    usingSheet = TRUE;
    challenge = chall;

    [window beginSheet:protect(panel) completionHandler:^(NSModalResponse modalResponse) {
        int returnCode = (modalResponse == NSModalResponseCancel) ? 1 : 0;
        [self sheetDidEnd:protect(panel) returnCode:returnCode contextInfo:NULL];
    }];
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
    RetainPtr<NSURLCredential> credential;

    ASSERT(usingSheet);
    ASSERT(challenge);

    if (!returnCode)
        credential = adoptNS([[NSURLCredential alloc] initWithUser:protect([protect(username) stringValue]) password:protect([protect(password) stringValue]) persistence:([protect(remember) state] == NSControlStateValueOn) ? NSURLCredentialPersistencePermanent : NSURLCredentialPersistenceForSession]);

    // We take this tricky approach to nilling out and releasing the challenge
    // because the callback below might remove our last ref.
    RetainPtr chall = std::exchange(challenge, nil);
    [callback performSelector:selector withObject:chall withObject:credential];
}

@end

@implementation WebNonBlockingPanel

- (BOOL)_blocksActionWhenModal:(SEL)theAction
{
    // This override of a private AppKit method allows the user to quit when a login dialog
    // is onscreen, which is nice in general but in particular prevents pathological cases
    // like 3744583 from requiring a Force Quit.
    //
    // It would be nice to allow closing the individual window as well as quitting the app when
    // a login sheet is up, but this _blocksActionWhenModal: mechanism doesn't support that.
    // This override matches those in NSOpenPanel and NSToolbarConfigPanel.
    if (theAction == @selector(terminate:))
        return NO;
    return YES;
}

@end

#endif // !PLATFORM(IOS_FAMILY)
