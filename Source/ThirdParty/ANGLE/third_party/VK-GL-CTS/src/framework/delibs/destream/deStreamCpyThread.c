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
#include "deStreamCpyThread.h"

#include <stdio.h>
#include <stdlib.h>

void cpyStream(void *arg)
{
    deStreamCpyThread *thread = (deStreamCpyThread *)arg;
    uint8_t *buffer           = malloc(sizeof(uint8_t) * (size_t)thread->bufferSize);

    for (;;)
    {
        int32_t read              = 0;
        int32_t written           = 0;
        deStreamResult readResult = DE_STREAMRESULT_ERROR;

        readResult = deInStream_read(thread->input, buffer, thread->bufferSize, &read);
        DE_ASSERT(readResult != DE_STREAMRESULT_ERROR);
        while (written < read)
        {
            int32_t wrote = 0;
            deOutStream_write(thread->output, buffer, read - written, &wrote);

            /* \todo [mika] Handle errors */
            written += wrote;
        }

        if (readResult == DE_STREAMRESULT_END_OF_STREAM)
        {
            break;
        }
    }

    deOutStream_flush(thread->output);
    free(buffer);
}

deStreamCpyThread *deStreamCpyThread_create(deInStream *input, deOutStream *output, int32_t bufferSize)
{
    deStreamCpyThread *thread = malloc(sizeof(deStreamCpyThread));

    DE_ASSERT(thread);
    DE_ASSERT(input);
    DE_ASSERT(output);

    thread->input      = input;
    thread->output     = output;
    thread->bufferSize = bufferSize;
    thread->thread     = deThread_create(cpyStream, thread, NULL);

    return thread;
}

void deStreamCpyThread_destroy(deStreamCpyThread *thread)
{
    deThread_destroy(thread->thread);

    free(thread);
}

void deStreamCpyThread_join(deStreamCpyThread *thread)
{
    deThread_join(thread->thread);
}
