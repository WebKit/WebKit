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

// FIXME: should QChar and QConstString be in separate source files?

#include <qstring.h>

// FIXME: what's the minimum capacity?
#define SCRATCH_BUFFER_CAPACITY 10

static UniChar scratchBuffer[SCRATCH_BUFFER_CAPACITY];

static CFMutableStringRef GetScratchBufferString()
{
    static CFMutableStringRef s = NULL;

    if (!s) {
        // FIXME: this string object will be leaked once
        s = CFStringCreateMutableWithExternalCharactersNoCopy(NULL,
                scratchBuffer, SCRATCH_BUFFER_CAPACITY,
                SCRATCH_BUFFER_CAPACITY, kCFAllocatorNull);
    }
    return s;
}

QChar::QChar()
{
    c = 0;
}

QChar::QChar(char ch)
{
    c = ch;
}

QChar::QChar(uchar uch)
{
    c = uch;
}

QChar::QChar(short n)
{
    c = n;
}

QChar::QChar(ushort n)
{
    c = n;
}

QChar::QChar(uint n)
{
    c = n;
}

QChar::QChar(int n)
{
    c = n;
}

QChar::QChar(const QChar &qc)
{
    c = qc.c;
}

QChar::~QChar()
{
    // do nothing
}

QChar QChar::lower() const
{
    // FIXME: unimplemented
    return *this;
}

QChar QChar::upper() const
{
    // FIXME: unimplemented
    return *this;
}

char QChar::latin1() const
{
    // FIXME: unimplemented
    return 0;
}

bool QChar::isNull() const
{
    // FIXME: unimplemented
    return FALSE;
}

bool QChar::isDigit() const
{
    // FIXME: unimplemented
    return FALSE;
}

bool QChar::isSpace() const
{
    // FIXME: unimplemented
    return FALSE;
}

bool QChar::isLetter() const
{
    // FIXME: unimplemented
    return FALSE;
}

bool QChar::isLetterOrNumber() const
{
    // FIXME: unimplemented
    return FALSE;
}

bool QChar::isPunct() const
{
    // FIXME: unimplemented
    return FALSE;
}

uchar QChar::cell() const
{
    // return least significant byte
    return c & 0xff;
}

uchar QChar::row() const
{
    // return most significant byte
    return (c & 0xff00) >> 8;
}

QChar::Direction QChar::direction() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    return DirL;
}

bool QChar::mirrored() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    // return whether character should be reversed if text direction is reversed
    return FALSE;
}

QChar QChar::mirroredChar() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    // return mirrored character if it is mirrored else return itself
    return *this;
}

ushort QChar::unicode() const
{
    return c;
}

QString::QString()
{
    s = NULL;
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(NULL, 0);
    CFStringAppendCharacters(s, &qc.c, 1);
}

QString::QString(const QChar *qc, uint len)
{
    if (qc || len) {
        s = CFStringCreateMutable(NULL, 0);
        CFStringAppendCharacters(s, &qc->c, len);
    } else {
        s = NULL;
    }
}

QString::QString(const char *ch)
{
    if (ch) {
        s = CFStringCreateMutable(NULL, 0);
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringAppendCString(s, ch, kCFStringEncodingISOLatin1);
    } else {
        s = NULL;
    }
}

QString::~QString()
{
    CFRelease(s);
}

QConstString::QConstString(QChar *qc, uint len)
{
    if (qc || len) {
        // NOTE: use instead of CFStringCreateWithCharactersNoCopy function to
        // guarantee backing store is not copied even though string is mutable
        s = CFStringCreateMutableWithExternalCharactersNoCopy(NULL, &qc->c,
                len, len, kCFAllocatorNull);
    } else {
        s = NULL;
    }
}

const QString &QConstString::string() const
{
    return *this;
}
