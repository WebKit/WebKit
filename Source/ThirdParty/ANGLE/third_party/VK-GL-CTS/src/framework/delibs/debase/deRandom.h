#ifndef _DERANDOM_H
#define _DERANDOM_H
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
 * \brief Random number generation.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Random number generator.
 *
 * Uses the Xorshift algorithm for producing pseudo-random numbers. The
 * values are generated based on an initial seed and the same seed always
 * produces the same sequence of numbers.
 *
 * See: http://en.wikipedia.org/wiki/Xorshift
 *//*--------------------------------------------------------------------*/
typedef struct deRandom_s
{
    uint32_t x; /*!< Current random state.        */
    uint32_t y;
    uint32_t z;
    uint32_t w;
} deRandom;

void deRandom_init(deRandom *rnd, uint32_t seed);
uint32_t deRandom_getUint32(deRandom *rnd);
uint64_t deRandom_getUint64(deRandom *rnd);
float deRandom_getFloat(deRandom *rnd);
double deRandom_getDouble(deRandom *rnd);
bool deRandom_getBool(deRandom *rnd);

DE_END_EXTERN_C

#endif /* _DERANDOM_H */
