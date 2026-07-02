#ifndef _EGLUSTRUTIL_HPP
#define _EGLUSTRUTIL_HPP
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
 * \brief EGL String Utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuFormatUtil.hpp"

namespace eglu
{

struct ConfigAttribValueFmt
{
    uint32_t attribute;
    int value;
    ConfigAttribValueFmt(uint32_t attribute_, int value_) : attribute(attribute_), value(value_)
    {
    }
};

struct SurfaceAttribValueFmt
{
    uint32_t attribute;
    int value;
    SurfaceAttribValueFmt(uint32_t attribute_, int value_) : attribute(attribute_), value(value_)
    {
    }
};

struct ContextAttribValueFmt
{
    uint32_t attribute;
    int value;
    ContextAttribValueFmt(uint32_t attribute_, int value_) : attribute(attribute_), value(value_)
    {
    }
};

struct ConfigAttribListFmt
{
    const int *attribs;
    ConfigAttribListFmt(const int *attribs_) : attribs(attribs_)
    {
    }
};

struct SurfaceAttribListFmt
{
    const int *attribs;
    SurfaceAttribListFmt(const int *attribs_) : attribs(attribs_)
    {
    }
};

struct ContextAttribListFmt
{
    const int *attribs;
    ContextAttribListFmt(const int *attribs_) : attribs(attribs_)
    {
    }
};

inline ConfigAttribValueFmt getConfigAttribValueStr(uint32_t attribute, int value)
{
    return ConfigAttribValueFmt(attribute, value);
}
std::ostream &operator<<(std::ostream &str, const ConfigAttribValueFmt &attribFmt);

inline SurfaceAttribValueFmt getSurfaceAttribValueStr(uint32_t attribute, int value)
{
    return SurfaceAttribValueFmt(attribute, value);
}
std::ostream &operator<<(std::ostream &str, const SurfaceAttribValueFmt &attribFmt);

inline ContextAttribValueFmt getContextAttribValueStr(uint32_t attribute, int value)
{
    return ContextAttribValueFmt(attribute, value);
}
std::ostream &operator<<(std::ostream &str, const ContextAttribValueFmt &attribFmt);

inline ConfigAttribListFmt getConfigAttribListStr(const int *attribs)
{
    return ConfigAttribListFmt(attribs);
}
std::ostream &operator<<(std::ostream &str, const ConfigAttribListFmt &fmt);

inline SurfaceAttribListFmt getSurfaceAttribListStr(const int *attribs)
{
    return SurfaceAttribListFmt(attribs);
}
std::ostream &operator<<(std::ostream &str, const SurfaceAttribListFmt &fmt);

inline ContextAttribListFmt getContextAttribListStr(const int *attribs)
{
    return ContextAttribListFmt(attribs);
}
std::ostream &operator<<(std::ostream &str, const ContextAttribListFmt &fmt);

#include "egluStrUtilPrototypes.inl"

} // namespace eglu

#endif // _EGLUSTRUTIL_HPP
