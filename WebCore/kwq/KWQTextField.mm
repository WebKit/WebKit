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

#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"
#import "KWQLineEdit.h"
#import "KWQNSViewExtras.h"
#import "KWQView.h"
#import "WebCoreBridge.h"

@interface NSString (KWQTextField)
- (int)_KWQ_numComposedCharacterSequences;
- (NSString *)_KWQ_truncateToNumComposedCharacterSequences:(int)num;
@end

@interface NSCell (KWQTextFieldKnowsAppKitSecrets)
- (NSMutableDictionary *)_textAttributes;
@end

@interface KWQTextField (KWQInternal)
- (void)setHasFocus:(BOOL)hasFocus;
@end

// KWQTextFieldCell allows us to tell when we get focus without an editor subclass,
// and override the base writing direction.
@interface KWQTextFieldCell : NSTextFieldCell
{
    NSWritingDirection baseWritingDirection;
}
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

// KWQTextFieldFormatter enforces a maximum length.
@interface KWQTextFieldFormatter : NSFormatter
{
    int maxLength;
}
- (void)setMaximumLength:(int)len;
- (int)maximumLength;
@end

// KWQSecureTextField has a few purposes.
// One is a workaround for bug 3024443.
// Another is hook up next and previous key views to KHTML.
@interface KWQSecureTextField : NSSecureTextField <KWQWidgetHolder>
{
    QLineEdit *widget;
    BOOL inNextValidKeyView;
    BOOL inSetFrameSize;
}

- (id)initWithQLineEdit:(QLineEdit *)widget;

@end

// KWQSecureTextFieldCell allows us to tell when we get focus without an editor subclass,
// and override the base writing direction.
@interface KWQSecureTextFieldCell : NSSecureTextFieldCell
{
    NSWritingDirection baseWritingDirection;
}
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
@end

@implementation KWQTextField

+ (void)initialize
{
    if (self == [KWQTextField class]) {
        [self setCellClass:[KWQTextFieldCell class]];
    }
}

- (void)setUpTextField:(NSTextField *)field
{
    // This is initialization that's shared by both self and the secure text field.

    [[field cell] setScrollable:YES];
    
    [field setFormatter:formatter];

    [field setDelegate:self];
    
    [field setTarget:self];
    [field setAction:@selector(action:)];
}

-(id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    formatter = [[KWQTextFieldFormatter alloc] init];
    [self setUpTextField:self];
    lastSelectedRange.location = NSNotFound;
    return self;
}

-(id)initWithQLineEdit:(QLineEdit *)w 
{
    widget = w;
    return [self init];
}

-(void)invalidate
{
    widget = NULL;
}

- (void)action:sender
{
    if (!widget) {
	return;
    }

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
    
    if (!flag) {
        // Don't use [self setStringValue:] because there are unwanted side effects,
        // like sending out a text changed signal.
        [super setStringValue:[secureField stringValue]];
        [secureField removeFromSuperview];
    } else {
        if (secureField == nil) {
            secureField = [[KWQSecureTextField alloc] initWithQLineEdit:widget];
            [secureField setFormatter:formatter];
            [secureField setFont:[self font]];
            [secureField setEditable:[self isEditable]];
            [secureField setSelectable:[self isSelectable]];
            [[secureField cell] setBaseWritingDirection:[[self cell] baseWritingDirection]];
            [self setUpTextField:secureField];
            [self updateSecureFieldFrame];
        }
        [secureField setStringValue:[super stringValue]];
        [self addSubview:secureField];
    }
}

- (void)setEditable:(BOOL)flag
{
    [secureField setEditable:flag];
    [super setEditable:flag];
}

- (void)setSelectable:(BOOL)flag
{
    [secureField setSelectable:flag];
    [super setSelectable:flag];
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
        return;
    }
    
    [super selectText:sender];
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
    if ([oldValue _KWQ_numComposedCharacterSequences] > len) {
        [self setStringValue:[oldValue _KWQ_truncateToNumComposedCharacterSequences:len]];
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

-(void)controlTextDidBeginEditing:(NSNotification *)notification
{
    if (!widget) {
	return;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidBeginEditing:notification];
}

-(void)controlTextDidEndEditing:(NSNotification *)notification
{
    if (!widget) {
	return;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidEndEditing:notification];
    
    [self setHasFocus:NO];
}

-(void)controlTextDidChange:(NSNotification *)notification
{
    if (!widget) {
	return;
    }
    
    if (KWQKHTMLPart::handleKeyboardOptionTabInView(self)) {
        return;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidChange:notification];
    
    edited = YES;
    widget->textChanged();
}

-(BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
    if (!widget) {
	return NO;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);

    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which some views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // turn off this caching for all views contained in a WebHTMLView that
    // would otherwise do it. Here we turn it off for the editor (NSTextView)
    // used for text fields in forms and the clip view it's embedded in.
    // See bug 3457875 and 3310943 for more context.
    [fieldEditor releaseGState];
    [[fieldEditor superview] releaseGState];

    return [bridge control:control textShouldBeginEditing:fieldEditor];
}

-(BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor
{
    if (!widget) {
	return NO;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textShouldEndEditing:fieldEditor];
}

-(BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error
{
    if (!widget) {
	return NO;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control didFailToFormatString:string errorDescription:error];
}

-(void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error
{
    if (!widget) {
	return;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge control:control didFailToValidatePartialString:string errorDescription:error];
}

-(BOOL)control:(NSControl *)control isValidObject:(id)obj
{
    if (!widget) {
	return NO;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control isValidObject:obj];
}

-(BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    if (!widget) {
	return NO;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textView:textView doCommandBySelector:commandSelector];
}

-(NSString *)stringValue
{
    if ([secureField superview]) {
        return [secureField stringValue];
    }
    return [super stringValue];
}

-(void)setStringValue:(NSString *)string
{
    if (!widget) {
	return;
    }

    int maxLength = [formatter maximumLength];
    string = [string _KWQ_truncateToNumComposedCharacterSequences:maxLength];
    [secureField setStringValue:string];
    [super setStringValue:string];
    widget->textChanged();
}

-(void)setFont:(NSFont *)font
{
    [secureField setFont:font];
    [super setFont:font];
}

-(NSView *)nextKeyView
{
    if (!widget) {
	return [super nextKeyView];
    }

    return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

-(NSView *)previousKeyView
{
    if (!widget) {
	return [super previousKeyView];
    }

    return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious)
        : [super previousKeyView];
}

-(NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

-(NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (BOOL)acceptsFirstResponder
{
    return [self isEnabled];
}

- (BOOL)becomeFirstResponder
{
    if ([self passwordMode]) {
        return [[self window] makeFirstResponder:secureField];
    }
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

- (void)setAlignment:(NSTextAlignment)alignment
{
    [secureField setAlignment:alignment];
    [super setAlignment:alignment];
}

// This is the only one of the display family of calls that we use, and the way we do
// displaying in WebCore means this is called on this NSView explicitly, so this catches
// all cases where we are inside the normal display machinery. (Used only by the insertion
// point method below.)
- (void)displayRectIgnoringOpacity:(NSRect)rect
{
    inDrawingMachinery = YES;
    [super displayRectIgnoringOpacity:rect];
    inDrawingMachinery = NO;
}

// Use the "needs display" mechanism to do all insertion point drawing in the web view.
- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase
{
    // We only need to take control of the cases where we are being asked to draw by something
    // outside the normal display machinery, and when we are being asked to draw the insertion
    // point, not erase it.
    if (inDrawingMachinery || !drawInsteadOfErase) {
        return YES;
    }

    // NSTextView's insertion-point drawing code sets the rect width to 1.
    // So we do the same thing, to affect exactly the same rectangle.
    rect.size.width = 1;

    // Call through to the setNeedsDisplayInRect implementation in NSView.
    // If we call the one in NSTextView through the normal method dispatch
    // we will reenter the caret blinking code and end up with a nasty crash
    // (see Radar 3250608).
    SEL selector = @selector(setNeedsDisplayInRect:);
    typedef void (*IMPWithNSRect)(id, SEL, NSRect);
    IMPWithNSRect implementation = (IMPWithNSRect)[NSView instanceMethodForSelector:selector];
    implementation(view, selector, rect);

    return NO;
}

- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event
{
    if (!widget) {
	return YES;
    }

    NSEventType type = [event type];
    if ((type == NSKeyDown || type == NSKeyUp) && 
	![[NSInputManager currentInputManager] hasMarkedText]) {
        WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
        BOOL intercepted = [bridge interceptKeyEvent:event toView:view];
        // Always return NO for key up events because we don't want them
        // passed along the responder chain. This is arguably a bug in
        // NSTextView; see Radar 3507083.
        return type != NSKeyUp && !intercepted;
    }
    return YES;
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    if (!widget) {
	return;
    }
    if ([event type] == NSLeftMouseUp) {
        widget->sendConsumedMouseUp();
        widget->clicked();
    }
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    KWQTextFieldCell *cell = [self cell];
    if ([cell baseWritingDirection] != direction) {
        [cell setBaseWritingDirection:direction];
        [[secureField cell] setBaseWritingDirection:direction];

        // One call to setNeedsDisplay will take care of both text fields.
        [self setNeedsDisplay:YES];
    }
}

@end

@implementation KWQTextField (KWQInternal)

- (NSText *)currentEditorForEitherField
{
    NSResponder *firstResponder = [[self window] firstResponder];
    if ([firstResponder isKindOfClass:[NSText class]]) {
        NSText *editor = (NSText *)firstResponder;
        id delegate = [editor delegate];
        if (delegate == self || delegate == secureField) {
            return editor;
        }
    }
    return nil;
}

- (NSRange)selectedRange
{
    NSText *editor = [self currentEditorForEitherField];
    return editor ? [editor selectedRange] : NSMakeRange(NSNotFound, 0);
}

- (void)setSelectedRange:(NSRange)range
{
    // Range check just in case the saved range has gotten out of sync.
    // Even though we don't see this in testing, we really don't want
    // an exception in this case, so we protect ourselves.
    NSText *editor = [self currentEditorForEitherField];    
    if (NSMaxRange(range) <= [[editor string] length]) {
        [editor setSelectedRange:range];
    }
}

- (void)setHasFocus:(BOOL)nowHasFocus
{
    if (!widget) {
	return;
    }

    if (nowHasFocus == hasFocus) {
        return;
    }

    hasFocus = nowHasFocus;

    if (nowHasFocus) {
        // Select all the text if we are tabbing in, but otherwise preserve/remember
        // the selection from last time we had focus (to match WinIE).
        if ([[self window] keyViewSelectionDirection] != NSDirectSelection) {
            lastSelectedRange.location = NSNotFound;
        }

        if (lastSelectedRange.location != NSNotFound) {
            [self setSelectedRange:lastSelectedRange];
        }
        
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(widget)) {
            [self _KWQ_scrollFrameToVisible];
        }

        QFocusEvent event(QEvent::FocusIn);
        const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);

	// Sending the onFocus event above, may have resulted in a blur() - if this
	// happens when tabbing from another text field, then endEditing: and
	// controlTextDidEndEditing: will never be called. The bad side effects of this 
	// include the fact that our idea of the focus state will be wrong;
	// and the text field will think it's still editing, so it will continue to draw
	// the focus ring. So we call endEditing: manually if we detect this inconsistency,
	// and the correct our internal impression of the focus state.

	if ([self currentEditorForEitherField] == nil && [self currentEditor] != nil) {
	    [[self cell] endEditing:[self currentEditor]];
	    [self setHasFocus:NO];
	}
    } else {
        lastSelectedRange = [self selectedRange];

        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
    }
}

@end

@implementation KWQTextFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[KWQTextField class]]);
    [(KWQTextField *)delegate setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[KWQTextField class]]);
    [(KWQTextField *)delegate setHasFocus:YES];
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    baseWritingDirection = direction;
}

- (NSWritingDirection)baseWritingDirection
{
    return baseWritingDirection;
}

- (NSMutableDictionary *)_textAttributes
{
    NSMutableDictionary *attributes = [super _textAttributes];
    NSParagraphStyle *style = [attributes objectForKey:NSParagraphStyleAttributeName];
    ASSERT(style != nil);
    if ([style baseWritingDirection] != baseWritingDirection) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:baseWritingDirection];
        [attributes setObject:mutableStyle forKey:NSParagraphStyleAttributeName];
        [mutableStyle release];
    }
    return attributes;
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
    if ([partialString _KWQ_numComposedCharacterSequences] > maxLength) {
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

-(id)initWithQLineEdit:(QLineEdit *)w 
{
    widget = w;
    return [self init];
}

// Can't use setCellClass: because NSSecureTextField won't let us (for no good reason).
+ (Class)cellClass
{
    return [KWQSecureTextFieldCell class];
}

- (NSView *)nextKeyView
{
    return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget([self widget], KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
   return inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget([self widget], KWQSelectingPrevious)
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

// The currentEditor method does not work for secure text fields.
// This works around that limitation.
- (NSText *)currentEditorForSecureField
{
    NSResponder *firstResponder = [[self window] firstResponder];
    if ([firstResponder isKindOfClass:[NSText class]]) {
        NSText *editor = (NSText *)firstResponder;
        if ([editor delegate] == self) {
            return editor;
        }
    }
    return nil;
}

// These next two methods are the workaround for bug 3024443.
// Basically, setFrameSize ends up calling an inappropriate selectText, so we just ignore
// calls to selectText while setFrameSize is running.

- (void)selectText:(id)sender
{
    if (sender == self && inSetFrameSize) {
        return;
    }

    // Don't call the NSSecureTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events and wreak havoc in other ways as well by setting the focus
    // back to the window.
    NSText *editor = [self currentEditorForSecureField];
    if (editor) {
        [editor setSelectedRange:NSMakeRange(0, [[editor string] length])];
        return;
    }

    [super selectText:sender];
}

- (void)setFrameSize:(NSSize)size
{
    inSetFrameSize = YES;
    [super setFrameSize:size];
    inSetFrameSize = NO;
}

- (void)display
{
    // This is a workaround for Radar 2753974.
    // Also, in the web page context, it's never OK to just display.
    [self setNeedsDisplay:YES];
}

- (QWidget *)widget
{
    ASSERT([[self delegate] isKindOfClass:[KWQTextField class]]);
    return [(KWQTextField *)[self delegate] widget];
}

- (void)textDidEndEditing:(NSNotification *)notification
{
    [super textDidEndEditing:notification];

    // When tabbing from one secure text field to another, the super
    // call above will change the focus, and then turn off bullet mode
    // for the secure field, leaving the plain text showing. As a
    // workaround for this AppKit bug, we detect this circumstance
    // (changing from one secure field to another) and set selectable
    // to YES, and then back to whatever it was - this has the side
    // effect of turning on bullet mode.

    NSTextView *textObject = [notification object];
    id delegate = [textObject delegate];
    if (delegate != self && [delegate isKindOfClass:[NSSecureTextField class]]) {
	BOOL oldSelectable = [textObject isSelectable];
	[textObject setSelectable:YES];
	[textObject setSelectable:oldSelectable];
    }
}

- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event
{
    if (!widget) {
	return YES;
    }

    if (([event type] == NSKeyDown || [event type] == NSKeyUp) &&
	![[NSInputManager currentInputManager] hasMarkedText]) {
        WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
        return ![bridge interceptKeyEvent:event toView:view];
    }
    return YES;
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    if (!widget) {
	return;
    }
    if ([event type] == NSLeftMouseUp) {
        widget->sendConsumedMouseUp();
        widget->clicked();
    }
}

@end

@implementation KWQSecureTextFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([[delegate delegate] isKindOfClass:[KWQTextField class]]);
    [(KWQTextField *)[delegate delegate] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([[delegate delegate] isKindOfClass:[KWQTextField class]]);
    [(KWQTextField *)[delegate delegate] setHasFocus:YES];
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    baseWritingDirection = direction;
}

- (NSMutableDictionary *)_textAttributes
{
    NSMutableDictionary *attributes = [super _textAttributes];
    NSParagraphStyle *style = [attributes objectForKey:NSParagraphStyleAttributeName];
    ASSERT(style != nil);
    if ([style baseWritingDirection] != baseWritingDirection) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:baseWritingDirection];
        [attributes setObject:mutableStyle forKey:NSParagraphStyleAttributeName];
        [mutableStyle release];
    }
    return attributes;
}

@end

@implementation NSString (KWQTextField)

- (int)_KWQ_numComposedCharacterSequences
{
    unsigned i = 0;
    unsigned l = [self length];
    int num = 0;
    while (i < l) {
        i = NSMaxRange([self rangeOfComposedCharacterSequenceAtIndex:i]);
        ++num;
    }
    return num;
}

- (NSString *)_KWQ_truncateToNumComposedCharacterSequences:(int)num
{
    unsigned i = 0;
    unsigned l = [self length];
    if (l == 0) {
        return self;
    }
    for (int j = 0; j < num; j++) {
        i = NSMaxRange([self rangeOfComposedCharacterSequenceAtIndex:i]);
        if (i >= l) {
            return self;
        }
    }
    return [self substringToIndex:i];
}

@end
