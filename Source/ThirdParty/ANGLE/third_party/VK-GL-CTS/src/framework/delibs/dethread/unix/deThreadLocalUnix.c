/*-------------------------------------------------------------------------
 * drawElements Thread Library
 * ---------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Unix implementation of thread-local storage.
 *//*--------------------------------------------------------------------*/

#include "deThreadLocal.h"

#if (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_OSX || DE_OS == DE_OS_ANDROID || DE_OS == DE_OS_SYMBIAN || \
     DE_OS == DE_OS_IOS || DE_OS == DE_OS_QNX || DE_OS == DE_OS_FUCHSIA)

#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 500)
#error You are using too old posix API!
#endif

#include <pthread.h>

DE_STATIC_ASSERT(sizeof(pthread_key_t) <= sizeof(deThreadLocal));

/* \note 0 is valid pthread_key_t, but not valid for deThreadLocal */

deThreadLocal keyToThreadLocal(pthread_key_t key)
{
    return (deThreadLocal)(key + 1);
}

pthread_key_t threadLocalToKey(deThreadLocal threadLocal)
{
    DE_ASSERT(threadLocal != 0);
    return (pthread_key_t)(threadLocal - 1);
}

deThreadLocal deThreadLocal_create(void)
{
    pthread_key_t key = (pthread_key_t)0;
    if (pthread_key_create(&key, NULL) != 0)
        return 0;
    return keyToThreadLocal(key);
}

void deThreadLocal_destroy(deThreadLocal threadLocal)
{
    int ret = 0;
    ret     = pthread_key_delete(threadLocalToKey(threadLocal));
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

void *deThreadLocal_get(deThreadLocal threadLocal)
{
    return pthread_getspecific(threadLocalToKey(threadLocal));
}

void deThreadLocal_set(deThreadLocal threadLocal, void *value)
{
    int ret = 0;
    ret     = pthread_setspecific(threadLocalToKey(threadLocal), value);
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

#endif /* DE_OS */
