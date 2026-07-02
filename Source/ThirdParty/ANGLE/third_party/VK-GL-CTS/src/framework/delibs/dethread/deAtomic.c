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
 * \brief Atomic operations.
 *//*--------------------------------------------------------------------*/

#include "deAtomic.h"

#if (DE_COMPILER == DE_COMPILER_MSC)
/* When compiling with MSC we assume that long can be exchanged to int. */
DE_STATIC_ASSERT(sizeof(long) == sizeof(int32_t));

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

void deMemoryReadWriteFence(void)
{
    MemoryBarrier();
}

#endif
