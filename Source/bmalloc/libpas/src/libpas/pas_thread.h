/*
 * Copyright (c) 2025 Ian Grunert <ian.grunert@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PAS_THREAD_H
#define PAS_THREAD_H

#include "pas_config.h"
#include "pas_platform.h"

#if !PAS_OS(WINDOWS)
#include <pthread.h>
#else

/* Implement the subset of pthread that libpas requires to run on Windows */

/* pas_utils.h includes this header before it defines the PAS_ macros, so the __PAS_ spellings from
   the prefix are the only ones available here. */
#include "pas_utils_prefix.h"

#include <process.h>
#include <time.h>
#include <windows.h>

/* Threads */

#define pthread_t uintptr_t

typedef INIT_ONCE pthread_once_t;
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

struct pthread_attr_t_internal { };
typedef struct pthread_attr_t_internal * pthread_attr_t;

struct pas_thread_t_internal { };
typedef struct pas_thread_t_internal pas_thread_t;

/* Mutexes, conditions */

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

__PAS_BEGIN_EXTERN_C;

__PAS_API int pthread_create(pthread_t *thread, const pthread_attr_t *attr, unsigned (*start_routine)(void*), void *arg);
__PAS_API int pthread_detach(pthread_t thread);

__PAS_API int pthread_getname_np(pthread_t thread, const char *name, size_t len);
__PAS_API pthread_t pthread_self(void);
__PAS_API int sched_yield();

__PAS_API int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

__PAS_API int pthread_mutex_init(pthread_mutex_t *mutex, const void *unused_attr);
__PAS_API int pthread_cond_init(pthread_cond_t *cond, const void *unused_attr);

__PAS_API int pthread_cond_broadcast(pthread_cond_t *cond);

__PAS_API int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
__PAS_API int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);

__PAS_API int pthread_mutex_lock(pthread_mutex_t *mutex);
__PAS_API int pthread_mutex_unlock(pthread_mutex_t *mutex);

__PAS_END_EXTERN_C;

#endif /* PAS_OS(WINDOWS) */

#endif /* PAS_THREAD_H */
