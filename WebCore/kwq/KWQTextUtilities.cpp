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

void KWQFindWordBoundary(const QChar *chars, int len, int position, int *start, int *end)
{
    TextBreakLocatorRef breakLocator;
    OSStatus status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakWordMask, &breakLocator);
    if (status == noErr) {
        UniCharArrayOffset startOffset, endOffset;
        if (position < len) {
            status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, kUCTextBreakLeadingEdgeMask, reinterpret_cast<const UniChar *>(chars), len, position, &endOffset);
        } else {
            // UCFindTextBreak treats this case as ParamErr
            endOffset = len;
        }
        if (status == noErr) {
            if (position > 0) {
                status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, kUCTextBreakGoBackwardsMask, reinterpret_cast<const UniChar *>(chars), len, position, &startOffset);
            } else {
                // UCFindTextBreak treats this case as ParamErr
                startOffset = 0;
            }
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
        while (pos >= 0 && chars[pos].isSpace())
            pos--;
        *start = pos+1;
        pos = position;
        while (pos < (int)len && chars[pos].isSpace())
            pos++;
        *end = pos;
    } else if (chars[position].isPunct()) {
        int pos = position;
        while (pos >= 0 && chars[pos].isPunct())
            pos--;
        *start = pos+1;
        pos = position;
        while (pos < (int)len && chars[pos].isPunct())
            pos++;
        *end = pos;
    } else {
        int pos = position;
        while (pos >= 0 && !chars[pos].isSpace() && !chars[pos].isPunct())
            pos--;
        *start = pos+1;
        pos = position;
        while (pos < (int)len && !chars[pos].isSpace() && !chars[pos].isPunct())
            pos++;
        *end = pos;
    }
}
