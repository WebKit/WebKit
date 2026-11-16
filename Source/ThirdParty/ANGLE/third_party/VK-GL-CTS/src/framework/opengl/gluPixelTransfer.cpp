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
 * \brief OpenGL ES Pixel Transfer Utilities.
 *//*--------------------------------------------------------------------*/

#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "deMemory.h"

namespace glu
{

static inline int getTransferAlignment(tcu::TextureFormat format)
{
    int pixelSize = format.getPixelSize();
    if (deIsPowerOfTwo32(pixelSize))
        return de::min(pixelSize, 8);
    else
        return 1;
}

/*--------------------------------------------------------------------*//*!
 * \brief Read pixels to pixel buffer access.
 * \note Stride must be default stride for format.
 *//*--------------------------------------------------------------------*/
void readPixels(const RenderContext &context, int x, int y, const tcu::PixelBufferAccess &dst)
{
    const glw::Functions &gl = context.getFunctions();

    TCU_CHECK_INTERNAL(dst.getDepth() == 1);
    TCU_CHECK_INTERNAL(dst.getRowPitch() == dst.getFormat().getPixelSize() * dst.getWidth());

    int width             = dst.getWidth();
    int height            = dst.getHeight();
    TransferFormat format = getTransferFormat(dst.getFormat());

    gl.pixelStorei(GL_PACK_ALIGNMENT, getTransferAlignment(dst.getFormat()));
    gl.readPixels(x, y, width, height, format.format, format.dataType, dst.getDataPtr());
}

/*--------------------------------------------------------------------*//*!
 * \brief Upload pixels from pixel buffer access.
 * \note Stride must be default stride for format.
 *//*--------------------------------------------------------------------*/
void texImage2D(const RenderContext &context, uint32_t target, int level, uint32_t internalFormat,
                const tcu::ConstPixelBufferAccess &src)
{
    const glw::Functions &gl = context.getFunctions();

    TCU_CHECK_INTERNAL(src.getDepth() == 1);
    TCU_CHECK_INTERNAL(src.getRowPitch() == src.getFormat().getPixelSize() * src.getWidth());

    int width             = src.getWidth();
    int height            = src.getHeight();
    TransferFormat format = getTransferFormat(src.getFormat());

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, getTransferAlignment(src.getFormat()));
    gl.texImage2D(target, level, internalFormat, width, height, 0, format.format, format.dataType, src.getDataPtr());
}

/*--------------------------------------------------------------------*//*!
 * \brief Upload pixels from pixel buffer access.
 * \note Stride must be default stride for format.
 *//*--------------------------------------------------------------------*/
void texImage3D(const RenderContext &context, uint32_t target, int level, uint32_t internalFormat,
                const tcu::ConstPixelBufferAccess &src)
{
    const glw::Functions &gl = context.getFunctions();

    TCU_CHECK_INTERNAL(src.getRowPitch() == src.getFormat().getPixelSize() * src.getWidth());
    TCU_CHECK_INTERNAL(src.getSlicePitch() == src.getRowPitch() * src.getHeight());

    int width             = src.getWidth();
    int height            = src.getHeight();
    int depth             = src.getDepth();
    TransferFormat format = getTransferFormat(src.getFormat());

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, getTransferAlignment(src.getFormat()));
    gl.texImage3D(target, level, internalFormat, width, height, depth, 0, format.format, format.dataType,
                  src.getDataPtr());
}

/*--------------------------------------------------------------------*//*!
 * \brief Upload pixels from pixel buffer access.
 * \note Stride must be default stride for format.
 *//*--------------------------------------------------------------------*/
void texSubImage2D(const RenderContext &context, uint32_t target, int level, int x, int y,
                   const tcu::ConstPixelBufferAccess &src)
{
    const glw::Functions &gl = context.getFunctions();

    TCU_CHECK_INTERNAL(src.getDepth() == 1);
    TCU_CHECK_INTERNAL(src.getRowPitch() == src.getFormat().getPixelSize() * src.getWidth());

    int width             = src.getWidth();
    int height            = src.getHeight();
    TransferFormat format = getTransferFormat(src.getFormat());

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, getTransferAlignment(src.getFormat()));
    gl.texSubImage2D(target, level, x, y, width, height, format.format, format.dataType, src.getDataPtr());
}

/*--------------------------------------------------------------------*//*!
 * \brief Upload pixels from pixel buffer access.
 * \note Stride must be default stride for format.
 *//*--------------------------------------------------------------------*/
void texSubImage3D(const RenderContext &context, uint32_t target, int level, int x, int y, int z,
                   const tcu::ConstPixelBufferAccess &src)
{
    const glw::Functions &gl = context.getFunctions();

    TCU_CHECK_INTERNAL(src.getRowPitch() == src.getFormat().getPixelSize() * src.getWidth());
    TCU_CHECK_INTERNAL(src.getSlicePitch() == src.getRowPitch() * src.getHeight());

    int width             = src.getWidth();
    int height            = src.getHeight();
    int depth             = src.getDepth();
    TransferFormat format = getTransferFormat(src.getFormat());

    gl.pixelStorei(GL_UNPACK_ALIGNMENT, getTransferAlignment(src.getFormat()));
    gl.texSubImage3D(target, level, x, y, z, width, height, depth, format.format, format.dataType, src.getDataPtr());
}

} // namespace glu
