#ifndef _DEFILESTREAM_H
#define _DEFILESTREAM_H
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
 * \brief Stream wrapper for deFile
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#include "deIOStream.h"
#include "deInStream.h"
#include "deOutStream.h"

#include "deFile.h"

DE_BEGIN_EXTERN_C

void deFileIOStream_init(deIOStream *stream, const char *filename, deFileMode mode);
void deFileInStream_init(deInStream *stream, const char *filename, deFileMode mode);
void deFileOutStream_init(deOutStream *stream, const char *filename, deFileMode mode);

DE_END_EXTERN_C

#endif /* _DEFILESTREAM_H */
