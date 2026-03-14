/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2023 the V8 project authors. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmBaselineData.h>
#include <JavaScriptCore/WasmCallProfile.h>
#include <JavaScriptCore/WasmCallee.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class MergedProfile;
class Module;
class InliningDecision;

class InliningNode {
    WTF_MAKE_TZONE_ALLOCATED(InliningNode);
    WTF_MAKE_NONMOVABLE(InliningNode);
public:
    using CallSite = Vector<InliningNode*, CallProfile::maxPolymorphicCallees>;

    InliningNode(const IPIntCallee&, InliningNode* caller, uint8_t caseIndex, unsigned callProfileIndex, size_t wasmSize, double relativeCallCount);

    void inlineNode(InliningDecision&);

    const IPIntCallee& callee() const { return m_callee; }
    InliningNode* caller() const { return m_caller; }
    const Vector<CallSite>& callSites() const LIFETIME_BOUND { return m_callSites; }
    bool isInlined() const { return m_isInlined; }
    bool isUnused() const { return m_isUnused; }
    uint8_t caseIndex() const { return m_caseIndex; }
    uint32_t depth() const { return m_depth; }
    unsigned callProfileIndex() const { return m_callProfileIndex; }
    double relativeCallCount() const { return m_relativeCallCount; }
    size_t wasmSize() const { return m_wasmSize; }
    double score() const;

    InliningNode* callTarget(FunctionSpaceIndex functionIndexSpace, unsigned callProfileIndex);

private:
    SUPPRESS_UNCOUNTED_MEMBER const IPIntCallee& m_callee;
    InliningNode* m_caller;
    Vector<CallSite> m_callSites;
    bool m_isInlined { false };
    bool m_isUnused { true };
    uint8_t m_caseIndex { 0 };
    uint32_t m_depth { 0 };
    unsigned m_callProfileIndex { 0 };
    size_t m_wasmSize { 0 };
    double m_relativeCallCount { 0.0 };
};

class InliningDecision final {
    WTF_MAKE_TZONE_ALLOCATED(InliningDecision);
    WTF_MAKE_NONMOVABLE(InliningDecision);
    friend class Module;
    friend class InliningNode;
public:
    InliningDecision(Module&, const IPIntCallee& rootCallee);

    MergedProfile* profileForCallee(const IPIntCallee&);

    void expand();

    InliningNode* root() LIFETIME_BOUND { return &m_root; }

private:
    bool canInline(InliningNode*, size_t initialWasmSize, size_t inlinedWasmSize);

    SUPPRESS_UNCOUNTED_MEMBER Module& m_module;
    SegmentedVector<InliningNode, 16> m_arena;
    UncheckedKeyHashMap<const IPIntCallee*, std::unique_ptr<MergedProfile>> m_profiles;
    InliningNode& m_root;
    uint32_t m_inlinedCount { 0 };
    double m_maxGrowthFactor { 0 };
    size_t m_budgetCap { 0 };
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
