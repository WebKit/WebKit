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

QLineEdit::QLineEdit(QWidget *parent)
{
    NSView *view = [[KWQNSTextField alloc] initWithFrame:NSMakeRect(0,0,0,0) widget:this];
    setView(view);
    [view release];
}

void QLineEdit::setEchoMode(EchoMode mode)
{
    [(KWQNSTextField *)getView() setPasswordMode:mode == Password];
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

void QLineEdit::setText(const QString &s)
{
    [(KWQNSTextField *)getView() setStringValue:s.getNSString()];
}

QString QLineEdit::text()
{
    return QString::fromNSString([(KWQNSTextField *)getView() stringValue]);
}

void QLineEdit::setMaxLength(int len)
{
    [(KWQNSTextField *)getView() setMaximumLength:len];
}

bool QLineEdit::isReadOnly() const
{
    return ![(KWQNSTextField *)getView() isEditable];
}

void QLineEdit::setReadOnly(bool flag)
{
    return [(KWQNSTextField *)getView() setEditable:!flag];
}

bool QLineEdit::frame() const
{
    _logNotYetImplemented();
    return FALSE;
}

int QLineEdit::maxLength() const
{
    return [(KWQNSTextField *)getView() maximumLength];
}

void QLineEdit::selectAll()
{
    return [(KWQNSTextField *)getView() selectText:nil];
}

bool QLineEdit::edited() const
{
    return [(KWQNSTextField *)getView() edited];
}

void QLineEdit::setEdited(bool flag)
{
    return [(KWQNSTextField *)getView() setEdited:flag];
}
