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

#import <qstring.h>
#import <kwqdebug.h>
#import <Foundation/Foundation.h>

static UniChar scratchUniChar;

static CFMutableStringRef GetScratchUniCharString()
{
    static CFMutableStringRef s = NULL;
    if (!s)
        s = CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault, &scratchUniChar, 1, 1, kCFAllocatorNull);
    return s;
}

const QChar QChar::null;

bool QChar::isSpace() const
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
    return CFCharacterSetIsCharacterMember(set, c);
}

bool QChar::isDigit() const
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit);
    return CFCharacterSetIsCharacterMember(set, c);
}

bool QChar::isLetter() const
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetLetter);
    return CFCharacterSetIsCharacterMember(set, c);
}

bool QChar::isNumber() const
{
    return isLetterOrNumber() && !isLetter();
}

bool QChar::isLetterOrNumber() const
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetAlphaNumeric);
    return CFCharacterSetIsCharacterMember(set, c);
}

bool QChar::isPunct() const
{
    static CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetPunctuation);
    return CFCharacterSetIsCharacterMember(set, c);
}

QChar QChar::lower() const
{
    scratchUniChar = c;
    CFStringLowercase(GetScratchUniCharString(), NULL);
    return scratchUniChar;
}

QChar QChar::upper() const
{
    scratchUniChar = c;
    CFStringUppercase(GetScratchUniCharString(), NULL);
    return scratchUniChar;
}

QChar::Direction QChar::direction() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    if (c == ' ')
        return DirWS;
    return DirL;
}

bool QChar::mirrored() const
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    // return whether character should be reversed if text direction is reversed
    return false;
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
    if (c < '0' || c > '9')
	return -1;
    else
	return c - '0';
}
