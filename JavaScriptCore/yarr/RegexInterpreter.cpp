/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "RegexInterpreter.h"

#include "RegexCompiler.h"
#include "RegexPattern.h"

#if ENABLE(YARR)

using namespace WTF;

namespace JSC { namespace Yarr {

class Interpreter {
public:
    struct ParenthesesDisjunctionContext;

    struct BackTrackInfoPatternCharacter {
        uintptr_t matchAmount;
    };
    struct BackTrackInfoCharacterClass {
        uintptr_t matchAmount;
    };
    struct BackTrackInfoBackReference {
        uintptr_t begin; // Not really needed for greedy quantifiers.
        uintptr_t matchAmount; // Not really needed for fixed quantifiers.
    };
    struct BackTrackInfoAlternative {
        uintptr_t offset;
    };
    struct BackTrackInfoParentheticalAssertionOnce {
        uintptr_t begin;
    };
    struct BackTrackInfoParenthesesOnce {
        uintptr_t inParentheses;
    };
    struct BackTrackInfoParentheses {
        uintptr_t matchAmount;
        ParenthesesDisjunctionContext* lastContext;
        uintptr_t prevBegin;
        uintptr_t prevEnd;
    };

    static inline void appendParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack, ParenthesesDisjunctionContext* context)
    {
        context->next = backTrack->lastContext;
        backTrack->lastContext = context;
        ++backTrack->matchAmount;
    }

    static inline void popParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack)
    {
        ASSERT(backTrack->matchAmount);
        ASSERT(backTrack->lastContext);
        backTrack->lastContext = backTrack->lastContext->next;
        --backTrack->matchAmount;
    }

    struct DisjunctionContext
    {
        DisjunctionContext()
            : term(0)
        {
        }
        
        void* operator new(size_t, void* where)
        {
            return where;
        }

        unsigned term;
        unsigned matchBegin;
        unsigned matchEnd;
        uintptr_t frame[1];
    };

    DisjunctionContext* allocDisjunctionContext(ByteDisjunction* disjunction)
    {
        return new(malloc(sizeof(DisjunctionContext) + (disjunction->m_frameSize - 1) * sizeof(uintptr_t))) DisjunctionContext();
    }

    void freeDisjunctionContext(DisjunctionContext* context)
    {
        free(context);
    }

    struct ParenthesesDisjunctionContext
    {
        ParenthesesDisjunctionContext(int* output, ByteTerm& term)
            : next(0)
        {
            unsigned firstSubpatternId = term.atom.subpatternId;
            unsigned numNestedSubpatterns = term.atom.parenthesesDisjunction->m_numSubpatterns;

            for (unsigned i = 0; i < (numNestedSubpatterns << 1); ++i) {
                subpatternBackup[i] = output[(firstSubpatternId << 1) + i];
                output[(firstSubpatternId << 1) + i] = -1;
            }
            
            new(getDisjunctionContext(term)) DisjunctionContext();
        }

        void* operator new(size_t, void* where)
        {
            return where;
        }

        void restoreOutput(int* output, unsigned firstSubpatternId, unsigned numNestedSubpatterns)
        {
            for (unsigned i = 0; i < (numNestedSubpatterns << 1); ++i)
                output[(firstSubpatternId << 1) + i] = subpatternBackup[i];
        }
        
        DisjunctionContext* getDisjunctionContext(ByteTerm& term)
        {
            return reinterpret_cast<DisjunctionContext*>(&(subpatternBackup[term.atom.parenthesesDisjunction->m_numSubpatterns << 1]));
        }

        ParenthesesDisjunctionContext* next;
        int subpatternBackup[1];
    };

    ParenthesesDisjunctionContext* allocParenthesesDisjunctionContext(ByteDisjunction* disjunction, int* output, ByteTerm& term)
    {
        return new(malloc(sizeof(ParenthesesDisjunctionContext) + (((term.atom.parenthesesDisjunction->m_numSubpatterns << 1) - 1) * sizeof(int)) + sizeof(DisjunctionContext) + (disjunction->m_frameSize - 1) * sizeof(uintptr_t))) ParenthesesDisjunctionContext(output, term);
    }

    void freeParenthesesDisjunctionContext(ParenthesesDisjunctionContext* context)
    {
        free(context);
    }

    class InputStream {
    public:
        InputStream(const UChar* input, unsigned start, unsigned length)
            : input(input)
            , pos(start)
            , length(length)
        {
        }

        void next()
        {
            ++pos;
        }

        void rewind(unsigned amount)
        {
            ASSERT(pos >= amount);
            pos -= amount;
        }

        int read()
        {
            ASSERT(pos < length);
            if (pos < length)
                return input[pos];
            return -1;
        }

        int readChecked(int position)
        {
            ASSERT(position < 0);
            ASSERT((unsigned)-position <= pos);
            unsigned p = pos + position;
            ASSERT(p < length);
            return input[p];
        }

        int reread(unsigned from)
        {
            ASSERT(from < length);
            return input[from];
        }

        int prev()
        {
            ASSERT(!(pos > length));
            if (pos && length)
                return input[pos - 1];
            return -1;
        }
        
        unsigned getPos()
        {
            return pos;
        }

        void setPos(unsigned p)
        {
            pos = p;
        }
        
        bool atStart()
        {
            return pos == 0;
        }

        bool atEnd()
        {
            return pos == length;
        }

        bool checkInput(int count)
        {
            if ((pos + count) <= length) {
                pos += count;
                return true;
            } else
                return false;
        }

        void uncheckInput(int count)
        {
            pos -= count;
        }

        bool atStart(int position)
        {
            return (pos + position) == 0;
        }

        bool atEnd(int position)
        {
            return (pos + position) == length;
        }

    private:
        const UChar* input;
        unsigned pos;
        unsigned length;
    };

    bool testCharacterClass(CharacterClass* characterClass, int ch)
    {
        if (ch & 0xFF80) {
            for (unsigned i = 0; i < characterClass->m_matchesUnicode.size(); ++i)
                if (ch == characterClass->m_matchesUnicode[i])
                    return true;
            for (unsigned i = 0; i < characterClass->m_rangesUnicode.size(); ++i)
                if ((ch >= characterClass->m_rangesUnicode[i].begin) && (ch <= characterClass->m_rangesUnicode[i].end))
                    return true;
        } else {
            for (unsigned i = 0; i < characterClass->m_matches.size(); ++i)
                if (ch == characterClass->m_matches[i])
                    return true;
            for (unsigned i = 0; i < characterClass->m_ranges.size(); ++i)
                if ((ch >= characterClass->m_ranges[i].begin) && (ch <= characterClass->m_ranges[i].end))
                    return true;
        }

        return false;
    }

    bool tryConsumeCharacter(int testChar)
    {
        if (input.atEnd())
            return false;
        
        int ch = input.read();

        if (pattern->m_ignoreCase ? ((Unicode::toLower(testChar) == ch) || (Unicode::toUpper(testChar) == ch)) : (testChar == ch)) {
            input.next();
            return true;
        }
        return false;
    }

    bool checkCharacter(int testChar, int inputPosition)
    {
        int ch = input.readChecked(inputPosition);
        return (pattern->m_ignoreCase ? ((Unicode::toLower(testChar) == ch) || (Unicode::toUpper(testChar) == ch)) : (testChar == ch));
    }

    bool tryConsumeCharacterClass(CharacterClass* characterClass, bool invert)
    {
        if (input.atEnd())
            return false;

        bool match = testCharacterClass(characterClass, input.read());

        if (invert)
            match = !match;

        if (match) {
            input.next();
            return true;
        }
        return false;
    }

    bool checkCharacterClass(CharacterClass* characterClass, bool invert, int inputPosition)
    {
        bool match = testCharacterClass(characterClass, input.readChecked(inputPosition));
        return invert ? !match : match;
    }

    bool tryConsumeBackReference(int matchBegin, int matchEnd, int inputOffset)
    {
        int matchSize = matchEnd - matchBegin;

        if (!input.checkInput(matchSize))
            return false;

        for (int i = 0; i < matchSize; ++i) {
            if (!checkCharacter(input.reread(matchBegin + i), inputOffset - matchSize + i)) {
                input.uncheckInput(matchSize);
                return false;
            }
        }
        
        return true;
    }

    bool matchAssertionBOL(ByteTerm& term)
    {
        return (input.atStart(term.inputPosition)) || (pattern->m_multiline && testCharacterClass(pattern->newlineCharacterClass, input.readChecked(term.inputPosition - 1)));
    }

    bool matchAssertionEOL(ByteTerm& term)
    {
        if (term.inputPosition)
            return (input.atEnd(term.inputPosition)) || (pattern->m_multiline && testCharacterClass(pattern->newlineCharacterClass, input.readChecked(term.inputPosition)));
        else
            return (input.atEnd()) || (pattern->m_multiline && testCharacterClass(pattern->newlineCharacterClass, input.read()));
    }

    bool matchAssertionWordBoundary(ByteTerm& term)
    {
        bool prevIsWordchar = !input.atStart(term.inputPosition) && testCharacterClass(pattern->wordcharCharacterClass, input.readChecked(term.inputPosition - 1));
        bool readIsWordchar;
        if (term.inputPosition)
            readIsWordchar = !input.atEnd(term.inputPosition) && testCharacterClass(pattern->wordcharCharacterClass, input.readChecked(term.inputPosition));
        else
            readIsWordchar = !input.atEnd() && testCharacterClass(pattern->wordcharCharacterClass, input.read());

        bool wordBoundary = prevIsWordchar != readIsWordchar;
        return term.invert() ? !wordBoundary : wordBoundary;
    }

    bool matchPatternCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypePatternCharacter);
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierFixedCount: {
            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityCount; ++matchAmount) {
                if (!checkCharacter(term.atom.patternCharacter, term.inputPosition + matchAmount))
                    return false;
            }
            return true;
        }

        case QuantifierGreedy: {
            unsigned matchAmount = 0;
            while ((matchAmount < term.atom.quantityCount) && input.checkInput(1)) {
                if (!checkCharacter(term.atom.patternCharacter, term.inputPosition - 1)) {
                    input.uncheckInput(1);
                    break;
                }
                ++matchAmount;
            }
            backTrack->matchAmount = matchAmount;

            return true;
        }

        case QuantifierNonGreedy:
            backTrack->matchAmount = 0;
            return true;
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool backtrackPatternCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypePatternCharacter);
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierFixedCount:
            break;

        case QuantifierGreedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                input.uncheckInput(1);
                return true;
            }
            break;

        case QuantifierNonGreedy:
            if ((backTrack->matchAmount < term.atom.quantityCount) && input.checkInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacter(term.atom.patternCharacter, term.inputPosition - 1))
                    return true;
            }
            input.uncheckInput(backTrack->matchAmount);
            break;
        }

        return false;
    }

    bool matchCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeCharacterClass);
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierFixedCount: {
            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityCount; ++matchAmount) {
                if (!checkCharacterClass(term.atom.characterClass, term.invert(), term.inputPosition + matchAmount))
                    return false;
            }
            return true;
        }

        case QuantifierGreedy: {
            unsigned matchAmount = 0;
            while ((matchAmount < term.atom.quantityCount) && input.checkInput(1)) {
                if (!checkCharacterClass(term.atom.characterClass, term.invert(), term.inputPosition - 1)) {
                    input.uncheckInput(1);
                    break;
                }
                ++matchAmount;
            }
            backTrack->matchAmount = matchAmount;

            return true;
        }

        case QuantifierNonGreedy:
            backTrack->matchAmount = 0;
            return true;
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool backtrackCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeCharacterClass);
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierFixedCount:
            break;

        case QuantifierGreedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                input.uncheckInput(1);
                return true;
            }
            break;

        case QuantifierNonGreedy:
            if ((backTrack->matchAmount < term.atom.quantityCount) && input.checkInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacterClass(term.atom.characterClass, term.invert(), term.inputPosition - 1))
                    return true;
            }
            input.uncheckInput(backTrack->matchAmount);
            break;
        }

        return false;
    }

    bool matchBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeBackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        int matchBegin = output[(term.atom.subpatternId << 1)];
        int matchEnd = output[(term.atom.subpatternId << 1) + 1];
        ASSERT((matchBegin == -1) == (matchEnd == -1));
        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return true;

        switch (term.atom.quantityType) {
        case QuantifierFixedCount: {
            backTrack->begin = input.getPos();
            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityCount; ++matchAmount) {
                if (!tryConsumeBackReference(matchBegin, matchEnd, term.inputPosition)) {
                    input.setPos(backTrack->begin);
                    return false;
                }
            }
            return true;
        }

        case QuantifierGreedy: {
            unsigned matchAmount = 0;
            while ((matchAmount < term.atom.quantityCount) && tryConsumeBackReference(matchBegin, matchEnd, term.inputPosition))
                ++matchAmount;
            backTrack->matchAmount = matchAmount;
            return true;
        }

        case QuantifierNonGreedy:
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            return true;
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool backtrackBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeBackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        int matchBegin = output[(term.atom.subpatternId << 1)];
        int matchEnd = output[(term.atom.subpatternId << 1) + 1];
        ASSERT((matchBegin == -1) == (matchEnd == -1));
        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return false;

        switch (term.atom.quantityType) {
        case QuantifierFixedCount:
            // for quantityCount == 1, could rewind.
            input.setPos(backTrack->begin);
            break;

        case QuantifierGreedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                input.rewind(matchEnd - matchBegin);
                return true;
            }
            break;

        case QuantifierNonGreedy:
            if ((backTrack->matchAmount < term.atom.quantityCount) && tryConsumeBackReference(matchBegin, matchEnd, term.inputPosition)) {
                ++backTrack->matchAmount;
                return true;
            } else
                input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    void recordParenthesesMatch(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        if (term.capture()) {
            unsigned subpatternId = term.atom.subpatternId;
            output[(subpatternId << 1)] = context->getDisjunctionContext(term)->matchBegin + term.inputPosition;
            output[(subpatternId << 1) + 1] = context->getDisjunctionContext(term)->matchEnd + term.inputPosition;
        }
    }
    void resetMatches(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        unsigned firstSubpatternId = term.atom.subpatternId;
        unsigned count = term.atom.parenthesesDisjunction->m_numSubpatterns;
        context->restoreOutput(output, firstSubpatternId, count);
    }
    void resetAssertionMatches(ByteTerm& term)
    {
        unsigned firstSubpatternId = term.atom.subpatternId;
        unsigned count = term.atom.parenthesesDisjunction->m_numSubpatterns;
        for (unsigned i = 0; i < (count << 1); ++i)
            output[(firstSubpatternId << 1) + i] = -1;
    }
    bool parenthesesDoBacktrack(ByteTerm& term, BackTrackInfoParentheses* backTrack)
    {
        while (backTrack->matchAmount) {
            ParenthesesDisjunctionContext* context = backTrack->lastContext;

            if (matchDisjunction(term.atom.parenthesesDisjunction, context->getDisjunctionContext(term), true))
                return true;
            
            resetMatches(term, context);
            freeParenthesesDisjunctionContext(context);
            popParenthesesDisjunctionContext(backTrack);
        }

        return false;
    }

    bool matchParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierGreedy: {
            // set this speculatively; if we get to the parens end this will be true.
            backTrack->inParentheses = 1;
            break;
        }
        case QuantifierNonGreedy: {
            backTrack->inParentheses = 0;
            context->term += term.atom.parenthesesWidth;
            return true;
        }
        case QuantifierFixedCount:
            break;
        }

        if (term.capture()) {
            unsigned subpatternId = term.atom.subpatternId;
            output[(subpatternId << 1)] = input.getPos() + term.inputPosition;
        }

        return true;
    }

    bool matchParenthesesOnceEnd(ByteTerm& term, DisjunctionContext*)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityCount == 1);

        if (term.capture()) {
            unsigned subpatternId = term.atom.subpatternId;
            output[(subpatternId << 1) + 1] = input.getPos() + term.inputPosition;
        }
        return true;
    }

    bool backtrackParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        if (term.capture()) {
            unsigned subpatternId = term.atom.subpatternId;
            output[(subpatternId << 1)] = -1;
            output[(subpatternId << 1) + 1] = -1;
        }

        switch (term.atom.quantityType) {
        case QuantifierGreedy:
            // if we backtrack to this point, there is another chance - try matching nothing.
            ASSERT(backTrack->inParentheses);
            backTrack->inParentheses = 0;
            context->term += term.atom.parenthesesWidth;
            return true;
        case QuantifierNonGreedy:
            ASSERT(backTrack->inParentheses);
        case QuantifierFixedCount:
            break;
        }

        return false;
    }

    bool backtrackParenthesesOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierGreedy:
            if (!backTrack->inParentheses) {
                context->term -= term.atom.parenthesesWidth;
                return false;
            }
        case QuantifierNonGreedy:
            if (!backTrack->inParentheses) {
                // now try to match the parens; set this speculatively.
                backTrack->inParentheses = 1;
                if (term.capture()) {
                    unsigned subpatternId = term.atom.subpatternId;
                    output[(subpatternId << 1) + 1] = input.getPos() + term.inputPosition;
                }
                context->term -= term.atom.parenthesesWidth;
                return true;
            }
        case QuantifierFixedCount:
            break;
        }

        return false;
    }

    bool matchParentheticalAssertionOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParentheticalAssertionOnceBegin);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParentheticalAssertionOnce* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertionOnce*>(context->frame + term.frameLocation);

        backTrack->begin = input.getPos();
        return true;
    }

    bool matchParentheticalAssertionOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParentheticalAssertionOnceEnd);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParentheticalAssertionOnce* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertionOnce*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        // We've reached the end of the parens; if they are inverted, this is failure.
        if (term.invert()) {
            context->term -= term.atom.parenthesesWidth;
            return false;
        }

        return true;
    }

    bool backtrackParentheticalAssertionOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParentheticalAssertionOnceBegin);
        ASSERT(term.atom.quantityCount == 1);

        // We've failed to match parens; if they are inverted, this is win!
        if (term.invert()) {
            context->term += term.atom.parenthesesWidth;
            return true;
        }

        return false;
    }

    bool backtrackParentheticalAssertionOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParentheticalAssertionOnceEnd);
        ASSERT(term.atom.quantityCount == 1);

        BackTrackInfoParentheticalAssertionOnce* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertionOnce*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        context->term -= term.atom.parenthesesWidth;
        return false;
    }

    bool matchParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpattern);
        ASSERT(term.atom.quantityCount > 1);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);

        unsigned subpatternId = term.atom.subpatternId;
        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        backTrack->prevBegin = output[(subpatternId << 1)];
        backTrack->prevEnd = output[(subpatternId << 1) + 1];

        backTrack->matchAmount = 0;
        backTrack->lastContext = 0;

        switch (term.atom.quantityType) {
        case QuantifierFixedCount: {
            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < term.atom.quantityCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (matchDisjunction(disjunctionBody, context->getDisjunctionContext(term)))
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                    if (!parenthesesDoBacktrack(term, backTrack))
                        return false;
                }
            }

            ASSERT(backTrack->matchAmount == term.atom.quantityCount);
            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
            return true;
        }

        case QuantifierGreedy: {
            while (backTrack->matchAmount < term.atom.quantityCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term)))
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                    break;
                }
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return true;
        }

        case QuantifierNonGreedy:
            return true;
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    // Rules for backtracking differ depending on whether this is greedy or non-greedy.
    //
    // Greedy matches never should try just adding more - you should already have done
    // the 'more' cases.  Always backtrack, at least a leetle bit.  However cases where
    // you backtrack an item off the list needs checking, since we'll never have matched
    // the one less case.  Tracking forwards, still add as much as possible.
    //
    // Non-greedy, we've already done the one less case, so don't match on popping.
    // We haven't done the one more case, so always try to add that.
    //
    bool backtrackParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::TypeParenthesesSubpattern);
        ASSERT(term.atom.quantityCount > 1);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);

        if (term.capture()) {
            unsigned subpatternId = term.atom.subpatternId;
            output[(subpatternId << 1)] = backTrack->prevBegin;
            output[(subpatternId << 1) + 1] = backTrack->prevEnd;
        }

        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        switch (term.atom.quantityType) {
        case QuantifierFixedCount: {
            ASSERT(backTrack->matchAmount == term.atom.quantityCount);

            ParenthesesDisjunctionContext* context = 0;

            if (!parenthesesDoBacktrack(term, backTrack))
                return false;

            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < term.atom.quantityCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (matchDisjunction(disjunctionBody, context->getDisjunctionContext(term)))
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                    if (!parenthesesDoBacktrack(term, backTrack))
                        return false;
                }
            }

            ASSERT(backTrack->matchAmount == term.atom.quantityCount);
            context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
            return true;
        }

        case QuantifierGreedy: {
            if (!backTrack->matchAmount)
                return false;

            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            if (matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term), true)) {
                while (backTrack->matchAmount < term.atom.quantityCount) {
                    ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                    if (matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term)))
                        appendParenthesesDisjunctionContext(backTrack, context);
                    else {
                        resetMatches(term, context);
                        freeParenthesesDisjunctionContext(context);
                        break;
                    }
                }
            } else {
                resetMatches(term, context);
                freeParenthesesDisjunctionContext(context);
                popParenthesesDisjunctionContext(backTrack);
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return true;
        }

        case QuantifierNonGreedy: {
            // If we've not reached the limit, try to add one more match.
            if (backTrack->matchAmount < term.atom.quantityCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                if (matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term))) {
                    appendParenthesesDisjunctionContext(backTrack, context);
                    recordParenthesesMatch(term, context);
                    return true;
                } else {
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                }
            }

            // Nope - okay backtrack looking for an alternative.
            while (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                if (matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term), true)) {
                    // successful backtrack! we're back in the game!
                    if (backTrack->matchAmount) {
                        context = backTrack->lastContext;
                        recordParenthesesMatch(term, context);
                    }
                    return true;
                }
                
                // pop a match off the stack
                resetMatches(term, context);
                freeParenthesesDisjunctionContext(context);
                popParenthesesDisjunctionContext(backTrack);
            }

            return false;
        }
        }

        ASSERT_NOT_REACHED();
        return false;
    }

    bool matchTerm(ByteDisjunction* disjunction, DisjunctionContext* context)
    {
        ByteTerm& term = disjunction->terms[context->term];

        switch (term.type) {
            case ByteTerm::TypeAlternativeBegin:
                return true;
            case ByteTerm::TypeAlternativeDisjunction:
            case ByteTerm::TypeAlternativeEnd: {
                int offset = term.alternative.end;
                if (term.alternative.isParentheses) {
                    BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + term.frameLocation);
                    backTrack->offset = offset;
                }
                context->term += offset;
                return true;
            }

            case ByteTerm::TypeAssertionBOL:
                return matchAssertionBOL(term);
            case ByteTerm::TypeAssertionEOL:
                return matchAssertionEOL(term);
            case ByteTerm::TypeAssertionWordBoundary:
                return matchAssertionWordBoundary(term);

            case ByteTerm::TypePatternCharacter:
                return matchPatternCharacter(term, context);
            case ByteTerm::TypeCharacterClass:
                return matchCharacterClass(term, context);
            case ByteTerm::TypeBackReference:
                return matchBackReference(term, context);
            case ByteTerm::TypeParenthesesSubpattern:
                return matchParentheses(term, context);
            case ByteTerm::TypeParenthesesSubpatternOnceBegin:
                return matchParenthesesOnceBegin(term, context);
            case ByteTerm::TypeParenthesesSubpatternOnceEnd:
                return matchParenthesesOnceEnd(term, context);
            case ByteTerm::TypeParentheticalAssertionOnceBegin:
                return matchParentheticalAssertionOnceBegin(term, context);
            case ByteTerm::TypeParentheticalAssertionOnceEnd:
                return matchParentheticalAssertionOnceEnd(term, context);

            case ByteTerm::TypeCheckInput:
                if (input.checkInput(term.checkInputCount))
                    return true;
                else
                    return false;

            default:
                ASSERT_NOT_REACHED();
                return false;
        }
    }

    bool backtrackTerm(ByteDisjunction* disjunction, DisjunctionContext* context)
    {
        ByteTerm& term = disjunction->terms[context->term];

        switch (term.type) {
            case ByteTerm::TypeAlternativeBegin:
            case ByteTerm::TypeAlternativeDisjunction: {
                int offset = term.alternative.next;
                context->term += offset;
                return (offset > 0);
            }
            case ByteTerm::TypeAlternativeEnd: {
                // We should never backtrack back into an alternative of the main body of the regex.
                ASSERT(term.alternative.isParentheses);
                BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + term.frameLocation);
                unsigned offset = backTrack->offset;
                context->term -= offset;
                return false;
            }

            case ByteTerm::TypeAssertionBOL:
            case ByteTerm::TypeAssertionEOL:
            case ByteTerm::TypeAssertionWordBoundary:
                return false;

            case ByteTerm::TypePatternCharacter:
                return backtrackPatternCharacter(term, context);
            case ByteTerm::TypeCharacterClass:
                return backtrackCharacterClass(term, context);
            case ByteTerm::TypeBackReference:
                return backtrackBackReference(term, context);
            case ByteTerm::TypeParenthesesSubpattern:
                return backtrackParentheses(term, context);
            case ByteTerm::TypeParenthesesSubpatternOnceBegin:
                return backtrackParenthesesOnceBegin(term, context);
            case ByteTerm::TypeParenthesesSubpatternOnceEnd:
                return backtrackParenthesesOnceEnd(term, context);
            case ByteTerm::TypeParentheticalAssertionOnceBegin:
                return backtrackParentheticalAssertionOnceBegin(term, context);
            case ByteTerm::TypeParentheticalAssertionOnceEnd:
                return backtrackParentheticalAssertionOnceEnd(term, context);

            case ByteTerm::TypeCheckInput:
                input.uncheckInput(term.checkInputCount);
                return false;

            default:
                ASSERT_NOT_REACHED();
                return false;
        }
    }

    bool matchAlternative(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        if (btrack)
            goto backtrack;

        context->term = -1;

        while (true) {
            do {
                ++context->term;
                ASSERT(context->term < disjunction->terms.size());
                if (disjunction->terms[context->term].type == ByteTerm::TypePatternEnd)
                    return true;
            } while (matchTerm(disjunction, context));

        backtrack:
            do {
                if (!context->term--)
                    return false;
            } while (!backtrackTerm(disjunction, context));
        }
    }

    bool matchDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        if (btrack) {
            bool matched = matchAlternative(disjunction, context, true);
            if (matched) {
                context->matchEnd = input.getPos();
                return true;
            }

        } else {
            unsigned begin = input.getPos();
            bool matched = matchAlternative(disjunction, context);
            if (matched) {
                context->matchBegin = begin;
                context->matchEnd = input.getPos();
                return true;
            }
            input.setPos(begin);
        }

        return false;
    }

    bool matchNonZeroDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        if (matchDisjunction(disjunction, context, btrack)) {
            while (context->matchBegin == context->matchEnd) {
                if (!matchDisjunction(disjunction, context, true))
                    return false;
            }
            return true;
        }

        return false;
    }

    int interpret()
    {
        DisjunctionContext* context = allocDisjunctionContext(pattern->m_body.get());

        for (unsigned i = 0; i < ((pattern->m_body->m_numSubpatterns + 1) << 1); ++i)
            output[i] = -1;

        while (true) {
            bool matched = matchDisjunction(pattern->m_body.get(), context);

            if (matched) {
                output[0] = context->matchBegin;
                output[1] = context->matchEnd;

                unsigned begin = context->matchBegin;
                freeDisjunctionContext(context);
                return begin;
            }

            if (input.atEnd()) {
                freeDisjunctionContext(context);
                return -1;
            }

            input.next();
        }
    }

    Interpreter(BytecodePattern* pattern, int* output, const UChar* inputChar, unsigned start, unsigned length)
        : pattern(pattern)
        , output(output)
        , input(inputChar, start, length)
    {
    }

private:
    BytecodePattern *pattern;
    int* output;
    InputStream input;
};



class ByteCompiler {
    struct ParenthesesStackEntry {
        unsigned beginTerm;
        unsigned savedAlternativeIndex;
        ParenthesesStackEntry(unsigned beginTerm, unsigned savedAlternativeIndex/*, unsigned subpatternId, bool capture = false*/)
            : beginTerm(beginTerm)
            , savedAlternativeIndex(savedAlternativeIndex)
        {
        }
    };

public:
    ByteCompiler(RegexPattern& pattern)
        : m_pattern(pattern)
    {
        bodyDisjunction = 0;
        currentAlternativeIndex = 0;
    }
    
    BytecodePattern* compile()
    {
        regexBegin(m_pattern.m_numSubpatterns, m_pattern.m_body->m_callFrameSize);
        emitDisjunction(m_pattern.m_body);
        regexEnd();

        return new BytecodePattern(bodyDisjunction, m_allParenthesesInfo, m_pattern);
    }
    
    void checkInput(unsigned count)
    {
        bodyDisjunction->terms.append(ByteTerm::CheckInput(count));
    }

    void assertionBOL(int inputPosition)
    {
        bodyDisjunction->terms.append(ByteTerm::BOL(inputPosition));
    }

    void assertionEOL(int inputPosition)
    {
        bodyDisjunction->terms.append(ByteTerm::EOL(inputPosition));
    }

    void assertionWordBoundary(bool invert, int inputPosition)
    {
        bodyDisjunction->terms.append(ByteTerm::WordBoundary(invert, inputPosition));
    }

    void atomPatternCharacter(UChar ch, int inputPosition, unsigned frameLocation, unsigned quantityCount, QuantifierType quantityType)
    {
        bodyDisjunction->terms.append(ByteTerm(ch, inputPosition));

        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityCount = quantityCount;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityType = quantityType;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = frameLocation;
    }
    
    void atomCharacterClass(CharacterClass* characterClass, bool invert, int inputPosition, unsigned frameLocation, unsigned quantityCount, QuantifierType quantityType)
    {
        bodyDisjunction->terms.append(ByteTerm(characterClass, invert, inputPosition));

        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityCount = quantityCount;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityType = quantityType;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = frameLocation;
    }

    void atomBackReference(unsigned subpatternId, int inputPosition, unsigned frameLocation, unsigned quantityCount, QuantifierType quantityType)
    {
        ASSERT(subpatternId);

        bodyDisjunction->terms.append(ByteTerm::BackReference(subpatternId, inputPosition));

        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityCount = quantityCount;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].atom.quantityType = quantityType;
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = frameLocation;
    }

    void atomParenthesesSubpatternBegin(unsigned subpatternId, bool capture, int inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        int beginTerm = bodyDisjunction->terms.size();

        bodyDisjunction->terms.append(ByteTerm(ByteTerm::TypeParenthesesSubpatternOnceBegin, subpatternId, capture, inputPosition));
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = frameLocation;
        bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(true));
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, currentAlternativeIndex));
        currentAlternativeIndex = beginTerm + 1;
    }

    void atomParentheticalAssertionBegin(unsigned subpatternId, bool invert, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        int beginTerm = bodyDisjunction->terms.size();

        bodyDisjunction->terms.append(ByteTerm(ByteTerm::TypeParentheticalAssertionOnceBegin, subpatternId, invert, 0));
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = frameLocation;
        bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(true));
        bodyDisjunction->terms[bodyDisjunction->terms.size() - 1].frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, currentAlternativeIndex));
        currentAlternativeIndex = beginTerm + 1;
    }

    unsigned popParenthesesStack()
    {
        ASSERT(m_parenthesesStack.size());
        int stackEnd = m_parenthesesStack.size() - 1;
        unsigned beginTerm = m_parenthesesStack[stackEnd].beginTerm;
        currentAlternativeIndex = m_parenthesesStack[stackEnd].savedAlternativeIndex;
        m_parenthesesStack.shrink(stackEnd);

        ASSERT(beginTerm < bodyDisjunction->terms.size());
        ASSERT(currentAlternativeIndex < bodyDisjunction->terms.size());
        
        return beginTerm;
    }

#ifndef NDEBUG
    void dumpDisjunction(ByteDisjunction* disjunction)
    {
        printf("ByteDisjunction(%p):\n\t", disjunction);
        for (unsigned i = 0; i < disjunction->terms.size(); ++i)
            printf("{ %d } ", disjunction->terms[i].type);
        printf("\n");
    }
#endif

    void closeAlternative(bool inParentheses, int beginTerm)
    {
        int origBeginTerm = beginTerm;
        ASSERT(bodyDisjunction->terms[beginTerm].type == ByteTerm::TypeAlternativeBegin);
        int endIndex = bodyDisjunction->terms.size();

        unsigned frameLocation = bodyDisjunction->terms[beginTerm].frameLocation;

        if (!bodyDisjunction->terms[beginTerm].alternative.next)
            bodyDisjunction->terms.remove(beginTerm);
        else {
            while (bodyDisjunction->terms[beginTerm].alternative.next) {
                beginTerm += bodyDisjunction->terms[beginTerm].alternative.next;
                ASSERT(bodyDisjunction->terms[beginTerm].type == ByteTerm::TypeAlternativeDisjunction);
                bodyDisjunction->terms[beginTerm].alternative.end = endIndex - beginTerm;
                bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
            }
            
            bodyDisjunction->terms[beginTerm].alternative.next = origBeginTerm - beginTerm;

            bodyDisjunction->terms.append(ByteTerm::AlternativeEnd(inParentheses));
            bodyDisjunction->terms[endIndex].frameLocation = frameLocation;
        }
    }

    void atomParenthesesEnd(unsigned lastSubpatternId, int inputPosition, unsigned frameLocation, unsigned quantityCount, QuantifierType quantityType, unsigned callFrameSize = 0)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(true, beginTerm + 1);
        unsigned endTerm = bodyDisjunction->terms.size();

        bool isAssertion = bodyDisjunction->terms[beginTerm].type == ByteTerm::TypeParentheticalAssertionOnceBegin;
        bool invertOrCapture = bodyDisjunction->terms[beginTerm].invertOrCapture;
        unsigned subpatternId = bodyDisjunction->terms[beginTerm].atom.subpatternId;

        bodyDisjunction->terms.append(ByteTerm(isAssertion ? ByteTerm::TypeParentheticalAssertionOnceEnd : ByteTerm::TypeParenthesesSubpatternOnceEnd, subpatternId, invertOrCapture, inputPosition));
        bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        bodyDisjunction->terms[endTerm].frameLocation = frameLocation;

        if (isAssertion || (quantityCount == 1)) {
            bodyDisjunction->terms[beginTerm].atom.quantityCount = quantityCount;
            bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
            bodyDisjunction->terms[endTerm].atom.quantityCount = quantityCount;
            bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
        } else {
            ByteTerm& parenthesesBegin = bodyDisjunction->terms[beginTerm];
            ASSERT(parenthesesBegin.type == ByteTerm::TypeParenthesesSubpatternOnceBegin);

            bool invertOrCapture = parenthesesBegin.invertOrCapture;
            unsigned subpatternId = parenthesesBegin.atom.subpatternId;

            unsigned numSubpatterns = lastSubpatternId - subpatternId + 1;
            ByteDisjunction* parenthesesDisjunction = new ByteDisjunction(numSubpatterns, callFrameSize);
            for (unsigned termInParentheses = beginTerm + 1; termInParentheses < endTerm; ++termInParentheses)
                parenthesesDisjunction->terms.append(bodyDisjunction->terms[termInParentheses]);
            parenthesesDisjunction->terms.append(ByteTerm::PatternEnd());

            bodyDisjunction->terms.shrink(beginTerm);

            m_allParenthesesInfo.append(parenthesesDisjunction);
            bodyDisjunction->terms.append(ByteTerm(ByteTerm::TypeParenthesesSubpattern, subpatternId, parenthesesDisjunction, invertOrCapture, inputPosition));

            bodyDisjunction->terms[beginTerm].atom.quantityCount = quantityCount;
            bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
            bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
        }
    }

    void regexBegin(unsigned numSubpatterns, unsigned callFrameSize)
    {
        bodyDisjunction = new ByteDisjunction(numSubpatterns, callFrameSize);
        bodyDisjunction->terms.append(ByteTerm::AlternativeBegin(false));
        bodyDisjunction->terms[0].frameLocation = 0;
        currentAlternativeIndex = 0;
    }

    void regexEnd()
    {
        closeAlternative(false, 0);
        bodyDisjunction->terms.append(ByteTerm::PatternEnd());
    }

    void alterantiveDisjunction()
    {
        int newAlternativeIndex = bodyDisjunction->terms.size();
        bodyDisjunction->terms[currentAlternativeIndex].alternative.next = newAlternativeIndex - currentAlternativeIndex;
        bodyDisjunction->terms.append(ByteTerm::AlternativeDisjunction(m_parenthesesStack.size()));

        currentAlternativeIndex = newAlternativeIndex;
    }

    void emitDisjunction(PatternDisjunction* disjunction, unsigned inputCountAlreadyChecked = 0, unsigned parenthesesInputCountAlreadyChecked = 0)
    {
        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            unsigned currentCountAlreadyChecked = inputCountAlreadyChecked;
        
            if (alt)
                alterantiveDisjunction();

            PatternAlternative* alternative = disjunction->m_alternatives[alt];
            unsigned minimumSize = alternative->m_minimumSize;

            ASSERT(minimumSize >= parenthesesInputCountAlreadyChecked);
            unsigned countToCheck = minimumSize - parenthesesInputCountAlreadyChecked;
            if (countToCheck)
                checkInput(countToCheck);
            currentCountAlreadyChecked += countToCheck;

            for (unsigned i = 0; i < alternative->m_terms.size(); ++i) {
                PatternTerm& term = alternative->m_terms[i];

                switch (term.type) {
                case PatternTerm::TypeAssertionBOL:
                    assertionBOL(term.inputPosition - currentCountAlreadyChecked);
                    break;

                case PatternTerm::TypeAssertionEOL:
                    assertionEOL(term.inputPosition - currentCountAlreadyChecked);
                    break;

                case PatternTerm::TypeAssertionWordBoundary:
                    assertionWordBoundary(term.invertOrCapture, term.inputPosition - currentCountAlreadyChecked);
                    break;

                case PatternTerm::TypePatternCharacter:
                    atomPatternCharacter(term.patternCharacter, term.inputPosition - currentCountAlreadyChecked, term.frameLocation, term.quantityCount, term.quantityType);
                    break;

                case PatternTerm::TypeCharacterClass:
                    atomCharacterClass(term.characterClass, term.invertOrCapture, term.inputPosition - currentCountAlreadyChecked, term.frameLocation, term.quantityCount, term.quantityType);
                    break;

                case PatternTerm::TypeBackReference:
                    atomBackReference(term.subpatternId, term.inputPosition - currentCountAlreadyChecked, term.frameLocation, term.quantityCount, term.quantityType);
                    break;

                case PatternTerm::TypeParenthesesSubpattern: {
                    unsigned disjunctionAlreadyCheckedCount = 0;
                    if (term.quantityCount == 1) {
                        if (term.quantityType == QuantifierFixedCount) {
                            disjunctionAlreadyCheckedCount = term.parentheses.disjunction->m_minimumSize;
                            unsigned delegateEndInputOffset = term.inputPosition - currentCountAlreadyChecked;
                            atomParenthesesSubpatternBegin(term.parentheses.subpatternId, term.invertOrCapture, delegateEndInputOffset - disjunctionAlreadyCheckedCount, term.frameLocation, term.frameLocation);
                            emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, term.parentheses.disjunction->m_minimumSize);
                            atomParenthesesEnd(term.parentheses.lastSubpatternId, delegateEndInputOffset, term.frameLocation, term.quantityCount, term.quantityType, term.parentheses.disjunction->m_callFrameSize);
                        } else {
                            unsigned delegateEndInputOffset = term.inputPosition - currentCountAlreadyChecked;
                            atomParenthesesSubpatternBegin(term.parentheses.subpatternId, term.invertOrCapture, delegateEndInputOffset - disjunctionAlreadyCheckedCount, term.frameLocation, term.frameLocation + RegexStackSpaceForBackTrackInfoParenthesesOnce);
                            emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, 0);
                            atomParenthesesEnd(term.parentheses.lastSubpatternId, delegateEndInputOffset, term.frameLocation, term.quantityCount, term.quantityType, term.parentheses.disjunction->m_callFrameSize);
                        }
                    } else {
                        unsigned delegateEndInputOffset = term.inputPosition - currentCountAlreadyChecked;
                        atomParenthesesSubpatternBegin(term.parentheses.subpatternId, term.invertOrCapture, delegateEndInputOffset - disjunctionAlreadyCheckedCount, term.frameLocation, 0);
                        emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, 0);
                        atomParenthesesEnd(term.parentheses.lastSubpatternId, delegateEndInputOffset, term.frameLocation, term.quantityCount, term.quantityType, term.parentheses.disjunction->m_callFrameSize);
                    }
                    break;
                }

                case PatternTerm::TypeParentheticalAssertion: {
                    unsigned alternativeFrameLocation = term.inputPosition + RegexStackSpaceForBackTrackInfoParentheticalAssertionOnce;
                    
                    atomParentheticalAssertionBegin(term.parentheses.subpatternId, term.invertOrCapture, term.frameLocation, alternativeFrameLocation);
                    emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, 0);
                    atomParenthesesEnd(term.parentheses.lastSubpatternId, 0, term.frameLocation, term.quantityCount, term.quantityType);
                    break;
                }
                }
            }
        }
    }

private:
    RegexPattern& m_pattern;
    ByteDisjunction* bodyDisjunction;
    unsigned currentAlternativeIndex;
    Vector<ParenthesesStackEntry> m_parenthesesStack;
    Vector<ByteDisjunction*> m_allParenthesesInfo;
};


BytecodePattern* byteCompileRegex(const UString& patternString, unsigned& numSubpatterns, const char*& error, bool ignoreCase, bool multiline)
{
    RegexPattern pattern(ignoreCase, multiline);

    if ((error = compileRegex(patternString, pattern)))
        return 0;

    numSubpatterns = pattern.m_numSubpatterns;

    return ByteCompiler(pattern).compile();
}

int interpretRegex(BytecodePattern* regex, const UChar* input, unsigned start, unsigned length, int* output)
{
    return Interpreter(regex, output, input, start, length).interpret();
}


COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoPatternCharacter) == (RegexStackSpaceForBackTrackInfoPatternCharacter * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoPatternCharacter);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoCharacterClass) == (RegexStackSpaceForBackTrackInfoCharacterClass * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoCharacterClass);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoBackReference) == (RegexStackSpaceForBackTrackInfoBackReference * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoBackReference);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoAlternative) == (RegexStackSpaceForBackTrackInfoAlternative * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoAlternative);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoParentheticalAssertionOnce) == (RegexStackSpaceForBackTrackInfoParentheticalAssertionOnce * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoParentheticalAssertionOnce);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoParenthesesOnce) == (RegexStackSpaceForBackTrackInfoParenthesesOnce * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoParenthesesOnce);
COMPILE_ASSERT(sizeof(Interpreter::BackTrackInfoParentheses) == (RegexStackSpaceForBackTrackInfoParentheses * sizeof(uintptr_t)), CheckRegexStackSpaceForBackTrackInfoParentheses);


} }

#endif
