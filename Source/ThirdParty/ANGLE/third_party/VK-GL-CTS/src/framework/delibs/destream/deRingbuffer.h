#ifndef _DERINGBUFFER_H
#define _DERINGBUFFER_H
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

#include "deInStream.h"
#include "deOutStream.h"

DE_BEGIN_EXTERN_C

typedef struct deRingbuffer_s deRingbuffer;

deRingbuffer *deRingbuffer_create(int32_t blockSize, int32_t blockCount);

/* Changes state of ringbuffer to stopping which will make all subsequent writes
 * to producer stream fail and notifies consumer with DE_STREAMRESULT_END_OF_STREAM
 * when buffer is empty.
 */
void deRingbuffer_stop(deRingbuffer *ringbuffer);
void deRingbuffer_destroy(deRingbuffer *ringbuffer);

void deProducerStream_init(deOutStream *stream, deRingbuffer *buffer);
void deConsumerStream_init(deInStream *stream, deRingbuffer *buffer);

DE_END_EXTERN_C

#endif /* _DERINGBUFFER_H */
