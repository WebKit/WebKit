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

#import "KWQLineEdit.h"

#import "KWQExceptions.h"
#import "KWQLogging.h"
#import "KWQTextField.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

QLineEdit::QLineEdit()
    : m_returnPressed(this, SIGNAL(returnPressed()))
    , m_textChanged(this, SIGNAL(textChanged(const QString &)))
    , m_clicked(this, SIGNAL(clicked()))
{
    KWQTextField *view = nil;

    KWQ_BLOCK_EXCEPTIONS;
    view = [[KWQTextField alloc] initWithQLineEdit:this];
    setView(view);
    [view release];
    [view setSelectable:YES]; // must do this explicitly so setEditable:NO does not make it NO
    KWQ_UNBLOCK_EXCEPTIONS;
}

QLineEdit::~QLineEdit()
{
    KWQTextField* textField = (KWQTextField*)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField invalidate];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setEchoMode(EchoMode mode)
{
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setPasswordMode:mode == Password];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setCursorPosition(int)
{
    // Don't do anything here.
}

int QLineEdit::cursorPosition() const
{
    // Not needed.  We ignore setCursorPosition().
    return 0;
}

void QLineEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setFont:font.getNSFont()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setText(const QString &s)
{
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setStringValue:s.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString QLineEdit::text()
{
    KWQTextField *textField = (KWQTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSMutableString *text = [[[textField stringValue] mutableCopy] autorelease];
    [text replaceOccurrencesOfString:@"\r\n" withString:@"\n" options:NSLiteralSearch range:NSMakeRange(0, [text length])];
    [text replaceOccurrencesOfString:@"\r" withString:@"\n" options:NSLiteralSearch range:NSMakeRange(0, [text length])];
    return QString::fromNSString(text);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

void QLineEdit::setMaxLength(int len)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setMaximumLength:len];
}

bool QLineEdit::isReadOnly() const
{
    KWQTextField *textField = (KWQTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return ![textField isEditable];
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

void QLineEdit::setReadOnly(bool flag)
{
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setEditable:!flag];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int QLineEdit::maxLength() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    return [textField maximumLength];
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

void QLineEdit::selectAll()
{
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField selectText:nil];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QLineEdit::edited() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    return [textField edited];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QLineEdit::setEdited(bool flag)
{
    KWQTextField *textField = (KWQTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setEdited:flag];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize QLineEdit::sizeForCharacterWidth(int numCharacters) const
{
    // Figure out how big a text field needs to be for a given number of characters
    // (using "0" as the nominal character).

    KWQTextField *textField = (KWQTextField *)getView();

    ASSERT(numCharacters > 0);

    NSSize size = { 0, 0 };

    KWQ_BLOCK_EXCEPTIONS;

    NSString *value = [textField stringValue];
    [textField setStringValue:@""];
    size = [[textField cell] cellSize];
    [textField setStringValue:value];

    id <WebCoreTextRenderer> renderer = [[WebCoreTextRendererFactory sharedFactory]
        rendererWithFont:[textField font] usingPrinterFont:![NSGraphicsContext currentContextDrawingToScreen]];

    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;

    const UniChar zero = '0';
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, &zero, 1, 0, 1);

    size.width += ceilf([renderer floatWidthForRun:&run style:&style widths:0] * numCharacters);

    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize(size);
}

int QLineEdit::baselinePosition(int height) const
{
    KWQTextField *textField = (KWQTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSRect bounds = [textField bounds];
    NSFont *font = [textField font];
    return static_cast<int>(ceilf([[textField cell] drawingRectForBounds:bounds].origin.y - bounds.origin.y
        + [font defaultLineHeightForFont] + [font descender]));
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

void QLineEdit::clicked()
{
    m_clicked.call();
}

bool QLineEdit::hasMarkedText()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [[NSInputManager currentInputManager] hasMarkedText];
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

void QLineEdit::setAlignment(AlignmentFlags alignment)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQTextField *textField = getView();
    [textField setAlignment:KWQNSTextAlignmentForAlignmentFlags(alignment)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setWritingDirection(QPainter::TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQTextField *textField = getView();
    [textField setBaseWritingDirection:(direction == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QLineEdit::checksDescendantsForFocus() const
{
    return true;
}

NSTextAlignment KWQNSTextAlignmentForAlignmentFlags(Qt::AlignmentFlags a)
{
    switch (a) {
        default:
            ERROR("unsupported alignment");
        case Qt::AlignLeft:
            return NSLeftTextAlignment;
        case Qt::AlignRight:
            return NSRightTextAlignment;
        case Qt::AlignHCenter:
            return NSCenterTextAlignment;
    }
}
