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

#include "config.h"
#include "WREC.h"

#if ENABLE(WREC)

#include "ExecState.h"
#include "Machine.h"
#include "pcre_internal.h"

using namespace WTF;

namespace KJS {

// ==== CharacterClass ====

struct CharacterClassRange {
    UChar begin;
    UChar end;
};

struct CharacterClass {
    const UChar* m_matches;
    unsigned m_numMatches;
    const CharacterClassRange* m_ranges;
    unsigned m_numRanges;
    const UChar* m_matchesUnicode;
    unsigned m_numMatchesUnicode;
    const CharacterClassRange* m_rangesUnicode;
    unsigned m_numRangesUnicode;
};

static const UChar ascii_newlines[2] = { '\n', '\r' };
static const UChar unicode_newlines[2] = { 0x2028, 0x2029 };
static CharacterClass& getCharacterClass_newline() {
    static CharacterClass charClass = {
        ascii_newlines, 2,
        0, 0,
        unicode_newlines, 2,
        0, 0,
    };
    
    return charClass;
}

static const CharacterClassRange ascii_digits_range[1] = { { '0', '9' } };
static CharacterClass& getCharacterClass_d() {
    static CharacterClass charClass = {
        0, 0,
        ascii_digits_range, 1,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

static const UChar ascii_spaces[1] = { ' ' };
static const CharacterClassRange ascii_spaces_range[1] = { { '\t', '\r' } };
static const UChar unicode_spaces[8] = { 0x00a0, 0x1680, 0x180e, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000 };
static const CharacterClassRange unicode_spaces_range[1] = { { 0x2000, 0x200a } };
static CharacterClass& getCharacterClass_s() {
    static CharacterClass charClass = {
        ascii_spaces, 1,
        ascii_spaces_range, 1,
        unicode_spaces, 8,
        unicode_spaces_range, 1,
    };
    
    return charClass;
}

static const UChar ascii_wordchar[1] = { '_' };
static const CharacterClassRange ascii_wordchar_range[3] = { { '0', '9' }, { 'A', 'Z' }, { 'a', 'z' } };
static CharacterClass& getCharacterClass_w() {
    static CharacterClass charClass = {
        ascii_wordchar, 1,
        ascii_wordchar_range, 3,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

static const CharacterClassRange ascii_nondigits_range[2] = { { 0, '0' - 1 }, { '9' + 1, 0x7f } };
static const CharacterClassRange unicode_nondigits_range[1] = { { 0x0080, 0xffff } };
static CharacterClass& getCharacterClass_D() {
    static CharacterClass charClass = {
        0, 0,
        ascii_nondigits_range, 2,
        0, 0,
        unicode_nondigits_range, 1,
    };
    
    return charClass;
}

static const CharacterClassRange ascii_nonspaces_range[3] = { { 0, '\t' - 1 }, { '\r' + 1, ' ' - 1 }, { ' ' + 1, 0x7f } }; 
static const CharacterClassRange unicode_nonspaces_range[9] = {
    { 0x0080, 0x009f },
    { 0x00a1, 0x167f },
    { 0x1681, 0x180d },
    { 0x180f, 0x1fff },
    { 0x200b, 0x2027 },
    { 0x202a, 0x202e },
    { 0x2030, 0x205e },
    { 0x2060, 0x2fff },
    { 0x3001, 0xffff }
}; 
static CharacterClass& getCharacterClass_S() {
    static CharacterClass charClass = {
        0, 0,
        ascii_nonspaces_range, 3,
        0, 0,
        unicode_nonspaces_range, 9,
    };
    
    return charClass;
}

static const UChar ascii_nonwordchar[1] = { '`' };
static const CharacterClassRange ascii_nonwordchar_range[4] = { { 0, '0' - 1 }, { '9' + 1, 'A' - 1 }, { 'Z' + 1, '_' - 1 }, { 'z' + 1, 0x7f } };
static const CharacterClassRange unicode_nonwordchar_range[1] = { { 0x0080, 0xffff } };
static CharacterClass& getCharacterClass_W() {
    static CharacterClass charClass = {
        ascii_nonwordchar, 1,
        ascii_nonwordchar_range, 4,
        0, 0,
        unicode_nonwordchar_range, 1,
    };
    
    return charClass;
}

struct CharacterClassConstructor {
    Vector<UChar> m_matches;
    Vector<CharacterClassRange> m_ranges;
    Vector<UChar> m_matchesUnicode;
    Vector<CharacterClassRange> m_rangesUnicode;

    int m_ch_buffer;
    bool m_pending_dash;
    bool m_ignoreCase;
    bool m_upsideDown;
    
    CharacterClassConstructor(bool ignoreCase)
        : m_ch_buffer(-1)
        , m_pending_dash(false)
        , m_ignoreCase(ignoreCase)
        , m_upsideDown(false)
    {
    }
    
    void flush();
    void put(UChar ch);
    void append(CharacterClass& other);
private:
    void addSorted(Vector<UChar>& matches, UChar ch);
    void addSortedRange(Vector<CharacterClassRange>& ranges, UChar lo, UChar hi);
};

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

void CharacterClassConstructor::addSortedRange(Vector<CharacterClassRange>& ranges, UChar lo, UChar hi)
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
            CharacterClassRange r = {lo, hi};
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

    // Range comes after all existing ranges.
    CharacterClassRange r = {lo, hi};
    ranges.append(r);
}

void CharacterClassConstructor::put(UChar ch)
{
    if (m_ch_buffer != -1) {
        if (m_pending_dash) {
            UChar lo = m_ch_buffer;
            UChar hi = ch;
            m_ch_buffer = -1;
            m_pending_dash = false;
            
            if (lo > hi)
                m_upsideDown = true;
            
            if (lo <= 0x7f) {
                char asciiLo = lo;
                char asciiHi = std::min(hi, (UChar)0x7f);
                addSortedRange(m_ranges, lo, asciiHi);
                
                if (m_ignoreCase) {
                    if ((asciiLo <= 'Z') && (asciiHi >= 'A'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'A')+('a'-'A'), std::min(asciiHi, 'Z')+('a'-'A'));
                    if ((asciiLo <= 'z') && (asciiHi >= 'a'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'a')+('A'-'a'), std::min(asciiHi, 'z')+('A'-'a'));
                }
            }
            if (hi >= 0x80) {
                UChar unicodeCurr = std::max(lo, (UChar)0x80);
                addSortedRange(m_rangesUnicode, unicodeCurr, hi);
                
                if (m_ignoreCase) {
                    // we're going to scan along, updating the start of the range
                    while (unicodeCurr <= hi) {
                        // Spin forwards over any characters that don't have two cases.
                        for (; kjs_pcre_ucp_othercase(unicodeCurr) == -1; ++unicodeCurr) {
                            // if this was the last character in the range, we're done.
                            if (unicodeCurr == hi)
                                return;
                        }
                        // if we fall through to here, unicodeCurr <= hi & has another case. Get the other case.
                        UChar rangeStart = unicodeCurr;
                        UChar otherCurr = kjs_pcre_ucp_othercase(unicodeCurr);
                        
                        // If unicodeCurr is not yet hi, check the next char in the range.  If it also has another case,
                        // and if it's other case value is one greater then the othercase value for the current last
                        // character included in the range, we can include next into the range.
                        while ((unicodeCurr < hi) && (kjs_pcre_ucp_othercase(unicodeCurr + 1) == (otherCurr + 1))) {
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
        } else if (ch == '-') {
            m_pending_dash = true;
        } else {
            flush();
            m_ch_buffer = ch;
        }
    } else
        m_ch_buffer = ch;
}

// When a character is added to the set we do not immediately add it to the arrays, in case it is actually defining a range.
// When we have determined the character is not used in specifing a range it is added, in a sorted fashion, to the appropriate
// array (either ascii or unicode).
// If the pattern is case insensitive we add entries for both cases.
void CharacterClassConstructor::flush()
{
    if (m_ch_buffer != -1) {
        if (m_ch_buffer <= 0x7f) {
            if (m_ignoreCase && isASCIILower(m_ch_buffer))
                addSorted(m_matches, toASCIIUpper(m_ch_buffer));
            addSorted(m_matches, m_ch_buffer);
            if (m_ignoreCase && isASCIIUpper(m_ch_buffer))
                addSorted(m_matches, toASCIILower(m_ch_buffer));
        } else {
            addSorted(m_matchesUnicode, m_ch_buffer);
            if (m_ignoreCase) {
                int other = kjs_pcre_ucp_othercase(m_ch_buffer);
                if (other != -1)
                    addSorted(m_matchesUnicode, other);
            }
        }
        m_ch_buffer = -1;
    }
    
    if (m_pending_dash) {
        addSorted(m_matches, '-');
    }
}

void CharacterClassConstructor::append(CharacterClass& other)
{
    // [x-\s] will add, 'x', '-', and all unicode spaces to new class (same as [x\s-]).
    // Need to check the spec, really, but think this matches PCRE behaviour.
    flush();
    
    if (other.m_numMatches) {
        for (size_t i = 0; i < other.m_numMatches; ++i)
            addSorted(m_matches, other.m_matches[i]);
    }
    if (other.m_numRanges) {
        for (size_t i = 0; i < other.m_numRanges; ++i)
            addSortedRange(m_ranges, other.m_ranges[i].begin, other.m_ranges[i].end);
    }
    if (other.m_numMatchesUnicode) {
        for (size_t i = 0; i < other.m_numMatchesUnicode; ++i)
            addSorted(m_matchesUnicode, other.m_matchesUnicode[i]);
    }
    if (other.m_numRangesUnicode) {
        for (size_t i = 0; i < other.m_numRangesUnicode; ++i)
            addSortedRange(m_rangesUnicode, other.m_rangesUnicode[i].begin, other.m_rangesUnicode[i].end);
    }
}

class GenerateAtomFunctor {
public:
    virtual ~GenerateAtomFunctor() {}

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&) = 0;
    virtual void backtrack(WRECGenerator*) = 0;
};

class GeneratePatternCharacterFunctor : public GenerateAtomFunctor {
public:
    GeneratePatternCharacterFunctor(const UChar ch)
        : m_ch(ch)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    const UChar m_ch;
};

class GenerateCharacterClassFunctor : public GenerateAtomFunctor {
public:
    GenerateCharacterClassFunctor(CharacterClass* charClass, bool invert)
        : m_charClass(charClass)
        , m_invert(invert)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    CharacterClass* m_charClass;
    bool m_invert;
};

class GenerateBackreferenceFunctor : public GenerateAtomFunctor {
public:
    GenerateBackreferenceFunctor(unsigned subpatternId)
        : m_subpatternId(subpatternId)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    unsigned m_subpatternId;
};

class GenerateParenthesesNonGreedyFunctor : public GenerateAtomFunctor {
public:
    GenerateParenthesesNonGreedyFunctor(WRECGenerator::JmpDst start, WRECGenerator::JmpSrc success, WRECGenerator::JmpSrc fail)
        : m_start(start)
        , m_success(success)
        , m_fail(fail)
    {
    }

    virtual void generateAtom(WRECGenerator*, JmpSrcVector&);
    virtual void backtrack(WRECGenerator*);

private:
    WRECGenerator::JmpDst m_start;
    WRECGenerator::JmpSrc m_success;
    WRECGenerator::JmpSrc m_fail;
};

void GeneratePatternCharacterFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generatePatternCharacter(failures, m_ch);
}
void GeneratePatternCharacterFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrack1();
}

void GenerateCharacterClassFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateCharacterClass(failures, *m_charClass, m_invert);
}
void GenerateCharacterClassFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrack1();
}

void GenerateBackreferenceFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateBackreference(failures, m_subpatternId);
}
void GenerateBackreferenceFunctor::backtrack(WRECGenerator* wrec)
{
    wrec->generateBacktrackBackreference(m_subpatternId);
}

void GenerateParenthesesNonGreedyFunctor::generateAtom(WRECGenerator* wrec, JmpSrcVector& failures)
{
    wrec->generateParenthesesNonGreedy(failures, m_start, m_success, m_fail);
}

void GenerateParenthesesNonGreedyFunctor::backtrack(WRECGenerator*)
{
    // FIXME: do something about this.
    CRASH();
}

void WRECGenerator::generateBacktrack1()
{
    m_jit.emitSubl_i8r(1, CURR_POS_REG);
}

void WRECGenerator::generateBacktrackBackreference(unsigned subpatternId)
{
    m_jit.emitSubl_mr((2 * subpatternId + 1) * sizeof(int), OUTPUT_REG, CURR_POS_REG);
    m_jit.emitAddl_mr((2 * subpatternId) * sizeof(int), OUTPUT_REG, CURR_POS_REG);
}

void WRECGenerator::generateBackreferenceQuantifier(JmpSrcVector& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max)
{
    GenerateBackreferenceFunctor functor(subpatternId);

    m_jit.emitMovl_mr((2 * subpatternId) * sizeof(int), OUTPUT_REG, CURR_VAL_REG);
    m_jit.emitCmpl_rm(CURR_VAL_REG, ((2 * subpatternId) + 1) * sizeof(int), OUTPUT_REG);
    JmpSrc skipIfEmpty = m_jit.emitUnlinkedJe();

    ASSERT(quantifierType == Quantifier::Greedy || quantifierType == Quantifier::NonGreedy);
    if (quantifierType == Quantifier::Greedy)
        generateGreedyQuantifier(failures, functor, min, max);
    else
        generateNonGreedyQuantifier(failures, functor, min, max);

    m_jit.link(skipIfEmpty, m_jit.label());
}

void WRECGenerator::generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init QUANTIFIER_COUNT_REG
    m_jit.emitPushl_r(QUANTIFIER_COUNT_REG);
    m_jit.emitXorl_rr(QUANTIFIER_COUNT_REG, QUANTIFIER_COUNT_REG);
    JmpSrc gotoStart = m_jit.emitUnlinkedJmp();

    // (4) Failure case

    JmpDst quantifierFailed = m_jit.label();
    // (4.1) Restore original value of QUANTIFIER_COUNT_REG from the stack
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    failures.append(m_jit.emitUnlinkedJmp()); 

    // (3) We just tried an alternative, and it failed - check we can try more.
    
    JmpDst alternativeFailed = m_jit.label();
    // (3.1) remove the value pushed prior to testing the alternative
    m_jit.emitPopl_r(CURR_POS_REG);
    // (3.2) if there is a limit, and we have reached it, game over. 
    if (max != Quantifier::NoMaxSpecified) {
        m_jit.emitCmpl_i32r(max, QUANTIFIER_COUNT_REG);
        m_jit.link(m_jit.emitUnlinkedJe(), quantifierFailed);
    }

    // (1) Do a check for the atom

    // (1.0) This is where we start, if there is a minimum (then we must read at least one of the atom).
    JmpDst testQuantifiedAtom = m_jit.label();
    if (min)
        m_jit.link(gotoStart, testQuantifiedAtom);
    // (1.1) Do a check for the atom check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    m_jit.emitAddl_i8r(1, QUANTIFIER_COUNT_REG);
    // (1.3) We needed to read the atom, and we failed - that's terminally  bad news.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], quantifierFailed);
    newFailures.clear();
    // (1.4) If there is a minimum, check we have read enough ...
    if (min) {
        // ... except if min was 1 no need to keep checking!
        if (min != 1) {
            m_jit.emitCmpl_i32r(min, QUANTIFIER_COUNT_REG);
            m_jit.link(m_jit.emitUnlinkedJl(), testQuantifiedAtom);
        }
    } else
        m_jit.link(gotoStart, m_jit.label());
    // if there was no minimum, this is where we start.

    // (2) Plant an alternative check for the remainder of the expression
    
    // (2.1) recursively call to parseAlternative, if it falls through, success!
    m_jit.emitPushl_r(CURR_POS_REG);
    m_parser.parseAlternative(newFailures);
    m_jit.emitAddl_i8r(4, IA32MacroAssembler::esp);
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    // (2.2) link failure cases to jump back up to alternativeFailed.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], alternativeFailed);
    newFailures.clear();
}

void WRECGenerator::generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    if (!max)
        return;

    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init QUANTIFIER_COUNT_REG
    m_jit.emitPushl_r(QUANTIFIER_COUNT_REG);
    m_jit.emitXorl_rr(QUANTIFIER_COUNT_REG, QUANTIFIER_COUNT_REG);

    // (1) Greedily read as many of the atom as possible

    JmpDst readMore = m_jit.label();

    // (1.1) Do a character class check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    m_jit.emitAddl_i8r(1, QUANTIFIER_COUNT_REG);
    // (1.3) loop, unless we've read m_max limit.
    if (max != Quantifier::NoMaxSpecified) {
        if (max != 1) {
            // if there is a limit, only loop while less than limit, otherwise fall throught to...
            m_jit.emitCmpl_i32r(max, QUANTIFIER_COUNT_REG);
            m_jit.link(m_jit.emitUnlinkedJne(), readMore);
        }
        // ...if there is no min we need jump to the alternative test, if there is we can just fall through to it.
        if (!min)
            newFailures.append(m_jit.emitUnlinkedJmp());
    } else
        m_jit.link(m_jit.emitUnlinkedJmp(), readMore);
    // (1.4) check enough matches to bother trying an alternative...
    if (min) {
        // We will fall through to here if (min && max), after the max check.
        // First, also link a
        JmpDst minimumTest = m_jit.label();
        for (unsigned i = 0; i < newFailures.size(); ++i)
            m_jit.link(newFailures[i], minimumTest);
        newFailures.clear();
        // 
        m_jit.emitCmpl_i32r(min, QUANTIFIER_COUNT_REG);
        newFailures.append(m_jit.emitUnlinkedJae());
    }

    // (4) Failure case

    JmpDst quantifierFailed = m_jit.label();
    // (4.1) Restore original value of QUANTIFIER_COUNT_REG from the stack
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    failures.append(m_jit.emitUnlinkedJmp()); 

    // (3) Backtrack

    JmpDst backtrack = m_jit.label();
    // (3.1) this was preserved prior to executing the alternative
    m_jit.emitPopl_r(CURR_POS_REG);
    // (3.2) check we can retry with fewer matches - backtracking fails if already at the minimum
    m_jit.emitCmpl_i32r(min, QUANTIFIER_COUNT_REG);
    m_jit.link(m_jit.emitUnlinkedJe(), quantifierFailed);
    // (3.3) roll off one match, and retry.
    functor.backtrack(this);
    m_jit.emitSubl_i8r(1, QUANTIFIER_COUNT_REG);

    // (2) Try an alternative.

    // (2.1) point to retry
    JmpDst tryAlternative = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], tryAlternative);
    newFailures.clear();
    // (2.2) recursively call to parseAlternative, if it falls through, success!
    m_jit.emitPushl_r(CURR_POS_REG);
    m_parser.parseAlternative(newFailures);
    m_jit.emitAddl_i8r(4, IA32MacroAssembler::esp);
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    // (2.3) link failure cases to here.
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], backtrack);
    newFailures.clear();
}

void WRECGenerator::generatePatternCharacter(JmpSrcVector& failures, int ch)
{
    // check there is more input, read a char.
    m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
    failures.append(m_jit.emitUnlinkedJe());
    m_jit.emitMovzwl_mr(INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);

    // used for unicode case insensitive
    bool hasUpper = false;
    JmpSrc isUpper;
    
    // if case insensitive match
    if (m_parser.m_ignoreCase) {
        UChar lower, upper;
        
        // check for ascii case sensitive characters
        if (isASCIIAlpha(ch)) {
            m_jit.emitOrl_i8r(32, CURR_VAL_REG);
            ch |= 32;
        } else if ((ch > 0x7f) && ((lower = Unicode::toLower(ch)) != (upper = Unicode::toUpper(ch)))) {
            // handle unicode case sentitive characters - branch to success on upper
            m_jit.emitCmpl_i32r(upper, CURR_VAL_REG);
            isUpper = m_jit.emitUnlinkedJne();
            hasUpper = true;
            ch = lower;
        }
    }
    
    // checks for ch, or lower case version of ch, if insensitive
    m_jit.emitCmpl_i32r((unsigned short)ch, CURR_VAL_REG);
    failures.append(m_jit.emitUnlinkedJne());

    if (m_parser.m_ignoreCase && hasUpper) {
        // for unicode case insensitive matches, branch here if upper matches.
        m_jit.link(isUpper, m_jit.label());
    }
    
    // on success consume the char
    m_jit.emitAddl_i8r(1, CURR_POS_REG);
}

void WRECGenerator::generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterClassRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
{
    do {
        // pick which range we're going to generate
        int which = count >> 1;
        char lo = ranges[which].begin;
        char hi = ranges[which].end;
        
        m_jit.emitCmpl_i32r((unsigned short)lo, CURR_VAL_REG);

        // check if there are any ranges or matches below lo.  If not, just jl to failure -
        // if there is anything else to check, check that first, if it falls through jmp to failure.
        if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
            JmpSrc loOrAbove = m_jit.emitUnlinkedJge();
            
            // generate code for all ranges before this one
            if (which)
                generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            
            do {
                m_jit.emitCmpl_i32r((unsigned short)matches[*matchIndex], CURR_VAL_REG);
                matchDest.append(m_jit.emitUnlinkedJe());
                ++*matchIndex;
            } while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo));
            failures.append(m_jit.emitUnlinkedJmp());

            m_jit.link(loOrAbove, m_jit.label());
        } else if (which) {
            JmpSrc loOrAbove = m_jit.emitUnlinkedJge();

            generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            failures.append(m_jit.emitUnlinkedJmp());

            m_jit.link(loOrAbove, m_jit.label());
        } else
            failures.append(m_jit.emitUnlinkedJl());

        while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
            ++*matchIndex;

        m_jit.emitCmpl_i32r((unsigned short)hi, CURR_VAL_REG);
        matchDest.append(m_jit.emitUnlinkedJle());
        // fall through to here, the value is above hi.

        // shuffle along & loop around if there are any more matches to handle.
        unsigned next = which + 1;
        ranges += next;
        count -= next;
    } while (count);
}

void WRECGenerator::generateCharacterClassInverted(JmpSrcVector& matchDest, CharacterClass& charClass)
{
    JmpSrc unicodeFail;
    if (charClass.m_numMatchesUnicode || charClass.m_numRangesUnicode) {
        m_jit.emitCmpl_i8r(0x7f, CURR_VAL_REG);
        JmpSrc isAscii = m_jit.emitUnlinkedJle();
    
        if (charClass.m_numMatchesUnicode) {
            for (unsigned i = 0; i < charClass.m_numMatchesUnicode; ++i) {
                UChar ch = charClass.m_matchesUnicode[i];
                m_jit.emitCmpl_i32r((unsigned short)ch, CURR_VAL_REG);
                matchDest.append(m_jit.emitUnlinkedJe());
            }
        }
        
        if (charClass.m_numRangesUnicode) {
            for (unsigned i = 0; i < charClass.m_numRangesUnicode; ++i) {
                UChar lo = charClass.m_rangesUnicode[i].begin;
                UChar hi = charClass.m_rangesUnicode[i].end;
                
                m_jit.emitCmpl_i32r((unsigned short)lo, CURR_VAL_REG);
                JmpSrc below = m_jit.emitUnlinkedJl();
                m_jit.emitCmpl_i32r((unsigned short)hi, CURR_VAL_REG);
                matchDest.append(m_jit.emitUnlinkedJle());
                m_jit.link(below, m_jit.label());
            }
        }

        unicodeFail = m_jit.emitUnlinkedJmp();
        m_jit.link(isAscii, m_jit.label());
    }

    if (charClass.m_numRanges) {
        unsigned matchIndex = 0;
        JmpSrcVector failures; 
        generateCharacterClassInvertedRange(failures, matchDest, charClass.m_ranges, charClass.m_numRanges, &matchIndex, charClass.m_matches, charClass.m_numMatches);
        while (matchIndex < charClass.m_numMatches) {
            m_jit.emitCmpl_i32r((unsigned short)charClass.m_matches[matchIndex], CURR_VAL_REG);
            matchDest.append(m_jit.emitUnlinkedJe());
            ++matchIndex;
        }
        JmpDst noMatch = m_jit.label();
        for (unsigned i = 0; i < failures.size(); ++i)
            m_jit.link(failures[i], noMatch);
        failures.clear();
    } else if (charClass.m_numMatches) {
        // optimization: gather 'a','A' etc back together, can mask & test once.
        Vector<char> matchesAZaz;

        for (unsigned i = 0; i < charClass.m_numMatches; ++i) {
            char ch = charClass.m_matches[i];
            if (m_parser.m_ignoreCase) {
                if (isASCIILower(ch)) {
                    matchesAZaz.append(ch);
                    continue;
                }
                if (isASCIIUpper(ch))
                    continue;
            }
            m_jit.emitCmpl_i32r((unsigned short)ch, CURR_VAL_REG);
            matchDest.append(m_jit.emitUnlinkedJe());
        }

        if (unsigned countAZaz = matchesAZaz.size()) {
            m_jit.emitOrl_i8r(32, CURR_VAL_REG);

            for (unsigned i = 0; i < countAZaz; ++i) {
                char ch = matchesAZaz[i];
                m_jit.emitCmpl_i32r((unsigned short)ch, CURR_VAL_REG);
                matchDest.append(m_jit.emitUnlinkedJe());
            }
        }
    }

    if (charClass.m_numMatchesUnicode || charClass.m_numRangesUnicode)
        m_jit.link(unicodeFail, m_jit.label());
}

void WRECGenerator::generateCharacterClass(JmpSrcVector& failures, CharacterClass& charClass, bool invert)
{
    m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
    failures.append(m_jit.emitUnlinkedJe());
    m_jit.emitMovzwl_mr(INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);

    if (invert)
        generateCharacterClassInverted(failures, charClass);
    else {
        JmpSrcVector successes;
        generateCharacterClassInverted(successes, charClass);
        failures.append(m_jit.emitUnlinkedJmp());
        JmpDst here = m_jit.label();
        for (unsigned i = 0; i < successes.size(); ++i)
            m_jit.link(successes[i], here);
    }
    
    m_jit.emitAddl_i8r(1, CURR_POS_REG);
}

WRECGenerator::JmpSrc WRECGenerator::generateParentheses(ParenthesesType type)
{
    unsigned subpatternId = (type == capturing) ? ++m_parser.m_numSubpatterns : m_parser.m_numSubpatterns;

    // push pos, both to preserve for fail + reloaded in parseDisjunction
    m_jit.emitPushl_r(CURR_POS_REG);

    // Plant a disjunction, wrapped to invert behaviour - 
    JmpSrcVector newFailures;
    m_parser.parseDisjunction(newFailures);
    
    if (type == capturing) {
        m_jit.emitPopl_r(CURR_VAL_REG);
        m_jit.emitMovl_rm(CURR_VAL_REG, (2 * subpatternId) * sizeof(int), OUTPUT_REG);
        m_jit.emitMovl_rm(CURR_POS_REG, (2 * subpatternId + 1) * sizeof(int), OUTPUT_REG);
    } else if (type == non_capturing)
        m_jit.emitAddl_i8r(4, IA32MacroAssembler::esp);
    else
        m_jit.emitPopl_r(CURR_POS_REG);

    // This is a little lame - jump to jump if there is a nested disjunction.
    // (suggestion to fix: make parseDisjunction populate a JmpSrcVector of
    // disjunct successes... this is probably not worth the compile cost in
    // the common case to fix).
    JmpSrc successfulMatch = m_jit.emitUnlinkedJmp();

    JmpDst originalFailure = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], originalFailure);
    newFailures.clear();
    // fail: restore CURR_POS_REG
    m_jit.emitPopl_r(CURR_POS_REG);

    JmpSrc jumpToFail;
    // If this was an inverted assert, fail = success! - just let the failure case drop through,
    // success case goes to failures.  Both paths restore curr pos.
    if (type == inverted_assertion)
        jumpToFail = successfulMatch;
    else {
        // plant a jump so any fail will link off to 'failures',
        jumpToFail = m_jit.emitUnlinkedJmp();
        // link successes to jump here
        m_jit.link(successfulMatch, m_jit.label());
    }
    return jumpToFail;
}

void WRECGenerator::generateParenthesesNonGreedy(JmpSrcVector& failures, JmpDst start, JmpSrc success, JmpSrc fail)
{
    m_jit.link(m_jit.emitUnlinkedJmp(), start);
    m_jit.link(success, m_jit.label());

    failures.append(fail);
}

WRECGenerator::JmpSrc WRECGenerator::gererateParenthesesResetTrampoline(JmpSrcVector& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter)
{
    JmpSrc skip = m_jit.emitUnlinkedJmp();

    JmpDst subPatternResetTrampoline = m_jit.label();
    for (unsigned i = 0; i < newFailures.size(); ++i)
        m_jit.link(newFailures[i], subPatternResetTrampoline);
    newFailures.clear();
    for (unsigned i = subpatternIdBefore + 1; i <= subpatternIdAfter; ++i) {
        m_jit.emitMovl_i32m(-1, (2 * i) * sizeof(int), OUTPUT_REG);
        m_jit.emitMovl_i32m(-1, (2 * i + 1) * sizeof(int), OUTPUT_REG);
    }
    
    JmpSrc newFailJump = m_jit.emitUnlinkedJmp();
    m_jit.link(skip, m_jit.label());
    
    return newFailJump;
}

void WRECGenerator::generateAssertionBOL(JmpSrcVector& failures)
{
    if (m_parser.m_multiline) {
        JmpSrcVector previousIsNewline;

        // begin of input == success
        m_jit.emitCmpl_i8r(0, CURR_POS_REG);
        previousIsNewline.append(m_jit.emitUnlinkedJe());

        // now check prev char against newline characters.
        m_jit.emitMovzwl_mr(-2, INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);
        generateCharacterClassInverted(previousIsNewline, getCharacterClass_newline());

        failures.append(m_jit.emitUnlinkedJmp());

        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < previousIsNewline.size(); ++i)
            m_jit.link(previousIsNewline[i], success);
        previousIsNewline.clear();
    } else {
        m_jit.emitCmpl_i8r(0, CURR_POS_REG);
        failures.append(m_jit.emitUnlinkedJne());
    }
}

void WRECGenerator::generateAssertionEOL(JmpSrcVector& failures)
{
    if (m_parser.m_multiline) {
        JmpSrcVector nextIsNewline;

        // end of input == success
        m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
        nextIsNewline.append(m_jit.emitUnlinkedJe());

        // now check next char against newline characters.
        m_jit.emitMovzwl_mr(INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);
        generateCharacterClassInverted(nextIsNewline, getCharacterClass_newline());

        failures.append(m_jit.emitUnlinkedJmp());

        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < nextIsNewline.size(); ++i)
            m_jit.link(nextIsNewline[i], success);
        nextIsNewline.clear();
    } else {
        m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
        failures.append(m_jit.emitUnlinkedJne());
    }
}

void WRECGenerator::generateAssertionWordBoundary(JmpSrcVector& failures, bool invert)
{
    JmpSrcVector wordBoundary;
    JmpSrcVector notWordBoundary;

    // (1) Check if the previous value was a word char

    // (1.1) check for begin of input
    m_jit.emitCmpl_i8r(0, CURR_POS_REG);
    JmpSrc atBegin = m_jit.emitUnlinkedJe();
    // (1.2) load the last char, and chck if is word character
    m_jit.emitMovzwl_mr(-2, INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);
    JmpSrcVector previousIsWord;
    generateCharacterClassInverted(previousIsWord, getCharacterClass_w());
    // (1.3) if we get here, previous is not a word char
    m_jit.link(atBegin, m_jit.label());

    // (2) Handle situation where previous was NOT a \w

    // (2.1) check for end of input
    m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
    notWordBoundary.append(m_jit.emitUnlinkedJe());
    // (2.2) load the next char, and chck if is word character
    m_jit.emitMovzwl_mr(INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);
    generateCharacterClassInverted(wordBoundary, getCharacterClass_w());
    // (2.3) If we get here, neither chars are word chars
    notWordBoundary.append(m_jit.emitUnlinkedJmp());

    // (3) Handle situation where previous was a \w

    // (3.0) link success in first match to here
    JmpDst section3 = m_jit.label();
    for (unsigned i = 0; i < previousIsWord.size(); ++i)
        m_jit.link(previousIsWord[i], section3);
    previousIsWord.clear();
    // (3.1) check for end of input
    m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
    wordBoundary.append(m_jit.emitUnlinkedJe());
    // (3.2) load the next char, and chck if is word character
    m_jit.emitMovzwl_mr(INPUT_REG, CURR_POS_REG, 2, CURR_VAL_REG);
    generateCharacterClassInverted(notWordBoundary, getCharacterClass_w());
    // (3.3) If we get here, this is an end of a word, within the input.
    
    // (4) Link everything up
    
    if (invert) {
        // handle the fall through case
        wordBoundary.append(m_jit.emitUnlinkedJmp());
    
        // looking for non word boundaries, so link boundary fails to here.
        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < notWordBoundary.size(); ++i)
            m_jit.link(notWordBoundary[i], success);
        notWordBoundary.clear();
        
        failures.append(wordBoundary.begin(), wordBoundary.size());
    } else {
        // looking for word boundaries, so link successes here.
        JmpDst success = m_jit.label();
        for (unsigned i = 0; i < wordBoundary.size(); ++i)
            m_jit.link(wordBoundary[i], success);
        wordBoundary.clear();
        
        failures.append(notWordBoundary.begin(), notWordBoundary.size());
    }
}

void WRECGenerator::generateBackreference(JmpSrcVector& failures, unsigned subpatternId)
{
    m_jit.emitPushl_r(CURR_POS_REG);
    m_jit.emitPushl_r(QUANTIFIER_COUNT_REG);

    // get the start pos of the backref into QUANTIFIER_COUNT_REG (multipurpose!)
    m_jit.emitMovl_mr((2 * subpatternId) * sizeof(int), OUTPUT_REG, QUANTIFIER_COUNT_REG);

    JmpSrc skipIncrement = m_jit.emitUnlinkedJmp();
    JmpDst topOfLoop = m_jit.label();
    m_jit.emitAddl_i8r(1, CURR_POS_REG);
    m_jit.emitAddl_i8r(1, QUANTIFIER_COUNT_REG);
    m_jit.link(skipIncrement, m_jit.label());

    // check if we're at the end of backref (if we are, success!)
    m_jit.emitCmpl_rm(QUANTIFIER_COUNT_REG, ((2 * subpatternId) + 1) * sizeof(int), OUTPUT_REG);
    JmpSrc endOfBackRef = m_jit.emitUnlinkedJe();
    
    m_jit.emitMovzwl_mr(INPUT_REG, QUANTIFIER_COUNT_REG, 2, CURR_VAL_REG);
    
    // check if we've run out of input (this would be a can o'fail)
    m_jit.emitCmpl_rr(LENGTH_REG, CURR_POS_REG);
    JmpSrc endOfInput = m_jit.emitUnlinkedJe();
    
    m_jit.emitCmpw_rm(CURR_VAL_REG, INPUT_REG, CURR_POS_REG, 2);
    m_jit.link(m_jit.emitUnlinkedJe(), topOfLoop);
    
    m_jit.link(endOfInput, m_jit.label());
    // Failure
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    m_jit.emitPopl_r(CURR_POS_REG);
    failures.append(m_jit.emitUnlinkedJmp());
    
    // Success
    m_jit.link(endOfBackRef, m_jit.label());
    m_jit.emitPopl_r(QUANTIFIER_COUNT_REG);
    m_jit.emitAddl_i8r(4, IA32MacroAssembler::esp);
}

void WRECGenerator::gernerateDisjunction(JmpSrcVector& successes, JmpSrcVector& failures)
{
    successes.append(m_jit.emitUnlinkedJmp());
    
    JmpDst here = m_jit.label();
    
    for (unsigned i = 0; i < failures.size(); ++i)
        m_jit.link(failures[i], here);
    failures.clear();
    
    m_jit.emitMovl_mr(IA32MacroAssembler::esp, CURR_POS_REG);
}

void WRECGenerator::terminateDisjunction(JmpSrcVector& successes)
{
    JmpDst here = m_jit.label();
    for (unsigned i = 0; i < successes.size(); ++i)
        m_jit.link(successes[i], here);
    successes.clear();
}

ALWAYS_INLINE Quantifier WRECParser::parseGreedyQuantifier()
{
    switch (peek()) {
        case '?':
            consume();
            return Quantifier(Quantifier::Greedy, 0, 1);

        case '*':
            consume();
            return Quantifier(Quantifier::Greedy, 0);

        case '+':
            consume();
            return Quantifier(Quantifier::Greedy, 1);

        case '{': {
            consume();
            // a numeric quantifier should always have a lower bound
            if (!peekIsDigit()) {
                m_err = Error_malformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
            int min = consumeNumber();
            
            // this should either be a , or a }
            switch (peek()) {
            case '}':
                // {n} - exactly n times. (technically I think a '?' is valid in the bnf - bit meaningless).
                consume();
                return Quantifier(Quantifier::Greedy, min, min);

            case ',':
                consume();
                switch (peek()) {
                case '}':
                    // {n,} - n to inf times.
                    consume();
                    return Quantifier(Quantifier::Greedy, min);

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    // {n,m} - n to m times.
                    int max = consumeNumber();
                    
                    if (peek() != '}') {
                        m_err = Error_malformedQuantifier;
                        return Quantifier(Quantifier::Error);
                    }
                    consume();

                    return Quantifier(Quantifier::Greedy, min, max);
                }

                default:
                    m_err = Error_malformedQuantifier;
                    return Quantifier(Quantifier::Error);
                }

            default:
                m_err = Error_malformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
        }
        // None of the above - no quantifier
        default:
            return Quantifier();
    }
}

Quantifier WRECParser::parseQuantifier()
{
    Quantifier q = parseGreedyQuantifier();
    
    if ((q.m_type == Quantifier::Greedy) && (peek() == '?')) {
        consume();
        q.m_type = Quantifier::NonGreedy;
    }
    
    return q;
}

bool WRECParser::parsePatternCharacterQualifier(JmpSrcVector& failures, int ch)
{
    Quantifier q = parseQuantifier();

    switch (q.m_type) {
    case Quantifier::None: {
        m_generator.generatePatternCharacter(failures, ch);
        break;
    }

    case Quantifier::Greedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateGreedyQuantifier(failures, functor, q.m_min, q.m_max);
        break;
    }

    case Quantifier::NonGreedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.m_min, q.m_max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseCharacterClassQuantifier(JmpSrcVector& failures, CharacterClass& charClass, bool invert)
{
    Quantifier q = parseQuantifier();

    switch (q.m_type) {
    case Quantifier::None: {
        m_generator.generateCharacterClass(failures, charClass, invert);
        break;
    }

    case Quantifier::Greedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateGreedyQuantifier(failures, functor, q.m_min, q.m_max);
        break;
    }

    case Quantifier::NonGreedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.m_min, q.m_max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseBackreferenceQuantifier(JmpSrcVector& failures, unsigned subpatternId)
{
    Quantifier q = parseQuantifier();

    switch (q.m_type) {
    case Quantifier::None: {
        m_generator.generateBackreference(failures, subpatternId);
        break;
    }

    case Quantifier::Greedy:
    case Quantifier::NonGreedy:
        m_generator.generateBackreferenceQuantifier(failures, q.m_type, subpatternId, q.m_min, q.m_max);
        return true;

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool WRECParser::parseParentheses(JmpSrcVector&)
{
    // FIXME: We don't currently backtrack correctly within parentheses in cases such as
    // "c".match(/(.*)c/) so we fall back to PCRE for any regexp containing parentheses.

    m_err = TempError_unsupportedParentheses;
    return false;
}

bool WRECParser::parseCharacterClass(JmpSrcVector& failures)
{
    bool invert = false;
    if (peek() == '^') {
        consume();
        invert = true;
    }

    CharacterClassConstructor charClassConstructor(m_ignoreCase);

    UChar ch;
    while ((ch = peek()) != ']') {
        switch (ch) {
        case EndOfPattern:
            m_err = Error_malformedCharacterClass;
            return false;
            
        case '\\':
            consume();
            switch (ch = peek()) {
            case EndOfPattern:
                m_err = Error_malformedEscape;
                return false;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                charClassConstructor.put(consumeOctal());
                break;

            // ControlEscape
            case 'b':
                consume();
                charClassConstructor.put('\b');
                break;
            case 'f':
                consume();
                charClassConstructor.put('\f');
                break;
            case 'n':
                consume();
                charClassConstructor.put('\n');
                break;
            case 'r':
                consume();
                charClassConstructor.put('\r');
                break;
            case 't':
                consume();
                charClassConstructor.put('\t');
                break;
            case 'v':
                consume();
                charClassConstructor.put('\v');
                break;

            // ControlLetter
            case 'c': {
                consume();
                int control = consume();
                if (!isASCIIAlpha(control)) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(control&31);
                break;
            }

            // HexEscape
            case 'x': {
                consume();
                int x = consumeHex(2);
                if (x == -1) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }

            // UnicodeEscape
            case 'u': {
                consume();
                int x = consumeHex(4);
                if (x == -1) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }
            
            // CharacterClassEscape
            case 'd':
                consume();
                charClassConstructor.append(getCharacterClass_d());
                break;
            case 's':
                consume();
                charClassConstructor.append(getCharacterClass_s());
                break;
            case 'w':
                consume();
                charClassConstructor.append(getCharacterClass_w());
                break;
            case 'D':
                consume();
                charClassConstructor.append(getCharacterClass_D());
                break;
            case 'S':
                consume();
                charClassConstructor.append(getCharacterClass_S());
                break;
            case 'W':
                consume();
                charClassConstructor.append(getCharacterClass_W());
                break;

            case '-':
                consume();
                charClassConstructor.put(ch);
                charClassConstructor.flush();
                break;

            // IdentityEscape
            default: {
                // TODO: check this test for IdentifierPart.
                int ch = consume();
                if (isASCIIAlphanumeric(ch) || (ch == '_')) {
                    m_err = Error_malformedEscape;
                    return false;
                }
                charClassConstructor.put(ch);
                break;
            }
            }
            break;
            
        default:
            consume();
            charClassConstructor.put(ch);
        }
    }
    consume();

    // lazily catch reversed ranges ([z-a])in character classes
    if (charClassConstructor.m_upsideDown) {
        m_err = Error_malformedCharacterClass;
        return false;
    }

    charClassConstructor.flush();

    CharacterClass charClass = {
        charClassConstructor.m_matches.begin(), charClassConstructor.m_matches.size(),
        charClassConstructor.m_ranges.begin(), charClassConstructor.m_ranges.size(),
        charClassConstructor.m_matchesUnicode.begin(), charClassConstructor.m_matchesUnicode.size(),
        charClassConstructor.m_rangesUnicode.begin(), charClassConstructor.m_rangesUnicode.size(),
    };
    return parseCharacterClassQuantifier(failures, charClass, invert);
}

bool WRECParser::parseOctalEscape(JmpSrcVector& failures)
{
    return parsePatternCharacterQualifier(failures, consumeOctal());
}

bool WRECParser::parseEscape(JmpSrcVector& failures)
{
    switch (peek()) {
    case EndOfPattern:
        m_err = Error_malformedEscape;
        return false;

    // Assertions
    case 'b':
        consume();
        m_generator.generateAssertionWordBoundary(failures, false);
        return true;

    case 'B':
        consume();
        m_generator.generateAssertionWordBoundary(failures, true);
        return true;

    // Octal escape
    case '0':
        consume();
        return parseOctalEscape(failures);

    // DecimalEscape
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
        // FIXME: This does not support forward references. It's not clear whether they
        // should be valid.
        unsigned value = peekDigit();
        if (value > m_numSubpatterns)
            return parseOctalEscape(failures);
    }
    case '8':
    case '9': {
        unsigned value = peekDigit();
        if (value > m_numSubpatterns) {
            consume();
            m_err = Error_malformedEscape;
            return false;
        }
        consume();

        while (peekIsDigit()) {
            unsigned newValue = value * 10 + peekDigit();
            if (newValue > m_numSubpatterns)
                break;
            value = newValue;
            consume();
        }

        parseBackreferenceQuantifier(failures, value);
        return true;
    }
    
    // ControlEscape
    case 'f':
        consume();
        return parsePatternCharacterQualifier(failures, '\f');
    case 'n':
        consume();
        return parsePatternCharacterQualifier(failures, '\n');
    case 'r':
        consume();
        return parsePatternCharacterQualifier(failures, '\r');
    case 't':
        consume();
        return parsePatternCharacterQualifier(failures, '\t');
    case 'v':
        consume();
        return parsePatternCharacterQualifier(failures, '\v');

    // ControlLetter
    case 'c': {
        consume();
        int control = consume();
        if (!isASCIIAlpha(control)) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, control&31);
    }

    // HexEscape
    case 'x': {
        consume();
        int x = consumeHex(2);
        if (x == -1) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // UnicodeEscape
    case 'u': {
        consume();
        int x = consumeHex(4);
        if (x == -1) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // CharacterClassEscape
    case 'd':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_d(), false);
    case 's':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_s(), false);
    case 'w':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_w(), false);
    case 'D':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_d(), true);
    case 'S':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_s(), true);
    case 'W':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_w(), true);

    // IdentityEscape
    default: {
        // TODO: check this test for IdentifierPart.
        int ch = consume();
        if (isASCIIAlphanumeric(ch) || (ch == '_')) {
            m_err = Error_malformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, ch);
    }
    }
}

bool WRECParser::parseTerm(JmpSrcVector& failures)
{
    switch (peek()) {
    case EndOfPattern:
    case '*':
    case '+':
    case '?':
    case ')':
    case ']':
    case '{':
    case '}':
    case '|':
        // Not allowed in a Term!
        return false;
        
    case '^':
        consume();
        m_generator.generateAssertionBOL(failures);
        return true;
    
    case '$':
        consume();
        m_generator.generateAssertionEOL(failures);
        return true;

    case '\\':
        // b & B are also assertions...
        consume();
        return parseEscape(failures);

    case '.':
        consume();
        return parseCharacterClassQuantifier(failures, getCharacterClass_newline(), true);

    case '[':
        // CharacterClass
        consume();
        return parseCharacterClass(failures);

    case '(':
        consume();
        return parseParentheses(failures);

    default:
        // Anything else is a PatternCharacter
        return parsePatternCharacterQualifier(failures, consume());
    }
}

/*
  interface req: CURR_POS is on stack (can be reloaded).
*/
void WRECParser::parseDisjunction(JmpSrcVector& failures)
{
    parseAlternative(failures);

    if (peek() == '|') {
        JmpSrcVector successes;

        do {
            consume();

            m_generator.gernerateDisjunction(successes, failures);

            parseAlternative(failures);
        } while (peek() == '|');

        m_generator.terminateDisjunction(successes);
    }
}

} // namespace KJS

#endif // ENABLE(WREC)
