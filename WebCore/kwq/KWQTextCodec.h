/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef QTEXTCODEC_H_
#define QTEXTCODEC_H_

#include "KWQCharsets.h"
#include "KWQString.h"
#include "KWQCString.h"

class QTextDecoder;

class QTextCodec {
public:
    static QTextCodec *codecForName(const char *);
    static QTextCodec *codecForNameEightBitOnly(const char *);
    static QTextCodec *codecForLocale();

#if __APPLE__
    explicit QTextCodec(CFStringEncoding e, KWQEncodingFlags f = NoEncodingFlags) : _encoding(e), _flags(f) { }
#endif

    const char *name() const;
    bool usesVisualOrdering() const { return _flags & VisualOrdering; }
    bool isJapanese() const { return _flags & IsJapanese; }
    
    QChar backslashAsCurrencySymbol() const;

    QTextDecoder *makeDecoder() const;

    QCString fromUnicode(const QString &str, bool allowEntities = false) const;

    QString toUnicode(const char *, int) const;
    QString toUnicode(const QByteArray &, int) const;
    
    friend bool operator==(const QTextCodec &, const QTextCodec &);
    unsigned hash() const;
    
private:
#if __APPLE__
    CFStringEncoding _encoding;
#endif
    KWQEncodingFlags _flags;
};

inline bool operator!=(const QTextCodec &a, const QTextCodec &b) { return !(a == b); }

class QTextDecoder {
public:
    virtual ~QTextDecoder();
    virtual QString toUnicode(const char *, int, bool flush = false) = 0;
};

#endif
