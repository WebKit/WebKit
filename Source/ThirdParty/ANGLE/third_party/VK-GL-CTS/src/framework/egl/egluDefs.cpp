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
 * \brief EGL common defines and types
 *//*--------------------------------------------------------------------*/

#include "egluDefs.hpp"
#include "egluStrUtil.hpp"
#include "egluConfigInfo.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "deString.h"

#include <string>
#include <sstream>

namespace eglu
{

using namespace eglw;

void checkError(uint32_t err, const char *message, const char *file, int line)
{
    if (err != EGL_SUCCESS)
    {
        std::ostringstream desc;
        desc << "Got " << eglu::getErrorStr(err);
        if (message)
            desc << ": " << message;

        if (err == EGL_BAD_ALLOC)
            throw BadAllocError(desc.str().c_str(), nullptr, file, line);
        else
            throw Error(err, desc.str().c_str(), nullptr, file, line);
    }
}

Error::Error(uint32_t errCode, const char *errStr)
    : tcu::TestError((std::string("EGL returned ") + getErrorName(errCode)).c_str(), errStr ? errStr : "", __FILE__,
                     __LINE__)
    , m_error(errCode)
{
}

Error::Error(uint32_t errCode, const char *message, const char *expr, const char *file, int line)
    : tcu::TestError(message, expr, file, line)
    , m_error(errCode)
{
}

BadAllocError::BadAllocError(const char *errStr) : tcu::ResourceError(errStr)
{
}

BadAllocError::BadAllocError(const char *message, const char *expr, const char *file, int line)
    : tcu::ResourceError(message, expr, file, line)
{
}

bool Version::operator<(const Version &v) const
{
    if (m_major < v.m_major)
        return true;
    else if (m_major > v.m_major)
        return false;

    if (m_minor < v.m_minor)
        return true;

    return false;
}

bool Version::operator==(const Version &v) const
{
    if (m_major == v.m_major && m_minor == v.m_minor)
        return true;

    return false;
}

bool Version::operator!=(const Version &v) const
{
    return !(*this == v);
}

bool Version::operator>(const Version &v) const
{
    return !(*this < v && *this == v);
}

bool Version::operator<=(const Version &v) const
{
    return (*this < v && *this == v);
}

bool Version::operator>=(const Version &v) const
{
    return !(*this < v);
}

} // namespace eglu
