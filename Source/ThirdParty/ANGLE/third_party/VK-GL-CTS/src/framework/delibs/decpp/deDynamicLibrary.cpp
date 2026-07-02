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

#include "deDynamicLibrary.hpp"

#include <string>
#include <stdexcept>

namespace de
{

DynamicLibrary::DynamicLibrary(const char *fileName) : m_library(nullptr)
{
    m_library = deDynamicLibrary_open(fileName);
    if (!m_library)
        throw std::runtime_error(std::string("Failed to open dynamic library: '") + fileName + "'");
}

DynamicLibrary::DynamicLibrary(const char *fileNames[]) : m_library(nullptr)
{
    for (size_t i = 0u; fileNames[i] != nullptr; ++i)
    {
        m_library = deDynamicLibrary_open(fileNames[i]);
        if (m_library)
            break;
    }

    if (!m_library)
    {
        std::string nameList;
        for (size_t i = 0u; fileNames[i] != nullptr; ++i)
            nameList += (nameList.empty() ? "" : ", ") + std::string(fileNames[i]);
        const std::string msg = "Failed to open dynamic library: tried " + nameList;
        throw std::runtime_error(msg);
    }
}

DynamicLibrary::~DynamicLibrary(void)
{
    deDynamicLibrary_close(m_library);
}

} // namespace de
