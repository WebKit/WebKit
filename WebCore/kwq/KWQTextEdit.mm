/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "KWQTextEdit.h"

#import "Font.h"
#import "IntSize.h"
#import "BlockExceptions.h"
#import "KWQLineEdit.h"
#import "WebCoreTextArea.h"
#import "WidgetClient.h"
#import <wtf/Assertions.h>

using namespace WebCore;

QTextEdit::QTextEdit(Widget *parent)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebCoreTextArea *textView = [[WebCoreTextArea alloc] initWithQTextEdit:this];
    setView(textView);
    [textView release];
    END_BLOCK_OBJC_EXCEPTIONS;
}

QTextEdit::~QTextEdit()
{
    WebCoreTextArea *textArea = (WebCoreTextArea *)getView();
    [textArea detachQTextEdit]; 
}

void QTextEdit::setText(const String& string)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setText:string];
    END_BLOCK_OBJC_EXCEPTIONS;
}

String QTextEdit::text() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return String([textView text]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return String();
}

String QTextEdit::textWithHardLineBreaks() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return String([textView textWithHardLineBreaks]);
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return String();
}

void QTextEdit::getCursorPosition(int *paragraph, int *index) const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    if (index)
        *index = 0;
    if (paragraph)
        *paragraph = 0;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView getCursorPositionAsIndex:index inParagraph:paragraph];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setCursorPosition(int paragraph, int index)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setCursorPositionToIndex:index inParagraph:paragraph];
    END_BLOCK_OBJC_EXCEPTIONS;
}

QTextEdit::WrapStyle QTextEdit::wordWrap() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [textView wordWrap] ? WidgetWidth : NoWrap;
    END_BLOCK_OBJC_EXCEPTIONS;

    return NoWrap;
}

void QTextEdit::setWordWrap(WrapStyle style)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setWordWrap:style == WidgetWidth];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // this declaration must be inside the BEGIN_BLOCK_OBJC_EXCEPTIONS block or the deployment build fails
    bool autohides = hMode == ScrollBarAuto || vMode == ScrollBarAuto;
    
    ASSERT(!autohides || hMode != ScrollBarAlwaysOn);
    ASSERT(!autohides || vMode != ScrollBarAlwaysOn);

    [textView setHasHorizontalScroller:hMode != ScrollBarAlwaysOff];
    [textView setHasVerticalScroller:vMode != ScrollBarAlwaysOff];
    [textView setAutohidesScrollers:autohides];

    END_BLOCK_OBJC_EXCEPTIONS;
}

bool QTextEdit::isReadOnly() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return ![textView isEditable];
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void QTextEdit::setReadOnly(bool flag)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setEditable:!flag];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool QTextEdit::isDisabled() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return ![textView isEnabled];
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void QTextEdit::setDisabled(bool flag)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setEnabled:!flag];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int QTextEdit::selectionStart()
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound)
        return 0;
    return range.location;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

int QTextEdit::selectionEnd()
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound)
        return 0;
    return range.location + range.length; // Use NSMaxRange when 4213314 is fixed
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

void QTextEdit::setSelectionStart(int start)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound) {
        range.location = 0;
        range.length = 0;
    }
    
    // coerce start to a valid value
    int maxLength = [[textView text] length];
    int newStart = start;
    if (newStart < 0)
        newStart = 0;
    if (newStart > maxLength)
        newStart = maxLength;
    
    if ((unsigned)newStart < range.location + range.length) {
        // If we're expanding or contracting, but not collapsing the selection
        range.length += range.location - newStart;
        range.location = newStart;
    } else {
        // ok, we're collapsing the selection
        range.location = newStart;
        range.length = 0;
    }
    
    [textView setSelectedRange:range];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setSelectionEnd(int end)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound) {
        range.location = 0;
        range.length = 0;
    }
    
    // coerce end to a valid value
    int maxLength = [[textView text] length];
    int newEnd = end;
    if (newEnd < 0)
        newEnd = 0;
    if (newEnd > maxLength)
        newEnd = maxLength;
    
    if ((unsigned)newEnd >= range.location) {
        // If we're just changing the selection length, but not location..
        range.length = newEnd - range.location;
    } else {
        // ok, we've collapsed the selection and are moving it
        range.location = newEnd;
        range.length = 0;
    }
    
    [textView setSelectedRange:range];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool QTextEdit::hasSelectedText() const
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [textView hasSelection];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

void QTextEdit::selectAll()
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView selectAll];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setSelectionRange(int start, int length)
{
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    int newStart = start;
    int newLength = length;
    if (newStart < 0) {
        // truncate the length by the negative start
        newLength = length + newStart;
        newStart = 0;
    }
    if (newLength < 0) {
        newLength = 0;
    }
    int maxlen = [[textView text] length];
    if (newStart > maxlen) {
        newStart = maxlen;
    }
    if (newStart + newLength > maxlen) {
        newLength = maxlen - newStart;
    }
    NSRange tempRange = {newStart, newLength}; // 4213314
    [textView setSelectedRange:tempRange];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setFont(const Font& font)
{
    Widget::setFont(font);
    WebCoreTextArea *textView = (WebCoreTextArea *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textView setFont:font.getNSFont()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setAlignment(HorizontalAlignment alignment)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    [textArea setAlignment:KWQNSTextAlignment(alignment)];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setLineHeight(int lineHeight)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    [textArea setLineHeight:lineHeight];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void QTextEdit::setWritingDirection(TextDirection direction)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    [textArea setBaseWritingDirection:(direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];

    END_BLOCK_OBJC_EXCEPTIONS;
}
 
IntSize QTextEdit::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());
    NSSize size = {0,0};

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    size = [textArea sizeWithColumns:numColumns rows:numRows];
    END_BLOCK_OBJC_EXCEPTIONS;

    return IntSize((int)ceil(size.width), (int)ceil(size.height));
}

Widget::FocusPolicy QTextEdit::focusPolicy() const
{
    FocusPolicy policy = ScrollView::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool QTextEdit::checksDescendantsForFocus() const
{
    return true;
}

void QTextEdit::setColors(const Color& background, const Color& foreground)
{
    WebCoreTextArea *textArea = static_cast<WebCoreTextArea *>(getView());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    // Below is a workaround for the following AppKit bug which causes transparent backgrounds to be 
    // drawn opaque <rdar://problem/3142730>.  Without this workaround, some textareas would be drawn with black backgrounds
    // as described in <rdar://problem/3854383>.  We now call setDrawsBackground:NO when the background color is completely 
    // transparent.  This does not solve the problem for translucent background colors for textareas <rdar://problem/3865161>.

    [textArea setTextColor:nsColor(foreground)];

    Color bg = background;
    if (!bg.isValid())
        bg = Color::white;
    [textArea setBackgroundColor:nsColor(bg)];
    [textArea setDrawsBackground:bg.alpha() != 0];

    END_BLOCK_OBJC_EXCEPTIONS;
}
