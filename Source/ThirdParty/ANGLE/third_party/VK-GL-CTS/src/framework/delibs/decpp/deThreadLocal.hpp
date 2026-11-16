#ifndef _DETHREADLOCAL_HPP
#define _DETHREADLOCAL_HPP
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
 * \brief deThreadLocal C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deThreadLocal.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Thread-local value
 *//*--------------------------------------------------------------------*/
class ThreadLocal
{
public:
    ThreadLocal(void);
    ~ThreadLocal(void);

    inline void *get(void) const
    {
        return deThreadLocal_get(m_var);
    }
    inline void set(void *value)
    {
        deThreadLocal_set(m_var, value);
    }

private:
    ThreadLocal(const ThreadLocal &other);            // Not allowed!
    ThreadLocal &operator=(const ThreadLocal &other); // Not allowed!

    deThreadLocal m_var;
};

} // namespace de

#endif // _DETHREADLOCAL_HPP
