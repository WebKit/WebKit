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

#ifndef QSTRING_H_
#define QSTRING_H_

#include <kwq.h>
#include <string.h>

#include "qcstring.h"

class QChar {
public:
    QChar(char);
    QChar(const QChar &);
    QChar lower() const;
    char latin1() const;
    bool isDigit() const;
    bool isSpace() const;
    friend inline int operator==(QChar, QChar);
    friend inline int operator!=(QChar, QChar);
};

class QString {
public:
    QString();
    QString(const QChar *, uint);
    int toInt() const;
    int toInt(bool *) const;
    bool isNull() const;
    const QChar *unicode() const;
    uint length() const;
    QString &sprintf(const char *format, ...);
    QString lower() const;
    QString stripWhiteSpace() const;
    bool isEmpty() const;
    int contains(const char *) const;

    static const QString null;

    // FIXME: bogus constructor hack for "conversion from int to non-scalar
    // type" error in "Node::toHTML()" function in "dom/dom_node.cpp"
    QString(int);
};

class QConstString {
public:
    QConstString(QChar *, uint);
    const QString string() const;
};

#endif
