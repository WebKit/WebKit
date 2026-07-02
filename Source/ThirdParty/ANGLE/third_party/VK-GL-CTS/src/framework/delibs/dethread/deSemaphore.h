#ifndef _DESEMAPHORE_H
#define _DESEMAPHORE_H
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
 * \brief Semaphore class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Semaphore type. */
typedef uintptr_t deSemaphore;

/*--------------------------------------------------------------------*//*!
 * \brief Semaphore attributes.
 *//*--------------------------------------------------------------------*/
typedef struct deSemaphoreAttributes_s
{
    uint32_t flags;
} deSemaphoreAttributes;

deSemaphore deSemaphore_create(int initialValue, const deSemaphoreAttributes *attributes);
void deSemaphore_destroy(deSemaphore semaphore);

void deSemaphore_increment(deSemaphore semaphore);
void deSemaphore_decrement(deSemaphore semaphore);

bool deSemaphore_tryDecrement(deSemaphore semaphore);

DE_END_EXTERN_C

#endif /* _DESEMAPHORE_H */
