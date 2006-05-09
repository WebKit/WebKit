/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "DeprecatedString.h"

#import <wtf/Assertions.h>
#import <Foundation/Foundation.h>
#import "TextEncoding.h"

using namespace WebCore;

void DeprecatedString::setBufferFromCFString(CFStringRef cfs)
{
    if (!cfs) {
        return;
    }
    CFIndex size = CFStringGetLength(cfs);
    UniChar fixedSizeBuffer[1024];
    UniChar *buffer;
    if (size > (CFIndex)(sizeof(fixedSizeBuffer) / sizeof(UniChar))) {
        buffer = (UniChar *)fastMalloc(size * sizeof(UniChar));
    } else {
        buffer = fixedSizeBuffer;
    }
    CFStringGetCharacters(cfs, CFRangeMake (0, size), buffer);
    setUnicode((const QChar *)buffer, (unsigned)size);
    if (buffer != fixedSizeBuffer) {
        fastFree(buffer);
    }
}


DeprecatedString DeprecatedString::fromCFString(CFStringRef cfs)
{
    DeprecatedString qs;
    qs.setBufferFromCFString(cfs);
    return qs;
}

DeprecatedString DeprecatedString::fromNSString(NSString *nss)
{
    DeprecatedString qs;
    qs.setBufferFromCFString((CFStringRef)nss);
    return qs;
}

NSString *DeprecatedString::getNSString() const
{
    // The Cocoa calls in this method don't need exceptions blocked
    // because they are simple NSString calls that can't throw.
    
    int length = dataHandle[0]->_length;
    if (dataHandle[0]->_isUnicodeValid) {
        return [NSString stringWithCharacters:(const unichar *)unicode() length:length];
    }
    
    if (dataHandle[0]->_isAsciiValid) {
        return [[[NSString alloc] initWithBytes:ascii() length:length encoding:NSISOLatin1StringEncoding] autorelease];
    }
    
    FATAL("invalid character cache");
    return nil;
}
