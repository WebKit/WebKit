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
#import <qlineedit.h>

#import <KWQNSTextField.h>

#import <kwqdebug.h>

QLineEdit::QLineEdit(QWidget *parent, const char *name)
{
    setView ([[[KWQNSTextField alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this] autorelease]);
}

QLineEdit::~QLineEdit()
{
}
    
void QLineEdit::setEchoMode(EchoMode mode)
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    if (mode == QLineEdit::Password)
        [field setPasswordMode: YES];
    else
        [field setPasswordMode: NO];
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
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    [field setStringValue: QSTRING_TO_NSSTRING(s)];
}


QString QLineEdit::text()
{
    KWQNSTextField *textView = (KWQNSTextField *)getView();
    
    return NSSTRING_TO_QSTRING ([textView stringValue]);
}


void QLineEdit::setMaxLength(int len)
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    [field setMaximumLength: len];
}


bool QLineEdit::isReadOnly() const
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field isEditable];
}


void QLineEdit::setReadOnly(bool flag)
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field setEditable: flag?NO:YES];
}


bool QLineEdit::event(QEvent *)
{
    _logNotYetImplemented();
    return FALSE;
}


bool QLineEdit::frame() const
{
    _logNotYetImplemented();
    return FALSE;
}


int QLineEdit::maxLength() const
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field maximumLength];
}


void QLineEdit::selectAll()
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field selectText: field];
}


bool QLineEdit::edited() const
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field edited];
}


void QLineEdit::setEdited(bool flag)
{
    KWQNSTextField *field = (KWQNSTextField *)getView();
    
    return [field setEdited:flag];
}
