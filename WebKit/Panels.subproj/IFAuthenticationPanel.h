/*	
    IFAuthenticationPanel.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <WebFoundation/IFAuthenticationManager.h>

@interface IFAuthenticationPanel : NSObject
{
    IBOutlet id mainLabel;
    IBOutlet id panel;
    IBOutlet id password;
    IBOutlet id smallLabel;
    IBOutlet id username;
    IBOutlet id imageView;
    BOOL nibLoaded;
    BOOL usingSheet;
    id callback;
    SEL selector;
    IFAuthenticationRequest *request;
}

-(id)initWithCallback:(id)cb selector:(SEL)sel;

// Interface-related methods
- (IBAction)cancel:(id)sender;
- (IBAction)logIn:(id)sender;

- (BOOL)loadNib;

- (void)runAsModalDialogWithRequest:(IFAuthenticationRequest *)req;
- (void)runAsSheetOnWindow:(NSWindow *)window withRequest:(IFAuthenticationRequest *)req;

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;


@end
