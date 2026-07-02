#ifndef _DESTREAMCPYTHREAD_H
#define _DESTREAMCPYTHREAD_H
/*-------------------------------------------------------------------------
 * drawElements Stream Library
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
 * \brief Stream copying thread
 *//*--------------------------------------------------------------------*/

#include "deInStream.h"
#include "deOutStream.h"
#include "deThread.h"

DE_BEGIN_EXTERN_C

typedef struct deStreamCpyThread_s
{
    deInStream *input;
    deOutStream *output;
    deThread thread;
    int32_t bufferSize;
} deStreamCpyThread;

deStreamCpyThread *deStreamCpyThread_create(deInStream *input, deOutStream *output, int32_t bufferSize);
void deStreamCpyThread_destroy(deStreamCpyThread *thread);

void deStreamCpyThread_join(deStreamCpyThread *thread);

DE_END_EXTERN_C

#endif /* _DESTREAMCPYTHREAD_H */
