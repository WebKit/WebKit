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
#include "VM.h"
#include "Yarr.h"
#include "YarrPattern.h"

#define YARR_CALL

namespace JSC {

class VM;
class ExecutablePool;

namespace Yarr {

class YarrCodeBlock;

enum class JITFailureReason : uint8_t {
    DecodeSurrogatePair,
    BackReference,
    ForwardReference,
    VariableCountedParenthesisWithNonZeroMinimum,
    ParenthesizedSubpattern,
    FixedCountParenthesizedSubpattern,
    ParenthesisNestedTooDeep,
    ExecutableMemoryAllocationFailure,
};

class MatchingContextHolder {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    MatchingContextHolder(VM&, YarrCodeBlock*, MatchFrom matchFrom = MatchFrom::VMThread);
    ~MatchingContextHolder();

    static ptrdiff_t offsetOfStackLimit() { return OBJECT_OFFSETOF(MatchingContextHolder, m_stackLimit); }
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    static ptrdiff_t offsetOfPatternContextBuffer() { return OBJECT_OFFSETOF(MatchingContextHolder, m_patternContextBuffer); }
    static ptrdiff_t offsetOfPatternContextBufferSize() { return OBJECT_OFFSETOF(MatchingContextHolder, m_patternContextBufferSize); }
#endif

private:
    VM& m_vm;
    void* m_stackLimit;
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    void* m_patternContextBuffer { nullptr };
    unsigned m_patternContextBufferSize { 0 };
#endif
};

#if CPU(ARM64E)
extern "C" EncodedMatchResult vmEntryToYarrJIT(const void* input, unsigned start, unsigned length, int* output, MatchingContextHolder* matchingContext, const void* codePtr);
extern "C" void vmEntryToYarrJITAfter(void);
#endif

class YarrCodeBlock {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(YarrCodeBlock);

    using YarrJITCode8 = EncodedMatchResult (*)(const LChar* input, unsigned start, unsigned length, int* output, MatchingContextHolder& matchingContext) YARR_CALL;
    using YarrJITCode16 = EncodedMatchResult (*)(const UChar* input, unsigned start, unsigned length, int* output, MatchingContextHolder& matchingContext) YARR_CALL;
    using YarrJITCodeMatchOnly8 = EncodedMatchResult (*)(const LChar* input, unsigned start, unsigned length, void*, MatchingContextHolder& matchingContext) YARR_CALL;
    using YarrJITCodeMatchOnly16 = EncodedMatchResult (*)(const UChar* input, unsigned start, unsigned length, void*, MatchingContextHolder& matchingContext) YARR_CALL;

public:
    YarrCodeBlock() = default;

    void setFallBackWithFailureReason(JITFailureReason failureReason) { m_failureReason = failureReason; }
    std::optional<JITFailureReason> failureReason() { return m_failureReason; }

    bool has8BitCode() { return m_ref8.size(); }
    bool has16BitCode() { return m_ref16.size(); }
    void set8BitCode(MacroAssemblerCodeRef<Yarr8BitPtrTag> ref) { m_ref8 = ref; }
    void set16BitCode(MacroAssemblerCodeRef<Yarr16BitPtrTag> ref) { m_ref16 = ref; }

    bool has8BitCodeMatchOnly() { return m_matchOnly8.size(); }
    bool has16BitCodeMatchOnly() { return m_matchOnly16.size(); }
    void set8BitCodeMatchOnly(MacroAssemblerCodeRef<YarrMatchOnly8BitPtrTag> matchOnly) { m_matchOnly8 = matchOnly; }
    void set16BitCodeMatchOnly(MacroAssemblerCodeRef<YarrMatchOnly16BitPtrTag> matchOnly) { m_matchOnly16 = matchOnly; }

    bool usesPatternContextBuffer() { return m_usesPatternContextBuffer; }
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    void setUsesPatternContextBuffer() { m_usesPatternContextBuffer = true; }
#endif

    MatchResult execute(const LChar* input, unsigned start, unsigned length, int* output, MatchingContextHolder& matchingContext)
    {
        ASSERT(has8BitCode());
#if CPU(ARM64E)
        if (Options::useJITCage())
            return MatchResult(vmEntryToYarrJIT(input, start, length, output, &matchingContext, retagCodePtr<Yarr8BitPtrTag, YarrEntryPtrTag>(m_ref8.code().executableAddress())));
#endif
        return MatchResult(untagCFunctionPtr<YarrJITCode8, Yarr8BitPtrTag>(m_ref8.code().executableAddress())(input, start, length, output, matchingContext));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length, int* output, MatchingContextHolder& matchingContext)
    {
        ASSERT(has16BitCode());
#if CPU(ARM64E)
        if (Options::useJITCage())
            return MatchResult(vmEntryToYarrJIT(input, start, length, output, &matchingContext, retagCodePtr<Yarr16BitPtrTag, YarrEntryPtrTag>(m_ref16.code().executableAddress())));
#endif
        return MatchResult(untagCFunctionPtr<YarrJITCode16, Yarr16BitPtrTag>(m_ref16.code().executableAddress())(input, start, length, output, matchingContext));
    }

    MatchResult execute(const LChar* input, unsigned start, unsigned length, MatchingContextHolder& matchingContext)
    {
        ASSERT(has8BitCodeMatchOnly());
#if CPU(ARM64E)
        if (Options::useJITCage())
            return MatchResult(vmEntryToYarrJIT(input, start, length, nullptr, &matchingContext, retagCodePtr<YarrMatchOnly8BitPtrTag, YarrEntryPtrTag>(m_matchOnly8.code().executableAddress())));
#endif
        return MatchResult(untagCFunctionPtr<YarrJITCodeMatchOnly8, YarrMatchOnly8BitPtrTag>(m_matchOnly8.code().executableAddress())(input, start, length, nullptr, matchingContext));
    }

    MatchResult execute(const UChar* input, unsigned start, unsigned length, MatchingContextHolder& matchingContext)
    {
        ASSERT(has16BitCodeMatchOnly());
#if CPU(ARM64E)
        if (Options::useJITCage())
            return MatchResult(vmEntryToYarrJIT(input, start, length, nullptr, &matchingContext, retagCodePtr<YarrMatchOnly16BitPtrTag, YarrEntryPtrTag>(m_matchOnly16.code().executableAddress())));
#endif
        return MatchResult(untagCFunctionPtr<YarrJITCodeMatchOnly16, YarrMatchOnly16BitPtrTag>(m_matchOnly16.code().executableAddress())(input, start, length, nullptr, matchingContext));
    }

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
        m_failureReason = std::nullopt;
    }

private:
    MacroAssemblerCodeRef<Yarr8BitPtrTag> m_ref8;
    MacroAssemblerCodeRef<Yarr16BitPtrTag> m_ref16;
    MacroAssemblerCodeRef<YarrMatchOnly8BitPtrTag> m_matchOnly8;
    MacroAssemblerCodeRef<YarrMatchOnly16BitPtrTag> m_matchOnly16;
    bool m_usesPatternContextBuffer { false };
    std::optional<JITFailureReason> m_failureReason;
};

enum YarrJITCompileMode {
    MatchOnly,
    IncludeSubpatterns
};
void jitCompile(YarrPattern&, String& patternString, YarrCharSize, VM*, YarrCodeBlock& jitObject, YarrJITCompileMode = IncludeSubpatterns);

} } // namespace JSC::Yarr

#endif
