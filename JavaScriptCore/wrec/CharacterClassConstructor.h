/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CharacterClassConstructor_h
#define CharacterClassConstructor_h

#include <wtf/Platform.h>

#if ENABLE(WREC)

#include "CharacterClass.h"
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace JSC { namespace WREC {

    class CharacterClassConstructor {
    public:
        CharacterClassConstructor(bool isCaseInsensitive)
            : m_charBuffer(-1)
            , m_isPendingDash(false)
            , m_isCaseInsensitive(isCaseInsensitive)
            , m_isUpsideDown(false)
        {
        }

        void flush();
        
        // We need to flush prior to an escaped hyphen to prevent it as being treated as indicating
        // a range, e.g. [a\-c] we flush prior to adding the hyphen so that this is not treated as
        // [a-c].  However, we do not want to flush if we have already seen a non escaped hyphen -
        // e.g. [+-\-] should be treated the same as [+--], producing a range that will also match
        // a comma.
        void flushBeforeEscapedHyphen()
        {
            if (!m_isPendingDash)
                flush();
        }
        
        void put(UChar ch);
        void append(const CharacterClass& other);

        bool isUpsideDown() { return m_isUpsideDown; }

        ALWAYS_INLINE CharacterClass charClass()
        {
            CharacterClass newCharClass = {
                m_matches.begin(), m_matches.size(),
                m_ranges.begin(), m_ranges.size(),
                m_matchesUnicode.begin(), m_matchesUnicode.size(),
                m_rangesUnicode.begin(), m_rangesUnicode.size(),
            };

            return newCharClass;
        }

    private:
        void addSorted(Vector<UChar>& matches, UChar ch);
        void addSortedRange(Vector<CharacterRange>& ranges, UChar lo, UChar hi);

        int m_charBuffer;
        bool m_isPendingDash;
        bool m_isCaseInsensitive;
        bool m_isUpsideDown;

        Vector<UChar> m_matches;
        Vector<CharacterRange> m_ranges;
        Vector<UChar> m_matchesUnicode;
        Vector<CharacterRange> m_rangesUnicode;
    };

} } // namespace JSC::WREC

#endif // ENABLE(WREC)

#endif // CharacterClassConstructor_h
