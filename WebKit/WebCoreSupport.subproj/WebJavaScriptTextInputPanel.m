//
//  WebJavaScriptTextInputPanel.m
//  WebKit
//
//  Created by Darin Adler on Tue October 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebJavaScriptTextInputPanel.h"

#import <WebKit/WebAssertions.h>

#import <WebKit/WebNSControlExtras.h>
#import <WebKit/WebNSWindowExtras.h>

@implementation WebJavaScriptTextInputPanel

- (id)initWithPrompt:(NSString *)p text:(NSString *)t
{
    [self initWithWindowNibName:@"WebJavaScriptTextInputPanel"];
    NSWindow *window = [self window];
    
    // This must be done after the call to [self window], because
    // until then, prompt and textInput will be nil.
    ASSERT(prompt);
    ASSERT(textInput);
    [prompt setStringValue:p];
    [textInput setStringValue:t];

    [prompt sizeToFitAndAdjustWindowHeight];
    [window centerOverMainWindow];
    
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
