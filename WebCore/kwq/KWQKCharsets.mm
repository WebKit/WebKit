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

#import <kcharsets.h>
#import <qtextcodec.h>
#import <kwqdebug.h>

QTextCodec *KCharsets::codecForName(const QString &qs) const
{
    return QTextCodec::codecForName(qs.latin1());
}

QTextCodec *KCharsets::codecForName(const QString &qs, bool &ok) const
{
    QTextCodec *codec = QTextCodec::codecForName(qs.latin1());
    if (codec == NULL) {
        ok = false;
        codec = QTextCodec::codecForName("latin1");
    } else {
        ok = true;
    }

    return codec;
}

QFont::CharSet KCharsets::charsetForEncoding(const QString &) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QFont::Unicode;
}

QFont::CharSet KCharsets::charsetForEncoding(const QString &, bool) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QFont::Unicode;
}

QString KCharsets::name(QFont::CharSet)
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QString();
}

QString KCharsets::xCharsetName(QFont::CharSet) const
{
    // FIXME: do we need a real implementation here?
    _logPartiallyImplemented();
    return QString();
}

bool KCharsets::supportsScript(const QFont &, QFont::CharSet)
{
    // FIXME: do we need a real implementation here?
    _logPartiallyImplemented();
    return true;
}
