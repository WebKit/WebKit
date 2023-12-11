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
#include <JavaScriptCore/LLIntThunks.h>
#include <JavaScriptCore/VM.h>

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

void compileSelector(CompiledSelector&, const CSSSelector*, SelectorContext);

inline unsigned ruleCollectorSimpleSelectorChecker(CompiledSelector& compiledSelector, const Element* element, unsigned* value)
{
    ASSERT(compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker);
#if CPU(ARM64E) && !ENABLE(C_LOOP)
    if (JSC::Options::useJITCage())
        return JSC::vmEntryToCSSJIT(bitwise_cast<uintptr_t>(element), bitwise_cast<uintptr_t>(value), 0, compiledSelector.codeRef.code().taggedPtr());
#endif
    using RuleCollectorSimpleSelectorChecker = unsigned(*)(const Element*, unsigned*);
    return untagCFunctionPtr<RuleCollectorSimpleSelectorChecker, JSC::CSSSelectorPtrTag>(compiledSelector.codeRef.code().taggedPtr())(element, value);
}

inline unsigned querySelectorSimpleSelectorChecker(CompiledSelector& compiledSelector, const Element* element)
{
    ASSERT(compiledSelector.status == SelectorCompilationStatus::SimpleSelectorChecker);
#if CPU(ARM64E) && !ENABLE(C_LOOP)
    if (JSC::Options::useJITCage())
        return JSC::vmEntryToCSSJIT(bitwise_cast<uintptr_t>(element), 0, 0, compiledSelector.codeRef.code().taggedPtr());
#endif
    using QuerySelectorSimpleSelectorChecker = unsigned(*)(const Element*);
    return untagCFunctionPtr<QuerySelectorSimpleSelectorChecker, JSC::CSSSelectorPtrTag>(compiledSelector.codeRef.code().taggedPtr())(element);
}

inline unsigned ruleCollectorSelectorCheckerWithCheckingContext(CompiledSelector& compiledSelector, const Element* element, SelectorChecker::CheckingContext* context, unsigned* value)
{
    ASSERT(compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
#if CPU(ARM64E) && !ENABLE(C_LOOP)
    if (JSC::Options::useJITCage())
        return JSC::vmEntryToCSSJIT(bitwise_cast<uintptr_t>(element), bitwise_cast<uintptr_t>(context), bitwise_cast<uintptr_t>(value), compiledSelector.codeRef.code().taggedPtr());
#endif
    using RuleCollectorSelectorCheckerWithCheckingContext = unsigned(*)(const Element*, SelectorChecker::CheckingContext*, unsigned*);
    return untagCFunctionPtr<RuleCollectorSelectorCheckerWithCheckingContext, JSC::CSSSelectorPtrTag>(compiledSelector.codeRef.code().taggedPtr())(element, context, value);
}

inline unsigned querySelectorSelectorCheckerWithCheckingContext(CompiledSelector& compiledSelector, const Element* element, const SelectorChecker::CheckingContext* context)
{
    ASSERT(compiledSelector.status == SelectorCompilationStatus::SelectorCheckerWithCheckingContext);
#if CPU(ARM64E) && !ENABLE(C_LOOP)
    if (JSC::Options::useJITCage())
        return JSC::vmEntryToCSSJIT(bitwise_cast<uintptr_t>(element), bitwise_cast<uintptr_t>(context), 0, compiledSelector.codeRef.code().taggedPtr());
#endif
    using QuerySelectorSelectorCheckerWithCheckingContext = unsigned(*)(const Element*, const SelectorChecker::CheckingContext*);
    return untagCFunctionPtr<QuerySelectorSelectorCheckerWithCheckingContext, JSC::CSSSelectorPtrTag>(compiledSelector.codeRef.code().taggedPtr())(element, context);
}

} // namespace SelectorCompiler
} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)
