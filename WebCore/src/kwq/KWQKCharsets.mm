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

// FIXME: obviously many functions here can be made inline

#include <kcharsets.h>
#include <qtextcodec.h>
#include <kwqdebug.h>

// constructors, copy constructors, and destructors ----------------------------

KCharsets::KCharsets()
{
    // do nothing
}

KCharsets::~KCharsets()
{
    // do nothing
}

// member functions --------------------------------------------------------

QTextCodec *KCharsets::codecForName(const QString &qs) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QTextCodec::codecForName(qs.latin1());
}

QTextCodec *KCharsets::codecForName(const QString &qs, bool &ok) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    ok = false;
    return QTextCodec::codecForName(qs.latin1());
}

QFont::CharSet KCharsets::charsetForEncoding(const QString &) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QFont::Latin1;
}

QFont::CharSet KCharsets::charsetForEncoding(const QString &, bool) const
{
    // FIXME: need real implementation here
    _logPartiallyImplemented();
    return QFont::Latin1;
}

void KCharsets::setQFont(QFont &font, QFont::CharSet) const
{
    // FIXME: do we need a real implementation here?
    // [kocienda: 2001-11-05] I don't think we need to implement this
    // do nothing
    _logNeverImplemented();
}

void KCharsets::setQFont(QFont &, QString) const
{
    // FIXME: do we need a real implementation here?
    // [kocienda: 2001-11-05] I don't think we need to implement this
    // do nothing
    _logNeverImplemented();
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
