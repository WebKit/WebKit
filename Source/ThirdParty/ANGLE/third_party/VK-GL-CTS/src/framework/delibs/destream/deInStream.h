#ifndef _DEINSTREAM_H
#define _DEINSTREAM_H
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
 * \brief Input stream abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#include "deIOStream.h"

DE_BEGIN_EXTERN_C

/* Input stream struct, implemented as a wrapper to io stream */
typedef struct deInStream_s
{
    deIOStream ioStream;
} deInStream;

static inline deStreamResult deInStream_read(deInStream *stream, void *buf, int32_t bufSize, int32_t *numWritten);
static inline deStreamResult deInStream_deinit(deInStream *stream);
static inline const char *deInStream_getError(deInStream *stream);
static inline deStreamStatus deInStream_getStatus(deInStream *stream);

static inline deStreamResult deInStream_read(deInStream *stream, void *buf, int32_t bufSize, int32_t *numWritten)
{
    return deIOStream_read(&(stream->ioStream), buf, bufSize, numWritten);
}

static inline deStreamResult deInStream_deinit(deInStream *stream)
{
    return deIOStream_deinit(&(stream->ioStream));
}

static inline const char *deInStream_getError(deInStream *stream)
{
    return deIOStream_getError(&(stream->ioStream));
}

static inline deStreamStatus deInStream_getStatus(deInStream *stream)
{
    return deIOStream_getStatus(&(stream->ioStream));
}

DE_END_EXTERN_C

#endif /* _DEINSTREAM_H */
