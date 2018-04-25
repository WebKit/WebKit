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

SelectorCompilationStatus compileSelector(const CSSSelector*, SelectorContext, JSC::MacroAssemblerCodeRef<CSSSelectorPtrTag>& outputCodeRef);

inline RuleCollectorSimpleSelectorChecker ruleCollectorSimpleSelectorCheckerFunction(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker);
    return WTF::untagCFunctionPtr<RuleCollectorSimpleSelectorChecker, CSSSelectorPtrTag>(executableAddress);
}

inline QuerySelectorSimpleSelectorChecker querySelectorSimpleSelectorCheckerFunction(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SimpleSelectorChecker);
    return WTF::untagCFunctionPtr<QuerySelectorSimpleSelectorChecker, CSSSelectorPtrTag>(executableAddress);
}

inline RuleCollectorSelectorCheckerWithCheckingContext ruleCollectorSelectorCheckerFunctionWithCheckingContext(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
    return WTF::untagCFunctionPtr<RuleCollectorSelectorCheckerWithCheckingContext, CSSSelectorPtrTag>(executableAddress);
}

inline QuerySelectorSelectorCheckerWithCheckingContext querySelectorSelectorCheckerFunctionWithCheckingContext(void* executableAddress, SelectorCompilationStatus compilationStatus)
{
    ASSERT_UNUSED(compilationStatus, compilationStatus == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
    return WTF::untagCFunctionPtr<QuerySelectorSelectorCheckerWithCheckingContext, CSSSelectorPtrTag>(executableAddress);
}

} // namespace SelectorCompiler
} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)
