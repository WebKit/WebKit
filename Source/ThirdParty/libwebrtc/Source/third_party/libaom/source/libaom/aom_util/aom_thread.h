/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
//
// Multi-threaded worker
//
// Original source:
//  https://chromium.googlesource.com/webm/libwebp

#ifndef AOM_AOM_UTIL_AOM_THREAD_H_
#define AOM_AOM_UTIL_AOM_THREAD_H_

#include "config/aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_THREADS 64

#if CONFIG_MULTITHREAD

#if defined(_WIN32) && !HAVE_PTHREAD_H
// Prevent leaking max/min macros.
#undef NOMINMAX
#define NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <errno.h>    // NOLINT
#include <process.h>  // NOLINT
#include <windows.h>  // NOLINT
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;

#if _WIN32_WINNT < 0x0600
#error _WIN32_WINNT must target Windows Vista / Server 2008 or newer.
#endif
typedef CONDITION_VARIABLE pthread_cond_t;

#ifndef WINAPI_FAMILY_PARTITION
#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_FAMILY_PARTITION(x) x
#endif

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define USE_CREATE_THREAD
#endif

//------------------------------------------------------------------------------
// simplistic pthread emulation layer

// _beginthreadex requires __stdcall
#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

static INLINE int pthread_create(pthread_t *const thread, const void *attr,
                                 unsigned int(__stdcall *start)(void *),
                                 void *arg) {
  (void)attr;
#ifdef USE_CREATE_THREAD
  *thread = CreateThread(NULL,          /* lpThreadAttributes */
                         0,             /* dwStackSize */
                         start, arg, 0, /* dwStackSize */
                         NULL);         /* lpThreadId */
#else
  *thread = (pthread_t)_beginthreadex(NULL,          /* void *security */
                                      0,             /* unsigned stack_size */
                                      start, arg, 0, /* unsigned initflag */
                                      NULL);         /* unsigned *thrdaddr */
#endif
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static INLINE int pthread_join(pthread_t thread, void **value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObjectEx(thread, INFINITE, FALSE /*bAlertable*/) !=
              WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

// Mutex
static INLINE int pthread_mutex_init(pthread_mutex_t *const mutex,
                                     void *mutexattr) {
  (void)mutexattr;
  InitializeCriticalSectionEx(mutex, 0 /*dwSpinCount*/, 0 /*Flags*/);
  return 0;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *const mutex) {
  return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *const mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *const mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *const mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

// Condition
static INLINE int pthread_cond_destroy(pthread_cond_t *const condition) {
  (void)condition;
  return 0;
}

static INLINE int pthread_cond_init(pthread_cond_t *const condition,
                                    void *cond_attr) {
  (void)cond_attr;
  InitializeConditionVariable(condition);
  return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *const condition) {
  WakeConditionVariable(condition);
  return 0;
}

static INLINE int pthread_cond_broadcast(pthread_cond_t *const condition) {
  WakeAllConditionVariable(condition);
  return 0;
}

static INLINE int pthread_cond_wait(pthread_cond_t *const condition,
                                    pthread_mutex_t *const mutex) {
  int ok;
  ok = SleepConditionVariableCS(condition, mutex, INFINITE);
  return !ok;
}
#else                 // _WIN32
#include <pthread.h>  // NOLINT
#define THREADFN void *
#define THREAD_RETURN(val) val
#endif

#endif  // CONFIG_MULTITHREAD

// State of the worker thread object
typedef enum {
  NOT_OK = 0,  // object is unusable
  OK,          // ready to work
  WORK         // busy finishing the current task
} AVxWorkerStatus;

// Function to be called by the worker thread. Takes two opaque pointers as
// arguments (data1 and data2). Should return true on success and return false
// in case of error.
typedef int (*AVxWorkerHook)(void *, void *);

// Platform-dependent implementation details for the worker.
typedef struct AVxWorkerImpl AVxWorkerImpl;

// Synchronization object used to launch job in the worker thread
typedef struct {
  AVxWorkerImpl *impl_;
  AVxWorkerStatus status_;
  // Thread name for the debugger. If not NULL, must point to a string that
  // outlives the worker thread. For portability, use a name <= 15 characters
  // long (not including the terminating NUL character).
  const char *thread_name;
  AVxWorkerHook hook;  // hook to call
  void *data1;         // first argument passed to 'hook'
  void *data2;         // second argument passed to 'hook'
  int had_error;       // true if a call to 'hook' returned false
} AVxWorker;

// The interface for all thread-worker related functions. All these functions
// must be implemented.
typedef struct {
  // Must be called first, before any other method.
  void (*init)(AVxWorker *const worker);
  // Must be called to initialize the object and spawn the thread. Re-entrant.
  // Will potentially launch the thread. Returns false in case of error.
  int (*reset)(AVxWorker *const worker);
  // Makes sure the previous work is finished. Returns true if worker->had_error
  // was not set and no error condition was triggered by the working thread.
  int (*sync)(AVxWorker *const worker);
  // Triggers the thread to call hook() with data1 and data2 arguments. These
  // hook/data1/data2 values can be changed at any time before calling this
  // function, but not be changed afterward until the next call to Sync().
  void (*launch)(AVxWorker *const worker);
  // This function is similar to launch() except that it calls the
  // hook directly instead of using a thread. Convenient to bypass the thread
  // mechanism while still using the AVxWorker structs. sync() must
  // still be called afterward (for error reporting).
  void (*execute)(AVxWorker *const worker);
  // Kill the thread and terminate the object. To use the object again, one
  // must call reset() again.
  void (*end)(AVxWorker *const worker);
} AVxWorkerInterface;

// Install a new set of threading functions, overriding the defaults. This
// should be done before any workers are started, i.e., before any encoding or
// decoding takes place. The contents of the interface struct are copied, it
// is safe to free the corresponding memory after this call. This function is
// not thread-safe. Return false in case of invalid pointer or methods.
int aom_set_worker_interface(const AVxWorkerInterface *const winterface);

// Retrieve the currently set thread worker interface.
const AVxWorkerInterface *aom_get_worker_interface(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_UTIL_AOM_THREAD_H_
