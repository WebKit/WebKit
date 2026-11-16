#ifndef _SGLRREFERENCECONTEXT_HPP
#define _SGLRREFERENCECONTEXT_HPP
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
 * \brief Reference Rendering Context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "sglrContext.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuSurface.hpp"
#include "tcuTexture.hpp"
#include "tcuVector.hpp"
#include "rrFragmentOperations.hpp"
#include "rrRenderState.hpp"
#include "rrRenderer.hpp"
#include "rrMultisamplePixelBufferAccess.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderUtil.hpp"
#include "deArrayBuffer.hpp"

#include <map>
#include <vector>

namespace sglr
{
namespace rc
{

enum
{
    MAX_TEXTURE_SIZE_LOG2 = 14,
    MAX_TEXTURE_SIZE      = 1 << MAX_TEXTURE_SIZE_LOG2
};

class NamedObject
{
public:
    virtual ~NamedObject(void)
    {
    }

    uint32_t getName(void) const
    {
        return m_name;
    }

    int getRefCount(void) const
    {
        return m_refCount;
    }
    void incRefCount(void)
    {
        m_refCount += 1;
    }
    void decRefCount(void)
    {
        DE_ASSERT(m_refCount > 0);
        m_refCount -= 1;
    }

protected:
    NamedObject(uint32_t name) : m_name(name), m_refCount(1)
    {
    }

private:
    uint32_t m_name;
    int m_refCount;
};

class Texture : public NamedObject
{
public:
    enum Type
    {
        TYPE_1D,
        TYPE_2D,
        TYPE_CUBE_MAP,
        TYPE_2D_ARRAY,
        TYPE_3D,
        TYPE_CUBE_MAP_ARRAY,

        TYPE_LAST
    };

    Texture(uint32_t name, Type type, bool seamless = true);
    virtual ~Texture(void)
    {
    }

    Type getType(void) const
    {
        return m_type;
    }

    int getBaseLevel(void) const
    {
        return m_baseLevel;
    }
    int getMaxLevel(void) const
    {
        return m_maxLevel;
    }
    bool isImmutable(void) const
    {
        return m_immutable;
    }

    void setBaseLevel(int baseLevel)
    {
        m_baseLevel = baseLevel;
    }
    void setMaxLevel(int maxLevel)
    {
        m_maxLevel = maxLevel;
    }
    void setImmutable(void)
    {
        m_immutable = true;
    }

    const tcu::Sampler &getSampler(void) const
    {
        return m_sampler;
    }
    tcu::Sampler &getSampler(void)
    {
        return m_sampler;
    }

private:
    Type m_type;

    bool m_immutable;

    tcu::Sampler m_sampler;
    int m_baseLevel;
    int m_maxLevel;
};

//! Class for managing list of texture levels.
class TextureLevelArray
{
public:
    TextureLevelArray(void);
    ~TextureLevelArray(void);

    bool hasLevel(int level) const
    {
        return deInBounds32(level, 0, DE_LENGTH_OF_ARRAY(m_data)) && !m_data[level].empty();
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        DE_ASSERT(hasLevel(level));
        return m_access[level];
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        DE_ASSERT(hasLevel(level));
        return m_access[level];
    }

    const tcu::ConstPixelBufferAccess *getLevels(void) const
    {
        return &m_access[0];
    }
    const tcu::ConstPixelBufferAccess *getEffectiveLevels(void) const
    {
        return &m_effectiveAccess[0];
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int depth);
    void clearLevel(int level);

    void clear(void);

    void updateSamplerMode(tcu::Sampler::DepthStencilMode);

private:
    de::ArrayBuffer<uint8_t> m_data[MAX_TEXTURE_SIZE_LOG2];
    tcu::PixelBufferAccess m_access[MAX_TEXTURE_SIZE_LOG2];
    tcu::ConstPixelBufferAccess m_effectiveAccess
        [MAX_TEXTURE_SIZE_LOG2]; //!< the currently effective sampling mode. For Depth-stencil texture always either Depth or stencil.
};

class Texture1D : public Texture
{
public:
    Texture1D(uint32_t name = 0);
    virtual ~Texture1D(void);

    void clearLevels(void)
    {
        m_levels.clear();
    }

    bool hasLevel(int level) const
    {
        return m_levels.hasLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        return m_levels.getLevel(level);
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        return m_levels.getLevel(level);
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width);

    bool isComplete(void) const;

    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float lod) const;
    void sample4(tcu::Vec4 output[4], const float packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels;
    tcu::Texture2DView m_view;
};

class Texture2D : public Texture
{
public:
    Texture2D(uint32_t name = 0, bool es2 = false);
    virtual ~Texture2D(void);

    void clearLevels(void)
    {
        m_levels.clear();
    }

    bool hasLevel(int level) const
    {
        return m_levels.hasLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        return m_levels.getLevel(level);
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        return m_levels.getLevel(level);
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width, int height);

    bool isComplete(void) const;

    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float t, float lod) const;
    void sample4(tcu::Vec4 output[4], const tcu::Vec2 packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels;
    tcu::Texture2DView m_view;
};

class TextureCube : public Texture
{
public:
    TextureCube(uint32_t name = 0, bool seamless = true);
    virtual ~TextureCube(void);

    void clearLevels(void);

    bool hasFace(int level, tcu::CubeFace face) const
    {
        return m_levels[face].hasLevel(level);
    }
    const tcu::PixelBufferAccess &getFace(int level, tcu::CubeFace face)
    {
        return m_levels[face].getLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getFace(int level, tcu::CubeFace face) const
    {
        return m_levels[face].getLevel(level);
    }

    void allocFace(int level, tcu::CubeFace face, const tcu::TextureFormat &format, int width, int height);

    bool isComplete(void) const;
    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float t, float p, float lod) const;
    void sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels[tcu::CUBEFACE_LAST];
    tcu::TextureCubeView m_view;
};

class Texture2DArray : public Texture
{
public:
    Texture2DArray(uint32_t name = 0);
    virtual ~Texture2DArray(void);

    void clearLevels(void)
    {
        m_levels.clear();
    }

    bool hasLevel(int level) const
    {
        return m_levels.hasLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        return m_levels.getLevel(level);
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        return m_levels.getLevel(level);
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int numLayers);

    bool isComplete(void) const;

    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float t, float r, float lod) const;
    void sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels;
    tcu::Texture2DArrayView m_view;
};

class Texture3D : public Texture
{
public:
    Texture3D(uint32_t name = 0);
    virtual ~Texture3D(void);

    void clearLevels(void)
    {
        m_levels.clear();
    }

    bool hasLevel(int level) const
    {
        return m_levels.hasLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        return m_levels.getLevel(level);
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        return m_levels.getLevel(level);
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int numLayers);

    bool isComplete(void) const;

    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float t, float r, float lod) const;
    void sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels;
    tcu::Texture3DView m_view;
};

class TextureCubeArray : public Texture
{
public:
    TextureCubeArray(uint32_t name = 0);
    virtual ~TextureCubeArray(void);

    void clearLevels(void)
    {
        m_levels.clear();
    }

    bool hasLevel(int level) const
    {
        return m_levels.hasLevel(level);
    }
    const tcu::ConstPixelBufferAccess &getLevel(int level) const
    {
        return m_levels.getLevel(level);
    }
    const tcu::PixelBufferAccess &getLevel(int level)
    {
        return m_levels.getLevel(level);
    }

    void allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int numLayers);

    bool isComplete(void) const;

    void updateView(
        tcu::Sampler::DepthStencilMode
            mode); // \note View must be refreshed after texture parameter/size changes, before calling sample*()

    tcu::Vec4 sample(float s, float t, float r, float q, float lod) const;
    void sample4(tcu::Vec4 output[4], const tcu::Vec4 packetTexcoords[4], float lodBias = 0.0f) const;

private:
    TextureLevelArray m_levels;
    tcu::TextureCubeArrayView m_view;
};

class Renderbuffer : public NamedObject
{
public:
    enum Format
    {
        FORMAT_DEPTH_COMPONENT16,
        FORMAT_RGBA4,
        FORMAT_RGB5_A1,
        FORMAT_RGB565,
        FORMAT_STENCIL_INDEX8,

        FORMAT_LAST
    };

    Renderbuffer(uint32_t name);
    virtual ~Renderbuffer(void);

    void setStorage(const tcu::TextureFormat &format, int width, int height);

    int getWidth(void) const
    {
        return m_data.getWidth();
    }
    int getHeight(void) const
    {
        return m_data.getHeight();
    }
    tcu::TextureFormat getFormat(void) const
    {
        return m_data.getFormat();
    }

    tcu::PixelBufferAccess getAccess(void)
    {
        return m_data.getAccess();
    }
    tcu::ConstPixelBufferAccess getAccess(void) const
    {
        return m_data.getAccess();
    }

private:
    tcu::TextureLevel m_data;
};

class Framebuffer : public NamedObject
{
public:
    enum AttachmentPoint
    {
        ATTACHMENTPOINT_COLOR0,
        ATTACHMENTPOINT_DEPTH,
        ATTACHMENTPOINT_STENCIL,

        ATTACHMENTPOINT_LAST
    };

    enum AttachmentType
    {
        ATTACHMENTTYPE_RENDERBUFFER,
        ATTACHMENTTYPE_TEXTURE,

        ATTACHMENTTYPE_LAST
    };

    enum TexTarget
    {
        TEXTARGET_2D,
        TEXTARGET_CUBE_MAP_POSITIVE_X,
        TEXTARGET_CUBE_MAP_POSITIVE_Y,
        TEXTARGET_CUBE_MAP_POSITIVE_Z,
        TEXTARGET_CUBE_MAP_NEGATIVE_X,
        TEXTARGET_CUBE_MAP_NEGATIVE_Y,
        TEXTARGET_CUBE_MAP_NEGATIVE_Z,
        TEXTARGET_2D_ARRAY,
        TEXTARGET_3D,
        TEXTARGET_CUBE_MAP_ARRAY,

        TEXTARGET_LAST
    };

    struct Attachment
    {
        AttachmentType type;
        uint32_t name;
        TexTarget texTarget;
        int level;
        int layer;

        Attachment(void) : type(ATTACHMENTTYPE_LAST), name(0), texTarget(TEXTARGET_LAST), level(0), layer(0)
        {
        }
    };

    Framebuffer(uint32_t name);
    virtual ~Framebuffer(void);

    Attachment &getAttachment(AttachmentPoint point)
    {
        return m_attachments[point];
    }
    const Attachment &getAttachment(AttachmentPoint point) const
    {
        return m_attachments[point];
    }

private:
    Attachment m_attachments[ATTACHMENTPOINT_LAST];
};

class DataBuffer : public NamedObject
{
public:
    DataBuffer(uint32_t name) : NamedObject(name)
    {
    }
    ~DataBuffer(void)
    {
    }

    void setStorage(int size)
    {
        m_data.resize(size);
    }

    int getSize(void) const
    {
        return (int)m_data.size();
    }
    const uint8_t *getData(void) const
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }
    uint8_t *getData(void)
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }

private:
    std::vector<uint8_t> m_data;
};

class VertexArray : public NamedObject
{
public:
    struct VertexAttribArray
    {
        bool enabled;
        int size;
        int stride;
        uint32_t type;

        bool normalized;
        bool integer;
        int divisor;

        /**
          ! These three variables define the state. bufferDeleted is needed to distinguish
          ! drawing from user pointer and offset to a deleted buffer from each other.
          !
          ! Only these three combinations are possible:
          ! 1) bufferDeleted = false, bufferBinding = NULL, pointer = user_ptr.   < render from a user ptr
          ! 2) bufferDeleted = false, bufferBinding = ptr,  pointer = offset.     < render from a buffer with offset
          ! 3) bufferDeleted = true,  bufferBinding = NULL, pointer = offset      < render from a deleted buffer. Don't do anything
          !
          ! (bufferFreed = true) implies (bufferBinding = NULL)
         */
        bool bufferDeleted;
        rc::DataBuffer *bufferBinding;
        const void *pointer;
    };

    VertexArray(uint32_t name, int maxVertexAttribs);
    ~VertexArray(void)
    {
    }

    rc::DataBuffer *m_elementArrayBufferBinding;
    std::vector<VertexAttribArray> m_arrays;
};

class ShaderProgramObjectContainer : public NamedObject
{
public:
    ShaderProgramObjectContainer(uint32_t name, ShaderProgram *program);
    ~ShaderProgramObjectContainer(void);

    ShaderProgram *m_program;
    bool m_deleteFlag;
};

template <typename T>
class ObjectManager
{
public:
    ObjectManager(void);
    ~ObjectManager(void);

    uint32_t allocateName(void);
    void insert(T *object);
    T *find(uint32_t name);

    void acquireReference(T *object);
    void releaseReference(T *object);

    int getCount(void) const
    {
        return (int)m_objects.size();
    }
    void getAll(typename std::vector<T *> &objects) const;

private:
    ObjectManager(const ObjectManager<T> &other);
    ObjectManager &operator=(const ObjectManager<T> &other);

    uint32_t m_lastName;
    std::map<uint32_t, T *> m_objects;
};

template <typename T>
ObjectManager<T>::ObjectManager(void) : m_lastName(0)
{
}

template <typename T>
ObjectManager<T>::~ObjectManager(void)
{
    DE_ASSERT(m_objects.size() == 0);
}

template <typename T>
uint32_t ObjectManager<T>::allocateName(void)
{
    TCU_CHECK(m_lastName != 0xffffffff);
    return ++m_lastName;
}

template <typename T>
void ObjectManager<T>::insert(T *object)
{
    uint32_t name = object->getName();
    DE_ASSERT(object->getName() != 0);

    if (name > m_lastName)
        m_lastName = name;

    m_objects.insert(std::pair<uint32_t, T *>(name, object));
}

template <typename T>
T *ObjectManager<T>::find(uint32_t name)
{
    typename std::map<uint32_t, T *>::iterator it = m_objects.find(name);
    if (it != m_objects.end())
        return it->second;
    else
        return nullptr;
}

template <typename T>
void ObjectManager<T>::acquireReference(T *object)
{
    DE_ASSERT(find(object->getName()) == object);
    object->incRefCount();
}

template <typename T>
void ObjectManager<T>::releaseReference(T *object)
{
    DE_ASSERT(find(object->getName()) == object);
    object->decRefCount();

    if (object->getRefCount() == 0)
    {
        m_objects.erase(object->getName());
        delete object;
    }
}

template <typename T>
void ObjectManager<T>::getAll(typename std::vector<T *> &objects) const
{
    objects.resize(m_objects.size());
    typename std::vector<T *>::iterator dst = objects.begin();

    for (typename std::map<uint32_t, T *>::const_iterator i = m_objects.begin(); i != m_objects.end(); i++)
    {
        *dst++ = i->second;
    }
}

} // namespace rc

struct ReferenceContextLimits
{
    ReferenceContextLimits(void)
        : contextType(glu::ApiType::es(3, 0))
        , maxTextureImageUnits(16)
        , maxTexture2DSize(2048)
        , maxTextureCubeSize(2048)
        , maxTexture2DArrayLayers(256)
        , maxTexture3DSize(256)
        , maxRenderbufferSize(2048)
        , maxVertexAttribs(16)
        , subpixelBits(rr::RenderState::DEFAULT_SUBPIXEL_BITS)
    {
    }

    ReferenceContextLimits(const glu::RenderContext &renderCtx);

    void addExtension(const char *extension);

    glu::ContextType contextType;

    int maxTextureImageUnits;
    int maxTexture2DSize;
    int maxTextureCubeSize;
    int maxTexture2DArrayLayers;
    int maxTexture3DSize;
    int maxRenderbufferSize;
    int maxVertexAttribs;
    int subpixelBits;

    // Both variants are needed since there are glGetString() and glGetStringi()
    std::vector<std::string> extensionList;
    std::string extensionStr;
};

class ReferenceContextBuffers
{
public:
    ReferenceContextBuffers(const tcu::PixelFormat &colorBits, int depthBits, int stencilBits, int width, int height,
                            int samples = 1);

    rr::MultisamplePixelBufferAccess getColorbuffer(void)
    {
        return rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_colorbuffer.getAccess());
    }
    rr::MultisamplePixelBufferAccess getDepthbuffer(void)
    {
        return rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_depthbuffer.getAccess());
    }
    rr::MultisamplePixelBufferAccess getStencilbuffer(void)
    {
        return rr::MultisamplePixelBufferAccess::fromMultisampleAccess(m_stencilbuffer.getAccess());
    }

private:
    tcu::TextureLevel m_colorbuffer;
    tcu::TextureLevel m_depthbuffer;
    tcu::TextureLevel m_stencilbuffer;
};

class ReferenceContext : public Context
{
public:
    ReferenceContext(const ReferenceContextLimits &limits, const rr::MultisamplePixelBufferAccess &colorbuffer,
                     const rr::MultisamplePixelBufferAccess &depthbuffer,
                     const rr::MultisamplePixelBufferAccess &stencilbuffer);
    virtual ~ReferenceContext(void);

    virtual int getWidth(void) const
    {
        return m_defaultColorbuffer.raw().getHeight();
    }
    virtual int getHeight(void) const
    {
        return m_defaultColorbuffer.raw().getDepth();
    }

    virtual void viewport(int x, int y, int width, int height)
    {
        m_viewport = tcu::IVec4(x, y, width, height);
    }
    virtual void activeTexture(uint32_t texture);

    virtual void bindTexture(uint32_t target, uint32_t texture);
    virtual void genTextures(int numTextures, uint32_t *textures);
    virtual void deleteTextures(int numTextures, const uint32_t *textures);

    virtual void bindFramebuffer(uint32_t target, uint32_t framebuffer);
    virtual void genFramebuffers(int numFramebuffers, uint32_t *framebuffers);
    virtual void deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers);

    virtual void bindRenderbuffer(uint32_t target, uint32_t renderbuffer);
    virtual void genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers);
    virtual void deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers);

    virtual void pixelStorei(uint32_t pname, int param);
    virtual void texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border, uint32_t format,
                            uint32_t type, const void *data);
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int border,
                            uint32_t format, uint32_t type, const void *data);
    virtual void texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                            int border, uint32_t format, uint32_t type, const void *data);
    virtual void texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                               const void *data);
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                               uint32_t format, uint32_t type, const void *data);
    virtual void texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                               int depth, uint32_t format, uint32_t type, const void *data);
    virtual void copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int border);
    virtual void copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int height, int border);
    virtual void copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width);
    virtual void copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                   int height);
    virtual void copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y,
                                   int width, int height);

    virtual void texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height);
    virtual void texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height, int depth);

    virtual void texParameteri(uint32_t target, uint32_t pname, int value);

    virtual void framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                      int level);
    virtual void framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level, int layer);
    virtual void framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                         uint32_t renderbuffer);
    virtual uint32_t checkFramebufferStatus(uint32_t target);

    virtual void getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname, int *params);

    virtual void renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height);
    virtual void renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                                int height);

    virtual void bindBuffer(uint32_t target, uint32_t buffer);
    virtual void genBuffers(int numBuffers, uint32_t *buffers);
    virtual void deleteBuffers(int numBuffers, const uint32_t *buffers);

    virtual void bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage);
    virtual void bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data);

    virtual void clearColor(float red, float green, float blue, float alpha);
    virtual void clearDepthf(float depth);
    virtual void clearStencil(int stencil);

    virtual void clear(uint32_t buffers);
    virtual void clearBufferiv(uint32_t buffer, int drawbuffer, const int *value);
    virtual void clearBufferfv(uint32_t buffer, int drawbuffer, const float *value);
    virtual void clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value);
    virtual void clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil);
    virtual void scissor(int x, int y, int width, int height);

    virtual void enable(uint32_t cap);
    virtual void disable(uint32_t cap);

    virtual void stencilFunc(uint32_t func, int ref, uint32_t mask);
    virtual void stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass);
    virtual void stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask);
    virtual void stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass);

    virtual void depthFunc(uint32_t func);
    virtual void depthRangef(float n, float f);
    virtual void depthRange(double n, double f);

    virtual void polygonOffset(float factor, float units);
    virtual void provokingVertex(uint32_t convention);
    virtual void primitiveRestartIndex(uint32_t index);

    virtual void blendEquation(uint32_t mode);
    virtual void blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha);
    virtual void blendFunc(uint32_t src, uint32_t dst);
    virtual void blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha);
    virtual void blendColor(float red, float green, float blue, float alpha);

    virtual void colorMask(bool r, bool g, bool b, bool a);
    virtual void depthMask(bool mask);
    virtual void stencilMask(uint32_t mask);
    virtual void stencilMaskSeparate(uint32_t face, uint32_t mask);

    virtual void blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                                 uint32_t mask, uint32_t filter);

    virtual void invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                          int y, int width, int height);
    virtual void invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments);

    virtual void bindVertexArray(uint32_t array);
    virtual void genVertexArrays(int numArrays, uint32_t *vertexArrays);
    virtual void deleteVertexArrays(int numArrays, const uint32_t *vertexArrays);

    virtual void vertexAttribPointer(uint32_t index, int size, uint32_t type, bool normalized, int stride,
                                     const void *pointer);
    virtual void vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer);
    virtual void enableVertexAttribArray(uint32_t index);
    virtual void disableVertexAttribArray(uint32_t index);
    virtual void vertexAttribDivisor(uint32_t index, uint32_t divisor);

    virtual void vertexAttrib1f(uint32_t index, float);
    virtual void vertexAttrib2f(uint32_t index, float, float);
    virtual void vertexAttrib3f(uint32_t index, float, float, float);
    virtual void vertexAttrib4f(uint32_t index, float, float, float, float);
    virtual void vertexAttribI4i(uint32_t index, int32_t, int32_t, int32_t, int32_t);
    virtual void vertexAttribI4ui(uint32_t index, uint32_t, uint32_t, uint32_t, uint32_t);

    virtual int32_t getAttribLocation(uint32_t program, const char *name);

    virtual void uniform1f(int32_t location, float);
    virtual void uniform1i(int32_t location, int32_t);
    virtual void uniform1fv(int32_t index, int32_t count, const float *);
    virtual void uniform2fv(int32_t index, int32_t count, const float *);
    virtual void uniform3fv(int32_t index, int32_t count, const float *);
    virtual void uniform4fv(int32_t index, int32_t count, const float *);
    virtual void uniform1iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform2iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform3iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform4iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value);
    virtual void uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value);
    virtual int32_t getUniformLocation(uint32_t program, const char *name);

    virtual void lineWidth(float);

    virtual void drawArrays(uint32_t mode, int first, int count);
    virtual void drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount);
    virtual void drawElements(uint32_t mode, int count, uint32_t type, const void *indices);
    virtual void drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices, int baseVertex);
    virtual void drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices, int instanceCount);
    virtual void drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                 int instanceCount, int baseVertex);
    virtual void drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                   const void *indices);
    virtual void drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                             const void *indices, int baseVertex);
    virtual void drawArraysIndirect(uint32_t mode, const void *indirect);
    virtual void drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect);

    virtual void multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount);
    virtual void multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices, int primCount);
    virtual void multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                             int primCount, const int *baseVertex);

    virtual uint32_t createProgram(ShaderProgram *program);
    virtual void useProgram(uint32_t program);
    virtual void deleteProgram(uint32_t program);

    virtual void readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data);
    virtual uint32_t getError(void);
    virtual void finish(void);

    virtual void getIntegerv(uint32_t pname, int *params);
    virtual const char *getString(uint32_t pname);

    // Expose helpers from Context.
    using Context::readPixels;
    using Context::texImage2D;
    using Context::texSubImage2D;

private:
    ReferenceContext(const ReferenceContext &other);            // Not allowed!
    ReferenceContext &operator=(const ReferenceContext &other); // Not allowed!

    void deleteTexture(rc::Texture *texture);
    void deleteFramebuffer(rc::Framebuffer *framebuffer);
    void deleteRenderbuffer(rc::Renderbuffer *renderbuffer);
    void deleteBuffer(rc::DataBuffer *buffer);
    void deleteVertexArray(rc::VertexArray *vertexArray);
    void deleteProgramObject(rc::ShaderProgramObjectContainer *sp);

    void acquireFboAttachmentReference(const rc::Framebuffer::Attachment &attachment);
    void releaseFboAttachmentReference(const rc::Framebuffer::Attachment &attachment);
    tcu::PixelBufferAccess getFboAttachment(const rc::Framebuffer &framebuffer, rc::Framebuffer::AttachmentPoint point);

    uint32_t blitResolveMultisampleFramebuffer(uint32_t mask, const tcu::IVec4 &srcRect, const tcu::IVec4 &dstRect,
                                               bool flipX, bool flipY);

    rr::MultisamplePixelBufferAccess getDrawColorbuffer(void)
    {
        return (m_drawFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_drawFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_COLOR0))) :
                                            (m_defaultColorbuffer);
    }
    rr::MultisamplePixelBufferAccess getDrawDepthbuffer(void)
    {
        return (m_drawFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_drawFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_DEPTH))) :
                                            (m_defaultDepthbuffer);
    }
    rr::MultisamplePixelBufferAccess getDrawStencilbuffer(void)
    {
        return (m_drawFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_drawFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_STENCIL))) :
                                            (m_defaultStencilbuffer);
    }
    rr::MultisamplePixelBufferAccess getReadColorbuffer(void)
    {
        return (m_readFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_readFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_COLOR0))) :
                                            (m_defaultColorbuffer);
    }
    rr::MultisamplePixelBufferAccess getReadDepthbuffer(void)
    {
        return (m_readFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_readFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_DEPTH))) :
                                            (m_defaultDepthbuffer);
    }
    rr::MultisamplePixelBufferAccess getReadStencilbuffer(void)
    {
        return (m_readFramebufferBinding) ? (rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(getFboAttachment(
                                                *m_readFramebufferBinding, rc::Framebuffer::ATTACHMENTPOINT_STENCIL))) :
                                            (m_defaultStencilbuffer);
    }

    const rc::Texture2D &getTexture2D(int unitNdx) const;
    const rc::TextureCube &getTextureCube(int unitNdx) const;
    const tcu::IVec4 &getViewport(void) const
    {
        return m_viewport;
    }

    void setError(uint32_t error);

    void setTex1DBinding(int unit, rc::Texture1D *tex1D);
    void setTex2DBinding(int unit, rc::Texture2D *tex2D);
    void setTexCubeBinding(int unit, rc::TextureCube *texCube);
    void setTex2DArrayBinding(int unit, rc::Texture2DArray *tex2DArray);
    void setTex3DBinding(int unit, rc::Texture3D *tex3D);
    void setTexCubeArrayBinding(int unit, rc::TextureCubeArray *texCubeArray);

    void setBufferBinding(uint32_t target, rc::DataBuffer *buffer);
    rc::DataBuffer *getBufferBinding(uint32_t target) const;

    void *getPixelPackPtr(void *ptrOffset) const
    {
        return m_pixelPackBufferBinding ?
                   (void *)((uintptr_t)m_pixelPackBufferBinding->getData() + (uintptr_t)ptrOffset) :
                   ptrOffset;
    }
    const void *getPixelUnpackPtr(const void *ptrOffset) const
    {
        return m_pixelUnpackBufferBinding ?
                   (const void *)((uintptr_t)m_pixelUnpackBufferBinding->getData() + (uintptr_t)ptrOffset) :
                   ptrOffset;
    }

    bool predrawErrorChecks(uint32_t mode);
    void drawWithReference(const rr::PrimitiveList &primitives, int instanceCount);

    // Helpers for getting valid access object based on current unpack state.
    tcu::ConstPixelBufferAccess getUnpack2DAccess(const tcu::TextureFormat &format, int width, int height,
                                                  const void *data);
    tcu::ConstPixelBufferAccess getUnpack3DAccess(const tcu::TextureFormat &format, int width, int height, int depth,
                                                  const void *data);

    void uniformv(int32_t index, glu::DataType type, int32_t count, const void *);

    struct TextureUnit
    {

        rc::Texture1D *tex1DBinding;
        rc::Texture2D *tex2DBinding;
        rc::TextureCube *texCubeBinding;
        rc::Texture2DArray *tex2DArrayBinding;
        rc::Texture3D *tex3DBinding;
        rc::TextureCubeArray *texCubeArrayBinding;

        rc::Texture1D default1DTex;
        rc::Texture2D default2DTex;
        rc::TextureCube defaultCubeTex;
        rc::Texture2DArray default2DArrayTex;
        rc::Texture3D default3DTex;
        rc::TextureCubeArray defaultCubeArrayTex;

        TextureUnit(void)
            : tex1DBinding(nullptr)
            , tex2DBinding(nullptr)
            , texCubeBinding(nullptr)
            , tex2DArrayBinding(nullptr)
            , tex3DBinding(nullptr)
            , texCubeArrayBinding(nullptr)
            , default1DTex(0)
            , default2DTex(0)
            , defaultCubeTex(0)
            , default2DArrayTex(0)
            , default3DTex(0)
            , defaultCubeArrayTex(0)
        {
        }
    };

    struct StencilState
    {
        uint32_t func;
        int ref;
        uint32_t opMask;
        uint32_t opStencilFail;
        uint32_t opDepthFail;
        uint32_t opDepthPass;
        uint32_t writeMask;

        StencilState(void);
    };

    ReferenceContextLimits m_limits;

    rr::MultisamplePixelBufferAccess m_defaultColorbuffer;
    rr::MultisamplePixelBufferAccess m_defaultDepthbuffer;
    rr::MultisamplePixelBufferAccess m_defaultStencilbuffer;
    rc::VertexArray m_clientVertexArray;

    tcu::IVec4 m_viewport;

    rc::ObjectManager<rc::Texture> m_textures;
    rc::ObjectManager<rc::Framebuffer> m_framebuffers;
    rc::ObjectManager<rc::Renderbuffer> m_renderbuffers;
    rc::ObjectManager<rc::DataBuffer> m_buffers;
    rc::ObjectManager<rc::VertexArray> m_vertexArrays;
    rc::ObjectManager<rc::ShaderProgramObjectContainer> m_programs;

    int m_activeTexture;
    std::vector<TextureUnit> m_textureUnits;
    rc::Texture1D m_emptyTex1D;
    rc::Texture2D m_emptyTex2D;
    rc::TextureCube m_emptyTexCube;
    rc::Texture2DArray m_emptyTex2DArray;
    rc::Texture3D m_emptyTex3D;
    rc::TextureCubeArray m_emptyTexCubeArray;

    int m_pixelUnpackRowLength;
    int m_pixelUnpackSkipRows;
    int m_pixelUnpackSkipPixels;
    int m_pixelUnpackImageHeight;
    int m_pixelUnpackSkipImages;
    int m_pixelUnpackAlignment;
    int m_pixelPackAlignment;

    rc::Framebuffer *m_readFramebufferBinding;
    rc::Framebuffer *m_drawFramebufferBinding;
    rc::Renderbuffer *m_renderbufferBinding;
    rc::VertexArray *m_vertexArrayBinding;
    rc::ShaderProgramObjectContainer *m_currentProgram;

    rc::DataBuffer *m_arrayBufferBinding;
    rc::DataBuffer *m_pixelPackBufferBinding;
    rc::DataBuffer *m_pixelUnpackBufferBinding;
    rc::DataBuffer *m_transformFeedbackBufferBinding;
    rc::DataBuffer *m_uniformBufferBinding;
    rc::DataBuffer *m_copyReadBufferBinding;
    rc::DataBuffer *m_copyWriteBufferBinding;
    rc::DataBuffer *m_drawIndirectBufferBinding;

    tcu::Vec4 m_clearColor;
    float m_clearDepth;
    int m_clearStencil;

    bool m_scissorEnabled;
    tcu::IVec4 m_scissorBox;

    bool m_stencilTestEnabled;
    StencilState m_stencil[rr::FACETYPE_LAST];

    bool m_depthTestEnabled;
    uint32_t m_depthFunc;
    float m_depthRangeNear;
    float m_depthRangeFar;

    float m_polygonOffsetFactor;
    float m_polygonOffsetUnits;
    bool m_polygonOffsetFillEnabled;

    bool m_provokingFirstVertexConvention;

    bool m_blendEnabled;
    uint32_t m_blendModeRGB;
    uint32_t m_blendModeAlpha;
    uint32_t m_blendFactorSrcRGB;
    uint32_t m_blendFactorDstRGB;
    uint32_t m_blendFactorSrcAlpha;
    uint32_t m_blendFactorDstAlpha;
    tcu::Vec4 m_blendColor;

    bool m_sRGBUpdateEnabled;

    bool m_depthClampEnabled;

    tcu::BVec4 m_colorMask;
    bool m_depthMask;

    std::vector<rr::GenericVec4> m_currentAttribs;
    float m_lineWidth;

    bool m_primitiveRestartFixedIndex;
    bool m_primitiveRestartSettableIndex;
    uint32_t m_primitiveRestartIndex;

    uint32_t m_lastError;

    rr::FragmentProcessor m_fragmentProcessor;
    std::vector<rr::Fragment> m_fragmentBuffer;
    std::vector<float> m_fragmentDepths;
} DE_WARN_UNUSED_TYPE;

} // namespace sglr

#endif // _SGLRREFERENCECONTEXT_HPP
