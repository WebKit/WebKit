//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SharedContextMutex.cpp: Classes for protecting Shared Context access and EGLImage siblings.

#include "libANGLE/SharedContextMutex.h"

#include "common/system_utils.h"
#include "libANGLE/Context.h"

namespace egl
{

namespace
{
[[maybe_unused]] bool CheckThreadIdCurrent(const std::atomic<angle::ThreadId> &threadId,
                                           angle::ThreadId *currentThreadIdOut)
{
    *currentThreadIdOut = angle::GetCurrentThreadId();
    return (threadId.load(std::memory_order_relaxed) == *currentThreadIdOut);
}

[[maybe_unused]] bool TryUpdateThreadId(std::atomic<angle::ThreadId> *threadId,
                                        angle::ThreadId oldThreadId,
                                        angle::ThreadId newThreadId)
{
    const bool ok = (threadId->load(std::memory_order_relaxed) == oldThreadId);
    if (ok)
    {
        threadId->store(newThreadId, std::memory_order_relaxed);
    }
    return ok;
}
}  // namespace

// ContextMutex
ContextMutex::~ContextMutex()
{
    ASSERT(mRefCount == 0);
}

void ContextMutex::onDestroy(UnlockBehaviour unlockBehaviour)
{
    if (unlockBehaviour == UnlockBehaviour::kUnlock)
    {
        unlock();
    }
}

void ContextMutex::release(UnlockBehaviour unlockBehaviour)
{
    ASSERT(isReferenced());
    if (--mRefCount == 0)
    {
        onDestroy(unlockBehaviour);
        delete this;
    }
    else if (unlockBehaviour == UnlockBehaviour::kUnlock)
    {
        unlock();
    }
}

// ScopedContextMutexAddRefLock
void ScopedContextMutexAddRefLock::lock(ContextMutex *mutex)
{
    ASSERT(mutex != nullptr);
    ASSERT(mMutex == nullptr);
    mMutex = mutex;
    // lock() before addRef() - using mMutex as synchronization
    mMutex->lock();
    // This lock alone must not cause mutex destruction
    ASSERT(mMutex->isReferenced());
    mMutex->addRef();
}

// ScopedContextMutexLock
bool ScopedContextMutexLock::IsContextMutexStateConsistent(gl::Context *context)
{
    ASSERT(context != nullptr);
    return context->isContextMutexStateConsistent();
}

// SingleContextMutex
bool SingleContextMutex::try_lock()
{
    UNREACHABLE();
    return false;
}

#if defined(ANGLE_ENABLE_CONTEXT_MUTEX_RECURSION)
void SingleContextMutex::lock()
{
    const int oldValue = mState.fetch_add(1, std::memory_order_relaxed);
    ASSERT(oldValue >= 0);
}

void SingleContextMutex::unlock()
{
    const int oldValue = mState.fetch_sub(1, std::memory_order_release);
    ASSERT(oldValue > 0);
}
#else
void SingleContextMutex::lock()
{
    ASSERT(!isLocked(std::memory_order_relaxed));
    mState.store(1, std::memory_order_relaxed);
}

void SingleContextMutex::unlock()
{
    ASSERT(isLocked(std::memory_order_relaxed));
    mState.store(0, std::memory_order_release);
}
#endif

// SharedContextMutex
template <class Mutex>
bool SharedContextMutex<Mutex>::try_lock()
{
    SharedContextMutex *const root = getRoot();
    return (root->doTryLock() != nullptr);
}

template <class Mutex>
void SharedContextMutex<Mutex>::lock()
{
    SharedContextMutex *const root = getRoot();
    (void)root->doLock();
}

template <class Mutex>
void SharedContextMutex<Mutex>::unlock()
{
    SharedContextMutex *const root = getRoot();
    // "root" is currently locked so "root->getRoot()" will return stable result.
    ASSERT(root == root->getRoot());
    root->doUnlock();
}

#if defined(ANGLE_ENABLE_CONTEXT_MUTEX_RECURSION)
template <class Mutex>
ANGLE_INLINE SharedContextMutex<Mutex> *SharedContextMutex<Mutex>::doTryLock()
{
    const angle::ThreadId threadId = angle::GetCurrentThreadId();
    if (ANGLE_UNLIKELY(!mMutex.try_lock()))
    {
        if (ANGLE_UNLIKELY(mOwnerThreadId.load(std::memory_order_relaxed) == threadId))
        {
            ASSERT(this == getRoot());
            ASSERT(mLockLevel > 0);
            ++mLockLevel;
            return this;
        }
        return nullptr;
    }
    ASSERT(mOwnerThreadId.load(std::memory_order_relaxed) == angle::InvalidThreadId());
    ASSERT(mLockLevel == 0);
    SharedContextMutex *const root = getRoot();
    if (ANGLE_UNLIKELY(this != root))
    {
        // Unlock, so only the "stable root" mutex remains locked
        mMutex.unlock();
        SharedContextMutex *const lockedRoot = root->doTryLock();
        ASSERT(lockedRoot == nullptr || lockedRoot == getRoot());
        return lockedRoot;
    }
    mOwnerThreadId.store(threadId, std::memory_order_relaxed);
    mLockLevel = 1;
    return this;
}

template <class Mutex>
ANGLE_INLINE SharedContextMutex<Mutex> *SharedContextMutex<Mutex>::doLock()
{
    const angle::ThreadId threadId = angle::GetCurrentThreadId();
    if (ANGLE_UNLIKELY(!mMutex.try_lock()))
    {
        if (ANGLE_UNLIKELY(mOwnerThreadId.load(std::memory_order_relaxed) == threadId))
        {
            ASSERT(this == getRoot());
            ASSERT(mLockLevel > 0);
            ++mLockLevel;
            return this;
        }
        mMutex.lock();
    }
    ASSERT(mOwnerThreadId.load(std::memory_order_relaxed) == angle::InvalidThreadId());
    ASSERT(mLockLevel == 0);
    SharedContextMutex *const root = getRoot();
    if (ANGLE_UNLIKELY(this != root))
    {
        // Unlock, so only the "stable root" mutex remains locked
        mMutex.unlock();
        SharedContextMutex *const lockedRoot = root->doLock();
        ASSERT(lockedRoot == getRoot());
        return lockedRoot;
    }
    mOwnerThreadId.store(threadId, std::memory_order_relaxed);
    mLockLevel = 1;
    return this;
}

template <class Mutex>
ANGLE_INLINE void SharedContextMutex<Mutex>::doUnlock()
{
    ASSERT(mOwnerThreadId.load(std::memory_order_relaxed) == angle::GetCurrentThreadId());
    ASSERT(mLockLevel > 0);
    if (ANGLE_LIKELY(--mLockLevel == 0))
    {
        mOwnerThreadId.store(angle::InvalidThreadId(), std::memory_order_relaxed);
        mMutex.unlock();
    }
}
#else
template <class Mutex>
ANGLE_INLINE SharedContextMutex<Mutex> *SharedContextMutex<Mutex>::doTryLock()
{
    angle::ThreadId currentThreadId;
    ASSERT(!CheckThreadIdCurrent(mOwnerThreadId, &currentThreadId));
    if (mMutex.try_lock())
    {
        SharedContextMutex *const root = getRoot();
        if (ANGLE_UNLIKELY(this != root))
        {
            // Unlock, so only the "stable root" mutex remains locked
            mMutex.unlock();
            SharedContextMutex *const lockedRoot = root->doTryLock();
            ASSERT(lockedRoot == nullptr || lockedRoot == getRoot());
            return lockedRoot;
        }
        ASSERT(TryUpdateThreadId(&mOwnerThreadId, angle::InvalidThreadId(), currentThreadId));
        return this;
    }
    return nullptr;
}

template <class Mutex>
ANGLE_INLINE SharedContextMutex<Mutex> *SharedContextMutex<Mutex>::doLock()
{
    angle::ThreadId currentThreadId;
    ASSERT(!CheckThreadIdCurrent(mOwnerThreadId, &currentThreadId));
    mMutex.lock();
    SharedContextMutex *const root = getRoot();
    if (ANGLE_UNLIKELY(this != root))
    {
        // Unlock, so only the "stable root" mutex remains locked
        mMutex.unlock();
        SharedContextMutex *const lockedRoot = root->doLock();
        ASSERT(lockedRoot == getRoot());
        return lockedRoot;
    }
    ASSERT(TryUpdateThreadId(&mOwnerThreadId, angle::InvalidThreadId(), currentThreadId));
    return this;
}

template <class Mutex>
ANGLE_INLINE void SharedContextMutex<Mutex>::doUnlock()
{
    ASSERT(
        TryUpdateThreadId(&mOwnerThreadId, angle::GetCurrentThreadId(), angle::InvalidThreadId()));
    mMutex.unlock();
}
#endif

template <class Mutex>
SharedContextMutex<Mutex>::SharedContextMutex()
    : mOwnerThreadId(angle::InvalidThreadId()), mLockLevel(0), mRoot(this), mRank(0)
{}

template <class Mutex>
SharedContextMutex<Mutex>::~SharedContextMutex()
{
    ASSERT(mLockLevel == 0);
    ASSERT(this == getRoot());
    ASSERT(mOldRoots.empty());
    ASSERT(mLeaves.empty());
}

template <class Mutex>
void SharedContextMutex<Mutex>::Merge(SharedContextMutex *lockedMutex,
                                      SharedContextMutex *otherMutex)
{
    ASSERT(lockedMutex != nullptr);
    ASSERT(otherMutex != nullptr);

    // Since lockedMutex is locked, its "root" pointer is stable.
    SharedContextMutex *lockedRoot      = lockedMutex->getRoot();
    SharedContextMutex *otherLockedRoot = nullptr;

    // Mutex merging will update the structure of both mutexes, therefore both mutexes must be
    // locked before continuing. First mutex is already locked, need to lock the other mutex.
    // Because other thread may perform merge with same mutexes reversed, we can't simply lock
    // otherMutex - this may cause a deadlock. Additionally, otherMutex may have same "root" (same
    // mutex or already merged), not only merging is unnecessary, but locking otherMutex will
    // guarantee a deadlock.

    for (;;)
    {
        // First, check that "root" of otherMutex is the same as "root" of lockedMutex.
        // lockedRoot is stable by definition and it is safe to compare with "unstable root".
        SharedContextMutex *otherRoot = otherMutex->getRoot();
        if (otherRoot == lockedRoot)
        {
            // Do nothing if two mutexes are the same/merged.
            return;
        }
        // Second, try to lock otherMutex "root" (can't use lock()/doLock(), see above comment).
        otherLockedRoot = otherRoot->doTryLock();
        if (otherLockedRoot != nullptr)
        {
            // otherMutex "root" can't become lockedMutex "root". For that to happen, lockedMutex
            // must be locked from some other thread first, which is impossible, since it is already
            // locked by this thread.
            ASSERT(otherLockedRoot != lockedRoot);
            // Lock is successful. Both mutexes are locked - can proceed with the merge...
            break;
        }
        // Lock was unsuccessful - unlock and retry...
        // May use "doUnlock()" because lockedRoot is a "stable root" mutex.
        // Note: lock will be preserved in case of the recursive lock.
        lockedRoot->doUnlock();
        // Sleep random amount to allow one of the thread acquire the lock next time...
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 91 + 10));
        // Because lockedMutex was unlocked, its "root" might have been changed. Below line will
        // reacquire the lock and update lockedRoot pointer.
        lockedRoot = lockedRoot->getRoot()->doLock();
    }

    // Mutexes that are not reference counted is not supported.
    ASSERT(lockedRoot->isReferenced());
    ASSERT(otherLockedRoot->isReferenced());

    // Decide the new "root". See mRank comment for more details...

    SharedContextMutex *oldRoot = otherLockedRoot;
    SharedContextMutex *newRoot = lockedRoot;

    if (oldRoot->mRank > newRoot->mRank)
    {
        std::swap(oldRoot, newRoot);
    }
    else if (oldRoot->mRank == newRoot->mRank)
    {
        ++newRoot->mRank;
    }

    // Update the structure
    for (SharedContextMutex *const leaf : oldRoot->mLeaves)
    {
        ASSERT(leaf->getRoot() == oldRoot);
        leaf->setNewRoot(newRoot);
    }
    oldRoot->mLeaves.clear();
    oldRoot->setNewRoot(newRoot);

    // Leave only the "merged" mutex locked. "oldRoot" already merged, need to use "doUnlock()"
    oldRoot->doUnlock();

    // Merge from recursive lock is unexpected. Handle such cases anyway to be safe.
    while (oldRoot->mLockLevel > 0)
    {
        newRoot->doLock();
        oldRoot->doUnlock();
    }
}

template <class Mutex>
void SharedContextMutex<Mutex>::setNewRoot(SharedContextMutex *newRoot)
{
    SharedContextMutex *const oldRoot = getRoot();

    ASSERT(newRoot != oldRoot);
    mRoot.store(newRoot, std::memory_order_relaxed);
    newRoot->addRef();

    newRoot->addLeaf(this);

    if (oldRoot != this)
    {
        mOldRoots.emplace_back(oldRoot);
    }
}

template <class Mutex>
void SharedContextMutex<Mutex>::addLeaf(SharedContextMutex *leaf)
{
    ASSERT(this == getRoot());
    ASSERT(leaf->getRoot() == this);
    ASSERT(leaf->mLeaves.empty());
    ASSERT(mLeaves.count(leaf) == 0);
    mLeaves.emplace(leaf);
}

template <class Mutex>
void SharedContextMutex<Mutex>::removeLeaf(SharedContextMutex *leaf)
{
    ASSERT(this == getRoot());
    ASSERT(leaf->getRoot() == this);
    ASSERT(leaf->mLeaves.empty());
    ASSERT(mLeaves.count(leaf) == 1);
    mLeaves.erase(leaf);
}

template <class Mutex>
void SharedContextMutex<Mutex>::onDestroy(UnlockBehaviour unlockBehaviour)
{
    ASSERT(mRefCount == 0);
    ASSERT(mLeaves.empty());

    SharedContextMutex *const root = getRoot();
    if (this == root)
    {
        ASSERT(mOldRoots.empty());
        if (unlockBehaviour == UnlockBehaviour::kUnlock)
        {
            doUnlock();
        }
    }
    else
    {
        for (SharedContextMutex *oldRoot : mOldRoots)
        {
            ASSERT(oldRoot->getRoot() == root);
            ASSERT(oldRoot->mLeaves.empty());
            oldRoot->release();
        }
        mOldRoots.clear();

        root->removeLeaf(this);

        root->release(unlockBehaviour);
        mRoot.store(this, std::memory_order_relaxed);
    }
}

template class SharedContextMutex<std::mutex>;

// SharedContextMutexManager
template <class Mutex>
ContextMutex *SharedContextMutexManager<Mutex>::create()
{
    return new SharedContextMutex<Mutex>();
}

template <class Mutex>
void SharedContextMutexManager<Mutex>::merge(ContextMutex *lockedMutex, ContextMutex *otherMutex)
{
    SharedContextMutex<Mutex>::Merge(static_cast<SharedContextMutex<Mutex> *>(lockedMutex),
                                     static_cast<SharedContextMutex<Mutex> *>(otherMutex));
}

template <class Mutex>
ContextMutex *SharedContextMutexManager<Mutex>::getRootMutex(ContextMutex *mutex)
{
    return static_cast<SharedContextMutex<Mutex> *>(mutex)->getRoot();
}

template class SharedContextMutexManager<std::mutex>;

}  // namespace egl
