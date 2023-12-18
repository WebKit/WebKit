/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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
#include "SourceTaintedOrigin.h"

#include "CodeBlock.h"
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

SourceTaintedOrigin sourceTaintedOriginFromStack(VM& vm, CallFrame* callFrame)
{
    if (!vm.mightBeExecutingTaintedCode())
        return SourceTaintedOrigin::Untainted;
    SourceTaintedOrigin result = SourceTaintedOrigin::IndirectlyTaintedByHistory;

    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) -> IterationStatus {
        if (!visitor->codeBlock() || !visitor->codeBlock()->couldBeTainted())
            return IterationStatus::Continue;

        result = std::max(result, visitor->codeBlock()->source().provider()->sourceTaintedOrigin());
        return result == SourceTaintedOrigin::KnownTainted ? IterationStatus::Done : IterationStatus::Continue;
    });

    return result;
}

SourceTaintedOrigin computeNewSourceTaintedOriginFromStack(VM& vm, CallFrame* callFrame)
{
    if (!vm.mightBeExecutingTaintedCode())
        return SourceTaintedOrigin::Untainted;

    SourceTaintedOrigin result = SourceTaintedOrigin::IndirectlyTaintedByHistory;
    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) -> IterationStatus {
        if (visitor->codeBlock() && visitor->codeBlock()->couldBeTainted()) {
            SourceTaintedOrigin currentTaintedOrigin = visitor->codeBlock()->source().provider()->sourceTaintedOrigin();
            if (currentTaintedOrigin >= SourceTaintedOrigin::IndirectlyTainted) {
                result = SourceTaintedOrigin::IndirectlyTainted;
                return IterationStatus::Done;
            }
        }

        return IterationStatus::Continue;
    });

    return result;
}

} // namespace JSC
