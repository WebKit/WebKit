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

#include <qtextcodec.h>
#include <kwqdebug.h>

// FIXME: do we need this one we have a REAL implementation?
static QTextCodec latin1TextCodec(kCFStringEncodingISOLatin1);

// class QTextDecoder ==========================================================

// constructors, copy constructors, and destructors ----------------------------

QTextDecoder::QTextDecoder(const QTextCodec *tc)
{
    textCodec = tc;
}

QTextDecoder::~QTextDecoder()
{
    // do nothing
}

// member functions --------------------------------------------------------

QString QTextDecoder::toUnicode(const char *chs, int len)
{
    return textCodec->toUnicode(chs, len);
}


// class QTextCodec ============================================================

// static member functions -----------------------------------------------------

QTextCodec *QTextCodec::codecForMib(int)
{
    // FIXME: need REAL implementation here
    _logPartiallyImplemented();
    return &latin1TextCodec;
}

QTextCodec *QTextCodec::codecForName(const char *)
{
    // FIXME: need REAL implementation here
    _logPartiallyImplemented();
    return &latin1TextCodec;
}

QTextCodec *QTextCodec::codecForLocale()
{
    // FIXME: need REAL implementation here
    _logPartiallyImplemented();
    return &latin1TextCodec;
}

// constructors, copy constructors, and destructors ----------------------------

QTextCodec::QTextCodec(int e)
{
    encoding = e;
}

QTextCodec::~QTextCodec()
{
    // do nothing
}

// member functions --------------------------------------------------------

QTextDecoder *QTextCodec::makeDecoder() const
{
    // FIXME: will this leak or do clients dispose of the object?
    return new QTextDecoder(this);
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    // FIXME: is there a more efficient way to do this?
    return QCString(qcs.latin1());
}

QString QTextCodec::toUnicode(const char *chs, int len) const
{
    return QString::fromStringWithEncoding(chs, len, encoding);
}

QString QTextCodec::toUnicode(const QByteArray &qba, int len) const
{
    return QString::fromStringWithEncoding(qba, len, encoding);
}

QString QTextCodec::toUnicode(const char *chs) const
{
    return QString::fromStringWithEncoding(chs, -1, encoding);
}
