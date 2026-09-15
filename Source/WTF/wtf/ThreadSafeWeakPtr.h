/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/SwiftBridging.h>
#include <wtf/WordLock.h>

namespace WTF {

template<typename T> class ThreadSafeWeakPtr;
template<typename T> class ThreadSafeWeakRef;
template<typename> class ThreadSafeWeakHashSet;
template<typename, DestructionThread> class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;

class ThreadSafeWeakPtrControlBlock {
    WTF_MAKE_NONCOPYABLE(ThreadSafeWeakPtrControlBlock);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ThreadSafeWeakPtrControlBlock);
public:
    using ObjectOffset = int16_t;

    ThreadSafeWeakPtrControlBlock* weakRef()
    {
        Locker locker { m_lock };
        ++m_weakReferenceCount;
        return this;
    }

    // Deleting ThreadSafeWeakPtrControlBlock does not delete a user object.
    SUPPRESS_NODELETE void NODELETE weakDeref()
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
        SUPPRESS_UNCOUNTED_LOCAL T* object;
        {
            Locker locker { m_lock };
            ASSERT_WITH_SECURITY_IMPLICATION(m_object);
            if (--m_strongReferenceCount) [[likely]]
                return;
            object = static_cast<T*>(std::exchange(m_object, nullptr));
            // We need to take a weak ref so `this` survives until the `delete object` below.
            // This comes up when destructors try to eagerly remove themselves from WeakHashSets.
            // e.g.
            // ~MyObject() { m_weakSet.remove(this); }
            // if m_weakSet has the last reference to the ControlBlock then we could end up doing
            // an amortized clean up, which removes the ControlBlock and destroys it. Then when we
            // check m_weakSet's backing table after the cleanup we UAF the ControlBlock.
            m_weakReferenceCount++;
        }

        SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto deleteObject = [this, object] {
            delete static_cast<const T*>(object);

            bool hasOtherWeakRefs;
            {
                // We retained ourselves above.
                Locker locker { m_lock };
                hasOtherWeakRefs = --m_weakReferenceCount;
                // release the lock here so we don't do it in Locker's destructor after we've already called delete.
            }

            if (!hasOtherWeakRefs)
                delete this;
        };
        switch (destructionThread) {
        case DestructionThread::Any:
            deleteObject();
            break;
        case DestructionThread::Main:
            ensureOnMainThread(WTF::move(deleteObject));
            break;
        case DestructionThread::MainRunLoop:
            ensureOnMainRunLoop(WTF::move(deleteObject));
            break;
        }
    }

    // N.B. These don't just make a strong reference to m_object because a ThreadSafeWeakPtr could be
    // referring to some interior pointer when there is multiple inheritance.
    // Consider:
    // struct Cat : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Cat>;
    // struct Dog { virtual ThreadSafeWeakPtrControlBlock& controlBlock() const = 0; };
    // struct CatDog : public Cat, public Dog {
    //     ThreadSafeWeakPtrControlBlock& controlBlock() const { return Cat::controlBlock(); }
    // };
    //
    // If we have a ThreadSafeWeakPtr<Dog> from a CatDog then we want to return the CatDog's Dog*
    // and not m_object's CatDog* pointer.
    template<typename U>
    RefPtr<U> makeStrongReferenceIfPossible(ObjectOffset objectOffset) const
    {
        auto* object = static_cast<uint8_t*>(refObjectIfAlive());
        if (!object)
            return nullptr;
        IGNORE_WARNINGS_BEGIN("unsafe-buffer-usage")
        return adoptRef(reinterpret_cast<U*>(object + objectOffset));
        IGNORE_WARNINGS_END
    }

    template<typename U>
    RefPtr<U> makeStrongReferenceIfPossible(const U* maybeInteriorPointer) const
    {
        if (!refObjectIfAlive())
            return nullptr;
        return adoptRef(const_cast<U*>(maybeInteriorPointer));
    }

    template<typename U>
    ObjectOffset objectOffset(const U* maybeInteriorPointer) const
    {
        Locker locker { m_lock };
        if (!m_object)
            return 0;
        ptrdiff_t offset = reinterpret_cast<const uint8_t*>(maybeInteriorPointer) - static_cast<const uint8_t*>(m_object);
        auto truncatedOffset = static_cast<ObjectOffset>(offset);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(truncatedOffset == offset);
        return truncatedOffset;
    }

    // These should really only be used for debugging and shouldn't be used to guard any checks in production,
    // unless you really know what you're doing. This is because they're prone to time of check time of use bugs.
    // Consider:
    // if (!objectHasStartedDeletion())
    //     strongRef();
    // Between objectHasStartedDeletion() and strongRef() another thread holding the sole remaining reference
    // to the underlying object could release it's reference and start deletion.
    bool objectHasStartedDeletion() const
    {
        Locker locker { m_lock };
        return !m_object;
    }
    uint32_t weakRefCount() const
    {
        Locker locker { m_lock };
        return m_weakReferenceCount;
    }

    uint32_t refCount() const
    {
        Locker locker { m_lock };
        return m_strongReferenceCount;
    }

    bool hasOneRef() const
    {
        Locker locker { m_lock };
        return m_strongReferenceCount == 1;
    }

private:
    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename T, DestructionThread thread>
    explicit ThreadSafeWeakPtrControlBlock(const ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<T, thread>* object)
        : m_object(const_cast<T*>(static_cast<const T*>(object)))
    { }

    void setStrongReferenceCountDuringInitialization(uint32_t count) WTF_IGNORES_THREAD_SAFETY_ANALYSIS { m_strongReferenceCount = count; }

    // Returns the object with its strong reference count incremented, or null if the object has started deletion.
    void* refObjectIfAlive() const
    {
        Locker locker { m_lock };
        if (!m_object)
            return nullptr;
        // Calling the RefPtr constructor would call strongRef() and deadlock.
        ++m_strongReferenceCount;
        return m_object;
    }

    mutable WordLock m_lock;
    mutable uint32_t m_strongReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    mutable uint32_t m_weakReferenceCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable void* m_object WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
};

struct ThreadSafeWeakPtrControlBlockRefDerefTraits {
    static ALWAYS_INLINE ThreadSafeWeakPtrControlBlock* refIfNotNull(ThreadSafeWeakPtrControlBlock* ptr)
    {
        if (ptr) [[likely]]
            return ptr->weakRef();
        return nullptr;
    }

    static ALWAYS_INLINE void derefIfNotNull(ThreadSafeWeakPtrControlBlock* ptr)
    {
        if (ptr) [[likely]]
            ptr->weakDeref();
    }
};
using ControlBlockRefPtr = RefPtr<ThreadSafeWeakPtrControlBlock, RawPtrTraits<ThreadSafeWeakPtrControlBlock>, ThreadSafeWeakPtrControlBlockRefDerefTraits>;

class ThreadSafeWeakPtrStorage {
    WTF_MAKE_NONCOPYABLE(ThreadSafeWeakPtrStorage);
public:
    using ObjectOffset = ThreadSafeWeakPtrControlBlock::ObjectOffset;

    ThreadSafeWeakPtrStorage() = default;

    ThreadSafeWeakPtrStorage(ThreadSafeWeakPtrControlBlock* controlBlock, ObjectOffset objectOffset)
        : m_controlBlock(controlBlock ? controlBlock->weakRef() : nullptr)
        , m_objectOffset(objectOffset)
    {
    }

    ~ThreadSafeWeakPtrStorage() { clear(); }

    template<typename T>
    RefPtr<T> makeStrongReferenceIfPossible() const
    {
        Locker locker { m_lock };
        if (!m_controlBlock)
            return nullptr;
        return m_controlBlock->makeStrongReferenceIfPossible<T>(m_objectOffset);
    }

    void set(ThreadSafeWeakPtrControlBlock* controlBlock, ObjectOffset objectOffset)
    {
        if (controlBlock)
            controlBlock->weakRef();
        adopt(controlBlock, objectOffset);
    }

    void clear() { adopt(nullptr, 0); }

    void copyFrom(const ThreadSafeWeakPtrStorage& other)
    {
        ThreadSafeWeakPtrControlBlock* controlBlock;
        ObjectOffset objectOffset;
        {
            Locker locker { other.m_lock };
            controlBlock = other.m_controlBlock;
            objectOffset = other.m_objectOffset;
            if (controlBlock)
                controlBlock->weakRef();
        }
        adopt(controlBlock, objectOffset);
    }

    void moveFrom(ThreadSafeWeakPtrStorage& other)
    {
        ThreadSafeWeakPtrControlBlock* controlBlock;
        ObjectOffset objectOffset;
        {
            Locker locker { other.m_lock };
            controlBlock = std::exchange(other.m_controlBlock, nullptr);
            objectOffset = std::exchange(other.m_objectOffset, 0);
        }
        adopt(controlBlock, objectOffset);
    }

private:
    void adopt(ThreadSafeWeakPtrControlBlock* controlBlock, ObjectOffset objectOffset)
    {
        ThreadSafeWeakPtrControlBlock* previous;
        {
            Locker locker { m_lock };
            previous = std::exchange(m_controlBlock, controlBlock);
            m_objectOffset = objectOffset;
        }
        if (previous)
            previous->weakDeref();
    }

    ThreadSafeWeakPtrControlBlock* m_controlBlock WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
    ObjectOffset m_objectOffset WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable Lock m_lock;
};

template<typename T, DestructionThread destructionThread = DestructionThread::Any>
class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr {
    WTF_MAKE_NONCOPYABLE(ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr);
public:
    static_assert(alignof(ThreadSafeWeakPtrControlBlock) >= 2);
    static constexpr uintptr_t strongOnlyFlag = 1;
    static constexpr uintptr_t destructionStartedFlag = 1ull << (sizeof(uintptr_t) * CHAR_BIT - 1);
    static constexpr uintptr_t refIncrement = 2;

    void ref() const
    {
        bool didRefStrongOnly = m_bits.transaction([&](uintptr_t& bits) {
            if (!isStrongOnly(bits))
                return false;
            // FIXME: Add support for ref()/deref() during destruction like we support for other RefCounted types.
            ASSERT(!(bits & destructionStartedFlag));
            bits += refIncrement;
            return true;
        }, std::memory_order_relaxed);
        if (didRefStrongOnly)
            return;

        std::bit_cast<ThreadSafeWeakPtrControlBlock*>(bits())->strongRef();
    }

    void deref() const
    {
        uintptr_t newStrongOnlyRefCount = 0;
        bool didDerefStrongOnly = m_bits.transaction([&](uintptr_t& bits) {
            if (!isStrongOnly(bits))
                return false;
            // FIXME: Add support for ref()/deref() during destruction like we support for other RefCounted types.
            ASSERT(!(bits & destructionStartedFlag));
            bits -= refIncrement;
            newStrongOnlyRefCount = bits;
            return true;
        }, std::memory_order_release);
        if (didDerefStrongOnly) {
            if (newStrongOnlyRefCount == strongOnlyFlag) {
                std::atomic_thread_fence(std::memory_order_acquire);
                ASSERT(m_bits.exchangeOr(destructionStartedFlag) == newStrongOnlyRefCount);
                SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto deleteObject = [this] {
                    delete static_cast<const T*>(this);
                };
                switch (destructionThread) {
                case DestructionThread::Any:
                    deleteObject();
                    break;
                case DestructionThread::Main:
                    ensureOnMainThread(WTF::move(deleteObject));
                    break;
                case DestructionThread::MainRunLoop:
                    ensureOnMainRunLoop(WTF::move(deleteObject));
                    break;
                }
            }
            return;
        }

        std::bit_cast<ThreadSafeWeakPtrControlBlock*>(bits())->template strongDeref<T, destructionThread>();
    }

    uint32_t refCount() const
    {
        uintptr_t bits = this->bits();
        if (isStrongOnly(bits)) {
            // FIXME: Add support for ref()/deref() during destruction like we support for other RefCounted types.
            ASSERT(!(bits & destructionStartedFlag));
            // Technically, this bit-and isn't needed but it's included for clarity since the compiler will elide it anyway.
            return (bits & ~strongOnlyFlag) / refIncrement;
        }

        return std::bit_cast<ThreadSafeWeakPtrControlBlock*>(bits)->refCount();
    }

    bool hasOneRef() const { return refCount() == 1; }

    // Ideally this would have been private but AbstractRefCounted subclasses need to be able to access this function
    // to provide its result to ThreadSafeWeakHashSet.
    uint32_t weakRefCount() const { return !isStrongOnly(bits()) ? controlBlock().weakRefCount() : 0; }

protected:
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr() = default;
    // Creating & destroying ThreadSafeWeakPtrControlBlock does not delete an user object.
    SUPPRESS_NODELETE ThreadSafeWeakPtrControlBlock& NODELETE controlBlock() const
    {
        // If we ever decided there was a lot of contention here we could have some lock bits in m_bits but
        // that seems unlikely since this is a one-way street. Once we add a controlBlock we don't go back
        // to strong only.
        uintptr_t bits = this->bits();
        if (!isStrongOnly(bits)) [[likely]]
            return *std::bit_cast<ThreadSafeWeakPtrControlBlock*>(bits);

        auto* controlBlock = new ThreadSafeWeakPtrControlBlock(this);

        bool didSetControlBlock = m_bits.transaction([&](uintptr_t& bits) {
            if (!isStrongOnly(bits))
                return false;

            // It doesn't really make sense to create a ThreadSafeWeakPtr during destruction since the controlBlock has to
            // view the object as dead. Otherwise a ThreadSafeWeakPtrFactory on an unrelated thread could vend out a partially
            // destroyed object.
            ASSERT(!(bits & destructionStartedFlag));
            // Technically, this bit-and isn't needed but it's included for clarity since the compiler will elide it anyway.
            controlBlock->setStrongReferenceCountDuringInitialization((bits & ~strongOnlyFlag) / refIncrement);
            bits = std::bit_cast<uintptr_t>(controlBlock);
            ASSERT(!isStrongOnly(bits));
            return true;
        }, std::memory_order_release); // We want memory_order_release here to make sure other threads see the right ref count / object.
        if (didSetControlBlock)
            return *controlBlock;

        delete controlBlock;
        return *std::bit_cast<ThreadSafeWeakPtrControlBlock*>(this->bits());
    }

private:
    static bool isStrongOnly(uintptr_t bits) { return bits & strongOnlyFlag; }

    // Use memory_order_acquire under TSan to pair with the memory_order_release
    // in controlBlock(). Without this, TSan reports a race between the
    // non-atomic initialization of the control block's members and subsequent
    // atomic operations on them (e.g., WordLock::lock()). ARM64 dependency
    // ordering and x86 total store ordering make this benign in practice, but
    // the C++ memory model requires acquire to formally synchronize with the
    // release store.
    ALWAYS_INLINE uintptr_t bits() const
    {
#if TSAN_ENABLED
        return m_bits.load(std::memory_order_acquire);
#else
        return m_bits.loadRelaxed();
#endif
    }

    template<typename> friend class ThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakRef;
    template<typename> friend class ThreadSafeWeakHashSet;

    mutable Atomic<uintptr_t> m_bits { refIncrement + strongOnlyFlag };
} SWIFT_SHARED_REFERENCE(.ref, .deref) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

template<typename T>
class ThreadSafeWeakPtr {
public:
    ThreadSafeWeakPtr() = default;

    ThreadSafeWeakPtr(std::nullptr_t) { }

    ThreadSafeWeakPtr(const ThreadSafeWeakPtr& other) { m_storage.copyFrom(other.m_storage); }

    ThreadSafeWeakPtr(ThreadSafeWeakPtr&& other) { m_storage.moveFrom(other.m_storage); }

    template<typename U>
        requires (!std::is_pointer_v<U>)
    ThreadSafeWeakPtr(const U& retainedReference)
        : ThreadSafeWeakPtr(controlBlockAndObjectOffset(retainedReference))
    { }

    template<typename U>
    ThreadSafeWeakPtr(const U* retainedPointer)
        : ThreadSafeWeakPtr(retainedPointer ? controlBlockAndObjectOffset(*retainedPointer) : ControlBlockAndObjectOffset { })
    { }

    template<typename U>
    ThreadSafeWeakPtr(const Ref<U>& strongReference)
        : ThreadSafeWeakPtr(strongReference.get())
    { }

    template<typename U>
    ThreadSafeWeakPtr(const RefPtr<U>& strongReference)
        : ThreadSafeWeakPtr(strongReference.get())
    { }

    ThreadSafeWeakPtr(ThreadSafeWeakPtrControlBlock& controlBlock, const T& objectOfCorrectType)
        : m_storage(&controlBlock, controlBlock.objectOffset(&objectOfCorrectType))
    { }

    ThreadSafeWeakPtr& operator=(ThreadSafeWeakPtr&& other)
    {
        m_storage.moveFrom(other.m_storage);
        return *this;
    }

    ThreadSafeWeakPtr& operator=(const ThreadSafeWeakPtr& other)
    {
        m_storage.copyFrom(other.m_storage);
        return *this;
    }

    template<typename U>
        requires (!std::is_pointer_v<U>)
    ThreadSafeWeakPtr& operator=(const U& retainedReference)
    {
        auto [controlBlock, objectOffset] = controlBlockAndObjectOffset(retainedReference);
        m_storage.set(controlBlock, objectOffset);
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const U* retainedPointer)
    {
        if (!retainedPointer) {
            m_storage.clear();
            return *this;
        }
        auto [controlBlock, objectOffset] = controlBlockAndObjectOffset(*retainedPointer);
        m_storage.set(controlBlock, objectOffset);
        return *this;
    }

    ThreadSafeWeakPtr& operator=(std::nullptr_t)
    {
        m_storage.clear();
        return *this;
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const Ref<U>& strongReference)
    {
        return *this = strongReference.get();
    }

    template<typename U>
    ThreadSafeWeakPtr& operator=(const RefPtr<U>& strongReference)
    {
        return *this = strongReference.get();
    }

    RefPtr<T> get() const { return m_storage.makeStrongReferenceIfPossible<T>(); }

private:
    struct ControlBlockAndObjectOffset {
        ThreadSafeWeakPtrControlBlock* controlBlock { nullptr };
        ThreadSafeWeakPtrControlBlock::ObjectOffset objectOffset { 0 };
    };

    explicit ThreadSafeWeakPtr(ControlBlockAndObjectOffset pair)
        : m_storage(pair.controlBlock, pair.objectOffset)
    { }

    template<typename U>
    requires (std::is_convertible_v<U*, T*>)
    static ControlBlockAndObjectOffset controlBlockAndObjectOffset(const U& classOrChildClass)
    {
        auto& controlBlock = classOrChildClass.controlBlock();
        return { &controlBlock, controlBlock.objectOffset(static_cast<const T*>(&classOrChildClass)) };
    }

    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;

    ThreadSafeWeakPtrStorage m_storage;
} SWIFT_ESCAPABLE;

template<class T> ThreadSafeWeakPtr(const T&) -> ThreadSafeWeakPtr<T>;
template<class T> ThreadSafeWeakPtr(const T*) -> ThreadSafeWeakPtr<T>;

template<typename T>
class ThreadSafeWeakRef {
public:
    ThreadSafeWeakRef(const ThreadSafeWeakRef& other) { m_storage.copyFrom(other.m_storage); }

    ThreadSafeWeakRef(ThreadSafeWeakRef&& other) { m_storage.moveFrom(other.m_storage); }

    template<typename U>
        requires (!std::is_pointer_v<U>)
    ThreadSafeWeakRef(const U& retainedReference)
        : ThreadSafeWeakRef(controlBlockAndObjectOffset(retainedReference))
    { }

    template<typename U>
    ThreadSafeWeakRef(const Ref<U>& strongReference)
        : ThreadSafeWeakRef(strongReference.get())
    { }

    ThreadSafeWeakRef(ThreadSafeWeakPtrControlBlock& controlBlock, const T& objectOfCorrectType)
        : m_storage(&controlBlock, controlBlock.objectOffset(&objectOfCorrectType))
    { }

    ThreadSafeWeakRef& operator=(ThreadSafeWeakRef&& other)
    {
        m_storage.moveFrom(other.m_storage);
        return *this;
    }

    ThreadSafeWeakRef& operator=(const ThreadSafeWeakRef& other)
    {
        m_storage.copyFrom(other.m_storage);
        return *this;
    }

    template<typename U>
        requires (!std::is_pointer_v<U>)
    ThreadSafeWeakRef& operator=(const U& retainedReference)
    {
        auto [controlBlock, objectOffset] = controlBlockAndObjectOffset(retainedReference);
        m_storage.set(controlBlock, objectOffset);
        return *this;
    }

    template<typename U>
    ThreadSafeWeakRef& operator=(const Ref<U>& strongReference)
    {
        return *this = strongReference.get();
    }

    Ref<T> get() const
    {
        RefPtr result = m_storage.makeStrongReferenceIfPossible<T>();
        RELEASE_ASSERT(result);
        return result.releaseNonNull();
    }

private:
    struct ControlBlockAndObjectOffset {
        ThreadSafeWeakPtrControlBlock* controlBlock { nullptr };
        ThreadSafeWeakPtrControlBlock::ObjectOffset objectOffset { 0 };
    };

    explicit ThreadSafeWeakRef(ControlBlockAndObjectOffset pair)
        : m_storage(pair.controlBlock, pair.objectOffset)
    { }

    template<typename U>
    requires (std::is_convertible_v<U*, T*>)
    static ControlBlockAndObjectOffset controlBlockAndObjectOffset(const U& classOrChildClass)
    {
        auto& controlBlock = classOrChildClass.controlBlock();
        return { &controlBlock, controlBlock.objectOffset(static_cast<const T*>(&classOrChildClass)) };
    }

    template<typename, DestructionThread> friend class ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
    template<typename> friend class ThreadSafeWeakHashSet;

    ThreadSafeWeakPtrStorage m_storage;
} SWIFT_ESCAPABLE;

template<class T> ThreadSafeWeakRef(const T&) -> ThreadSafeWeakRef<T>;

template<typename T>
ALWAYS_INLINE RefPtr<T> protect(const ThreadSafeWeakPtr<T>& weakPtr)
{
    return weakPtr.get();
}

template<typename T>
ALWAYS_INLINE Ref<T> protect(const ThreadSafeWeakRef<T>& weakRef)
{
    return weakRef.get();
}

}

using WTF::ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr;
using WTF::ThreadSafeWeakPtr;
using WTF::ThreadSafeWeakRef;
using WTF::ThreadSafeWeakPtrControlBlock;
