/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_SELECTOR_JIT)

#include "CompiledSelector.h"
#include "SelectorChecker.h"

#define CSS_SELECTOR_JIT_PROFILING 0

namespace WebCore {

class CSSSelector;
class Element;

namespace SelectorCompiler {

enum class SelectorContext {
    // Rule Collector needs a resolvingMode and can modify the tree as it matches.
    RuleCollector,

    // Query Selector does not modify the tree and never match :visited.
    QuerySelector
};

typedef unsigned (*RuleCollectorSimpleSelectorChecker)(const Element*, unsigned*);
typedef unsigned (*QuerySelectorSimpleSelectorChecker)(const Element*);

typedef unsigned (*RuleCollectorSelectorCheckerWithCheckingContext)(const Element*, SelectorChecker::CheckingContext*, unsigned*);
typedef unsigned (*QuerySelectorSelectorCheckerWithCheckingContext)(const Element*, const SelectorChecker::CheckingContext*);

SelectorCompilationStatus compileSelector(const CSSSelector*, SelectorContext, JSC::MacroAssemblerCodeRef& outputCodeRef);

inline RuleCollectorSimpleSelectorChecker ruleCollectorSimpleSelectorCheckerFunction(const JSC::MacroAssemblerCodeRef& codeRef, void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker);
    JSC::PtrTag tag = JSC::ptrTag(&codeRef, SelectorContext::RuleCollector);
    return JSC::untagCFunctionPtr<RuleCollectorSimpleSelectorChecker>(executableAddress, tag);
}

inline QuerySelectorSimpleSelectorChecker querySelectorSimpleSelectorCheckerFunction(const JSC::MacroAssemblerCodeRef& codeRef, void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker);
    JSC::PtrTag tag = JSC::ptrTag(&codeRef, SelectorContext::QuerySelector);
    return JSC::untagCFunctionPtr<QuerySelectorSimpleSelectorChecker>(executableAddress, tag);
}

inline RuleCollectorSelectorCheckerWithCheckingContext ruleCollectorSelectorCheckerFunctionWithCheckingContext(const JSC::MacroAssemblerCodeRef& codeRef, void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
    JSC::PtrTag tag = JSC::ptrTag(&codeRef, SelectorContext::RuleCollector);
    return JSC::untagCFunctionPtr<RuleCollectorSelectorCheckerWithCheckingContext>(executableAddress, tag);
}

inline QuerySelectorSelectorCheckerWithCheckingContext querySelectorSelectorCheckerFunctionWithCheckingContext(const JSC::MacroAssemblerCodeRef& codeRef, void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
    JSC::PtrTag tag = JSC::ptrTag(&codeRef, SelectorContext::QuerySelector);
    return JSC::untagCFunctionPtr<QuerySelectorSelectorCheckerWithCheckingContext>(executableAddress, tag);
}

} // namespace SelectorCompiler
} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)
