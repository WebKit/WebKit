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

#import "KWQTextUtilities.h"

#import <qstring.h>

#import <CoreServices/CoreServices.h>

void KWQFindWordBoundary(QChar *chars, int len, int position, int *start, int *end)
{
    TextBreakLocatorRef breakLocator;
    OSStatus status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakWordMask, &breakLocator);
    if (status == noErr) {
        UniCharArrayOffset startOffset, endOffset;
        status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, 0, (const UniChar *)chars, len, position, &endOffset);
        if (status == noErr) {
            status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, kUCTextBreakGoBackwardsMask, (const UniChar *)chars, len, position, &startOffset);
        }
        UCDisposeTextBreakLocator(&breakLocator);
        if (status == noErr) {
            *start = startOffset;
            *end = endOffset;
            return;
        }
    }
    
    // If Carbon fails (why would it?), do a simple space/punctuation boundary check.
    if (chars[position].isSpace()) {
        int pos = position;
        while (chars[pos].isSpace() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (chars[pos].isSpace() && pos < (int)len)
            pos++;
        *end = pos;
    } else if (chars[position].isPunct()) {
        int pos = position;
        while (chars[pos].isPunct() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (chars[pos].isPunct() && pos < (int)len)
            pos++;
        *end = pos;
    } else {
        int pos = position;
        while (!chars[pos].isSpace() && !chars[pos].isPunct() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (!chars[pos].isSpace() && !chars[pos].isPunct() && pos < (int)len)
            pos++;
        *end = pos;
    }
}
