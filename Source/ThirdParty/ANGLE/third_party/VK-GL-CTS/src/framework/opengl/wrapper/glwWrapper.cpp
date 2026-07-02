/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
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
 * \brief OpenGL wrapper implementation.
 *//*--------------------------------------------------------------------*/

#include "glwWrapper.hpp"
#include "glwFunctions.hpp"
#include "deThreadLocal.h"

#include <stdexcept>

DE_BEGIN_EXTERN_C

#include "glwTypes.inl"
#include "glwEnums.inl"

DE_END_EXTERN_C

namespace glw
{

#if defined(DE_THREAD_LOCAL)

// Thread-local current function table.
DE_THREAD_LOCAL const glw::Functions *s_functions = nullptr;

void setCurrentThreadFunctions(const glw::Functions *gl)
{
    s_functions = gl;
}

inline const glw::Functions *getCurrentThreadFunctions(void)
{
    return s_functions;
}

#else // defined(DE_THREAD_LOCAL)

namespace
{

class FunctionTLSPtr
{
public:
    FunctionTLSPtr(void) : m_ptr(deThreadLocal_create())
    {
        if (!m_ptr)
            throw std::runtime_error("glw: TLS allocation failed");
    }

    inline void set(const glw::Functions *gl)
    {
        deThreadLocal_set(m_ptr, (void *)gl);
    }

    inline const glw::Functions *get(void) const
    {
        return (const glw::Functions *)deThreadLocal_get(m_ptr);
    }

private:
    deThreadLocal m_ptr;
};

} // namespace

// Initialized in startup.
static FunctionTLSPtr s_functions;

void setCurrentThreadFunctions(const glw::Functions *gl)
{
    s_functions.set(gl);
}

inline const glw::Functions *getCurrentThreadFunctions(void)
{
    return s_functions.get();
}

#endif // defined(DE_THREAD_LOCAL)

} // namespace glw

DE_BEGIN_EXTERN_C

#include "glwImpl.inl"

DE_END_EXTERN_C
