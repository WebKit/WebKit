/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#import <qwidget.h>

#import <KWQTextArea.h>
#import <IFNSStringExtensions.h>

/*
    This widget is used to implement the <TEXTAREA> element.
    
    It has a small set of features required by the definition of the <TEXTAREA>
    element.  
    
    It supports the three wierd <TEXTAREA> WRAP attributes:
    
              OFF - Text is not wrapped.  It is kept to single line, although if
                    the user enters a return the line IS broken.  This emulates
                    Mac IE 5.1.
     SOFT|VIRUTAL - Text is wrapped, but not actually broken.  
    HARD|PHYSICAL - Text is wrapped, and text is broken into seperate lines.
                         
    kde expects a line based widget.  It uses a line API to convert text into multiple lines
    when wrapping is set to HARD.  To support kde with implement [textLine] and [numLines].
    
    If the wrap mode HARD kde will repeatedly call textForLine: (via emulated API in KWQTextEdit)
    to construct the text with inserted \n.
*/


@implementation KWQTextArea

const float LargeNumberForText = 1.0e7;

- initWithFrame: (NSRect)r
{
    return [self initWithFrame: r widget: 0];
}

- (void)_createTextView
{
    NSDictionary *attr;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSRect frame, textFrame;

    frame = [self frame];
    
    textFrame.origin.x = textFrame.origin.y = 0;
    if (frame.size.width > 0 && frame.size.height > 0)
        textFrame.size = [NSScrollView contentSizeForFrameSize:frame.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType: [self borderType]];
    else {
        textFrame.size.width = LargeNumberForText;
        textFrame.size.height = LargeNumberForText;
    }
        
    textView = [[NSTextView alloc] initWithFrame: textFrame];
    [[textView textContainer] setWidthTracksTextView: YES];
    
    // Setup attributes for default cases WRAP=SOFT|VIRTUAL and WRAP=HARD|PHYSICAL.
    // If WRAP=OFF we reset many of these attributes.
    [style setLineBreakMode: NSLineBreakByWordWrapping];
    [style setAlignment: NSLeftTextAlignment];
    attr = [NSDictionary dictionaryWithObjectsAndKeys: style, NSParagraphStyleAttributeName, nil];
    [textView setTypingAttributes: attr];
    [textView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];

    [textView setDelegate: self];
    
    [self setDocumentView: textView];
}

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];

    [self setHasVerticalScroller: YES];
    [self setHasHorizontalScroller: NO];
    [self setBorderType: NSLineBorder];

    //if (r.size.width > 0 && r.size.height > 0)
        [self _createTextView];
    
    widget = w;
    
    return self;
}

- (void)textDidEndEditing:(NSNotification *)aNotification
{
    if (widget)
        widget->emitAction(QObject::ACTION_TEXT_AREA_END_EDITING);
}


- (void) setWordWrap: (BOOL)f
{
    if (f == wrap)
        return;
        
    // This widget may have issues toggling back and forth between WRAP=YES and WRAP=NO.
    NSDictionary *attr;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    
    if (f){
        [self setHasHorizontalScroller: NO];
        [textView setHorizontallyResizable: NO];
        [[textView textContainer] setWidthTracksTextView: NO];
        [style setLineBreakMode: NSLineBreakByWordWrapping];
    }
    else {
        [self setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [[self contentView] setAutoresizesSubviews:YES];
        [self setHasHorizontalScroller: YES];

        [[textView textContainer] setContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        [[textView textContainer] setWidthTracksTextView:NO];
        [[textView textContainer] setHeightTracksTextView:NO];

        [textView setMinSize: [textView frame].size];
        [textView setMaxSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        [textView setHorizontallyResizable: YES];

        [style setLineBreakMode: NSLineBreakByClipping];
    }
    
    [style setAlignment: NSLeftTextAlignment];
    attr = [NSDictionary dictionaryWithObjectsAndKeys: style, NSParagraphStyleAttributeName, nil];
    
    [textView setMaxSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
    [textView setTypingAttributes: attr];
    
    wrap = f;
}


- (BOOL) wordWrap
{
    return wrap;
}


- (BOOL) isReadOnly
{
    return [textView isEditable] ? NO : YES;
}


- (void) setReadOnly: (BOOL)flag
{
    return [textView setEditable: flag?NO:YES];
}

- (void) setText: (NSString *)s
{
    [textView setString: s];
}


- (NSString *)text
{
    return [textView string];
}


- (NSString *)textForLine: (int)line
{
    NSRange glyphRange = NSMakeRange(0,0), characterRange;
    int lineCount = 0;
    NSString *stringLine;
    NSLayoutManager *layoutManager = [textView layoutManager];
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    
    while (NSMaxRange(glyphRange) < numberOfGlyphs) {
        (void)[layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        characterRange = [layoutManager characterRangeForGlyphRange:glyphRange actualGlyphRange:NULL];
        if (line == lineCount){
            // I hope this works, the alternative is
            // [[view string] substringWithRange: characterRange]
            stringLine = [[[textView textStorage] attributedSubstringFromRange: characterRange] string];
            return stringLine;
        }
        lineCount++;
    }
    return @"";
}


- (int) numLines
{
    NSRange glyphRange = NSMakeRange(0,0);
    NSLayoutManager *layoutManager = [textView layoutManager];
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    int lineCount = 0;
    
    while (NSMaxRange(glyphRange) < numberOfGlyphs) {
        (void)[layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        lineCount++;
    }
    return lineCount;
}


- (void) selectAll
{
    [textView setSelectedRange: NSMakeRange(0, [[textView textStorage] length])];
}


- (void) setEditable: (BOOL)flag
{
    [textView setEditable: flag];
}


- (BOOL)isEditable
{
    return [textView isEditable];
}

- (void)setFrame:(NSRect)frameRect
{    
    NSRect textFrame;
    NSSize contentSize;
    
    [super setFrame:frameRect];

    if ([self wordWrap]){
        contentSize = [NSScrollView contentSizeForFrameSize:frameRect.size hasHorizontalScroller:[self hasHorizontalScroller] hasVerticalScroller:[self hasVerticalScroller] borderType:[self borderType]];
        textFrame = [textView frame];
        textFrame.size.width = contentSize.width;
        contentSize.height = LargeNumberForText;
        [textView setFrame: textFrame];
        [[textView textContainer] setContainerSize: contentSize];
    }
    //if (frameRect.size.width > 0 && frameRect.size.height > 0 && textView == nil)
    //    [self _createTextView];
}

- (int)paragraphs
{
    return [[textView string] _IF_countOfString:@"\n"] + 1;
}

static NSRange RangeOfParagraph(NSString *text, int paragraph)
{
    int paragraphSoFar = 0;
    NSRange searchRange = NSMakeRange(0, [text length]);
    NSRange newlineRange;
    int advance;

    while (true) {
	newlineRange = [text rangeOfString:@"\n" options:NSLiteralSearch range:searchRange];
	if (newlineRange.location == NSNotFound) {
	    break;
	}

	if (paragraphSoFar == paragraph) {
	    break;
	}

	paragraphSoFar++;

        advance = newlineRange.location + 1 - searchRange.location;
	if ((int)searchRange.length <= advance) {
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
        (void)[layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphRange) effectiveRange:&glyphRange];
        lineCount++;
    }

    return lineCount;
}

- (void)getCursorPositionAsIndex:(int *)index inParagraph:(int *)paragraph
{
    // FIXME: is this right? Cocoa text view docs are impenetrable
    NSString *text = [textView string];
    NSRange selectedRange = [textView selectedRange];
    
    if (selectedRange.location == NSNotFound) {
	*index = 0;
	*paragraph = 0;
    } else {
	int num = [self paragraphs];
	int i;
	NSRange range;
	
	// this loop will
	for (i = 0; i < num; i++) {
	    range = RangeOfParagraph(text, i);
	    if (range.location + range.length > selectedRange.location) {
		break;
	    }
	}

	*paragraph = num;
	*index = selectedRange.location - range.location;
    }
}

- (void)setCursorPositionToIndex:(int)index inParagraph:(int)paragraph
{
    // FIXME: is this right? Cocoa text view docs are impenetrable
    NSString *text = [textView string];
    NSRange range = RangeOfParagraph(text, paragraph);

    if (range.location == NSNotFound) {
	[textView setMarkedText:@"" selectedRange:NSMakeRange([text length], 0)];
    } else {
	[textView setMarkedText:@"" selectedRange:NSMakeRange(range.location + index, 0)];
    }
}

@end



