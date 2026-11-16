#ifndef _GLUSTRUTIL_HPP
#define _GLUSTRUTIL_HPP
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
 * \brief OpenGL value to string utilities.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "tcuFormatUtil.hpp"

namespace glu
{

// Internal format utilities.
namespace detail
{

class EnumPointerFmt
{
public:
    typedef const char *(*GetEnumNameFunc)(int value);

    const uint32_t *const value;
    const uint32_t size;
    const GetEnumNameFunc getName;

    EnumPointerFmt(const uint32_t *value_, uint32_t size_, GetEnumNameFunc getName_)
        : value(value_)
        , size(size_)
        , getName(getName_)
    {
    }
};

class BooleanPointerFmt
{
public:
    const uint8_t *const value;
    const uint32_t size;

    BooleanPointerFmt(const uint8_t *value_, uint32_t size_) : value(value_), size(size_)
    {
    }
};

class TextureUnitStr
{
public:
    const uint32_t texUnit;
    TextureUnitStr(uint32_t texUnit_) : texUnit(texUnit_)
    {
    }
};

class TextureParameterValueStr
{
public:
    const uint32_t param;
    const int value;
    TextureParameterValueStr(uint32_t param_, int value_) : param(param_), value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const TextureUnitStr &unitStr);
std::ostream &operator<<(std::ostream &str, const TextureParameterValueStr &valueStr);
std::ostream &operator<<(std::ostream &str, const BooleanPointerFmt &fmt);
std::ostream &operator<<(std::ostream &str, const EnumPointerFmt &fmt);

} // namespace detail

inline detail::EnumPointerFmt getEnumPointerStr(const uint32_t *value, int32_t size,
                                                detail::EnumPointerFmt::GetEnumNameFunc getName)
{
    return detail::EnumPointerFmt(value, (uint32_t)de::max(0, size), getName);
}

inline detail::BooleanPointerFmt getBooleanPointerStr(const uint8_t *value, int32_t size)
{
    return detail::BooleanPointerFmt(value, (uint32_t)de::max(0, size));
}

inline detail::TextureUnitStr getTextureUnitStr(uint32_t unit)
{
    return detail::TextureUnitStr(unit);
}
inline detail::TextureParameterValueStr getTextureParameterValueStr(uint32_t param, int value)
{
    return detail::TextureParameterValueStr(param, value);
}
detail::EnumPointerFmt getInvalidateAttachmentStr(const uint32_t *attachments, int numAttachments);

std::ostream &operator<<(std::ostream &str, ApiType apiType);
std::ostream &operator<<(std::ostream &str, ContextType contextType);

// prevent implicit conversions from bool to int.
//
// While it is well-defined that (int)true == GL_TRUE and (int)false == GL_FALSE,
// using these functions to convert non-GL-types suggests a that the calling code is
// mixing and matching GLboolean and bool types which may not be safe.
//
// \note return value is void to prevent compilation. Otherwise this would only break linking.
void getBooleanPointerStr(const bool *value, int32_t size); // delete
void getBooleanStr(bool);                                   // delete
void getBooleanName(bool);                                  // delete

#include "gluStrUtilPrototypes.inl"

} // namespace glu

#endif // _GLUSTRUTIL_HPP
