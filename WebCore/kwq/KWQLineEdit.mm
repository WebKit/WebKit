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

#import "KWQLineEdit.h"

#import "KWQTextField.h"
#import "KWQLogging.h"
#import "WebCoreTextRendererFactory.h"

// This replicates constants from [NSTextFieldCell drawingRectForBounds].
#define VERTICAL_FUDGE_FACTOR 3

QLineEdit::QLineEdit()
    : m_returnPressed(this, SIGNAL(returnPressed()))
    , m_textChanged(this, SIGNAL(textChanged(const QString &)))
    , m_clicked(this, SIGNAL(clicked()))
{
    KWQTextField *view = [[KWQTextField alloc] initWithQLineEdit:this];
    [view setSelectable:YES]; // must do this explicitly so setEditable:NO does not make it NO
    setView(view);
    [view release];
}

QLineEdit::~QLineEdit()
{
    KWQTextField* textField = (KWQTextField*)getView();
    [textField invalidate];
}

void QLineEdit::setEchoMode(EchoMode mode)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setPasswordMode:mode == Password];
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
    [textField setFont:font.getNSFont()];
}

void QLineEdit::setText(const QString &s)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setStringValue:s.getNSString()];
}

QString QLineEdit::text()
{
    KWQTextField *textField = (KWQTextField *)getView();
    NSMutableString *text = [[[textField stringValue] mutableCopy] autorelease];
    [text replaceOccurrencesOfString:@"\r\n" withString:@"\n" options:NSLiteralSearch range:NSMakeRange(0, [text length])];
    [text replaceOccurrencesOfString:@"\r" withString:@"\n" options:NSLiteralSearch range:NSMakeRange(0, [text length])];
    return QString::fromNSString(text);
}

void QLineEdit::setMaxLength(int len)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setMaximumLength:len];
}

bool QLineEdit::isReadOnly() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    return ![textField isEditable];
}

void QLineEdit::setReadOnly(bool flag)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setEditable:!flag];
}

int QLineEdit::maxLength() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    return [textField maximumLength];
}

void QLineEdit::selectAll()
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField selectText:nil];
}

bool QLineEdit::edited() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    return [textField edited];
}

void QLineEdit::setEdited(bool flag)
{
    KWQTextField *textField = (KWQTextField *)getView();
    [textField setEdited:flag];
}

QSize QLineEdit::sizeForCharacterWidth(int numCharacters) const
{
    // Figure out how big a text field needs to be for a given number of characters
    // by installing a string with that number of characters (using "0" as the nominal
    // character) and then asking the field's cell what the size should be.

    KWQTextField *textField = (KWQTextField *)getView();

    ASSERT(numCharacters > 0);

    NSMutableString *nominalWidthString = [NSMutableString stringWithCapacity:numCharacters];
    for (int i = 0; i < numCharacters; ++i) {
        [nominalWidthString appendString:@"0"];
    }

    NSString *value = [textField stringValue];
    int maximumLength = [textField maximumLength];
    [textField setMaximumLength:numCharacters];
    [textField setStringValue:nominalWidthString];
    NSSize size = [[textField cell] cellSize];
    [textField setMaximumLength:maximumLength];
    [textField setStringValue:value];

    return QSize(size);
}

int QLineEdit::baselinePosition() const
{
    KWQTextField *textField = (KWQTextField *)getView();
    NSRect bounds = [textField bounds];
    NSFont *font = [textField font];
    return (int)ceil([[textField cell] drawingRectForBounds:bounds].origin.y - bounds.origin.y
        + [font defaultLineHeightForFont] + [font descender]);
}

void QLineEdit::clicked()
{
    m_clicked.call();
}

void QLineEdit::setAlignment(AlignmentFlags alignment)
{
    ASSERT(alignment == AlignLeft || alignment == AlignRight);
    KWQTextField *textField = getView();
    [textField setAlignment:(alignment == AlignRight ? NSRightTextAlignment : NSLeftTextAlignment)];
}
