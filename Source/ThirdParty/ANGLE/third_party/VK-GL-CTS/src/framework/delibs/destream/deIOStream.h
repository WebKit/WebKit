#ifndef _DEIOSTREAM_H
#define _DEIOSTREAM_H
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
 * \brief Input-output stream abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Result of operation on stream */
typedef enum deStreamResult_e
{
    DE_STREAMRESULT_SUCCESS = 0,
    DE_STREAMRESULT_END_OF_STREAM,
    DE_STREAMRESULT_ERROR,

    DE_STREAMRESULT_LAST
} deStreamResult;

typedef enum deStreamStatus_e
{
    DE_STREAMSTATUS_GOOD = 0,
    DE_STREAMSTATUS_ERROR,

    DE_STREAMSTATUS_LAST
} deStreamStatus;

/* Type for pointer to internal stream psecifig data */
typedef void deStreamData;

/* Function types for v_table */
typedef deStreamResult (*deIOStreamReadFunc)(deStreamData *stream, void *buf, int32_t bufSize, int32_t *numRead);
typedef deStreamResult (*deIOStreamWriteFunc)(deStreamData *stream, const void *buf, int32_t bufSize,
                                              int32_t *numWritten);
typedef const char *(*deIOStreamGetErrorFunc)(deStreamData *stream);
typedef deStreamResult (*deIOStreamFlushFunc)(deStreamData *stream);
typedef deStreamResult (*deIOStreamDeinitFunc)(deStreamData *stream);
typedef deStreamStatus (*deIOStreamStatusFunc)(deStreamData *stream);

/* Virtual table type for specifying stream specifig behaviour */
typedef struct deIOStreamVFTable_s
{
    deIOStreamReadFunc readFunc;
    deIOStreamWriteFunc writeFunc;
    deIOStreamGetErrorFunc getErrorFunc;
    deIOStreamFlushFunc flushFunc;
    deIOStreamDeinitFunc deinitFunc;
    deIOStreamStatusFunc statusFunc;
} deIOStreamVFTable;

/* Generig IOStream struct */
typedef struct deIOStream_s
{
    deStreamData *streamData;
    const deIOStreamVFTable *vfTable;
} deIOStream;

static inline deStreamResult deIOStream_read(deIOStream *stream, void *buf, int32_t bufSize, int32_t *numRead);
static inline deStreamResult deIOStream_write(deIOStream *stream, const void *buf, int32_t bufSize,
                                              int32_t *numWritten);
static inline const char *deIOStream_getError(deIOStream *stream);
static inline deStreamStatus deIOStream_getStatus(deIOStream *stream);
static inline deStreamResult deIOStream_flush(deIOStream *stream);
static inline deStreamResult deIOStream_deinit(deIOStream *stream);

static inline deStreamResult deIOStream_write(deIOStream *stream, const void *buf, int32_t bufSize, int32_t *numWritten)
{
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->writeFunc);

    return stream->vfTable->writeFunc(stream->streamData, buf, bufSize, numWritten);
}

static inline deStreamResult deIOStream_read(deIOStream *stream, void *buf, int32_t bufSize, int32_t *numRead)
{
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->readFunc);

    return stream->vfTable->readFunc(stream->streamData, buf, bufSize, numRead);
}

static inline const char *deIOStream_getError(deIOStream *stream)
{
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->getErrorFunc);

    return stream->vfTable->getErrorFunc(stream->streamData);
}

static inline deStreamResult deIOStream_flush(deIOStream *stream)
{
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->flushFunc);

    return stream->vfTable->flushFunc(stream->streamData);
}

static inline deStreamResult deIOStream_deinit(deIOStream *stream)
{
    deStreamResult result = DE_STREAMRESULT_ERROR;
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->deinitFunc);

    result = stream->vfTable->deinitFunc(stream->streamData);

    stream->vfTable    = NULL;
    stream->streamData = NULL;

    return result;
}

static inline deStreamStatus deIOStream_getStatus(deIOStream *stream)
{
    DE_ASSERT(stream);
    DE_ASSERT(stream->vfTable);
    DE_ASSERT(stream->vfTable->statusFunc);

    return stream->vfTable->statusFunc(stream->streamData);
}

DE_END_EXTERN_C

#endif /* _DEIOSTREAM_H */
