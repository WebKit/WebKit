//
//  WebJavaScriptTextInputPanel.h
//  WebKit
//
//  Created by Darin Adler on Tue October 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface WebJavaScriptTextInputPanel : NSWindowController
{
    IBOutlet NSTextField *prompt;
    IBOutlet NSTextField *textInput;
}

- (id)initWithPrompt:(NSString *)prompt text:(NSString *)text;
- (NSString *)text;

- (IBAction)pressedCancel:(id)sender;
- (IBAction)pressedOK:(id)sender;

@end
