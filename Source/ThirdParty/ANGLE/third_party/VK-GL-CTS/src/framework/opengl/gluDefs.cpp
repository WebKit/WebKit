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
 * \brief OpenGL ES Test Utility Library.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "gluStrUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <sstream>

namespace glu
{

Error::Error(int error, const char *message, const char *expr, const char *file, int line)
    : tcu::TestError(message, expr, file, line)
    , m_error(error)
{
}

Error::Error(int error, const std::string &message) : tcu::TestError(message), m_error(error)
{
}

Error::~Error(void) throw()
{
}

OutOfMemoryError::OutOfMemoryError(const char *message, const char *expr, const char *file, int line)
    : tcu::ResourceError(message, expr, file, line)
{
}

OutOfMemoryError::OutOfMemoryError(const std::string &message) : tcu::ResourceError(message)
{
}

OutOfMemoryError::~OutOfMemoryError(void) throw()
{
}

void checkError(const RenderContext &context, const char *msg, const char *file, int line)
{
    checkError(context.getFunctions().getError(), msg, file, line);
}

void checkError(uint32_t err, const char *msg, const char *file, int line)
{
    if (err != GL_NO_ERROR)
    {
        std::ostringstream msgStr;
        if (msg)
            msgStr << msg << ": ";

        msgStr << "glGetError() returned " << getErrorStr(err);

        if (err == GL_OUT_OF_MEMORY)
            throw OutOfMemoryError(msgStr.str().c_str(), nullptr, file, line);
        else
            throw Error(err, msgStr.str().c_str(), nullptr, file, line);
    }
}

} // namespace glu
