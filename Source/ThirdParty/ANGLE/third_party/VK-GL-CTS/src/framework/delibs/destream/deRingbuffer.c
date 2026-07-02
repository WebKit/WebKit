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
 * \brief Thread safe ringbuffer
 *//*--------------------------------------------------------------------*/
#include "deRingbuffer.h"

#include "deInt32.h"
#include "deMemory.h"
#include "deSemaphore.h"

#include <stdlib.h>
#include <stdio.h>

struct deRingbuffer_s
{
    int32_t blockSize;
    int32_t blockCount;
    int32_t *blockUsage;
    uint8_t *buffer;

    deSemaphore emptyCount;
    deSemaphore fullCount;

    int32_t outBlock;
    int32_t outPos;

    int32_t inBlock;
    int32_t inPos;

    bool stopNotified;
    bool consumerStopping;
};

deRingbuffer *deRingbuffer_create(int32_t blockSize, int32_t blockCount)
{
    deRingbuffer *ringbuffer = (deRingbuffer *)deCalloc(sizeof(deRingbuffer));

    DE_ASSERT(ringbuffer);
    DE_ASSERT(blockCount > 0);
    DE_ASSERT(blockSize > 0);

    ringbuffer->blockSize  = blockSize;
    ringbuffer->blockCount = blockCount;
    ringbuffer->buffer     = (uint8_t *)deMalloc(sizeof(uint8_t) * (size_t)blockSize * (size_t)blockCount);
    ringbuffer->blockUsage = (int32_t *)deMalloc(sizeof(uint32_t) * (size_t)blockCount);
    ringbuffer->emptyCount = deSemaphore_create(ringbuffer->blockCount, NULL);
    ringbuffer->fullCount  = deSemaphore_create(0, NULL);

    if (!ringbuffer->buffer || !ringbuffer->blockUsage || !ringbuffer->emptyCount || !ringbuffer->fullCount)
    {
        if (ringbuffer->emptyCount)
            deSemaphore_destroy(ringbuffer->emptyCount);
        if (ringbuffer->fullCount)
            deSemaphore_destroy(ringbuffer->fullCount);
        deFree(ringbuffer->buffer);
        deFree(ringbuffer->blockUsage);
        deFree(ringbuffer);
        return NULL;
    }

    memset(ringbuffer->blockUsage, 0, sizeof(int32_t) * (size_t)blockCount);

    ringbuffer->outBlock = 0;
    ringbuffer->outPos   = 0;

    ringbuffer->inBlock = 0;
    ringbuffer->inPos   = 0;

    ringbuffer->stopNotified     = false;
    ringbuffer->consumerStopping = false;

    return ringbuffer;
}

void deRingbuffer_stop(deRingbuffer *ringbuffer)
{
    /* Set notify to true and increment fullCount to let consumer continue */
    ringbuffer->stopNotified = true;
    deSemaphore_increment(ringbuffer->fullCount);
}

void deRingbuffer_destroy(deRingbuffer *ringbuffer)
{
    deSemaphore_destroy(ringbuffer->emptyCount);
    deSemaphore_destroy(ringbuffer->fullCount);

    free(ringbuffer->buffer);
    free(ringbuffer->blockUsage);
    free(ringbuffer);
}

static deStreamResult producerStream_write(deStreamData *stream, const void *buf, int32_t bufSize, int32_t *written)
{
    deRingbuffer *ringbuffer = (deRingbuffer *)stream;

    DE_ASSERT(stream);
    /* If ringbuffer is stopping return error on write */
    if (ringbuffer->stopNotified)
    {
        DE_ASSERT(false);
        return DE_STREAMRESULT_ERROR;
    }

    *written = 0;

    /* Write while more data available */
    while (*written < bufSize)
    {
        int32_t writeSize = 0;
        uint8_t *src      = NULL;
        uint8_t *dst      = NULL;

        /* If between blocks accuire new block */
        if (ringbuffer->inPos == 0)
        {
            deSemaphore_decrement(ringbuffer->emptyCount);
        }

        writeSize = deMin32(ringbuffer->blockSize - ringbuffer->inPos, bufSize - *written);
        dst       = ringbuffer->buffer + ringbuffer->blockSize * ringbuffer->inBlock + ringbuffer->inPos;
        src       = (uint8_t *)buf + *written;

        deMemcpy(dst, src, (size_t)writeSize);

        ringbuffer->inPos += writeSize;
        *written += writeSize;
        ringbuffer->blockUsage[ringbuffer->inBlock] += writeSize;

        /* Block is full move to next one (or "between" this and next block) */
        if (ringbuffer->inPos == ringbuffer->blockSize)
        {
            ringbuffer->inPos = 0;
            ringbuffer->inBlock++;

            if (ringbuffer->inBlock == ringbuffer->blockCount)
                ringbuffer->inBlock = 0;
            deSemaphore_increment(ringbuffer->fullCount);
        }
    }

    return DE_STREAMRESULT_SUCCESS;
}

static deStreamResult producerStream_flush(deStreamData *stream)
{
    deRingbuffer *ringbuffer = (deRingbuffer *)stream;

    DE_ASSERT(stream);

    /* No blocks reserved by producer */
    if (ringbuffer->inPos == 0)
        return DE_STREAMRESULT_SUCCESS;

    ringbuffer->inPos = 0;
    ringbuffer->inBlock++;

    if (ringbuffer->inBlock == ringbuffer->blockCount)
        ringbuffer->inBlock = 0;

    deSemaphore_increment(ringbuffer->fullCount);
    return DE_STREAMRESULT_SUCCESS;
}

static deStreamResult producerStream_deinit(deStreamData *stream)
{
    DE_ASSERT(stream);

    producerStream_flush(stream);

    /* \note mika Stream doesn't own ringbuffer, so it's not deallocated */
    return DE_STREAMRESULT_SUCCESS;
}

static deStreamResult consumerStream_read(deStreamData *stream, void *buf, int32_t bufSize, int32_t *read)
{
    deRingbuffer *ringbuffer = (deRingbuffer *)stream;

    DE_ASSERT(stream);

    *read = 0;
    DE_ASSERT(ringbuffer);

    while (*read < bufSize)
    {
        int32_t writeSize = 0;
        uint8_t *src      = NULL;
        uint8_t *dst      = NULL;

        /* If between blocks accuire new block */
        if (ringbuffer->outPos == 0)
        {
            /* If consumer is set to stop after everything is consumed,
             * do not block if there is no more input left
             */
            if (ringbuffer->consumerStopping)
            {
                /* Try to accuire new block, if can't there is no more input */
                if (!deSemaphore_tryDecrement(ringbuffer->fullCount))
                {
                    return DE_STREAMRESULT_END_OF_STREAM;
                }
            }
            else
            {
                /* If not stopping block until there is more input */
                deSemaphore_decrement(ringbuffer->fullCount);
                /* Ringbuffer was set to stop */
                if (ringbuffer->stopNotified)
                {
                    ringbuffer->consumerStopping = true;
                }
            }
        }

        writeSize = deMin32(ringbuffer->blockUsage[ringbuffer->outBlock] - ringbuffer->outPos, bufSize - *read);
        src       = ringbuffer->buffer + ringbuffer->blockSize * ringbuffer->outBlock + ringbuffer->outPos;
        dst       = (uint8_t *)buf + *read;

        deMemcpy(dst, src, (size_t)writeSize);

        ringbuffer->outPos += writeSize;
        *read += writeSize;

        /* Block is consumed move to next one (or "between" this and next block) */
        if (ringbuffer->outPos == ringbuffer->blockUsage[ringbuffer->outBlock])
        {
            ringbuffer->blockUsage[ringbuffer->outBlock] = 0;
            ringbuffer->outPos                           = 0;
            ringbuffer->outBlock++;

            if (ringbuffer->outBlock == ringbuffer->blockCount)
                ringbuffer->outBlock = 0;

            deSemaphore_increment(ringbuffer->emptyCount);
        }
    }

    return DE_STREAMRESULT_SUCCESS;
}

static deStreamResult consumerStream_deinit(deStreamData *stream)
{
    DE_ASSERT(stream);
    DE_UNREF(stream);

    return DE_STREAMRESULT_SUCCESS;
}

/* There are no sensible errors so status is always good */
deStreamStatus empty_getStatus(deStreamData *stream)
{
    DE_UNREF(stream);

    return DE_STREAMSTATUS_GOOD;
}

/* There are no sensible errors in ringbuffer */
static const char *empty_getError(deStreamData *stream)
{
    DE_ASSERT(stream);
    DE_UNREF(stream);
    return NULL;
}

static const deIOStreamVFTable producerStreamVFTable = {
    NULL, producerStream_write, empty_getError, producerStream_flush, producerStream_deinit, empty_getStatus};

static const deIOStreamVFTable consumerStreamVFTable = {consumerStream_read,   NULL,           empty_getError, NULL,
                                                        consumerStream_deinit, empty_getStatus};

void deProducerStream_init(deOutStream *stream, deRingbuffer *buffer)
{
    stream->ioStream.streamData = (deStreamData *)buffer;
    stream->ioStream.vfTable    = &producerStreamVFTable;
}

void deConsumerStream_init(deInStream *stream, deRingbuffer *buffer)
{
    stream->ioStream.streamData = (deStreamData *)buffer;
    stream->ioStream.vfTable    = &consumerStreamVFTable;
}
