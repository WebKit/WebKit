/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SourceTaintedness.h"

#include "CodeBlock.h"
#include "JSCellInlines.h"
#include "JSWebAssemblyInstance.h"
#include "StackVisitor.h"
#include "VM.h"

namespace JSC {

String sourceTaintedOriginToString(SourceTaintedOrigin taintedness)
{
    switch (taintedness) {
    case SourceTaintedOrigin::Untainted: return "Untainted"_s;
    case SourceTaintedOrigin::KnownTainted: return "KnownTainted"_s;
    case SourceTaintedOrigin::IndirectlyTainted: return "IndirectlyTainted"_s;
    case SourceTaintedOrigin::IndirectlyTaintedByHistory: return "IndirectlyTaintedByHistory"_s;
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

static SourceProvider* sourceProviderIfCouldBeTainted(StackVisitor& visitor)
{
#if ENABLE(WEBASSEMBLY)
    if (visitor->callFrame()->callee().isNativeCallee() && visitor->callFrame()->wasmInstance()) {
        JSWebAssemblyInstance* instance = std::bit_cast<JSWebAssemblyInstance*>(*visitor->callFrame()->addressOfCodeBlock());
        auto* sourceProvider = instance->sourceProvider();
        return sourceProvider->couldBeTainted() ? sourceProvider : nullptr;
    }
#endif

    auto* codeBlock = visitor->codeBlock();
    if (!codeBlock || !codeBlock->couldBeTainted())
        return nullptr;
    return codeBlock->source().provider();
}

static std::pair<SourceTaintedness, URL> highestSourceTaintednessOnStack(VM& vm, CallFrame* callFrame, std::span<const SourceTaintKind> kinds)
{
    if (!vm.mightBeExecutingTaintedCode())
        return { };
    auto result = SourceTaintedness::forEveryKind(SourceTaintedOrigin::IndirectlyTaintedByHistory);

    URL sourceURL;
    StackVisitor::visit(callFrame, vm, [&](StackVisitor& visitor) -> IterationStatus {
        auto* sourceProvider = sourceProviderIfCouldBeTainted(visitor);
        if (!sourceProvider)
            return IterationStatus::Continue;

        if (result[SourceTaintKind::ScriptTrackingPrivacy] != SourceTaintedOrigin::KnownTainted && sourceProvider->sourceTaintedOrigin(SourceTaintKind::ScriptTrackingPrivacy) == SourceTaintedOrigin::KnownTainted)
            sourceURL = sourceProvider->sourceOrigin().url();

        bool isKnownTaintedForEveryKind = true;
        for (auto kind : kinds) {
            result[kind] = std::max(result[kind], sourceProvider->sourceTaintedOrigin(kind));
            isKnownTaintedForEveryKind &= result[kind] == SourceTaintedOrigin::KnownTainted;
        }
        return isKnownTaintedForEveryKind ? IterationStatus::Done : IterationStatus::Continue;
    });

    return { result, WTF::move(sourceURL) };
}

std::pair<SourceTaintedOrigin, URL> sourceTaintedOriginFromStack(VM& vm, CallFrame* callFrame)
{
    constexpr std::array kinds { SourceTaintKind::ScriptTrackingPrivacy };
    auto [taintedness, sourceURL] = highestSourceTaintednessOnStack(vm, callFrame, kinds);
    return { taintedness[SourceTaintKind::ScriptTrackingPrivacy], WTF::move(sourceURL) };
}

std::pair<SourceTaintedness, URL> sourceTaintednessFromStack(VM& vm, CallFrame* callFrame)
{
    return highestSourceTaintednessOnStack(vm, callFrame, allSourceTaintKinds);
}

SourceTaintedness computeNewSourceTaintednessFromStack(VM& vm, CallFrame* callFrame)
{
    if (!vm.mightBeExecutingTaintedCode())
        return { };

    auto result = SourceTaintedness::forEveryKind(SourceTaintedOrigin::IndirectlyTaintedByHistory);
    StackVisitor::visit(callFrame, vm, [&](StackVisitor& visitor) -> IterationStatus {
        auto* sourceProvider = sourceProviderIfCouldBeTainted(visitor);
        if (!sourceProvider)
            return IterationStatus::Continue;

        bool isIndirectlyTaintedForEveryKind = true;
        for (auto kind : allSourceTaintKinds) {
            if (sourceProvider->sourceTaintedOrigin(kind) >= SourceTaintedOrigin::IndirectlyTainted)
                result[kind] = SourceTaintedOrigin::IndirectlyTainted;
            isIndirectlyTaintedForEveryKind &= result[kind] == SourceTaintedOrigin::IndirectlyTainted;
        }
        return isIndirectlyTaintedForEveryKind ? IterationStatus::Done : IterationStatus::Continue;
    });

    return result;
}

void markSourceAsPrevalentDomainTainted(VM& vm, SourceProvider& provider)
{
    provider.setSourceTaintedOrigin(SourceTaintKind::PrevalentDomain, SourceTaintedOrigin::KnownTainted);
    vm.setMightHavePrevalentDomainTaintedCode();
}

bool isPrevalentDomainTaintedCodeOnStack(VM& vm, CallFrame* callFrame)
{
    if (!callFrame || !vm.mightHavePrevalentDomainTaintedCode())
        return false;

    bool mightBeExecutingTaintedCode = vm.mightBeExecutingTaintedCode();

    bool foundPrevalentDomainTaintedCode = false;
    bool foundOtherNonBuiltinCodeSinceVMEntry = false;
    bool isBeforeVMEntry = true;
    StackVisitor::visit(callFrame, vm, [&](StackVisitor& visitor) -> IterationStatus {
        SourceProvider* sourceProvider = nullptr;
#if ENABLE(WEBASSEMBLY)
        if (visitor->callFrame()->callee().isNativeCallee() && visitor->callFrame()->wasmInstance())
            sourceProvider = std::bit_cast<JSWebAssemblyInstance*>(*visitor->callFrame()->addressOfCodeBlock())->sourceProvider();
#endif
        if (auto* codeBlock = visitor->codeBlock(); codeBlock && !codeBlock->unlinkedCodeBlock()->isBuiltinFunction())
            sourceProvider = codeBlock->source().provider();

        if (sourceProvider) {
            if (sourceProvider->sourceTaintedOrigin(SourceTaintKind::PrevalentDomain) >= SourceTaintedOrigin::IndirectlyTainted) {
                foundPrevalentDomainTaintedCode = true;
                return IterationStatus::Done;
            }
            if (isBeforeVMEntry)
                foundOtherNonBuiltinCodeSinceVMEntry = true;
            if (!mightBeExecutingTaintedCode)
                return IterationStatus::Done;
        }
        if (visitor->callerIsEntryFrame()) {
            isBeforeVMEntry = false;
            if (!mightBeExecutingTaintedCode)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });

    return foundPrevalentDomainTaintedCode || !foundOtherNonBuiltinCodeSinceVMEntry;
}

} // namespace JSC
