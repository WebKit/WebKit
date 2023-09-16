//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SharedContextMutex.h: Classes for protecting Shared Context access and EGLImage siblings.

#ifndef LIBANGLE_SHARED_CONTEXT_MUTEX_H_
#define LIBANGLE_SHARED_CONTEXT_MUTEX_H_

#include <atomic>

#include "common/debug.h"

namespace gl
{
class Context;
}

namespace egl
{
#if defined(ANGLE_ENABLE_SHARED_CONTEXT_MUTEX)
constexpr bool kIsSharedContextMutexEnabled = true;
#else
constexpr bool kIsSharedContextMutexEnabled = false;
#endif

class ContextMutex : angle::NonCopyable
{
  public:
    // Below group of methods are not thread safe and requires external synchronization.
    // Note: since release*() may call onDestroy(), additional synchronization requirements may be
    // enforced by concrete implementations.
    ANGLE_INLINE void addRef() { ++mRefCount; }
    ANGLE_INLINE void release() { release(UnlockBehaviour::kNotUnlock); }
    ANGLE_INLINE void releaseAndUnlock() { release(UnlockBehaviour::kUnlock); }
    ANGLE_INLINE bool isReferenced() const { return mRefCount > 0; }

    virtual bool try_lock() = 0;
    virtual void lock()     = 0;
    virtual void unlock()   = 0;

  protected:
    virtual ~ContextMutex();

    enum class UnlockBehaviour
    {
        kNotUnlock,
        kUnlock
    };

    void release(UnlockBehaviour unlockBehaviour);

    virtual void onDestroy(UnlockBehaviour unlockBehaviour);

  protected:
    size_t mRefCount = 0;
};

struct ContextMutexMayBeNullTag final
{};
constexpr ContextMutexMayBeNullTag kContextMutexMayBeNull;

// Prevents destruction while locked, uses mMutex to protect addRef()/releaseAndUnlock() calls.
class [[nodiscard]] ScopedContextMutexAddRefLock final : angle::NonCopyable
{
  public:
    ANGLE_INLINE ScopedContextMutexAddRefLock() = default;
    ANGLE_INLINE explicit ScopedContextMutexAddRefLock(ContextMutex *mutex) { lock(mutex); }
    ANGLE_INLINE ScopedContextMutexAddRefLock(ContextMutex *mutex, ContextMutexMayBeNullTag)
    {
        if (mutex != nullptr)
        {
            lock(mutex);
        }
    }
    ANGLE_INLINE ~ScopedContextMutexAddRefLock()
    {
        if (mMutex != nullptr)
        {
            mMutex->releaseAndUnlock();
        }
    }

  private:
    void lock(ContextMutex *mutex);

  private:
    ContextMutex *mMutex = nullptr;
};

class [[nodiscard]] ScopedContextMutexLock final
{
  public:
    ANGLE_INLINE ScopedContextMutexLock() = default;
    ANGLE_INLINE ScopedContextMutexLock(ContextMutex *mutex, gl::Context *context)
        : mMutex(mutex), mContext(context)
    {
        ASSERT(mutex != nullptr);
        ASSERT(context != nullptr);
        mutex->lock();
    }
    ANGLE_INLINE ScopedContextMutexLock(ContextMutex *mutex,
                                        gl::Context *context,
                                        ContextMutexMayBeNullTag)
        : mMutex(mutex), mContext(context)
    {
        if (ANGLE_LIKELY(mutex != nullptr))
        {
            ASSERT(context != nullptr);
            mutex->lock();
        }
    }
    ANGLE_INLINE ~ScopedContextMutexLock()
    {
        if (ANGLE_LIKELY(mMutex != nullptr))
        {
            ASSERT(IsContextMutexStateConsistent(mContext));
            mMutex->unlock();
        }
    }

    ANGLE_INLINE ScopedContextMutexLock(ScopedContextMutexLock &&other)
        : mMutex(other.mMutex), mContext(other.mContext)
    {
        other.mMutex = nullptr;
    }
    ANGLE_INLINE ScopedContextMutexLock &operator=(ScopedContextMutexLock &&other)
    {
        std::swap(mMutex, other.mMutex);
        mContext = other.mContext;
        return *this;
    }

  private:
    static bool IsContextMutexStateConsistent(gl::Context *context);

  private:
    ContextMutex *mMutex  = nullptr;
    gl::Context *mContext = nullptr;
};

// Mutex may be locked only by a single thread. Other threads may only check the status.
class SingleContextMutex final : public ContextMutex
{
  public:
    ANGLE_INLINE bool isLocked(std::memory_order order) const { return mState.load(order) > 0; }

    // ContextMutex
    bool try_lock() override;
    void lock() override;
    void unlock() override;

  private:
    std::atomic_int mState{0};
};

// Note: onDestroy() method must be protected by "this" mutex, since onDestroy() is called from
// release*() methods, these methods must also be protected by "this" mutex.
template <class Mutex>
class SharedContextMutex final : public ContextMutex
{
  public:
    SharedContextMutex();
    ~SharedContextMutex() override;

    // Merges mutexes so they work as one.
    // At the end, only single "root" mutex will be locked.
    // Does nothing if two mutexes are the same or already merged (have same "root" mutex).
    // Note: synchronization requirements for addRef()/release*() calls for merged mutexes are
    // the same as for the single unmerged mutex. For example: can't call at the same time
    // mutexA.addRef() and mutexB.release() if they are merged.
    static void Merge(SharedContextMutex *lockedMutex, SharedContextMutex *otherMutex);

    // Returns current "root" mutex.
    // Warning! Result is only stable if mutex is locked, while may change any time if unlocked.
    // May be used to compare against already locked "root" mutex.
    ANGLE_INLINE SharedContextMutex *getRoot() { return mRoot.load(std::memory_order_relaxed); }

    // ContextMutex
    bool try_lock() override;
    void lock() override;
    void unlock() override;

  private:
    SharedContextMutex *doTryLock();
    SharedContextMutex *doLock();
    void doUnlock();

    // All methods below must be protected by "this" mutex ("stable root" in "this" instance).

    void setNewRoot(SharedContextMutex *newRoot);
    void addLeaf(SharedContextMutex *leaf);
    void removeLeaf(SharedContextMutex *leaf);

    // ContextMutex
    void onDestroy(UnlockBehaviour unlockBehaviour) override;

  private:
    Mutex mMutex;
    // Used when ASSERT() and/or recursion are/is enabled.
    std::atomic<angle::ThreadId> mOwnerThreadId;
    // Used only when recursion is enabled.
    uint32_t mLockLevel;

    // mRoot and mLeaves tree structure details:
    // - used to implement primary functionality of this class;
    // - initially, all mutexes are "root"s;
    // - "root" mutex has "mRoot == this";
    // - "root" mutex stores unreferenced pointers to all its leaves (used in merging);
    // - "leaf" mutex holds reference (addRef) to the current "root" mutex in the mRoot;
    // - "leaf" mutex has empty mLeaves;
    // - "leaf" mutex can't become a "root" mutex;
    // - before locking the mMutex, "this" is an "unstable root" or a "leaf";
    // - the implementation always locks mRoot's mMutex ("unstable root");
    // - if after locking the mMutex "mRoot != this", then "this" is/become a "leaf";
    // - otherwise, "this" is a locked "stable root" - lock is successful.
    std::atomic<SharedContextMutex *> mRoot;
    std::set<SharedContextMutex *> mLeaves;

    // mOldRoots is used to solve a particular problem (below example does not use mRank):
    // - have "leaf" mutex_2 with a reference to mutex_1 "root";
    // - the mutex_1 has no other references (only in the mutex_2);
    // - have other mutex_3 "root";
    // - mutex_1 pointer is cached on the stack during locking of mutex_2 (thread A);
    // - merge mutex_3 and mutex_2 (thread B):
    //     * now "leaf" mutex_2 stores reference to mutex_3 "root";
    //     * old "root" mutex_1 becomes a "leaf" of mutex_3;
    //     * old "root" mutex_1 has no references and gets destroyed.
    // - invalid pointer to destroyed mutex_1 stored on the stack and in the mLeaves of mutex_3;
    // - to fix this problem, references to old "root"s are kept in the mOldRoots vector.
    std::vector<SharedContextMutex *> mOldRoots;

    // mRank is used to fix a problem of indefinite grows of mOldRoots:
    // - merge mutex_2 and mutex_1 -> mutex_2 is "root" of mutex_1 (mOldRoots == 0);
    // - destroy mutex_2;
    // - merge mutex_3 and mutex_1 -> mutex_3 is "root" of mutex_1 (mOldRoots == 1);
    // - destroy mutex_3;
    // - merge mutex_4 and mutex_1 -> mutex_4 is "root" of mutex_1 (mOldRoots == 2);
    // - destroy mutex_4;
    // - continuing this pattern can lead to indefinite grows of mOldRoots, while pick number of
    //   mutexes is only 2.
    // Fix details using mRank:
    // - initially "mRank == 0" and only relevant for "root" mutexes;
    // - merging mutexes with equal mRank of their "root"s, will use first (lockedMutex) "root"
    //   mutex as a new "root" and increase its mRank by 1;
    // - otherwise, "root" mutex with a highest rank will be used without changing the mRank;
    // - this way, "stronger" (with a higher mRank) "root" mutex will "protect" its "leaves" from
    //   "mRoot" replacement and therefore - mOldRoots grows.
    // Lets look at the problematic pattern with the mRank:
    // - merge mutex_2 and mutex_1 -> mutex_2 is "root" (mRank == 1) of mutex_1 (mOldRoots == 0);
    // - destroy mutex_2;
    // - merge mutex_3 and mutex_1 -> mutex_2 is "root" (mRank == 1) of mutex_3 (mOldRoots == 0);
    // - destroy mutex_3;
    // - merge mutex_4 and mutex_1 -> mutex_2 is "root" (mRank == 1) of mutex_4 (mOldRoots == 0);
    // - destroy mutex_4;
    // - no mOldRoots grows at all;
    // - minumum number of mutexes to reach mOldRoots size of N => 2^(N+1).
    uint32_t mRank;
};

class ContextMutexManager
{
  public:
    virtual ~ContextMutexManager() = default;

    virtual ContextMutex *create()                                          = 0;
    virtual void merge(ContextMutex *lockedMutex, ContextMutex *otherMutex) = 0;
    virtual ContextMutex *getRootMutex(ContextMutex *mutex)                 = 0;
};

template <class Mutex>
class SharedContextMutexManager final : public ContextMutexManager
{
  public:
    ContextMutex *create() override;
    void merge(ContextMutex *lockedMutex, ContextMutex *otherMutex) override;
    ContextMutex *getRootMutex(ContextMutex *mutex) override;
};

}  // namespace egl

#endif  // LIBANGLE_SHARED_CONTEXT_MUTEX_H_
