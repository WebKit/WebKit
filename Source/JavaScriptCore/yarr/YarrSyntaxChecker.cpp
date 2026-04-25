/*
 * Copyright (C) 2011, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
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
#include "YarrSyntaxChecker.h"

#include "YarrFlags.h"
#include "YarrParser.h"

namespace JSC { namespace Yarr {

class SyntaxChecker {
public:
    void NODELETE assertionBOL() { }
    void NODELETE assertionEOL() { }
    void NODELETE assertionWordBoundary(bool) { }
    void NODELETE atomPatternCharacter(char32_t, bool) { }
    void NODELETE atomBuiltInCharacterClass(BuiltInCharacterClassID, bool) { }
    void NODELETE atomCharacterClassBegin(bool = false) { }
    void NODELETE atomCharacterClassAtom(char16_t) { }
    void NODELETE atomCharacterClassRange(char16_t, char16_t) { }
    void NODELETE atomCharacterClassBuiltIn(BuiltInCharacterClassID, bool) { }
    void NODELETE atomClassStringDisjunction(Vector<Vector<char32_t>>&) { }
    void NODELETE atomCharacterClassSetOp(CharacterClassSetOp) { }
    void NODELETE atomCharacterClassPushNested(bool) { }
    void NODELETE atomCharacterClassPopNested(bool) { }
    void NODELETE atomCharacterClassEnd() { }
    void NODELETE atomParenthesesSubpatternBegin(bool = true, std::optional<String> = std::nullopt) { }
    void NODELETE atomParentheticalAssertionBegin(bool, MatchDirection) { }
    void NODELETE atomParentheticalModifierBegin(OptionSet<Flags>, OptionSet<Flags>) { }
    void NODELETE atomParenthesesEnd() { }
    void NODELETE atomBackReference(unsigned) { }
    void NODELETE atomNamedBackReference(const String&) { }
    void NODELETE atomNamedForwardReference(const String&) { }
    void NODELETE quantifyAtom(unsigned, unsigned, bool) { }
    void NODELETE disjunction(CreateDisjunctionPurpose) { }
    void NODELETE resetForReparsing() { }

    constexpr static bool NODELETE abortedDueToError() { return false; }
    constexpr static ErrorCode NODELETE abortErrorCode() { return ErrorCode::NoError; }
};
static_assert(YarrSyntaxCheckable<SyntaxChecker>);

ErrorCode checkSyntax(StringView pattern, StringView flags)
{
    SyntaxChecker syntaxChecker;

    auto parsedFlags = parseFlags(flags);
    if (!parsedFlags)
        return ErrorCode::InvalidRegularExpressionFlags;

    return parse(syntaxChecker, pattern, compileMode(parsedFlags));
}

}} // JSC::Yarr
