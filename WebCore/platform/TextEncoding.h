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

#ifndef TextEncoding_H
#define TextEncoding_H

#include "DeprecatedString.h"

namespace WebCore {

    class StreamingTextDecoder;

#ifdef __APPLE__
    typedef CFStringEncoding TextEncodingID;
    
    const TextEncodingID InvalidEncoding = kCFStringEncodingInvalidId;
    const TextEncodingID UTF8Encoding = kCFStringEncodingUTF8;
    const TextEncodingID UTF16Encoding = kCFStringEncodingUnicode;
    const TextEncodingID Latin1Encoding = kCFStringEncodingISOLatin1;
    const TextEncodingID ASCIIEncoding = kCFStringEncodingASCII;
    const TextEncodingID WinLatin1Encoding = kCFStringEncodingWindowsLatin1;
#else
    enum TextEncodingID {
        InvalidEncoding = -1,
        UTF8Encoding,
        UTF16Encoding,
        Latin1Encoding,
        ASCIIEncoding,
        WinLatin1Encoding
    };
#endif

    enum TextEncodingFlags {
        NoEncodingFlags = 0,
        VisualOrdering = 1,
        BigEndian = 2,
        LittleEndian = 4,
        IsJapanese = 8,
        BackslashIsYen = 16
    };

    class TextEncoding {
    public:
        enum { 
            EightBitOnly = true 
        };

        explicit TextEncoding(TextEncodingID encodingID, TextEncodingFlags flags = NoEncodingFlags) 
            : m_encodingID(encodingID)
            , m_flags(flags) 
        { 
        }

        explicit TextEncoding(const char*, bool eightBitOnly = false);

        bool isValid() const { return m_encodingID != InvalidEncoding; }
        const char* name() const;
        bool usesVisualOrdering() const { return m_flags & VisualOrdering; }
        bool isJapanese() const { return m_flags & IsJapanese; }
        
        UChar backslashAsCurrencySymbol() const;
        
        DeprecatedCString fromUnicode(const DeprecatedString&, bool allowEntities = false) const;

        DeprecatedString toUnicode(const char*, int length) const;
        DeprecatedString toUnicode(const DeprecatedByteArray&, int length) const;

        TextEncodingID encodingID() const { return m_encodingID; }
        TextEncodingFlags flags() const { return m_flags; }
        
    private:
        TextEncodingID m_encodingID;
        TextEncodingFlags m_flags;
    };

    inline bool operator==(const TextEncoding& a, const TextEncoding& b)
    {
        return a.encodingID() == b.encodingID() && a.flags() == b.flags();
    }
    
    inline bool operator!=(const TextEncoding& a, const TextEncoding& b)
    {
        return a.encodingID() != b.encodingID() || a.flags() != b.flags();
    }
    
} // namespace WebCore

#endif // TextEncoding_H
