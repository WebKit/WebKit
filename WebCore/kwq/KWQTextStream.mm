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

#define NEED_BOGUS_TEXTSTREAMS 1

#include <kwqdebug.h>

#include <qtextstream.h>

// class QTextStream ===========================================================

QTextStream::QTextStream()
{
    _logNotYetImplemented();
}


QTextStream::QTextStream(QByteArray, int)
{
    _logNotYetImplemented();
}


QTextStream::~QTextStream()
{
    _logNotYetImplemented();
}


QTextStream &QTextStream::operator<<(char)
{
    _logNotYetImplemented();
    return *this;
}


QTextStream &QTextStream::operator<<(const char *)
{
    _logNotYetImplemented();
    return *this;
}


QTextStream &QTextStream::operator<<(const QCString &)
{
    _logNotYetImplemented();
    return *this;
}


QTextStream &QTextStream::operator<<(const QString &)
{
    _logNotYetImplemented();
    return *this;
}


// class QTextIStream ==========================================================

QTextIStream::QTextIStream(QString *)
{
    _logNotYetImplemented();
}


QString QTextIStream::readLine()
{
    _logNotYetImplemented();
    return QString();
}


// class QTextOStream ==========================================================

QTextOStream::QTextOStream(QString *)
{
    _logNotYetImplemented();
}


QTextOStream::QTextOStream(QByteArray)
{
    _logNotYetImplemented();
}

QString QTextOStream::readLine()
{
    _logNotYetImplemented();
    return QString();
}

