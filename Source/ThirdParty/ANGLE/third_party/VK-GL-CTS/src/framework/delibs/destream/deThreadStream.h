#ifndef _DETHREADSTREAM_H
#define _DETHREADSTREAM_H
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

#include "deDefs.h"

#include "deIOStream.h"
#include "deInStream.h"
#include "deOutStream.h"

DE_BEGIN_EXTERN_C

void deThreadInStream_init(deInStream *stream, deInStream *input, int ringbufferBlockSize, int ringbufferBlockCount);
void deThreadOutStream_init(deOutStream *stream, deOutStream *output, int ringbufferBlockSize,
                            int ringbufferBlockCount);

DE_END_EXTERN_C

#endif /* _DETHREADSTREAM_H */
