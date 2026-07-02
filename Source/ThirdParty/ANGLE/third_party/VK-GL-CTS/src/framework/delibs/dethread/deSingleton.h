#ifndef _DESINGLETON_H
#define _DESINGLETON_H
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

#include "deDefs.h"

typedef enum deSingletonState_e
{
    DE_SINGLETON_STATE_NOT_INITIALIZED = 0,
    DE_SINGLETON_STATE_INITIALIZING,
    DE_SINGLETON_STATE_INITIALIZED,

    DE_SINGLETON_STATE_LAST
} deSingletonState;

DE_BEGIN_EXTERN_C

typedef void (*deSingletonConstructorFunc)(void *arg);

/*--------------------------------------------------------------------*//*!
 * \brief Initialize singleton.
 *
 * This function ensures that singletonState = DE_SINGLETON_STATE_INITIALIZED
 * upon return.
 *
 * If current singleton state is DE_SINGLETON_NOT_INITIALIZED, constructor
 * function is called with the supplied argument (arg).
 *
 * It is guaranteed that constructor is called only once, even when multiple
 * concurrent calls are made to deInitSingleton().
 *
 * Note that singletonState memory location must be initialized to
 * DE_SINGLETON_STATE_NOT_INITIALIZED prior to any calls to deInitSingleton().
 *
 * \param singletonState    Pointer to singleton state.
 * \param constructor        Constructor function.
 * \param arg                Generic arg pointer for constructor.
 *//*--------------------------------------------------------------------*/
void deInitSingleton(volatile deSingletonState *singletonState, deSingletonConstructorFunc constructor, void *arg);

void deSingleton_selfTest(void);

DE_END_EXTERN_C

#endif /* _DESINGLETON_H */
