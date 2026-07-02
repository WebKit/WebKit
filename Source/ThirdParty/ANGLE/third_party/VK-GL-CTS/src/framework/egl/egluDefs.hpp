#ifndef _EGLUDEFS_HPP
#define _EGLUDEFS_HPP
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

#include "tcuDefs.hpp"

#define EGLU_CHECK(EGLW) eglu::checkError((EGLW).getError(), nullptr, __FILE__, __LINE__)
#define EGLU_CHECK_MSG(EGLW, MSG) eglu::checkError((EGLW).getError(), MSG, __FILE__, __LINE__)
#define EGLU_CHECK_CALL(EGLW, CALL)                                     \
    do                                                                  \
    {                                                                   \
        (EGLW).CALL;                                                    \
        eglu::checkError((EGLW).getError(), #CALL, __FILE__, __LINE__); \
    } while (false)
#define EGLU_CHECK_CALL_FPTR(EGLW, CALL)                                \
    do                                                                  \
    {                                                                   \
        CALL;                                                           \
        eglu::checkError((EGLW).getError(), #CALL, __FILE__, __LINE__); \
    } while (false)

/*--------------------------------------------------------------------*//*!
 * \brief EGL utilities
 *//*--------------------------------------------------------------------*/
namespace eglu
{

class Error : public tcu::TestError
{
public:
    Error(uint32_t errCode, const char *errStr);
    Error(uint32_t errCode, const char *message, const char *expr, const char *file, int line);
    ~Error(void) throw()
    {
    }

    uint32_t getError(void) const
    {
        return m_error;
    }

private:
    uint32_t m_error;
};

class BadAllocError : public tcu::ResourceError
{
public:
    BadAllocError(const char *errStr);
    BadAllocError(const char *message, const char *expr, const char *file, int line);
    ~BadAllocError(void) throw()
    {
    }
};

void checkError(uint32_t err, const char *msg, const char *file, int line);

class Version
{
public:
    Version(int major, int minor) : m_major(major), m_minor(minor)
    {
    }

    int getMajor(void) const
    {
        return m_major;
    }
    int getMinor(void) const
    {
        return m_minor;
    }

    bool operator<(const Version &v) const;
    bool operator==(const Version &v) const;

    bool operator!=(const Version &v) const;
    bool operator>(const Version &v) const;
    bool operator<=(const Version &v) const;
    bool operator>=(const Version &v) const;

private:
    int m_major;
    int m_minor;
};

} // namespace eglu

#endif // _EGLUDEFS_HPP
