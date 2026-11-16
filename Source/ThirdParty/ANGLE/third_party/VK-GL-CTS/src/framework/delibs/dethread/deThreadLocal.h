#ifndef _DETHREADLOCAL_H
#define _DETHREADLOCAL_H
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
 * \brief Thread-local storage.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Thread-local qualifier for variable - not defined if not supported. */
#if (DE_OS == DE_OS_WIN32) && (DE_COMPILER == DE_COMPILER_MSC)
#define DE_THREAD_LOCAL __declspec(thread)
#elif ((DE_OS == DE_OS_UNIX || DE_OS == DE_OS_QNX) && \
       (DE_COMPILER == DE_COMPILER_GCC || DE_COMPILER == DE_COMPILER_CLANG))
#define DE_THREAD_LOCAL __thread
#else
/* Not supported - not defined. */
#endif

typedef uintptr_t deThreadLocal;

deThreadLocal deThreadLocal_create(void);
void deThreadLocal_destroy(deThreadLocal threadLocal);

void *deThreadLocal_get(deThreadLocal threadLocal);
void deThreadLocal_set(deThreadLocal threadLocal, void *value);

DE_END_EXTERN_C

#endif /* _DETHREADLOCAL_H */
