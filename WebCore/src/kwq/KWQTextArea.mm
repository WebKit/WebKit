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

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    NSDictionary *attr;
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    NSRect textFrame;

    [super initWithFrame: r];

    [self setHasVerticalScroller: YES];
    [self setHasHorizontalScroller: NO];

    textFrame.origin.x = textFrame.origin.y = 0;
    textFrame.size = [NSScrollView contentSizeForFrameSize:r.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType: [self borderType]];

    textView = [[NSTextView alloc] initWithFrame: textFrame];
    [[textView textContainer] setWidthTracksTextView: NO];
    
    // Setup attributes for default cases WRAP=SOFT|VIRTUAL and WRAP=HARD|PHYSICAL.
    // If WRAP=OFF we reset many of these attributes.
    [style setLineBreakMode: NSLineBreakByWordWrapping];
    [style setAlignment: NSLeftTextAlignment];
    attr = [NSDictionary dictionaryWithObjectsAndKeys: style, NSParagraphStyleAttributeName, nil];
    [textView setTypingAttributes: attr];

    [self setDocumentView: textView];
    
    widget = w;
    
    return self;
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



@end

