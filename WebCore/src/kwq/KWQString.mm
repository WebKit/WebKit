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

#include <qstring.h>

QString::QString()
{
    s = NULL;
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(NULL, 0);
    CFStringAppendCharacters(s, &qc.c, 1);
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        s = CFStringCreateMutable(NULL, 0);
        const int capacity = 64;
        UniChar buf[capacity];
        int fill = 0;
        for (uint len = 0; (len < qba.size()) && qba[len]; len++) {
            buf[fill] = qba[len];
            fill++;
            if (fill == capacity) {
                CFStringAppendCharacters(s, buf, fill);
                fill = 0;
            }
        }
        if (fill) {
            CFStringAppendCharacters(s, buf, fill);
        }
    } else {
        s = NULL;
    }
}

QString::QString(const QChar *qcs, uint len)
{
    if (qcs || len) {
        s = CFStringCreateMutable(NULL, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else {
        s = NULL;
    }
}

QString::QString(const char *chs)
{
    if (chs && *chs) {
        s = CFStringCreateMutable(NULL, 0);
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else {
        s = NULL;
    }
}

QString::~QString()
{
    CFRelease(s);
}

QString &QString::operator=(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    CFRelease(s);
    s = qs.s;
    return *this;
}

QString &QString::operator=(const QCString &qcs)
{
    return *this = QString(qcs);
}

QString &QString::operator=(const char *chs)
{
    return *this = QString(chs);
}

QString &QString::operator=(QChar qc)
{
    return *this = QString(qc);
}

QString &QString::operator=(char ch)
{
    return *this = QString(QChar(ch));
}

QConstString::QConstString(QChar *qcs, uint len)
{
    if (qcs || len) {
        // NOTE: use instead of CFStringCreateWithCharactersNoCopy function to
        // guarantee backing store is not copied even though string is mutable
        s = CFStringCreateMutableWithExternalCharactersNoCopy(NULL, &qcs->c,
                len, len, kCFAllocatorNull);
    } else {
        s = NULL;
    }
}

const QString &QConstString::string() const
{
    return *this;
}
