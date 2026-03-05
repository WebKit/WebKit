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

#pragma once

#include <optional>
#include <utility>

#include <wtf/AutomaticThread.h>
#include <wtf/Box.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/SequesteredImmortalHeap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if USE(PROTECTED_JIT_STACKS)

namespace WTF {

struct StackHandle;

class SequesteredStack {
    WTF_MAKE_NONCOPYABLE(SequesteredStack);
public:
    static constexpr size_t defaultSize = 1 * MB;
    static constexpr size_t defaultGuardSize = 16 * KB;

    SequesteredStack(size_t stackSize = defaultSize, size_t guardSize = defaultGuardSize);

    WTF_EXPORT_PRIVATE ~SequesteredStack() = default;
    SequesteredStack(SequesteredStack&&) = default;
    SequesteredStack& operator=(SequesteredStack&&) = default;

    explicit operator bool() const { return !!m_handle; }

    std::span<std::byte> span() const
    {
        RELEASE_ASSERT(m_handle);
        return m_handle->stack;
    }

private:
    struct StackHandleFreer {
        void operator()(StackHandle* handle) const noexcept;
    };

    // Important: default-constructed unique_ptr still needs a deleter object.
    // This is fine because your deleter is stateless.
    std::unique_ptr<StackHandle, StackHandleFreer> m_handle { nullptr };
};

// An AutomaticThread variant that runs on a sequestered stack:
// i.e. a userspace-managed stack allocated in the constructor of
// SequesteredAutomaticThread and destroyed in the destructor.
// Used for JIT compiler threads where stack isolation is desired.
class WTF_EXPORT_PRIVATE SequesteredAutomaticThread : public AutomaticThread {
    WTF_FORBID_HEAP_ALLOCATION(SequesteredAutomaticThread);
public:
    SequesteredAutomaticThread(const AbstractLocker&, Box<Lock>,
        Ref<AutomaticThreadCondition>&&,
        Seconds timeout = 10_s,
        size_t stackSize = 1 * MB);

    ~SequesteredAutomaticThread() override;

protected:
    StackAllocationSpecification stackSpecification() override;

private:
    SequesteredStack m_stack;
};

} // namespace WTF

using WTF::SequesteredAutomaticThread;

#endif // USE(PROTECTED_JIT_STACKS)
