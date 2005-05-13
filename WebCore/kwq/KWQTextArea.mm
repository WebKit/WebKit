/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "DOMHTML.h"
#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "KWQTextEdit.h"
#import "render_replaced.h"
#import "WebCoreBridge.h"

#define ALLOW_RESIZING_TEXTAREAS 1

using DOM::EventImpl;
using DOM::NodeImpl;
using khtml::RenderWidget;

/*
    This widget is used to implement the <TEXTAREA> element.
    
    It has a small set of features required by the definition of the <TEXTAREA>
    element.  
    
    It supports the three wierd <TEXTAREA> WRAP attributes:
    
              OFF - Text is not wrapped.  It is kept to single line, although if
                    the user enters a return the line IS broken.  This emulates
                    Mac IE 5.1.
     SOFT|VIRTUAL - Text is wrapped, but not actually broken.  
    HARD|PHYSICAL - Text is wrapped, and text is broken into separate lines.
*/


@interface NSView (KWQTextArea)
- (void)_KWQ_setKeyboardFocusRingNeedsDisplay;
@end

@interface NSTextView (KWQTextArea)
- (NSParagraphStyle *)_KWQ_typingParagraphStyle;
- (void)_KWQ_setTypingParagraphStyle:(NSParagraphStyle *)style;
@end

@interface NSTextStorage (KWQTextArea)
- (void)_KWQ_setBaseWritingDirection:(NSWritingDirection)direction;
@end

#if ALLOW_RESIZING_TEXTAREAS
@interface KWQTextArea (KWQTextAreaTextView)
- (void)getNumColumns:(long*)numColumns andNumRows:(long*)numRows forSize:(NSSize)frameSize;
@end
#endif

@interface KWQTextAreaTextView : NSTextView <KWQWidgetHolder>
{
    QTextEdit *widget;
#if ALLOW_RESIZING_TEXTAREAS
    NSTrackingRectTag resizeCornerTrackingRectTag;
#endif
    BOOL disabled;
    BOOL editableIfEnabled;
    BOOL inCut;
}

- (void)setWidget:(QTextEdit *)widget;

- (void)setEnabled:(BOOL)flag;
- (BOOL)isEnabled;

- (void)setEditableIfEnabled:(BOOL)flag;
- (BOOL)isEditableIfEnabled;

- (void)updateTextColor;

@end

@implementation KWQTextArea

const float LargeNumberForText = 1.0e7;

- (void)_configureTextViewForWordWrapMode
{
    [textView setHorizontallyResizable:!wrap];
    [textView setMaxSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];

    [[textView textContainer] setWidthTracksTextView:wrap];
    [[textView textContainer] setContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
}

- (void)_createTextView
{
    textView = [[KWQTextAreaTextView alloc] init];

    [textView setRichText:NO];
    [textView setAllowsUndo:YES];
    [textView setDelegate:self];

    [self setDocumentView:textView];

    [self _configureTextViewForWordWrapMode];
}

- (void)_updateTextViewWidth
{
    if (wrap) {
        [textView setFrameSize:NSMakeSize([self contentSize].width, [textView frame].size.height)];
    }
}

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    wrap = YES;

    [self setBorderType:NSBezelBorder];

    [self _createTextView];
    [self _updateTextViewWidth];

    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which clip views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // release the GState for all clip views for all views contained in 
    // a WebHTMLView. Here we do it for textareas used in forms.
    // See these bugs for more information:
    // <rdar://problem/3310943>: REGRESSION (Panther): textareas in forms sometimes draw blank (bugreporter)
    [[self contentView] releaseGState];
    [[self documentView] releaseGState];

    return self;
}

- initWithQTextEdit:(QTextEdit *)w 
{
    [self init];

    widget = w;
    [textView setWidget:widget];

    return self;
}

- (void)detachQTextEdit
{
    widget = 0;
    [textView setWidget:0];
}

- (void)dealloc
{
    [textView release];
    [_font release];
    
    [super dealloc];
}

- (void)textDidChange:(NSNotification *)notification
{
    if (widget)
        widget->textChanged();
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge textDidChange:notification];
}

- (void)setWordWrap:(BOOL)f
{
    if (f != wrap) {
        wrap = f;
        [self _configureTextViewForWordWrapMode];
    }
}

- (BOOL)wordWrap
{
    return wrap;
}

- (void)setText:(NSString *)s
{
    [textView setString:s];
    [textView updateTextColor];
}

- (NSString *)text
{
    NSString *text = [textView string];
    
    if ([text rangeOfString:@"\r" options:NSLiteralSearch].location != NSNotFound) {
        NSMutableString *mutableText = [[text mutableCopy] autorelease];
    
        [mutableText replaceOccurrencesOfString:@"\r\n" withString:@"\n"
            options:NSLiteralSearch range:NSMakeRange(0, [mutableText length])];
        [mutableText replaceOccurrencesOfString:@"\r" withString:@"\n"
            options:NSLiteralSearch range:NSMakeRange(0, [mutableText length])];
        
        text = mutableText;
    }

    return text;
}

- (NSString *)textWithHardLineBreaks
{
    NSMutableString *textWithHardLineBreaks = [NSMutableString string];
    
    NSString *text = [textView string];
    NSLayoutManager *layoutManager = [textView layoutManager];
    
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    NSRange lineGlyphRange = NSMakeRange(0, 0);
    while (NSMaxRange(lineGlyphRange) < numberOfGlyphs) {
        [layoutManager lineFragmentRectForGlyphAtIndex:NSMaxRange(lineGlyphRange) effectiveRange:&lineGlyphRange];
        NSRange lineRange = [layoutManager characterRangeForGlyphRange:lineGlyphRange actualGlyphRange:NULL];
        if ([textWithHardLineBreaks length]) {
            unichar lastCharacter = [textWithHardLineBreaks characterAtIndex:[textWithHardLineBreaks length] - 1];
            if (lastCharacter != '\n' && lastCharacter != '\r') {
                [textWithHardLineBreaks appendString:@"\n"];
            }
        }
        [textWithHardLineBreaks appendString:[text substringWithRange:lineRange]];
    }

    [textWithHardLineBreaks replaceOccurrencesOfString:@"\r\n" withString:@"\n"
        options:NSLiteralSearch range:NSMakeRange(0, [textWithHardLineBreaks length])];
    [textWithHardLineBreaks replaceOccurrencesOfString:@"\r" withString:@"\n"
        options:NSLiteralSearch range:NSMakeRange(0, [textWithHardLineBreaks length])];

    return textWithHardLineBreaks;
}

- (void)selectAll
{
    [textView selectAll:nil];
}

- (void)setEditable:(BOOL)flag
{
    [textView setEditableIfEnabled:flag];
}

- (BOOL)isEditable
{
    return [textView isEditableIfEnabled];
}

- (void)setEnabled:(BOOL)flag
{
    if (flag == [textView isEnabled])
        return;
        
    [textView setEnabled:flag];
    
    [self setNeedsDisplay:YES];
}

- (BOOL)isEnabled
{
    return [textView isEnabled];
}

- (void)tile
{
    [super tile];
#if !BUILDING_ON_PANTHER
    [self _updateTextViewWidth];
#else
    // Pre-Tiger, if we change the width here we re-enter in a way that makes NSText unhappy.
    [self performSelector:@selector(_updateTextViewWidth) withObject:nil afterDelay:0];
#endif
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
        // FIXME: Doesn't work for CR-separated or CRLF-separated text.
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

static NSRange RangeOfParagraph(NSString *text, int paragraph)
{
    int paragraphSoFar = 0;
    NSRange searchRange = NSMakeRange(0, [text length]);

    NSRange newlineRange;
    while (true) {
        // FIXME: Doesn't work for CR-separated or CRLF-separated text.
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
    [font retain];
    [_font release];
    _font = font;
    [textView setFont:font];
}

- (void)setTextColor:(NSColor *)color
{
    [textView setTextColor:color];
}

- (void)setBackgroundColor:(NSColor *)color
{
    [textView setBackgroundColor:color];
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    [super setDrawsBackground:drawsBackground];
    [[self contentView] setDrawsBackground:drawsBackground];
    [textView setDrawsBackground:drawsBackground];
}

- (BOOL)becomeFirstResponder
{
    if (widget)
        [KWQKHTMLPart::bridgeForWidget(widget) makeFirstResponder:textView];
    return YES;
}

- (NSView *)nextKeyView
{
    return inNextValidKeyView && widget
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
   return inNextValidKeyView && widget
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

    if (![textView isEnabled]) {
        // draw a disabled bezel border
        [[NSColor controlColor] set];
        NSFrameRect(rect);
        
        rect = NSInsetRect(rect, 1, 1);
        [[NSColor controlShadowColor] set];
        NSFrameRect(rect);
    
        rect = NSInsetRect(rect, 1, 1);
        [[NSColor textBackgroundColor] set];
        NSRectFill(rect);
    }
    else if (widget && [KWQKHTMLPart::bridgeForWidget(widget) firstResponder] == textView) {
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

- (void)setAlignment:(NSTextAlignment)alignment
{
    [textView setAlignment:alignment];
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    // Set the base writing direction for typing.
    NSParagraphStyle *style = [textView _KWQ_typingParagraphStyle];
    if ([style baseWritingDirection] != direction) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:direction];
        [textView _KWQ_setTypingParagraphStyle:mutableStyle];
        [mutableStyle release];
    }

    // Set the base writing direction for text.
    [[textView textStorage] _KWQ_setBaseWritingDirection:direction];
    [textView setNeedsDisplay:YES];
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

- (NSSize)sizeWithColumns:(int)numColumns rows:(int)numRows
{
    // Must use font from _font field rather than from the text view's font method,
    // because the text view will return a substituted font if the first character in
    // the text view requires font substitution, and we don't want the size to depend on
    // the text in the text view.
    
    NSSize textSize = NSMakeSize(ceil(numColumns * [_font widthOfString:@"0"]), numRows * [_font defaultLineHeightForFont]);
    NSSize textContainerSize = NSMakeSize(textSize.width + [[textView textContainer] lineFragmentPadding] * 2, textSize.height);
    NSSize textContainerInset = [textView textContainerInset];
    NSSize textViewSize = NSMakeSize(textContainerSize.width + textContainerInset.width, textContainerSize.height + textContainerInset.height); 
    return [[self class] frameSizeForContentSize:textViewSize
        hasHorizontalScroller:[self hasHorizontalScroller]
        hasVerticalScroller:[self hasVerticalScroller]
        borderType:[self borderType]];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if ([self window] != window) {
        [[textView undoManager] removeAllActionsWithTarget:[textView textStorage]];
    }
    [super viewWillMoveToWindow:window];
}

@end

#if ALLOW_RESIZING_TEXTAREAS
@implementation KWQTextArea (KWQTextAreaTextView)

- (void)getNumColumns:(long*)numColumns andNumRows:(long*)numRows forSize:(NSSize)frameSize
{
    ASSERT_ARG(numColumns, numColumns != nil);
    ASSERT_ARG(numRows, numRows != nil);
    NSSize textViewSize = [[self class] contentSizeForFrameSize:frameSize
                                          hasHorizontalScroller:[self hasHorizontalScroller]
                                            hasVerticalScroller:[self hasVerticalScroller]
                                                     borderType:[self borderType]];
    NSSize textContainerInset = [textView textContainerInset];
    NSSize textContainerSize = NSMakeSize(textViewSize.width - textContainerInset.width, textViewSize.height - textContainerInset.width);
    NSSize textSize = NSMakeSize(textContainerSize.width - [[textView textContainer] lineFragmentPadding] * 2, textContainerSize.height);
    *numColumns = (long)(textSize.width/[_font widthOfString:@"0"]);
    *numRows = (long)(textSize.height/[_font defaultLineHeightForFont]);
}

@end
#endif

@implementation KWQTextAreaTextView

static BOOL _spellCheckingInitiallyEnabled = NO;
static NSString *WebContinuousSpellCheckingEnabled = @"WebContinuousSpellCheckingEnabled";

#if ALLOW_RESIZING_TEXTAREAS
const long MinimumColsWhileResizing = 10;
const long MinimumRowsWhileResizing = 2;
const float ResizeCornerWidth = 16;
const float ResizeCornerHeight = 16;
#endif

+ (void)_setContinuousSpellCheckingEnabledForNewTextAreas:(BOOL)flag
{
    _spellCheckingInitiallyEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebContinuousSpellCheckingEnabled];
}

+ (BOOL)_isContinuousSpellCheckingEnabledForNewTextAreas
{
    static BOOL _checkedUserDefault = NO;
    if (!_checkedUserDefault) {
        _spellCheckingInitiallyEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebContinuousSpellCheckingEnabled];
        _checkedUserDefault = YES;
    }

    return _spellCheckingInitiallyEnabled;
}

- (id)initWithFrame:(NSRect)frame textContainer:(NSTextContainer *)aTextContainer
{
    self = [super initWithFrame:frame textContainer:aTextContainer];
    [super setContinuousSpellCheckingEnabled:
        [[self class] _isContinuousSpellCheckingEnabledForNewTextAreas]];

    editableIfEnabled = YES;

    return self;
}

- (void)setContinuousSpellCheckingEnabled:(BOOL)flag
{
    [[self class] _setContinuousSpellCheckingEnabledForNewTextAreas:flag];
    [super setContinuousSpellCheckingEnabled:flag];
}


- (void)setWidget:(QTextEdit *)w
{
    widget = w;
}

- (void)insertTab:(id)sender
{
    NSView *view = [[self delegate] nextValidKeyView];
    if (view && view != self && view != [self delegate] && widget) {
        [KWQKHTMLPart::bridgeForWidget(widget) makeFirstResponder:view];
    }
}

- (void)insertBacktab:(id)sender
{
    NSView *view = [[self delegate] previousValidKeyView];
    if (view && view != self && view != [self delegate] && widget) {
        [KWQKHTMLPart::bridgeForWidget(widget) makeFirstResponder:view];
    }
}

- (BOOL)becomeFirstResponder
{
    if (disabled)
        return NO;

    BOOL become = [super becomeFirstResponder];
    
    if (become) {
        // Select all the text if we are tabbing in, but otherwise preserve/remember
        // the selection from last time we had focus (to match WinIE).
        if ([[self window] keyViewSelectionDirection] != NSDirectSelection) {
            [self selectAll:nil];
        }
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(widget)) {
            [[self enclosingScrollView] _KWQ_scrollFrameToVisible];
        }
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];

        if (widget) {
            QFocusEvent event(QEvent::FocusIn);
            const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
        }
    }

    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];

    if (resign) {
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];

        if (widget) {
            QFocusEvent event(QEvent::FocusOut);
            const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
        }
    }

    return resign;
}

- (BOOL)shouldDrawInsertionPoint
{
    return widget && self == [KWQKHTMLPart::bridgeForWidget(widget) firstResponder] && [super shouldDrawInsertionPoint];
}

- (NSDictionary *)selectedTextAttributes
{
    if (widget && self != [KWQKHTMLPart::bridgeForWidget(widget) firstResponder])
        return nil;
    return [super selectedTextAttributes];
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

#if ALLOW_RESIZING_TEXTAREAS
- (void)_trackResizeFromMouseDown:(NSEvent *)event
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    DOMHTMLTextAreaElement *element = [bridge elementForView:self];
    ASSERT([element isKindOfClass:[DOMHTMLTextAreaElement class]]);
    
    long minCols = kMin([element cols], MinimumColsWhileResizing);
    long minRows = kMin([element rows], MinimumRowsWhileResizing);
    
    KWQTextArea *textArea = [[self superview] superview];
    ASSERT([textArea isKindOfClass:[KWQTextArea class]]);
    
    NSPoint initialLocalPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    NSSize initialTextAreaSize = [textArea frame].size;

    for (;;) {
        if ([event type] == NSRightMouseDown || [event type] == NSRightMouseUp) {
            // Ignore right mouse button events and remove them from the queue.
        } else {
            NSPoint localPoint = [self convertPoint:[event locationInWindow] fromView:nil];
            if ([event type] == NSLeftMouseUp) {
                break;
            }

            // FIXME Radar 4118564: ideally we'd autoscroll the window as necessary to keep the point under
            // the cursor in view.
            NSSize newTextAreaSize = NSMakeSize(initialTextAreaSize.width + (localPoint.x - initialLocalPoint.x), 
                                                initialTextAreaSize.height + (localPoint.y - initialLocalPoint.y));
            long newCols, newRows;
            [textArea getNumColumns:&newCols andNumRows:&newRows forSize:newTextAreaSize];
            [element setCols:kMax(newCols, minCols)];
            [element setRows:kMax(newRows, minRows)];
            [bridge part]->forceLayout();
        }
        
        // Go get the next event.
        event = [[self window] nextEventMatchingMask:
            NSLeftMouseUpMask | NSLeftMouseDraggedMask | NSRightMouseDownMask | NSRightMouseUpMask];
    }    
}

- (NSImage *)_resizeCornerImage
{
    NSImage *cornerImage = nil;
    if (cornerImage == nil) {
	cornerImage = [[NSImage alloc] initWithContentsOfFile:
            [[NSBundle bundleForClass:[KWQTextAreaTextView class]]
            pathForResource:@"textAreaResizeCorner" ofType:@"tiff"]];
    }
    ASSERT(cornerImage != nil);
    return cornerImage;
}

- (NSRect)_resizeCornerRect
{
    NSClipView *clipView = [self superview];
    NSRect visibleRect = [clipView documentVisibleRect];
    NSImage *cornerImage = [self _resizeCornerImage];
    NSSize imageSize = [cornerImage size];
    // Add one pixel of whitespace at right and bottom of image to match normal resize corner appearance.
    // This could be built into the image, alternatively.
    return NSMakeRect(NSMaxX(visibleRect) - imageSize.width - 1, NSMaxY(visibleRect) - imageSize.height - 1, imageSize.width + 1, imageSize.height + 1);
}

- (void)resetCursorRects {
    [super resetCursorRects];
    
    // FIXME Radar 4118575: This is intended to change the cursor to the arrow cursor whenever it is
    // over the resize corner. However, it currently only works when the cursor had
    // been inside the textarea, presumably due to interactions with the way NSTextView
    // sets the cursor via [NSClipView setDocumentCursor:]. Also, it stops working once
    // the textview has been resized, for reasons not yet understood.
    NSRect visibleRect = [self visibleRect];
    if (resizeCornerTrackingRectTag != -1) {
        [self removeTrackingRect:resizeCornerTrackingRectTag];
        resizeCornerTrackingRectTag = -1;
    }
    if ([self isEnabled] && !NSIsEmptyRect(visibleRect)) {
        NSRect resizeCornerRect = [self _resizeCornerRect];
        [self addCursorRect:resizeCornerRect cursor:[NSCursor arrowCursor]];
        resizeCornerTrackingRectTag = [self addTrackingRect:resizeCornerRect owner:self userData:NULL assumeInside:NO];
    }
}

#endif

- (void)drawRect:(NSRect)rect
{
    [super drawRect:rect];
    
#if ALLOW_RESIZING_TEXTAREAS
    if ([self isEnabled]) {
        NSImage *cornerImage = [self _resizeCornerImage];
        NSPoint imagePoint = [self _resizeCornerRect].origin;
        imagePoint.y += [cornerImage size].height;
        [cornerImage compositeToPoint:imagePoint operation:NSCompositeSourceOver];
    }
#endif
}

- (void)mouseDown:(NSEvent *)event
{
    if (disabled)
        return;
    
#if ALLOW_RESIZING_TEXTAREAS
    NSPoint localPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    // FIXME Radar 4118510: Maybe the right design is to only have a resize corner when a scroll bar is 
    // present, and put it in the bottom-right corner (below and/or to the right of  the scroll bar?).
    // FIXME Radar 4118599: With this "bottom right corner" design, we'd need to distinguish between a click in text
    // and a drag-to-resize. This code currently always does the drag-to-resize behavior.
    // FIXME Radar 4118559: This behaves very oddly for textareas that are in blocks with right-aligned text; you have
    // to drag the bottom-right corner to make the bottom-left corner move.
    BOOL inResizeCorner = NSPointInRect(localPoint, [self _resizeCornerRect]);
    if (inResizeCorner) {
        // If the cursor tracking worked perfectly, this next line wouldn't be necessary, but it would be harmless still.
        [[NSCursor arrowCursor] set];
        [self _trackResizeFromMouseDown:event];
        return;
    }
#endif
    
    [super mouseDown:event];
    if (widget) {
        widget->sendConsumedMouseUp();
    }
    if (widget) {
        widget->clicked();
    }
}

- (void)keyDown:(NSEvent *)event
{
    if (disabled || !widget) {
        return;
    }
    
    // Don't mess with text marked by an input method
    if ([[NSInputManager currentInputManager] hasMarkedText]) {
        [super keyDown:event];
        return;
    }

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    if ([bridge interceptKeyEvent:event toView:self]) {
        return;
    }
    
    // Don't let option-tab insert a character since we use it for
    // tabbing between links
    if (KWQKHTMLPart::handleKeyboardOptionTabInView(self)) {
        return;
    }
    
    [super keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    if (disabled || !widget)
        return;

    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    if (![[NSInputManager currentInputManager] hasMarkedText]) {
	[bridge interceptKeyEvent:event toView:self];
    }
    // Don't call super because NSTextView will simply pass the
    // event along the responder chain. This is arguably a bug in
    // NSTextView; see Radar 3507083.
}

- (void)setEnabled:(BOOL)flag
{
    if (disabled == !flag) {
        return;
    }

    disabled = !flag;
    if (editableIfEnabled) {
        [self setEditable:!disabled];
    }
    [self updateTextColor];
}

- (BOOL)isEnabled
{
    return !disabled;
}

- (void)setEditableIfEnabled:(BOOL)flag
{
    editableIfEnabled = flag;
    if (!disabled) {
        [self setEditable:editableIfEnabled];
    }
}

- (BOOL)isEditableIfEnabled
{
    return editableIfEnabled;
}

- (void)updateTextColor
{
    // Make the text look disabled by changing its color.
    NSColor *color = disabled ? [NSColor disabledControlTextColor] : [NSColor controlTextColor];
    [[self textStorage] setForegroundColor:color];
}

// Could get fancy and send this to QTextEdit, then RenderTextArea, but there's really no harm
// in doing this directly right here. Could refactor some day if you disagree.

// FIXME: This does not yet implement the feature of canceling the operation, or the necessary
// support to implement the clipboard operations entirely in JavaScript.
- (void)dispatchHTMLEvent:(EventImpl::EventId)eventID
{
    if (widget) {
        const RenderWidget *rw = static_cast<const RenderWidget *>(widget->eventFilterObject());
        if (rw) {
            NodeImpl *node = rw->element();
            if (node) {
                node->dispatchHTMLEvent(eventID, false, false);
            }
        }
    }
}

- (void)cut:(id)sender
{
    inCut = YES;
    [self dispatchHTMLEvent:EventImpl::BEFORECUT_EVENT];
    [super cut:sender];
    [self dispatchHTMLEvent:EventImpl::CUT_EVENT];
    inCut = NO;
}

- (void)copy:(id)sender
{
    if (!inCut)
        [self dispatchHTMLEvent:EventImpl::BEFORECOPY_EVENT];
    [super copy:sender];
    if (!inCut)
        [self dispatchHTMLEvent:EventImpl::COPY_EVENT];
}

- (void)paste:(id)sender
{
    [self dispatchHTMLEvent:EventImpl::BEFOREPASTE_EVENT];
    [super paste:sender];
    [self dispatchHTMLEvent:EventImpl::PASTE_EVENT];
}

- (void)pasteAsPlainText:(id)sender
{
    [self dispatchHTMLEvent:EventImpl::BEFOREPASTE_EVENT];
    [super pasteAsPlainText:sender];
    [self dispatchHTMLEvent:EventImpl::PASTE_EVENT];
}

- (void)pasteAsRichText:(id)sender
{
    [self dispatchHTMLEvent:EventImpl::BEFOREPASTE_EVENT];
    [super pasteAsRichText:sender];
    [self dispatchHTMLEvent:EventImpl::PASTE_EVENT];
}

@end

@implementation NSView (KWQTextArea)

- (void)_KWQ_setKeyboardFocusRingNeedsDisplay
{
    [[self superview] _KWQ_setKeyboardFocusRingNeedsDisplay];
}

@end

@implementation NSTextView (KWQTextArea)

- (NSParagraphStyle *)_KWQ_typingParagraphStyle
{
    NSParagraphStyle *style = [[self typingAttributes] objectForKey:NSParagraphStyleAttributeName];
    if (style != nil) {
        return style;
    }
    style = [self defaultParagraphStyle];
    if (style != nil) {
        return style;
    }
    return [NSParagraphStyle defaultParagraphStyle];
}

- (void)_KWQ_setTypingParagraphStyle:(NSParagraphStyle *)style
{
    NSParagraphStyle *immutableStyle = [style copy];
    NSDictionary *attributes = [self typingAttributes];
    if (attributes != nil) {
        NSMutableDictionary *mutableAttributes = [attributes mutableCopy];
        [mutableAttributes setObject:immutableStyle forKey:NSParagraphStyleAttributeName];
        attributes = mutableAttributes;
    } else {
        attributes = [[NSDictionary alloc] initWithObjectsAndKeys:immutableStyle, NSParagraphStyleAttributeName, nil];
    }
    [immutableStyle release];
    [self setTypingAttributes:attributes];
    [attributes release];
}

@end

@implementation NSTextStorage (KWQTextArea)

- (void)_KWQ_setBaseWritingDirection:(NSWritingDirection)direction
{
    unsigned end = [self length];
    NSRange range = NSMakeRange(0, end);
    if (end != 0) {
        [self beginEditing];
        for (unsigned i = 0; i < end; ) {
            NSRange effectiveRange;
            NSParagraphStyle *style = [self attribute:NSParagraphStyleAttributeName atIndex:i longestEffectiveRange:&effectiveRange inRange:range];
            if (style == nil) {
                style = [NSParagraphStyle defaultParagraphStyle];
            }
            if ([style baseWritingDirection] != direction) {
                NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
                [mutableStyle setBaseWritingDirection:direction];
                [self addAttribute:NSParagraphStyleAttributeName value:mutableStyle range:effectiveRange];
                [mutableStyle release];
            }
            i = NSMaxRange(effectiveRange);
        }
        [self endEditing];
    }
}

@end
