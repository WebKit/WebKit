//
//  WebJavaScriptTextInputPanel.m
//  WebKit
//
//  Created by Darin Adler on Tue October 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebJavaScriptTextInputPanel.h"

@implementation WebJavaScriptTextInputPanel

- (id)initWithPrompt:(NSString *)p text:(NSString *)t
{
    [self initWithWindowNibName:@"WebJavaScriptTextInputPanel"];
    NSWindow *window = [self window];
    
    // This must be done after the first call to [self window], because
    // until then, prompt and textInput will be nil.
    ASSERT(prompt);
    ASSERT(textInput);
    [prompt setStringValue:p];
    [textInput setStringValue:t];

    // Resize the prompt field, then the window, to account for the prompt text.
    float promptHeightBefore = [prompt frame].size.height;
    [prompt sizeToFit];
    NSRect promptFrame = [prompt frame];
    float heightDelta = promptFrame.size.height - promptHeightBefore;
    promptFrame.origin.y -= heightDelta;
    [prompt setFrame:promptFrame];
    NSRect windowFrame = [window frame];
    windowFrame.size.height += heightDelta;
    [window setFrame:windowFrame display:NO];
    
    return self;
}

- (NSString *)text
{
    return [textInput stringValue];
}

- (IBAction)pressedCancel:(id)sender
{
    [NSApp stopModalWithCode:NO];
}

- (IBAction)pressedOK:(id)sender
{
    [NSApp stopModalWithCode:YES];
}

@end
