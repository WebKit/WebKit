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

#pragma once

#include "JSDestructibleObject.h"
#include "StackFrame.h"
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class ExceptionStackContent final : public RefCounted<ExceptionStackContent> {
    WTF_MAKE_TZONE_ALLOCATED(ExceptionStackContent);
public:
    static Ref<ExceptionStackContent> create(std::unique_ptr<Vector<StackFrame>>&& stack)
    {
        return adoptRef(*new ExceptionStackContent(WTFMove(stack)));
    }

    static Ref<ExceptionStackContent> create(LineColumn lineColumn, String&& stackString, String&& sourceURL)
    {
        return adoptRef(*new ExceptionStackContent(lineColumn, WTFMove(stackString), WTFMove(sourceURL)));
    }

    bool isEmpty() const;

    DECLARE_VISIT_AGGREGATE;

    void computeErrorInfo(VM&);
    void finalizeUnconditionally(VM&, CollectionScope);

    const String& sourceURL() const { return m_sourceURL; }
    const String& stackString() const { return m_stackString; }
    LineColumn lineColumn() const { return m_lineColumn; }
    const FixedVector<StackFrame::ComputedStackFrame>& computed() { return m_computed; }

    size_t sizeInBytes() const;

    LineColumn computeLineAndColumn(VM&);

private:
    ExceptionStackContent(std::unique_ptr<Vector<StackFrame>>&&);
    ExceptionStackContent(LineColumn, String&&, String&&);

    mutable Lock m_lock;
    LineColumn m_lineColumn;
    String m_stackString;
    String m_sourceURL;
    std::unique_ptr<Vector<StackFrame>> m_stack;
    FixedVector<StackFrame::ComputedStackFrame> m_computed;
};

class ExceptionStack {
public:
    ExceptionStack(RefPtr<ExceptionStackContent> stack)
        : m_stack(WTFMove(stack))
    {
    }

    ExceptionStack(RefPtr<ExceptionStackContent> stack, LineColumn replacedLineColumn, unsigned callDepth)
        : m_stack(WTFMove(stack))
        , m_replacedLineColumn(replacedLineColumn)
        , m_callDepth(callDepth)
    {
    }

    LineColumn lineColumn() const
    {
        if (m_replacedLineColumn)
            return m_replacedLineColumn;
        if (m_stack)
            return m_stack->lineColumn();
        return { };
    }

    size_t sizeInBytes() const
    {
        if (m_stack)
            return m_stack->sizeInBytes();
        return 0;
    }

    bool isEmpty() const
    {
        return !size();
    }

    size_t size() const
    {
        if (!m_stack)
            return 0;
        size_t originalSize = m_stack->computed().size();
        if (originalSize >= m_callDepth)
            return originalSize - m_callDepth;
        return 0;
    }

    RefPtr<ExceptionStackContent> stack() const { return m_stack; }

    void computeErrorInfo(VM&);
    LineColumn computeLineAndColumn(VM&);
    StackFrame::ComputedStackFrame computedAt(size_t);

private:
    const RefPtr<ExceptionStackContent> m_stack;
    LineColumn m_replacedLineColumn { };
    unsigned m_callDepth { };
};

} // namespace JSC
