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

#include <qtextcodec.h>

class QTextCodec;

class KWQSimpleTextCodec : public QTextCodec {
public:

    virtual ~KWQSimpleTextCodec() {}

    int KWQSimpleTextCodec::mibEnum() const {
        // FIXME: do real work here
        return 0;
    }

    const char *KWQSimpleTextCodec::name() const
    {
        // FIXME: do real work here
        return "KWQSimpleTextCodec";
    }

};


class KWQSimpleTextDecoder : public QTextDecoder {
public:

    virtual ~KWQSimpleTextDecoder() {}

    QString KWQSimpleTextDecoder::toUnicode(const char *s, int i) {
        return QString(s);
    }
};

// class QTextDecoder ==========================================================


QTextDecoder::~QTextDecoder()
{
}


// class QTextCodec ============================================================

QTextCodec *QTextCodec::codecForMib(int)
{
    // FIXME: do real work here
    return new KWQSimpleTextCodec();
}


QTextCodec *QTextCodec::codecForName(const char *, int accuracy=0)
{
    // FIXME: do real work here
    return new KWQSimpleTextCodec();
}


QTextCodec *QTextCodec::codecForLocale()
{
    // FIXME: do real work here
    return new KWQSimpleTextCodec();
}


QTextCodec::~QTextCodec()
{
}

// member functions --------------------------------------------------------

QTextDecoder *QTextCodec::makeDecoder() const
{
    // FIXME: do real work here
    return new KWQSimpleTextDecoder();
}


QCString QTextCodec::fromUnicode(const QString &s) const
{
    return QCString(s.latin1());
}


QString QTextCodec::toUnicode(const char *s, int) const
{
    return QString(s);
}

QString QTextCodec::toUnicode(const QByteArray &array, int) const
{
    return QString(array);
}


QString QTextCodec::toUnicode(const char *s) const
{
    return QString(s);
}

