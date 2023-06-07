/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/CompactRefPtrTuple.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>

namespace WTF {

template<typename> class ThreadSafeWeakPtr;
template<typename> class ThreadSafeWeakHashSet;
template<typename, DestructionThread> class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;

class ThreadSafeWeakPtrControlBlock {
    WTF_MAKE_NONCOPYABLE(ThreadSafeWeakPtrControlBlock);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void ref() const
    {
        Locker locker { m_lock };
        ++m_weakReferenceCount;
    }

    void deref() const
    {
        bool shouldDeleteControlBlock { false };
        {
            Locker locker { m_lock };
            ASSERT_WITH_SECURITY_IMPLICATION(m_weakReferenceCount);
            if (!--m_weakReferenceCount && !m_strongReferenceCount)
                shouldDeleteControlBlock = true;
        }
        if (shouldDeleteControlBlock)
            delete this;
    }

    void strongRef() const
    {
        Locker locker { m_lock };
        ASSERT_WITH_SECURITY_IMPLICATION(m_object);
        ++m_strongReferenceCount;
    }

    template<typename T, DestructionThread destructionThread>
    void strongDeref() const
    {
        bool shouldDeleteControlBlock { false };
        T* object;

        {
            Locker locker { m_lock };
            ASSERT_WITH_SECURITY_IMPLICATION(m_object);
            if (LIKELY(--m_strongReferenceCount))
                return;
            object = static_cast<T*>(std::exchange(m_object, nullptr));
            if (!m_weakReferenceCount)
                shouldDeleteControlBlock = true;
        }

        auto deleteObject = [this, object, shouldDeleteControlBlock] {
            delete static_cast<const T*>(object);
            if (shouldDeleteControlBlock)
                delete this;
        };
        switch (destructionThread) {
        case DestructionThread::Any:
            deleteObject();
            break;
        case DestructionThread::Main:
            ensureOnMainThread(WTFMove(deleteObject));
            break;
        case DestructionThread::MainRunLoop:
            ensureOnMainRunLoop(WTFMove(deleteObject));
            break;
        }
    }

    template<typename T>
    RefPtr<T> makeStrongReferenceIfPossible(uint16_t objectOffset) const
    {
        Locker locker { m_lock };
        if (m_object) {
            // Calling the RefPtr constructor would call strongRef() and deadlock.
            ++m_strongReferenceCount;
            return adoptRef(reinterpret_cast<T*>(reinterpret_cast<size_t>(m_object) + objectOffset));
        }
        return nullptr;
    }

    bool objectHasBeenDeleted() const
    {
        Locker locker { m_lock };
        return !m_object;
    }

    template<typename T>
    uint16_t objectOffset(T* pointerMaybeToSubclass)
    {
        Locker locker { m_lock };
        if (!m_object)
            return 0;
        auto offset = reinterpret_cast<size_t>(pointerMaybeToSubclass) - reinterpret_cast<size_t>(m_object);
        ASSERT(offset <= std::numeric_limits<uint16_t>::max());
        return static_cast<uint16_t>(offset);
    }

private:
    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename T>
    explicit ThreadSafeWeakPtrControlBlock(T& object)
        : m_object(&object) { }

    mutable Lock m_lock;
    mutable size_t m_strongReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    mutable size_t m_weakReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable void* m_object WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
};

template<typename T, DestructionThread destructionThread = DestructionThread::Any>
class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr {
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void ref() const { m_controlBlock.strongRef(); }
    void deref() const { m_controlBlock.template strongDeref<T, destructionThread>(); }
protected:
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr() = default;
    ThreadSafeWeakPtrControlBlock& controlBlock() const { return m_controlBlock; }
private:
    template<typename> friend class ThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;
    ThreadSafeWeakPtrControlBlock& m_controlBlock { *new ThreadSafeWeakPtrControlBlock(static_cast<T&>(*this)) };
};

template<typename T>
class ThreadSafeWeakPtr {
public:
    ThreadSafeWeakPtr() = default;

    ThreadSafeWeakPtr(std::nullptr_t) { }

    ThreadSafeWeakPtr(const ThreadSafeWeakPtr<T>& other)
        : m_controlBlockAndObjectOffset(other.m_controlBlockAndObjectOffset) { }

    template<typename U, std::enable_if_t<!std::is_pointer_v<U>>* = nullptr>
    ThreadSafeWeakPtr(const U& retainedReference)
        : m_controlBlockAndObjectOffset(controlBlockAndObjectOffset(retainedReference))
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_controlBlockAndObjectOffset.pointer()->objectHasBeenDeleted());
    }

    template<typename U>
    ThreadSafeWeakPtr(const U* retainedPointer)
        : m_controlBlockAndObjectOffset(retainedPointer ? controlBlockAndObjectOffset(*retainedPointer) : CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> { })
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!retainedPointer || !m_controlBlockAndObjectOffset.pointer()->objectHasBeenDeleted());
    }

    template<typename U>
    ThreadSafeWeakPtr(const Ref<U>& strongReference)
        : m_controlBlockAndObjectOffset(controlBlockAndObjectOffset(strongReference)) { }

    template<typename U>
    ThreadSafeWeakPtr(const RefPtr<U>& strongReference)
        : m_controlBlockAndObjectOffset(strongReference ? controlBlockAndObjectOffset(*strongReference) : CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> { }) { }

    ThreadSafeWeakPtr(ThreadSafeWeakPtr&& other)
        : m_controlBlockAndObjectOffset(std::exchange(other.m_controlBlockAndObjectOffset, { })) { }

    ThreadSafeWeakPtr& operator=(ThreadSafeWeakPtr&& other)
    {
        m_controlBlockAndObjectOffset = std::exchange(other.m_controlBlockAndObjectOffset, { });
        return *this;
    }

    template<typename U, std::enable_if_t<!std::is_pointer_v<U>>* = nullptr>
    ThreadSafeWeakPtr& operator=(const U& retainedReference)
    {
        m_controlBlockAndObjectOffset = controlBlockAndObjectOffset(retainedReference);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_controlBlockAndObjectOffset.pointer()->objectHasBeenDeleted());
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const U* retainedPointer)
    {
        m_controlBlockAndObjectOffset = retainedPointer ? controlBlockAndObjectOffset(*retainedPointer) : CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> { };
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!retainedPointer || !m_controlBlockAndObjectOffset.pointer()->objectHasBeenDeleted());
        return *this;
    }

    ThreadSafeWeakPtr& operator=(std::nullptr_t)
    {
        m_controlBlockAndObjectOffset = { };
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const Ref<U>& strongReference)
    {
        m_controlBlockAndObjectOffset = controlBlockAndObjectOffset(strongReference);
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const RefPtr<U>& strongReference)
    {
        m_controlBlockAndObjectOffset = strongReference ? controlBlockAndObjectOffset(*strongReference) : CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> { };
        return *this;
    }

    RefPtr<T> get() const { return m_controlBlockAndObjectOffset.pointer() ? m_controlBlockAndObjectOffset.pointer()->template makeStrongReferenceIfPossible<T>(m_controlBlockAndObjectOffset.type()) : nullptr; }

private:
    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> controlBlockAndObjectOffset(const U& classOrChildClass)
    {
        CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> result;
        auto& controlBlock = classOrChildClass.controlBlock();
        result.setPointer(&controlBlock);
        result.setType(controlBlock.objectOffset(&classOrChildClass));
        return result;
    }

    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;
    explicit ThreadSafeWeakPtr(CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t>&& controlBlockAndObjectOffset)
        : m_controlBlockAndObjectOffset(WTFMove(controlBlockAndObjectOffset)) { }

    CompactRefPtrTuple<ThreadSafeWeakPtrControlBlock, uint16_t> m_controlBlockAndObjectOffset;
};

template<class T> ThreadSafeWeakPtr(const T&) -> ThreadSafeWeakPtr<T>;
template<class T> ThreadSafeWeakPtr(const T*) -> ThreadSafeWeakPtr<T>;

}

using WTF::ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
using WTF::ThreadSafeWeakPtr;
using WTF::ThreadSafeWeakPtrControlBlock;
