/*-------------------------------------------------------------------------
 * drawElements Thread Library
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
 * \brief Thread-safe singleton.
 *//*--------------------------------------------------------------------*/

#include "deSingleton.h"
#include "deAtomic.h"
#include "deThread.h"

DE_STATIC_ASSERT(sizeof(deSingletonState) == sizeof(uint32_t));

void deInitSingleton(volatile deSingletonState *singletonState, deSingletonConstructorFunc constructor, void *arg)
{
    if (*singletonState != DE_SINGLETON_STATE_INITIALIZED)
    {
        deSingletonState curState = (deSingletonState)deAtomicCompareExchange32(
            (volatile uint32_t *)singletonState, (uint32_t)DE_SINGLETON_STATE_NOT_INITIALIZED,
            (uint32_t)DE_SINGLETON_STATE_INITIALIZING);

        if (curState == DE_SINGLETON_STATE_NOT_INITIALIZED)
        {
            constructor(arg);

            deMemoryReadWriteFence();

            *singletonState = DE_SINGLETON_STATE_INITIALIZED;

            deMemoryReadWriteFence();
        }
        else if (curState == DE_SINGLETON_STATE_INITIALIZING)
        {
            for (;;)
            {
                deMemoryReadWriteFence();

                if (*singletonState == DE_SINGLETON_STATE_INITIALIZED)
                    break;
                else
                    deYield();
            }
        }

        DE_ASSERT(*singletonState == DE_SINGLETON_STATE_INITIALIZED);
    }
}
