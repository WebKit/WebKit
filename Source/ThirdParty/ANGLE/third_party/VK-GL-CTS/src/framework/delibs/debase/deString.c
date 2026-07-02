/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief Basic string operations.
 *//*--------------------------------------------------------------------*/

#include "deString.h"

#include <string.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdarg.h>

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Compute hash from string.
 * \param str    String to compute hash value for.
 * \return Computed hash value.
 *//*--------------------------------------------------------------------*/
uint32_t deStringHash(const char *str)
{
    /* \note [pyry] This hash is used in DT_GNU_HASH and is proven
    to be robust for symbol hashing. */
    /* \see http://sources.redhat.com/ml/binutils/2006-06/msg00418.html */
    uint32_t hash = 5381;
    unsigned int c;

    DE_ASSERT(str);
    while ((c = (unsigned int)*str++) != 0)
        hash = (hash << 5) + hash + c;

    return hash;
}

uint32_t deStringHashLeading(const char *str, int numLeadingChars)
{
    uint32_t hash = 5381;
    unsigned int c;

    DE_ASSERT(str);
    while (numLeadingChars-- && (c = (unsigned int)*str++) != 0)
        hash = (hash << 5) + hash + c;

    return hash;
}

uint32_t deMemoryHash(const void *ptr, size_t numBytes)
{
    /* \todo [2010-05-10 pyry] Better generic hash function? */
    const uint8_t *input = (const uint8_t *)ptr;
    uint32_t hash        = 5381;

    DE_ASSERT(ptr);
    while (numBytes--)
        hash = (hash << 5) + hash + *input++;

    return hash;
}

bool deStringBeginsWith(const char *str, const char *lead)
{
    const char *a = str;
    const char *b = lead;

    while (*b)
    {
        if (*a++ != *b++)
            return false;
    }

    return true;
}

DE_END_EXTERN_C
