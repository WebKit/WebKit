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

#include "egluStrUtil.hpp"
#include "eglwEnums.hpp"

namespace eglu
{

std::ostream &operator<<(std::ostream &str, const ConfigAttribValueFmt &attribFmt)
{
    switch (attribFmt.attribute)
    {
    case EGL_COLOR_BUFFER_TYPE:
        return str << getColorBufferTypeStr(attribFmt.value);

    case EGL_CONFIG_CAVEAT:
        return str << getConfigCaveatStr(attribFmt.value);

    case EGL_CONFORMANT:
    case EGL_RENDERABLE_TYPE:
        return str << getAPIBitsStr(attribFmt.value);

    case EGL_SURFACE_TYPE:
        return str << getSurfaceBitsStr(attribFmt.value);

    case EGL_MATCH_NATIVE_PIXMAP:
        if (attribFmt.value == EGL_NONE)
            return str << "EGL_NONE";
        else
            return str << tcu::toHex(attribFmt.value);

    case EGL_TRANSPARENT_TYPE:
        return str << getTransparentTypeStr(attribFmt.value);

    case EGL_BIND_TO_TEXTURE_RGB:
    case EGL_BIND_TO_TEXTURE_RGBA:
    case EGL_NATIVE_RENDERABLE:
    case EGL_RECORDABLE_ANDROID:
        return str << getBoolDontCareStr(attribFmt.value);

    case EGL_ALPHA_MASK_SIZE:
    case EGL_ALPHA_SIZE:
    case EGL_BLUE_SIZE:
    case EGL_BUFFER_SIZE:
    case EGL_CONFIG_ID:
    case EGL_DEPTH_SIZE:
    case EGL_GREEN_SIZE:
    case EGL_LEVEL:
    case EGL_LUMINANCE_SIZE:
    case EGL_MAX_SWAP_INTERVAL:
    case EGL_MIN_SWAP_INTERVAL:
    case EGL_RED_SIZE:
    case EGL_SAMPLE_BUFFERS:
    case EGL_SAMPLES:
    case EGL_STENCIL_SIZE:
    case EGL_TRANSPARENT_RED_VALUE:
    case EGL_TRANSPARENT_GREEN_VALUE:
    case EGL_TRANSPARENT_BLUE_VALUE:
        return str << (int)attribFmt.value;

    default:
        return str << tcu::toHex(attribFmt.value);
    }
}

std::ostream &operator<<(std::ostream &str, const ContextAttribValueFmt &attribFmt)
{
    switch (attribFmt.attribute)
    {
    case EGL_CONFIG_ID:
    case EGL_CONTEXT_CLIENT_VERSION:
        return str << (int)attribFmt.value;

    case EGL_CONTEXT_CLIENT_TYPE:
        return str << getAPIStr(attribFmt.value);

    case EGL_RENDER_BUFFER:
        return str << getRenderBufferStr(attribFmt.value);

    default:
        return str << tcu::toHex(attribFmt.value);
    }
}

std::ostream &operator<<(std::ostream &str, const SurfaceAttribValueFmt &attribFmt)
{
    switch (attribFmt.attribute)
    {
    case EGL_CONFIG_ID:
    case EGL_WIDTH:
    case EGL_HEIGHT:
    case EGL_HORIZONTAL_RESOLUTION:
    case EGL_VERTICAL_RESOLUTION:
    case EGL_PIXEL_ASPECT_RATIO:
        return str << (int)attribFmt.value;

    case EGL_LARGEST_PBUFFER:
    case EGL_MIPMAP_TEXTURE:
        return str << getBoolDontCareStr(attribFmt.value);

    case EGL_MULTISAMPLE_RESOLVE:
        return str << getMultisampleResolveStr(attribFmt.value);

    case EGL_RENDER_BUFFER:
        return str << getRenderBufferStr(attribFmt.value);

    case EGL_SWAP_BEHAVIOR:
        return str << getSwapBehaviorStr(attribFmt.value);

    case EGL_TEXTURE_FORMAT:
        return str << getTextureFormatStr(attribFmt.value);

    case EGL_TEXTURE_TARGET:
        return str << getTextureTargetStr(attribFmt.value);

    case EGL_ALPHA_FORMAT:
        return str << getAlphaFormatStr(attribFmt.value);

    case EGL_COLORSPACE:
        return str << getColorspaceStr(attribFmt.value);

    default:
        return str << tcu::toHex(attribFmt.value);
    }
}

std::ostream &operator<<(std::ostream &str, const ConfigAttribListFmt &fmt)
{
    int pos = 0;

    str << "{ ";

    for (;;)
    {
        int attrib = fmt.attribs[pos];

        if (pos != 0)
            str << ", ";

        if (attrib == EGL_NONE)
        {
            // Terminate.
            str << "EGL_NONE";
            break;
        }

        const char *attribName = getConfigAttribName(attrib);

        if (attribName)
        {
            // Valid attribute, print value.
            str << attribName << ", " << getConfigAttribValueStr(attrib, fmt.attribs[pos + 1]);
            pos += 2;
        }
        else
        {
            // Invalid attribute. Terminate parsing.
            str << tcu::toHex(attrib) << ", ???";
            break;
        }
    }

    str << " }";
    return str;
}

std::ostream &operator<<(std::ostream &str, const SurfaceAttribListFmt &fmt)
{
    int pos = 0;

    str << "{ ";

    for (;;)
    {
        int attrib = fmt.attribs[pos];

        if (pos != 0)
            str << ", ";

        if (attrib == EGL_NONE)
        {
            // Terminate.
            str << "EGL_NONE";
            break;
        }

        const char *attribName = getSurfaceAttribName(attrib);

        if (attribName)
        {
            // Valid attribute, print value.
            str << attribName << ", " << getSurfaceAttribValueStr(attrib, fmt.attribs[pos + 1]);
            pos += 2;
        }
        else
        {
            // Invalid attribute. Terminate parsing.
            str << tcu::toHex(attrib) << ", ???";
            break;
        }
    }

    str << " }";
    return str;
}

std::ostream &operator<<(std::ostream &str, const ContextAttribListFmt &fmt)
{
    int pos = 0;

    str << "{ ";

    for (;;)
    {
        int attrib = fmt.attribs[pos];

        if (pos != 0)
            str << ", ";

        if (attrib == EGL_NONE)
        {
            // Terminate.
            str << "EGL_NONE";
            break;
        }

        const char *attribName = getContextAttribName(attrib);

        if (attribName)
        {
            // Valid attribute, print value.
            str << attribName << ", " << getContextAttribValueStr(attrib, fmt.attribs[pos + 1]);
            pos += 2;
        }
        else
        {
            // Invalid attribute. Terminate parsing.
            str << tcu::toHex(attrib) << ", ???";
            break;
        }
    }

    str << " }";
    return str;
}

#include "egluStrUtil.inl"

} // namespace eglu
