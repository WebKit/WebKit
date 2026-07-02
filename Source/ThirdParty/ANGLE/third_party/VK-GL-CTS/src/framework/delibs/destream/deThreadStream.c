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
 * \brief Buffered and threaded input and output streams
 *//*--------------------------------------------------------------------*/

#include "deThreadStream.h"
#include "deStreamCpyThread.h"
#include "deRingbuffer.h"
#include "stdlib.h"

typedef struct deThreadInStream_s
{
    deRingbuffer *ringbuffer;
    deInStream *input;
    deInStream consumerStream;
    deOutStream producerStream;
    deThread thread;
    int bufferSize;
} deThreadInStream;

typedef struct deThreadOutStream_s
{
    deRingbuffer *ringbuffer;
    deInStream consumerStream;
    deOutStream producerStream;
    deStreamCpyThread *thread;
} deThreadOutStream;

static void inStreamCopy(void *arg)
{
    deThreadInStream *threadStream = (deThreadInStream *)arg;

    uint8_t *buffer = malloc(sizeof(uint8_t) * (size_t)threadStream->bufferSize);

    for (;;)
    {
        int32_t read              = 0;
        int32_t written           = 0;
        deStreamResult readResult = DE_STREAMRESULT_ERROR;

        readResult = deInStream_read(threadStream->input, buffer, threadStream->bufferSize, &read);
        DE_ASSERT(readResult != DE_STREAMRESULT_ERROR);
        while (written < read)
        {
            int32_t wrote = 0;

            /* \todo [mika] Handle errors */
            deOutStream_write(&(threadStream->producerStream), buffer, read - written, &wrote);

            written += wrote;
        }

        if (readResult == DE_STREAMRESULT_END_OF_STREAM)
        {
            break;
        }
    }

    deOutStream_flush(&(threadStream->producerStream));
    deRingbuffer_stop(threadStream->ringbuffer);
    free(buffer);
}

static deStreamResult threadInStream_read(deStreamData *stream, void *buf, int32_t bufSize, int32_t *numRead)
{
    deThreadInStream *threadStream = (deThreadInStream *)stream;
    return deInStream_read(&(threadStream->consumerStream), buf, bufSize, numRead);
}

static const char *threadInStream_getError(deStreamData *stream)
{
    deThreadInStream *threadStream = (deThreadInStream *)stream;

    /* \todo [mika] Add handling for errors on thread stream */
    return deInStream_getError(&(threadStream->consumerStream));
}

static deStreamStatus threadInStream_getStatus(deStreamData *stream)
{
    deThreadInStream *threadStream = (deThreadInStream *)stream;

    /* \todo [mika] Add handling for status on thread stream */
    return deInStream_getStatus(&(threadStream->consumerStream));
}

/* \note [mika] Used by both in and out stream */
static deStreamResult threadStream_deinit(deStreamData *stream)
{
    deThreadInStream *threadStream = (deThreadInStream *)stream;

    deRingbuffer_stop(threadStream->ringbuffer);

    deThread_join(threadStream->thread);
    deThread_destroy(threadStream->thread);

    deOutStream_deinit(&(threadStream->producerStream));
    deInStream_deinit(&(threadStream->consumerStream));

    deRingbuffer_destroy(threadStream->ringbuffer);

    return DE_STREAMRESULT_SUCCESS;
}

static const deIOStreamVFTable threadInStreamVFTable = {
    threadInStream_read, NULL, threadInStream_getError, NULL, threadStream_deinit, threadInStream_getStatus};

void deThreadInStream_init(deInStream *stream, deInStream *input, int ringbufferBlockSize, int ringbufferBlockCount)
{
    deThreadInStream *threadStream = NULL;

    threadStream = malloc(sizeof(deThreadInStream));
    DE_ASSERT(threadStream);

    threadStream->ringbuffer = deRingbuffer_create(ringbufferBlockSize, ringbufferBlockCount);
    DE_ASSERT(threadStream->ringbuffer);

    threadStream->bufferSize = ringbufferBlockSize;
    threadStream->input      = input;
    deProducerStream_init(&(threadStream->producerStream), threadStream->ringbuffer);
    deConsumerStream_init(&(threadStream->consumerStream), threadStream->ringbuffer);

    threadStream->thread        = deThread_create(inStreamCopy, threadStream, NULL);
    stream->ioStream.vfTable    = &threadInStreamVFTable;
    stream->ioStream.streamData = threadStream;
}

static deStreamResult threadOutStream_write(deStreamData *stream, const void *buf, int32_t bufSize, int32_t *numWritten)
{
    deThreadOutStream *threadStream = (deThreadOutStream *)stream;
    return deOutStream_write(&(threadStream->producerStream), buf, bufSize, numWritten);
}

static const char *threadOutStream_getError(deStreamData *stream)
{
    deThreadOutStream *threadStream = (deThreadOutStream *)stream;

    /* \todo [mika] Add handling for errors on thread stream */
    return deOutStream_getError(&(threadStream->producerStream));
}

static deStreamStatus threadOutStream_getStatus(deStreamData *stream)
{
    deThreadOutStream *threadStream = (deThreadOutStream *)stream;

    /* \todo [mika] Add handling for errors on thread stream */
    return deOutStream_getStatus(&(threadStream->producerStream));
}

static deStreamResult threadOutStream_flush(deStreamData *stream)
{
    deThreadOutStream *threadStream = (deThreadOutStream *)stream;

    return deOutStream_flush(&(threadStream->producerStream));
}

static const deIOStreamVFTable threadOutStreamVFTable = {NULL,
                                                         threadOutStream_write,
                                                         threadOutStream_getError,
                                                         threadOutStream_flush,
                                                         threadStream_deinit,
                                                         threadOutStream_getStatus};

void deThreadOutStream_init(deOutStream *stream, deOutStream *output, int ringbufferBlockSize, int ringbufferBlockCount)
{
    deThreadOutStream *threadStream = NULL;

    threadStream = malloc(sizeof(deThreadOutStream));
    DE_ASSERT(threadStream);

    threadStream->ringbuffer = deRingbuffer_create(ringbufferBlockSize, ringbufferBlockCount);
    DE_ASSERT(threadStream->ringbuffer);

    deProducerStream_init(&(threadStream->producerStream), threadStream->ringbuffer);
    deConsumerStream_init(&(threadStream->consumerStream), threadStream->ringbuffer);

    threadStream->thread     = deStreamCpyThread_create(&(threadStream->consumerStream), output, ringbufferBlockSize);
    stream->ioStream.vfTable = &threadOutStreamVFTable;
    stream->ioStream.streamData = threadStream;
}
