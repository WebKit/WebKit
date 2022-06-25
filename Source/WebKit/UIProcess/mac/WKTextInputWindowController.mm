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

#if USE(APPKIT)

#import <Carbon/Carbon.h>
#import <pal/spi/mac/HIToolboxSPI.h>
#import <pal/system/mac/WebPanel.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

@interface WKTextInputView : NSTextView
@end

@implementation WKTextInputView

- (NSArray *)validAttributesForMarkedText
{
    // Let TSM know that a bottom input window would be created for marked text.
    return [[super validAttributesForMarkedText] arrayByAddingObject:@"__NSUsesFloatingInputWindow"];
}

@end

@interface WKTextInputPanel : WebPanel {
    RetainPtr<NSTextView> _inputTextView;
}

- (NSTextInputContext *)_inputContext;
- (BOOL)_interpretKeyEvent:(NSEvent *)event usingLegacyCocoaTextInput:(BOOL)usingLegacyCocoaTextInput string:(NSString **)string;

- (BOOL)_hasMarkedText;
- (void)_unmarkText;

@end

#define inputWindowHeight 20

@implementation WKTextInputPanel

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [super dealloc];
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    // Set the frame size.
    NSRect visibleFrame = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(visibleFrame.origin.x, visibleFrame.origin.y, visibleFrame.size.width, inputWindowHeight);
     
    [self setFrame:frame display:NO];
        
    _inputTextView = adoptNS([[WKTextInputView alloc] initWithFrame:[(NSView *)self.contentView frame]]);
    [_inputTextView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable | NSViewMaxXMargin | NSViewMinXMargin | NSViewMaxYMargin | NSViewMinYMargin];
        
    auto scrollView = adoptNS([[NSScrollView alloc] initWithFrame:[(NSView *)self.contentView frame]]);
    [scrollView setDocumentView: _inputTextView.get()];
    self.contentView = scrollView.get();
        
    [self setFloatingPanel:YES];

    return self;
}

- (void)_unmarkText
{
    [_inputTextView setString:@""];
    [self orderOut:nil];
}

- (BOOL)_interpretKeyEvent:(NSEvent *)event usingLegacyCocoaTextInput:(BOOL)usingLegacyCocoaTextInput string:(NSString **)string
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    BOOL hadMarkedText = [_inputTextView hasMarkedText];
    ALLOW_DEPRECATED_DECLARATIONS_END
 
    *string = nil;

    // Let TSM know that a bottom input window would be created for marked text.
    // FIXME: Can be removed once we can rely on __NSUsesFloatingInputWindow (or a better API) being available everywhere.
    EventRef carbonEvent = static_cast<EventRef>(const_cast<void*>([event eventRef]));
    if (carbonEvent) {
        Boolean ignorePAH = true;
        SetEventParameter(carbonEvent, 'iPAH', typeBoolean, sizeof(ignorePAH), &ignorePAH);
    }

    if (![[_inputTextView inputContext] handleEvent:event])
        return NO;
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if ([_inputTextView hasMarkedText]) {
        ALLOW_DEPRECATED_DECLARATIONS_END
        // Don't show the input method window for dead keys
        if ([[event characters] length] > 0)
            [self orderFront:nil];

        return YES;
    }

    bool shouldReturnTextString = hadMarkedText;

    // In the updated Cocoa text input model spec, we always want to return the text even if the text view didn't have marked text.
    if (!usingLegacyCocoaTextInput)
        shouldReturnTextString = true;

    if (shouldReturnTextString) {
        [[_inputTextView inputContext] discardMarkedText]; // Don't show Cangjie predictive candidates, cf. <rdar://14458297>.
        [self orderOut:nil];

        NSString *text = [[_inputTextView textStorage] string];
        if ([text length] > 0)
            *string = adoptNS([text copy]).autorelease();
    }
            
    [_inputTextView setString:@""];
    return hadMarkedText;
}

- (NSTextInputContext *)_inputContext
{
    return [_inputTextView inputContext];
}

- (BOOL)_hasMarkedText
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [_inputTextView hasMarkedText];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

@end

@implementation WKTextInputWindowController

+ (WKTextInputWindowController *)sharedTextInputWindowController
{
    static NeverDestroyed<RetainPtr<WKTextInputWindowController>> textInputWindowController = adoptNS([[WKTextInputWindowController alloc] init]);
    return textInputWindowController.get().get();
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

- (BOOL)hasMarkedText
{
    return [_panel _hasMarkedText];
}

- (BOOL)interpretKeyEvent:(NSEvent *)event usingLegacyCocoaTextInput:(BOOL)usingLegacyCocoaTextInput string:(NSString **)string
{
    return [_panel _interpretKeyEvent:event usingLegacyCocoaTextInput:usingLegacyCocoaTextInput string:string];
}

- (void)unmarkText
{
    [_panel _unmarkText];
}

@end

#endif // USE(APPKIT)
