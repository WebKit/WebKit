/*
    WebAuthenticationPanel.m

    Copyright 2002 Apple, Inc. All rights reserved.
*/


#import <WebKit/WebAuthenticationPanel.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebFoundation/WebAssertions.h>


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
    [panel close];
    if (usingSheet) {
	[[NSApplication sharedApplication] endSheet:panel returnCode:1];
    } else {
	[[NSApplication sharedApplication] stopModalWithCode:1];
    }
}

- (IBAction)logIn:(id)sender
{
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
            NSLog(@"%s:%d  %s: couldn't load nib named '%@'",
                  __FILE__, __LINE__, __FUNCTION__,WebAuthenticationPanelNibName);
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

    // FIXME Radar 2876448: we should display a different dialog depending on the
    // failure count (if the user tried and failed, the dialog should
    // explain possible reasons)
    // FIXME Radar 2876446: need to automatically adjust height of main label
    [mainLabel setStringValue:[NSString stringWithFormat:@"To view this page, you need to log in to area \"%@\" on %@.", [resource realm], [[resource URL] host]]];
    if ([resource receivesCredentialSecurely]) {
        [smallLabel setStringValue:@"Your log-in information will be sent securely."];
    } else {
        [smallLabel setStringValue:@"Your password will be sent in the clear."];
    }

    if ([resource username] != nil) {
        [username setStringValue:[resource username]];
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
        credential = [WebCredential credentialWithUsername:[username stringValue] password:[password stringValue] remembered:NO];
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
        credential = [WebCredential credentialWithUsername:[username stringValue] password:[password stringValue] remembered:NO];
    }

    // We take this tricky approach to nilling out and releasing the request,
    // because the callback below might remove our last ref.
    req = request;
    request = nil;
    [callback performSelector:selector withObject:req withObject:credential];
    [req release];
}

@end
