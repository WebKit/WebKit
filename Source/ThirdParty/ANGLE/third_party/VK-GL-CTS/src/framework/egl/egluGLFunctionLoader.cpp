/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief glw::FunctionLoader using eglGetProcAddress() and tcu::Library.
 *//*--------------------------------------------------------------------*/

#include "egluGLFunctionLoader.hpp"
#include "egluPlatform.hpp"
#include "eglwLibrary.hpp"
#include "tcuFunctionLibrary.hpp"

namespace eglu
{

GLFunctionLoader::GLFunctionLoader(const eglw::Library &egl, const tcu::FunctionLibrary *library)
    : m_egl(egl)
    , m_library(library)
{
}

glw::GenericFuncType GLFunctionLoader::get(const char *name) const
{
    glw::GenericFuncType func = (glw::GenericFuncType)m_library->getFunction(name);

    if (!func)
        return (glw::GenericFuncType)m_egl.getProcAddress(name);
    else
        return func;
}

GLLibraryCache::GLLibraryCache(const Platform &platform, const tcu::CommandLine &cmdLine)
    : m_platform(platform)
    , m_cmdLine(cmdLine)
{
}

GLLibraryCache::~GLLibraryCache(void)
{
    for (LibraryMap::iterator i = m_libraries.begin(); i != m_libraries.end(); ++i)
        delete i->second;
}

const tcu::FunctionLibrary *GLLibraryCache::getLibrary(glu::ApiType apiType)
{
    tcu::FunctionLibrary *library = nullptr;
    const uint32_t key            = apiType.getPacked();
    LibraryMap::iterator iter     = m_libraries.find(key);

    if (iter == m_libraries.end())
    {
        library = m_platform.createDefaultGLFunctionLibrary(apiType, m_cmdLine);
        try
        {
            m_libraries.insert(std::make_pair(key, library));
        }
        catch (...)
        {
            delete library;
            throw;
        }
    }
    else
        library = iter->second;

    return library;
}

} // namespace eglu
