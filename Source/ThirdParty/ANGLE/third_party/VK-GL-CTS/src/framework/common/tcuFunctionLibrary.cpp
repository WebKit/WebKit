/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Generic interface for library containing functions.
 *//*--------------------------------------------------------------------*/

#include "tcuFunctionLibrary.hpp"

namespace tcu
{

// FunctionLibrary

FunctionLibrary::FunctionLibrary(void)
{
}

FunctionLibrary::~FunctionLibrary(void)
{
}

// StaticFunctionLibrary

StaticFunctionLibrary::StaticFunctionLibrary(const Entry *entries, int numEntries)
{
    for (int entryNdx = 0; entryNdx < numEntries; entryNdx++)
        m_functions.insert(std::make_pair(std::string(entries[entryNdx].name), entries[entryNdx].ptr));
}

StaticFunctionLibrary::~StaticFunctionLibrary(void)
{
}

deFunctionPtr StaticFunctionLibrary::getFunction(const char *funcName) const
{
    std::map<std::string, deFunctionPtr>::const_iterator iter = m_functions.find(funcName);

    if (iter == m_functions.end())
        return nullptr;
    else
        return iter->second;
}

// DynamicFunctionLibrary

DynamicFunctionLibrary::DynamicFunctionLibrary(const char *fileName) : m_dynamicLibrary(fileName)
{
}

DynamicFunctionLibrary::~DynamicFunctionLibrary(void)
{
}

deFunctionPtr DynamicFunctionLibrary::getFunction(const char *funcName) const
{
    return m_dynamicLibrary.getFunction(funcName);
}

// CompositeFunctionLibrary

CompositeFunctionLibrary::CompositeFunctionLibrary(const FunctionLibrary *libraries, int numLibraries)
    : m_libraries(libraries)
    , m_numLibraries(numLibraries)
{
}

CompositeFunctionLibrary::~CompositeFunctionLibrary(void)
{
}

deFunctionPtr CompositeFunctionLibrary::getFunction(const char *name) const
{
    for (int ndx = 0; ndx < m_numLibraries; ndx++)
    {
        const deFunctionPtr ptr = m_libraries[ndx].getFunction(name);
        if (ptr)
            return ptr;
    }
    return nullptr;
}

} // namespace tcu
