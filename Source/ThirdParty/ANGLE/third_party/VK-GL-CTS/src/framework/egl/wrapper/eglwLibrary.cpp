/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Utilities
 * ------------------------------------------
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
 * \brief EGL API Library.
 *//*--------------------------------------------------------------------*/

#include "eglwLibrary.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deDynamicLibrary.hpp"

#if defined(DEQP_EGL_DIRECT_LINK)
#include <EGL/egl.h>
#endif

namespace eglw
{

FuncPtrLibrary::FuncPtrLibrary(void)
{
}

FuncPtrLibrary::~FuncPtrLibrary(void)
{
}

#include "eglwFuncPtrLibraryImpl.inl"

namespace
{

tcu::FunctionLibrary *createStaticLibrary(void)
{
#if defined(DEQP_EGL_DIRECT_LINK)
    static tcu::StaticFunctionLibrary::Entry s_staticEntries[] = {
#if defined(EGL_VERSION_1_5)
#include "eglwStaticLibrary15.inl"
#elif defined(EGL_VERSION_1_4)
#include "eglwStaticLibrary14.inl"
#endif
    };
    return new tcu::StaticFunctionLibrary(s_staticEntries, DE_LENGTH_OF_ARRAY(s_staticEntries));
#else
    return new tcu::StaticFunctionLibrary(nullptr, 0);
#endif
}

class CoreLoader : public FunctionLoader
{
public:
    CoreLoader(const de::DynamicLibrary *dynLib)
        : m_staticLib(createStaticLibrary())
        , m_dynLib(dynLib)
        , m_getProcAddress(nullptr)
    {
        // Try to obtain eglGetProcAddress
        m_getProcAddress = (eglGetProcAddressFunc)m_staticLib->getFunction("eglGetProcAddress");

        if (!m_getProcAddress && m_dynLib)
            m_getProcAddress = (eglGetProcAddressFunc)m_dynLib->getFunction("eglGetProcAddress");
    }

    ~CoreLoader(void)
    {
        delete m_staticLib;
    }

    GenericFuncType get(const char *name) const
    {
        GenericFuncType res = nullptr;

        res = (GenericFuncType)m_staticLib->getFunction(name);

        if (!res && m_dynLib)
            res = (GenericFuncType)m_dynLib->getFunction(name);

        if (!res && m_getProcAddress)
            res = (GenericFuncType)m_getProcAddress(name);

        return res;
    }

protected:
    tcu::FunctionLibrary *const m_staticLib;
    const de::DynamicLibrary *m_dynLib;
    eglGetProcAddressFunc m_getProcAddress;
};

class ExtLoader : public FunctionLoader
{
public:
    ExtLoader(const eglGetProcAddressFunc getProcAddress) : m_getProcAddress(getProcAddress)
    {
    }

    GenericFuncType get(const char *name) const
    {
        return (GenericFuncType)m_getProcAddress(name);
    }

protected:
    const eglGetProcAddressFunc m_getProcAddress;
};

} // namespace

DefaultLibrary::DefaultLibrary(const char *dynamicLibraryName) : m_dynLib(nullptr)
{
    if (dynamicLibraryName)
        m_dynLib = new de::DynamicLibrary(dynamicLibraryName);

    {
        const CoreLoader loader(m_dynLib);
        initCore(&m_egl, &loader);
    }

    if (m_egl.getProcAddress)
    {
        const ExtLoader loader(m_egl.getProcAddress);
        initExtensions(&m_egl, &loader);
    }
}

DefaultLibrary::~DefaultLibrary(void)
{
    delete m_dynLib;
}

const char *DefaultLibrary::getLibraryFileName(void)
{
#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX)
    return "libEGL.so";
#elif (DE_OS == DE_OS_WIN32)
    return "libEGL.dll";
#else
    return nullptr;
#endif
}

} // namespace eglw
