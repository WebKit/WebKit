/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CharacterClassConstructor.h"

#if ENABLE(WREC)

#include "pcre_internal.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

namespace JSC { namespace WREC {

void CharacterClassConstructor::addSorted(Vector<UChar>& matches, UChar ch)
{
    unsigned pos = 0;
    unsigned range = matches.size();

    // binary chop, find position to insert char.
    while (range) {
        unsigned index = range >> 1;

        int val = matches[pos+index] - ch;
        if (!val)
            return;
        else if (val > 0)
            range = index;
        else {
            pos += (index+1);
            range -= (index+1);
        }
    }
    
    if (pos == matches.size())
        matches.append(ch);
    else
        matches.insert(pos, ch);
}

void CharacterClassConstructor::addSortedRange(Vector<CharacterRange>& ranges, UChar lo, UChar hi)
{
    unsigned end = ranges.size();
    
    // Simple linear scan - I doubt there are that many ranges anyway...
    // feel free to fix this with something faster (eg binary chop).
    for (unsigned i = 0; i < end; ++i) {
        // does the new range fall before the current position in the array
        if (hi < ranges[i].begin) {
            // optional optimization: concatenate appending ranges? - may not be worthwhile.
            if (hi == (ranges[i].begin - 1)) {
                ranges[i].begin = lo;
                return;
            }
            CharacterRange r = {lo, hi};
            ranges.insert(i, r);
            return;
        }
        // Okay, since we didn't hit the last case, the end of the new range is definitely at or after the begining
        // If the new range start at or before the end of the last range, then the overlap (if it starts one after the
        // end of the last range they concatenate, which is just as good.
        if (lo <= (ranges[i].end + 1)) {
            // found an intersect! we'll replace this entry in the array.
            ranges[i].begin = std::min(ranges[i].begin, lo);
            ranges[i].end = std::max(ranges[i].end, hi);

            // now check if the new range can subsume any subsequent ranges.
            unsigned next = i+1;
            // each iteration of the loop we will either remove something from the list, or break the loop.
            while (next < ranges.size()) {
                if (ranges[next].begin <= (ranges[i].end + 1)) {
                    // the next entry now overlaps / concatenates this one.
                    ranges[i].end = std::max(ranges[i].end, ranges[next].end);
                    ranges.remove(next);
                } else
                    break;
            }
            
            return;
        }
    }

    // CharacterRange comes after all existing ranges.
    CharacterRange r = {lo, hi};
    ranges.append(r);
}

void CharacterClassConstructor::put(UChar ch)
{
    // Parsing a regular expression like [a-z], we start in an initial empty state:
    //     ((m_charBuffer == -1) && !m_isPendingDash)
    // When buffer the 'a' sice it may be (and is in this case) part of a range:
    //     ((m_charBuffer != -1) && !m_isPendingDash)
    // Having parsed the hyphen we then record that the dash is also pending:
    //     ((m_charBuffer != -1) && m_isPendingDash)
    // The next change will always take us back to the initial state - either because
    // a complete range has been parsed (such as [a-z]), or because a flush is forced,
    // due to an early end in the regexp ([a-]), or a character class escape being added
    // ([a-\s]).  The fourth permutation of m_charBuffer and m_isPendingDash is not permitted.
    ASSERT(!((m_charBuffer == -1) && m_isPendingDash));

    if (m_charBuffer != -1) {
        if (m_isPendingDash) {
            // EXAMPLE: parsing [-a-c], the 'c' reaches this case - we have buffered a previous character and seen a hyphen, so this is a range.
            UChar lo = m_charBuffer;
            UChar hi = ch;
            // Reset back to the inital state.
            m_charBuffer = -1;
            m_isPendingDash = false;
            
            // This is an error, detected lazily.  Do not proceed.
            if (lo > hi) {
                m_isUpsideDown = true;
                return;
            }
            
            if (lo <= 0x7f) {
                char asciiLo = lo;
                char asciiHi = std::min(hi, (UChar)0x7f);
                addSortedRange(m_ranges, lo, asciiHi);
                
                if (m_isCaseInsensitive) {
                    if ((asciiLo <= 'Z') && (asciiHi >= 'A'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'A')+('a'-'A'), std::min(asciiHi, 'Z')+('a'-'A'));
                    if ((asciiLo <= 'z') && (asciiHi >= 'a'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'a')+('A'-'a'), std::min(asciiHi, 'z')+('A'-'a'));
                }
            }
            if (hi >= 0x80) {
                UChar unicodeCurr = std::max(lo, (UChar)0x80);
                addSortedRange(m_rangesUnicode, unicodeCurr, hi);
                
                if (m_isCaseInsensitive) {
                    // we're going to scan along, updating the start of the range
                    while (unicodeCurr <= hi) {
                        // Spin forwards over any characters that don't have two cases.
                        for (; jsc_pcre_ucp_othercase(unicodeCurr) == -1; ++unicodeCurr) {
                            // if this was the last character in the range, we're done.
                            if (unicodeCurr == hi)
                                return;
                        }
                        // if we fall through to here, unicodeCurr <= hi & has another case. Get the other case.
                        UChar rangeStart = unicodeCurr;
                        UChar otherCurr = jsc_pcre_ucp_othercase(unicodeCurr);
                        
                        // If unicodeCurr is not yet hi, check the next char in the range.  If it also has another case,
                        // and if it's other case value is one greater then the othercase value for the current last
                        // character included in the range, we can include next into the range.
                        while ((unicodeCurr < hi) && (jsc_pcre_ucp_othercase(unicodeCurr + 1) == (otherCurr + 1))) {
                            // increment unicodeCurr; it points to the end of the range.
                            // increment otherCurr, due to the check above other for next must be 1 greater than the currrent other value.
                            ++unicodeCurr;
                            ++otherCurr;
                        }
                        
                        // otherChar is the last in the range of other case chars, calculate offset to get back to the start.
                        addSortedRange(m_rangesUnicode, otherCurr-(unicodeCurr-rangeStart), otherCurr);
                        
                        // unicodeCurr has been added, move on to the next char.
                        ++unicodeCurr;
                    }
                }
            }
        } else if (ch == '-')
            // EXAMPLE: parsing [-a-c], the second '-' reaches this case - the hyphen is treated as potentially indicating a range.
            m_isPendingDash = true;
        else {
            // EXAMPLE: Parsing [-a-c], the 'a' reaches this case - we repace the previously buffered char with the 'a'.
            flush();
            m_charBuffer = ch;
        }
    } else
        // EXAMPLE: Parsing [-a-c], the first hyphen reaches this case - there is no buffered character
        // (the hyphen not treated as a special character in this case, same handling for any char).
        m_charBuffer = ch;
}

// When a character is added to the set we do not immediately add it to the arrays, in case it is actually defining a range.
// When we have determined the character is not used in specifing a range it is added, in a sorted fashion, to the appropriate
// array (either ascii or unicode).
// If the pattern is case insensitive we add entries for both cases.
void CharacterClassConstructor::flush()
{
    if (m_charBuffer != -1) {
        if (m_charBuffer <= 0x7f) {
            if (m_isCaseInsensitive && isASCIILower(m_charBuffer))
                addSorted(m_matches, toASCIIUpper(m_charBuffer));
            addSorted(m_matches, m_charBuffer);
            if (m_isCaseInsensitive && isASCIIUpper(m_charBuffer))
                addSorted(m_matches, toASCIILower(m_charBuffer));
        } else {
            addSorted(m_matchesUnicode, m_charBuffer);
            if (m_isCaseInsensitive) {
                int other = jsc_pcre_ucp_othercase(m_charBuffer);
                if (other != -1)
                    addSorted(m_matchesUnicode, other);
            }
        }
        m_charBuffer = -1;
    }
    
    if (m_isPendingDash) {
        addSorted(m_matches, '-');
        m_isPendingDash = false;
    }
}

void CharacterClassConstructor::append(const CharacterClass& other)
{
    // [x-\s] will add, 'x', '-', and all unicode spaces to new class (same as [x\s-]).
    // Need to check the spec, really, but think this matches PCRE behaviour.
    flush();
    
    if (other.numMatches) {
        for (size_t i = 0; i < other.numMatches; ++i)
            addSorted(m_matches, other.matches[i]);
    }
    if (other.numRanges) {
        for (size_t i = 0; i < other.numRanges; ++i)
            addSortedRange(m_ranges, other.ranges[i].begin, other.ranges[i].end);
    }
    if (other.numMatchesUnicode) {
        for (size_t i = 0; i < other.numMatchesUnicode; ++i)
            addSorted(m_matchesUnicode, other.matchesUnicode[i]);
    }
    if (other.numRangesUnicode) {
        for (size_t i = 0; i < other.numRangesUnicode; ++i)
            addSortedRange(m_rangesUnicode, other.rangesUnicode[i].begin, other.rangesUnicode[i].end);
    }
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
