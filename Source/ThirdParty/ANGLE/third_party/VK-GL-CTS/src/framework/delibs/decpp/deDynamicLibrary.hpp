#ifndef _DEDYNAMICLIBRARY_HPP
#define _DEDYNAMICLIBRARY_HPP
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
 * \brief deDynamicLibrary C++ wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deDynamicLibrary.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Dynamic library
 *//*--------------------------------------------------------------------*/
class DynamicLibrary
{
public:
    DynamicLibrary(const char *fileName);
    DynamicLibrary(const char *fileNames[]);
    ~DynamicLibrary(void);

    deFunctionPtr getFunction(const char *name) const
    {
        return deDynamicLibrary_getFunction(m_library, name);
    }

private:
    DynamicLibrary(const DynamicLibrary &other);            // Not allowed!
    DynamicLibrary &operator=(const DynamicLibrary &other); // Not allowed!

    deDynamicLibrary *m_library;
};

} // namespace de

#endif // _DEDYNAMICLIBRARY_HPP
