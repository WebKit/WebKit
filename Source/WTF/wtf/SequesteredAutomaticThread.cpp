/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/SequesteredAutomaticThread.h>

#include <wtf/SequesteredImmortalHeap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WTF {

#if USE(PROTECTED_JIT_STACKS)

SequesteredStack::SequesteredStack(size_t stackSize, size_t guardSize)
    : m_handle(SequesteredImmortalHeap::instance().stackAllocator().allocate(stackSize, guardSize).handle, { })
{ }

void SequesteredStack::StackHandleFreer::operator()(StackHandle* handle) const noexcept {
    if (!handle)
        return;
    SequesteredImmortalHeap::instance().stackAllocator().deallocate(handle);
}

SequesteredAutomaticThread::SequesteredAutomaticThread(const AbstractLocker& locker, Box<Lock> lock, Ref<AutomaticThreadCondition>&& condition, Seconds timeout, size_t stackSize)
    : AutomaticThread(locker, lock, WTF::move(condition), ThreadType::Compiler, timeout)
    , m_stack(SequesteredStack(stackSize))
{
}

SequesteredAutomaticThread::~SequesteredAutomaticThread()
{
    ASSERT(!m_hasUnderlyingThread);
}

StackAllocationSpecification SequesteredAutomaticThread::stackSpecification()
{
    return StackAllocationSpecification::CustomStack(m_stack.span());
}

#endif // USE(PROTECTED_JIT_STACKS)

} // namespace WTF
