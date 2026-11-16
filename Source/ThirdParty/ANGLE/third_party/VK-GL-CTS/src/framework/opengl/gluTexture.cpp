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
 * \brief Texture classes.
 *//*--------------------------------------------------------------------*/

#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "deFilePath.hpp"
#include "tcuImageIO.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deUniquePtr.hpp"

using std::vector;

namespace glu
{

static inline int computePixelStore(const tcu::TextureFormat &format)
{
    int pixelSize = format.getPixelSize();
    if (deIsPowerOfTwo32(pixelSize))
        return de::min(pixelSize, 8);
    else
        return 1;
}

// Texture1D

Texture1D::Texture1D(const RenderContext &context, uint32_t format, uint32_t dataType, int width)
    : m_context(context)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), width)
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture1D::Texture1D(const RenderContext &context, uint32_t sizedFormat, int width)
    : m_context(context)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), width)
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture1D::~Texture1D(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void Texture1D::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_1D, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        gl.texImage1D(GL_TEXTURE_1D, levelNdx, m_format, access.getWidth(), 0 /* border */, transferFormat.format,
                      transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// Texture2D

Texture2D::Texture2D(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), width, height, isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture2D::Texture2D(const RenderContext &context, uint32_t sizedFormat, int width, int height)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), width, height, isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture2D::Texture2D(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                     const tcu::CompressedTexture *levels, const tcu::TexDecompressionParams &decompressionParams)
    : m_context(context)
    , m_isCompressed(true)
    , m_format(getGLFormat(levels[0].getFormat()))
    , m_refTexture(getUncompressedFormat(levels[0].getFormat()), levels[0].getWidth(), levels[0].getHeight(),
                   isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();

    if (!contextInfo.isCompressedTextureFormatSupported(m_format))
        TCU_THROW(NotSupportedError, "Compressed texture format not supported");

    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

    try
    {
        loadCompressed(numLevels, levels, decompressionParams);
    }
    catch (const std::exception &)
    {
        gl.deleteTextures(1, &m_glTexture);
        throw;
    }
}

Texture2D::~Texture2D(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void Texture2D::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    DE_ASSERT(!m_isCompressed);

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_2D, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        gl.texImage2D(GL_TEXTURE_2D, levelNdx, m_format, access.getWidth(), access.getHeight(), 0 /* border */,
                      transferFormat.format, transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

void Texture2D::loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                               const tcu::TexDecompressionParams &decompressionParams)
{
    const glw::Functions &gl  = m_context.getFunctions();
    uint32_t compressedFormat = getGLFormat(levels[0].getFormat());

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_2D, m_glTexture);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        const tcu::CompressedTexture &level = levels[levelNdx];

        // Decompress to reference texture.
        m_refTexture.allocLevel(levelNdx);
        tcu::PixelBufferAccess refLevelAccess = m_refTexture.getLevel(levelNdx);
        TCU_CHECK(level.getWidth() == refLevelAccess.getWidth() && level.getHeight() == refLevelAccess.getHeight());
        level.decompress(refLevelAccess, decompressionParams);

        // Upload to GL texture in compressed form.
        gl.compressedTexImage2D(GL_TEXTURE_2D, levelNdx, compressedFormat, level.getWidth(), level.getHeight(),
                                0 /* border */, level.getDataSize(), level.getData());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

Texture2D *Texture2D::create(const RenderContext &context, const ContextInfo &contextInfo, const tcu::Archive &archive,
                             int numLevels, const char *const *levelFileNames)
{
    DE_ASSERT(numLevels > 0);

    std::string ext = de::FilePath(levelFileNames[0]).getFileExtension();

    if (ext == "png")
    {
        // Uncompressed texture.

        tcu::TextureLevel level;

        // Load level 0.
        tcu::ImageIO::loadPNG(level, archive, levelFileNames[0]);

        TCU_CHECK_INTERNAL(
            level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8) ||
            level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8));

        bool isRGBA = level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
        Texture2D *texture =
            new Texture2D(context, isRGBA ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, level.getWidth(), level.getHeight());

        try
        {
            // Fill level 0.
            texture->getRefTexture().allocLevel(0);
            tcu::copy(texture->getRefTexture().getLevel(0), level.getAccess());

            // Fill remaining levels.
            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                tcu::ImageIO::loadPNG(level, archive, levelFileNames[levelNdx]);

                texture->getRefTexture().allocLevel(levelNdx);
                tcu::copy(texture->getRefTexture().getLevel(levelNdx), level.getAccess());
            }

            // Upload data.
            texture->upload();
        }
        catch (const std::exception &)
        {
            delete texture;
            throw;
        }

        return texture;
    }
    else if (ext == "pkm")
    {
        // Compressed texture.
        vector<tcu::CompressedTexture> levels(numLevels);

        for (int ndx = 0; ndx < numLevels; ndx++)
            tcu::ImageIO::loadPKM(levels[ndx], archive, levelFileNames[ndx]);

        return new Texture2D(context, contextInfo, numLevels, &levels[0]);
    }
    else
        TCU_FAIL("Unsupported file format");
}

Texture2D *Texture2D::create(const RenderContext &context, const ContextInfo &contextInfo, const tcu::Archive &archive,
                             int numLevels, const std::vector<std::string> &filenames)
{
    TCU_CHECK(numLevels == (int)filenames.size());

    std::vector<const char *> charPtrs(filenames.size());
    for (int ndx = 0; ndx < (int)filenames.size(); ndx++)
        charPtrs[ndx] = filenames[ndx].c_str();

    return Texture2D::create(context, contextInfo, archive, numLevels, &charPtrs[0]);
}

// ImmutableTexture2D

ImmutableTexture2D::ImmutableTexture2D(const RenderContext &context, uint32_t sizedFormat, int width, int height)
    : Texture2D(context, sizedFormat, width, height)
{
}

void ImmutableTexture2D::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    DE_ASSERT(!m_isCompressed);

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_2D, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    gl.texStorage2D(GL_TEXTURE_2D, m_refTexture.getNumLevels(), m_format, m_refTexture.getWidth(),
                    m_refTexture.getHeight());
    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        gl.texSubImage2D(GL_TEXTURE_2D, levelNdx, 0, 0, access.getWidth(), access.getHeight(), transferFormat.format,
                         transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// TextureCube

TextureCube::TextureCube(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                         const tcu::CompressedTexture *levels, const tcu::TexDecompressionParams &decompressionParams)
    : m_context(context)
    , m_isCompressed(true)
    , m_format(getGLFormat(levels[0].getFormat()))
    , m_refTexture(getUncompressedFormat(levels[0].getFormat()), levels[0].getWidth(), isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = m_context.getFunctions();

    TCU_CHECK_INTERNAL(levels[0].getWidth() == levels[0].getHeight());

    if (!contextInfo.isCompressedTextureFormatSupported(m_format))
        throw tcu::NotSupportedError("Compressed texture format not supported", "", __FILE__, __LINE__);

    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

    try
    {
        loadCompressed(numLevels, levels, decompressionParams);
    }
    catch (const std::exception &)
    {
        gl.deleteTextures(1, &m_glTexture);
        throw;
    }
}

TextureCube::TextureCube(const RenderContext &context, uint32_t format, uint32_t dataType, int size)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), size, isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

TextureCube::TextureCube(const RenderContext &context, uint32_t internalFormat, int size)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(internalFormat)
    , m_refTexture(mapGLInternalFormat(internalFormat), size, isES2Context(context.getType()))
    , m_glTexture(0)
{
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

TextureCube::~TextureCube(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void TextureCube::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    DE_ASSERT(!m_isCompressed);

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
        {
            if (m_refTexture.isLevelEmpty((tcu::CubeFace)face, levelNdx))
                continue; // Don't upload.

            tcu::ConstPixelBufferAccess access = m_refTexture.getLevelFace(levelNdx, (tcu::CubeFace)face);
            DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
            gl.texImage2D(getGLCubeFace((tcu::CubeFace)face), levelNdx, m_format, access.getWidth(), access.getHeight(),
                          0 /* border */, transferFormat.format, transferFormat.dataType, access.getDataPtr());
        }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

void TextureCube::loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                                 const tcu::TexDecompressionParams &decompressionParams)
{
    const glw::Functions &gl  = m_context.getFunctions();
    uint32_t compressedFormat = getGLFormat(levels[0].getFormat());

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_glTexture);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            const tcu::CompressedTexture &level = levels[levelNdx * tcu::CUBEFACE_LAST + face];

            // Decompress to reference texture.
            m_refTexture.allocLevel((tcu::CubeFace)face, levelNdx);
            tcu::PixelBufferAccess refLevelAccess = m_refTexture.getLevelFace(levelNdx, (tcu::CubeFace)face);
            TCU_CHECK(level.getWidth() == refLevelAccess.getWidth() && level.getHeight() == refLevelAccess.getHeight());
            level.decompress(refLevelAccess, decompressionParams);

            // Upload to GL texture in compressed form.
            gl.compressedTexImage2D(getGLCubeFace((tcu::CubeFace)face), levelNdx, compressedFormat, level.getWidth(),
                                    level.getHeight(), 0 /* border */, level.getDataSize(), level.getData());
        }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

TextureCube *TextureCube::create(const RenderContext &context, const ContextInfo &contextInfo,
                                 const tcu::Archive &archive, int numLevels, const char *const *filenames)
{
    DE_ASSERT(numLevels > 0);

    std::string ext = de::FilePath(filenames[0]).getFileExtension();

    // \todo [2011-11-21 pyry] Support PNG images.
    if (ext == "pkm")
    {
        // Compressed texture.
        int numImages = numLevels * tcu::CUBEFACE_LAST;
        vector<tcu::CompressedTexture> levels(numImages);

        for (int ndx = 0; ndx < numImages; ndx++)
            tcu::ImageIO::loadPKM(levels[ndx], archive, filenames[ndx]);

        return new TextureCube(context, contextInfo, numLevels, &levels[0]);
    }
    else
        TCU_FAIL("Unsupported file format");
}

TextureCube *TextureCube::create(const RenderContext &context, const ContextInfo &contextInfo,
                                 const tcu::Archive &archive, int numLevels, const std::vector<std::string> &filenames)
{
    DE_STATIC_ASSERT(tcu::CUBEFACE_LAST == 6);
    TCU_CHECK(numLevels * tcu::CUBEFACE_LAST == (int)filenames.size());

    std::vector<const char *> charPtrs(filenames.size());
    for (int ndx = 0; ndx < (int)filenames.size(); ndx++)
        charPtrs[ndx] = filenames[ndx].c_str();

    return TextureCube::create(context, contextInfo, archive, numLevels, &charPtrs[0]);
}

// Texture1DArray

Texture1DArray::Texture1DArray(const RenderContext &context, uint32_t format, uint32_t dataType, int width,
                               int numLevels)
    : m_context(context)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), width, numLevels)
    , m_glTexture(0)
{
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture1DArray::Texture1DArray(const RenderContext &context, uint32_t sizedFormat, int width, int numLevels)
    : m_context(context)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), width, numLevels)
    , m_glTexture(0)
{
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture1DArray::~Texture1DArray(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void Texture1DArray::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_1D_ARRAY, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        gl.texImage2D(GL_TEXTURE_1D_ARRAY, levelNdx, m_format, access.getWidth(), access.getHeight(), 0 /* border */,
                      transferFormat.format, transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// Texture2DArray

Texture2DArray::Texture2DArray(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height,
                               int numLevels)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), width, height, numLevels)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture2DArray::Texture2DArray(const RenderContext &context, uint32_t sizedFormat, int width, int height, int numLevels)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), width, height, numLevels)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture2DArray::Texture2DArray(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                               const tcu::CompressedTexture *levels,
                               const tcu::TexDecompressionParams &decompressionParams)
    : m_context(context)
    , m_isCompressed(true)
    , m_format(getGLFormat(levels[0].getFormat()))
    , m_refTexture(getUncompressedFormat(levels[0].getFormat()), levels[0].getWidth(), levels[0].getHeight(),
                   levels[0].getDepth())
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();

    if (!contextInfo.isCompressedTextureFormatSupported(m_format))
        throw tcu::NotSupportedError("Compressed texture format not supported", "", __FILE__, __LINE__);

    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

    try
    {
        loadCompressed(numLevels, levels, decompressionParams);
    }
    catch (const std::exception &)
    {
        gl.deleteTextures(1, &m_glTexture);
        throw;
    }
}

Texture2DArray::~Texture2DArray(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void Texture2DArray::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    if (!gl.texImage3D)
        throw tcu::NotSupportedError("glTexImage3D() is not supported");

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        DE_ASSERT(access.getSlicePitch() == access.getFormat().getPixelSize() * access.getWidth() * access.getHeight());
        gl.texImage3D(GL_TEXTURE_2D_ARRAY, levelNdx, m_format, access.getWidth(), access.getHeight(), access.getDepth(),
                      0 /* border */, transferFormat.format, transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

void Texture2DArray::loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                                    const tcu::TexDecompressionParams &decompressionParams)
{
    const glw::Functions &gl  = m_context.getFunctions();
    uint32_t compressedFormat = getGLFormat(levels[0].getFormat());

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_glTexture);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        const tcu::CompressedTexture &level = levels[levelNdx];

        // Decompress to reference texture.
        m_refTexture.allocLevel(levelNdx);
        tcu::PixelBufferAccess refLevelAccess = m_refTexture.getLevel(levelNdx);
        TCU_CHECK(level.getWidth() == refLevelAccess.getWidth() && level.getHeight() == refLevelAccess.getHeight() &&
                  level.getDepth() == refLevelAccess.getDepth());
        level.decompress(refLevelAccess, decompressionParams);

        // Upload to GL texture in compressed form.
        gl.compressedTexImage3D(GL_TEXTURE_2D_ARRAY, levelNdx, compressedFormat, level.getWidth(), level.getHeight(),
                                m_refTexture.getLevel(levelNdx).getDepth(), 0 /* border */, level.getDataSize(),
                                level.getData());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// Texture3D

Texture3D::Texture3D(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height, int depth)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), width, height, depth)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture3D::Texture3D(const RenderContext &context, uint32_t sizedFormat, int width, int height, int depth)
    : m_context(context)
    , m_isCompressed(false)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), width, height, depth)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

Texture3D::Texture3D(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                     const tcu::CompressedTexture *levels, const tcu::TexDecompressionParams &decompressionParams)
    : m_context(context)
    , m_isCompressed(true)
    , m_format(getGLFormat(levels[0].getFormat()))
    , m_refTexture(getUncompressedFormat(levels[0].getFormat()), levels[0].getWidth(), levels[0].getHeight(),
                   levels[0].getDepth())
    , m_glTexture(0)
{
    const glw::Functions &gl = context.getFunctions();

    if (!contextInfo.isCompressedTextureFormatSupported(m_format))
        throw tcu::NotSupportedError("Compressed texture format not supported", "", __FILE__, __LINE__);

    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

    try
    {
        loadCompressed(numLevels, levels, decompressionParams);
    }
    catch (const std::exception &)
    {
        gl.deleteTextures(1, &m_glTexture);
        throw;
    }
}

Texture3D::~Texture3D(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void Texture3D::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    DE_ASSERT(!m_isCompressed);

    if (!gl.texImage3D)
        throw tcu::NotSupportedError("glTexImage3D() is not supported");

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_3D, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        DE_ASSERT(access.getSlicePitch() == access.getFormat().getPixelSize() * access.getWidth() * access.getHeight());
        gl.texImage3D(GL_TEXTURE_3D, levelNdx, m_format, access.getWidth(), access.getHeight(), access.getDepth(),
                      0 /* border */, transferFormat.format, transferFormat.dataType, access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

void Texture3D::loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                               const tcu::TexDecompressionParams &decompressionParams)
{
    const glw::Functions &gl  = m_context.getFunctions();
    uint32_t compressedFormat = getGLFormat(levels[0].getFormat());

    if (!gl.compressedTexImage3D)
        throw tcu::NotSupportedError("glCompressedTexImage3D() is not supported");

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_3D, m_glTexture);

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        const tcu::CompressedTexture &level = levels[levelNdx];

        // Decompress to reference texture.
        m_refTexture.allocLevel(levelNdx);
        tcu::PixelBufferAccess refLevelAccess = m_refTexture.getLevel(levelNdx);
        TCU_CHECK(level.getWidth() == refLevelAccess.getWidth() && level.getHeight() == refLevelAccess.getHeight() &&
                  level.getDepth() == refLevelAccess.getDepth());
        level.decompress(refLevelAccess, decompressionParams);

        // Upload to GL texture in compressed form.
        gl.compressedTexImage3D(GL_TEXTURE_3D, levelNdx, compressedFormat, level.getWidth(), level.getHeight(),
                                level.getDepth(), 0 /* border */, level.getDataSize(), level.getData());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// TextureCubeArray

TextureCubeArray::TextureCubeArray(const RenderContext &context, uint32_t format, uint32_t dataType, int size,
                                   int numLayers)
    : m_context(context)
    , m_format(format)
    , m_refTexture(mapGLTransferFormat(format, dataType), size, numLayers)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

TextureCubeArray::TextureCubeArray(const RenderContext &context, uint32_t sizedFormat, int size, int numLayers)
    : m_context(context)
    , m_format(sizedFormat)
    , m_refTexture(mapGLInternalFormat(sizedFormat), size, numLayers)
    , m_glTexture(0)
{
    // \todo [2013-04-08 pyry] Check support here.
    const glw::Functions &gl = m_context.getFunctions();
    gl.genTextures(1, &m_glTexture);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
}

TextureCubeArray::~TextureCubeArray(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);
}

void TextureCubeArray::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    if (!gl.texImage3D)
        throw tcu::NotSupportedError("glTexImage3D() is not supported");

    TCU_CHECK(m_glTexture);
    gl.bindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_glTexture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(m_refTexture.getFormat()));
    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");

    TransferFormat transferFormat = getTransferFormat(m_refTexture.getFormat());

    for (int levelNdx = 0; levelNdx < m_refTexture.getNumLevels(); levelNdx++)
    {
        if (m_refTexture.isLevelEmpty(levelNdx))
            continue; // Don't upload.

        tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(levelNdx);
        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * access.getWidth());
        DE_ASSERT(access.getSlicePitch() == access.getFormat().getPixelSize() * access.getWidth() * access.getHeight());
        gl.texImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levelNdx, m_format, access.getWidth(), access.getHeight(),
                      access.getDepth(), 0 /* border */, transferFormat.format, transferFormat.dataType,
                      access.getDataPtr());
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Texture upload failed");
}

// TextureBuffer

TextureBuffer::TextureBuffer(const RenderContext &context, uint32_t internalFormat, size_t bufferSize)
    : m_context(context)
    , m_format(0)
    , m_offset(0)
    , m_size(0)
    , m_glTexture(0)
    , m_glBuffer(0)
{
    init(internalFormat, bufferSize, 0, 0, nullptr);
}

TextureBuffer::TextureBuffer(const RenderContext &context, uint32_t internalFormat, size_t bufferSize, size_t offset,
                             size_t size, const void *data)
    : m_context(context)
    , m_format(0)
    , m_offset(0)
    , m_size(0)
    , m_glTexture(0)
    , m_glBuffer(0)
{
    init(internalFormat, bufferSize, offset, size, data);
}

void TextureBuffer::init(uint32_t internalFormat, size_t bufferSize, size_t offset, size_t size, const void *data)
{
    const glw::Functions &gl = m_context.getFunctions();
    de::UniquePtr<ContextInfo> info(ContextInfo::create(m_context));

    if (offset != 0 || size != 0)
    {
        if (!(contextSupports(m_context.getType(), glu::ApiType(3, 3, glu::PROFILE_CORE)) &&
              info->isExtensionSupported("GL_ARB_texture_buffer_range")) &&
            !(contextSupports(m_context.getType(), glu::ApiType(3, 1, glu::PROFILE_ES)) &&
              info->isExtensionSupported("GL_EXT_texture_buffer")))
        {
            throw tcu::NotSupportedError("Ranged texture buffers not supported", "", __FILE__, __LINE__);
        }
    }
    else
    {
        if (!contextSupports(m_context.getType(), glu::ApiType(3, 3, glu::PROFILE_CORE)) &&
            !(contextSupports(m_context.getType(), glu::ApiType(3, 1, glu::PROFILE_ES)) &&
              info->isExtensionSupported("GL_EXT_texture_buffer")))
        {
            throw tcu::NotSupportedError("Texture buffers not supported", "", __FILE__, __LINE__);
        }
    }

    m_refBuffer.setStorage(bufferSize);
    if (data)
        deMemcpy(m_refBuffer.getPtr(), data, (int)bufferSize);

    m_format = internalFormat;
    m_offset = offset;
    m_size   = size;

    DE_ASSERT(size != 0 || offset == 0);

    {
        gl.genTextures(1, &m_glTexture);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

        gl.genBuffers(1, &m_glBuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers() failed");

        gl.bindBuffer(GL_TEXTURE_BUFFER, m_glBuffer);
        gl.bufferData(GL_TEXTURE_BUFFER, (glw::GLsizei)m_refBuffer.size(), data, GL_STATIC_DRAW);
        gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to create buffer");

        gl.bindTexture(GL_TEXTURE_BUFFER, m_glTexture);

        if (offset != 0 || size != 0)
            gl.texBufferRange(GL_TEXTURE_BUFFER, m_format, m_glBuffer, (glw::GLintptr)m_offset,
                              (glw::GLsizeiptr)m_size);
        else
            gl.texBuffer(GL_TEXTURE_BUFFER, m_format, m_glBuffer);

        gl.bindTexture(GL_TEXTURE_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to bind buffer to texture");
    }
}

TextureBuffer::~TextureBuffer(void)
{
    if (m_glTexture)
        m_context.getFunctions().deleteTextures(1, &m_glTexture);

    if (m_glBuffer)
        m_context.getFunctions().deleteBuffers(1, &m_glBuffer);
}

const tcu::PixelBufferAccess TextureBuffer::getFullRefTexture(void)
{
    const tcu::TextureFormat format = mapGLInternalFormat(m_format);
    const size_t bufferLengthBytes  = (m_size != 0) ? (m_size) : (m_refBuffer.size());
    const int bufferLengthPixels    = (int)bufferLengthBytes / format.getPixelSize();

    return tcu::PixelBufferAccess(format, tcu::IVec3(bufferLengthPixels, 1, 1),
                                  (uint8_t *)m_refBuffer.getPtr() + m_offset);
}

const tcu::ConstPixelBufferAccess TextureBuffer::getFullRefTexture(void) const
{
    return const_cast<TextureBuffer *>(this)->getFullRefTexture();
}

void TextureBuffer::upload(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    gl.bindBuffer(GL_TEXTURE_BUFFER, m_glBuffer);
    gl.bufferData(GL_TEXTURE_BUFFER, (glw::GLsizei)m_refBuffer.size(), m_refBuffer.getPtr(), GL_STATIC_DRAW);
    gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to upload buffer");
}

} // namespace glu
