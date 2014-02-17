/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SelectorCompiler_h
#define SelectorCompiler_h

#if ENABLE(CSS_SELECTOR_JIT)

#include "SelectorChecker.h"
#include <JavaScriptCore/MacroAssemblerCodeRef.h>

namespace JSC {
class MacroAssemblerCodeRef;
class VM;
}

namespace WebCore {

class CSSSelector;
class Element;
class RenderStyle;

class SelectorCompilationStatus {
public:
    enum Status {
        NotCompiled,
        CannotCompile,
        SimpleSelectorChecker,
        SelectorCheckerWithCheckingContext
    };

    SelectorCompilationStatus()
        : m_status(NotCompiled)
    { }

    SelectorCompilationStatus(Status status)
        : m_status(status)
    { }

    operator Status() const { return m_status; }

private:
    Status m_status;
};

namespace SelectorCompiler {

struct CheckingContext {
    SelectorChecker::Mode resolvingMode;
    RenderStyle* elementStyle;
};

enum class SelectorContext {
    // Rule Collector needs a resolvingMode and can modify the tree as it matches.
    RuleCollector,

    // Query Selector does not modify the tree and never match :visited.
    QuerySelector
};

typedef unsigned (*SimpleSelectorChecker)(Element*);
typedef unsigned (*SelectorCheckerWithCheckingContext)(Element*, const CheckingContext*);
SelectorCompilationStatus compileSelector(const CSSSelector*, JSC::VM*, SelectorContext, JSC::MacroAssemblerCodeRef& outputCodeRef);

inline SimpleSelectorChecker simpleSelectorCheckerFunction(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker);
    return reinterpret_cast<SimpleSelectorChecker>(executableAddress);
}

inline SelectorCheckerWithCheckingContext selectorCheckerFunctionWithCheckingContext(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
    return reinterpret_cast<SelectorCheckerWithCheckingContext>(executableAddress);
}


} // namespace SelectorCompiler
} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // SelectorCompiler_h
