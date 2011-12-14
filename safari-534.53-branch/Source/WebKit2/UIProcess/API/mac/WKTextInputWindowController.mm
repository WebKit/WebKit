/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKTextInputWindowController.h"

#import <WebKitSystemInterface.h>

@interface WKTextInputPanel : NSPanel {
    NSTextView *_inputTextView;
}

- (NSTextInputContext *)_inputContext;
- (BOOL)_interpretKeyEvent:(NSEvent *)event string:(NSString **)string;

@end

#define inputWindowHeight 20

@implementation WKTextInputPanel

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [_inputTextView release];
    
    [super dealloc];
}

- (id)init
{
    self = [super initWithContentRect:NSZeroRect styleMask:WKGetInputPanelWindowStyle() backing:NSBackingStoreBuffered defer:YES];
    if (!self)
        return nil;
    
    // Set the frame size.
    NSRect visibleFrame = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(visibleFrame.origin.x, visibleFrame.origin.y, visibleFrame.size.width, inputWindowHeight);
     
    [self setFrame:frame display:NO];
        
    _inputTextView = [[NSTextView alloc] initWithFrame:[(NSView *)self.contentView frame]];        
    _inputTextView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable | NSViewMaxXMargin | NSViewMinXMargin | NSViewMaxYMargin | NSViewMinYMargin;
        
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:[(NSView *)self.contentView frame]];
    scrollView.documentView = _inputTextView;
    self.contentView = scrollView;
    [scrollView release];
        
    [self setFloatingPanel:YES];

    return self;
}

- (void)_keyboardInputSourceChanged
{
    [_inputTextView setString:@""];
    [self orderOut:nil];
}

- (BOOL)_interpretKeyEvent:(NSEvent *)event string:(NSString **)string
{
    BOOL hadMarkedText = [_inputTextView hasMarkedText];
 
    *string = nil;

    // Let TSM know that a bottom input window would be created for marked text.
    EventRef carbonEvent = static_cast<EventRef>(const_cast<void*>([event eventRef]));
    if (carbonEvent) {
        Boolean ignorePAH = true;
        SetEventParameter(carbonEvent, 'iPAH', typeBoolean, sizeof(ignorePAH), &ignorePAH);
    }

    if (![[_inputTextView inputContext] handleEvent:event])
        return NO;
    
    if ([_inputTextView hasMarkedText]) {
        // Don't show the input method window for dead keys
        if ([[event characters] length] > 0)
            [self orderFront:nil];

        return YES;
    }
    
    if (hadMarkedText) {
        [self orderOut:nil];

        NSString *text = [[_inputTextView textStorage] string];
        if ([text length] > 0)
            *string = [[text copy] autorelease];
    }
            
    [_inputTextView setString:@""];
    return hadMarkedText;
}

- (NSTextInputContext *)_inputContext
{
    return [_inputTextView inputContext];
}

@end

@implementation WKTextInputWindowController

+ (WKTextInputWindowController *)sharedTextInputWindowController
{
    static WKTextInputWindowController *textInputWindowController;
    if (!textInputWindowController)
        textInputWindowController = [[WKTextInputWindowController alloc] init];
    
    return textInputWindowController;
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _panel = [[WKTextInputPanel alloc] init];
    
    return self;
}

- (NSTextInputContext *)inputContext
{
    return [_panel _inputContext];
}

- (BOOL)interpretKeyEvent:(NSEvent *)event string:(NSString **)string
{
    return [_panel _interpretKeyEvent:event string:string];
}

- (void)keyboardInputSourceChanged
{
    [_panel _keyboardInputSourceChanged];
}

@end
