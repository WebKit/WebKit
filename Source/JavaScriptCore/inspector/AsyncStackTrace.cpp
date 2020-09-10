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

#include "config.h"
#include "AsyncStackTrace.h"

#include "ScriptCallStack.h"
#include <wtf/Ref.h>

namespace Inspector {

Ref<AsyncStackTrace> AsyncStackTrace::create(Ref<ScriptCallStack>&& callStack, bool singleShot, RefPtr<AsyncStackTrace> parent)
{
    ASSERT(callStack->size());
    return adoptRef(*new AsyncStackTrace(WTFMove(callStack), singleShot, WTFMove(parent)));
}

AsyncStackTrace::AsyncStackTrace(Ref<ScriptCallStack>&& callStack, bool singleShot, RefPtr<AsyncStackTrace> parent)
    : m_callStack(WTFMove(callStack))
    , m_parent(parent)
    , m_singleShot(singleShot)
{
    if (m_parent)
        m_parent->m_childCount++;
}

AsyncStackTrace::~AsyncStackTrace()
{
    if (m_parent)
        remove();
    ASSERT(!m_childCount);
}

bool AsyncStackTrace::isPending() const
{
    return m_state == State::Pending;
}

bool AsyncStackTrace::isLocked() const
{
    return m_state == State::Pending || m_state == State::Active || m_childCount > 1;
}

void AsyncStackTrace::willDispatchAsyncCall(size_t maxDepth)
{
    ASSERT(m_state == State::Pending);
    m_state = State::Active;

    truncate(maxDepth);
}

void AsyncStackTrace::didDispatchAsyncCall()
{
    ASSERT(m_state == State::Active || m_state == State::Canceled);

    if (m_state == State::Active && !m_singleShot) {
        m_state = State::Pending;
        return;
    }

    m_state = State::Dispatched;

    if (!m_childCount)
        remove();
}

void AsyncStackTrace::didCancelAsyncCall()
{
    if (m_state == State::Canceled)
        return;

    if (m_state == State::Pending && !m_childCount)
        remove();

    m_state = State::Canceled;
}

Ref<Protocol::Console::StackTrace> AsyncStackTrace::buildInspectorObject() const
{
    RefPtr<Protocol::Console::StackTrace> topStackTrace;
    RefPtr<Protocol::Console::StackTrace> previousStackTrace;

    auto* stackTrace = this;
    while (stackTrace) {
        auto& callStack = stackTrace->m_callStack;
        ASSERT(callStack->size());

        auto protocolObject = Protocol::Console::StackTrace::create()
            .setCallFrames(callStack->buildInspectorArray())
            .release();

        if (stackTrace->m_truncated)
            protocolObject->setTruncated(true);
        if (callStack->at(0).isNative())
            protocolObject->setTopCallFrameIsBoundary(true);

        if (!topStackTrace)
            topStackTrace = protocolObject.ptr();

        if (previousStackTrace)
            previousStackTrace->setParentStackTrace(protocolObject.copyRef());

        previousStackTrace = WTFMove(protocolObject);
        stackTrace = stackTrace->m_parent.get();
    }

    return topStackTrace.releaseNonNull();
}

void AsyncStackTrace::truncate(size_t maxDepth)
{
    AsyncStackTrace* lastUnlockedAncestor = nullptr;
    size_t depth = 0;

    auto* newStackTraceRoot = this;
    while (newStackTraceRoot) {
        depth += newStackTraceRoot->m_callStack->size();
        if (depth >= maxDepth)
            break;

        auto* parent = newStackTraceRoot->m_parent.get();
        if (!lastUnlockedAncestor && parent && parent->isLocked())
            lastUnlockedAncestor = newStackTraceRoot;

        newStackTraceRoot = parent;
    }

    if (!newStackTraceRoot || !newStackTraceRoot->m_parent)
        return;

    if (!lastUnlockedAncestor) {
        // No locked nodes belong to the trace. The subtree at the new root
        // is moved to a new tree, and marked as truncated if necessary.
        newStackTraceRoot->m_truncated = true;
        newStackTraceRoot->remove();
        return;
    }

    // The new root has a locked descendent. Since truncating a stack trace
    // cannot mutate locked nodes or their ancestors, a new tree is created by
    // cloning the locked portion of the trace (the path from the locked node
    // to the new root). The subtree rooted at the last unlocked ancestor is
    // then appended to the new tree.
    auto* previousNode = lastUnlockedAncestor;

    // The subtree being truncated must be removed from it's parent before
    // updating its parent pointer chain.
    RefPtr<AsyncStackTrace> sourceNode = lastUnlockedAncestor->m_parent;
    lastUnlockedAncestor->remove();

    while (sourceNode) {
        previousNode->m_parent = AsyncStackTrace::create(sourceNode->m_callStack.copyRef(), true, nullptr);
        previousNode->m_parent->m_childCount = 1;
        previousNode = previousNode->m_parent.get();

        if (sourceNode.get() == newStackTraceRoot)
            break;

        sourceNode = sourceNode->m_parent;
    }

    previousNode->m_truncated = true;
}

void AsyncStackTrace::remove()
{
    if (!m_parent)
        return;

    ASSERT(m_parent->m_childCount);
    m_parent->m_childCount--;
    m_parent = nullptr;
}

} // namespace Inspector
