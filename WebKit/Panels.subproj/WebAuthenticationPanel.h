/*	
    WebAuthenticationPanel.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <WebFoundation/WebAuthenticationManager.h>

@interface WebAuthenticationPanel : NSObject
{
    IBOutlet id mainLabel;
    IBOutlet id panel;
    IBOutlet id password;
    IBOutlet id smallLabel;
    IBOutlet id username;
    IBOutlet id imageView;
    IBOutlet id remember;
    BOOL nibLoaded;
    BOOL usingSheet;
    id callback;
    SEL selector;
    WebAuthenticationRequest *request;
}

-(id)initWithCallback:(id)cb selector:(SEL)sel;

// Interface-related methods
- (IBAction)cancel:(id)sender;
- (IBAction)logIn:(id)sender;

- (BOOL)loadNib;

- (void)runAsModalDialogWithRequest:(WebAuthenticationRequest *)req;
- (void)runAsSheetOnWindow:(NSWindow *)window withRequest:(WebAuthenticationRequest *)req;

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;


@end
