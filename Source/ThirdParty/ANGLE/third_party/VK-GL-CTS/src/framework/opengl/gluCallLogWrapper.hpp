#ifndef _GLUCALLLOGWRAPPER_HPP
#define _GLUCALLLOGWRAPPER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief GL call wrapper for logging.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "tcuTestLog.hpp"
#include "glwDefs.hpp"

namespace glw
{
class Functions;
}

namespace glu
{

class CallLogWrapper
{
public:
    CallLogWrapper(const glw::Functions &gl, tcu::TestLog &log);
    ~CallLogWrapper(void);

// GL API is exposed as member functions
#include "gluCallLogWrapperApi.inl"

    void enableLogging(bool enable)
    {
        m_enableLog = enable;
    }
    bool isLoggingEnabled(void)
    {
        return m_enableLog;
    }
    tcu::TestLog &getLog(void)
    {
        return m_log;
    }

private:
    const glw::Functions &m_gl;
    tcu::TestLog &m_log;
    bool m_enableLog;
} DE_WARN_UNUSED_TYPE;

} // namespace glu

#endif // _GLUCALLLOGWRAPPER_HPP
