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

#include "Options.h"
#include "YarrFlags.h"
#include "YarrParser.h"

namespace JSC { namespace Yarr {

class SyntaxChecker {
public:
    NODELETE void assertionBOL() { }
    NODELETE void assertionEOL() { }
    NODELETE void assertionBOI() { }
    NODELETE void assertionEOI(bool) { }
    NODELETE void assertionWordBoundary(bool) { }
    NODELETE void atomPatternCharacter(char32_t, bool) { }
    NODELETE void atomBuiltInCharacterClass(BuiltInCharacterClassID, bool) { }
    NODELETE void atomCharacterClassBegin(bool = false) { }
    NODELETE void atomCharacterClassAtom(char16_t) { }
    NODELETE void atomCharacterClassRange(char16_t, char16_t) { }
    NODELETE void atomCharacterClassBuiltIn(BuiltInCharacterClassID, bool) { }
    NODELETE void atomClassStringDisjunction(Vector<Vector<char32_t>>&) { }
    NODELETE void atomCharacterClassSetOp(CharacterClassSetOp) { }
    NODELETE void atomCharacterClassPushNested(bool) { }
    NODELETE void atomCharacterClassPopNested(bool) { }
    NODELETE void atomCharacterClassEnd() { }
    NODELETE void atomParenthesesSubpatternBegin(bool, std::optional<String> = std::nullopt) { }
    NODELETE void atomParentheticalAssertionBegin(bool, MatchDirection) { }
    NODELETE void atomParentheticalModifierBegin(OptionSet<Flags>, OptionSet<Flags>) { }
    NODELETE void atomParenthesesEnd() { }
    NODELETE void atomBackReference(unsigned) { }
    NODELETE void atomNamedBackReference(const String&) { }
    NODELETE void atomNamedForwardReference(const String&) { }
    NODELETE void quantifyAtom(unsigned, unsigned, bool) { }
    NODELETE void disjunction(CreateDisjunctionPurpose) { }
    NODELETE void resetForReparsing() { }

    NODELETE constexpr static bool abortedDueToError() { return false; }
    NODELETE constexpr static ErrorCode abortErrorCode() { return ErrorCode::NoError; }
};
static_assert(YarrSyntaxCheckable<SyntaxChecker>);

ErrorCode checkSyntax(StringView pattern, StringView flags)
{
    SyntaxChecker syntaxChecker;

    auto parsedFlags = parseFlags(flags);
    if (!parsedFlags)
        return ErrorCode::InvalidRegularExpressionFlags;

    return parse(syntaxChecker, pattern, compileMode(parsedFlags), quantifyInfinite, true, Options::useRegExpBufferBoundaries());
}

}} // JSC::Yarr
