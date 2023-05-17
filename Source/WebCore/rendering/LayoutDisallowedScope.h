/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RuntimeApplicationChecks.h"
#include <wtf/Compiler.h>

namespace WebCore {

class LayoutDisallowedScope {
public:
    enum class Reason : bool { PerformanceOptimization, ReentrancyAvoidance };

    LayoutDisallowedScope(Reason) { ++s_currentScopeCount; }
    ~LayoutDisallowedScope() { --s_currentScopeCount; }

    static bool isLayoutAllowed() { return LIKELY(!s_currentScopeCount) || UNLIKELY(!isInWebProcess()); }

    class AllowedScope {
    public:
        AllowedScope();
        ~AllowedScope();

    private:
        unsigned m_originalCount;
    };

private:
    static unsigned s_currentScopeCount;
};

ALWAYS_INLINE LayoutDisallowedScope::AllowedScope::AllowedScope()
    : m_originalCount { s_currentScopeCount }
{
    s_currentScopeCount = 0;
}

ALWAYS_INLINE LayoutDisallowedScope::AllowedScope::~AllowedScope()
{
    s_currentScopeCount = m_originalCount;
}

}
