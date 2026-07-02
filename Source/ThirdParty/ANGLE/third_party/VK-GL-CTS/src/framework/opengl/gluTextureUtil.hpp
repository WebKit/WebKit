#ifndef _GLUTEXTUREUTIL_HPP
#define _GLUTEXTUREUTIL_HPP
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
 * \brief Texture format utilities.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "gluShaderUtil.hpp"
#include "deInt32.h"

namespace glu
{

class RenderContext;
class ContextInfo;
class TextureBuffer;

/*--------------------------------------------------------------------*//*!
 * \brief GL pixel transfer format.
 *//*--------------------------------------------------------------------*/
struct TransferFormat
{
    uint32_t format;   //!< Pixel format.
    uint32_t dataType; //!< Data type.

    TransferFormat(void) : format(0), dataType(0)
    {
    }

    TransferFormat(uint32_t format_, uint32_t dataType_) : format(format_), dataType(dataType_)
    {
    }
} DE_WARN_UNUSED_TYPE;

tcu::TextureFormat mapGLTransferFormat(uint32_t format, uint32_t dataType);
tcu::TextureFormat mapGLInternalFormat(uint32_t internalFormat);
tcu::CompressedTexFormat mapGLCompressedTexFormat(uint32_t format);
bool isGLInternalColorFormatFilterable(uint32_t internalFormat);
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t minFilter, uint32_t magFilter);
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter);
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t wrapT, uint32_t wrapR, uint32_t minFilter, uint32_t magFilter);
tcu::Sampler::CompareMode mapGLCompareFunc(uint32_t mode);

TransferFormat getTransferFormat(tcu::TextureFormat format);
uint32_t getInternalFormat(tcu::TextureFormat format);
uint32_t getGLFormat(tcu::CompressedTexFormat format);

uint32_t getGLWrapMode(tcu::Sampler::WrapMode wrapMode);
uint32_t getGLFilterMode(tcu::Sampler::FilterMode filterMode);
uint32_t getGLCompareFunc(tcu::Sampler::CompareMode compareMode);

uint32_t getGLCubeFace(tcu::CubeFace face);
tcu::CubeFace getCubeFaceFromGL(uint32_t face);

DataType getSampler1DType(tcu::TextureFormat format);
DataType getSampler2DType(tcu::TextureFormat format);
DataType getSamplerCubeType(tcu::TextureFormat format);
DataType getSampler1DArrayType(tcu::TextureFormat format);
DataType getSampler2DArrayType(tcu::TextureFormat format);
DataType getSampler3DType(tcu::TextureFormat format);
DataType getSamplerCubeArrayType(tcu::TextureFormat format);

bool isSizedFormatColorRenderable(const RenderContext &renderCtx, const ContextInfo &contextInfo, uint32_t sizedFormat);
bool isCompressedFormat(uint32_t internalFormat);

const tcu::IVec2 (&getDefaultGatherOffsets(void))[4];

tcu::PixelBufferAccess getTextureBufferEffectiveRefTexture(TextureBuffer &buffer, int maxTextureBufferSize);
tcu::ConstPixelBufferAccess getTextureBufferEffectiveRefTexture(const TextureBuffer &buffer, int maxTextureBufferSize);

} // namespace glu

#endif // _GLUTEXTUREUTIL_HPP
