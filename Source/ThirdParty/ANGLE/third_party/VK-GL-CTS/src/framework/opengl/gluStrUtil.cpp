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

#include "gluStrUtil.hpp"
#include "glwEnums.hpp"

namespace glu
{

namespace detail
{

std::ostream &operator<<(std::ostream &str, const BooleanPointerFmt &fmt)
{
    if (fmt.value)
    {
        str << "{ ";
        for (uint32_t ndx = 0; ndx < fmt.size; ndx++)
        {
            if (ndx != 0)
                str << ", ";
            str << getBooleanStr(fmt.value[ndx]);
        }
        str << " }";
        return str;
    }
    else
        return str << "(null)";
}

std::ostream &operator<<(std::ostream &str, const EnumPointerFmt &fmt)
{
    if (fmt.value)
    {
        str << "{ ";
        for (uint32_t ndx = 0; ndx < fmt.size; ndx++)
        {
            if (ndx != 0)
                str << ", ";
            // use storage size (4) as print width for clarity
            str << tcu::Format::Enum<int, 4>(fmt.getName, fmt.value[ndx]);
        }
        str << " }";
        return str;
    }
    else
        return str << "(null)";
}

std::ostream &operator<<(std::ostream &str, const TextureUnitStr &unitStr)
{
    int unitNdx = unitStr.texUnit - GL_TEXTURE0;
    if (unitNdx >= 0)
        return str << "GL_TEXTURE" << unitNdx;
    else
        return str << tcu::toHex(unitStr.texUnit);
}

std::ostream &operator<<(std::ostream &str, const TextureParameterValueStr &valueStr)
{
    switch (valueStr.param)
    {
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
    case GL_TEXTURE_WRAP_R:
        return str << getTextureWrapModeStr(valueStr.value);

    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        return str << valueStr.value;

    case GL_TEXTURE_COMPARE_MODE:
        return str << getTextureCompareModeStr(valueStr.value);

    case GL_TEXTURE_COMPARE_FUNC:
        return str << getCompareFuncStr(valueStr.value);

    case GL_TEXTURE_SWIZZLE_R:
    case GL_TEXTURE_SWIZZLE_G:
    case GL_TEXTURE_SWIZZLE_B:
    case GL_TEXTURE_SWIZZLE_A:
        return str << getTextureSwizzleStr(valueStr.value);

    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_MAG_FILTER:
        return str << getTextureFilterStr(valueStr.value);

    case GL_DEPTH_STENCIL_TEXTURE_MODE:
        return str << getTextureDepthStencilModeStr(valueStr.value);

    default:
        return str << tcu::toHex(valueStr.value);
    }
}

} // namespace detail

detail::EnumPointerFmt getInvalidateAttachmentStr(const uint32_t *attachments, int numAttachments)
{
    return detail::EnumPointerFmt(attachments, (uint32_t)numAttachments, getInvalidateAttachmentName);
}

std::ostream &operator<<(std::ostream &str, ApiType apiType)
{
    str << "OpenGL ";

    if (apiType.getProfile() == PROFILE_ES)
        str << "ES ";

    str << apiType.getMajorVersion() << "." << apiType.getMinorVersion();

    if (apiType.getProfile() == PROFILE_CORE)
        str << " core profile";
    else if (apiType.getProfile() == PROFILE_COMPATIBILITY)
        str << " compatibility profile";
    else if (apiType.getProfile() != PROFILE_ES)
        str << " (unknown profile)";

    return str;
}

std::ostream &operator<<(std::ostream &str, ContextType contextType)
{
    str << contextType.getAPI();

    if (contextType.getFlags() != ContextFlags(0))
    {
        static const struct
        {
            ContextFlags flag;
            const char *desc;
        } s_descs[] = {
            {CONTEXT_DEBUG, "debug"}, {CONTEXT_FORWARD_COMPATIBLE, "forward-compatible"}, {CONTEXT_ROBUST, "robust"}};
        ContextFlags flags = contextType.getFlags();

        str << " (";

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_descs) && flags != 0; ndx++)
        {
            if ((flags & s_descs[ndx].flag) != 0)
            {
                if (flags != contextType.getFlags())
                    str << ", ";

                str << s_descs[ndx].desc;
                flags = flags & ~s_descs[ndx].flag;
            }
        }

        if (flags != 0)
        {
            // Unresolved
            if (flags != contextType.getFlags())
                str << ", ";
            str << tcu::toHex(flags);
        }

        str << ")";
    }

    return str;
}

#include "gluStrUtil.inl"

} // namespace glu
