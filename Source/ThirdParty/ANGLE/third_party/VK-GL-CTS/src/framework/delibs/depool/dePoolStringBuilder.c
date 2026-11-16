/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
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
 * \brief Memory pool management.
 *//*--------------------------------------------------------------------*/

#include "dePoolStringBuilder.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct StringBlock_s
{
    const char *str;
    struct StringBlock_s *next;
} StringBlock;

struct dePoolStringBuilder_s
{
    deMemPool *pool;
    int length;
    StringBlock *blockListHead;
    StringBlock *blockListTail;
};

dePoolStringBuilder *dePoolStringBuilder_create(deMemPool *pool)
{
    dePoolStringBuilder *builder = DE_POOL_NEW(pool, dePoolStringBuilder);
    if (!builder)
        return NULL;

    builder->pool          = pool;
    builder->length        = 0;
    builder->blockListHead = NULL;
    builder->blockListTail = NULL;

    return builder;
}

bool dePoolStringBuilder_appendString(dePoolStringBuilder *builder, const char *str)
{
    StringBlock *block = DE_POOL_NEW(builder->pool, StringBlock);
    size_t len         = strlen(str);
    char *blockStr     = (char *)deMemPool_alloc(builder->pool, len + 1);

    if (!block || !blockStr)
        return false;

    /* Initialize block. */
    {
        char *d       = blockStr;
        const char *s = str;
        while (*s)
            *d++ = *s++;
        *d = 0;

        block->str  = blockStr;
        block->next = NULL;
    }

    /* Add block to list. */
    if (builder->blockListTail)
        builder->blockListTail->next = block;
    else
        builder->blockListHead = block;

    builder->blockListTail = block;

    builder->length += (int)len;

    return true;
}

bool dePoolStringBuilder_appendFormat(dePoolStringBuilder *builder, const char *format, ...)
{
    char buf[512];
    va_list args;
    bool ok;

    va_start(args, format);
    vsnprintf(buf, DE_LENGTH_OF_ARRAY(buf), format, args);
    ok = dePoolStringBuilder_appendString(builder, buf);
    va_end(args);

    return ok;
}

/* \todo [2009-09-05 petri] Other appends? printf style? */

int dePoolStringBuilder_getLength(dePoolStringBuilder *builder)
{
    return builder->length;
}

char *dePoolStringBuilder_dupToString(dePoolStringBuilder *builder)
{
    return dePoolStringBuilder_dupToPool(builder, builder->pool);
}

char *dePoolStringBuilder_dupToPool(dePoolStringBuilder *builder, deMemPool *pool)
{
    char *resultStr = (char *)deMemPool_alloc(pool, (size_t)builder->length + 1);

    if (resultStr)
    {
        StringBlock *block = builder->blockListHead;
        char *dstPtr       = resultStr;

        while (block)
        {
            const char *p = block->str;
            while (*p)
                *dstPtr++ = *p++;
            block = block->next;
        }

        *dstPtr++ = 0;

        DE_ASSERT((int)strlen(resultStr) == builder->length);
    }

    return resultStr;
}
