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

#import "KWQTextArea.h"

#import "KWQTextEdit.h"
#import "KWQKHTMLPart.h"

/*
    This widget is used to implement the <TEXTAREA> element.
    
    It has a small set of features required by the definition of the <TEXTAREA>
    element.  
    
    It supports the three wierd <TEXTAREA> WRAP attributes:
    
              OFF - Text is not wrapped.  It is kept to single line, although if
                    the user enters a return the line IS broken.  This emulates
                    Mac IE 5.1.
     SOFT|VIRTUAL - Text is wrapped, but not actually broken.  
    HARD|PHYSICAL - Text is wrapped, and text is broken into seperate lines.
                         
    KDE expects a line based widget.  It uses a line API to convert text into multiple lines
    when wrapping is set to HARD.  To support KDE we implement [textLine] and [numLines].
    
    If the wrap mode HARD KDE will repeatedly call textForLine: (via emulated API in KWQTextEdit)
    to construct the text with inserted \n.
*/

@interface NSView (KWQTextArea)
- (void)_KWQ_setKeyboardFocusRingNeedsDisplay;
@end

@interface KWQTextAreaTextView : NSTextView <KWQWidgetHolder>
{
    QTextEdit *widget;
}
- (void)setWidget:(QTextEdit *)widget;
@end

@implementation KWQTextArea

const float LargeNumberForText = 1.0e7;

- (void)_createTextView
{
    NSDictionary *attr;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSRect frame, textFrame;

    frame = [self frame];
    
    textFrame.origin.x = textFrame.origin.y = 0;
    if (frame.size.width > 0 && frame.size.height > 0)
        textFrame.size = [NSScrollView contentSizeForFrameSize:frame.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType:[self borderType]];
    else {
        textFrame.size.width = LargeNumberForText;
        textFrame.size.height = LargeNumberForText;
    }
        
    textView = [[KWQTextAreaTextView alloc] initWithFrame:textFrame];
    [[textView textContainer] setWidthTracksTextView:YES];
    [textView setRichText:NO];

    // Setup attributes for default cases WRAP=SOFT|VIRTUAL and WRAP=HARD|PHYSICAL.
    // If WRAP=OFF we reset many of these attributes.
    [style setLineBreakMode:NSLineBreakByWordWrapping];
    [style setAlignment:NSLeftTextAlignment];
    attr = [NSDictionary dictionaryWithObject:style forKey:NSParagraphStyleAttributeName];
    [textView setTypingAttributes:attr];
    [textView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    [textView setDelegate:self];
    
    [self setDocumentView:textView];
}

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
    [self setHasVerticalScroller:YES];
    [self setHasHorizontalScroller:NO];
    [self setBorderType:NSBezelBorder];
    
    [self _createTextView];
    
    return self;
}

- initWithQTextEdit:(QTextEdit *)w 
{
    [super init];

    widget = w;
    [textView setWidget:widget];

    return self;
}

- (void)dealloc
{    
    [textView release];
    
    [super dealloc];
}

- (void)textDidChange:(NSNotification *)aNotification
{
    widget->textChanged();
}

- (void)setWordWrap:(BOOL)f
{
    if (f == wrap) {
        return;
    }
        
    // This widget may have issues toggling back and forth between WRAP=YES and WRAP=NO.
    NSDictionary *attr;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    
    if (f) {
        [self setHasHorizontalScroller:NO];
        [textView setHorizontallyResizable:NO];
        [[textView textContainer] setWidthTracksTextView:NO];
        [style setLineBreakMode:NSLineBreakByWordWrapping];
    } else {
        [self setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [[self contentView] setAutoresizesSubviews:YES];
        [self setHasHorizontalScroller:YES];

        [[textView textContainer] setContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        [[textView textContainer] setWidthTracksTextView:NO];
        [[textView textContainer] setHeightTracksTextView:NO];

        [textView setMinSize:[textView frame].size];
        [textView setMaxSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        [textView setHorizontallyResizable:YES];

        [style setLineBreakMode:NSLineBreakByClipping];
    }
    
    [style setAlignment:NSLeftTextAlignment];
    attr = [NSDictionary dictionaryWithObject:style forKey:NSParagraphStyleAttributeName];
    
    [textView setMaxSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
    [textView setTypingAttributes:attr];
    
    wrap = f;
}

- (BOOL)wordWrap
{
    return wrap;
}

- (void)setText:(NSString *)s
{
    //NSLog(@"extraLineFragmentTextContainer before setString: is %@", [[textView layoutManager] extraLineFragmentTextContainer]);
    [textView setString:s];
    //NSLog(@"extraLineFragmentTextContainer after setString: is %@", [[textView layoutManager] extraLineFragmentTextContainer]);
}

- (NSString *)text
{
    return [textView string];
}

- (NSString *)textForLine:(int)line
{
    NSRange glyphRange = NSMakeRange(0,0), characterRange;
    int lineCount = 0;
    NSLayoutManager *layoutManager = [textView layoutManager];
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    
    while (NSMaxRange(glyphRange) < numberOfGlyphs) {
        [layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        characterRange = [layoutManager characterRangeForGlyphRange:glyphRange actualGlyphRange:NULL];
        if (line == lineCount) {
            return [[[textView textStorage] attributedSubstringFromRange:characterRange] string];
        }
        lineCount++;
    }
    return @"";
}

- (int)numLines
{
    NSRange glyphRange = NSMakeRange(0,0);
    NSLayoutManager *layoutManager = [textView layoutManager];
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    int lineCount = 0;
    
    while (NSMaxRange(glyphRange) < numberOfGlyphs) {
        [layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        lineCount++;
    }
    return lineCount;
}

- (void)selectAll
{
    [textView selectAll:nil];
}

- (void)setEditable:(BOOL)flag
{
    [textView setEditable:flag];
}

- (BOOL)isEditable
{
    return [textView isEditable];
}

- (void)setFrame:(NSRect)frameRect
{    
    [super setFrame:frameRect];

    if ([self wordWrap]) {
        NSSize contentSize = [NSScrollView contentSizeForFrameSize:frameRect.size hasHorizontalScroller:[self hasHorizontalScroller] hasVerticalScroller:[self hasVerticalScroller] borderType:[self borderType]];
        NSRect textFrame = [textView frame];
        textFrame.size.width = contentSize.width;
        contentSize.height = LargeNumberForText;
        [textView setFrame:textFrame];
        [[textView textContainer] setContainerSize:contentSize];
    }
}

- (int)paragraphs
{
    NSString *text = [textView string];
    int paragraphSoFar = 0;
    NSRange searchRange = NSMakeRange(0, [text length]);

    while (true) {
	NSRange newlineRange = [text rangeOfString:@"\n" options:NSLiteralSearch range:searchRange];
	if (newlineRange.location == NSNotFound) {
	    break;
	}

	paragraphSoFar++;

        unsigned advance = newlineRange.location + 1 - searchRange.location;
        
	searchRange.length -= advance;
	searchRange.location += advance;
    }
    
    return paragraphSoFar + (searchRange.length == 0 ? 0 : 1);
}

static NSRange RangeOfParagraph(NSString *text, int paragraph)
{
    int paragraphSoFar = 0;
    NSRange searchRange = NSMakeRange(0, [text length]);

    NSRange newlineRange;
    while (true) {
	newlineRange = [text rangeOfString:@"\n" options:NSLiteralSearch range:searchRange];
	if (newlineRange.location == NSNotFound) {
	    break;
	}

	if (paragraphSoFar == paragraph) {
	    break;
	}

	paragraphSoFar++;

        unsigned advance = newlineRange.location + 1 - searchRange.location;
	if (searchRange.length <= advance) {
	    searchRange.location = NSNotFound;
	    searchRange.length = 0;
	    break;
	}
        
	searchRange.length -= advance;
	searchRange.location += advance;
    }

    if (paragraphSoFar < paragraph) {
	return NSMakeRange(NSNotFound, 0);
    } else if (searchRange.location == NSNotFound || newlineRange.location == NSNotFound) {
	return searchRange;
    } else {
	return NSMakeRange(searchRange.location, newlineRange.location - searchRange.location);
    }
}

- (int)paragraphLength:(int)paragraph
{
    NSString *text = [textView string];
    NSRange range = RangeOfParagraph(text, paragraph);
    if (range.location == NSNotFound) {
	return 0;
    } else {
	return range.length;
    }
}

- (NSString *)textForParagraph:(int)paragraph
{
    NSString *text = [textView string];
    NSRange range = RangeOfParagraph(text, paragraph);
    if (range.location == NSNotFound) {
	return [NSString string];
    } else {
	return [text substringWithRange:range];
    }
}

- (int)lineOfCharAtIndex:(int)index inParagraph:(int)paragraph
{
    NSString *text = [textView string];
    NSRange range = RangeOfParagraph(text, paragraph);
    NSLayoutManager *layoutManager = [textView layoutManager];

    if (range.location == NSNotFound) {
	return -1;
    }

    NSRange characterMaxRange = NSMakeRange(0, range.location);
    NSRange glyphMaxRange = [layoutManager glyphRangeForCharacterRange:characterMaxRange actualCharacterRange:NULL];

    // FIXME: factor line counting code out into something shared
    unsigned numberOfGlyphs = glyphMaxRange.location + glyphMaxRange.length;
    NSRange glyphRange = NSMakeRange(0,0);
    int lineCount = 0;

    while (NSMaxRange(glyphRange) < numberOfGlyphs) {
        [layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        lineCount++;
    }

    return lineCount;
}

- (void)getCursorPositionAsIndex:(int *)index inParagraph:(int *)paragraph
{
    NSString *text = [textView string];
    NSRange selectedRange = [textView selectedRange];
    
    if (selectedRange.location == NSNotFound) {
        *paragraph = 0;
        *index = 0;
        return;
    }
    
    int paragraphSoFar = 0;
    NSRange searchRange = NSMakeRange(0, [text length]);

    while (true) {
	NSRange newlineRange = [text rangeOfString:@"\n" options:NSLiteralSearch range:searchRange];
	if (newlineRange.location == NSNotFound || selectedRange.location <= newlineRange.location) {
	    break;
	}
        
	paragraphSoFar++;

        unsigned advance = newlineRange.location + 1 - searchRange.location;

	searchRange.length -= advance;
	searchRange.location += advance;
    }

    *paragraph = paragraphSoFar;
    *index = selectedRange.location - searchRange.location;
}

- (void)setCursorPositionToIndex:(int)index inParagraph:(int)paragraph
{
    NSString *text = [textView string];
    NSRange range = RangeOfParagraph(text, paragraph);
    if (range.location == NSNotFound) {
	[textView setSelectedRange:NSMakeRange([text length], 0)];
    } else {
        if (index < 0) {
            index = 0;
        } else if ((unsigned)index > range.length) {
            index = range.length;
        }
	[textView setSelectedRange:NSMakeRange(range.location + index, 0)];
    }
}

- (void)setFont:(NSFont *)font
{
    //NSLog(@"extraLineFragmentTextContainer before setFont: is %@", [[textView layoutManager] extraLineFragmentTextContainer]);
    [textView setFont:font];
    //NSLog(@"extraLineFragmentTextContainer after setFont: is %@", [[textView layoutManager] extraLineFragmentTextContainer]);
}

- (BOOL)becomeFirstResponder
{
    [[self window] makeFirstResponder:textView];
    return YES;
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

- (BOOL)needsPanelToBecomeKey
{
    // If we don't return YES here, tabbing backwards into this view doesn't work.
    return YES;
}

- (void)drawRect:(NSRect)rect
{
    [super drawRect:rect];
    if ([[self window] firstResponder] == textView) {
        NSSetFocusRingStyle(NSFocusRingOnly);
        NSRectFill([self bounds]);
    }
}

- (void)_KWQ_setKeyboardFocusRingNeedsDisplay
{
    [self setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
}

- (QWidget *)widget
{
    return widget;
}

@end

@implementation KWQTextAreaTextView

- (void)setWidget:(QTextEdit *)w
{
    widget = w;
}

- (void)insertTab:(id)sender
{
    NSView *view = [[self delegate] nextValidKeyView];
    if (view && view != self && view != [self delegate]) {
        [[self window] makeFirstResponder:view];
    }
}

- (void)insertBacktab:(id)sender
{
    NSView *view = [[self delegate] previousValidKeyView];
    if (view && view != self && view != [self delegate]) {
        [[self window] makeFirstResponder:view];
    }
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];

    if (become) {
	[self selectAll:nil];
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];
	QFocusEvent event(QEvent::FocusIn);
	const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
    }
       
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];

    if (resign) {
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];
	QFocusEvent event(QEvent::FocusOut);
	const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
    }

    return resign;
}

- (BOOL)shouldDrawInsertionPoint
{
    return self == [[self window] firstResponder] && [super shouldDrawInsertionPoint];
}

- (NSDictionary *)selectedTextAttributes
{
    return self == [[self window] firstResponder] ? [super selectedTextAttributes] : nil;
}

- (void)scrollPageUp:(id)sender
{
    // After hitting the top, tell our parent to scroll
    float oldY = [[[self enclosingScrollView] contentView] bounds].origin.y;
    [super scrollPageUp:sender];
    if (oldY == [[[self enclosingScrollView] contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageUp:) with:nil];
    }
}

- (void)scrollPageDown:(id)sender
{
    // After hitting the bottom, tell our parent to scroll
    float oldY = [[[self enclosingScrollView] contentView] bounds].origin.y;
    [super scrollPageDown:sender];
    if (oldY == [[[self enclosingScrollView] contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageDown:) with:nil];
    }
}

- (QWidget *)widget
{
    return widget;
}

- (void)mouseDown:(NSEvent *)event
{
    [super mouseDown:event];
    widget->sendConsumedMouseUp();
    widget->clicked();
}

@end

@implementation NSView (KWQTextArea)

- (void)_KWQ_setKeyboardFocusRingNeedsDisplay
{
    [[self superview] _KWQ_setKeyboardFocusRingNeedsDisplay];
}

@end
