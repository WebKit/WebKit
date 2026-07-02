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
 * \brief Simplified GLES reference context.
 *//*--------------------------------------------------------------------*/

#include "sglrContext.hpp"
#include "sglrGLContext.hpp"
#include "gluTextureUtil.hpp"

#include "glwEnums.hpp"

namespace sglr
{

using std::vector;

void Context::texImage2D(uint32_t target, int level, uint32_t internalFormat, const tcu::Surface &src)
{
    int width  = src.getWidth();
    int height = src.getHeight();
    texImage2D(target, level, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               src.getAccess().getDataPtr());
}

void Context::texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height)
{
    uint32_t format   = GL_NONE;
    uint32_t dataType = GL_NONE;

    switch (internalFormat)
    {
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
    case GL_RGB:
    case GL_RGBA:
        format   = internalFormat;
        dataType = GL_UNSIGNED_BYTE;
        break;

    default:
    {
        glu::TransferFormat transferFmt = glu::getTransferFormat(glu::mapGLInternalFormat(internalFormat));
        format                          = transferFmt.format;
        dataType                        = transferFmt.dataType;
        break;
    }
    }

    texImage2D(target, level, internalFormat, width, height, 0, format, dataType, nullptr);
}

void Context::texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, const tcu::Surface &src)
{
    int width  = src.getWidth();
    int height = src.getHeight();
    texSubImage2D(target, level, xoffset, yoffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                  src.getAccess().getDataPtr());
}

void Context::readPixels(tcu::Surface &dst, int x, int y, int width, int height)
{
    dst.setSize(width, height);
    readPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst.getAccess().getDataPtr());
}

} // namespace sglr
