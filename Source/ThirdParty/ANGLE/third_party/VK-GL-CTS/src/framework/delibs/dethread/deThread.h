#ifndef _DETHREAD_H
#define _DETHREAD_H
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
 * \brief Thread management.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Basic types associated with threads. */
typedef uintptr_t deThread;
typedef void (*deThreadFunc)(void *arg);

/*--------------------------------------------------------------------*//*!
 * \brief Thread scheduling priority.
 *//*--------------------------------------------------------------------*/
typedef enum deThreadPriority_e
{
    DE_THREADPRIORITY_LOWEST = 0,
    DE_THREADPRIORITY_LOW,
    DE_THREADPRIORITY_NORMAL,
    DE_THREADPRIORITY_HIGH,
    DE_THREADPRIORITY_HIGHEST,

    DE_THREADPRIORITY_LAST
} deThreadPriority;

/*--------------------------------------------------------------------*//*!
 * \brief Thread attributes.
 *//*--------------------------------------------------------------------*/
typedef struct deThreadAttributes_s
{
    deThreadPriority priority;
} deThreadAttributes;

void deSleep(uint32_t milliseconds);
void deYield(void);

deThread deThread_create(deThreadFunc func, void *arg, const deThreadAttributes *attributes);
bool deThread_join(deThread thread);
void deThread_destroy(deThread thread);

uint32_t deGetNumTotalPhysicalCores(void);
uint32_t deGetNumTotalLogicalCores(void);
uint32_t deGetNumAvailableLogicalCores(void);

DE_END_EXTERN_C

#endif /* _DETHREAD_H */
