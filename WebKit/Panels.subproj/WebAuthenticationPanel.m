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

-(void)setUpForRequest:(WebAuthenticationRequest *)req
{
    [self loadNib];

    WebAuthenticatingResource *resource = [req resource];

    NSString *host;
    if ([resource port] == 0) {
	host = [resource host];
    } else {
	host = [NSString stringWithFormat:@"%@:%u", [resource host], [resource port]];
    }

    NSString *realm = [resource realm];
    NSString *message;

    if ([req previousFailureCount] == 0) {
        if ([resource isProxy]) {
            message = [NSString stringWithFormat:UI_STRING("To view this page, you need to log in to the %@ proxy server %@.",
                                                           "prompt string in authentication panel"),
                [resource type], host];
        } else {
            message = [NSString stringWithFormat:UI_STRING("To view this page, you need to log in to area “%@” on %@.",
                                                           "prompt string in authentication panel"),
                realm, host];
        }
    } else {
        if ([resource isProxy]) {
            message = [NSString stringWithFormat:UI_STRING("The name or password entered for the %@ proxy server %@ was incorrect. Please try again.",
                                                           "prompt string in authentication panel"),
				[resource type], host];
        } else {
            message = [NSString stringWithFormat:UI_STRING("The name or password entered for area “%@” on %@ was incorrect. Please try again.",
                                                           "prompt string in authentication panel"),
                realm, host];
        }
    }

    [mainLabel setStringValue:message];
    [mainLabel sizeToFitAndAdjustWindowHeight];

    if ([resource receivesCredentialSecurely]) {
        [smallLabel setStringValue:
            UI_STRING("Your log-in information will be sent securely.",
                "message in authentication panel")];
    } else {
        [smallLabel setStringValue:
            UI_STRING("Your password will be sent in the clear.",
                "message in authentication panel")];
    }

    if ([req defaultUsername] != nil) {
        [username setStringValue:[req defaultUsername]];
        [panel setInitialFirstResponder:password];
    } else {
        [username setStringValue:@""];
        [password setStringValue:@""];
        [panel setInitialFirstResponder:username];
    }
}

- (void)runAsModalDialogWithRequest:(WebAuthenticationRequest *)req
{
    [self setUpForRequest:req];
    usingSheet = FALSE;
    WebCredential *credential = nil;

    if ([[NSApplication sharedApplication] runModalForWindow:panel] == 0) {
        credential = [WebCredential credentialWithUsername:[username stringValue] password:[password stringValue] remembered:[remember state] == NSOnState];
    }

    [callback performSelector:selector withObject:req withObject:credential];
}

- (void)runAsSheetOnWindow:(NSWindow *)window withRequest:(WebAuthenticationRequest *)req
{
    ASSERT(!usingSheet);

    [self setUpForRequest:req];

    usingSheet = TRUE;
    request = [req retain];
    
    [[NSApplication sharedApplication] beginSheet:panel modalForWindow:window modalDelegate:self didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
    WebCredential *credential = nil;
    WebAuthenticationRequest *req;

    ASSERT(usingSheet);
    ASSERT(request != nil);

    if (returnCode == 0) {
        credential = [WebCredential credentialWithUsername:[username stringValue] password:[password stringValue] remembered:[remember state] == NSOnState];
    }

    // We take this tricky approach to nilling out and releasing the request,
    // because the callback below might remove our last ref.
    req = request;
    request = nil;
    [callback performSelector:selector withObject:req withObject:credential];
    [req release];
}

@end
