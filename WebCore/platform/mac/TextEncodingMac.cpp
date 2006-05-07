/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "TextEncoding.h"

#include <kxmlcore/Assertions.h>
#include <kxmlcore/HashSet.h>
#include "StreamingTextDecoder.h"

namespace WebCore {

static inline TextEncodingID effectiveEncoding(TextEncodingID encoding)
{
    if (encoding == Latin1Encoding || encoding == ASCIIEncoding)
        return WinLatin1Encoding;
    return encoding;
}

// We'd like to use ICU for this on OS X as well eventually, but we need to make sure
// it covers all the encodings that we need
DeprecatedCString TextEncoding::fromUnicode(const DeprecatedString &qcs, bool allowEntities) const
{
    // FIXME: We should really use the same API in both directions.
    // Currently we use ICU to decode and CFString to encode; it would be better to encode with ICU too.
    
    TextEncodingID encoding = effectiveEncoding(m_encodingID);

    // FIXME: Since there's no "force ASCII range" mode in CFString, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    DeprecatedString copy = qcs;
    copy.replace(QChar('\\'), backslashAsCurrencySymbol());
    CFStringRef cfs = copy.getCFString();
    CFMutableStringRef cfms = CFStringCreateMutableCopy(0, 0, cfs); // in rare cases, normalization can make the string longer, thus no limit on its length
    CFStringNormalize(cfms, kCFStringNormalizationFormC);
    
    CFIndex startPos = 0;
    CFIndex charactersLeft = CFStringGetLength(cfms);
    DeprecatedCString result(1); // for trailing zero

    while (charactersLeft > 0) {
        CFRange range = CFRangeMake(startPos, charactersLeft);
        CFIndex bufferLength;
        CFStringGetBytes(cfms, range, encoding, allowEntities ? 0 : '?', false, NULL, 0x7FFFFFFF, &bufferLength);
        
        DeprecatedCString chunk(bufferLength + 1);
        unsigned char *buffer = reinterpret_cast<unsigned char *>(chunk.data());
        CFIndex charactersConverted = CFStringGetBytes(cfms, range, encoding, allowEntities ? 0 : '?', false, buffer, bufferLength, &bufferLength);
        buffer[bufferLength] = 0;
        result.append(chunk);
        
        if (charactersConverted != charactersLeft) {
            unsigned int badChar = CFStringGetCharacterAtIndex(cfms, startPos + charactersConverted);
            ++charactersConverted;

            if ((badChar & 0xfc00) == 0xd800 &&     // is high surrogate
                  charactersConverted != charactersLeft) {
                UniChar low = CFStringGetCharacterAtIndex(cfms, startPos + charactersConverted);
                if ((low & 0xfc00) == 0xdc00) {     // is low surrogate
                    badChar <<= 10;
                    badChar += low;
                    badChar += 0x10000 - (0xd800 << 10) - 0xdc00;
                    ++charactersConverted;
                }
            }
            char buf[16];
            sprintf(buf, "&#%u;", badChar);
            result.append(buf);
        }
        
        startPos += charactersConverted;
        charactersLeft -= charactersConverted;
    }
    CFRelease(cfms);
    return result;
}

} // namespace WebCore
