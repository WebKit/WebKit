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

#include "gluCallLogWrapper.hpp"
#include "gluStrUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

using tcu::TestLog;
using tcu::toHex;

namespace glu
{

CallLogWrapper::CallLogWrapper(const glw::Functions &gl, tcu::TestLog &log) : m_gl(gl), m_log(log), m_enableLog(false)
{
}

CallLogWrapper::~CallLogWrapper(void)
{
}

template <typename T>
inline tcu::Format::ArrayPointer<T> getPointerStr(const T *arr, uint32_t size)
{
    return tcu::formatArray(arr, (int)size);
}

template <typename T>
inline tcu::Format::ArrayPointer<T> getPointerStr(const T *arr, int size)
{
    return tcu::formatArray(arr, de::max(size, 0));
}

// String formatter.

class StringFmt
{
public:
    const glw::GLchar *str;
    StringFmt(const glw::GLchar *str_) : str(str_)
    {
    }
};

inline std::ostream &operator<<(std::ostream &str, StringFmt fmt)
{
    return str << (fmt.str ? (const char *)fmt.str : "NULL");
}

inline StringFmt getStringStr(const char *value)
{
    return StringFmt(value);
}
inline StringFmt getStringStr(const glw::GLubyte *value)
{
    return StringFmt((const char *)value);
}

// Framebuffer parameter pointer formatter.

class FboParamPtrFmt
{
public:
    uint32_t param;
    const int *value;

    FboParamPtrFmt(uint32_t param_, const int *value_) : param(param_), value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, FboParamPtrFmt fmt)
{
    if (fmt.value)
    {
        switch (fmt.param)
        {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return str << tcu::Format::Enum<int, 2>(getFramebufferAttachmentTypeName, *fmt.value);

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            return str << tcu::Format::Enum<int, 2>(getCubeMapFaceName, *fmt.value);

        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
            return str << tcu::Format::Enum<int, 2>(getTypeName, *fmt.value);

        case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
            return str << tcu::Format::Enum<int, 2>(getFramebufferColorEncodingName, *fmt.value);

        case GL_FRAMEBUFFER_ATTACHMENT_LAYERED:
            return str << tcu::Format::Enum<int, 2>(getBooleanName, *fmt.value);

        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
            return str << *fmt.value;

        default:
            return str << tcu::toHex(*fmt.value);
        }
    }
    else
        return str << "(null)";
}

inline FboParamPtrFmt getFramebufferAttachmentParameterValueStr(uint32_t param, const int *value)
{
    return FboParamPtrFmt(param, value);
}

#include "gluQueryUtil.inl"
#include "gluCallLogUtil.inl"

// API entry-point implementations are auto-generated
#include "gluCallLogWrapper.inl"

} // namespace glu
