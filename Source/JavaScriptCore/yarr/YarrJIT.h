/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(YARR_JIT)

#include "MacroAssemblerCodeRef.h"
#include "MatchResult.h"
#include "Yarr.h"
#include "YarrPattern.h"

#if CPU(X86) && !COMPILER(MSVC)
#define YARR_CALL __attribute__ ((regparm (3)))
#else
#define YARR_CALL
#endif

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
constexpr size_t patternContextBufferSize = 8192; // Space caller allocates to save nested parenthesis context
#endif

namespace JSC {

class VM;
class ExecutablePool;

namespace Yarr {

enum class JITFailureReason : uint8_t {
    DecodeSurrogatePair,
    BackReference,
    ForwardReference,
    VariableCountedParenthesisWithNonZeroMinimum,
    ParenthesizedSubpattern,
    FixedCountParenthesizedSubpattern,
    ExecutableMemoryAllocationFailure,
};

class YarrCodeBlock {
#if CPU(X86_64) || CPU(ARM64)
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    typedef MatchResult (*YarrJITCode8)(const LChar* input, unsigned start, unsigned length, int* output, void* freeParenContext, unsigned parenContextSize) YARR_CALL;
    typedef MatchResult (*YarrJITCode16)(const UChar* input, unsigned start, unsigned length, int* output, void* freeParenContext, unsigned parenContextSize) YARR_CALL;
    typedef MatchResult (*YarrJITCodeMatchOnly8)(const LChar* input, unsigned start, unsigned length, void*, void* freeParenContext, unsigned parenContextSize) YARR_CALL;
    typedef MatchResult (*YarrJITCodeMatchOnly16)(const UChar* input, unsigned start, unsigned length, void*, void* freeParenContext, unsigned parenContextSize) YARR_CALL;
#else
    typedef MatchResult (*YarrJITCode8)(const LChar* input, unsigned start, unsigned length, int* output) YARR_CALL;
    typedef MatchResult (*YarrJITCode16)(const UChar* input, unsigned start, unsigned length, int* output) YARR_CALL;
    typedef MatchResult (*YarrJITCodeMatchOnly8)(const LChar* input, unsigned start, unsigned length) YARR_CALL;
    typedef MatchResult (*YarrJITCodeMatchOnly16)(const UChar* input, unsigned start, unsigned length) YARR_CALL;
#endif
#else
    typedef EncodedMatchResult (*YarrJITCode8)(const LChar* input, unsigned start, unsigned length, int* output) YARR_CALL;
    typedef EncodedMatchResult (*YarrJITCode16)(const UChar* input, unsigned start, unsigned length, int* output) YARR_CALL;
    typedef EncodedMatchResult (*YarrJITCodeMatchOnly8)(const LChar* input, unsigned start, unsigned length) YARR_CALL;
    typedef EncodedMatchResult (*YarrJITCodeMatchOnly16)(const UChar* input, unsigned start, unsigned length) YARR_CALL;
#endif

public:
    YarrCodeBlock() = default;

    void setFallBackWithFailureReason(JITFailureReason failureReason) { m_failureReason = failureReason; }
    Optional<JITFailureReason> failureReason() { return m_failureReason; }

    bool has8BitCode() { return m_ref8.size(); }
    bool has16BitCode() { return m_ref16.size(); }
    void set8BitCode(MacroAssemblerCodeRef<Yarr8BitPtrTag> ref) { m_ref8 = ref; }
    void set16BitCode(MacroAssemblerCodeRef<Yarr16BitPtrTag> ref) { m_ref16 = ref; }

    bool has8BitCodeMatchOnly() { return m_matchOnly8.size(); }
    bool has16BitCodeMatchOnly() { return m_matchOnly16.size(); }
    void set8BitCodeMatchOnly(MacroAssemblerCodeRef<YarrMatchOnly8BitPtrTag> matchOnly) { m_matchOnly8 = matchOnly; }
    void set16BitCodeMatchOnly(MacroAssemblerCodeRef<YarrMatchOnly16BitPtrTag> matchOnly) { m_matchOnly16 = matchOnly; }

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    bool usesPatternContextBuffer() { return m_usesPatternContextBuffer; }
    void setUsesPatternContextBuffer() { m_usesPatternContextBuffer = true; }

    MatchResult execute(const LChar* input, unsigned start, unsigned length, int* output, void* freeParenContext, unsigned parenContextSize)
    {
        ASSERT(has8BitCode());
        return MatchResult(untagCFunctionPtr<YarrJITCode8, Yarr8BitPtrTag>(m_ref8.code().executableAddress())(input, start, length, output, freeParenContext, parenContextSize));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length, int* output, void* freeParenContext, unsigned parenContextSize)
    {
        ASSERT(has16BitCode());
        return MatchResult(untagCFunctionPtr<YarrJITCode16, Yarr16BitPtrTag>(m_ref16.code().executableAddress())(input, start, length, output, freeParenContext, parenContextSize));
    }

    MatchResult execute(const LChar* input, unsigned start, unsigned length, void* freeParenContext, unsigned parenContextSize)
    {
        ASSERT(has8BitCodeMatchOnly());
        return MatchResult(untagCFunctionPtr<YarrJITCodeMatchOnly8, YarrMatchOnly8BitPtrTag>(m_matchOnly8.code().executableAddress())(input, start, length, 0, freeParenContext, parenContextSize));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length, void* freeParenContext, unsigned parenContextSize)
    {
        ASSERT(has16BitCodeMatchOnly());
        return MatchResult(untagCFunctionPtr<YarrJITCodeMatchOnly16, YarrMatchOnly16BitPtrTag>(m_matchOnly16.code().executableAddress())(input, start, length, 0, freeParenContext, parenContextSize));
    }
#else
    MatchResult execute(const LChar* input, unsigned start, unsigned length, int* output)
    {
        ASSERT(has8BitCode());
        return MatchResult(reinterpret_cast<YarrJITCode8>(m_ref8.code().executableAddress())(input, start, length, output));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length, int* output)
    {
        ASSERT(has16BitCode());
        return MatchResult(reinterpret_cast<YarrJITCode16>(m_ref16.code().executableAddress())(input, start, length, output));
    }

    MatchResult execute(const LChar* input, unsigned start, unsigned length)
    {
        ASSERT(has8BitCodeMatchOnly());
        return MatchResult(reinterpret_cast<YarrJITCodeMatchOnly8>(m_matchOnly8.code().executableAddress())(input, start, length));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length)
    {
        ASSERT(has16BitCodeMatchOnly());
        return MatchResult(reinterpret_cast<YarrJITCodeMatchOnly16>(m_matchOnly16.code().executableAddress())(input, start, length));
    }
#endif

#if ENABLE(REGEXP_TRACING)
    void *get8BitMatchOnlyAddr()
    {
        if (!has8BitCodeMatchOnly())
            return 0;

        return m_matchOnly8.code().executableAddress();
    }

    void *get16BitMatchOnlyAddr()
    {
        if (!has16BitCodeMatchOnly())
            return 0;

        return m_matchOnly16.code().executableAddress();
    }

    void *get8BitMatchAddr()
    {
        if (!has8BitCode())
            return 0;

        return m_ref8.code().executableAddress();
    }

    void *get16BitMatchAddr()
    {
        if (!has16BitCode())
            return 0;

        return m_ref16.code().executableAddress();
    }
#endif

    size_t size() const
    {
        return m_ref8.size() + m_ref16.size() + m_matchOnly8.size() + m_matchOnly16.size();
    }

    void clear()
    {
        m_ref8 = MacroAssemblerCodeRef<Yarr8BitPtrTag>();
        m_ref16 = MacroAssemblerCodeRef<Yarr16BitPtrTag>();
        m_matchOnly8 = MacroAssemblerCodeRef<YarrMatchOnly8BitPtrTag>();
        m_matchOnly16 = MacroAssemblerCodeRef<YarrMatchOnly16BitPtrTag>();
        m_failureReason = WTF::nullopt;
    }

private:
    MacroAssemblerCodeRef<Yarr8BitPtrTag> m_ref8;
    MacroAssemblerCodeRef<Yarr16BitPtrTag> m_ref16;
    MacroAssemblerCodeRef<YarrMatchOnly8BitPtrTag> m_matchOnly8;
    MacroAssemblerCodeRef<YarrMatchOnly16BitPtrTag> m_matchOnly16;
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    bool m_usesPatternContextBuffer;
#endif
    Optional<JITFailureReason> m_failureReason;
};

enum YarrJITCompileMode {
    MatchOnly,
    IncludeSubpatterns
};
void jitCompile(YarrPattern&, String& patternString, YarrCharSize, VM*, YarrCodeBlock& jitObject, YarrJITCompileMode = IncludeSubpatterns);

} } // namespace JSC::Yarr

#endif
