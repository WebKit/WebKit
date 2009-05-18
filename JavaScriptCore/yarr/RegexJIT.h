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

#include <wtf/Platform.h>

#if ENABLE(YARR_JIT)

#include "RegexPattern.h"
#include <UString.h>

#include <pcre.h>
struct JSRegExp; // temporary, remove when fallback is removed.

namespace JSC {

class JSGlobalData;
class ExecutablePool;

namespace Yarr {

struct RegexCodeBlock {
    void* m_jitCode;
    RefPtr<ExecutablePool> m_executablePool;
    JSRegExp* m_pcreFallback;

    RegexCodeBlock()
        : m_jitCode(0)
        , m_pcreFallback(0)
    {
    }

    ~RegexCodeBlock()
    {
        if (m_pcreFallback)
            jsRegExpFree(m_pcreFallback);
    }
};

void jitCompileRegex(JSGlobalData* globalData, RegexCodeBlock& jitObject, const UString& pattern, unsigned& numSubpatterns, const char*& error, bool ignoreCase = false, bool multiline = false);
int executeRegex(RegexCodeBlock& jitObject, const UChar* input, unsigned start, unsigned length, int* output, int outputArraySize);

} } // namespace JSC::Yarr

#endif

#endif // RegexJIT_h
