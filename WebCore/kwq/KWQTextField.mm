/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "KWQTextField.h"

#import "KWQView.h"
#import "KWQLineEdit.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "WebCoreFirstResponderChanges.h"
#import "WebCoreBridge.h"

// KWQTextFieldFormatter enforces a maximum length.

@interface KWQTextFieldFormatter : NSFormatter
{
    int maxLength;
}

- (void)setMaximumLength:(int)len;
- (int)maximumLength;

@end

// KWQSecureTextField has two purposes.
// One is a workaround for bug 3024443.
// The other is hook up next and previous key views to KHTML.

@interface KWQSecureTextField : NSSecureTextField <KWQWidgetHolder>
{
    QWidget *widget;
    BOOL inSetFrameSize;
    BOOL inNextValidKeyView;
}

- initWithQWidget:(QWidget *)widget;

@end

@implementation KWQTextField

- (void)setUpTextField:(NSTextField *)field
{
    // This is initialization that's shared by both self and the secure text field.

    [[field cell] setScrollable:YES];
    
    [field setFormatter:formatter];

    [field setDelegate:self];
    
    [field setTarget:self];
    [field setAction:@selector(action:)];
}

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    formatter = [[KWQTextFieldFormatter alloc] init];
    [self setUpTextField:self];
    return self;
}

- initWithQLineEdit:(QLineEdit *)w 
{
    widget = w;
    return [super init];
}

- (void)action:sender
{
    widget->returnPressed();
}

- (void)dealloc
{
    [secureField release];
    [formatter release];
    [super dealloc];
}

- (KWQTextFieldFormatter *)formatter
{
    return formatter;
}

- (void)updateSecureFieldFrame
{
    [secureField setFrame:[self bounds]];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self updateSecureFieldFrame];
}

- (void)setPasswordMode:(BOOL)flag
{
    if (!flag == ![secureField superview]) {
        return;
    }
    
    [self setStringValue:@""];
    if (!flag) {
        [secureField removeFromSuperview];
    } else {
        if (secureField == nil) {
            secureField = [[KWQSecureTextField alloc] initWithQWidget:widget];
            [secureField setFormatter:formatter];
            [secureField setFont:[self font]];
            [self setUpTextField:secureField];
            [self updateSecureFieldFrame];
        }
        [self addSubview:secureField];
    }
    [self setStringValue:@""];
}

- (void)setEditable:(BOOL)flag
{
    [secureField setEditable:flag];
    [super setEditable:flag];
}

- (void)selectText:(id)sender
{
    if ([self passwordMode]) {
        [secureField selectText:sender];
        return;
    }
    
    // Don't call the NSTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events.
    NSText *editor = [self currentEditor];
    if (editor) {
        [editor setSelectedRange:NSMakeRange(0, [[editor string] length])];
    } else {
        [super selectText:sender];
    }
}

- (BOOL)isEditable
{
    return [super isEditable];
}

- (BOOL)passwordMode
{
    return [secureField superview] != nil;
}

- (void)setMaximumLength:(int)len
{
    NSString *oldValue = [self stringValue];
    if ((int)[oldValue length] > len) {
        [self setStringValue:[oldValue substringToIndex:len]];
    }
    [formatter setMaximumLength:len];
}

- (int)maximumLength
{
    return [formatter maximumLength];
}

- (BOOL)edited
{
    return edited;
}

- (void)setEdited:(BOOL)ed
{
    edited = ed;
}

- (void)controlTextDidBeginEditing:(NSNotification *)obj
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidBeginEditing:obj];
}

- (void)controlTextDidEndEditing:(NSNotification *)obj
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidEndEditing:obj];
}

- (void)controlTextDidChange:(NSNotification *)obj
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidChange:obj];
    edited = true;
    widget->textChanged();
}

- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textShouldBeginEditing:fieldEditor];
}

- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textShouldEndEditing:fieldEditor];
}

- (BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control didFailToFormatString:string errorDescription:error];
}

- (void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge control:control didFailToValidatePartialString:string errorDescription:error];
}

- (BOOL)control:(NSControl *)control isValidObject:(id)obj
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control isValidObject:obj];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textView:textView doCommandBySelector:commandSelector];
}

- (NSString *)stringValue
{
    if ([secureField superview]) {
        return [secureField stringValue];
    }
    return [super stringValue];
}

- (void)setStringValue:(NSString *)string
{
    [secureField setStringValue:string];
    [super setStringValue:string];
    widget->textChanged();
}

- (void)setFont:(NSFont *)font
{
    [secureField setFont:font];
    [super setFont:font];
}

- (NSView *)nextKeyView
{
    return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
   return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious)
        : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (BOOL)becomeFirstResponder
{
    KWQKHTMLPart::setDocumentFocus(widget);
    if (!widget->hasFocus()) {
        return NO;
    }
    [self _KWQ_scrollFrameToVisible];
    return [super becomeFirstResponder];
}

- (void)fieldEditorWillBecomeFirstResponder
{
    //FIXME:  Note due to 3178518, this code is never called.
    QFocusEvent event(QEvent::FocusIn);
    (const_cast<QObject *>(widget->eventFilterObject()))->eventFilter(widget, &event);
}

- (void)fieldEditorWillResignFirstResponder
{
    QFocusEvent event(QEvent::FocusOut);
    (const_cast<QObject *>(widget->eventFilterObject()))->eventFilter(widget, &event);
}

- (void)display
{
    // This is a workaround for Radar 2753974.
    // Also, in the web page context, it's never OK to just display.
    [self setNeedsDisplay:YES];
}

- (QWidget *)widget
{
    return widget;
}

@end

@implementation KWQTextFieldFormatter

- init
{
    [super init];
    maxLength = INT_MAX;
    return self;
}

- (void)setMaximumLength:(int)len
{
    maxLength = len;
}

- (int)maximumLength
{
    return maxLength;
}

- (NSString *)stringForObjectValue:(id)object
{
    return (NSString *)object;
}

- (BOOL)getObjectValue:(id *)object forString:(NSString *)string errorDescription:(NSString **)error
{
    *object = string;
    return YES;
}

- (BOOL)isPartialStringValid:(NSString *)partialString newEditingString:(NSString **)newString errorDescription:(NSString **)error
{
    if ((int)[partialString length] > maxLength) {
        *newString = nil;
        return NO;
    }

    return YES;
}

- (NSAttributedString *)attributedStringForObjectValue:(id)anObject withDefaultAttributes:(NSDictionary *)attributes
{
    return nil;
}

@end

@implementation KWQSecureTextField

- initWithQWidget:(QWidget *)w
{
    widget = w;
    return [super init];
}

- (NSView *)nextKeyView
{
    return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
   return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious)
        : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

// These next two methods are the workaround for bug 3024443.
// Basically, setFrameSize ends up calling an inappropriate selectText, so we just ignore
// calls to selectText while setFrameSize is running.

- (void)selectText:(id)sender
{
    if (sender == self && inSetFrameSize) {
        return;
    }
    
    // Don't call the NSTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events.
    NSText *editor = [self currentEditor];
    if (editor) {
        [editor setSelectedRange:NSMakeRange(0, [[editor string] length])];
    } else {
        [super selectText:sender];
    }
}

- (void)setFrameSize:(NSSize)size
{
    inSetFrameSize = YES;
    [super setFrameSize:size];
    inSetFrameSize = NO;
}

- (BOOL)becomeFirstResponder
{
    KWQKHTMLPart::setDocumentFocus(widget);
    [self _KWQ_scrollFrameToVisible];
    return [super becomeFirstResponder];
}

- (void)display
{
    // This is a workaround for Radar 2753974.
    // Also, in the web page context, it's never OK to just display.
    [self setNeedsDisplay:YES];
}

- (QWidget *)widget
{
    return widget;
}

@end
