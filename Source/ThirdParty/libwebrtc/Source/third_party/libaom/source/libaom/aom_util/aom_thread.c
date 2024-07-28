/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
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

// Enable GNU extensions in glibc so that we can call pthread_setname_np().
// This must be before any #include statements.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>  // for memset()

#include "config/aom_config.h"

#include "aom_mem/aom_mem.h"
#include "aom_ports/sanitizer.h"
#include "aom_util/aom_pthread.h"
#include "aom_util/aom_thread.h"

#if CONFIG_MULTITHREAD

struct AVxWorkerImpl {
  pthread_mutex_t mutex_;
  pthread_cond_t condition_;
  pthread_t thread_;
};

//------------------------------------------------------------------------------

static void execute(AVxWorker *const worker);  // Forward declaration.

static THREADFN thread_loop(void *ptr) {
  AVxWorker *const worker = (AVxWorker *)ptr;
#ifdef __APPLE__
  if (worker->thread_name != NULL) {
    // Apple's version of pthread_setname_np takes one argument and operates on
    // the current thread only. The maximum size of the thread_name buffer was
    // noted in the Chromium source code and was confirmed by experiments. If
    // thread_name is too long, pthread_setname_np returns -1 with errno
    // ENAMETOOLONG (63).
    char thread_name[64];
    strncpy(thread_name, worker->thread_name, sizeof(thread_name) - 1);
    thread_name[sizeof(thread_name) - 1] = '\0';
    pthread_setname_np(thread_name);
  }
#elif (defined(__GLIBC__) && !defined(__GNU__)) || defined(__BIONIC__)
  if (worker->thread_name != NULL) {
    // Linux and Android require names (with nul) fit in 16 chars, otherwise
    // pthread_setname_np() returns ERANGE (34).
    char thread_name[16];
    strncpy(thread_name, worker->thread_name, sizeof(thread_name) - 1);
    thread_name[sizeof(thread_name) - 1] = '\0';
    pthread_setname_np(pthread_self(), thread_name);
  }
#endif
  pthread_mutex_lock(&worker->impl_->mutex_);
  for (;;) {
    while (worker->status_ == AVX_WORKER_STATUS_OK) {  // wait in idling mode
      pthread_cond_wait(&worker->impl_->condition_, &worker->impl_->mutex_);
    }
    if (worker->status_ == AVX_WORKER_STATUS_WORKING) {
      // When worker->status_ is AVX_WORKER_STATUS_WORKING, the main thread
      // doesn't change worker->status_ and will wait until the worker changes
      // worker->status_ to AVX_WORKER_STATUS_OK. See change_state(). So the
      // worker can safely call execute() without holding worker->impl_->mutex_.
      // When the worker reacquires worker->impl_->mutex_, worker->status_ must
      // still be AVX_WORKER_STATUS_WORKING.
      pthread_mutex_unlock(&worker->impl_->mutex_);
      execute(worker);
      pthread_mutex_lock(&worker->impl_->mutex_);
      assert(worker->status_ == AVX_WORKER_STATUS_WORKING);
      worker->status_ = AVX_WORKER_STATUS_OK;
      // signal to the main thread that we're done (for sync())
      pthread_cond_signal(&worker->impl_->condition_);
    } else {
      assert(worker->status_ == AVX_WORKER_STATUS_NOT_OK);  // finish the worker
      break;
    }
  }
  pthread_mutex_unlock(&worker->impl_->mutex_);
  return THREAD_EXIT_SUCCESS;  // Thread is finished
}

// main thread state control
static void change_state(AVxWorker *const worker, AVxWorkerStatus new_status) {
  // No-op when attempting to change state on a thread that didn't come up.
  // Checking status_ without acquiring the lock first would result in a data
  // race.
  if (worker->impl_ == NULL) return;

  pthread_mutex_lock(&worker->impl_->mutex_);
  if (worker->status_ >= AVX_WORKER_STATUS_OK) {
    // wait for the worker to finish
    while (worker->status_ != AVX_WORKER_STATUS_OK) {
      pthread_cond_wait(&worker->impl_->condition_, &worker->impl_->mutex_);
    }
    // assign new status and release the working thread if needed
    if (new_status != AVX_WORKER_STATUS_OK) {
      worker->status_ = new_status;
      pthread_cond_signal(&worker->impl_->condition_);
    }
  }
  pthread_mutex_unlock(&worker->impl_->mutex_);
}

#endif  // CONFIG_MULTITHREAD

//------------------------------------------------------------------------------

static void init(AVxWorker *const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status_ = AVX_WORKER_STATUS_NOT_OK;
}

static int sync(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  change_state(worker, AVX_WORKER_STATUS_OK);
#endif
  assert(worker->status_ <= AVX_WORKER_STATUS_OK);
  return !worker->had_error;
}

static int reset(AVxWorker *const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status_ < AVX_WORKER_STATUS_OK) {
#if CONFIG_MULTITHREAD
    worker->impl_ = (AVxWorkerImpl *)aom_calloc(1, sizeof(*worker->impl_));
    if (worker->impl_ == NULL) {
      return 0;
    }
    if (pthread_mutex_init(&worker->impl_->mutex_, NULL)) {
      goto Error;
    }
    if (pthread_cond_init(&worker->impl_->condition_, NULL)) {
      pthread_mutex_destroy(&worker->impl_->mutex_);
      goto Error;
    }
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) goto Error2;
      // Debug ASan builds require at least ~1MiB of stack; prevents
      // failures on macOS arm64 where the default is 512KiB.
      // See: https://crbug.com/aomedia/3379
#if defined(AOM_ADDRESS_SANITIZER) && defined(__APPLE__) && AOM_ARCH_ARM && \
    !defined(NDEBUG)
    const size_t kMinStackSize = 1024 * 1024;
#else
    const size_t kMinStackSize = 256 * 1024;
#endif
    size_t stacksize;
    if (!pthread_attr_getstacksize(&attr, &stacksize)) {
      if (stacksize < kMinStackSize &&
          pthread_attr_setstacksize(&attr, kMinStackSize)) {
        pthread_attr_destroy(&attr);
        goto Error2;
      }
    }
    pthread_mutex_lock(&worker->impl_->mutex_);
    ok = !pthread_create(&worker->impl_->thread_, &attr, thread_loop, worker);
    if (ok) worker->status_ = AVX_WORKER_STATUS_OK;
    pthread_mutex_unlock(&worker->impl_->mutex_);
    pthread_attr_destroy(&attr);
    if (!ok) {
    Error2:
      pthread_mutex_destroy(&worker->impl_->mutex_);
      pthread_cond_destroy(&worker->impl_->condition_);
    Error:
      aom_free(worker->impl_);
      worker->impl_ = NULL;
      return 0;
    }
#else
    worker->status_ = AVX_WORKER_STATUS_OK;
#endif
  } else if (worker->status_ > AVX_WORKER_STATUS_OK) {
    ok = sync(worker);
  }
  assert(!ok || (worker->status_ == AVX_WORKER_STATUS_OK));
  return ok;
}

static void execute(AVxWorker *const worker) {
  if (worker->hook != NULL) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

static void launch(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  change_state(worker, AVX_WORKER_STATUS_WORKING);
#else
  execute(worker);
#endif
}

static void end(AVxWorker *const worker) {
#if CONFIG_MULTITHREAD
  if (worker->impl_ != NULL) {
    change_state(worker, AVX_WORKER_STATUS_NOT_OK);
    pthread_join(worker->impl_->thread_, NULL);
    pthread_mutex_destroy(&worker->impl_->mutex_);
    pthread_cond_destroy(&worker->impl_->condition_);
    aom_free(worker->impl_);
    worker->impl_ = NULL;
  }
#else
  worker->status_ = AVX_WORKER_STATUS_NOT_OK;
  assert(worker->impl_ == NULL);
#endif
  assert(worker->status_ == AVX_WORKER_STATUS_NOT_OK);
}

//------------------------------------------------------------------------------

static AVxWorkerInterface g_worker_interface = { init,   reset,   sync,
                                                 launch, execute, end };

int aom_set_worker_interface(const AVxWorkerInterface *const winterface) {
  if (winterface == NULL || winterface->init == NULL ||
      winterface->reset == NULL || winterface->sync == NULL ||
      winterface->launch == NULL || winterface->execute == NULL ||
      winterface->end == NULL) {
    return 0;
  }
  g_worker_interface = *winterface;
  return 1;
}

const AVxWorkerInterface *aom_get_worker_interface(void) {
  return &g_worker_interface;
}

//------------------------------------------------------------------------------
