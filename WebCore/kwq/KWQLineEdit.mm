/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qlineedit.h>

#import <KWQNSTextField.h>
#import <kwqdebug.h>
#import <WebCoreTextRendererFactory.h>

QLineEdit::QLineEdit(QWidget *parent)
{
    NSView *view = [[KWQNSTextField alloc] initWithFrame:NSMakeRect(0,0,0,0) widget:this];
    setView(view);
    [view release];
}

void QLineEdit::setEchoMode(EchoMode mode)
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
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
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    [textField setFont: [[WebCoreTextRendererFactory sharedFactory] 
            fontWithFamily: font.getNSFamily()
            traits: font.getNSTraits() 
            size: font.getNSSize()]];
}

void QLineEdit::setText(const QString &s)
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    [textField setStringValue:s.getNSString()];
}

QString QLineEdit::text()
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return QString::fromNSString([textField stringValue]);
}

void QLineEdit::setMaxLength(int len)
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    [textField setMaximumLength:len];
}

bool QLineEdit::isReadOnly() const
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return ![textField isEditable];
}

void QLineEdit::setReadOnly(bool flag)
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return [textField setEditable:!flag];
}

bool QLineEdit::frame() const
{
    _logNotYetImplemented();
    return FALSE;
}

int QLineEdit::maxLength() const
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return [textField maximumLength];
}

void QLineEdit::selectAll()
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return [textField selectText:nil];
}

bool QLineEdit::edited() const
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return [textField edited];
}

void QLineEdit::setEdited(bool flag)
{
    KWQNSTextField *textField = (KWQNSTextField *)getView();
    return [textField setEdited:flag];
}
