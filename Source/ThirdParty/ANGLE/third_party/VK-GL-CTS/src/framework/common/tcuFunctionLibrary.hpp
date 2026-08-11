#ifndef _TCUFUNCTIONLIBRARY_HPP
#define _TCUFUNCTIONLIBRARY_HPP
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

#include "tcuDefs.hpp"
#include "deDynamicLibrary.hpp"

#include <string>
#include <vector>
#include <map>

namespace tcu
{

// \note Returned function pointers have same lifetime as the library.
class FunctionLibrary
{
protected:
    FunctionLibrary(void);

public:
    virtual ~FunctionLibrary(void);
    virtual deFunctionPtr getFunction(const char *funcName) const = 0;

private:
    FunctionLibrary(const FunctionLibrary &);
    FunctionLibrary &operator=(const FunctionLibrary &);
};

class StaticFunctionLibrary : public FunctionLibrary
{
public:
    struct Entry
    {
        const char *name;
        deFunctionPtr ptr;
    };

    StaticFunctionLibrary(const Entry *entries, int numEntries);
    ~StaticFunctionLibrary(void);

    deFunctionPtr getFunction(const char *funcName) const;

private:
    StaticFunctionLibrary(const StaticFunctionLibrary &);
    StaticFunctionLibrary &operator=(const StaticFunctionLibrary &);

    // \todo [2014-03-11 pyry] This could be implemented with const char* pointers and custom compare.
    std::map<std::string, deFunctionPtr> m_functions;
};

class DynamicFunctionLibrary : public FunctionLibrary
{
public:
    DynamicFunctionLibrary(const char *fileName);
    ~DynamicFunctionLibrary(void);

    deFunctionPtr getFunction(const char *funcName) const;

private:
    DynamicFunctionLibrary(const DynamicFunctionLibrary &);
    DynamicFunctionLibrary &operator=(const DynamicFunctionLibrary &);

    de::DynamicLibrary m_dynamicLibrary;
};

class CompositeFunctionLibrary : public FunctionLibrary
{
public:
    CompositeFunctionLibrary(const FunctionLibrary *libraries, int numLibraries);
    ~CompositeFunctionLibrary(void);

    deFunctionPtr getFunction(const char *funcName) const;

private:
    const FunctionLibrary *const m_libraries;
    const int m_numLibraries;
};

} // namespace tcu

#endif // _TCUFUNCTIONLIBRARY_HPP
