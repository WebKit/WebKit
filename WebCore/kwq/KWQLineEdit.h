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

#ifndef QLINEEDIT_H_
#define QLINEEDIT_H_

#include "KWQWidget.h"
#include "KWQEvent.h"
#include "KWQString.h"
#include "KWQSignal.h"

class QLineEdit : public QWidget {
public:
    enum EchoMode { Normal, Password };

    QLineEdit();

    virtual void setEchoMode(EchoMode);
    virtual void setCursorPosition(int);
    virtual void setText(const QString &);
    virtual QString text();
    virtual void setMaxLength(int);

    bool isReadOnly() const;
    void setReadOnly(bool);
    bool frame() const;
    int cursorPosition() const;
    int maxLength() const;
    void selectAll();
    bool edited() const;
    void setEdited(bool);
    
    void setFont(const QFont &font);
    
    QSize sizeForCharacterWidth(int numCharacters) const;
    int baselinePosition() const;
    
    void returnPressed() { m_returnPressed.call(); }
    void textChanged() { m_textChanged.call(text()); }

private:
    KWQSignal m_returnPressed;
    KWQSignal m_textChanged;
};

#endif
