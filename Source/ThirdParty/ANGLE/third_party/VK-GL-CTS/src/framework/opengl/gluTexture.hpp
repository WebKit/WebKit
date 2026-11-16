#ifndef _GLUTEXTURE_HPP
#define _GLUTEXTURE_HPP
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

#include "gluDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuResource.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "deArrayBuffer.hpp"

#include <vector>
#include <string>

namespace glu
{

/*--------------------------------------------------------------------*//*!
 * \brief 1D Texture only supported on OpenGL
 *//*--------------------------------------------------------------------*/
class Texture1D
{
public:
    Texture1D(const RenderContext &context, uint32_t format, uint32_t dataType, int width);
    Texture1D(const RenderContext &context, uint32_t internalFormat, int width);
    ~Texture1D(void);

    tcu::Texture1D &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::Texture1D &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

    void upload(void);

private:
    Texture1D(const Texture1D &other);            // Not allowed!
    Texture1D &operator=(const Texture1D &other); // Not allowed!

    const RenderContext &m_context;
    uint32_t m_format; //!< Internal format.
    tcu::Texture1D m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 2D Texture
 *//*--------------------------------------------------------------------*/
class Texture2D
{
public:
    Texture2D(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
              const tcu::CompressedTexture *levels,
              const tcu::TexDecompressionParams &decompressionParams = tcu::TexDecompressionParams());
    Texture2D(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height);
    Texture2D(const RenderContext &context, uint32_t internalFormat, int width, int height);
    virtual ~Texture2D(void);

    virtual void upload(void); // Not supported on compressed textures.

    tcu::Texture2D &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::Texture2D &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

    static Texture2D *create(const RenderContext &context, const ContextInfo &contextInfo, const tcu::Archive &archive,
                             int numLevels, const std::vector<std::string> &filenames);
    static Texture2D *create(const RenderContext &context, const ContextInfo &contextInfo, const tcu::Archive &archive,
                             int numLevels, const char *const *filenames);
    static Texture2D *create(const RenderContext &context, const ContextInfo &contextInfo, const tcu::Archive &archive,
                             const char *filename)
    {
        return create(context, contextInfo, archive, 1, &filename);
    }

protected:
    const RenderContext &m_context;

    bool m_isCompressed;
    uint32_t m_format; //!< Internal format.
    tcu::Texture2D m_refTexture;

    uint32_t m_glTexture;

private:
    Texture2D(const Texture2D &other);            // Not allowed!
    Texture2D &operator=(const Texture2D &other); // Not allowed!

    void loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                        const tcu::TexDecompressionParams &decompressionParams);
} DE_WARN_UNUSED_TYPE;

class ImmutableTexture2D : public Texture2D
{
public:
    ImmutableTexture2D(const RenderContext &context, uint32_t internalFormat, int width, int height);

    void upload(void); // Not supported on compressed textures.

private:
    ImmutableTexture2D(const ImmutableTexture2D &other);            // Not allowed!
    ImmutableTexture2D &operator=(const ImmutableTexture2D &other); // Not allowed!
};

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Texture
 *//*--------------------------------------------------------------------*/
class TextureCube
{
public:
    // For compressed cubemap constructor and create() function input level pointers / filenames are expected
    // to laid out to array in following order:
    //   { l0_neg_x, l0_pos_x, l0_neg_y, l0_pos_y, l0_neg_z, l0_pos_z, l1_neg_x, l1_pos_x, ... }

    TextureCube(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                const tcu::CompressedTexture *levels,
                const tcu::TexDecompressionParams &decompressionParams = tcu::TexDecompressionParams());
    TextureCube(const RenderContext &context, uint32_t format, uint32_t dataType, int size);
    TextureCube(const RenderContext &context, uint32_t internalFormat, int size);
    ~TextureCube(void);

    void upload(void); // Not supported on compressed textures.

    tcu::TextureCube &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::TextureCube &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

    static TextureCube *create(const RenderContext &context, const ContextInfo &contextInfo,
                               const tcu::Archive &archive, int numLevels, const std::vector<std::string> &filenames);
    static TextureCube *create(const RenderContext &context, const ContextInfo &contextInfo,
                               const tcu::Archive &archive, int numLevels, const char *const *filenames);

private:
    TextureCube(const TextureCube &other);            // Not allowed!
    TextureCube &operator=(const TextureCube &other); // Not allowed!

    void loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                        const tcu::TexDecompressionParams &decompressionParams);

    const RenderContext &m_context;

    bool m_isCompressed;
    uint32_t m_format; //!< Internal format.

    tcu::TextureCube m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 2D Array Texture
 * \note Not supported on OpenGL ES 2
 *//*--------------------------------------------------------------------*/
class Texture2DArray
{
public:
    Texture2DArray(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height,
                   int numLayers);
    Texture2DArray(const RenderContext &context, uint32_t internalFormat, int width, int height, int numLayers);
    Texture2DArray(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
                   const tcu::CompressedTexture *levels,
                   const tcu::TexDecompressionParams &decompressionParams = tcu::TexDecompressionParams());
    ~Texture2DArray(void);

    void upload(void);

    tcu::Texture2DArray &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::Texture2DArray &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

private:
    Texture2DArray(const Texture2DArray &other);            // Not allowed!
    Texture2DArray &operator=(const Texture2DArray &other); // Not allowed!

    void loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                        const tcu::TexDecompressionParams &decompressionParams);

    const RenderContext &m_context;

    bool m_isCompressed;
    uint32_t m_format; //!< Internal format.

    tcu::Texture2DArray m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 1D Array Texture
 * \note Only supported on OpenGL
 *//*--------------------------------------------------------------------*/
class Texture1DArray
{
public:
    Texture1DArray(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int numLayers);
    Texture1DArray(const RenderContext &context, uint32_t internalFormat, int width, int numLayers);
    ~Texture1DArray(void);

    void upload(void);

    tcu::Texture1DArray &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::Texture1DArray &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

private:
    Texture1DArray(const Texture1DArray &other);            // Not allowed!
    Texture1DArray &operator=(const Texture1DArray &other); // Not allowed!

    const RenderContext &m_context;

    uint32_t m_format; //!< Internal format.

    tcu::Texture1DArray m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 3D Texture
 * \note Not supported on OpenGL ES 2
 *//*--------------------------------------------------------------------*/
class Texture3D
{
public:
    Texture3D(const RenderContext &context, uint32_t format, uint32_t dataType, int width, int height, int depth);
    Texture3D(const RenderContext &context, uint32_t internalFormat, int width, int height, int depth);
    Texture3D(const RenderContext &context, const ContextInfo &contextInfo, int numLevels,
              const tcu::CompressedTexture *levels,
              const tcu::TexDecompressionParams &decompressionParams = tcu::TexDecompressionParams());
    ~Texture3D(void);

    void upload(void);

    tcu::Texture3D &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::Texture3D &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

private:
    Texture3D(const Texture3D &other);            // Not allowed!
    Texture3D &operator=(const Texture3D &other); // Not allowed!

    void loadCompressed(int numLevels, const tcu::CompressedTexture *levels,
                        const tcu::TexDecompressionParams &decompressionParams);

    const RenderContext &m_context;

    bool m_isCompressed;
    uint32_t m_format; //!< Internal format.

    tcu::Texture3D m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Array Texture
 * \note Not supported on OpenGL ES 3.0 or lower
 *//*--------------------------------------------------------------------*/
class TextureCubeArray
{
public:
    TextureCubeArray(const RenderContext &context, uint32_t format, uint32_t dataType, int size, int numLayers);
    TextureCubeArray(const RenderContext &context, uint32_t internalFormat, int size, int numLayers);

    ~TextureCubeArray(void);

    void upload(void);

    tcu::TextureCubeArray &getRefTexture(void)
    {
        return m_refTexture;
    }
    const tcu::TextureCubeArray &getRefTexture(void) const
    {
        return m_refTexture;
    }
    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }

private:
    TextureCubeArray(const TextureCubeArray &other);            // Not allowed!
    TextureCubeArray &operator=(const TextureCubeArray &other); // Not allowed!

    const RenderContext &m_context;

    bool m_isCompressed;
    uint32_t m_format; //!< Internal format.

    tcu::TextureCubeArray m_refTexture;
    uint32_t m_glTexture;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 1D Texture Buffer only supported on OpenGL
 *//*--------------------------------------------------------------------*/
class TextureBuffer
{
public:
    TextureBuffer(const RenderContext &context, uint32_t internalFormat, size_t bufferSize);
    TextureBuffer(const RenderContext &context, uint32_t internalFormat, size_t bufferSize, size_t offset, size_t size,
                  const void *data = nullptr);

    ~TextureBuffer(void);

    // \note Effective pixel buffer access must be limited to w <= GL_MAX_TEXTURE_BUFFER_SIZE
    const tcu::PixelBufferAccess getFullRefTexture(void);
    const tcu::ConstPixelBufferAccess getFullRefTexture(void) const;

    // \note mutating buffer storage will invalidate existing pixel buffer accesses
    de::ArrayBuffer<uint8_t> &getRefBuffer(void)
    {
        return m_refBuffer;
    }
    const de::ArrayBuffer<uint8_t> &getRefBuffer(void) const
    {
        return m_refBuffer;
    }

    size_t getSize(void) const
    {
        return m_size;
    }
    size_t getOffset(void) const
    {
        return m_offset;
    }
    size_t getBufferSize(void) const
    {
        return m_refBuffer.size();
    }

    uint32_t getGLTexture(void) const
    {
        return m_glTexture;
    }
    uint32_t getGLBuffer(void) const
    {
        return m_glBuffer;
    }

    void upload(void);

private:
    void init(uint32_t internalFormat, size_t bufferSize, size_t offset, size_t size, const void *data);
    TextureBuffer(const TextureBuffer &other);            // Not allowed!
    TextureBuffer &operator=(const TextureBuffer &other); // Not allowed!

    const RenderContext &m_context;
    uint32_t m_format; //!< Internal format.
    de::ArrayBuffer<uint8_t> m_refBuffer;
    size_t m_offset;
    size_t m_size;

    uint32_t m_glTexture;
    uint32_t m_glBuffer;
} DE_WARN_UNUSED_TYPE;

} // namespace glu

#endif // _GLUTEXTURE_HPP
