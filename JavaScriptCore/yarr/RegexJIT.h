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

#ifndef RegexJIT_h
#define RegexJIT_h

#if ENABLE(YARR_JIT)

#include "MacroAssembler.h"
#include "RegexInterpreter.h" // temporary, remove when fallback is removed.
#include "RegexPattern.h"
#include "UString.h"

#if CPU(X86) && !COMPILER(MSVC)
#define YARR_CALL __attribute__ ((regparm (3)))
#else
#define YARR_CALL
#endif

namespace JSC {

class JSGlobalData;
class ExecutablePool;

namespace Yarr {

class RegexCodeBlock {
    typedef int (*RegexJITCode)(const UChar* input, unsigned start, unsigned length, int* output) YARR_CALL;

public:
    RegexCodeBlock()
        : m_needFallback(false)
    {
    }

    ~RegexCodeBlock()
    {
    }

    BytecodePattern* getFallback() { return m_fallback.get(); }
    bool isFallback() { return m_needFallback; }
    void setFallback(PassOwnPtr<BytecodePattern> fallback)
    {
        m_fallback = fallback;
        m_needFallback = true;
    }

    bool operator!() { return (!m_ref.m_code.executableAddress() && !m_fallback); }
    void set(MacroAssembler::CodeRef ref) { m_ref = ref; }

    int execute(const UChar* input, unsigned start, unsigned length, int* output)
    {
        return reinterpret_cast<RegexJITCode>(m_ref.m_code.executableAddress())(input, start, length, output);
    }

#if ENABLE(REGEXP_TRACING)
    void *getAddr() { return m_ref.m_code.executableAddress(); }
#endif

private:
    MacroAssembler::CodeRef m_ref;
    OwnPtr<Yarr::BytecodePattern> m_fallback;
    bool m_needFallback;
};

void jitCompileRegex(JSGlobalData* globalData, RegexCodeBlock& jitObject, const UString& pattern, unsigned& numSubpatterns, const char*& error, BumpPointerAllocator* allocator, bool ignoreCase = false, bool multiline = false);

inline int executeRegex(RegexCodeBlock& jitObject, const UChar* input, unsigned start, unsigned length, int* output)
{
    if (jitObject.isFallback())
        return (interpretRegex(jitObject.getFallback(), input, start, length, output));

    return jitObject.execute(input, start, length, output);
}

} } // namespace JSC::Yarr

#endif

#endif // RegexJIT_h
