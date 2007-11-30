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

#ifndef TextEncoding_h
#define TextEncoding_h

#include <wtf/unicode/Unicode.h>

namespace WebCore {

    class CString;
    class String;

    class TextEncoding {
    public:
        TextEncoding() : m_name(0) { }
        TextEncoding(const char* name);
        TextEncoding(const String& name);

        bool isValid() const { return m_name; }
        const char* name() const { return m_name; }
        bool usesVisualOrdering() const;
        bool isJapanese() const;
        UChar backslashAsCurrencySymbol() const;
        const TextEncoding& closest8BitEquivalent() const;

        String decode(const char*, size_t length) const;
        CString encode(const UChar*, size_t length, bool allowEntities = false) const;

    private:
        const char* m_name;
    };

    inline bool operator==(const TextEncoding& a, const TextEncoding& b) { return a.name() == b.name(); }
    inline bool operator!=(const TextEncoding& a, const TextEncoding& b) { return a.name() != b.name(); }

    const TextEncoding& ASCIIEncoding();
    const TextEncoding& Latin1Encoding();
    const TextEncoding& UTF16BigEndianEncoding();
    const TextEncoding& UTF16LittleEndianEncoding();
    const TextEncoding& UTF32BigEndianEncoding();
    const TextEncoding& UTF32LittleEndianEncoding();
    const TextEncoding& UTF8Encoding();
    const TextEncoding& WindowsLatin1Encoding();

} // namespace WebCore

#endif // TextEncoding_h
