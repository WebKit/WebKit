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
template<typename> class ThreadSafeWeakHashSet;
template<typename, DestructionThread> class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;

class ThreadSafeWeakPtrControlBlock {
    WTF_MAKE_NONCOPYABLE(ThreadSafeWeakPtrControlBlock);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSafeWeakPtrControlBlock* weakRef()
    {
        Locker locker { m_lock };
        if (m_object) {
            ++m_weakReferenceCount;
            return this;
        }
        return nullptr;
    }

    void weakDeref()
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

    size_t weakReferenceCount() const
    {
        Locker locker { m_lock };
        return m_weakReferenceCount;
    }

    bool hasOneRef() const
    {
        Locker locker { m_lock };
        return m_strongReferenceCount == 1;
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
    RefPtr<T> makeStrongReferenceIfPossible(const T* objectOfCorrectType) const
    {
        Locker locker { m_lock };
        if (m_object) {
            // Calling the RefPtr constructor would call strongRef() and deadlock.
            ++m_strongReferenceCount;
            return adoptRef(const_cast<T*>(objectOfCorrectType));
        }
        return nullptr;
    }

    bool objectHasStartedDeletion() const
    {
        Locker locker { m_lock };
        return !m_object;
    }

private:
    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename T>
    explicit ThreadSafeWeakPtrControlBlock(T* object)
        : m_object(object) { }

    mutable Lock m_lock;
    mutable size_t m_strongReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    mutable size_t m_weakReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable void* m_object WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
};

struct ThreadSafeWeakPtrControlBlockRefDerefTraits {
    static ALWAYS_INLINE ThreadSafeWeakPtrControlBlock* refIfNotNull(ThreadSafeWeakPtrControlBlock* ptr)
    {
        if (LIKELY(ptr))
            return ptr->weakRef();
        return nullptr;
    }

    static ALWAYS_INLINE void derefIfNotNull(ThreadSafeWeakPtrControlBlock* ptr)
    {
        if (LIKELY(ptr))
            ptr->weakDeref();
    }
};
using ControlBlockRefPtr = RefPtr<ThreadSafeWeakPtrControlBlock, RawPtrTraits<ThreadSafeWeakPtrControlBlock>, ThreadSafeWeakPtrControlBlockRefDerefTraits>;

template<typename T, DestructionThread destructionThread = DestructionThread::Any>
class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr {
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void ref() const { m_controlBlock.strongRef(); }
    void deref() const { m_controlBlock.template strongDeref<T, destructionThread>(); }
    bool hasOneRef() const { return m_controlBlock.hasOneRef(); }
protected:
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr() = default;
    ThreadSafeWeakPtrControlBlock& controlBlock() const { return m_controlBlock; }
private:
    template<typename> friend class ThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;
    ThreadSafeWeakPtrControlBlock& m_controlBlock { *new ThreadSafeWeakPtrControlBlock(static_cast<T*>(this)) };
};

template<typename T>
class ThreadSafeWeakPtr {
public:
    ThreadSafeWeakPtr() = default;

    ThreadSafeWeakPtr(std::nullptr_t) { }

    ThreadSafeWeakPtr(const ThreadSafeWeakPtr<T>& other)
        : m_controlBlock(other.m_controlBlock)
        , m_objectOfCorrectType(other.m_objectOfCorrectType) { }

    template<typename U, std::enable_if_t<!std::is_pointer_v<U>>* = nullptr>
    ThreadSafeWeakPtr(const U& retainedReference)
        : m_controlBlock(controlBlock(retainedReference))
        , m_objectOfCorrectType(static_cast<const T*>(&retainedReference)) { }

    template<typename U>
    ThreadSafeWeakPtr(const U* retainedPointer)
        : m_controlBlock(retainedPointer ? controlBlock(*retainedPointer) : nullptr)
        , m_objectOfCorrectType(static_cast<const T*>(retainedPointer)) { }

    template<typename U>
    ThreadSafeWeakPtr(const Ref<U>& strongReference)
        : m_controlBlock(controlBlock(strongReference))
        , m_objectOfCorrectType(static_cast<const T*>(strongReference.ptr())) { }

    template<typename U>
    ThreadSafeWeakPtr(const RefPtr<U>& strongReference)
        : m_controlBlock(strongReference ? controlBlock(*strongReference) : nullptr)
        , m_objectOfCorrectType(static_cast<const T*>(strongReference.get())) { }

    ThreadSafeWeakPtr(ThreadSafeWeakPtr&& other)
        : m_controlBlock(std::exchange(other.m_controlBlock, nullptr))
        , m_objectOfCorrectType(std::exchange(other.m_objectOfCorrectType, nullptr)) { }

    ThreadSafeWeakPtr(ThreadSafeWeakPtrControlBlock& controlBlock, const T& objectOfCorrectType)
        : m_controlBlock(&controlBlock)
        , m_objectOfCorrectType(&objectOfCorrectType) { }

    ThreadSafeWeakPtr& operator=(ThreadSafeWeakPtr&& other)
    {
        m_controlBlock = std::exchange(other.m_controlBlock, nullptr);
        m_objectOfCorrectType = std::exchange(other.m_objectOfCorrectType, nullptr);
        return *this;
    }

    template<typename U, std::enable_if_t<!std::is_pointer_v<U>>* = nullptr>
    ThreadSafeWeakPtr& operator=(const U& retainedReference)
    {
        m_controlBlock = controlBlock(retainedReference);
        m_objectOfCorrectType = static_cast<const T*>(static_cast<const U*>(&retainedReference));
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const U* retainedPointer)
    {
        m_controlBlock = retainedPointer ? controlBlock(*retainedPointer) : nullptr;
        m_objectOfCorrectType = static_cast<const T*>(retainedPointer);
        return *this;
    }

    ThreadSafeWeakPtr& operator=(std::nullptr_t)
    {
        m_controlBlock = nullptr;
        m_objectOfCorrectType = nullptr;
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const Ref<U>& strongReference)
    {
        m_controlBlock = controlBlock(strongReference);
        m_objectOfCorrectType = static_cast<const T*>(strongReference.ptr());
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const RefPtr<U>& strongReference)
    {
        m_controlBlock = strongReference ? controlBlock(*strongReference) : nullptr;
        m_objectOfCorrectType = static_cast<const T*>(strongReference.get());
        return *this;
    }

    RefPtr<T> get() const { return m_controlBlock ? m_controlBlock->template makeStrongReferenceIfPossible<T>(m_objectOfCorrectType) : nullptr; }

private:
    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    ThreadSafeWeakPtrControlBlock* controlBlock(const U& classOrChildClass)
    {
        return &classOrChildClass.controlBlock();
    }

    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;

    // FIXME: Either remove ThreadSafeWeakPtrControlBlock::m_object as redundant information,
    // or use CompactRefPtrTuple to reduce sizeof(ThreadSafeWeakPtr) by storing just an offset
    // from ThreadSafeWeakPtrControlBlock::m_object and don't support structs larger than 65535.
    ControlBlockRefPtr m_controlBlock;
    const T* m_objectOfCorrectType { nullptr };
};

template<class T> ThreadSafeWeakPtr(const T&) -> ThreadSafeWeakPtr<T>;
template<class T> ThreadSafeWeakPtr(const T*) -> ThreadSafeWeakPtr<T>;

}

using WTF::ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
using WTF::ThreadSafeWeakPtr;
using WTF::ThreadSafeWeakPtrControlBlock;
