/*
    WebAuthenticationPanel.m

    Copyright 2002 Apple, Inc. All rights reserved.
*/


#import <WebKit/WebAuthenticationPanel.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <WebKit/WebNSControlExtras.h>
#import <WebKit/WebStandardPanelsPrivate.h>

#define WebAuthenticationPanelNibName @"WebAuthenticationPanel"

@implementation WebAuthenticationPanel

-(id)initWithCallback:(id)cb selector:(SEL)sel
{
    self = [self init];
    if (self != nil) {
        callback = [cb retain];
        selector = sel;
    }
    return self;
}


- (void)dealloc
{
    [panel release];

    [callback release];
    
    [super dealloc];
}

// IB actions

- (IBAction)cancel:(id)sender
{
    // This is required because the body of this method is going to
    // remove all of the panel's remaining refs, which can cause a
    // crash later when finishing button hit tracking.  So we make
    // sure it lives on a bit longer.
    [[panel retain] autorelease];

    [panel close];
    if (usingSheet) {
	[[NSApplication sharedApplication] endSheet:panel returnCode:1];
    } else {
	[[NSApplication sharedApplication] stopModalWithCode:1];
    }
}

- (IBAction)logIn:(id)sender
{
    // This is required because the body of this method is going to
    // remove all of the panel's remaining refs, which can cause a
    // crash later when finishing button hit tracking.  So we make
    // sure it lives on a bit longer.
    [[panel retain] autorelease];

    [panel close];
    if (usingSheet) {
	[[NSApplication sharedApplication] endSheet:panel returnCode:0];
    } else {
	[[NSApplication sharedApplication] stopModalWithCode:0];
    }
}

- (BOOL)loadNib
{
    if (!nibLoaded) {
        if ([NSBundle loadNibNamed:WebAuthenticationPanelNibName owner:self]) {
            nibLoaded = YES;
            [imageView setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
        } else {
            ERROR("couldn't load nib named '%@'", WebAuthenticationPanelNibName);
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
    if ([space port] == 0) {
	host = [space host];
    } else {
	host = [NSString stringWithFormat:@"%@:%u", [space host], [space port]];
    }

    NSString *realm = [space realm];
    NSString *message;

    if ([challenge previousFailureCount] == 0) {
        if ([space isProxy]) {
            message = [NSString stringWithFormat:UI_STRING("To view this page, you need to log in to the %@ proxy server %@.",
                                                           "prompt string in authentication panel"),
                [space type], host];
        } else {
            message = [NSString stringWithFormat:UI_STRING("To view this page, you need to log in to area “%@” on %@.",
                                                           "prompt string in authentication panel"),
                realm, host];
        }
    } else {
        if ([space isProxy]) {
            message = [NSString stringWithFormat:UI_STRING("The name or password entered for the %@ proxy server %@ was incorrect. Please try again.",
                                                           "prompt string in authentication panel"),
				[space type], host];
        } else {
            message = [NSString stringWithFormat:UI_STRING("The name or password entered for area “%@” on %@ was incorrect. Please try again.",
                                                           "prompt string in authentication panel"),
                realm, host];
        }
    }

    [mainLabel setStringValue:message];
    [mainLabel sizeToFitAndAdjustWindowHeight];

    if ([space receivesCredentialSecurely]) {
        [smallLabel setStringValue:
            UI_STRING("Your log-in information will be sent securely.",
                "message in authentication panel")];
    } else {
        [smallLabel setStringValue:
            UI_STRING("Your password will be sent in the clear.",
                "message in authentication panel")];
    }

    if ([[challenge proposedCredential] user] != nil) {
        [username setStringValue:[[challenge proposedCredential] user]];
        [panel setInitialFirstResponder:password];
    } else {
        [username setStringValue:@""];
        [password setStringValue:@""];
        [panel setInitialFirstResponder:username];
    }
}

- (void)runAsModalDialogWithChallenge:(NSURLAuthenticationChallenge *)chall
{
    [self setUpForChallenge:chall];
    usingSheet = FALSE;
    NSURLCredential *credential = nil;

    if ([[NSApplication sharedApplication] runModalForWindow:panel] == 0) {
        credential = [NSURLCredential URLCredentialWithUser:[username stringValue] password:[password stringValue] persistence:([remember state] == NSOnState) ? NSURLCredentialPersistencePermanent : NSURLCredentialPersistenceForSession];
    }

    [callback performSelector:selector withObject:chall withObject:credential];
}

- (void)runAsSheetOnWindow:(NSWindow *)window withChallenge:(NSURLAuthenticationChallenge *)chall
{
    ASSERT(!usingSheet);

    [self setUpForChallenge:chall];

    usingSheet = TRUE;
    challenge = [chall retain];
    
    [[NSApplication sharedApplication] beginSheet:panel modalForWindow:window modalDelegate:self didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
    NSURLCredential *credential = nil;
    NSURLAuthenticationChallenge *chall;

    ASSERT(usingSheet);
    ASSERT(challenge != nil);

    if (returnCode == 0) {
        credential = [NSURLCredential URLCredentialWithUser:[username stringValue] password:[password stringValue] persistence:([remember state] == NSOnState) ? NSURLCredentialPersistencePermanent : NSURLCredentialPersistenceForSession];
    }

    // We take this tricky approach to nilling out and releasing the challenge
    // because the callback below might remove our last ref.
    chall = challenge;
    challenge = nil;
    [callback performSelector:selector withObject:chall withObject:credential];
    [chall release];
}

@end
