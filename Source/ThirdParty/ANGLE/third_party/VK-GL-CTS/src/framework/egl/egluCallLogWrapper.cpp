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
 * \brief EGL call wrapper for logging.
 *//*--------------------------------------------------------------------*/

#include "egluCallLogWrapper.hpp"
#include "egluStrUtil.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "deStringUtil.hpp"
#include "deInt32.h"

namespace eglu
{

using tcu::TestLog;
using tcu::toHex;

CallLogWrapper::CallLogWrapper(const eglw::Library &egl, TestLog &log) : m_egl(egl), m_log(log), m_enableLog(false)
{
}

CallLogWrapper::~CallLogWrapper(void)
{
}

// Pointer formatter.

template <typename T>
class PointerFmt
{
public:
    const T *arr;
    uint32_t size;

    PointerFmt(const T *arr_, uint32_t size_) : arr(arr_), size(size_)
    {
    }
};

template <typename T>
std::ostream &operator<<(std::ostream &str, PointerFmt<T> fmt)
{
    if (fmt.arr != nullptr)
    {
        str << "{ ";
        for (uint32_t ndx = 0; ndx < fmt.size; ndx++)
        {
            if (ndx != 0)
                str << ", ";
            str << fmt.arr[ndx];
        }
        str << " }";
        return str;
    }
    else
        return str << "(null)";
}

template <typename T>
inline PointerFmt<T> getPointerStr(const T *arr, uint32_t size)
{
    return PointerFmt<T>(arr, size);
}

typedef const char *(*GetEnumNameFunc)(int value);

// Enum pointer formatter.

class EnumPointerFmt
{
public:
    const int *value;
    GetEnumNameFunc getName;

    EnumPointerFmt(const int *value_, GetEnumNameFunc getName_) : value(value_), getName(getName_)
    {
    }
};

inline std::ostream &operator<<(std::ostream &str, EnumPointerFmt fmt)
{
    if (fmt.value)
        return str << tcu::Format::Enum<int, 2>(fmt.getName, *fmt.value);
    else
        return str << "(null)";
}

inline EnumPointerFmt getEnumPointerStr(const int *value, GetEnumNameFunc getName)
{
    return EnumPointerFmt(value, getName);
}

// String formatter.

class StringFmt
{
public:
    const char *str;
    StringFmt(const char *str_) : str(str_)
    {
    }
};

inline std::ostream &operator<<(std::ostream &str, StringFmt fmt)
{
    return str << (fmt.str ? fmt.str : "NULL");
}

inline StringFmt getStringStr(const char *value)
{
    return StringFmt(value);
}

// Config attrib pointer formatter

class ConfigAttribValuePointerFmt
{
public:
    uint32_t attrib;
    const int *value;
    ConfigAttribValuePointerFmt(uint32_t attrib_, const int *value_) : attrib(attrib_), value(value_)
    {
    }
};

inline ConfigAttribValuePointerFmt getConfigAttribValuePointerStr(uint32_t attrib, const int *value)
{
    return ConfigAttribValuePointerFmt(attrib, value);
}

inline std::ostream &operator<<(std::ostream &str, const ConfigAttribValuePointerFmt &fmt)
{
    if (fmt.value)
        return str << getConfigAttribValueStr(fmt.attrib, *fmt.value);
    else
        return str << "NULL";
}

// Context attrib pointer formatter

class ContextAttribValuePointerFmt
{
public:
    uint32_t attrib;
    const int *value;
    ContextAttribValuePointerFmt(uint32_t attrib_, const int *value_) : attrib(attrib_), value(value_)
    {
    }
};

inline ContextAttribValuePointerFmt getContextAttribValuePointerStr(uint32_t attrib, const int *value)
{
    return ContextAttribValuePointerFmt(attrib, value);
}

inline std::ostream &operator<<(std::ostream &str, const ContextAttribValuePointerFmt &fmt)
{
    if (fmt.value)
        return str << getContextAttribValueStr(fmt.attrib, *fmt.value);
    else
        return str << "NULL";
}

// Surface attrib pointer formatter

class SurfaceAttribValuePointerFmt
{
public:
    uint32_t attrib;
    const int *value;
    SurfaceAttribValuePointerFmt(uint32_t attrib_, const int *value_) : attrib(attrib_), value(value_)
    {
    }
};

inline SurfaceAttribValuePointerFmt getSurfaceAttribValuePointerStr(uint32_t attrib, const int *value)
{
    return SurfaceAttribValuePointerFmt(attrib, value);
}

inline std::ostream &operator<<(std::ostream &str, const SurfaceAttribValuePointerFmt &fmt)
{
    if (fmt.value)
        return str << getSurfaceAttribValueStr(fmt.attrib, *fmt.value);
    else
        return str << "NULL";
}

// EGLDisplay formatter

class EGLDisplayFmt
{
public:
    eglw::EGLDisplay display;
    EGLDisplayFmt(eglw::EGLDisplay display_) : display(display_)
    {
    }
};

inline EGLDisplayFmt getEGLDisplayStr(eglw::EGLDisplay display)
{
    return EGLDisplayFmt(display);
}

inline std::ostream &operator<<(std::ostream &str, const EGLDisplayFmt &fmt)
{
    if (fmt.display == EGL_NO_DISPLAY)
        return str << "EGL_NO_DISPLAY";
    else
        return str << toHex(fmt.display);
}

// EGLSurface formatter

class EGLSurfaceFmt
{
public:
    eglw::EGLSurface surface;
    EGLSurfaceFmt(eglw::EGLSurface surface_) : surface(surface_)
    {
    }
};

inline EGLSurfaceFmt getEGLSurfaceStr(eglw::EGLSurface surface)
{
    return EGLSurfaceFmt(surface);
}

inline std::ostream &operator<<(std::ostream &str, const EGLSurfaceFmt &fmt)
{
    if (fmt.surface == EGL_NO_SURFACE)
        return str << "EGL_NO_SURFACE";
    else
        return str << toHex(fmt.surface);
}

// EGLContext formatter

class EGLContextFmt
{
public:
    eglw::EGLContext context;
    EGLContextFmt(eglw::EGLContext context_) : context(context_)
    {
    }
};

inline EGLContextFmt getEGLContextStr(eglw::EGLContext context)
{
    return EGLContextFmt(context);
}

inline std::ostream &operator<<(std::ostream &str, const EGLContextFmt &fmt)
{
    if (fmt.context == EGL_NO_CONTEXT)
        return str << "EGL_NO_CONTEXT";
    else
        return str << toHex(fmt.context);
}

// API entry-point implementations are auto-generated
#include "egluCallLogWrapper.inl"

} // namespace eglu
