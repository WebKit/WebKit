/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ExceptionStack.h"

#include "Error.h"
#include <wtf/FixedVector.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExceptionStackContent);

ExceptionStackContent::ExceptionStackContent(std::unique_ptr<Vector<StackFrame>>&& stack)
    : m_stack(WTFMove(stack))
{
}

ExceptionStackContent::ExceptionStackContent(LineColumn lineColumn, String&& stackString, String&& sourceURL)
    : m_lineColumn(lineColumn)
    , m_stackString(WTFMove(stackString))
    , m_sourceURL(WTFMove(sourceURL))
{
}

template<typename Visitor>
void ExceptionStackContent::visitAggregateImpl(Visitor& visitor)
{
    {
        Locker locker { m_lock };
        if (m_stack) {
            for (StackFrame& frame : *m_stack)
                frame.visitAggregate(visitor);
            visitor.reportExtraMemoryVisited(m_stack->sizeInBytes());
        }
    }
}

DEFINE_VISIT_AGGREGATE(ExceptionStackContent);

bool ExceptionStackContent::isEmpty() const
{
    if (m_stack)
        return m_stack->isEmpty();
    return m_computed.isEmpty();
}

void ExceptionStackContent::computeErrorInfo(VM& vm)
{
    if (m_stack) {
        // Here we use DeferGCForAWhile instead of DeferGC since GC's Heap::runEndPhase can trigger this function. In
        // that case, DeferGC's destructor might trigger another GC cycle which is unexpected.
        DeferGCForAWhile deferGC(vm);
        if (!m_stack->isEmpty()) {
            for (auto& frame : *m_stack) {
                if (frame.hasLineAndColumnInfo()) {
                    m_lineColumn = frame.computeLineAndColumn();
                    m_sourceURL = frame.sourceURLStripped(vm);
                    break;
                }
            }
            m_stackString = Interpreter::stackTraceAsString(vm, *m_stack);
            m_computed = FixedVector<StackFrame::ComputedStackFrame>::map(m_stack->span(), [&](const StackFrame& frame) {
                return frame.computed(vm);
            });
        }
        {
            Locker locker { m_lock };
            m_stack = nullptr;
        }
    }
}

void ExceptionStackContent::finalizeUnconditionally(VM& vm, CollectionScope)
{
    if (!m_stack)
        return;

    // We don't want to keep our stack traces alive forever if the user doesn't access the stack trace.
    // If we did, we might end up keeping functions (and their global objects) alive that happened to
    // get caught in a trace.
    for (const auto& frame : *m_stack) {
        if (!frame.isMarked(vm)) {
            computeErrorInfo(vm);
            return;
        }
    }
}

size_t ExceptionStackContent::sizeInBytes() const
{
    Locker locker { m_lock };
    if (m_stack)
        return m_stack->sizeInBytes();
    return 0;
}

LineColumn ExceptionStackContent::computeLineAndColumn(VM& vm)
{
    computeErrorInfo(vm);
    return m_lineColumn;
}


void ExceptionStack::computeErrorInfo(VM& vm)
{
    if (m_stack)
        m_stack->computeErrorInfo(vm);
}

LineColumn ExceptionStack::computeLineAndColumn(VM& vm)
{
    LineColumn result { };
    if (m_stack)
        result = m_stack->computeLineAndColumn(vm);
    if (m_replacedLineColumn)
        return m_replacedLineColumn;
    return result;
}

StackFrame::ComputedStackFrame ExceptionStack::computedAt(size_t index)
{
    auto computed = m_stack->computed();
    index += m_callDepth;
    if (index < computed.size()) {
        auto frame = computed[index];
        if (m_replacedLineColumn)
            frame.m_lineColumn = m_replacedLineColumn;
        return frame;
    }
    return { };
}

} // namespace JSC
