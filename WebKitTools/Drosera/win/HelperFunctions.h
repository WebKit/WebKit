/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HelperFunctions_H
#define HelperFunctions_H

#include "ole2.h" // For BSTR

#include <wtf/RetainPtr.h>

// These functions are actually defined somewhere else but could not be linked for one reason or another.

// This function was copied from WebCore BString.cpp I would lik to it if I could
BSTR cfStringToBSTR(CFStringRef cfstr)
{
    BSTR bstr;
    if (!cfstr)
        return 0;

    const UniChar* uniChars = CFStringGetCharactersPtr(cfstr);
    if (uniChars) {
        bstr = SysAllocStringLen((LPCTSTR)uniChars, CFStringGetLength(cfstr));

        return bstr;
    }

    CFIndex length = CFStringGetLength(cfstr);
    bstr = SysAllocStringLen(0, length);
    CFStringGetCharacters(cfstr, CFRangeMake(0, length), (UniChar*)bstr);
    bstr[length] = 0;

    return bstr;
}

//FIXME This is a class I believe I will need in the future but if not should be removed.
// It will include a Smart JSStringRef ptr
//#include <JavaScriptCore/JSStringRef.h>

#endif //HelperFunctions_H

