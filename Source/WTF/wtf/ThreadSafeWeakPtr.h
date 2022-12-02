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

#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>

namespace WTF {

template<typename> class ThreadSafeWeakPtr;
template<typename, DestructionThread> class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;

template<typename T>
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

    template<DestructionThread destructionThread>
    void strongDeref() const
    {
        bool shouldDeleteControlBlock { false };
        T* object;

        {
            Locker locker { m_lock };
            ASSERT_WITH_SECURITY_IMPLICATION(m_object);
            if (LIKELY(--m_strongReferenceCount))
                return;
            object = std::exchange(m_object, nullptr);
            if (!m_weakReferenceCount)
                shouldDeleteControlBlock = true;
        }

        auto deleteObject = [object] {
            delete static_cast<const T*>(object);
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

        if (shouldDeleteControlBlock)
            delete this;
    }

    RefPtr<T> makeStrongReferenceIfPossible() const
    {
        Locker locker { m_lock };
        if (m_object) {
            // Calling the RefPtr constructor would call strongRef() and deadlock.
            ++m_strongReferenceCount;
            return adoptRef(m_object);
        }
        return nullptr;
    }

    bool objectHasBeenDeleted() const
    {
        Locker locker { m_lock };
        return !m_object;
    }

private:
    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    explicit ThreadSafeWeakPtrControlBlock(T& object)
        : m_object(&object) { }

    mutable Lock m_lock;
    mutable size_t m_strongReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    mutable size_t m_weakReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable T* m_object WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
};

template<typename T, DestructionThread destructionThread = DestructionThread::Any>
class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr {
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void ref() const { m_controlBlock.strongRef(); }
    void deref() const { m_controlBlock.template strongDeref<destructionThread>(); }
protected:
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr() = default;
private:
    template<typename> friend class ThreadSafeWeakPtr;
    ThreadSafeWeakPtrControlBlock<T>& m_controlBlock { *new ThreadSafeWeakPtrControlBlock<T>(static_cast<T&>(*this)) };
};

template<typename T>
class ThreadSafeWeakPtr {
public:
    ThreadSafeWeakPtr() = default;

    ThreadSafeWeakPtr(std::nullptr_t) { }

    ThreadSafeWeakPtr(const ThreadSafeWeakPtr<T>& other)
        : m_controlBlock(other.m_controlBlock) { }

    template<typename U>
    ThreadSafeWeakPtr(const U& retainedReference)
        : m_controlBlock(controlBlock(retainedReference))
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_controlBlock->objectHasBeenDeleted());
    }

    template<typename U>
    ThreadSafeWeakPtr(const U* retainedPointer)
        : m_controlBlock(retainedPointer ? controlBlock(*retainedPointer) : nullptr)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!retainedPointer || !m_controlBlock->objectHasBeenDeleted());
    }

    template<typename U>
    ThreadSafeWeakPtr(const Ref<U>& strongReference)
        : m_controlBlock(controlBlock(strongReference)) { }

    template<typename U>
    ThreadSafeWeakPtr(const RefPtr<U>& strongReference)
        : m_controlBlock(strongReference ? controlBlock(*strongReference) : nullptr) { }

    ThreadSafeWeakPtr(ThreadSafeWeakPtr&& other)
        : m_controlBlock(std::exchange(other.m_controlBlock, nullptr)) { }

    ThreadSafeWeakPtr& operator=(ThreadSafeWeakPtr&& other)
    {
        m_controlBlock = std::exchange(other.m_controlBlock, nullptr);
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const U& retainedReference)
    {
        m_controlBlock = controlBlock(retainedReference);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_controlBlock->objectHasBeenDeleted());
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const U* retainedPointer)
    {
        m_controlBlock = retainedPointer ? controlBlock(*retainedPointer) : nullptr;
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!retainedPointer || !m_controlBlock->objectHasBeenDeleted());
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const Ref<U>& strongReference)
    {
        m_controlBlock = controlBlock(strongReference);
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const RefPtr<U>& strongReference)
    {
        m_controlBlock = strongReference ? controlBlock(*strongReference) : nullptr;
        return *this;
    }

    RefPtr<T> get() const { return m_controlBlock->makeStrongReferenceIfPossible(); }

private:
    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    ThreadSafeWeakPtrControlBlock<T>* controlBlock(const U& classOrChildClass)
    {
        return &reinterpret_cast<ThreadSafeWeakPtrControlBlock<T>&>(classOrChildClass.m_controlBlock);
    }

    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    explicit ThreadSafeWeakPtr(ThreadSafeWeakPtrControlBlock<T>& controlBlock)
        : m_controlBlock(&controlBlock) { }

    RefPtr<ThreadSafeWeakPtrControlBlock<T>> m_controlBlock;
};

template<class T> ThreadSafeWeakPtr(const T&) -> ThreadSafeWeakPtr<T>;
template<class T> ThreadSafeWeakPtr(const T*) -> ThreadSafeWeakPtr<T>;

}

using WTF::ThreadSafeWeakPtr;
using WTF::ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
