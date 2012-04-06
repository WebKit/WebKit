/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "HandleSet.h"

#include "HeapRootVisitor.h"
#include "JSObject.h"

namespace JSC {

HandleSet::HandleSet(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_nextToFinalize(0)
{
    grow();
}

void HandleSet::grow()
{
    Node* block = m_blockStack.grow();
    for (int i = m_blockStack.blockLength - 1; i >= 0; --i) {
        Node* node = &block[i];
        new (NotNull, node) Node(this);
        m_freeList.push(node);
    }
}

void HandleSet::visitStrongHandles(HeapRootVisitor& heapRootVisitor)
{
    Node* end = m_strongList.end();
    for (Node* node = m_strongList.begin(); node != end; node = node->next()) {
#if ENABLE(GC_VALIDATION)
        if (!isLiveNode(node))
            CRASH();
#endif
        heapRootVisitor.visit(node->slot());
    }
}

void HandleSet::writeBarrier(HandleSlot slot, const JSValue& value)
{
    // Forbid assignment to handles during the finalization phase, since it would violate many GC invariants.
    // File a bug with stack trace if you hit this.
    if (m_nextToFinalize)
        CRASH();

    if (!value == !*slot && slot->isCell() == value.isCell())
        return;

    Node* node = toNode(slot);
#if ENABLE(GC_VALIDATION)
    if (!isLiveNode(node))
        CRASH();
#endif
    SentinelLinkedList<Node>::remove(node);
    if (!value || !value.isCell()) {
        m_immediateList.push(node);
        return;
    }

    m_strongList.push(node);
#if ENABLE(GC_VALIDATION)
    if (!isLiveNode(node))
        CRASH();
#endif
}

unsigned HandleSet::protectedGlobalObjectCount()
{
    unsigned count = 0;
    Node* end = m_strongList.end();
    for (Node* node = m_strongList.begin(); node != end; node = node->next()) {
        JSValue value = *node->slot();
        if (value.isObject() && asObject(value.asCell())->isGlobalObject())
            count++;
    }
    return count;
}

#if ENABLE(GC_VALIDATION) || !ASSERT_DISABLED
bool HandleSet::isLiveNode(Node* node)
{
    if (node->prev()->next() != node)
        return false;
    if (node->next()->prev() != node)
        return false;
        
    return true;
}
#endif

} // namespace JSC
