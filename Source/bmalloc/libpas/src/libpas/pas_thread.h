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

#include "pas_platform.h"

#if !PAS_OS(WINDOWS)
#include <pthread.h>
#else

/* Implement the subset of pthread that libpas requires to run on Windows */
#pragma once
#include <process.h>
#include <time.h>
#include <windows.h>

#define pthread_t uintptr_t

/* Threads */

typedef PINIT_ONCE pthread_once_t;

struct pthread_attr_t_internal { };
typedef struct pthread_attr_t_internal * pthread_attr_t;

struct pas_thread_t_internal { };
typedef struct pas_thread_t_internal pas_thread_t;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, unsigned (*start_routine)(void*), void *arg);
int pthread_detach(pthread_t thread);

int pthread_getname_np(pthread_t thread, const char *name, size_t len);
pthread_t pthread_self(void);
int sched_yield();

#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

/* Thread Local Storage */

#define pthread_key_t uintptr_t

int pthread_key_create(pthread_key_t *key, void (*destructor)(void*));

void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, void *value);

/* Mutexes, conditions */

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const void *unused_attr);
int pthread_cond_init(pthread_cond_t *cond, const void *unused_attr);

int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);

int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

#endif /* PAS_OS(WINDOWS) */
