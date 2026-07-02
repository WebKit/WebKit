#ifndef _DESHA1_H
#define _DESHA1_H
/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief SHA1 hash functions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

typedef struct deSha1Stream_s
{
    uint64_t size;
    uint32_t hash[5];

    uint32_t data[80];
} deSha1Stream;

typedef struct deSha1_s
{
    uint32_t hash[5];
} deSha1;

/* Initialize sha1 stream. */
void deSha1Stream_init(deSha1Stream *stream);

/* Process single 512bit chunk. */
void deSha1Stream_process(deSha1Stream *stream, size_t size, const void *data);

/* Finalize the stream and output the hash. */
void deSha1Stream_finalize(deSha1Stream *stream, deSha1 *hash);

/* Compute the sha1 hash from data. */
void deSha1_compute(deSha1 *hash, size_t size, const void *data);

/* Render sha1 hash as 40 digit hex string. */
void deSha1_render(const deSha1 *hash, char *buffer);

/* Parse sha1 from 40 digit hex string. */
bool deSha1_parse(deSha1 *hash, const char *buffer);

/* Compare hashes for equality. */
bool deSha1_equal(const deSha1 *a, const deSha1 *b);

void deSha1_selfTest(void);

DE_END_EXTERN_C

#endif /* _DESHA1_H */
