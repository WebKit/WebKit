//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlobalMutex.cpp: Defines Global Mutex and utilities.

#include "libANGLE/GlobalMutex.h"

#include <atomic>

#include "common/debug.h"
#include "common/system_utils.h"

namespace egl
{
namespace priv
{
using GlobalMutexType = std::mutex;

#if !defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Default version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock() { mMutex.lock(); }
    ANGLE_INLINE void unlock() { mMutex.unlock(); }

  protected:
    GlobalMutexType mMutex;
};
#endif

#if defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Debug version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock()
    {
        const angle::ThreadId threadId = angle::GetCurrentThreadId();
        ASSERT(getOwnerThreadId() != threadId);
        mMutex.lock();
        ASSERT(getOwnerThreadId() == angle::InvalidThreadId());
        mOwnerThreadId.store(threadId, std::memory_order_relaxed);
    }

    ANGLE_INLINE void unlock()
    {
        ASSERT(getOwnerThreadId() == angle::GetCurrentThreadId());
        mOwnerThreadId.store(angle::InvalidThreadId(), std::memory_order_relaxed);
        mMutex.unlock();
    }

  private:
    ANGLE_INLINE angle::ThreadId getOwnerThreadId() const
    {
        return mOwnerThreadId.load(std::memory_order_relaxed);
    }

    GlobalMutexType mMutex;
    std::atomic<angle::ThreadId> mOwnerThreadId{angle::InvalidThreadId()};
};
#endif  // defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)

#if defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Recursive version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock()
    {
        const angle::ThreadId threadId = angle::GetCurrentThreadId();
        if (ANGLE_UNLIKELY(!mMutex.try_lock()))
        {
            if (ANGLE_UNLIKELY(getOwnerThreadId() == threadId))
            {
                ASSERT(mLockLevel > 0);
                ++mLockLevel;
                return;
            }
            mMutex.lock();
        }
        ASSERT(getOwnerThreadId() == angle::InvalidThreadId());
        ASSERT(mLockLevel == 0);
        mOwnerThreadId.store(threadId, std::memory_order_relaxed);
        mLockLevel = 1;
    }

    ANGLE_INLINE void unlock()
    {
        ASSERT(getOwnerThreadId() == angle::GetCurrentThreadId());
        ASSERT(mLockLevel > 0);
        if (ANGLE_LIKELY(--mLockLevel == 0))
        {
            mOwnerThreadId.store(angle::InvalidThreadId(), std::memory_order_relaxed);
            mMutex.unlock();
        }
    }

  private:
    ANGLE_INLINE angle::ThreadId getOwnerThreadId() const
    {
        return mOwnerThreadId.load(std::memory_order_relaxed);
    }

    GlobalMutexType mMutex;
    std::atomic<angle::ThreadId> mOwnerThreadId{angle::InvalidThreadId()};
    uint32_t mLockLevel = 0;
};
#endif  // defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
}  // namespace priv

namespace
{
#if defined(ANGLE_ENABLE_GLOBAL_MUTEX_LOAD_TIME_ALLOCATE)
#    if !ANGLE_HAS_ATTRIBUTE_CONSTRUCTOR || !ANGLE_HAS_ATTRIBUTE_DESTRUCTOR
#        error \
            "'angle_enable_global_mutex_load_time_allocate' " \
               "requires constructor/destructor compiler atributes."
#    endif
priv::GlobalMutex *g_MutexPtr = nullptr;

void ANGLE_CONSTRUCTOR AllocateGlobalMutex()
{
    ASSERT(g_MutexPtr == nullptr);
    g_MutexPtr = new priv::GlobalMutex();
}

void ANGLE_DESTRUCTOR DeallocateGlobalMutex()
{
    SafeDelete(g_MutexPtr);
}

#else
ANGLE_REQUIRE_CONSTANT_INIT std::atomic<priv::GlobalMutex *> g_Mutex(nullptr);
static_assert(std::is_trivially_destructible<decltype(g_Mutex)>::value,
              "global mutex is not trivially destructible");

priv::GlobalMutex *AllocateGlobalMutexImpl()
{
    priv::GlobalMutex *currentMutex = nullptr;
    std::unique_ptr<priv::GlobalMutex> newMutex(new priv::GlobalMutex());
    do
    {
        if (g_Mutex.compare_exchange_weak(currentMutex, newMutex.get()))
        {
            return newMutex.release();
        }
    } while (currentMutex == nullptr);
    return currentMutex;
}

priv::GlobalMutex *GetGlobalMutex()
{
    priv::GlobalMutex *mutex = g_Mutex.load();
    return mutex != nullptr ? mutex : AllocateGlobalMutexImpl();
}
#endif
}  // anonymous namespace

// ScopedGlobalMutexLock implementation.
#if defined(ANGLE_ENABLE_GLOBAL_MUTEX_LOAD_TIME_ALLOCATE)
ScopedGlobalMutexLock::ScopedGlobalMutexLock()
{
    g_MutexPtr->lock();
}

ScopedGlobalMutexLock::~ScopedGlobalMutexLock()
{
    g_MutexPtr->unlock();
}
#else
ScopedGlobalMutexLock::ScopedGlobalMutexLock() : mMutex(*GetGlobalMutex())
{
    mMutex.lock();
}

ScopedGlobalMutexLock::~ScopedGlobalMutexLock()
{
    mMutex.unlock();
}
#endif

// ScopedOptionalGlobalMutexLock implementation.
ScopedOptionalGlobalMutexLock::ScopedOptionalGlobalMutexLock(bool enabled)
{
    if (enabled)
    {
#if defined(ANGLE_ENABLE_GLOBAL_MUTEX_LOAD_TIME_ALLOCATE)
        mMutex = g_MutexPtr;
#else
        mMutex = GetGlobalMutex();
#endif
        mMutex->lock();
    }
    else
    {
        mMutex = nullptr;
    }
}

ScopedOptionalGlobalMutexLock::~ScopedOptionalGlobalMutexLock()
{
    if (mMutex != nullptr)
    {
        mMutex->unlock();
    }
}

// Global functions.
#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_STATIC)
#    if defined(ANGLE_ENABLE_GLOBAL_MUTEX_LOAD_TIME_ALLOCATE)
#        error "'angle_enable_global_mutex_load_time_allocate' is not supported in Windows DLL."
#    endif

void AllocateGlobalMutex()
{
    (void)AllocateGlobalMutexImpl();
}

void DeallocateGlobalMutex()
{
    priv::GlobalMutex *mutex = g_Mutex.exchange(nullptr);
    if (mutex != nullptr)
    {
        {
            // Wait for the mutex to become released by other threads before deleting.
            std::lock_guard<priv::GlobalMutex> lock(*mutex);
        }
        delete mutex;
    }
}
#endif

}  // namespace egl
