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
    void LEGACY_NODELETE assertionBOL() { }
    void LEGACY_NODELETE assertionEOL() { }
    void LEGACY_NODELETE assertionBOI() { }
    void LEGACY_NODELETE assertionEOI(bool) { }
    void LEGACY_NODELETE assertionWordBoundary(bool) { }
    void LEGACY_NODELETE atomPatternCharacter(char32_t, bool) { }
    void LEGACY_NODELETE atomBuiltInCharacterClass(BuiltInCharacterClassID, bool) { }
    void LEGACY_NODELETE atomCharacterClassBegin(bool = false) { }
    void LEGACY_NODELETE atomCharacterClassAtom(char16_t) { }
    void LEGACY_NODELETE atomCharacterClassRange(char16_t, char16_t) { }
    void LEGACY_NODELETE atomCharacterClassBuiltIn(BuiltInCharacterClassID, bool) { }
    void LEGACY_NODELETE atomClassStringDisjunction(Vector<Vector<char32_t>>&) { }
    void LEGACY_NODELETE atomCharacterClassSetOp(CharacterClassSetOp) { }
    void LEGACY_NODELETE atomCharacterClassPushNested(bool) { }
    void LEGACY_NODELETE atomCharacterClassPopNested(bool) { }
    void LEGACY_NODELETE atomCharacterClassEnd() { }
    void LEGACY_NODELETE atomParenthesesSubpatternBegin(bool, std::optional<String> = std::nullopt) { }
    void LEGACY_NODELETE atomParentheticalAssertionBegin(bool, MatchDirection) { }
    void LEGACY_NODELETE atomParentheticalModifierBegin(OptionSet<Flags>, OptionSet<Flags>) { }
    void LEGACY_NODELETE atomParenthesesEnd() { }
    void LEGACY_NODELETE atomBackReference(unsigned) { }
    void LEGACY_NODELETE atomNamedBackReference(const String&) { }
    void LEGACY_NODELETE atomNamedForwardReference(const String&) { }
    void LEGACY_NODELETE quantifyAtom(unsigned, unsigned, bool) { }
    void LEGACY_NODELETE disjunction(CreateDisjunctionPurpose) { }
    void LEGACY_NODELETE resetForReparsing() { }

    constexpr static bool LEGACY_NODELETE abortedDueToError() { return false; }
    constexpr static ErrorCode LEGACY_NODELETE abortErrorCode() { return ErrorCode::NoError; }
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
