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

#include <kwqdebug.h>
#include <Foundation/Foundation.h>
#include <qstring.h>

#ifndef USING_BORROWED_QSTRING

static UniChar scratchUniChar;

/* FIXME: use of this function is clearly not threadsafe! */
static CFMutableStringRef GetScratchUniCharString()
{
    static CFMutableStringRef s = NULL;

    if (!s) {
        // FIXME: this CFMutableString will be leaked exactly once
        s = CFStringCreateMutableWithExternalCharactersNoCopy(
                kCFAllocatorDefault, &scratchUniChar, 1, 1, kCFAllocatorNull);
    }
    return s;
}

// constants -------------------------------------------------------------------

const QChar QChar::null;

// constructors, copy constructors, and destructors ----------------------------

#ifndef _KWQ_QCHAR_INLINES_

QChar::QChar()
{
    c = 0;
}

QChar::QChar(char ch)
{
    c = (uchar) ch;
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
    // do nothing because the single data member is a UniChar
}

#endif  // _KWQ_QCHAR_INLINES_

// member functions ------------------------------------------------------------

ushort QChar::unicode() const
{
    return c;
}

uchar QChar::cell() const
{
    // return least significant byte
    return c;
}

uchar QChar::row() const
{
    // return most significant byte
    return c >> 8;
}

char QChar::latin1() const
{
    return c > 0xff ? 0 : c;
}

QChar::operator char() const
{
    return c > 0xff ? 0 : c;
}

bool QChar::isNull() const
{
    return c == 0;
}

bool QChar::isSpace() const
{
    // FIXME: should we use this optimization?
#if 0
    if (c <= 0xff) {
	return isspace(c);
    }
#endif
    return CFCharacterSetIsCharacterMember(CFCharacterSetGetPredefined(
                kCFCharacterSetWhitespaceAndNewline), c);
}

bool QChar::isDigit() const
{
    return CFCharacterSetIsCharacterMember(CFCharacterSetGetPredefined(
                kCFCharacterSetDecimalDigit), c);
}

bool QChar::isLetter() const
{
    return CFCharacterSetIsCharacterMember(CFCharacterSetGetPredefined(
                kCFCharacterSetLetter), c);
}

bool QChar::isNumber() const
{
    return isLetterOrNumber() && !isLetter();
}

bool QChar::isLetterOrNumber() const
{
    return CFCharacterSetIsCharacterMember(CFCharacterSetGetPredefined(
                kCFCharacterSetAlphaNumeric), c);
}

bool QChar::isPunct() const
{
    return CFCharacterSetIsCharacterMember(CFCharacterSetGetPredefined(
                kCFCharacterSetPunctuation), c);
}

QChar QChar::lower() const
{
    CFMutableStringRef scratchUniCharString = GetScratchUniCharString();
    if (scratchUniCharString) {
        scratchUniChar = c;
        CFStringLowercase(scratchUniCharString, NULL);
        if (scratchUniChar) {
            return QChar(scratchUniChar);
        }
    }
    return *this;
}

QChar QChar::upper() const
{
    CFMutableStringRef scratchUniCharString = GetScratchUniCharString();
    if (scratchUniCharString) {
        scratchUniChar = c;
        CFStringUppercase(scratchUniCharString, NULL);
        if (scratchUniChar) {
            return QChar(scratchUniChar);
        }
    }
    return *this;
}

QChar::Direction QChar::direction() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    return DirL;
}

bool QChar::mirrored() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    // return whether character should be reversed if text direction is
    // reversed
    return FALSE;
}

QChar QChar::mirroredChar() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    // return mirrored character if it is mirrored else return itself
    return *this;
}

int QChar::digitValue() const
{
    // ##### just latin1
    if ( c < '0' || c > '9' )
	return -1;
    else
	return c - '0';
}

// operators -------------------------------------------------------------------

#ifndef _KWQ_QCHAR_INLINES_

bool operator==(QChar qc1, QChar qc2)
{
    return qc1.c == qc2.c;
}

bool operator==(QChar qc, char ch)
{
    return qc.c == (uchar) ch;
}

bool operator==(char ch, QChar qc)
{
    return (uchar) ch == qc.c;
}

bool operator!=(QChar qc1, QChar qc2)
{
    return qc1.c != qc2.c;
}

bool operator!=(QChar qc, char ch)
{
    return qc.c != (uchar) ch;
}

bool operator!=(char ch, QChar qc)
{
    return (uchar) ch != qc.c;
}

bool operator>=(QChar qc1, QChar qc2)
{
    return qc1.c >= qc2.c;
}

bool operator>=(QChar qc, char ch)
{
    return qc.c >= (uchar) ch;
}

bool operator>=(char ch, QChar qc)
{
    return (uchar) ch >= qc.c;
}

bool operator>(QChar qc1, QChar qc2)
{
    return qc1.c > qc2.c;
}

bool operator>(QChar qc, char ch)
{
    return qc.c > ch;
}

bool operator>(char ch, QChar qc)
{
    return (uchar) ch > qc.c;
}

bool operator<=(QChar qc1, QChar qc2)
{
    return qc1.c <= qc2.c;
}

bool operator<=(QChar qc, char ch)
{
    return qc.c <= (uchar) ch;
}

bool operator<=(char ch, QChar qc)
{
    return (uchar) ch <= qc.c;
}

bool operator<(QChar qc1, QChar qc2)
{
    return qc1.c < qc2.c;
}

bool operator<(QChar qc, char ch)
{
    return qc.c < (uchar) ch;
}

bool operator<(char ch, QChar qc)
{
    return (uchar) ch < qc.c;
}

#endif  // _KWQ_QCHAR_INLINES_

#else // USING_BORROWED_QSTRING
// This will help to keep the linker from complaining about empty archives
void KWQChar_Dummy() {}
#endif // USING_BORROWED_QSTRING
