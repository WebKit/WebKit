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

#import <KWQLogging.h>

#import <qtextstream.h>

// class QTextStream ===========================================================

QTextStream::QTextStream()
{
    LOG(NotYetImplemented, "not yet implemented");
}


QTextStream::QTextStream(QByteArray, int)
{
    LOG(NotYetImplemented, "not yet implemented");
}

QTextStream::QTextStream(QString *, int)
{
    LOG(NotYetImplemented, "not yet implemented");
}


QTextStream::~QTextStream()
{
    LOG(NotYetImplemented, "not yet implemented");
}


QTextStream &QTextStream::operator<<(char)
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}


QTextStream &QTextStream::operator<<(const char *)
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}


QTextStream &QTextStream::operator<<(const QCString &)
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}


QTextStream &QTextStream::operator<<(const QString &)
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}


// class QTextIStream ==========================================================

QTextIStream::QTextIStream(QString *)
{
    LOG(NotYetImplemented, "not yet implemented");
}


QString QTextIStream::readLine()
{
    LOG(NotYetImplemented, "not yet implemented");
    return QString();
}


// class QTextOStream ==========================================================

QTextOStream::QTextOStream(QString *)
{
    LOG(NotYetImplemented, "not yet implemented");
}


QTextOStream::QTextOStream(QByteArray)
{
    LOG(NotYetImplemented, "not yet implemented");
}

QString QTextOStream::readLine()
{
    LOG(NotYetImplemented, "not yet implemented");
    return QString();
}


QTextStream &QTextStream::operator<<(QTextStream &(*const &)(QTextStream &)) 
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}

QTextStream &QTextStream::operator<<(void const *)
{
    LOG(NotYetImplemented, "not yet implemented");
    return *this;
}

QTextStream &endl(QTextStream& stream)
{
    LOG(NotYetImplemented, "not yet implemented");
    return stream;
}
