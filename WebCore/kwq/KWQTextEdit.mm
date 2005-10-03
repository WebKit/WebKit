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

#include "config.h"
#import "KWQTextEdit.h"

#import <kxmlcore/Assertions.h>
#import "KWQExceptions.h"
#import "KWQLineEdit.h"
#import "KWQPalette.h"
#import "KWQTextArea.h"

QTextEdit::QTextEdit(QWidget *parent)
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged()))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQTextArea *textView = [[KWQTextArea alloc] initWithQTextEdit:this];
    setView(textView);
    [textView release];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QTextEdit::~QTextEdit()
{
    KWQTextArea *textArea = (KWQTextArea *)getView();
    [textArea detachQTextEdit]; 
}

void QTextEdit::setText(const QString &string)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textView setText:string.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString QTextEdit::text() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([textView text]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString QTextEdit::textWithHardLineBreaks() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([textView textWithHardLineBreaks]);
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return QString();
}

void QTextEdit::getCursorPosition(int *paragraph, int *index) const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    if (index)
        *index = 0;
    if (paragraph)
        *paragraph = 0;
    
    KWQ_BLOCK_EXCEPTIONS;
    [textView getCursorPositionAsIndex:index inParagraph:paragraph];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setCursorPosition(int paragraph, int index)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [textView setCursorPositionToIndex:index inParagraph:paragraph];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QTextEdit::WrapStyle QTextEdit::wordWrap() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return [textView wordWrap] ? WidgetWidth : NoWrap;
    KWQ_UNBLOCK_EXCEPTIONS;

    return NoWrap;
}

void QTextEdit::setWordWrap(WrapStyle style)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textView setWordWrap:style == WidgetWidth];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;

    // this declaration must be inside the KWQ_BLOCK_EXCEPTIONS block or the deployment build fails
    bool autohides = hMode == Auto || vMode == Auto;
    
    ASSERT(!autohides || hMode != AlwaysOn);
    ASSERT(!autohides || vMode != AlwaysOn);

    [textView setHasHorizontalScroller:hMode != AlwaysOff];
    [textView setHasVerticalScroller:vMode != AlwaysOff];
    [textView setAutohidesScrollers:autohides];

    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QTextEdit::isReadOnly() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return ![textView isEditable];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QTextEdit::setReadOnly(bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [textView setEditable:!flag];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QTextEdit::isDisabled() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return ![textView isEnabled];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QTextEdit::setDisabled(bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [textView setEnabled:!flag];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int QTextEdit::selectionStart()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound)
        return 0;
    return range.location;
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return 0;
}

int QTextEdit::selectionEnd()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    NSRange range = [textView selectedRange];
    if (range.location == NSNotFound)
        return 0;
    return range.location + range.length; // Use NSMaxRange when 4213314 is fixed
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return 0;
}

void QTextEdit::setSelectionStart(int start)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
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
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setSelectionEnd(int end)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
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
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QTextEdit::hasSelectedText() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    return [textView hasSelection];
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return false;
}

void QTextEdit::selectAll()
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [textView selectAll];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setSelectionRange(int start, int length)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
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
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [textView setFont:font.getNSFont()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::clicked()
{
    _clicked.call();
}

void QTextEdit::setAlignment(AlignmentFlags alignment)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQTextArea *textArea = static_cast<KWQTextArea *>(getView());
    [textArea setAlignment:KWQNSTextAlignmentForAlignmentFlags(alignment)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setLineHeight(int lineHeight)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQTextArea *textArea = static_cast<KWQTextArea *>(getView());
    [textArea setLineHeight:lineHeight];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QTextEdit::setWritingDirection(QPainter::TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQTextArea *textArea = static_cast<KWQTextArea *>(getView());
    [textArea setBaseWritingDirection:(direction == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];

    KWQ_UNBLOCK_EXCEPTIONS;
}
 
QSize QTextEdit::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
    KWQTextArea *textArea = static_cast<KWQTextArea *>(getView());
    NSSize size = {0,0};

    KWQ_BLOCK_EXCEPTIONS;
    size = [textArea sizeWithColumns:numColumns rows:numRows];
    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize((int)ceil(size.width), (int)ceil(size.height));
}

QWidget::FocusPolicy QTextEdit::focusPolicy() const
{
    FocusPolicy policy = QScrollView::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool QTextEdit::checksDescendantsForFocus() const
{
    return true;
}

void QTextEdit::setPalette(const QPalette &palette)
{
    QWidget::setPalette(palette);

    KWQTextArea *textArea = static_cast<KWQTextArea *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    
    // Below is a workaround for the following AppKit bug which causes transparent backgrounds to be 
    // drawn opaque <rdar://problem/3142730>.  Without this workaround, some textareas would be drawn with black backgrounds
    // as described in <rdar://problem/3854383>.  We now call setDrawsBackground:NO when the background color is completely 
    // transparent.  This does not solve the problem for translucent background colors for textareas <rdar://problem/3865161>.

    [textArea setTextColor:nsColor(palette.foreground())];

    QColor background = palette.background();
    if (!background.isValid())
        background = Qt::white;
    [textArea setBackgroundColor:nsColor(background)];
    [textArea setDrawsBackground:background.alpha() != 0];

    KWQ_UNBLOCK_EXCEPTIONS;
}
