// Copyright (c) 2005, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat <opensource@google.com>

#ifndef TCMALLOC_INTERNAL_SPINLOCK_H__
#define TCMALLOC_INTERNAL_SPINLOCK_H__

#include "config.h"
#include <time.h>       /* For nanosleep() */
#include <sched.h>      /* For sched_yield() */
#if HAVE(STDINT_H)
#include <stdint.h>
#elif HAVE(INTTYPES_H)
#include <inttypes.h>
#else
#include <sys/types.h>
#endif
#include <stdlib.h>     /* for abort() */

#if (PLATFORM(X86) || PLATFORM(PPC)) && COMPILER(GCC)
static void TCMalloc_SlowLock(volatile unsigned int* lockword);

// The following is a struct so that it can be initialized at compile time
struct TCMalloc_SpinLock {
  volatile unsigned int private_lockword_;

  inline void Init() { private_lockword_ = 0; }
  inline void Finalize() { }
    
  inline void Lock() {
    int r;
#if PLATFORM(X86)
    __asm__ __volatile__
      ("xchgl %0, %1"
       : "=r"(r), "=m"(private_lockword_)
       : "0"(1), "m"(private_lockword_)
       : "memory");
#else
    volatile unsigned int *lockword_ptr = &private_lockword_;
    __asm__ __volatile__
        ("1: lwarx %0, 0, %1\n\t"
         "stwcx. %2, 0, %1\n\t"
         "bne- 1b\n\t"
         "isync"
         : "=&r" (r), "=r" (lockword_ptr)
         : "r" (1), "1" (lockword_ptr)
         : "memory");
#endif
    if (r) TCMalloc_SlowLock(&private_lockword_);
  }

  inline void Unlock() {
#if PLATFORM(X86)
    __asm__ __volatile__
      ("movl $0, %0"
       : "=m"(private_lockword_)
       : "m" (private_lockword_)
       : "memory");
#else
    __asm__ __volatile__
      ("isync\n\t"
       "eieio\n\t"
       "stw %1, %0"
       : "=o" (private_lockword_) 
       : "r" (0)
       : "memory");
#endif
  }
};

#define SPINLOCK_INITIALIZER { 0 }

static void TCMalloc_SlowLock(volatile unsigned int* lockword) {
  sched_yield();        // Yield immediately since fast path failed
  while (true) {
    int r;
#if PLATFORM(X86)
    __asm__ __volatile__
      ("xchgl %0, %1"
       : "=r"(r), "=m"(*lockword)
       : "0"(1), "m"(*lockword)
       : "memory");

#else
    int tmp = 1;
    __asm__ __volatile__
        ("1: lwarx %0, 0, %1\n\t"
         "stwcx. %2, 0, %1\n\t"
         "bne- 1b\n\t"
         "isync"
         : "=&r" (r), "=r" (lockword)
         : "r" (tmp), "1" (lockword)
         : "memory");
#endif
    if (!r) {
      return;
    }

    // This code was adapted from the ptmalloc2 implementation of
    // spinlocks which would sched_yield() upto 50 times before
    // sleeping once for a few milliseconds.  Mike Burrows suggested
    // just doing one sched_yield() outside the loop and always
    // sleeping after that.  This change helped a great deal on the
    // performance of spinlocks under high contention.  A test program
    // with 10 threads on a dual Xeon (four virtual processors) went
    // from taking 30 seconds to 16 seconds.

    // Sleep for a few milliseconds
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 2000001;
    nanosleep(&tm, NULL);
  }
}

#else

#include <pthread.h>

// Portable version
struct TCMalloc_SpinLock {
  pthread_mutex_t private_lock_;

  inline void Init() {
    if (pthread_mutex_init(&private_lock_, NULL) != 0) abort();
  }
  inline void Finalize() {
    if (pthread_mutex_destroy(&private_lock_) != 0) abort();
  }
  inline void Lock() {
    if (pthread_mutex_lock(&private_lock_) != 0) abort();
  }
  inline void Unlock() {
    if (pthread_mutex_unlock(&private_lock_) != 0) abort();
  }
};

#define SPINLOCK_INITIALIZER { PTHREAD_MUTEX_INITIALIZER }

#endif

// Corresponding locker object that arranges to acquire a spinlock for
// the duration of a C++ scope.
class TCMalloc_SpinLockHolder {
 private:
  TCMalloc_SpinLock* lock_;
 public:
  inline explicit TCMalloc_SpinLockHolder(TCMalloc_SpinLock* l)
    : lock_(l) { l->Lock(); }
  inline ~TCMalloc_SpinLockHolder() { lock_->Unlock(); }
};

// Short-hands for convenient use by tcmalloc.cc
typedef TCMalloc_SpinLock SpinLock;
typedef TCMalloc_SpinLockHolder SpinLockHolder;

#endif  // TCMALLOC_INTERNAL_SPINLOCK_H__
