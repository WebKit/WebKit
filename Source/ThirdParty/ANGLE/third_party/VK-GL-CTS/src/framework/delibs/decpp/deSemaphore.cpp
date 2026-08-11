/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief deSemaphore C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deSemaphore.hpp"
#include "deMemory.h"

#include <new>

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Construct semaphore.
 * \param initialValue    Initial value for semaphore. Must be 0 or greater.
 * \param flags            Semaphore flags (reserved for further use).
 *//*--------------------------------------------------------------------*/
Semaphore::Semaphore(int initialValue, uint32_t flags)
{
    deSemaphoreAttributes attribs;
    deMemset(&attribs, 0, sizeof(attribs));
    attribs.flags = flags;

    m_semaphore = deSemaphore_create(initialValue, &attribs);
    if (!m_semaphore)
        throw std::bad_alloc();
}

Semaphore::~Semaphore(void)
{
    deSemaphore_destroy(m_semaphore);
}

} // namespace de
