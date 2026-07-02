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

#include "sglrReferenceContext.hpp"
#include "sglrReferenceUtils.hpp"
#include "sglrShaderProgram.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "gluDefs.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deMemory.h"
#include "rrFragmentOperations.hpp"
#include "rrRenderer.hpp"

#include <cstdint>

namespace sglr
{

using std::map;
using std::vector;

using tcu::IVec2;
using tcu::IVec4;
using tcu::RGBA;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

// Reference context implementation
using namespace rc;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;
using tcu::TextureFormat;

// Utilities for ReferenceContext
#define RC_RET_VOID

#define RC_ERROR_RET(ERR, RET) \
    do                         \
    {                          \
        setError(ERR);         \
        return RET;            \
    } while (false)

#define RC_IF_ERROR(COND, ERR, RET) \
    do                              \
    {                               \
        if (COND)                   \
            RC_ERROR_RET(ERR, RET); \
    } while (false)

static inline tcu::PixelBufferAccess nullAccess(void)
{
    return tcu::PixelBufferAccess(TextureFormat(TextureFormat::R, TextureFormat::UNSIGNED_INT8), 0, 0, 0, nullptr);
}

static inline bool isEmpty(const tcu::ConstPixelBufferAccess &access)
{
    return access.getWidth() == 0 || access.getHeight() == 0 || access.getDepth() == 0;
}

static inline bool isEmpty(const rr::MultisampleConstPixelBufferAccess &access)
{
    return access.raw().getWidth() == 0 || access.raw().getHeight() == 0 || access.raw().getDepth() == 0;
}

static inline bool isEmpty(const IVec4 &rect)
{
    return rect.z() == 0 || rect.w() == 0;
}

inline int getNumMipLevels1D(int size)
{
    return deLog2Floor32(size) + 1;
}

inline int getNumMipLevels2D(int width, int height)
{
    return deLog2Floor32(de::max(width, height)) + 1;
}

inline int getNumMipLevels3D(int width, int height, int depth)
{
    return deLog2Floor32(de::max(width, de::max(height, depth))) + 1;
}

inline int getMipLevelSize(int baseLevelSize, int levelNdx)
{
    return de::max(baseLevelSize >> levelNdx, 1);
}

inline bool isMipmapFilter(const tcu::Sampler::FilterMode mode)
{
    return mode != tcu::Sampler::NEAREST && mode != tcu::Sampler::LINEAR;
}

static tcu::CubeFace texTargetToFace(Framebuffer::TexTarget target)
{
    switch (target)
    {
    case Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_X:
        return tcu::CUBEFACE_NEGATIVE_X;
    case Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_X:
        return tcu::CUBEFACE_POSITIVE_X;
    case Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Y:
        return tcu::CUBEFACE_NEGATIVE_Y;
    case Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_Y:
        return tcu::CUBEFACE_POSITIVE_Y;
    case Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Z:
        return tcu::CUBEFACE_NEGATIVE_Z;
    case Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_Z:
        return tcu::CUBEFACE_POSITIVE_Z;
    default:
        return tcu::CUBEFACE_LAST;
    }
}

static Framebuffer::TexTarget texLayeredTypeToTarget(Texture::Type type)
{
    switch (type)
    {
    case Texture::TYPE_2D_ARRAY:
        return Framebuffer::TEXTARGET_2D_ARRAY;
    case Texture::TYPE_3D:
        return Framebuffer::TEXTARGET_3D;
    case Texture::TYPE_CUBE_MAP_ARRAY:
        return Framebuffer::TEXTARGET_CUBE_MAP_ARRAY;
    default:
        return Framebuffer::TEXTARGET_LAST;
    }
}

static tcu::CubeFace mapGLCubeFace(uint32_t face)
{
    switch (face)
    {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return tcu::CUBEFACE_NEGATIVE_X;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return tcu::CUBEFACE_POSITIVE_X;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return tcu::CUBEFACE_NEGATIVE_Y;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return tcu::CUBEFACE_POSITIVE_Y;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return tcu::CUBEFACE_NEGATIVE_Z;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return tcu::CUBEFACE_POSITIVE_Z;
    default:
        return tcu::CUBEFACE_LAST;
    }
}

tcu::TextureFormat toTextureFormat(const tcu::PixelFormat &pixelFmt)
{
    static const struct
    {
        tcu::PixelFormat pixelFmt;
        tcu::TextureFormat texFmt;
    } pixelFormatMap[] = {
        {tcu::PixelFormat(8, 8, 8, 8), tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)},
        {tcu::PixelFormat(8, 8, 8, 0), tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8)},
        {tcu::PixelFormat(4, 4, 4, 4),
         tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_SHORT_4444)},
        {tcu::PixelFormat(5, 5, 5, 1),
         tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_SHORT_5551)},
        {tcu::PixelFormat(5, 6, 5, 0),
         tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_SHORT_565)}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pixelFormatMap); ndx++)
    {
        if (pixelFormatMap[ndx].pixelFmt == pixelFmt)
            return pixelFormatMap[ndx].texFmt;
    }

    TCU_FAIL("Can't map pixel format to texture format");
}

tcu::TextureFormat toNonSRGBFormat(const tcu::TextureFormat &fmt)
{
    switch (fmt.order)
    {
    case tcu::TextureFormat::sRGB:
        return tcu::TextureFormat(tcu::TextureFormat::RGB, fmt.type);
    case tcu::TextureFormat::sRGBA:
        return tcu::TextureFormat(tcu::TextureFormat::RGBA, fmt.type);
    default:
        return fmt;
    }
}

tcu::TextureFormat getDepthFormat(int depthBits)
{
    switch (depthBits)
    {
    case 8:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT8);
    case 16:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
    case 24:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNSIGNED_INT_24_8);
    case 32:
        return tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
    default:
        TCU_FAIL("Can't map depth buffer format");
    }
}

tcu::TextureFormat getStencilFormat(int stencilBits)
{
    switch (stencilBits)
    {
    case 8:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
    case 16:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT16);
    case 24:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT_24_8);
    case 32:
        return tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT32);
    default:
        TCU_FAIL("Can't map depth buffer format");
    }
}

static inline tcu::IVec4 intersect(const tcu::IVec4 &a, const tcu::IVec4 &b)
{
    int x0 = de::max(a.x(), b.x());
    int y0 = de::max(a.y(), b.y());
    int x1 = de::min(a.x() + a.z(), b.x() + b.z());
    int y1 = de::min(a.y() + a.w(), b.y() + b.w());
    int w  = de::max(0, x1 - x0);
    int h  = de::max(0, y1 - y0);

    return tcu::IVec4(x0, y0, w, h);
}

static inline tcu::IVec4 getBufferRect(const rr::MultisampleConstPixelBufferAccess &access)
{
    return tcu::IVec4(0, 0, access.raw().getHeight(), access.raw().getDepth());
}

ReferenceContextLimits::ReferenceContextLimits(const glu::RenderContext &renderCtx)
    : contextType(renderCtx.getType())
    , maxTextureImageUnits(0)
    , maxTexture2DSize(0)
    , maxTextureCubeSize(0)
    , maxTexture2DArrayLayers(0)
    , maxTexture3DSize(0)
    , maxRenderbufferSize(0)
    , maxVertexAttribs(0)
    , subpixelBits(0)
{
    const glw::Functions &gl = renderCtx.getFunctions();

    // When the OpenGL ES's major version bigger than 3, and the expect context version is 3,
    // we need query the real GL context version and update the real version to reference context.
    if (glu::IsES3Compatible(gl) && isES2Context(contextType))
    {
        int majorVersion = contextType.getMajorVersion();
        int minorVersion = contextType.getMinorVersion();
        gl.getIntegerv(GL_MAJOR_VERSION, &majorVersion);
        gl.getIntegerv(GL_MINOR_VERSION, &minorVersion);
        contextType.setAPI(glu::ApiType::es(majorVersion, minorVersion));
    }

    gl.getIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
    gl.getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture2DSize);
    gl.getIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxTextureCubeSize);
    gl.getIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    gl.getIntegerv(GL_SUBPIXEL_BITS, &subpixelBits);

    if (contextSupports(contextType, glu::ApiType::es(3, 0)) || glu::isContextTypeGLCore(contextType))
    {
        gl.getIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTexture2DArrayLayers);
        gl.getIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxTexture3DSize);
    }

    // Limit texture sizes to supported values
    maxTexture2DSize   = de::min(maxTexture2DSize, (int)MAX_TEXTURE_SIZE);
    maxTextureCubeSize = de::min(maxTextureCubeSize, (int)MAX_TEXTURE_SIZE);
    maxTexture3DSize   = de::min(maxTexture3DSize, (int)MAX_TEXTURE_SIZE);

    GLU_EXPECT_NO_ERROR(gl.getError(), GL_NO_ERROR);

    // \todo [pyry] Figure out following things:
    // + supported fbo configurations
    // ...

    // \todo [2013-08-01 pyry] Do we want to make these conditional based on renderCtx?
    addExtension("GL_EXT_color_buffer_half_float");
    addExtension("GL_EXT_color_buffer_float");
    addExtension("GL_EXT_texture_format_BGRA8888");

    if (contextSupports(contextType, glu::ApiType::es(3, 1)))
        addExtension("GL_EXT_texture_cube_map_array");
}

void ReferenceContextLimits::addExtension(const char *extension)
{
    extensionList.push_back(extension);

    if (!extensionStr.empty())
        extensionStr += " ";
    extensionStr += extension;
}

ReferenceContextBuffers::ReferenceContextBuffers(const tcu::PixelFormat &colorBits, int depthBits, int stencilBits,
                                                 int width, int height, int samples)
{
    m_colorbuffer.setStorage(toTextureFormat(colorBits), samples, width, height);

    if (depthBits > 0)
        m_depthbuffer.setStorage(getDepthFormat(depthBits), samples, width, height);

    if (stencilBits > 0)
        m_stencilbuffer.setStorage(getStencilFormat(stencilBits), samples, width, height);
}

ReferenceContext::StencilState::StencilState(void)
    : func(GL_ALWAYS)
    , ref(0)
    , opMask(~0u)
    , opStencilFail(GL_KEEP)
    , opDepthFail(GL_KEEP)
    , opDepthPass(GL_KEEP)
    , writeMask(~0u)
{
}

ReferenceContext::ReferenceContext(const ReferenceContextLimits &limits,
                                   const rr::MultisamplePixelBufferAccess &colorbuffer,
                                   const rr::MultisamplePixelBufferAccess &depthbuffer,
                                   const rr::MultisamplePixelBufferAccess &stencilbuffer)
    : Context(limits.contextType)
    , m_limits(limits)
    , m_defaultColorbuffer(colorbuffer)
    , m_defaultDepthbuffer(depthbuffer)
    , m_defaultStencilbuffer(stencilbuffer)
    , m_clientVertexArray(0, m_limits.maxVertexAttribs)

    , m_viewport(0, 0, colorbuffer.raw().getHeight(), colorbuffer.raw().getDepth())

    , m_activeTexture(0)
    , m_textureUnits(m_limits.maxTextureImageUnits)
    , m_emptyTex1D()
    , m_emptyTex2D(isES2Context(limits.contextType))
    , m_emptyTexCube(!isES2Context(limits.contextType))
    , m_emptyTex2DArray()
    , m_emptyTex3D()
    , m_emptyTexCubeArray()

    , m_pixelUnpackRowLength(0)
    , m_pixelUnpackSkipRows(0)
    , m_pixelUnpackSkipPixels(0)
    , m_pixelUnpackImageHeight(0)
    , m_pixelUnpackSkipImages(0)
    , m_pixelUnpackAlignment(4)
    , m_pixelPackAlignment(4)

    , m_readFramebufferBinding(nullptr)
    , m_drawFramebufferBinding(nullptr)
    , m_renderbufferBinding(nullptr)
    , m_vertexArrayBinding(nullptr)
    , m_currentProgram(nullptr)

    , m_arrayBufferBinding(nullptr)
    , m_pixelPackBufferBinding(nullptr)
    , m_pixelUnpackBufferBinding(nullptr)
    , m_transformFeedbackBufferBinding(nullptr)
    , m_uniformBufferBinding(nullptr)
    , m_copyReadBufferBinding(nullptr)
    , m_copyWriteBufferBinding(nullptr)
    , m_drawIndirectBufferBinding(nullptr)

    , m_clearColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_clearDepth(1.0f)
    , m_clearStencil(0)
    , m_scissorEnabled(false)
    , m_scissorBox(m_viewport)
    , m_stencilTestEnabled(false)
    , m_depthTestEnabled(false)
    , m_depthFunc(GL_LESS)
    , m_depthRangeNear(0.0f)
    , m_depthRangeFar(1.0f)
    , m_polygonOffsetFactor(0.0f)
    , m_polygonOffsetUnits(0.0f)
    , m_polygonOffsetFillEnabled(false)
    , m_provokingFirstVertexConvention(false)
    , m_blendEnabled(false)
    , m_blendModeRGB(GL_FUNC_ADD)
    , m_blendModeAlpha(GL_FUNC_ADD)
    , m_blendFactorSrcRGB(GL_ONE)
    , m_blendFactorDstRGB(GL_ZERO)
    , m_blendFactorSrcAlpha(GL_ONE)
    , m_blendFactorDstAlpha(GL_ZERO)
    , m_blendColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_sRGBUpdateEnabled(true)
    , m_depthClampEnabled(false)
    , m_colorMask(true, true, true, true)
    , m_depthMask(true)
    , m_currentAttribs(m_limits.maxVertexAttribs, rr::GenericVec4(tcu::Vec4(0, 0, 0, 1)))
    , m_lineWidth(1.0f)
    , m_primitiveRestartFixedIndex(false)
    , m_primitiveRestartSettableIndex(false)
    , m_primitiveRestartIndex(0)

    , m_lastError(GL_NO_ERROR)
{
    // Create empty textures to be used when texture objects are incomplete.
    m_emptyTex1D.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex1D.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex1D.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTex1D.getSampler().magFilter = tcu::Sampler::NEAREST;
    m_emptyTex1D.allocLevel(0, tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1);
    m_emptyTex1D.getLevel(0).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
    m_emptyTex1D.updateView(tcu::Sampler::MODE_LAST);

    m_emptyTex2D.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex2D.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex2D.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTex2D.getSampler().magFilter = tcu::Sampler::NEAREST;
    m_emptyTex2D.allocLevel(0, tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1);
    m_emptyTex2D.getLevel(0).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
    m_emptyTex2D.updateView(tcu::Sampler::MODE_LAST);

    m_emptyTexCube.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTexCube.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTexCube.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTexCube.getSampler().magFilter = tcu::Sampler::NEAREST;
    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
    {
        m_emptyTexCube.allocFace(0, (tcu::CubeFace)face,
                                 tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1);
        m_emptyTexCube.getFace(0, (tcu::CubeFace)face).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
    }
    m_emptyTexCube.updateView(tcu::Sampler::MODE_LAST);

    m_emptyTex2DArray.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex2DArray.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex2DArray.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTex2DArray.getSampler().magFilter = tcu::Sampler::NEAREST;
    m_emptyTex2DArray.allocLevel(0, tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1,
                                 1);
    m_emptyTex2DArray.getLevel(0).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
    m_emptyTex2DArray.updateView(tcu::Sampler::MODE_LAST);

    m_emptyTex3D.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex3D.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex3D.getSampler().wrapR     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTex3D.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTex3D.getSampler().magFilter = tcu::Sampler::NEAREST;
    m_emptyTex3D.allocLevel(0, tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1, 1, 1);
    m_emptyTex3D.getLevel(0).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0);
    m_emptyTex3D.updateView(tcu::Sampler::MODE_LAST);

    m_emptyTexCubeArray.getSampler().wrapS     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTexCubeArray.getSampler().wrapT     = tcu::Sampler::CLAMP_TO_EDGE;
    m_emptyTexCubeArray.getSampler().minFilter = tcu::Sampler::NEAREST;
    m_emptyTexCubeArray.getSampler().magFilter = tcu::Sampler::NEAREST;
    m_emptyTexCubeArray.allocLevel(0, tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1,
                                   1, 6);
    for (int faceNdx = 0; faceNdx < 6; faceNdx++)
        m_emptyTexCubeArray.getLevel(0).setPixel(Vec4(0.0f, 0.0f, 0.0f, 1.0f), 0, 0, faceNdx);
    m_emptyTexCubeArray.updateView(tcu::Sampler::MODE_LAST);

    for (int unitNdx = 0; unitNdx < m_limits.maxTextureImageUnits; unitNdx++)
        m_textureUnits[unitNdx].defaultCubeTex.getSampler().seamlessCubeMap = !isES2Context(limits.contextType);

    if (glu::isContextTypeGLCore(getType()))
        m_sRGBUpdateEnabled = false;
}

ReferenceContext::~ReferenceContext(void)
{
    // Destroy all objects -- verifies that ref counting works
    {
        vector<VertexArray *> vertexArrays;
        m_vertexArrays.getAll(vertexArrays);
        for (vector<VertexArray *>::iterator i = vertexArrays.begin(); i != vertexArrays.end(); i++)
            deleteVertexArray(*i);

        DE_ASSERT(m_clientVertexArray.getRefCount() == 1);
    }

    {
        vector<Texture *> textures;
        m_textures.getAll(textures);
        for (vector<Texture *>::iterator i = textures.begin(); i != textures.end(); i++)
            deleteTexture(*i);
    }

    {
        vector<Framebuffer *> framebuffers;
        m_framebuffers.getAll(framebuffers);
        for (vector<Framebuffer *>::iterator i = framebuffers.begin(); i != framebuffers.end(); i++)
            deleteFramebuffer(*i);
    }

    {
        vector<Renderbuffer *> renderbuffers;
        m_renderbuffers.getAll(renderbuffers);
        for (vector<Renderbuffer *>::iterator i = renderbuffers.begin(); i != renderbuffers.end(); i++)
            deleteRenderbuffer(*i);
    }

    {
        vector<DataBuffer *> buffers;
        m_buffers.getAll(buffers);
        for (vector<DataBuffer *>::iterator i = buffers.begin(); i != buffers.end(); i++)
            deleteBuffer(*i);
    }

    {
        vector<ShaderProgramObjectContainer *> programs;
        m_programs.getAll(programs);
        for (vector<ShaderProgramObjectContainer *>::iterator i = programs.begin(); i != programs.end(); i++)
            deleteProgramObject(*i);
    }
}

void ReferenceContext::activeTexture(uint32_t texture)
{
    if (deInBounds32(texture, GL_TEXTURE0, GL_TEXTURE0 + (uint32_t)m_textureUnits.size()))
        m_activeTexture = texture - GL_TEXTURE0;
    else
        setError(GL_INVALID_ENUM);
}

void ReferenceContext::setTex1DBinding(int unitNdx, Texture1D *texture)
{
    if (m_textureUnits[unitNdx].tex1DBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].tex1DBinding);
        m_textureUnits[unitNdx].tex1DBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].tex1DBinding = texture;
    }
}

void ReferenceContext::setTex2DBinding(int unitNdx, Texture2D *texture)
{
    if (m_textureUnits[unitNdx].tex2DBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].tex2DBinding);
        m_textureUnits[unitNdx].tex2DBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].tex2DBinding = texture;
    }
}

void ReferenceContext::setTexCubeBinding(int unitNdx, TextureCube *texture)
{
    if (m_textureUnits[unitNdx].texCubeBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].texCubeBinding);
        m_textureUnits[unitNdx].texCubeBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].texCubeBinding = texture;
    }
}

void ReferenceContext::setTex2DArrayBinding(int unitNdx, Texture2DArray *texture)
{
    if (m_textureUnits[unitNdx].tex2DArrayBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].tex2DArrayBinding);
        m_textureUnits[unitNdx].tex2DArrayBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].tex2DArrayBinding = texture;
    }
}

void ReferenceContext::setTex3DBinding(int unitNdx, Texture3D *texture)
{
    if (m_textureUnits[unitNdx].tex3DBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].tex3DBinding);
        m_textureUnits[unitNdx].tex3DBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].tex3DBinding = texture;
    }
}

void ReferenceContext::setTexCubeArrayBinding(int unitNdx, TextureCubeArray *texture)
{
    if (m_textureUnits[unitNdx].texCubeArrayBinding)
    {
        m_textures.releaseReference(m_textureUnits[unitNdx].texCubeArrayBinding);
        m_textureUnits[unitNdx].texCubeArrayBinding = nullptr;
    }

    if (texture)
    {
        m_textures.acquireReference(texture);
        m_textureUnits[unitNdx].texCubeArrayBinding = texture;
    }
}

void ReferenceContext::bindTexture(uint32_t target, uint32_t texture)
{
    int unitNdx = m_activeTexture;

    RC_IF_ERROR(target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP &&
                    target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_3D && target != GL_TEXTURE_CUBE_MAP_ARRAY,
                GL_INVALID_ENUM, RC_RET_VOID);

    RC_IF_ERROR(glu::isContextTypeES(m_limits.contextType) && (target == GL_TEXTURE_1D), GL_INVALID_ENUM, RC_RET_VOID);

    if (texture == 0)
    {
        // Clear binding.
        switch (target)
        {
        case GL_TEXTURE_1D:
            setTex1DBinding(unitNdx, nullptr);
            break;
        case GL_TEXTURE_2D:
            setTex2DBinding(unitNdx, nullptr);
            break;
        case GL_TEXTURE_CUBE_MAP:
            setTexCubeBinding(unitNdx, nullptr);
            break;
        case GL_TEXTURE_2D_ARRAY:
            setTex2DArrayBinding(unitNdx, nullptr);
            break;
        case GL_TEXTURE_3D:
            setTex3DBinding(unitNdx, nullptr);
            break;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            setTexCubeArrayBinding(unitNdx, nullptr);
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
    {
        Texture *texObj = m_textures.find(texture);

        if (texObj)
        {
            // Validate type.
            Texture::Type expectedType = Texture::TYPE_LAST;
            switch (target)
            {
            case GL_TEXTURE_1D:
                expectedType = Texture::TYPE_1D;
                break;
            case GL_TEXTURE_2D:
                expectedType = Texture::TYPE_2D;
                break;
            case GL_TEXTURE_CUBE_MAP:
                expectedType = Texture::TYPE_CUBE_MAP;
                break;
            case GL_TEXTURE_2D_ARRAY:
                expectedType = Texture::TYPE_2D_ARRAY;
                break;
            case GL_TEXTURE_3D:
                expectedType = Texture::TYPE_3D;
                break;
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                expectedType = Texture::TYPE_CUBE_MAP_ARRAY;
                break;
            default:
                DE_ASSERT(false);
            }
            RC_IF_ERROR(texObj->getType() != expectedType, GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
        {
            // New texture object.
            bool seamlessCubeMap = !isES2Context(m_limits.contextType);
            switch (target)
            {
            case GL_TEXTURE_1D:
                texObj = new Texture1D(texture);
                break;
            case GL_TEXTURE_2D:
                texObj = new Texture2D(texture);
                break;
            case GL_TEXTURE_CUBE_MAP:
                texObj = new TextureCube(texture, seamlessCubeMap);
                break;
            case GL_TEXTURE_2D_ARRAY:
                texObj = new Texture2DArray(texture);
                break;
            case GL_TEXTURE_3D:
                texObj = new Texture3D(texture);
                break;
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                texObj = new TextureCubeArray(texture);
                break;
            default:
                DE_ASSERT(false);
            }

            m_textures.insert(texObj);
        }

        switch (target)
        {
        case GL_TEXTURE_1D:
            setTex1DBinding(unitNdx, static_cast<Texture1D *>(texObj));
            break;
        case GL_TEXTURE_2D:
            setTex2DBinding(unitNdx, static_cast<Texture2D *>(texObj));
            break;
        case GL_TEXTURE_CUBE_MAP:
            setTexCubeBinding(unitNdx, static_cast<TextureCube *>(texObj));
            break;
        case GL_TEXTURE_2D_ARRAY:
            setTex2DArrayBinding(unitNdx, static_cast<Texture2DArray *>(texObj));
            break;
        case GL_TEXTURE_3D:
            setTex3DBinding(unitNdx, static_cast<Texture3D *>(texObj));
            break;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            setTexCubeArrayBinding(unitNdx, static_cast<TextureCubeArray *>(texObj));
            break;
        default:
            DE_ASSERT(false);
        }
    }
}

void ReferenceContext::genTextures(int numTextures, uint32_t *textures)
{
    while (numTextures--)
        *textures++ = m_textures.allocateName();
}

void ReferenceContext::deleteTextures(int numTextures, const uint32_t *textures)
{
    for (int i = 0; i < numTextures; i++)
    {
        uint32_t name    = textures[i];
        Texture *texture = name ? m_textures.find(name) : nullptr;

        if (texture)
            deleteTexture(texture);
    }
}

void ReferenceContext::deleteTexture(Texture *texture)
{
    // Unbind from context
    for (int unitNdx = 0; unitNdx < (int)m_textureUnits.size(); unitNdx++)
    {
        if (m_textureUnits[unitNdx].tex1DBinding == texture)
            setTex1DBinding(unitNdx, nullptr);
        else if (m_textureUnits[unitNdx].tex2DBinding == texture)
            setTex2DBinding(unitNdx, nullptr);
        else if (m_textureUnits[unitNdx].texCubeBinding == texture)
            setTexCubeBinding(unitNdx, nullptr);
        else if (m_textureUnits[unitNdx].tex2DArrayBinding == texture)
            setTex2DArrayBinding(unitNdx, nullptr);
        else if (m_textureUnits[unitNdx].tex3DBinding == texture)
            setTex3DBinding(unitNdx, nullptr);
        else if (m_textureUnits[unitNdx].texCubeArrayBinding == texture)
            setTexCubeArrayBinding(unitNdx, nullptr);
    }

    // Unbind from currently bound framebuffers
    for (int ndx = 0; ndx < 2; ndx++)
    {
        rc::Framebuffer *framebufferBinding = ndx ? m_drawFramebufferBinding : m_readFramebufferBinding;
        if (framebufferBinding)
        {
            int releaseRefCount = (framebufferBinding == m_drawFramebufferBinding ? 1 : 0) +
                                  (framebufferBinding == m_readFramebufferBinding ? 1 : 0);

            for (int point = 0; point < Framebuffer::ATTACHMENTPOINT_LAST; point++)
            {
                Framebuffer::Attachment &attachment =
                    framebufferBinding->getAttachment((Framebuffer::AttachmentPoint)point);
                if (attachment.name == texture->getName())
                {
                    for (int refNdx = 0; refNdx < releaseRefCount; refNdx++)
                        releaseFboAttachmentReference(attachment);
                    attachment = Framebuffer::Attachment();
                }
            }
        }
    }

    DE_ASSERT(texture->getRefCount() == 1);
    m_textures.releaseReference(texture);
}

void ReferenceContext::bindFramebuffer(uint32_t target, uint32_t name)
{
    Framebuffer *fbo = nullptr;

    RC_IF_ERROR(target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (name != 0)
    {
        // Find or create framebuffer object.
        fbo = m_framebuffers.find(name);
        if (!fbo)
        {
            fbo = new Framebuffer(name);
            m_framebuffers.insert(fbo);
        }
    }

    for (int ndx = 0; ndx < 2; ndx++)
    {
        uint32_t bindingTarget    = ndx ? GL_DRAW_FRAMEBUFFER : GL_READ_FRAMEBUFFER;
        rc::Framebuffer *&binding = ndx ? m_drawFramebufferBinding : m_readFramebufferBinding;

        if (target != GL_FRAMEBUFFER && target != bindingTarget)
            continue; // Doesn't match this target.

        // Remove old references
        if (binding)
        {
            // Clear all attachment point references
            for (int point = 0; point < Framebuffer::ATTACHMENTPOINT_LAST; point++)
                releaseFboAttachmentReference(binding->getAttachment((Framebuffer::AttachmentPoint)point));

            m_framebuffers.releaseReference(binding);
        }

        // Create new references
        if (fbo)
        {
            m_framebuffers.acquireReference(fbo);

            for (int point = 0; point < Framebuffer::ATTACHMENTPOINT_LAST; point++)
                acquireFboAttachmentReference(fbo->getAttachment((Framebuffer::AttachmentPoint)point));
        }

        binding = fbo;
    }
}

void ReferenceContext::genFramebuffers(int numFramebuffers, uint32_t *framebuffers)
{
    while (numFramebuffers--)
        *framebuffers++ = m_framebuffers.allocateName();
}

void ReferenceContext::deleteFramebuffer(Framebuffer *framebuffer)
{
    // Remove bindings.
    if (m_drawFramebufferBinding == framebuffer)
        bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if (m_readFramebufferBinding == framebuffer)
        bindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    DE_ASSERT(framebuffer->getRefCount() == 1);
    m_framebuffers.releaseReference(framebuffer);
}

void ReferenceContext::deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers)
{
    for (int i = 0; i < numFramebuffers; i++)
    {
        uint32_t name            = framebuffers[i];
        Framebuffer *framebuffer = name ? m_framebuffers.find(name) : nullptr;

        if (framebuffer)
            deleteFramebuffer(framebuffer);
    }
}

void ReferenceContext::bindRenderbuffer(uint32_t target, uint32_t name)
{
    Renderbuffer *rbo = nullptr;

    RC_IF_ERROR(target != GL_RENDERBUFFER, GL_INVALID_ENUM, RC_RET_VOID);

    if (name != 0)
    {
        rbo = m_renderbuffers.find(name);
        if (!rbo)
        {
            rbo = new Renderbuffer(name);
            m_renderbuffers.insert(rbo);
        }
    }

    // Remove old reference
    if (m_renderbufferBinding)
        m_renderbuffers.releaseReference(m_renderbufferBinding);

    // Create new reference
    if (rbo)
        m_renderbuffers.acquireReference(rbo);

    m_renderbufferBinding = rbo;
}

void ReferenceContext::genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers)
{
    while (numRenderbuffers--)
        *renderbuffers++ = m_renderbuffers.allocateName();
}

void ReferenceContext::deleteRenderbuffer(Renderbuffer *renderbuffer)
{
    if (m_renderbufferBinding == renderbuffer)
        bindRenderbuffer(GL_RENDERBUFFER, 0);

    // Unbind from currently bound framebuffers
    for (int ndx = 0; ndx < 2; ndx++)
    {
        rc::Framebuffer *framebufferBinding = ndx ? m_drawFramebufferBinding : m_readFramebufferBinding;
        if (framebufferBinding)
        {
            int releaseRefCount = (framebufferBinding == m_drawFramebufferBinding ? 1 : 0) +
                                  (framebufferBinding == m_readFramebufferBinding ? 1 : 0);

            for (int point = 0; point < Framebuffer::ATTACHMENTPOINT_LAST; point++)
            {
                Framebuffer::Attachment &attachment =
                    framebufferBinding->getAttachment((Framebuffer::AttachmentPoint)point);
                if (attachment.name == renderbuffer->getName())
                {
                    for (int refNdx = 0; refNdx < releaseRefCount; refNdx++)
                        releaseFboAttachmentReference(attachment);
                    attachment = Framebuffer::Attachment();
                }
            }
        }
    }

    DE_ASSERT(renderbuffer->getRefCount() == 1);
    m_renderbuffers.releaseReference(renderbuffer);
}

void ReferenceContext::deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers)
{
    for (int i = 0; i < numRenderbuffers; i++)
    {
        uint32_t name              = renderbuffers[i];
        Renderbuffer *renderbuffer = name ? m_renderbuffers.find(name) : nullptr;

        if (renderbuffer)
            deleteRenderbuffer(renderbuffer);
    }
}

void ReferenceContext::pixelStorei(uint32_t pname, int param)
{
    switch (pname)
    {
    case GL_UNPACK_ALIGNMENT:
        RC_IF_ERROR(param != 1 && param != 2 && param != 4 && param != 8, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackAlignment = param;
        break;

    case GL_PACK_ALIGNMENT:
        RC_IF_ERROR(param != 1 && param != 2 && param != 4 && param != 8, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelPackAlignment = param;
        break;

    case GL_UNPACK_ROW_LENGTH:
        RC_IF_ERROR(param < 0, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackRowLength = param;
        break;

    case GL_UNPACK_SKIP_ROWS:
        RC_IF_ERROR(param < 0, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackSkipRows = param;
        break;

    case GL_UNPACK_SKIP_PIXELS:
        RC_IF_ERROR(param < 0, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackSkipPixels = param;
        break;

    case GL_UNPACK_IMAGE_HEIGHT:
        RC_IF_ERROR(param < 0, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackImageHeight = param;
        break;

    case GL_UNPACK_SKIP_IMAGES:
        RC_IF_ERROR(param < 0, GL_INVALID_VALUE, RC_RET_VOID);
        m_pixelUnpackSkipImages = param;
        break;

    default:
        setError(GL_INVALID_ENUM);
    }
}

tcu::ConstPixelBufferAccess ReferenceContext::getUnpack2DAccess(const tcu::TextureFormat &format, int width, int height,
                                                                const void *data)
{
    int pixelSize      = format.getPixelSize();
    int rowLen         = m_pixelUnpackRowLength > 0 ? m_pixelUnpackRowLength : width;
    int rowPitch       = deAlign32(rowLen * pixelSize, m_pixelUnpackAlignment);
    const uint8_t *ptr = (const uint8_t *)data + m_pixelUnpackSkipRows * rowPitch + m_pixelUnpackSkipPixels * pixelSize;

    return tcu::ConstPixelBufferAccess(format, width, height, 1, rowPitch, 0, ptr);
}

tcu::ConstPixelBufferAccess ReferenceContext::getUnpack3DAccess(const tcu::TextureFormat &format, int width, int height,
                                                                int depth, const void *data)
{
    int pixelSize      = format.getPixelSize();
    int rowLen         = m_pixelUnpackRowLength > 0 ? m_pixelUnpackRowLength : width;
    int imageHeight    = m_pixelUnpackImageHeight > 0 ? m_pixelUnpackImageHeight : height;
    int rowPitch       = deAlign32(rowLen * pixelSize, m_pixelUnpackAlignment);
    int slicePitch     = imageHeight * rowPitch;
    const uint8_t *ptr = (const uint8_t *)data + m_pixelUnpackSkipImages * slicePitch +
                         m_pixelUnpackSkipRows * rowPitch + m_pixelUnpackSkipPixels * pixelSize;

    return tcu::ConstPixelBufferAccess(format, width, height, depth, rowPitch, slicePitch, ptr);
}

static tcu::TextureFormat mapInternalFormat(uint32_t internalFormat)
{
    switch (internalFormat)
    {
    case GL_ALPHA:
        return TextureFormat(TextureFormat::A, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE:
        return TextureFormat(TextureFormat::L, TextureFormat::UNORM_INT8);
    case GL_LUMINANCE_ALPHA:
        return TextureFormat(TextureFormat::LA, TextureFormat::UNORM_INT8);
    case GL_RGB:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
    case GL_RGBA:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);

    default:
        return glu::mapGLInternalFormat(internalFormat);
    }
}

static void depthValueFloatClampCopy(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src)
{
    int width  = dst.getWidth();
    int height = dst.getHeight();
    int depth  = dst.getDepth();

    DE_ASSERT(src.getWidth() == width && src.getHeight() == height && src.getDepth() == depth);

    // clamping copy

    if (src.getFormat().order == tcu::TextureFormat::DS && dst.getFormat().order == tcu::TextureFormat::DS)
    {
        // copy only depth and stencil
        for (int z = 0; z < depth; z++)
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                {
                    dst.setPixDepth(de::clamp(src.getPixDepth(x, y, z), 0.0f, 1.0f), x, y, z);
                    dst.setPixStencil(src.getPixStencil(x, y, z), x, y, z);
                }
    }
    else
    {
        // copy only depth
        for (int z = 0; z < depth; z++)
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    dst.setPixDepth(de::clamp(src.getPixDepth(x, y, z), 0.0f, 1.0f), x, y, z);
    }
}

void ReferenceContext::texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border,
                                  uint32_t format, uint32_t type, const void *data)
{
    texImage2D(target, level, internalFormat, width, 1, border, format, type, data);
}

void ReferenceContext::texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height,
                                  int border, uint32_t format, uint32_t type, const void *data)
{
    texImage3D(target, level, internalFormat, width, height, 1, border, format, type, data);
}

static void clearToTextureInitialValue(PixelBufferAccess access)
{
    const bool hasDepth =
        access.getFormat().order == tcu::TextureFormat::D || access.getFormat().order == tcu::TextureFormat::DS;
    const bool hasStencil =
        access.getFormat().order == tcu::TextureFormat::S || access.getFormat().order == tcu::TextureFormat::DS;
    const bool hasColor = !hasDepth && !hasStencil;

    if (hasDepth)
        tcu::clearDepth(access, 0.0f);
    if (hasStencil)
        tcu::clearStencil(access, 0u);
    if (hasColor)
        tcu::clear(access, Vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void ReferenceContext::texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                                  int border, uint32_t format, uint32_t type, const void *data)
{
    TextureUnit &unit     = m_textureUnits[m_activeTexture];
    const void *unpackPtr = getPixelUnpackPtr(data);
    const bool isDstFloatDepthFormat =
        (internalFormat == GL_DEPTH_COMPONENT32F ||
         internalFormat == GL_DEPTH32F_STENCIL8); // depth components are limited to [0,1] range
    TextureFormat storageFmt;
    TextureFormat transferFmt;

    RC_IF_ERROR(border != 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || height < 0 || depth < 0 || level < 0, GL_INVALID_VALUE, RC_RET_VOID);

    // Map storage format.
    storageFmt = mapInternalFormat(internalFormat);
    RC_IF_ERROR(storageFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    storageFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    // Map transfer format.
    transferFmt = glu::mapGLTransferFormat(format, type);
    RC_IF_ERROR(transferFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    transferFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (target == GL_TEXTURE_1D && glu::isContextTypeGLCore(m_limits.contextType))
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height != 1 || depth != 1, GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture1D *texture = unit.tex1DBinding ? unit.tex1DBinding : &unit.default1DTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth(), GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack2DAccess(transferFmt, width, 1, unpackPtr);
            PixelBufferAccess dst(texture->getLevel(level));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getLevel(level));
        }
    }
    else if (target == GL_TEXTURE_2D)
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height > m_limits.maxTexture2DSize || depth != 1,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture2D *texture = unit.tex2DBinding ? unit.tex2DBinding : &unit.default2DTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width, height);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack2DAccess(transferFmt, width, height, unpackPtr);
            PixelBufferAccess dst(texture->getLevel(level));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getLevel(level));
        }
    }
    else if (target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X || target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    {
        // Validate size and level.
        RC_IF_ERROR(width != height || width > m_limits.maxTextureCubeSize || depth != 1, GL_INVALID_VALUE,
                    RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTextureCubeSize), GL_INVALID_VALUE, RC_RET_VOID);

        TextureCube *texture = unit.texCubeBinding ? unit.texCubeBinding : &unit.defaultCubeTex;
        tcu::CubeFace face   = mapGLCubeFace(target);

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasFace(level, face), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getFace(level, face));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocFace(level, face, storageFmt, width, height);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack2DAccess(transferFmt, width, height, unpackPtr);
            PixelBufferAccess dst(texture->getFace(level, face));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getFace(level, face));
        }
    }
    else if (target == GL_TEXTURE_2D_ARRAY)
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height > m_limits.maxTexture2DSize ||
                        depth > m_limits.maxTexture2DArrayLayers,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture2DArray *texture = unit.tex2DArrayBinding ? unit.tex2DArrayBinding : &unit.default2DArrayTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight() ||
                            depth != dst.getDepth(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width, height, depth);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack3DAccess(transferFmt, width, height, depth, unpackPtr);
            PixelBufferAccess dst(texture->getLevel(level));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getLevel(level));
        }
    }
    else if (target == GL_TEXTURE_3D)
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture3DSize || height > m_limits.maxTexture3DSize ||
                        depth > m_limits.maxTexture3DSize,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture3DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture3D *texture = unit.tex3DBinding ? unit.tex3DBinding : &unit.default3DTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight() ||
                            depth != dst.getDepth(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width, height, depth);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack3DAccess(transferFmt, width, height, depth, unpackPtr);
            PixelBufferAccess dst(texture->getLevel(level));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getLevel(level));
        }
    }
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        // Validate size and level.
        RC_IF_ERROR(width != height || width > m_limits.maxTexture2DSize || depth % 6 != 0 ||
                        depth > m_limits.maxTexture2DArrayLayers,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        TextureCubeArray *texture = unit.texCubeArrayBinding ? unit.texCubeArrayBinding : &unit.defaultCubeArrayTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight() ||
                            depth != dst.getDepth(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width, height, depth);

        if (unpackPtr)
        {
            ConstPixelBufferAccess src = getUnpack3DAccess(transferFmt, width, height, depth, unpackPtr);
            PixelBufferAccess dst(texture->getLevel(level));

            if (isDstFloatDepthFormat)
                depthValueFloatClampCopy(dst, src);
            else
                tcu::copy(dst, src);
        }
        else
        {
            // No data supplied, clear to initial
            clearToTextureInitialValue(texture->getLevel(level));
        }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                                     const void *data)
{
    texSubImage2D(target, level, xoffset, 0, width, 1, format, type, data);
}

void ReferenceContext::texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                                     uint32_t format, uint32_t type, const void *data)
{
    texSubImage3D(target, level, xoffset, yoffset, 0, width, height, 1, format, type, data);
}

void ReferenceContext::texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width,
                                     int height, int depth, uint32_t format, uint32_t type, const void *data)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];

    RC_IF_ERROR(xoffset < 0 || yoffset < 0 || zoffset < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || height < 0 || depth < 0, GL_INVALID_VALUE, RC_RET_VOID);

    TextureFormat transferFmt = glu::mapGLTransferFormat(format, type);
    RC_IF_ERROR(transferFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    transferFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    ConstPixelBufferAccess src = getUnpack3DAccess(transferFmt, width, height, depth, getPixelUnpackPtr(data));

    if (target == GL_TEXTURE_1D && glu::isContextTypeGLCore(m_limits.contextType))
    {
        Texture1D &texture = unit.tex1DBinding ? *unit.tex1DBinding : unit.default1DTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else if (target == GL_TEXTURE_2D)
    {
        Texture2D &texture = unit.tex2DBinding ? *unit.tex2DBinding : unit.default2DTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else if (target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X || target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    {
        TextureCube &texture = unit.texCubeBinding ? *unit.texCubeBinding : unit.defaultCubeTex;
        tcu::CubeFace face   = mapGLCubeFace(target);

        RC_IF_ERROR(!texture.hasFace(level, face), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getFace(level, face);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else if (target == GL_TEXTURE_3D)
    {
        Texture3D &texture = unit.tex3DBinding ? *unit.tex3DBinding : unit.default3DTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else if (target == GL_TEXTURE_2D_ARRAY)
    {
        Texture2DArray &texture = unit.tex2DArrayBinding ? *unit.tex2DArrayBinding : unit.default2DArrayTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        TextureCubeArray &texture = unit.texCubeArrayBinding ? *unit.texCubeArrayBinding : unit.defaultCubeArrayTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight() ||
                        zoffset + depth > dst.getDepth(),
                    GL_INVALID_VALUE, RC_RET_VOID);

        // depth components are limited to [0,1] range
        if (dst.getFormat().order == tcu::TextureFormat::D || dst.getFormat().order == tcu::TextureFormat::DS)
            depthValueFloatClampCopy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
        else
            tcu::copy(tcu::getSubregion(dst, xoffset, yoffset, zoffset, width, height, depth), src);
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                      int border)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];
    TextureFormat storageFmt;
    rr::MultisampleConstPixelBufferAccess src = getReadColorbuffer();

    RC_IF_ERROR(border != 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || level < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(isEmpty(src), GL_INVALID_OPERATION, RC_RET_VOID);

    // Map storage format.
    storageFmt = mapInternalFormat(internalFormat);
    RC_IF_ERROR(storageFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    storageFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (target == GL_TEXTURE_1D)
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture2DSize, GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture1D *texture = unit.tex1DBinding ? unit.tex1DBinding : &unit.default1DTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth(), GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width);

        // Copy from current framebuffer.
        PixelBufferAccess dst = texture->getLevel(level);
        for (int xo = 0; xo < width; xo++)
        {
            if (!de::inBounds(x + xo, 0, src.raw().getHeight()))
                continue; // Undefined pixel.

            dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y), xo, 0);
        }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                      int height, int border)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];
    TextureFormat storageFmt;
    rr::MultisampleConstPixelBufferAccess src = getReadColorbuffer();

    RC_IF_ERROR(border != 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || height < 0 || level < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(isEmpty(src), GL_INVALID_OPERATION, RC_RET_VOID);

    // Map storage format.
    storageFmt = mapInternalFormat(internalFormat);
    RC_IF_ERROR(storageFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    storageFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (target == GL_TEXTURE_2D)
    {
        // Validate size and level.
        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height > m_limits.maxTexture2DSize, GL_INVALID_VALUE,
                    RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTexture2DSize), GL_INVALID_VALUE, RC_RET_VOID);

        Texture2D *texture = unit.tex2DBinding ? unit.tex2DBinding : &unit.default2DTex;

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasLevel(level), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getLevel(level));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocLevel(level, storageFmt, width, height);

        // Copy from current framebuffer.
        PixelBufferAccess dst = texture->getLevel(level);
        for (int yo = 0; yo < height; yo++)
            for (int xo = 0; xo < width; xo++)
            {
                if (!de::inBounds(x + xo, 0, src.raw().getHeight()) || !de::inBounds(y + yo, 0, src.raw().getDepth()))
                    continue; // Undefined pixel.

                dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y + yo), xo, yo);
            }
    }
    else if (target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X || target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    {
        // Validate size and level.
        RC_IF_ERROR(width != height || width > m_limits.maxTextureCubeSize, GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(level > deLog2Floor32(m_limits.maxTextureCubeSize), GL_INVALID_VALUE, RC_RET_VOID);

        TextureCube *texture = unit.texCubeBinding ? unit.texCubeBinding : &unit.defaultCubeTex;
        tcu::CubeFace face   = mapGLCubeFace(target);

        if (texture->isImmutable())
        {
            RC_IF_ERROR(!texture->hasFace(level, face), GL_INVALID_OPERATION, RC_RET_VOID);

            ConstPixelBufferAccess dst(texture->getFace(level, face));
            RC_IF_ERROR(storageFmt != dst.getFormat() || width != dst.getWidth() || height != dst.getHeight(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        }
        else
            texture->allocFace(level, face, storageFmt, width, height);

        // Copy from current framebuffer.
        PixelBufferAccess dst = texture->getFace(level, face);
        for (int yo = 0; yo < height; yo++)
            for (int xo = 0; xo < width; xo++)
            {
                if (!de::inBounds(x + xo, 0, src.raw().getHeight()) || !de::inBounds(y + yo, 0, src.raw().getDepth()))
                    continue; // Undefined pixel.

                dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y + yo), xo, yo);
            }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width)
{
    TextureUnit &unit                         = m_textureUnits[m_activeTexture];
    rr::MultisampleConstPixelBufferAccess src = getReadColorbuffer();

    RC_IF_ERROR(xoffset < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(isEmpty(src), GL_INVALID_OPERATION, RC_RET_VOID);

    if (target == GL_TEXTURE_1D)
    {
        Texture1D &texture = unit.tex1DBinding ? *unit.tex1DBinding : unit.default1DTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth(), GL_INVALID_VALUE, RC_RET_VOID);

        for (int xo = 0; xo < width; xo++)
        {
            if (!de::inBounds(x + xo, 0, src.raw().getHeight()))
                continue;

            dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y), xo + xoffset, 0);
        }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                         int height)
{
    TextureUnit &unit                         = m_textureUnits[m_activeTexture];
    rr::MultisampleConstPixelBufferAccess src = getReadColorbuffer();

    RC_IF_ERROR(xoffset < 0 || yoffset < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || height < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(isEmpty(src), GL_INVALID_OPERATION, RC_RET_VOID);

    if (target == GL_TEXTURE_2D)
    {
        Texture2D &texture = unit.tex2DBinding ? *unit.tex2DBinding : unit.default2DTex;

        RC_IF_ERROR(!texture.hasLevel(level), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getLevel(level);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight(), GL_INVALID_VALUE,
                    RC_RET_VOID);

        for (int yo = 0; yo < height; yo++)
            for (int xo = 0; xo < width; xo++)
            {
                if (!de::inBounds(x + xo, 0, src.raw().getHeight()) || !de::inBounds(y + yo, 0, src.raw().getDepth()))
                    continue;

                dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y + yo), xo + xoffset, yo + yoffset);
            }
    }
    else if (target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X || target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
             target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    {
        TextureCube &texture = unit.texCubeBinding ? *unit.texCubeBinding : unit.defaultCubeTex;
        tcu::CubeFace face   = mapGLCubeFace(target);

        RC_IF_ERROR(!texture.hasFace(level, face), GL_INVALID_VALUE, RC_RET_VOID);

        PixelBufferAccess dst = texture.getFace(level, face);

        RC_IF_ERROR(xoffset + width > dst.getWidth() || yoffset + height > dst.getHeight(), GL_INVALID_VALUE,
                    RC_RET_VOID);

        for (int yo = 0; yo < height; yo++)
            for (int xo = 0; xo < width; xo++)
            {
                if (!de::inBounds(x + xo, 0, src.raw().getHeight()) || !de::inBounds(y + yo, 0, src.raw().getDepth()))
                    continue;

                dst.setPixel(rr::resolveMultisamplePixel(src, x + xo, y + yo), xo + xoffset, yo + yoffset);
            }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x,
                                         int y, int width, int height)
{
    DE_UNREF(target && level && xoffset && yoffset && zoffset && x && y && width && height);
    DE_ASSERT(false);
}

void ReferenceContext::texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];
    TextureFormat storageFmt;

    RC_IF_ERROR(width <= 0 || height <= 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(!de::inRange(levels, 1, (int)deLog2Floor32(de::max(width, height)) + 1), GL_INVALID_VALUE, RC_RET_VOID);

    // Map storage format.
    storageFmt = mapInternalFormat(internalFormat);
    RC_IF_ERROR(storageFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    storageFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (target == GL_TEXTURE_2D)
    {
        Texture2D &texture = unit.tex2DBinding ? *unit.tex2DBinding : unit.default2DTex;

        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height >= m_limits.maxTexture2DSize, GL_INVALID_VALUE,
                    RC_RET_VOID);
        RC_IF_ERROR(texture.isImmutable(), GL_INVALID_OPERATION, RC_RET_VOID);

        texture.clearLevels();
        texture.setImmutable();

        for (int level = 0; level < levels; level++)
        {
            int levelW = de::max(1, width >> level);
            int levelH = de::max(1, height >> level);

            texture.allocLevel(level, storageFmt, levelW, levelH);
        }
    }
    else if (target == GL_TEXTURE_CUBE_MAP)
    {
        TextureCube &texture = unit.texCubeBinding ? *unit.texCubeBinding : unit.defaultCubeTex;

        RC_IF_ERROR(width > m_limits.maxTextureCubeSize || height > m_limits.maxTextureCubeSize, GL_INVALID_VALUE,
                    RC_RET_VOID);
        RC_IF_ERROR(texture.isImmutable(), GL_INVALID_OPERATION, RC_RET_VOID);

        texture.clearLevels();
        texture.setImmutable();

        for (int level = 0; level < levels; level++)
        {
            int levelW = de::max(1, width >> level);
            int levelH = de::max(1, height >> level);

            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                texture.allocFace(level, (tcu::CubeFace)face, storageFmt, levelW, levelH);
        }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

void ReferenceContext::texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height,
                                    int depth)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];
    TextureFormat storageFmt;

    RC_IF_ERROR(width <= 0 || height <= 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(!de::inRange(levels, 1, (int)deLog2Floor32(de::max(width, height)) + 1), GL_INVALID_VALUE, RC_RET_VOID);

    // Map storage format.
    storageFmt = mapInternalFormat(internalFormat);
    RC_IF_ERROR(storageFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    storageFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    if (target == GL_TEXTURE_2D_ARRAY)
    {
        Texture2DArray &texture = unit.tex2DArrayBinding ? *unit.tex2DArrayBinding : unit.default2DArrayTex;

        RC_IF_ERROR(width > m_limits.maxTexture2DSize || height >= m_limits.maxTexture2DSize ||
                        depth >= m_limits.maxTexture2DArrayLayers,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(texture.isImmutable(), GL_INVALID_OPERATION, RC_RET_VOID);

        texture.clearLevels();
        texture.setImmutable();

        for (int level = 0; level < levels; level++)
        {
            int levelW = de::max(1, width >> level);
            int levelH = de::max(1, height >> level);

            texture.allocLevel(level, storageFmt, levelW, levelH, depth);
        }
    }
    else if (target == GL_TEXTURE_3D)
    {
        Texture3D &texture = unit.tex3DBinding ? *unit.tex3DBinding : unit.default3DTex;

        RC_IF_ERROR(width > m_limits.maxTexture3DSize || height > m_limits.maxTexture3DSize ||
                        depth > m_limits.maxTexture3DSize,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(texture.isImmutable(), GL_INVALID_OPERATION, RC_RET_VOID);

        texture.clearLevels();
        texture.setImmutable();

        for (int level = 0; level < levels; level++)
        {
            int levelW = de::max(1, width >> level);
            int levelH = de::max(1, height >> level);
            int levelD = de::max(1, depth >> level);

            texture.allocLevel(level, storageFmt, levelW, levelH, levelD);
        }
    }
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        TextureCubeArray &texture = unit.texCubeArrayBinding ? *unit.texCubeArrayBinding : unit.defaultCubeArrayTex;

        RC_IF_ERROR(width != height || depth % 6 != 0 || width > m_limits.maxTexture2DSize ||
                        depth >= m_limits.maxTexture2DArrayLayers,
                    GL_INVALID_VALUE, RC_RET_VOID);
        RC_IF_ERROR(texture.isImmutable(), GL_INVALID_OPERATION, RC_RET_VOID);

        texture.clearLevels();
        texture.setImmutable();

        for (int level = 0; level < levels; level++)
        {
            int levelW = de::max(1, width >> level);
            int levelH = de::max(1, height >> level);

            texture.allocLevel(level, storageFmt, levelW, levelH, depth);
        }
    }
    else
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
}

// \todo [2014-02-19 pyry] Duplicated with code in gluTextureUtil.hpp

static inline tcu::Sampler::WrapMode mapGLWrapMode(int value)
{
    switch (value)
    {
    case GL_CLAMP_TO_EDGE:
        return tcu::Sampler::CLAMP_TO_EDGE;
    case GL_REPEAT:
        return tcu::Sampler::REPEAT_GL;
    case GL_MIRRORED_REPEAT:
        return tcu::Sampler::MIRRORED_REPEAT_GL;
    default:
        return tcu::Sampler::WRAPMODE_LAST;
    }
}

static inline tcu::Sampler::FilterMode mapGLFilterMode(int value)
{
    switch (value)
    {
    case GL_NEAREST:
        return tcu::Sampler::NEAREST;
    case GL_LINEAR:
        return tcu::Sampler::LINEAR;
    case GL_NEAREST_MIPMAP_NEAREST:
        return tcu::Sampler::NEAREST_MIPMAP_NEAREST;
    case GL_NEAREST_MIPMAP_LINEAR:
        return tcu::Sampler::NEAREST_MIPMAP_LINEAR;
    case GL_LINEAR_MIPMAP_NEAREST:
        return tcu::Sampler::LINEAR_MIPMAP_NEAREST;
    case GL_LINEAR_MIPMAP_LINEAR:
        return tcu::Sampler::LINEAR_MIPMAP_LINEAR;
    default:
        return tcu::Sampler::FILTERMODE_LAST;
    }
}

void ReferenceContext::texParameteri(uint32_t target, uint32_t pname, int value)
{
    TextureUnit &unit = m_textureUnits[m_activeTexture];
    Texture *texture  = nullptr;

    switch (target)
    {
    case GL_TEXTURE_1D:
        texture = unit.tex1DBinding ? unit.tex1DBinding : &unit.default1DTex;
        break;
    case GL_TEXTURE_2D:
        texture = unit.tex2DBinding ? unit.tex2DBinding : &unit.default2DTex;
        break;
    case GL_TEXTURE_CUBE_MAP:
        texture = unit.texCubeBinding ? unit.texCubeBinding : &unit.defaultCubeTex;
        break;
    case GL_TEXTURE_2D_ARRAY:
        texture = unit.tex2DArrayBinding ? unit.tex2DArrayBinding : &unit.default2DArrayTex;
        break;
    case GL_TEXTURE_3D:
        texture = unit.tex3DBinding ? unit.tex3DBinding : &unit.default3DTex;
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        texture = unit.texCubeArrayBinding ? unit.texCubeArrayBinding : &unit.defaultCubeArrayTex;
        break;

    default:
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
    }

    switch (pname)
    {
    case GL_TEXTURE_WRAP_S:
    {
        tcu::Sampler::WrapMode wrapS = mapGLWrapMode(value);
        RC_IF_ERROR(wrapS == tcu::Sampler::WRAPMODE_LAST, GL_INVALID_VALUE, RC_RET_VOID);
        texture->getSampler().wrapS = wrapS;
        break;
    }

    case GL_TEXTURE_WRAP_T:
    {
        tcu::Sampler::WrapMode wrapT = mapGLWrapMode(value);
        RC_IF_ERROR(wrapT == tcu::Sampler::WRAPMODE_LAST, GL_INVALID_VALUE, RC_RET_VOID);
        texture->getSampler().wrapT = wrapT;
        break;
    }

    case GL_TEXTURE_WRAP_R:
    {
        tcu::Sampler::WrapMode wrapR = mapGLWrapMode(value);
        RC_IF_ERROR(wrapR == tcu::Sampler::WRAPMODE_LAST, GL_INVALID_VALUE, RC_RET_VOID);
        texture->getSampler().wrapR = wrapR;
        break;
    }

    case GL_TEXTURE_MIN_FILTER:
    {
        tcu::Sampler::FilterMode minMode = mapGLFilterMode(value);
        RC_IF_ERROR(minMode == tcu::Sampler::FILTERMODE_LAST, GL_INVALID_VALUE, RC_RET_VOID);
        texture->getSampler().minFilter = minMode;
        break;
    }

    case GL_TEXTURE_MAG_FILTER:
    {
        tcu::Sampler::FilterMode magMode = mapGLFilterMode(value);
        RC_IF_ERROR(magMode != tcu::Sampler::LINEAR && magMode != tcu::Sampler::NEAREST, GL_INVALID_VALUE, RC_RET_VOID);
        texture->getSampler().magFilter = magMode;
        break;
    }

    case GL_TEXTURE_MAX_LEVEL:
    {
        RC_IF_ERROR(value < 0, GL_INVALID_VALUE, RC_RET_VOID);
        texture->setMaxLevel(value);
        break;
    }

    default:
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
    }
}

static inline Framebuffer::AttachmentPoint mapGLAttachmentPoint(uint32_t attachment)
{
    switch (attachment)
    {
    case GL_COLOR_ATTACHMENT0:
        return Framebuffer::ATTACHMENTPOINT_COLOR0;
    case GL_DEPTH_ATTACHMENT:
        return Framebuffer::ATTACHMENTPOINT_DEPTH;
    case GL_STENCIL_ATTACHMENT:
        return Framebuffer::ATTACHMENTPOINT_STENCIL;
    default:
        return Framebuffer::ATTACHMENTPOINT_LAST;
    }
}

static inline Framebuffer::TexTarget mapGLFboTexTarget(uint32_t target)
{
    switch (target)
    {
    case GL_TEXTURE_2D:
        return Framebuffer::TEXTARGET_2D;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_X;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_Y;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_Z;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_X;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Y;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Z;
    default:
        return Framebuffer::TEXTARGET_LAST;
    }
}

void ReferenceContext::acquireFboAttachmentReference(const Framebuffer::Attachment &attachment)
{
    switch (attachment.type)
    {
    case Framebuffer::ATTACHMENTTYPE_TEXTURE:
    {
        TCU_CHECK(attachment.name != 0);
        Texture *texture = m_textures.find(attachment.name);
        TCU_CHECK(texture);
        m_textures.acquireReference(texture);
        break;
    }

    case Framebuffer::ATTACHMENTTYPE_RENDERBUFFER:
    {
        TCU_CHECK(attachment.name != 0);
        Renderbuffer *rbo = m_renderbuffers.find(attachment.name);
        TCU_CHECK(rbo);
        m_renderbuffers.acquireReference(rbo);
        break;
    }

    default:
        break; // Silently ignore
    }
}

void ReferenceContext::releaseFboAttachmentReference(const Framebuffer::Attachment &attachment)
{
    switch (attachment.type)
    {
    case Framebuffer::ATTACHMENTTYPE_TEXTURE:
    {
        TCU_CHECK(attachment.name != 0);
        Texture *texture = m_textures.find(attachment.name);
        TCU_CHECK(texture);
        m_textures.releaseReference(texture);
        break;
    }

    case Framebuffer::ATTACHMENTTYPE_RENDERBUFFER:
    {
        TCU_CHECK(attachment.name != 0);
        Renderbuffer *rbo = m_renderbuffers.find(attachment.name);
        TCU_CHECK(rbo);
        m_renderbuffers.releaseReference(rbo);
        break;
    }

    default:
        break; // Silently ignore
    }
}

void ReferenceContext::framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                            int level)
{
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        // Attach to both depth and stencil.
        framebufferTexture2D(target, GL_DEPTH_ATTACHMENT, textarget, texture, level);
        framebufferTexture2D(target, GL_STENCIL_ATTACHMENT, textarget, texture, level);
    }
    else
    {
        Framebuffer::AttachmentPoint point  = mapGLAttachmentPoint(attachment);
        Texture *texObj                     = nullptr;
        Framebuffer::TexTarget fboTexTarget = mapGLFboTexTarget(textarget);

        RC_IF_ERROR(target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER,
                    GL_INVALID_ENUM, RC_RET_VOID);
        RC_IF_ERROR(point == Framebuffer::ATTACHMENTPOINT_LAST, GL_INVALID_ENUM, RC_RET_VOID);

        // Select binding point.
        rc::Framebuffer *framebufferBinding = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) ?
                                                  m_drawFramebufferBinding :
                                                  m_readFramebufferBinding;
        RC_IF_ERROR(!framebufferBinding, GL_INVALID_OPERATION, RC_RET_VOID);

        // If framebuffer object is bound for both reading and writing then we need to acquire/release multiple references.
        int bindingRefCount = (framebufferBinding == m_drawFramebufferBinding ? 1 : 0) +
                              (framebufferBinding == m_readFramebufferBinding ? 1 : 0);

        if (texture != 0)
        {
            texObj = m_textures.find(texture);

            RC_IF_ERROR(!texObj, GL_INVALID_OPERATION, RC_RET_VOID);
            RC_IF_ERROR(level != 0, GL_INVALID_VALUE,
                        RC_RET_VOID); // \todo [2012-03-19 pyry] We should allow other levels as well.

            if (texObj->getType() == Texture::TYPE_2D)
                RC_IF_ERROR(fboTexTarget != Framebuffer::TEXTARGET_2D, GL_INVALID_OPERATION, RC_RET_VOID);
            else
            {
                TCU_CHECK(texObj->getType() == Texture::TYPE_CUBE_MAP);
                if (!deInRange32(fboTexTarget, Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_X,
                                 Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Z))
                    RC_ERROR_RET(GL_INVALID_OPERATION, RC_RET_VOID);
            }
        }

        Framebuffer::Attachment &fboAttachment = framebufferBinding->getAttachment(point);
        for (int ndx = 0; ndx < bindingRefCount; ndx++)
            releaseFboAttachmentReference(fboAttachment);
        fboAttachment = Framebuffer::Attachment();

        if (texObj)
        {
            fboAttachment.type      = Framebuffer::ATTACHMENTTYPE_TEXTURE;
            fboAttachment.name      = texObj->getName();
            fboAttachment.texTarget = fboTexTarget;
            fboAttachment.level     = level;

            for (int ndx = 0; ndx < bindingRefCount; ndx++)
                acquireFboAttachmentReference(fboAttachment);
        }
    }
}

void ReferenceContext::framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level,
                                               int layer)
{
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        // Attach to both depth and stencil.
        framebufferTextureLayer(target, GL_DEPTH_ATTACHMENT, texture, level, layer);
        framebufferTextureLayer(target, GL_STENCIL_ATTACHMENT, texture, level, layer);
    }
    else
    {
        Framebuffer::AttachmentPoint point = mapGLAttachmentPoint(attachment);
        Texture *texObj                    = nullptr;

        RC_IF_ERROR(target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER,
                    GL_INVALID_ENUM, RC_RET_VOID);
        RC_IF_ERROR(point == Framebuffer::ATTACHMENTPOINT_LAST, GL_INVALID_ENUM, RC_RET_VOID);

        // Select binding point.
        rc::Framebuffer *framebufferBinding = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) ?
                                                  m_drawFramebufferBinding :
                                                  m_readFramebufferBinding;
        RC_IF_ERROR(!framebufferBinding, GL_INVALID_OPERATION, RC_RET_VOID);

        // If framebuffer object is bound for both reading and writing then we need to acquire/release multiple references.
        int bindingRefCount = (framebufferBinding == m_drawFramebufferBinding ? 1 : 0) +
                              (framebufferBinding == m_readFramebufferBinding ? 1 : 0);

        if (texture != 0)
        {
            texObj = m_textures.find(texture);

            RC_IF_ERROR(!texObj, GL_INVALID_OPERATION, RC_RET_VOID);
            RC_IF_ERROR(level != 0, GL_INVALID_VALUE,
                        RC_RET_VOID); // \todo [2012-03-19 pyry] We should allow other levels as well.

            RC_IF_ERROR(texObj->getType() != Texture::TYPE_2D_ARRAY && texObj->getType() != Texture::TYPE_3D &&
                            texObj->getType() != Texture::TYPE_CUBE_MAP_ARRAY,
                        GL_INVALID_OPERATION, RC_RET_VOID);

            if (texObj->getType() == Texture::TYPE_2D_ARRAY || texObj->getType() == Texture::TYPE_CUBE_MAP_ARRAY)
            {
                RC_IF_ERROR((layer < 0) || (layer >= GL_MAX_ARRAY_TEXTURE_LAYERS), GL_INVALID_VALUE, RC_RET_VOID);
                RC_IF_ERROR((level < 0) || (level > deLog2Floor32(GL_MAX_TEXTURE_SIZE)), GL_INVALID_VALUE, RC_RET_VOID);
            }
            else if (texObj->getType() == Texture::TYPE_3D)
            {
                RC_IF_ERROR((layer < 0) || (layer >= GL_MAX_3D_TEXTURE_SIZE), GL_INVALID_VALUE, RC_RET_VOID);
                RC_IF_ERROR((level < 0) || (level > deLog2Floor32(GL_MAX_3D_TEXTURE_SIZE)), GL_INVALID_VALUE,
                            RC_RET_VOID);
            }
        }

        Framebuffer::Attachment &fboAttachment = framebufferBinding->getAttachment(point);
        for (int ndx = 0; ndx < bindingRefCount; ndx++)
            releaseFboAttachmentReference(fboAttachment);
        fboAttachment = Framebuffer::Attachment();

        if (texObj)
        {
            fboAttachment.type      = Framebuffer::ATTACHMENTTYPE_TEXTURE;
            fboAttachment.name      = texObj->getName();
            fboAttachment.texTarget = texLayeredTypeToTarget(texObj->getType());
            fboAttachment.level     = level;
            fboAttachment.layer     = layer;

            DE_ASSERT(fboAttachment.texTarget != Framebuffer::TEXTARGET_LAST);

            for (int ndx = 0; ndx < bindingRefCount; ndx++)
                acquireFboAttachmentReference(fboAttachment);
        }
    }
}

void ReferenceContext::framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                               uint32_t renderbuffer)
{
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        // Attach both to depth and stencil.
        framebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, renderbuffer);
        framebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, renderbuffer);
    }
    else
    {
        Framebuffer::AttachmentPoint point = mapGLAttachmentPoint(attachment);
        Renderbuffer *rbo                  = nullptr;

        RC_IF_ERROR(target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER,
                    GL_INVALID_ENUM, RC_RET_VOID);
        RC_IF_ERROR(point == Framebuffer::ATTACHMENTPOINT_LAST, GL_INVALID_ENUM, RC_RET_VOID);

        // Select binding point.
        rc::Framebuffer *framebufferBinding = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) ?
                                                  m_drawFramebufferBinding :
                                                  m_readFramebufferBinding;
        RC_IF_ERROR(!framebufferBinding, GL_INVALID_OPERATION, RC_RET_VOID);

        // If framebuffer object is bound for both reading and writing then we need to acquire/release multiple references.
        int bindingRefCount = (framebufferBinding == m_drawFramebufferBinding ? 1 : 0) +
                              (framebufferBinding == m_readFramebufferBinding ? 1 : 0);

        if (renderbuffer != 0)
        {
            rbo = m_renderbuffers.find(renderbuffer);

            RC_IF_ERROR(renderbuffertarget != GL_RENDERBUFFER, GL_INVALID_ENUM, RC_RET_VOID);
            RC_IF_ERROR(!rbo, GL_INVALID_OPERATION, RC_RET_VOID);
        }

        Framebuffer::Attachment &fboAttachment = framebufferBinding->getAttachment(point);
        for (int ndx = 0; ndx < bindingRefCount; ndx++)
            releaseFboAttachmentReference(fboAttachment);
        fboAttachment = Framebuffer::Attachment();

        if (rbo)
        {
            fboAttachment.type = Framebuffer::ATTACHMENTTYPE_RENDERBUFFER;
            fboAttachment.name = rbo->getName();

            for (int ndx = 0; ndx < bindingRefCount; ndx++)
                acquireFboAttachmentReference(fboAttachment);
        }
    }
}

uint32_t ReferenceContext::checkFramebufferStatus(uint32_t target)
{
    RC_IF_ERROR(target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER,
                GL_INVALID_ENUM, 0);

    // Select binding point.
    rc::Framebuffer *framebufferBinding = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) ?
                                              m_drawFramebufferBinding :
                                              m_readFramebufferBinding;

    // Default framebuffer is always complete.
    if (!framebufferBinding)
        return GL_FRAMEBUFFER_COMPLETE;

    int width               = -1;
    int height              = -1;
    bool hasAttachment      = false;
    bool attachmentComplete = true;
    bool dimensionsOk       = true;

    for (int point = 0; point < Framebuffer::ATTACHMENTPOINT_LAST; point++)
    {
        const Framebuffer::Attachment &attachment =
            framebufferBinding->getAttachment((Framebuffer::AttachmentPoint)point);
        int attachmentWidth  = 0;
        int attachmentHeight = 0;
        tcu::TextureFormat attachmentFormat;

        if (attachment.type == Framebuffer::ATTACHMENTTYPE_TEXTURE)
        {
            const Texture *texture = m_textures.find(attachment.name);
            tcu::ConstPixelBufferAccess level;
            TCU_CHECK(texture);

            if (attachment.texTarget == Framebuffer::TEXTARGET_2D)
            {
                DE_ASSERT(texture->getType() == Texture::TYPE_2D);
                const Texture2D *tex2D = static_cast<const Texture2D *>(texture);

                if (tex2D->hasLevel(attachment.level))
                    level = tex2D->getLevel(attachment.level);
            }
            else if (deInRange32(attachment.texTarget, Framebuffer::TEXTARGET_CUBE_MAP_POSITIVE_X,
                                 Framebuffer::TEXTARGET_CUBE_MAP_NEGATIVE_Z))
            {
                DE_ASSERT(texture->getType() == Texture::TYPE_CUBE_MAP);

                const TextureCube *texCube = static_cast<const TextureCube *>(texture);
                const tcu::CubeFace face   = texTargetToFace(attachment.texTarget);
                TCU_CHECK(de::inBounds<int>(face, 0, tcu::CUBEFACE_LAST));

                if (texCube->hasFace(attachment.level, face))
                    level = texCube->getFace(attachment.level, face);
            }
            else if (attachment.texTarget == Framebuffer::TEXTARGET_2D_ARRAY)
            {
                DE_ASSERT(texture->getType() == Texture::TYPE_2D_ARRAY);
                const Texture2DArray *tex2DArr = static_cast<const Texture2DArray *>(texture);

                if (tex2DArr->hasLevel(attachment.level))
                    level = tex2DArr->getLevel(attachment.level); // \note Slice doesn't matter here.
            }
            else if (attachment.texTarget == Framebuffer::TEXTARGET_3D)
            {
                DE_ASSERT(texture->getType() == Texture::TYPE_3D);
                const Texture3D *tex3D = static_cast<const Texture3D *>(texture);

                if (tex3D->hasLevel(attachment.level))
                    level = tex3D->getLevel(attachment.level); // \note Slice doesn't matter here.
            }
            else if (attachment.texTarget == Framebuffer::TEXTARGET_CUBE_MAP_ARRAY)
            {
                DE_ASSERT(texture->getType() == Texture::TYPE_CUBE_MAP_ARRAY);
                const TextureCubeArray *texCubeArr = static_cast<const TextureCubeArray *>(texture);

                if (texCubeArr->hasLevel(attachment.level))
                    level = texCubeArr->getLevel(attachment.level); // \note Slice doesn't matter here.
            }
            else
                TCU_FAIL("Framebuffer attached to a texture but no valid target specified");

            attachmentWidth  = level.getWidth();
            attachmentHeight = level.getHeight();
            attachmentFormat = level.getFormat();
        }
        else if (attachment.type == Framebuffer::ATTACHMENTTYPE_RENDERBUFFER)
        {
            const Renderbuffer *renderbuffer = m_renderbuffers.find(attachment.name);
            TCU_CHECK(renderbuffer);

            attachmentWidth  = renderbuffer->getWidth();
            attachmentHeight = renderbuffer->getHeight();
            attachmentFormat = renderbuffer->getFormat();
        }
        else
        {
            TCU_CHECK(attachment.type == Framebuffer::ATTACHMENTTYPE_LAST);
            continue; // Skip rest of checks.
        }

        if (!hasAttachment && attachmentWidth > 0 && attachmentHeight > 0)
        {
            width         = attachmentWidth;
            height        = attachmentHeight;
            hasAttachment = true;
        }
        else if (attachmentWidth != width || attachmentHeight != height)
            dimensionsOk = false;

        // Validate attachment point compatibility.
        switch (attachmentFormat.order)
        {
        case TextureFormat::R:
        case TextureFormat::RG:
        case TextureFormat::RGB:
        case TextureFormat::RGBA:
        case TextureFormat::sRGB:
        case TextureFormat::sRGBA:
        case TextureFormat::BGRA:
            if (point != Framebuffer::ATTACHMENTPOINT_COLOR0)
                attachmentComplete = false;
            break;

        case TextureFormat::D:
            if (point != Framebuffer::ATTACHMENTPOINT_DEPTH)
                attachmentComplete = false;
            break;

        case TextureFormat::S:
            if (point != Framebuffer::ATTACHMENTPOINT_STENCIL)
                attachmentComplete = false;
            break;

        case TextureFormat::DS:
            if (point != Framebuffer::ATTACHMENTPOINT_DEPTH && point != Framebuffer::ATTACHMENTPOINT_STENCIL)
                attachmentComplete = false;
            break;

        default:
            TCU_FAIL("Unsupported attachment channel order");
        }
    }

    if (!attachmentComplete)
        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    else if (!hasAttachment)
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    else if (!dimensionsOk)
        return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    else
        return GL_FRAMEBUFFER_COMPLETE;
}

void ReferenceContext::getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname,
                                                           int *params)
{
    DE_UNREF(target && attachment && pname && params);
    TCU_CHECK(false); // \todo [pyry] Implement
}

void ReferenceContext::renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height)
{
    TextureFormat format = glu::mapGLInternalFormat(internalformat);

    RC_IF_ERROR(target != GL_RENDERBUFFER, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(!m_renderbufferBinding, GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(!deInRange32(width, 0, m_limits.maxRenderbufferSize) ||
                    !deInRange32(height, 0, m_limits.maxRenderbufferSize),
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(format.order == TextureFormat::CHANNELORDER_LAST || format.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    m_renderbufferBinding->setStorage(format, (int)width, (int)height);
}

void ReferenceContext::renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                                      int height)
{
    // \todo [2012-04-07 pyry] Implement MSAA support.
    DE_UNREF(samples);
    renderbufferStorage(target, internalFormat, width, height);
}

tcu::PixelBufferAccess ReferenceContext::getFboAttachment(const rc::Framebuffer &framebuffer,
                                                          rc::Framebuffer::AttachmentPoint point)
{
    const Framebuffer::Attachment &attachment = framebuffer.getAttachment(point);

    switch (attachment.type)
    {
    case Framebuffer::ATTACHMENTTYPE_TEXTURE:
    {
        Texture *texture = m_textures.find(attachment.name);
        TCU_CHECK(texture);

        if (texture->getType() == Texture::TYPE_2D)
        {
            if (Texture2D *texture2D = dynamic_cast<Texture2D *>(texture))
                return texture2D->getLevel(attachment.level);
            else
                return nullAccess();
        }
        else if (texture->getType() == Texture::TYPE_CUBE_MAP)
        {
            if (TextureCube *cubeMap = dynamic_cast<TextureCube *>(texture))
                return cubeMap->getFace(attachment.level, texTargetToFace(attachment.texTarget));
            else
                return nullAccess();
        }
        else if (texture->getType() == Texture::TYPE_2D_ARRAY || texture->getType() == Texture::TYPE_3D ||
                 texture->getType() == Texture::TYPE_CUBE_MAP_ARRAY)
        {
            tcu::PixelBufferAccess level;

            if (texture->getType() == Texture::TYPE_2D_ARRAY)
            {
                if (Texture2DArray *texture2DArray = dynamic_cast<Texture2DArray *>(texture))
                    level = texture2DArray->getLevel(attachment.level);
            }
            else if (texture->getType() == Texture::TYPE_3D)
            {
                if (Texture3D *texture3D = dynamic_cast<Texture3D *>(texture))
                    level = texture3D->getLevel(attachment.level);
            }
            else if (texture->getType() == Texture::TYPE_CUBE_MAP_ARRAY)
            {
                if (TextureCubeArray *cubeArray = dynamic_cast<TextureCubeArray *>(texture))
                    level = cubeArray->getLevel(attachment.level);
            }

            void *layerData = static_cast<uint8_t *>(level.getDataPtr()) + level.getSlicePitch() * attachment.layer;

            return tcu::PixelBufferAccess(level.getFormat(), level.getWidth(), level.getHeight(), 1,
                                          level.getRowPitch(), 0, layerData);
        }
        else
            return nullAccess();
    }

    case Framebuffer::ATTACHMENTTYPE_RENDERBUFFER:
    {
        Renderbuffer *rbo = m_renderbuffers.find(attachment.name);
        TCU_CHECK(rbo);

        return rbo->getAccess();
    }

    default:
        return nullAccess();
    }
}

const Texture2D &ReferenceContext::getTexture2D(int unitNdx) const
{
    const TextureUnit &unit = m_textureUnits[unitNdx];
    return unit.tex2DBinding ? *unit.tex2DBinding : unit.default2DTex;
}

const TextureCube &ReferenceContext::getTextureCube(int unitNdx) const
{
    const TextureUnit &unit = m_textureUnits[unitNdx];
    return unit.texCubeBinding ? *unit.texCubeBinding : unit.defaultCubeTex;
}

static bool isValidBufferTarget(uint32_t target)
{
    switch (target)
    {
    case GL_ARRAY_BUFFER:
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_DRAW_INDIRECT_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
        return true;

    default:
        return false;
    }
}

void ReferenceContext::setBufferBinding(uint32_t target, DataBuffer *buffer)
{
    DataBuffer **bindingPoint      = nullptr;
    VertexArray *vertexArrayObject = (m_vertexArrayBinding) ? (m_vertexArrayBinding) : (&m_clientVertexArray);

    switch (target)
    {
    case GL_ARRAY_BUFFER:
        bindingPoint = &m_arrayBufferBinding;
        break;
    case GL_COPY_READ_BUFFER:
        bindingPoint = &m_copyReadBufferBinding;
        break;
    case GL_COPY_WRITE_BUFFER:
        bindingPoint = &m_copyWriteBufferBinding;
        break;
    case GL_DRAW_INDIRECT_BUFFER:
        bindingPoint = &m_drawIndirectBufferBinding;
        break;
    case GL_ELEMENT_ARRAY_BUFFER:
        bindingPoint = &vertexArrayObject->m_elementArrayBufferBinding;
        break;
    case GL_PIXEL_PACK_BUFFER:
        bindingPoint = &m_pixelPackBufferBinding;
        break;
    case GL_PIXEL_UNPACK_BUFFER:
        bindingPoint = &m_pixelUnpackBufferBinding;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        bindingPoint = &m_transformFeedbackBufferBinding;
        break;
    case GL_UNIFORM_BUFFER:
        bindingPoint = &m_uniformBufferBinding;
        break;
    default:
        DE_ASSERT(false);
        return;
    }

    if (*bindingPoint)
    {
        m_buffers.releaseReference(*bindingPoint);
        *bindingPoint = nullptr;
    }

    if (buffer)
        m_buffers.acquireReference(buffer);

    *bindingPoint = buffer;
}

DataBuffer *ReferenceContext::getBufferBinding(uint32_t target) const
{
    const VertexArray *vertexArrayObject = (m_vertexArrayBinding) ? (m_vertexArrayBinding) : (&m_clientVertexArray);

    switch (target)
    {
    case GL_ARRAY_BUFFER:
        return m_arrayBufferBinding;
    case GL_COPY_READ_BUFFER:
        return m_copyReadBufferBinding;
    case GL_COPY_WRITE_BUFFER:
        return m_copyWriteBufferBinding;
    case GL_DRAW_INDIRECT_BUFFER:
        return m_drawIndirectBufferBinding;
    case GL_ELEMENT_ARRAY_BUFFER:
        return vertexArrayObject->m_elementArrayBufferBinding;
    case GL_PIXEL_PACK_BUFFER:
        return m_pixelPackBufferBinding;
    case GL_PIXEL_UNPACK_BUFFER:
        return m_pixelUnpackBufferBinding;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        return m_transformFeedbackBufferBinding;
    case GL_UNIFORM_BUFFER:
        return m_uniformBufferBinding;
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void ReferenceContext::bindBuffer(uint32_t target, uint32_t buffer)
{
    RC_IF_ERROR(!isValidBufferTarget(target), GL_INVALID_ENUM, RC_RET_VOID);

    rc::DataBuffer *bufObj = nullptr;

    if (buffer != 0)
    {
        bufObj = m_buffers.find(buffer);
        if (!bufObj)
        {
            bufObj = new DataBuffer(buffer);
            m_buffers.insert(bufObj);
        }
    }

    setBufferBinding(target, bufObj);
}

void ReferenceContext::genBuffers(int numBuffers, uint32_t *buffers)
{
    RC_IF_ERROR(!buffers, GL_INVALID_VALUE, RC_RET_VOID);

    for (int ndx = 0; ndx < numBuffers; ndx++)
        buffers[ndx] = m_buffers.allocateName();
}

void ReferenceContext::deleteBuffers(int numBuffers, const uint32_t *buffers)
{
    RC_IF_ERROR(numBuffers < 0, GL_INVALID_VALUE, RC_RET_VOID);

    for (int ndx = 0; ndx < numBuffers; ndx++)
    {
        uint32_t buffer    = buffers[ndx];
        DataBuffer *bufObj = nullptr;

        if (buffer == 0)
            continue;

        bufObj = m_buffers.find(buffer);

        if (bufObj)
            deleteBuffer(bufObj);
    }
}

void ReferenceContext::deleteBuffer(DataBuffer *buffer)
{
    static const uint32_t bindingPoints[] = {
        GL_ARRAY_BUFFER,         GL_COPY_READ_BUFFER,          GL_COPY_WRITE_BUFFER,
        GL_DRAW_INDIRECT_BUFFER, GL_ELEMENT_ARRAY_BUFFER,      GL_PIXEL_PACK_BUFFER,
        GL_PIXEL_UNPACK_BUFFER,  GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

    for (int bindingNdx = 0; bindingNdx < DE_LENGTH_OF_ARRAY(bindingPoints); bindingNdx++)
    {
        if (getBufferBinding(bindingPoints[bindingNdx]) == buffer)
            setBufferBinding(bindingPoints[bindingNdx], nullptr);
    }

    {
        vector<VertexArray *> vertexArrays;
        m_vertexArrays.getAll(vertexArrays);
        vertexArrays.push_back(&m_clientVertexArray);

        for (vector<VertexArray *>::iterator i = vertexArrays.begin(); i != vertexArrays.end(); i++)
        {
            if ((*i)->m_elementArrayBufferBinding == buffer)
            {
                m_buffers.releaseReference(buffer);
                (*i)->m_elementArrayBufferBinding = nullptr;
            }

            for (size_t vertexAttribNdx = 0; vertexAttribNdx < (*i)->m_arrays.size(); ++vertexAttribNdx)
            {
                if ((*i)->m_arrays[vertexAttribNdx].bufferBinding == buffer)
                {
                    m_buffers.releaseReference(buffer);
                    (*i)->m_arrays[vertexAttribNdx].bufferDeleted = true;
                    (*i)->m_arrays[vertexAttribNdx].bufferBinding = nullptr;
                }
            }
        }
    }

    DE_ASSERT(buffer->getRefCount() == 1);
    m_buffers.releaseReference(buffer);
}

void ReferenceContext::bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    RC_IF_ERROR(!isValidBufferTarget(target), GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(size < 0, GL_INVALID_VALUE, RC_RET_VOID);

    DE_UNREF(usage);

    DataBuffer *buffer = getBufferBinding(target);
    RC_IF_ERROR(!buffer, GL_INVALID_OPERATION, RC_RET_VOID);

    DE_ASSERT((intptr_t)(int)size == size);
    buffer->setStorage((int)size);
    if (data)
        deMemcpy(buffer->getData(), data, (int)size);
}

void ReferenceContext::bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    RC_IF_ERROR(!isValidBufferTarget(target), GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(offset < 0 || size < 0, GL_INVALID_VALUE, RC_RET_VOID);

    DataBuffer *buffer = getBufferBinding(target);

    RC_IF_ERROR(!buffer, GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR((int)(offset + size) > buffer->getSize(), GL_INVALID_VALUE, RC_RET_VOID);

    deMemcpy(buffer->getData() + offset, data, (int)size);
}

void ReferenceContext::clearColor(float red, float green, float blue, float alpha)
{
    m_clearColor = Vec4(de::clamp(red, 0.0f, 1.0f), de::clamp(green, 0.0f, 1.0f), de::clamp(blue, 0.0f, 1.0f),
                        de::clamp(alpha, 0.0f, 1.0f));
}

void ReferenceContext::clearDepthf(float depth)
{
    m_clearDepth = de::clamp(depth, 0.0f, 1.0f);
}

void ReferenceContext::clearStencil(int stencil)
{
    m_clearStencil = stencil;
}

void ReferenceContext::scissor(int x, int y, int width, int height)
{
    RC_IF_ERROR(width < 0 || height < 0, GL_INVALID_VALUE, RC_RET_VOID);
    m_scissorBox = IVec4(x, y, width, height);
}

void ReferenceContext::enable(uint32_t cap)
{
    switch (cap)
    {
    case GL_BLEND:
        m_blendEnabled = true;
        break;
    case GL_SCISSOR_TEST:
        m_scissorEnabled = true;
        break;
    case GL_DEPTH_TEST:
        m_depthTestEnabled = true;
        break;
    case GL_STENCIL_TEST:
        m_stencilTestEnabled = true;
        break;
    case GL_POLYGON_OFFSET_FILL:
        m_polygonOffsetFillEnabled = true;
        break;

    case GL_FRAMEBUFFER_SRGB:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_sRGBUpdateEnabled = true;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_DEPTH_CLAMP:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_depthClampEnabled = true;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_DITHER:
        // Not implemented - just ignored.
        break;

    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        if (!glu::isContextTypeGLCore(getType()))
        {
            m_primitiveRestartFixedIndex = true;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_PRIMITIVE_RESTART:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_primitiveRestartSettableIndex = true;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    default:
        setError(GL_INVALID_ENUM);
        break;
    }
}

void ReferenceContext::disable(uint32_t cap)
{
    switch (cap)
    {
    case GL_BLEND:
        m_blendEnabled = false;
        break;
    case GL_SCISSOR_TEST:
        m_scissorEnabled = false;
        break;
    case GL_DEPTH_TEST:
        m_depthTestEnabled = false;
        break;
    case GL_STENCIL_TEST:
        m_stencilTestEnabled = false;
        break;
    case GL_POLYGON_OFFSET_FILL:
        m_polygonOffsetFillEnabled = false;
        break;

    case GL_FRAMEBUFFER_SRGB:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_sRGBUpdateEnabled = false;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_DEPTH_CLAMP:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_depthClampEnabled = false;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_DITHER:
        break;

    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        if (!glu::isContextTypeGLCore(getType()))
        {
            m_primitiveRestartFixedIndex = false;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    case GL_PRIMITIVE_RESTART:
        if (glu::isContextTypeGLCore(getType()))
        {
            m_primitiveRestartSettableIndex = false;
            break;
        }
        setError(GL_INVALID_ENUM);
        break;

    default:
        setError(GL_INVALID_ENUM);
        break;
    }
}

static bool isValidCompareFunc(uint32_t func)
{
    switch (func)
    {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_GEQUAL:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
        return true;

    default:
        return false;
    }
}

static bool isValidStencilOp(uint32_t op)
{
    switch (op)
    {
    case GL_KEEP:
    case GL_ZERO:
    case GL_REPLACE:
    case GL_INCR:
    case GL_INCR_WRAP:
    case GL_DECR:
    case GL_DECR_WRAP:
    case GL_INVERT:
        return true;

    default:
        return false;
    }
}

void ReferenceContext::stencilFunc(uint32_t func, int ref, uint32_t mask)
{
    stencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void ReferenceContext::stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask)
{
    const bool setFront = face == GL_FRONT || face == GL_FRONT_AND_BACK;
    const bool setBack  = face == GL_BACK || face == GL_FRONT_AND_BACK;

    RC_IF_ERROR(!isValidCompareFunc(func), GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(!setFront && !setBack, GL_INVALID_ENUM, RC_RET_VOID);

    for (int type = 0; type < rr::FACETYPE_LAST; ++type)
    {
        if ((type == rr::FACETYPE_FRONT && setFront) || (type == rr::FACETYPE_BACK && setBack))
        {
            m_stencil[type].func   = func;
            m_stencil[type].ref    = ref;
            m_stencil[type].opMask = mask;
        }
    }
}

void ReferenceContext::stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass)
{
    stencilOpSeparate(GL_FRONT_AND_BACK, sfail, dpfail, dppass);
}

void ReferenceContext::stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass)
{
    const bool setFront = face == GL_FRONT || face == GL_FRONT_AND_BACK;
    const bool setBack  = face == GL_BACK || face == GL_FRONT_AND_BACK;

    RC_IF_ERROR(!isValidStencilOp(sfail) || !isValidStencilOp(dpfail) || !isValidStencilOp(dppass), GL_INVALID_ENUM,
                RC_RET_VOID);
    RC_IF_ERROR(!setFront && !setBack, GL_INVALID_ENUM, RC_RET_VOID);

    for (int type = 0; type < rr::FACETYPE_LAST; ++type)
    {
        if ((type == rr::FACETYPE_FRONT && setFront) || (type == rr::FACETYPE_BACK && setBack))
        {
            m_stencil[type].opStencilFail = sfail;
            m_stencil[type].opDepthFail   = dpfail;
            m_stencil[type].opDepthPass   = dppass;
        }
    }
}

void ReferenceContext::depthFunc(uint32_t func)
{
    RC_IF_ERROR(!isValidCompareFunc(func), GL_INVALID_ENUM, RC_RET_VOID);
    m_depthFunc = func;
}

void ReferenceContext::depthRangef(float n, float f)
{
    m_depthRangeNear = de::clamp(n, 0.0f, 1.0f);
    m_depthRangeFar  = de::clamp(f, 0.0f, 1.0f);
}

void ReferenceContext::depthRange(double n, double f)
{
    depthRangef((float)n, (float)f);
}

void ReferenceContext::polygonOffset(float factor, float units)
{
    m_polygonOffsetFactor = factor;
    m_polygonOffsetUnits  = units;
}

void ReferenceContext::provokingVertex(uint32_t convention)
{
    // only in core
    DE_ASSERT(glu::isContextTypeGLCore(getType()));

    switch (convention)
    {
    case GL_FIRST_VERTEX_CONVENTION:
        m_provokingFirstVertexConvention = true;
        break;
    case GL_LAST_VERTEX_CONVENTION:
        m_provokingFirstVertexConvention = false;
        break;

    default:
        RC_ERROR_RET(GL_INVALID_ENUM, RC_RET_VOID);
    }
}

void ReferenceContext::primitiveRestartIndex(uint32_t index)
{
    // only in core
    DE_ASSERT(glu::isContextTypeGLCore(getType()));
    m_primitiveRestartIndex = index;
}

static inline bool isValidBlendEquation(uint32_t mode)
{
    return mode == GL_FUNC_ADD || mode == GL_FUNC_SUBTRACT || mode == GL_FUNC_REVERSE_SUBTRACT || mode == GL_MIN ||
           mode == GL_MAX;
}

static bool isValidBlendFactor(uint32_t factor)
{
    switch (factor)
    {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR:
    case GL_CONSTANT_ALPHA:
    case GL_ONE_MINUS_CONSTANT_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        return true;

    default:
        return false;
    }
}

void ReferenceContext::blendEquation(uint32_t mode)
{
    RC_IF_ERROR(!isValidBlendEquation(mode), GL_INVALID_ENUM, RC_RET_VOID);

    m_blendModeRGB   = mode;
    m_blendModeAlpha = mode;
}

void ReferenceContext::blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha)
{
    RC_IF_ERROR(!isValidBlendEquation(modeRGB) || !isValidBlendEquation(modeAlpha), GL_INVALID_ENUM, RC_RET_VOID);

    m_blendModeRGB   = modeRGB;
    m_blendModeAlpha = modeAlpha;
}

void ReferenceContext::blendFunc(uint32_t src, uint32_t dst)
{
    RC_IF_ERROR(!isValidBlendFactor(src) || !isValidBlendFactor(dst), GL_INVALID_ENUM, RC_RET_VOID);

    m_blendFactorSrcRGB   = src;
    m_blendFactorSrcAlpha = src;
    m_blendFactorDstRGB   = dst;
    m_blendFactorDstAlpha = dst;
}

void ReferenceContext::blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha)
{
    RC_IF_ERROR(!isValidBlendFactor(srcRGB) || !isValidBlendFactor(dstRGB) || !isValidBlendFactor(srcAlpha) ||
                    !isValidBlendFactor(dstAlpha),
                GL_INVALID_ENUM, RC_RET_VOID);

    m_blendFactorSrcRGB   = srcRGB;
    m_blendFactorSrcAlpha = srcAlpha;
    m_blendFactorDstRGB   = dstRGB;
    m_blendFactorDstAlpha = dstAlpha;
}

void ReferenceContext::blendColor(float red, float green, float blue, float alpha)
{
    m_blendColor = Vec4(de::clamp(red, 0.0f, 1.0f), de::clamp(green, 0.0f, 1.0f), de::clamp(blue, 0.0f, 1.0f),
                        de::clamp(alpha, 0.0f, 1.0f));
}

void ReferenceContext::colorMask(bool r, bool g, bool b, bool a)
{
    m_colorMask = tcu::BVec4(!!r, !!g, !!b, !!a);
}

void ReferenceContext::depthMask(bool mask)
{
    m_depthMask = !!mask;
}

void ReferenceContext::stencilMask(uint32_t mask)
{
    stencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void ReferenceContext::stencilMaskSeparate(uint32_t face, uint32_t mask)
{
    const bool setFront = face == GL_FRONT || face == GL_FRONT_AND_BACK;
    const bool setBack  = face == GL_BACK || face == GL_FRONT_AND_BACK;

    RC_IF_ERROR(!setFront && !setBack, GL_INVALID_ENUM, RC_RET_VOID);

    if (setFront)
        m_stencil[rr::FACETYPE_FRONT].writeMask = mask;
    if (setBack)
        m_stencil[rr::FACETYPE_BACK].writeMask = mask;
}

static int getNumStencilBits(const tcu::TextureFormat &format)
{
    switch (format.order)
    {
    case tcu::TextureFormat::S:
        switch (format.type)
        {
        case tcu::TextureFormat::UNSIGNED_INT8:
            return 8;
        case tcu::TextureFormat::UNSIGNED_INT16:
            return 16;
        case tcu::TextureFormat::UNSIGNED_INT32:
            return 32;
        default:
            DE_ASSERT(false);
            return 0;
        }

    case tcu::TextureFormat::DS:
        switch (format.type)
        {
        case tcu::TextureFormat::UNSIGNED_INT_24_8:
            return 8;
        case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
            return 8;
        default:
            DE_ASSERT(false);
            return 0;
        }

    default:
        DE_ASSERT(false);
        return 0;
    }
}

static inline uint32_t maskStencil(int numBits, uint32_t s)
{
    return s & deBitMask32(0, numBits);
}

static inline void writeMaskedStencil(const rr::MultisamplePixelBufferAccess &access, int s, int x, int y,
                                      uint32_t stencil, uint32_t writeMask)
{
    DE_ASSERT(access.raw().getFormat().order == tcu::TextureFormat::S);

    const uint32_t oldVal = access.raw().getPixelUint(s, x, y).x();
    const uint32_t newVal = (oldVal & ~writeMask) | (stencil & writeMask);
    access.raw().setPixel(tcu::UVec4(newVal, 0u, 0u, 0u), s, x, y);
}

static inline void writeDepthOnly(const rr::MultisamplePixelBufferAccess &access, int s, int x, int y, float depth)
{
    access.raw().setPixDepth(depth, s, x, y);
}

static rr::MultisamplePixelBufferAccess getDepthMultisampleAccess(
    const rr::MultisamplePixelBufferAccess &combinedDSaccess)
{
    return rr::MultisamplePixelBufferAccess::fromMultisampleAccess(
        tcu::getEffectiveDepthStencilAccess(combinedDSaccess.raw(), tcu::Sampler::MODE_DEPTH));
}

static rr::MultisamplePixelBufferAccess getStencilMultisampleAccess(
    const rr::MultisamplePixelBufferAccess &combinedDSaccess)
{
    return rr::MultisamplePixelBufferAccess::fromMultisampleAccess(
        tcu::getEffectiveDepthStencilAccess(combinedDSaccess.raw(), tcu::Sampler::MODE_STENCIL));
}

uint32_t ReferenceContext::blitResolveMultisampleFramebuffer(uint32_t mask, const IVec4 &srcRect, const IVec4 &dstRect,
                                                             bool flipX, bool flipY)
{
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        rr::MultisampleConstPixelBufferAccess src =
            rr::getSubregion(getReadColorbuffer(), srcRect.x(), srcRect.y(), srcRect.z(), srcRect.w());
        tcu::PixelBufferAccess dst        = tcu::getSubregion(getDrawColorbuffer().toSinglesampleAccess(), dstRect.x(),
                                                              dstRect.y(), dstRect.z(), dstRect.w());
        tcu::TextureChannelClass dstClass = tcu::getTextureChannelClass(dst.getFormat().type);
        bool dstIsFloat                   = dstClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                          dstClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                          dstClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
        bool srcIsSRGB         = tcu::isSRGB(src.raw().getFormat());
        bool dstIsSRGB         = tcu::isSRGB(dst.getFormat());
        const bool convertSRGB = m_sRGBUpdateEnabled && glu::isContextTypeES(getType());

        if (!convertSRGB)
        {
            tcu::ConstPixelBufferAccess srcRaw = src.raw();
            tcu::TextureFormat srcFmt          = toNonSRGBFormat(srcRaw.getFormat());

            srcRaw = tcu::ConstPixelBufferAccess(srcFmt, srcRaw.getWidth(), srcRaw.getHeight(), srcRaw.getDepth(),
                                                 srcRaw.getRowPitch(), srcRaw.getSlicePitch(), srcRaw.getDataPtr());
            src    = rr::MultisampleConstPixelBufferAccess::fromMultisampleAccess(srcRaw);

            dst = tcu::PixelBufferAccess(toNonSRGBFormat(dst.getFormat()), dst.getWidth(), dst.getHeight(),
                                         dst.getDepth(), dst.getRowPitch(), dst.getSlicePitch(), dst.getDataPtr());
        }

        for (int x = 0; x < dstRect.z(); ++x)
            for (int y = 0; y < dstRect.w(); ++y)
            {
                int srcX = (flipX) ? (srcRect.z() - x - 1) : (x);
                int srcY = (flipY) ? (srcRect.z() - y - 1) : (y);

                if (dstIsFloat || srcIsSRGB)
                {
                    Vec4 p = src.raw().getPixel(0, srcX, srcY);
                    dst.setPixel((dstIsSRGB && convertSRGB) ? tcu::linearToSRGB(p) : p, x, y);
                }
                else
                    dst.setPixel(src.raw().getPixelInt(0, srcX, srcY), x, y);
            }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        rr::MultisampleConstPixelBufferAccess src =
            rr::getSubregion(getReadDepthbuffer(), srcRect.x(), srcRect.y(), srcRect.z(), srcRect.w());
        rr::MultisamplePixelBufferAccess dst =
            rr::getSubregion(getDrawDepthbuffer(), dstRect.x(), dstRect.y(), dstRect.z(), dstRect.w());

        for (int x = 0; x < dstRect.z(); ++x)
            for (int y = 0; y < dstRect.w(); ++y)
            {
                int srcX = (flipX) ? (srcRect.z() - x - 1) : (x);
                int srcY = (flipY) ? (srcRect.z() - y - 1) : (y);

                writeDepthOnly(dst, 0, x, y, src.raw().getPixel(0, srcX, srcY).x());
            }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        rr::MultisampleConstPixelBufferAccess src = getStencilMultisampleAccess(
            rr::getSubregion(getReadStencilbuffer(), srcRect.x(), srcRect.y(), srcRect.z(), srcRect.w()));
        rr::MultisamplePixelBufferAccess dst = getStencilMultisampleAccess(
            rr::getSubregion(getDrawStencilbuffer(), dstRect.x(), dstRect.y(), dstRect.z(), dstRect.w()));

        for (int x = 0; x < dstRect.z(); ++x)
            for (int y = 0; y < dstRect.w(); ++y)
            {
                int srcX            = (flipX) ? (srcRect.z() - x - 1) : (x);
                int srcY            = (flipY) ? (srcRect.z() - y - 1) : (y);
                uint32_t srcStencil = src.raw().getPixelUint(0, srcX, srcY).x();

                writeMaskedStencil(dst, 0, x, y, srcStencil, m_stencil[rr::FACETYPE_FRONT].writeMask);
            }
    }

    return GL_NO_ERROR;
}

void ReferenceContext::blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1,
                                       int dstY1, uint32_t mask, uint32_t filter)
{
    // p0 in inclusive, p1 exclusive.
    // Negative width/height means swap.
    bool swapSrcX  = srcX1 < srcX0;
    bool swapSrcY  = srcY1 < srcY0;
    bool swapDstX  = dstX1 < dstX0;
    bool swapDstY  = dstY1 < dstY0;
    int srcW       = de::abs(srcX1 - srcX0);
    int srcH       = de::abs(srcY1 - srcY0);
    int dstW       = de::abs(dstX1 - dstX0);
    int dstH       = de::abs(dstY1 - dstY0);
    bool scale     = srcW != dstW || srcH != dstH;
    int srcOriginX = swapSrcX ? srcX1 : srcX0;
    int srcOriginY = swapSrcY ? srcY1 : srcY0;
    int dstOriginX = swapDstX ? dstX1 : dstX0;
    int dstOriginY = swapDstY ? dstY1 : dstY0;
    IVec4 srcRect  = IVec4(srcOriginX, srcOriginY, srcW, srcH);
    IVec4 dstRect  = IVec4(dstOriginX, dstOriginY, dstW, dstH);

    RC_IF_ERROR(filter != GL_NEAREST && filter != GL_LINEAR, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0 && filter != GL_NEAREST,
                GL_INVALID_OPERATION, RC_RET_VOID);

    // Validate that both targets are complete.
    RC_IF_ERROR(checkFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
                    checkFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE,
                GL_INVALID_OPERATION, RC_RET_VOID);

    // Check samples count is valid
    RC_IF_ERROR(getDrawColorbuffer().getNumSamples() != 1, GL_INVALID_OPERATION, RC_RET_VOID);

    // Check size restrictions of multisampled case
    if (getReadColorbuffer().getNumSamples() != 1)
    {
        // Src and Dst rect dimensions must be the same
        RC_IF_ERROR(srcW != dstW || srcH != dstH, GL_INVALID_OPERATION, RC_RET_VOID);

        // Framebuffer formats must match
        if (mask & GL_COLOR_BUFFER_BIT)
            RC_IF_ERROR(getReadColorbuffer().raw().getFormat() != getDrawColorbuffer().raw().getFormat(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        if (mask & GL_DEPTH_BUFFER_BIT)
            RC_IF_ERROR(getReadDepthbuffer().raw().getFormat() != getDrawDepthbuffer().raw().getFormat(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
        if (mask & GL_STENCIL_BUFFER_BIT)
            RC_IF_ERROR(getReadStencilbuffer().raw().getFormat() != getDrawStencilbuffer().raw().getFormat(),
                        GL_INVALID_OPERATION, RC_RET_VOID);
    }

    // Compute actual source rect.
    srcRect = (mask & GL_COLOR_BUFFER_BIT) ? intersect(srcRect, getBufferRect(getReadColorbuffer())) : srcRect;
    srcRect = (mask & GL_DEPTH_BUFFER_BIT) ? intersect(srcRect, getBufferRect(getReadDepthbuffer())) : srcRect;
    srcRect = (mask & GL_STENCIL_BUFFER_BIT) ? intersect(srcRect, getBufferRect(getReadStencilbuffer())) : srcRect;

    // Compute destination rect.
    dstRect = (mask & GL_COLOR_BUFFER_BIT) ? intersect(dstRect, getBufferRect(getDrawColorbuffer())) : dstRect;
    dstRect = (mask & GL_DEPTH_BUFFER_BIT) ? intersect(dstRect, getBufferRect(getDrawDepthbuffer())) : dstRect;
    dstRect = (mask & GL_STENCIL_BUFFER_BIT) ? intersect(dstRect, getBufferRect(getDrawStencilbuffer())) : dstRect;
    dstRect = m_scissorEnabled ? intersect(dstRect, m_scissorBox) : dstRect;

    if (isEmpty(srcRect) || isEmpty(dstRect))
        return; // Don't attempt copy.

    // Multisampled read buffer is a special case
    if (getReadColorbuffer().getNumSamples() != 1)
    {
        uint32_t error =
            blitResolveMultisampleFramebuffer(mask, srcRect, dstRect, swapSrcX ^ swapDstX, swapSrcY ^ swapDstY);

        if (error != GL_NO_ERROR)
            setError(error);

        return;
    }

    // \note Multisample pixel buffers can now be accessed like non-multisampled because multisample read buffer case is already handled. => sample count must be 1

    // Coordinate transformation:
    // Dst offset space -> dst rectangle space -> src rectangle space -> src offset space.
    tcu::Mat3 transform = tcu::translationMatrix(Vec2((float)(srcX0 - srcRect.x()), (float)(srcY0 - srcRect.y()))) *
                          tcu::Mat3(Vec3((float)(srcX1 - srcX0) / (float)(dstX1 - dstX0),
                                         (float)(srcY1 - srcY0) / (float)(dstY1 - dstY0), 1.0f)) *
                          tcu::translationMatrix(Vec2((float)(dstRect.x() - dstX0), (float)(dstRect.y() - dstY0)));

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        tcu::ConstPixelBufferAccess src   = tcu::getSubregion(getReadColorbuffer().toSinglesampleAccess(), srcRect.x(),
                                                              srcRect.y(), srcRect.z(), srcRect.w());
        tcu::PixelBufferAccess dst        = tcu::getSubregion(getDrawColorbuffer().toSinglesampleAccess(), dstRect.x(),
                                                              dstRect.y(), dstRect.z(), dstRect.w());
        tcu::TextureChannelClass dstClass = tcu::getTextureChannelClass(dst.getFormat().type);
        bool dstIsFloat                   = dstClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
                          dstClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
                          dstClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
        tcu::Sampler::FilterMode sFilter =
            (scale && filter == GL_LINEAR) ? tcu::Sampler::LINEAR : tcu::Sampler::NEAREST;
        tcu::Sampler sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                             sFilter, sFilter, 0.0f /* lod threshold */, false /* non-normalized coords */);
        bool srcIsSRGB         = tcu::isSRGB(src.getFormat());
        bool dstIsSRGB         = tcu::isSRGB(dst.getFormat());
        const bool convertSRGB = m_sRGBUpdateEnabled && glu::isContextTypeES(getType());

        if (!convertSRGB)
        {
            src = tcu::ConstPixelBufferAccess(toNonSRGBFormat(src.getFormat()), src.getWidth(), src.getHeight(),
                                              src.getDepth(), src.getRowPitch(), src.getSlicePitch(), src.getDataPtr());
            dst = tcu::PixelBufferAccess(toNonSRGBFormat(dst.getFormat()), dst.getWidth(), dst.getHeight(),
                                         dst.getDepth(), dst.getRowPitch(), dst.getSlicePitch(), dst.getDataPtr());
        }

        // \note We don't check for unsupported conversions, unlike spec requires.

        for (int yo = 0; yo < dstRect.w(); yo++)
        {
            for (int xo = 0; xo < dstRect.z(); xo++)
            {
                float dX = (float)xo + 0.5f;
                float dY = (float)yo + 0.5f;

                // \note Only affine part is used.
                float sX = transform(0, 0) * dX + transform(0, 1) * dY + transform(0, 2);
                float sY = transform(1, 0) * dX + transform(1, 1) * dY + transform(1, 2);

                // do not copy pixels outside the modified source region (modified by buffer intersection)
                if (sX < 0.0f || sX >= (float)srcRect.z() || sY < 0.0f || sY >= (float)srcRect.w())
                    continue;

                if (dstIsFloat || srcIsSRGB || filter == tcu::Sampler::LINEAR)
                {
                    Vec4 p = src.sample2D(sampler, sampler.minFilter, sX, sY, 0);
                    dst.setPixel((dstIsSRGB && convertSRGB) ? tcu::linearToSRGB(p) : p, xo, yo);
                }
                else
                    dst.setPixel(src.getPixelInt(deFloorFloatToInt32(sX), deFloorFloatToInt32(sY)), xo, yo);
            }
        }
    }

    if ((mask & GL_DEPTH_BUFFER_BIT) && m_depthMask)
    {
        rr::MultisampleConstPixelBufferAccess src = getDepthMultisampleAccess(
            rr::getSubregion(getReadDepthbuffer(), srcRect.x(), srcRect.y(), srcRect.z(), srcRect.w()));
        rr::MultisamplePixelBufferAccess dst = getDepthMultisampleAccess(
            rr::getSubregion(getDrawDepthbuffer(), dstRect.x(), dstRect.y(), dstRect.z(), dstRect.w()));

        for (int yo = 0; yo < dstRect.w(); yo++)
        {
            for (int xo = 0; xo < dstRect.z(); xo++)
            {
                const int sampleNdx = 0; // multisample read buffer case is already handled

                float dX = (float)xo + 0.5f;
                float dY = (float)yo + 0.5f;
                float sX = transform(0, 0) * dX + transform(0, 1) * dY + transform(0, 2);
                float sY = transform(1, 0) * dX + transform(1, 1) * dY + transform(1, 2);

                writeDepthOnly(dst, sampleNdx, xo, yo,
                               src.raw().getPixDepth(sampleNdx, deFloorFloatToInt32(sX), deFloorFloatToInt32(sY)));
            }
        }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        rr::MultisampleConstPixelBufferAccess src = getStencilMultisampleAccess(
            rr::getSubregion(getReadStencilbuffer(), srcRect.x(), srcRect.y(), srcRect.z(), srcRect.w()));
        rr::MultisamplePixelBufferAccess dst = getStencilMultisampleAccess(
            rr::getSubregion(getDrawStencilbuffer(), dstRect.x(), dstRect.y(), dstRect.z(), dstRect.w()));

        for (int yo = 0; yo < dstRect.w(); yo++)
        {
            for (int xo = 0; xo < dstRect.z(); xo++)
            {
                const int sampleNdx = 0; // multisample read buffer case is already handled

                float dX = (float)xo + 0.5f;
                float dY = (float)yo + 0.5f;
                float sX = transform(0, 0) * dX + transform(0, 1) * dY + transform(0, 2);
                float sY = transform(1, 0) * dX + transform(1, 1) * dY + transform(1, 2);
                uint32_t srcStencil =
                    src.raw().getPixelUint(sampleNdx, deFloorFloatToInt32(sX), deFloorFloatToInt32(sY)).x();

                writeMaskedStencil(dst, sampleNdx, xo, yo, srcStencil, m_stencil[rr::FACETYPE_FRONT].writeMask);
            }
        }
    }
}

void ReferenceContext::invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                                int y, int width, int height)
{
    RC_IF_ERROR(target != GL_FRAMEBUFFER, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR((numAttachments < 0) || (numAttachments > 1 && attachments == nullptr), GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(width < 0 || height < 0, GL_INVALID_VALUE, RC_RET_VOID);

    // \todo [2012-07-17 pyry] Support multiple color attachments.

    const Vec4 colorClearValue(0.0f);
    const float depthClearValue = 1.0f;
    const int stencilClearValue = 0;

    bool isFboBound        = m_drawFramebufferBinding != nullptr;
    bool discardBuffers[3] = {false, false, false}; // Color, depth, stencil

    for (int attNdx = 0; attNdx < numAttachments; attNdx++)
    {
        bool isColor        = attachments[attNdx] == (isFboBound ? GL_COLOR_ATTACHMENT0 : GL_COLOR);
        bool isDepth        = attachments[attNdx] == (isFboBound ? GL_DEPTH_ATTACHMENT : GL_DEPTH);
        bool isStencil      = attachments[attNdx] == (isFboBound ? GL_STENCIL_ATTACHMENT : GL_STENCIL);
        bool isDepthStencil = isFboBound && attachments[attNdx] == GL_DEPTH_STENCIL_ATTACHMENT;

        RC_IF_ERROR(!isColor && !isDepth && !isStencil && !isDepthStencil, GL_INVALID_VALUE, RC_RET_VOID);

        if (isColor)
            discardBuffers[0] = true;
        if (isDepth || isDepthStencil)
            discardBuffers[1] = true;
        if (isStencil || isDepthStencil)
            discardBuffers[2] = true;
    }

    for (int ndx = 0; ndx < 3; ndx++)
    {
        if (!discardBuffers[ndx])
            continue;

        bool isColor                         = ndx == 0;
        bool isDepth                         = ndx == 1;
        bool isStencil                       = ndx == 2;
        rr::MultisamplePixelBufferAccess buf = isColor ? getDrawColorbuffer() :
                                               isDepth ? getDepthMultisampleAccess(getDrawDepthbuffer()) :
                                                         getStencilMultisampleAccess(getDrawStencilbuffer());

        if (isEmpty(buf))
            continue;

        tcu::IVec4 area =
            intersect(tcu::IVec4(0, 0, buf.raw().getHeight(), buf.raw().getDepth()), tcu::IVec4(x, y, width, height));
        rr::MultisamplePixelBufferAccess access = rr::getSubregion(buf, area.x(), area.y(), area.z(), area.w());

        if (isColor)
            rr::clear(access, colorClearValue);
        else if (isDepth)
            rr::clear(access, tcu::Vec4(depthClearValue));
        else if (isStencil)
            rr::clear(access, tcu::IVec4(stencilClearValue));
    }
}

void ReferenceContext::invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments)
{
    // \todo [2012-07-17 pyry] Support multiple color attachments.
    rr::MultisampleConstPixelBufferAccess colorBuf0  = getDrawColorbuffer();
    rr::MultisampleConstPixelBufferAccess depthBuf   = getDrawDepthbuffer();
    rr::MultisampleConstPixelBufferAccess stencilBuf = getDrawStencilbuffer();
    int width                                        = 0;
    int height                                       = 0;

    width = de::max(width, colorBuf0.raw().getHeight());
    width = de::max(width, depthBuf.raw().getHeight());
    width = de::max(width, stencilBuf.raw().getHeight());

    height = de::max(height, colorBuf0.raw().getDepth());
    height = de::max(height, depthBuf.raw().getDepth());
    height = de::max(height, stencilBuf.raw().getDepth());

    invalidateSubFramebuffer(target, numAttachments, attachments, 0, 0, width, height);
}

void ReferenceContext::clear(uint32_t buffers)
{
    RC_IF_ERROR((buffers & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0, GL_INVALID_VALUE,
                RC_RET_VOID);

    rr::MultisamplePixelBufferAccess colorBuf0  = getDrawColorbuffer();
    rr::MultisamplePixelBufferAccess depthBuf   = getDrawDepthbuffer();
    rr::MultisamplePixelBufferAccess stencilBuf = getDrawStencilbuffer();
    IVec4 baseArea                              = m_scissorEnabled ? m_scissorBox : IVec4(0, 0, 0x7fffffff, 0x7fffffff);
    IVec4 colorArea                             = intersect(baseArea, getBufferRect(colorBuf0));
    IVec4 depthArea                             = intersect(baseArea, getBufferRect(depthBuf));
    IVec4 stencilArea                           = intersect(baseArea, getBufferRect(stencilBuf));
    bool hasColor0                              = !isEmpty(colorArea);
    bool hasDepth                               = !isEmpty(depthArea);
    bool hasStencil                             = !isEmpty(stencilArea);

    if (hasColor0 && (buffers & GL_COLOR_BUFFER_BIT) != 0)
    {
        rr::MultisamplePixelBufferAccess access =
            rr::getSubregion(colorBuf0, colorArea.x(), colorArea.y(), colorArea.z(), colorArea.w());
        bool isSRGB   = tcu::isSRGB(colorBuf0.raw().getFormat());
        Vec4 c        = (isSRGB && m_sRGBUpdateEnabled) ? tcu::linearToSRGB(m_clearColor) : m_clearColor;
        bool maskUsed = !m_colorMask[0] || !m_colorMask[1] || !m_colorMask[2] || !m_colorMask[3];
        bool maskZero = !m_colorMask[0] && !m_colorMask[1] && !m_colorMask[2] && !m_colorMask[3];

        if (!maskUsed)
            rr::clear(access, c);
        else if (!maskZero)
        {
            for (int y = 0; y < access.raw().getDepth(); y++)
                for (int x = 0; x < access.raw().getHeight(); x++)
                    for (int s = 0; s < access.getNumSamples(); s++)
                        access.raw().setPixel(tcu::select(c, access.raw().getPixel(s, x, y), m_colorMask), s, x, y);
        }
        // else all channels masked out
    }

    if (hasDepth && (buffers & GL_DEPTH_BUFFER_BIT) != 0 && m_depthMask)
    {
        rr::MultisamplePixelBufferAccess access = getDepthMultisampleAccess(
            rr::getSubregion(depthBuf, depthArea.x(), depthArea.y(), depthArea.z(), depthArea.w()));
        rr::clearDepth(access, m_clearDepth);
    }

    if (hasStencil && (buffers & GL_STENCIL_BUFFER_BIT) != 0)
    {
        rr::MultisamplePixelBufferAccess access = getStencilMultisampleAccess(
            rr::getSubregion(stencilBuf, stencilArea.x(), stencilArea.y(), stencilArea.z(), stencilArea.w()));
        int stencilBits = getNumStencilBits(stencilBuf.raw().getFormat());
        int stencil     = maskStencil(stencilBits, m_clearStencil);

        if ((m_stencil[rr::FACETYPE_FRONT].writeMask & ((1u << stencilBits) - 1u)) != ((1u << stencilBits) - 1u))
        {
            // Slow path where depth or stencil is masked out in write.
            for (int y = 0; y < access.raw().getDepth(); y++)
                for (int x = 0; x < access.raw().getHeight(); x++)
                    for (int s = 0; s < access.getNumSamples(); s++)
                        writeMaskedStencil(access, s, x, y, stencil, m_stencil[rr::FACETYPE_FRONT].writeMask);
        }
        else
            rr::clearStencil(access, stencil);
    }
}

void ReferenceContext::clearBufferiv(uint32_t buffer, int drawbuffer, const int *value)
{
    RC_IF_ERROR(buffer != GL_COLOR && buffer != GL_STENCIL, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(drawbuffer != 0, GL_INVALID_VALUE, RC_RET_VOID); // \todo [2012-04-06 pyry] MRT support.

    IVec4 baseArea = m_scissorEnabled ? m_scissorBox : IVec4(0, 0, 0x7fffffff, 0x7fffffff);

    if (buffer == GL_COLOR)
    {
        rr::MultisamplePixelBufferAccess colorBuf = getDrawColorbuffer();
        bool maskUsed = !m_colorMask[0] || !m_colorMask[1] || !m_colorMask[2] || !m_colorMask[3];
        bool maskZero = !m_colorMask[0] && !m_colorMask[1] && !m_colorMask[2] && !m_colorMask[3];
        IVec4 area    = intersect(baseArea, getBufferRect(colorBuf));

        if (!isEmpty(area) && !maskZero)
        {
            rr::MultisamplePixelBufferAccess access =
                rr::getSubregion(colorBuf, area.x(), area.y(), area.z(), area.w());
            IVec4 color(value[0], value[1], value[2], value[3]);

            if (!maskUsed)
                rr::clear(access, color);
            else
            {
                for (int y = 0; y < access.raw().getDepth(); y++)
                    for (int x = 0; x < access.raw().getHeight(); x++)
                        for (int s = 0; s < access.getNumSamples(); s++)
                            access.raw().setPixel(tcu::select(color, access.raw().getPixelInt(s, x, y), m_colorMask), s,
                                                  x, y);
            }
        }
    }
    else
    {
        TCU_CHECK_INTERNAL(buffer == GL_STENCIL);

        rr::MultisamplePixelBufferAccess stencilBuf = getDrawStencilbuffer();
        IVec4 area                                  = intersect(baseArea, getBufferRect(stencilBuf));

        if (!isEmpty(area) && m_stencil[rr::FACETYPE_FRONT].writeMask != 0)
        {
            rr::MultisamplePixelBufferAccess access =
                getStencilMultisampleAccess(rr::getSubregion(stencilBuf, area.x(), area.y(), area.z(), area.w()));
            int stencil = value[0];

            for (int y = 0; y < access.raw().getDepth(); y++)
                for (int x = 0; x < access.raw().getHeight(); x++)
                    for (int s = 0; s < access.getNumSamples(); s++)
                        writeMaskedStencil(access, s, x, y, stencil, m_stencil[rr::FACETYPE_FRONT].writeMask);
        }
    }
}

void ReferenceContext::clearBufferfv(uint32_t buffer, int drawbuffer, const float *value)
{
    RC_IF_ERROR(buffer != GL_COLOR && buffer != GL_DEPTH, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(drawbuffer != 0, GL_INVALID_VALUE, RC_RET_VOID); // \todo [2012-04-06 pyry] MRT support.

    IVec4 baseArea = m_scissorEnabled ? m_scissorBox : IVec4(0, 0, 0x7fffffff, 0x7fffffff);

    if (buffer == GL_COLOR)
    {
        rr::MultisamplePixelBufferAccess colorBuf = getDrawColorbuffer();
        bool maskUsed = !m_colorMask[0] || !m_colorMask[1] || !m_colorMask[2] || !m_colorMask[3];
        bool maskZero = !m_colorMask[0] && !m_colorMask[1] && !m_colorMask[2] && !m_colorMask[3];
        IVec4 area    = intersect(baseArea, getBufferRect(colorBuf));

        if (!isEmpty(area) && !maskZero)
        {
            rr::MultisamplePixelBufferAccess access =
                rr::getSubregion(colorBuf, area.x(), area.y(), area.z(), area.w());
            Vec4 color(value[0], value[1], value[2], value[3]);

            if (m_sRGBUpdateEnabled && tcu::isSRGB(access.raw().getFormat()))
                color = tcu::linearToSRGB(color);

            if (!maskUsed)
                rr::clear(access, color);
            else
            {
                for (int y = 0; y < access.raw().getDepth(); y++)
                    for (int x = 0; x < access.raw().getHeight(); x++)
                        for (int s = 0; s < access.getNumSamples(); s++)
                            access.raw().setPixel(tcu::select(color, access.raw().getPixel(s, x, y), m_colorMask), s, x,
                                                  y);
            }
        }
    }
    else
    {
        TCU_CHECK_INTERNAL(buffer == GL_DEPTH);

        rr::MultisamplePixelBufferAccess depthBuf = getDrawDepthbuffer();
        IVec4 area                                = intersect(baseArea, getBufferRect(depthBuf));

        if (!isEmpty(area) && m_depthMask)
        {
            rr::MultisamplePixelBufferAccess access =
                rr::getSubregion(depthBuf, area.x(), area.y(), area.z(), area.w());
            float depth = value[0];

            rr::clearDepth(access, depth);
        }
    }
}

void ReferenceContext::clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value)
{
    RC_IF_ERROR(buffer != GL_COLOR, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(drawbuffer != 0, GL_INVALID_VALUE, RC_RET_VOID); // \todo [2012-04-06 pyry] MRT support.

    IVec4 baseArea = m_scissorEnabled ? m_scissorBox : IVec4(0, 0, 0x7fffffff, 0x7fffffff);

    TCU_CHECK_INTERNAL(buffer == GL_COLOR);
    {
        rr::MultisamplePixelBufferAccess colorBuf = getDrawColorbuffer();
        bool maskUsed = !m_colorMask[0] || !m_colorMask[1] || !m_colorMask[2] || !m_colorMask[3];
        bool maskZero = !m_colorMask[0] && !m_colorMask[1] && !m_colorMask[2] && !m_colorMask[3];
        IVec4 area    = intersect(baseArea, getBufferRect(colorBuf));

        if (!isEmpty(area) && !maskZero)
        {
            rr::MultisamplePixelBufferAccess access =
                rr::getSubregion(colorBuf, area.x(), area.y(), area.z(), area.w());
            tcu::UVec4 color(value[0], value[1], value[2], value[3]);

            if (!maskUsed)
                rr::clear(access, color.asInt());
            else
            {
                for (int y = 0; y < access.raw().getDepth(); y++)
                    for (int x = 0; x < access.raw().getHeight(); x++)
                        for (int s = 0; s < access.getNumSamples(); s++)
                            access.raw().setPixel(tcu::select(color, access.raw().getPixelUint(s, x, y), m_colorMask),
                                                  s, x, y);
            }
        }
    }
}

void ReferenceContext::clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil)
{
    RC_IF_ERROR(buffer != GL_DEPTH_STENCIL, GL_INVALID_ENUM, RC_RET_VOID);
    clearBufferfv(GL_DEPTH, drawbuffer, &depth);
    clearBufferiv(GL_STENCIL, drawbuffer, &stencil);
}

void ReferenceContext::bindVertexArray(uint32_t array)
{
    rc::VertexArray *vertexArrayObject = nullptr;

    if (array != 0)
    {
        vertexArrayObject = m_vertexArrays.find(array);
        if (!vertexArrayObject)
        {
            vertexArrayObject = new rc::VertexArray(array, m_limits.maxVertexAttribs);
            m_vertexArrays.insert(vertexArrayObject);
        }
    }

    // Create new references
    if (vertexArrayObject)
        m_vertexArrays.acquireReference(vertexArrayObject);

    // Remove old references
    if (m_vertexArrayBinding)
        m_vertexArrays.releaseReference(m_vertexArrayBinding);

    m_vertexArrayBinding = vertexArrayObject;
}

void ReferenceContext::genVertexArrays(int numArrays, uint32_t *vertexArrays)
{
    RC_IF_ERROR(!vertexArrays, GL_INVALID_VALUE, RC_RET_VOID);

    for (int ndx = 0; ndx < numArrays; ndx++)
        vertexArrays[ndx] = m_vertexArrays.allocateName();
}

void ReferenceContext::deleteVertexArrays(int numArrays, const uint32_t *vertexArrays)
{
    for (int i = 0; i < numArrays; i++)
    {
        uint32_t name            = vertexArrays[i];
        VertexArray *vertexArray = name ? m_vertexArrays.find(name) : nullptr;

        if (vertexArray)
            deleteVertexArray(vertexArray);
    }
}

void ReferenceContext::vertexAttribPointer(uint32_t index, int rawSize, uint32_t type, bool normalized, int stride,
                                           const void *pointer)
{
    const bool allowBGRA    = !glu::isContextTypeES(getType());
    const int effectiveSize = (allowBGRA && rawSize == GL_BGRA) ? (4) : (rawSize);

    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(effectiveSize <= 0 || effectiveSize > 4, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(type != GL_BYTE && type != GL_UNSIGNED_BYTE && type != GL_SHORT && type != GL_UNSIGNED_SHORT &&
                    type != GL_INT && type != GL_UNSIGNED_INT && type != GL_FIXED && type != GL_DOUBLE &&
                    type != GL_FLOAT && type != GL_HALF_FLOAT && type != GL_INT_2_10_10_10_REV &&
                    type != GL_UNSIGNED_INT_2_10_10_10_REV,
                GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(normalized != GL_TRUE && normalized != GL_FALSE, GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(stride < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) && effectiveSize != 4,
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(m_vertexArrayBinding != nullptr && m_arrayBufferBinding == nullptr && pointer != nullptr,
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(allowBGRA && rawSize == GL_BGRA && type != GL_INT_2_10_10_10_REV &&
                    type != GL_UNSIGNED_INT_2_10_10_10_REV && type != GL_UNSIGNED_BYTE,
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(allowBGRA && rawSize == GL_BGRA && normalized == GL_FALSE, GL_INVALID_OPERATION, RC_RET_VOID);

    rc::VertexArray &vao = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);

    vao.m_arrays[index].size       = rawSize;
    vao.m_arrays[index].stride     = stride;
    vao.m_arrays[index].type       = type;
    vao.m_arrays[index].normalized = normalized == GL_TRUE;
    vao.m_arrays[index].integer    = false;
    vao.m_arrays[index].pointer    = pointer;

    // acquire new reference
    if (m_arrayBufferBinding)
        m_buffers.acquireReference(m_arrayBufferBinding);

    // release old reference
    if (vao.m_arrays[index].bufferBinding)
        m_buffers.releaseReference(vao.m_arrays[index].bufferBinding);

    vao.m_arrays[index].bufferDeleted = false;
    vao.m_arrays[index].bufferBinding = m_arrayBufferBinding;
}

void ReferenceContext::vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(size <= 0 || size > 4, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(type != GL_BYTE && type != GL_UNSIGNED_BYTE && type != GL_SHORT && type != GL_UNSIGNED_SHORT &&
                    type != GL_INT && type != GL_UNSIGNED_INT,
                GL_INVALID_ENUM, RC_RET_VOID);
    RC_IF_ERROR(stride < 0, GL_INVALID_VALUE, RC_RET_VOID);
    RC_IF_ERROR(m_vertexArrayBinding != nullptr && m_arrayBufferBinding == nullptr && pointer != nullptr,
                GL_INVALID_OPERATION, RC_RET_VOID);

    rc::VertexArray &vao = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);

    vao.m_arrays[index].size       = size;
    vao.m_arrays[index].stride     = stride;
    vao.m_arrays[index].type       = type;
    vao.m_arrays[index].normalized = false;
    vao.m_arrays[index].integer    = true;
    vao.m_arrays[index].pointer    = pointer;

    // acquire new reference
    if (m_arrayBufferBinding)
        m_buffers.acquireReference(m_arrayBufferBinding);

    // release old reference
    if (vao.m_arrays[index].bufferBinding)
        m_buffers.releaseReference(vao.m_arrays[index].bufferBinding);

    vao.m_arrays[index].bufferDeleted = false;
    vao.m_arrays[index].bufferBinding = m_arrayBufferBinding;
}

void ReferenceContext::enableVertexAttribArray(uint32_t index)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    rc::VertexArray &vao        = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);
    vao.m_arrays[index].enabled = true;
}

void ReferenceContext::disableVertexAttribArray(uint32_t index)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    rc::VertexArray &vao        = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);
    vao.m_arrays[index].enabled = false;
}

void ReferenceContext::vertexAttribDivisor(uint32_t index, uint32_t divisor)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    rc::VertexArray &vao        = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);
    vao.m_arrays[index].divisor = divisor;
}

void ReferenceContext::vertexAttrib1f(uint32_t index, float x)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::Vec4(x, 0, 0, 1));
}

void ReferenceContext::vertexAttrib2f(uint32_t index, float x, float y)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::Vec4(x, y, 0, 1));
}

void ReferenceContext::vertexAttrib3f(uint32_t index, float x, float y, float z)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::Vec4(x, y, z, 1));
}

void ReferenceContext::vertexAttrib4f(uint32_t index, float x, float y, float z, float w)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::Vec4(x, y, z, w));
}

void ReferenceContext::vertexAttribI4i(uint32_t index, int32_t x, int32_t y, int32_t z, int32_t w)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::IVec4(x, y, z, w));
}

void ReferenceContext::vertexAttribI4ui(uint32_t index, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    RC_IF_ERROR(index >= (uint32_t)m_limits.maxVertexAttribs, GL_INVALID_VALUE, RC_RET_VOID);

    m_currentAttribs[index] = rr::GenericVec4(tcu::UVec4(x, y, z, w));
}

int32_t ReferenceContext::getAttribLocation(uint32_t program, const char *name)
{
    ShaderProgramObjectContainer *shaderProg = m_programs.find(program);

    RC_IF_ERROR(shaderProg == nullptr, GL_INVALID_OPERATION, -1);

    if (name)
    {
        std::string nameString(name);

        for (size_t ndx = 0; ndx < shaderProg->m_program->m_attributeNames.size(); ++ndx)
            if (shaderProg->m_program->m_attributeNames[ndx] == nameString)
                return (int)ndx;
    }

    return -1;
}

void ReferenceContext::uniformv(int32_t location, glu::DataType type, int32_t count, const void *v)
{
    RC_IF_ERROR(m_currentProgram == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);

    std::vector<sglr::UniformSlot> &uniforms = m_currentProgram->m_program->m_uniforms;

    if (location == -1)
        return;

    RC_IF_ERROR(location < 0 || (size_t)location >= uniforms.size(), GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(uniforms[location].type != type, GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(count != 1, GL_INVALID_OPERATION, RC_RET_VOID); // \todo [2013-12-13 pyry] Array uniforms.

    {
        const int scalarSize = glu::getDataTypeScalarSize(type);
        DE_ASSERT(scalarSize * sizeof(uint32_t) <= sizeof(uniforms[location].value));
        deMemcpy(&uniforms[location].value, v, scalarSize * (int)sizeof(uint32_t));
    }
}

void ReferenceContext::uniform1iv(int32_t location, int32_t count, const int32_t *v)
{
    RC_IF_ERROR(m_currentProgram == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);

    std::vector<sglr::UniformSlot> &uniforms = m_currentProgram->m_program->m_uniforms;

    if (location == -1)
        return;

    RC_IF_ERROR(location < 0 || (size_t)location >= uniforms.size(), GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(count != 1, GL_INVALID_OPERATION, RC_RET_VOID); // \todo [2013-12-13 pyry] Array uniforms.

    switch (uniforms[location].type)
    {
    case glu::TYPE_INT:
        uniforms[location].value.i = *v;
        return;

    // \note texture unit is stored to value
    case glu::TYPE_SAMPLER_2D:
    case glu::TYPE_UINT_SAMPLER_2D:
    case glu::TYPE_INT_SAMPLER_2D:
    case glu::TYPE_SAMPLER_CUBE:
    case glu::TYPE_UINT_SAMPLER_CUBE:
    case glu::TYPE_INT_SAMPLER_CUBE:
    case glu::TYPE_SAMPLER_2D_ARRAY:
    case glu::TYPE_UINT_SAMPLER_2D_ARRAY:
    case glu::TYPE_INT_SAMPLER_2D_ARRAY:
    case glu::TYPE_SAMPLER_3D:
    case glu::TYPE_UINT_SAMPLER_3D:
    case glu::TYPE_INT_SAMPLER_3D:
    case glu::TYPE_SAMPLER_CUBE_ARRAY:
    case glu::TYPE_UINT_SAMPLER_CUBE_ARRAY:
    case glu::TYPE_INT_SAMPLER_CUBE_ARRAY:
        uniforms[location].value.i = *v;
        return;

    default:
        setError(GL_INVALID_OPERATION);
        return;
    }
}

void ReferenceContext::uniform1f(int32_t location, const float v0)
{
    uniform1fv(location, 1, &v0);
}

void ReferenceContext::uniform1i(int32_t location, int32_t v0)
{
    uniform1iv(location, 1, &v0);
}

void ReferenceContext::uniform1fv(int32_t location, int32_t count, const float *v)
{
    uniformv(location, glu::TYPE_FLOAT, count, v);
}

void ReferenceContext::uniform2fv(int32_t location, int32_t count, const float *v)
{
    uniformv(location, glu::TYPE_FLOAT_VEC2, count, v);
}

void ReferenceContext::uniform3fv(int32_t location, int32_t count, const float *v)
{
    uniformv(location, glu::TYPE_FLOAT_VEC3, count, v);
}

void ReferenceContext::uniform4fv(int32_t location, int32_t count, const float *v)
{
    uniformv(location, glu::TYPE_FLOAT_VEC4, count, v);
}

void ReferenceContext::uniform2iv(int32_t location, int32_t count, const int32_t *v)
{
    uniformv(location, glu::TYPE_INT_VEC2, count, v);
}

void ReferenceContext::uniform3iv(int32_t location, int32_t count, const int32_t *v)
{
    uniformv(location, glu::TYPE_INT_VEC3, count, v);
}

void ReferenceContext::uniform4iv(int32_t location, int32_t count, const int32_t *v)
{
    uniformv(location, glu::TYPE_INT_VEC4, count, v);
}

void ReferenceContext::uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value)
{
    RC_IF_ERROR(m_currentProgram == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);

    std::vector<sglr::UniformSlot> &uniforms = m_currentProgram->m_program->m_uniforms;

    if (location == -1)
        return;

    RC_IF_ERROR(location < 0 || (size_t)location >= uniforms.size(), GL_INVALID_OPERATION, RC_RET_VOID);

    if (count == 0)
        return;

    RC_IF_ERROR(transpose != GL_TRUE && transpose != GL_FALSE, GL_INVALID_ENUM, RC_RET_VOID);

    switch (uniforms[location].type)
    {
    case glu::TYPE_FLOAT_MAT3:
        RC_IF_ERROR(count > 1, GL_INVALID_OPERATION, RC_RET_VOID);

        if (transpose == GL_FALSE) // input is column major => transpose from column major to internal row major
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    uniforms[location].value.m3[row * 3 + col] = value[col * 3 + row];
        else // input is row major
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    uniforms[location].value.m3[row * 3 + col] = value[row * 3 + col];

        break;

    default:
        setError(GL_INVALID_OPERATION);
        return;
    }
}

void ReferenceContext::uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value)
{
    RC_IF_ERROR(m_currentProgram == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);

    std::vector<sglr::UniformSlot> &uniforms = m_currentProgram->m_program->m_uniforms;

    if (location == -1)
        return;

    RC_IF_ERROR(location < 0 || (size_t)location >= uniforms.size(), GL_INVALID_OPERATION, RC_RET_VOID);

    if (count == 0)
        return;

    RC_IF_ERROR(transpose != GL_TRUE && transpose != GL_FALSE, GL_INVALID_ENUM, RC_RET_VOID);

    switch (uniforms[location].type)
    {
    case glu::TYPE_FLOAT_MAT4:
        RC_IF_ERROR(count > 1, GL_INVALID_OPERATION, RC_RET_VOID);

        if (transpose == GL_FALSE) // input is column major => transpose from column major to internal row major
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col)
                    uniforms[location].value.m4[row * 3 + col] = value[col * 3 + row];
        else // input is row major
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col)
                    uniforms[location].value.m4[row * 3 + col] = value[row * 3 + col];

        break;

    default:
        setError(GL_INVALID_OPERATION);
        return;
    }
}

int32_t ReferenceContext::getUniformLocation(uint32_t program, const char *name)
{
    ShaderProgramObjectContainer *shaderProg = m_programs.find(program);
    RC_IF_ERROR(shaderProg == nullptr, GL_INVALID_OPERATION, -1);

    std::vector<sglr::UniformSlot> &uniforms = shaderProg->m_program->m_uniforms;

    for (size_t i = 0; i < uniforms.size(); ++i)
        if (name && uniforms[i].name == name)
            return (int)i;

    return -1;
}

void ReferenceContext::lineWidth(float w)
{
    RC_IF_ERROR(w < 0.0f, GL_INVALID_VALUE, RC_RET_VOID);
    m_lineWidth = w;
}

void ReferenceContext::deleteVertexArray(rc::VertexArray *vertexArray)
{
    if (m_vertexArrayBinding == vertexArray)
        bindVertexArray(0);

    if (vertexArray->m_elementArrayBufferBinding)
        m_buffers.releaseReference(vertexArray->m_elementArrayBufferBinding);

    for (size_t ndx = 0; ndx < vertexArray->m_arrays.size(); ++ndx)
        if (vertexArray->m_arrays[ndx].bufferBinding)
            m_buffers.releaseReference(vertexArray->m_arrays[ndx].bufferBinding);

    DE_ASSERT(vertexArray->getRefCount() == 1);
    m_vertexArrays.releaseReference(vertexArray);
}

void ReferenceContext::deleteProgramObject(rc::ShaderProgramObjectContainer *sp)
{
    // Unbinding program will delete it
    if (m_currentProgram == sp && sp->m_deleteFlag)
    {
        useProgram(0);
        return;
    }

    // Unbinding program will NOT delete it
    if (m_currentProgram == sp)
        useProgram(0);

    DE_ASSERT(sp->getRefCount() == 1);
    m_programs.releaseReference(sp);
}

void ReferenceContext::drawArrays(uint32_t mode, int first, int count)
{
    drawArraysInstanced(mode, first, count, 1);
}

void ReferenceContext::drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount)
{
    // Error conditions
    {
        RC_IF_ERROR(first < 0 || count < 0 || instanceCount < 0, GL_INVALID_VALUE, RC_RET_VOID);

        if (!predrawErrorChecks(mode))
            return;
    }

    // All is ok
    {
        const rr::PrimitiveType primitiveType = sglr::rr_util::mapGLPrimitiveType(mode);

        drawWithReference(rr::PrimitiveList(primitiveType, count, first), instanceCount);
    }
}

void ReferenceContext::drawElements(uint32_t mode, int count, uint32_t type, const void *indices)
{
    drawElementsInstanced(mode, count, type, indices, 1);
}

void ReferenceContext::drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                              int baseVertex)
{
    drawElementsInstancedBaseVertex(mode, count, type, indices, 1, baseVertex);
}

void ReferenceContext::drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices,
                                             int instanceCount)
{
    drawElementsInstancedBaseVertex(mode, count, type, indices, instanceCount, 0);
}

void ReferenceContext::drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                       int instanceCount, int baseVertex)
{
    rc::VertexArray &vao = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);

    // Error conditions
    {
        RC_IF_ERROR(type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT, GL_INVALID_ENUM,
                    RC_RET_VOID);
        RC_IF_ERROR(count < 0 || instanceCount < 0, GL_INVALID_VALUE, RC_RET_VOID);

        if (!predrawErrorChecks(mode))
            return;
    }

    // All is ok
    {
        const rr::PrimitiveType primitiveType = sglr::rr_util::mapGLPrimitiveType(mode);
        const void *indicesPtr =
            (vao.m_elementArrayBufferBinding) ?
                (vao.m_elementArrayBufferBinding->getData() + reinterpret_cast<uintptr_t>(indices)) :
                (indices);

        drawWithReference(
            rr::PrimitiveList(primitiveType, count,
                              rr::DrawIndices(indicesPtr, sglr::rr_util::mapGLIndexType(type), baseVertex)),
            instanceCount);
    }
}

void ReferenceContext::drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                         const void *indices)
{
    RC_IF_ERROR(end < start, GL_INVALID_VALUE, RC_RET_VOID);

    drawElements(mode, count, type, indices);
}

void ReferenceContext::drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count,
                                                   uint32_t type, const void *indices, int baseVertex)
{
    RC_IF_ERROR(end < start, GL_INVALID_VALUE, RC_RET_VOID);

    drawElementsBaseVertex(mode, count, type, indices, baseVertex);
}

void ReferenceContext::drawArraysIndirect(uint32_t mode, const void *indirect)
{
    struct DrawArraysIndirectCommand
    {
        uint32_t count;
        uint32_t primCount;
        uint32_t first;
        uint32_t reservedMustBeZero;
    };

    const DrawArraysIndirectCommand *command;

    // Check errors

    if (!predrawErrorChecks(mode))
        return;

    // Check pointer validity

    RC_IF_ERROR(m_drawIndirectBufferBinding == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(!deIsAlignedPtr(indirect, 4), GL_INVALID_OPERATION, RC_RET_VOID);

    // \note watch for overflows, indirect might be close to 0xFFFFFFFF and indirect+something might overflow
    RC_IF_ERROR((size_t) reinterpret_cast<uintptr_t>(indirect) > (size_t)m_drawIndirectBufferBinding->getSize(),
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR((size_t) reinterpret_cast<uintptr_t>(indirect) + sizeof(DrawArraysIndirectCommand) >
                    (size_t)m_drawIndirectBufferBinding->getSize(),
                GL_INVALID_OPERATION, RC_RET_VOID);

    // Check values

    command = (const DrawArraysIndirectCommand *)(m_drawIndirectBufferBinding->getData() +
                                                  reinterpret_cast<uintptr_t>(indirect));
    RC_IF_ERROR(command->reservedMustBeZero != 0, GL_INVALID_OPERATION, RC_RET_VOID);

    // draw
    drawArraysInstanced(mode, command->first, command->count, command->primCount);
}

void ReferenceContext::drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect)
{
    struct DrawElementsIndirectCommand
    {
        uint32_t count;
        uint32_t primCount;
        uint32_t firstIndex;
        int32_t baseVertex;
        uint32_t reservedMustBeZero;
    };

    const DrawElementsIndirectCommand *command;

    // Check errors

    if (!predrawErrorChecks(mode))
        return;

    RC_IF_ERROR(type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT, GL_INVALID_ENUM,
                RC_RET_VOID);

    RC_IF_ERROR(!getBufferBinding(GL_ELEMENT_ARRAY_BUFFER), GL_INVALID_OPERATION, RC_RET_VOID);

    // Check pointer validity

    RC_IF_ERROR(m_drawIndirectBufferBinding == nullptr, GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR(!deIsAlignedPtr(indirect, 4), GL_INVALID_OPERATION, RC_RET_VOID);

    // \note watch for overflows, indirect might be close to 0xFFFFFFFF and indirect+something might overflow
    RC_IF_ERROR((size_t) reinterpret_cast<uintptr_t>(indirect) > (size_t)m_drawIndirectBufferBinding->getSize(),
                GL_INVALID_OPERATION, RC_RET_VOID);
    RC_IF_ERROR((size_t) reinterpret_cast<uintptr_t>(indirect) + sizeof(DrawElementsIndirectCommand) >
                    (size_t)m_drawIndirectBufferBinding->getSize(),
                GL_INVALID_OPERATION, RC_RET_VOID);

    // Check values

    command = (const DrawElementsIndirectCommand *)(m_drawIndirectBufferBinding->getData() +
                                                    reinterpret_cast<uintptr_t>(indirect));
    RC_IF_ERROR(command->reservedMustBeZero != 0, GL_INVALID_OPERATION, RC_RET_VOID);

    // Check command error conditions
    RC_IF_ERROR((int)command->count < 0 || (int)command->primCount < 0, GL_INVALID_VALUE, RC_RET_VOID);

    // Draw
    {
        const size_t sizeOfType = (type == GL_UNSIGNED_BYTE) ? (1) : ((type == GL_UNSIGNED_SHORT) ? (2) : (4));
        const void *indicesPtr  = glu::BufferOffsetAsPointer(command->firstIndex * sizeOfType);

        drawElementsInstancedBaseVertex(mode, (int)command->count, type, indicesPtr, (int)command->primCount,
                                        command->baseVertex);
    }
}

void ReferenceContext::multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount)
{
    DE_UNREF(mode);
    DE_UNREF(first);
    DE_UNREF(count);
    DE_UNREF(primCount);

    // not supported in gles, prevent accidental use
    DE_ASSERT(false);
}

void ReferenceContext::multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                         int primCount)
{
    DE_UNREF(mode);
    DE_UNREF(count);
    DE_UNREF(type);
    DE_UNREF(indices);
    DE_UNREF(primCount);

    // not supported in gles, prevent accidental use
    DE_ASSERT(false);
}

void ReferenceContext::multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                                   int primCount, const int *baseVertex)
{
    DE_UNREF(mode);
    DE_UNREF(count);
    DE_UNREF(type);
    DE_UNREF(indices);
    DE_UNREF(primCount);
    DE_UNREF(baseVertex);

    // not supported in gles, prevent accidental use
    DE_ASSERT(false);
}

bool ReferenceContext::predrawErrorChecks(uint32_t mode)
{
    RC_IF_ERROR(mode != GL_POINTS && mode != GL_LINE_STRIP && mode != GL_LINE_LOOP && mode != GL_LINES &&
                    mode != GL_TRIANGLE_STRIP && mode != GL_TRIANGLE_FAN && mode != GL_TRIANGLES &&
                    mode != GL_LINES_ADJACENCY && mode != GL_LINE_STRIP_ADJACENCY && mode != GL_TRIANGLES_ADJACENCY &&
                    mode != GL_TRIANGLE_STRIP_ADJACENCY,
                GL_INVALID_ENUM, false);

    // \todo [jarkko] Uncomment following code when the buffer mapping support is added
    //for (size_t ndx = 0; ndx < vao.m_arrays.size(); ++ndx)
    //    if (vao.m_arrays[ndx].enabled && vao.m_arrays[ndx].bufferBinding && vao.m_arrays[ndx].bufferBinding->isMapped)
    // RC_ERROR_RET(GL_INVALID_OPERATION, RC_RET_VOID);

    RC_IF_ERROR(checkFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE,
                GL_INVALID_FRAMEBUFFER_OPERATION, false);

    // Geometry shader checks
    if (m_currentProgram && m_currentProgram->m_program->m_hasGeometryShader)
    {
        RC_IF_ERROR(m_currentProgram->m_program->rr::GeometryShader::getInputType() ==
                            rr::GEOMETRYSHADERINPUTTYPE_POINTS &&
                        mode != GL_POINTS,
                    GL_INVALID_OPERATION, false);

        RC_IF_ERROR(m_currentProgram->m_program->rr::GeometryShader::getInputType() ==
                            rr::GEOMETRYSHADERINPUTTYPE_LINES &&
                        (mode != GL_LINES && mode != GL_LINE_STRIP && mode != GL_LINE_LOOP),
                    GL_INVALID_OPERATION, false);

        RC_IF_ERROR(m_currentProgram->m_program->rr::GeometryShader::getInputType() ==
                            rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES &&
                        (mode != GL_TRIANGLES && mode != GL_TRIANGLE_STRIP && mode != GL_TRIANGLE_FAN),
                    GL_INVALID_OPERATION, false);

        RC_IF_ERROR(m_currentProgram->m_program->rr::GeometryShader::getInputType() ==
                            rr::GEOMETRYSHADERINPUTTYPE_LINES_ADJACENCY &&
                        (mode != GL_LINES_ADJACENCY && mode != GL_LINE_STRIP_ADJACENCY),
                    GL_INVALID_OPERATION, false);

        RC_IF_ERROR(m_currentProgram->m_program->rr::GeometryShader::getInputType() ==
                            rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES_ADJACENCY &&
                        (mode != GL_TRIANGLES_ADJACENCY && mode != GL_TRIANGLE_STRIP_ADJACENCY),
                    GL_INVALID_OPERATION, false);
    }

    return true;
}

static rr::PrimitiveType getPrimitiveBaseType(rr::PrimitiveType derivedType)
{
    switch (derivedType)
    {
    case rr::PRIMITIVETYPE_TRIANGLES:
    case rr::PRIMITIVETYPE_TRIANGLE_STRIP:
    case rr::PRIMITIVETYPE_TRIANGLE_FAN:
    case rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY:
    case rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY:
        return rr::PRIMITIVETYPE_TRIANGLES;

    case rr::PRIMITIVETYPE_LINES:
    case rr::PRIMITIVETYPE_LINE_STRIP:
    case rr::PRIMITIVETYPE_LINE_LOOP:
    case rr::PRIMITIVETYPE_LINES_ADJACENCY:
    case rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY:
        return rr::PRIMITIVETYPE_LINES;

    case rr::PRIMITIVETYPE_POINTS:
        return rr::PRIMITIVETYPE_POINTS;

    default:
        DE_ASSERT(false);
        return rr::PRIMITIVETYPE_LAST;
    }
}

static uint32_t getFixedRestartIndex(rr::IndexType indexType)
{
    switch (indexType)
    {
    case rr::INDEXTYPE_UINT8:
        return 0xFF;
    case rr::INDEXTYPE_UINT16:
        return 0xFFFF;
    case rr::INDEXTYPE_UINT32:
        return 0xFFFFFFFFul;

    case rr::INDEXTYPE_LAST:
    default:
        DE_ASSERT(false);
        return 0;
    }
}

void ReferenceContext::drawWithReference(const rr::PrimitiveList &primitives, int instanceCount)
{
    // undefined results
    if (m_currentProgram == nullptr)
        return;

    rr::MultisamplePixelBufferAccess colorBuf0  = getDrawColorbuffer();
    rr::MultisamplePixelBufferAccess depthBuf   = getDepthMultisampleAccess(getDrawDepthbuffer());
    rr::MultisamplePixelBufferAccess stencilBuf = getStencilMultisampleAccess(getDrawStencilbuffer());
    const bool hasStencil                       = !isEmpty(stencilBuf);
    const int stencilBits = (hasStencil) ? (getNumStencilBits(stencilBuf.raw().getFormat())) : (0);

    const rr::RenderTarget renderTarget(colorBuf0, depthBuf, stencilBuf);
    const rr::Program program(
        m_currentProgram->m_program->getVertexShader(), m_currentProgram->m_program->getFragmentShader(),
        (m_currentProgram->m_program->m_hasGeometryShader) ? (m_currentProgram->m_program->getGeometryShader()) :
                                                             (nullptr));
    rr::RenderState state((rr::ViewportState)(colorBuf0), m_limits.subpixelBits);

    const rr::Renderer referenceRenderer;
    std::vector<rr::VertexAttrib> vertexAttribs;

    // Gen state
    {
        const rr::PrimitiveType baseType = getPrimitiveBaseType(primitives.getPrimitiveType());
        const bool polygonOffsetEnabled =
            (baseType == rr::PRIMITIVETYPE_TRIANGLES) ? (m_polygonOffsetFillEnabled) : (false);

        //state.cullMode = m_cullMode

        state.fragOps.scissorTestEnabled = m_scissorEnabled;
        state.fragOps.scissorRectangle =
            rr::WindowRectangle(m_scissorBox.x(), m_scissorBox.y(), m_scissorBox.z(), m_scissorBox.w());

        state.fragOps.numStencilBits     = stencilBits;
        state.fragOps.stencilTestEnabled = m_stencilTestEnabled;

        for (int faceType = 0; faceType < rr::FACETYPE_LAST; faceType++)
        {
            state.fragOps.stencilStates[faceType].compMask  = m_stencil[faceType].opMask;
            state.fragOps.stencilStates[faceType].writeMask = m_stencil[faceType].writeMask;
            state.fragOps.stencilStates[faceType].ref       = m_stencil[faceType].ref;
            state.fragOps.stencilStates[faceType].func      = sglr::rr_util::mapGLTestFunc(m_stencil[faceType].func);
            state.fragOps.stencilStates[faceType].sFail =
                sglr::rr_util::mapGLStencilOp(m_stencil[faceType].opStencilFail);
            state.fragOps.stencilStates[faceType].dpFail =
                sglr::rr_util::mapGLStencilOp(m_stencil[faceType].opDepthFail);
            state.fragOps.stencilStates[faceType].dpPass =
                sglr::rr_util::mapGLStencilOp(m_stencil[faceType].opDepthPass);
        }

        state.fragOps.depthTestEnabled = m_depthTestEnabled;
        state.fragOps.depthFunc        = sglr::rr_util::mapGLTestFunc(m_depthFunc);
        state.fragOps.depthMask        = m_depthMask;

        state.fragOps.blendMode              = m_blendEnabled ? rr::BLENDMODE_STANDARD : rr::BLENDMODE_NONE;
        state.fragOps.blendRGBState.equation = sglr::rr_util::mapGLBlendEquation(m_blendModeRGB);
        state.fragOps.blendRGBState.srcFunc  = sglr::rr_util::mapGLBlendFunc(m_blendFactorSrcRGB);
        state.fragOps.blendRGBState.dstFunc  = sglr::rr_util::mapGLBlendFunc(m_blendFactorDstRGB);
        state.fragOps.blendAState.equation   = sglr::rr_util::mapGLBlendEquation(m_blendModeAlpha);
        state.fragOps.blendAState.srcFunc    = sglr::rr_util::mapGLBlendFunc(m_blendFactorSrcAlpha);
        state.fragOps.blendAState.dstFunc    = sglr::rr_util::mapGLBlendFunc(m_blendFactorDstAlpha);
        state.fragOps.blendColor             = m_blendColor;

        state.fragOps.sRGBEnabled = m_sRGBUpdateEnabled;

        state.fragOps.colorMask = m_colorMask;

        state.fragOps.depthClampEnabled = m_depthClampEnabled;

        state.viewport.rect = rr::WindowRectangle(m_viewport.x(), m_viewport.y(), m_viewport.z(), m_viewport.w());
        state.viewport.zn   = m_depthRangeNear;
        state.viewport.zf   = m_depthRangeFar;

        //state.point.pointSize = m_pointSize;
        state.line.lineWidth = m_lineWidth;

        state.fragOps.polygonOffsetEnabled = polygonOffsetEnabled;
        state.fragOps.polygonOffsetFactor  = m_polygonOffsetFactor;
        state.fragOps.polygonOffsetUnits   = m_polygonOffsetUnits;

        {
            const rr::IndexType indexType = primitives.getIndexType();

            if (m_primitiveRestartFixedIndex && indexType != rr::INDEXTYPE_LAST)
            {
                state.restart.enabled      = true;
                state.restart.restartIndex = getFixedRestartIndex(indexType);
            }
            else if (m_primitiveRestartSettableIndex)
            {
                // \note PRIMITIVE_RESTART is active for non-indexed (DrawArrays) operations too.
                state.restart.enabled      = true;
                state.restart.restartIndex = m_primitiveRestartIndex;
            }
            else
            {
                state.restart.enabled = false;
            }
        }

        state.provokingVertexConvention =
            (m_provokingFirstVertexConvention) ? (rr::PROVOKINGVERTEX_FIRST) : (rr::PROVOKINGVERTEX_LAST);
    }

    // gen attributes
    {
        rc::VertexArray &vao = (m_vertexArrayBinding) ? (*m_vertexArrayBinding) : (m_clientVertexArray);

        vertexAttribs.resize(vao.m_arrays.size());
        for (size_t ndx = 0; ndx < vao.m_arrays.size(); ++ndx)
        {
            if (!vao.m_arrays[ndx].enabled)
            {
                vertexAttribs[ndx].type =
                    rr::VERTEXATTRIBTYPE_DONT_CARE; // reading with wrong type is allowed, but results are undefined
                vertexAttribs[ndx].generic = m_currentAttribs[ndx];
            }
            else if (vao.m_arrays[ndx].bufferDeleted)
            {
                vertexAttribs[ndx].type = rr::VERTEXATTRIBTYPE_DONT_CARE; // reading from deleted buffer, output zeros
                vertexAttribs[ndx].generic = tcu::Vec4(0, 0, 0, 0);
            }
            else
            {
                vertexAttribs[ndx].type =
                    (vao.m_arrays[ndx].integer) ?
                        (sglr::rr_util::mapGLPureIntegerVertexAttributeType(vao.m_arrays[ndx].type)) :
                        (sglr::rr_util::mapGLFloatVertexAttributeType(vao.m_arrays[ndx].type,
                                                                      vao.m_arrays[ndx].normalized,
                                                                      vao.m_arrays[ndx].size, this->getType()));
                vertexAttribs[ndx].size            = sglr::rr_util::mapGLSize(vao.m_arrays[ndx].size);
                vertexAttribs[ndx].stride          = vao.m_arrays[ndx].stride;
                vertexAttribs[ndx].instanceDivisor = vao.m_arrays[ndx].divisor;
                vertexAttribs[ndx].pointer         = (vao.m_arrays[ndx].bufferBinding) ?
                                                         (vao.m_arrays[ndx].bufferBinding->getData() +
                                                  reinterpret_cast<uintptr_t>(vao.m_arrays[ndx].pointer)) :
                                                         (vao.m_arrays[ndx].pointer);
            }
        }
    }

    // Set shader samplers
    for (size_t uniformNdx = 0; uniformNdx < m_currentProgram->m_program->m_uniforms.size(); ++uniformNdx)
    {
        const tcu::Sampler::DepthStencilMode depthStencilMode =
            tcu::Sampler::MODE_DEPTH; // \todo[jarkko] support sampler state
        const int texNdx = m_currentProgram->m_program->m_uniforms[uniformNdx].value.i;

        switch (m_currentProgram->m_program->m_uniforms[uniformNdx].type)
        {
        case glu::TYPE_SAMPLER_1D:
        case glu::TYPE_UINT_SAMPLER_1D:
        case glu::TYPE_INT_SAMPLER_1D:
        {
            rc::Texture1D *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].tex1DBinding) ? (m_textureUnits[texNdx].tex1DBinding) :
                                                              (&m_textureUnits[texNdx].default1DTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex1D = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex1D = &m_emptyTex1D;

            break;
        }
        case glu::TYPE_SAMPLER_2D:
        case glu::TYPE_UINT_SAMPLER_2D:
        case glu::TYPE_INT_SAMPLER_2D:
        {
            rc::Texture2D *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].tex2DBinding) ? (m_textureUnits[texNdx].tex2DBinding) :
                                                              (&m_textureUnits[texNdx].default2DTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex2D = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex2D = &m_emptyTex2D;

            break;
        }
        case glu::TYPE_SAMPLER_CUBE:
        case glu::TYPE_UINT_SAMPLER_CUBE:
        case glu::TYPE_INT_SAMPLER_CUBE:
        {
            rc::TextureCube *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].texCubeBinding) ? (m_textureUnits[texNdx].texCubeBinding) :
                                                                (&m_textureUnits[texNdx].defaultCubeTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.texCube = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.texCube = &m_emptyTexCube;

            break;
        }
        case glu::TYPE_SAMPLER_2D_ARRAY:
        case glu::TYPE_UINT_SAMPLER_2D_ARRAY:
        case glu::TYPE_INT_SAMPLER_2D_ARRAY:
        {
            rc::Texture2DArray *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].tex2DArrayBinding) ? (m_textureUnits[texNdx].tex2DArrayBinding) :
                                                                   (&m_textureUnits[texNdx].default2DArrayTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex2DArray = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex2DArray = &m_emptyTex2DArray;

            break;
        }
        case glu::TYPE_SAMPLER_3D:
        case glu::TYPE_UINT_SAMPLER_3D:
        case glu::TYPE_INT_SAMPLER_3D:
        {
            rc::Texture3D *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].tex3DBinding) ? (m_textureUnits[texNdx].tex3DBinding) :
                                                              (&m_textureUnits[texNdx].default3DTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex3D = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.tex3D = &m_emptyTex3D;

            break;
        }
        case glu::TYPE_SAMPLER_CUBE_ARRAY:
        case glu::TYPE_UINT_SAMPLER_CUBE_ARRAY:
        case glu::TYPE_INT_SAMPLER_CUBE_ARRAY:
        {
            rc::TextureCubeArray *tex = nullptr;

            if (texNdx >= 0 && (size_t)texNdx < m_textureUnits.size())
                tex = (m_textureUnits[texNdx].texCubeArrayBinding) ? (m_textureUnits[texNdx].texCubeArrayBinding) :
                                                                     (&m_textureUnits[texNdx].defaultCubeArrayTex);

            if (tex && tex->isComplete())
            {
                tex->updateView(depthStencilMode);
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.texCubeArray = tex;
            }
            else
                m_currentProgram->m_program->m_uniforms[uniformNdx].sampler.texCubeArray = &m_emptyTexCubeArray;

            break;
        }
        default:
            // nothing
            break;
        }
    }

    referenceRenderer.drawInstanced(
        rr::DrawCommand(state, renderTarget, program, (int)vertexAttribs.size(), &vertexAttribs[0], primitives),
        instanceCount);
}

uint32_t ReferenceContext::createProgram(ShaderProgram *program)
{
    int name = m_programs.allocateName();

    m_programs.insert(new rc::ShaderProgramObjectContainer(name, program));

    return name;
}

void ReferenceContext::useProgram(uint32_t program)
{
    rc::ShaderProgramObjectContainer *shaderProg         = nullptr;
    rc::ShaderProgramObjectContainer *programToBeDeleted = nullptr;

    if (program)
    {
        shaderProg = m_programs.find(program);

        // shader has not been linked
        if (!shaderProg || shaderProg->m_deleteFlag)
            RC_ERROR_RET(GL_INVALID_OPERATION, RC_RET_VOID);
    }

    if (m_currentProgram && m_currentProgram->m_deleteFlag)
        programToBeDeleted = m_currentProgram;

    m_currentProgram = shaderProg;

    if (programToBeDeleted)
    {
        DE_ASSERT(programToBeDeleted->getRefCount() == 1);
        deleteProgramObject(programToBeDeleted);
    }
}

void ReferenceContext::deleteProgram(uint32_t program)
{
    if (!program)
        return;

    rc::ShaderProgramObjectContainer *shaderProg = m_programs.find(program);
    if (shaderProg)
    {
        if (shaderProg == m_currentProgram)
        {
            m_currentProgram->m_deleteFlag = true;
        }
        else
        {
            DE_ASSERT(shaderProg->getRefCount() == 1);
            m_programs.releaseReference(shaderProg);
        }
    }
}

void ReferenceContext::readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data)
{
    rr::MultisamplePixelBufferAccess src = getReadColorbuffer();
    TextureFormat transferFmt;

    // Map transfer format.
    transferFmt = glu::mapGLTransferFormat(format, type);
    RC_IF_ERROR(transferFmt.order == TextureFormat::CHANNELORDER_LAST ||
                    transferFmt.type == TextureFormat::CHANNELTYPE_LAST,
                GL_INVALID_ENUM, RC_RET_VOID);

    // Clamp input values
    const int copyX      = deClamp32(x, 0, src.raw().getHeight());
    const int copyY      = deClamp32(y, 0, src.raw().getDepth());
    const int copyWidth  = deClamp32(width, 0, src.raw().getHeight() - x);
    const int copyHeight = deClamp32(height, 0, src.raw().getDepth() - y);

    PixelBufferAccess dst(transferFmt, width, height, 1,
                          deAlign32(width * transferFmt.getPixelSize(), m_pixelPackAlignment), 0,
                          getPixelPackPtr(data));
    rr::resolveMultisampleColorBuffer(tcu::getSubregion(dst, 0, 0, copyWidth, copyHeight),
                                      rr::getSubregion(src, copyX, copyY, copyWidth, copyHeight));
}

uint32_t ReferenceContext::getError(void)
{
    uint32_t err = m_lastError;
    m_lastError  = GL_NO_ERROR;
    return err;
}

void ReferenceContext::finish(void)
{
}

inline void ReferenceContext::setError(uint32_t error)
{
    if (m_lastError == GL_NO_ERROR)
        m_lastError = error;
}

void ReferenceContext::getIntegerv(uint32_t pname, int *param)
{
    switch (pname)
    {
    case GL_MAX_TEXTURE_SIZE:
        *param = m_limits.maxTexture2DSize;
        break;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        *param = m_limits.maxTextureCubeSize;
        break;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
        *param = m_limits.maxTexture2DArrayLayers;
        break;
    case GL_MAX_3D_TEXTURE_SIZE:
        *param = m_limits.maxTexture3DSize;
        break;
    case GL_MAX_RENDERBUFFER_SIZE:
        *param = m_limits.maxRenderbufferSize;
        break;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
        *param = m_limits.maxTextureImageUnits;
        break;
    case GL_MAX_VERTEX_ATTRIBS:
        *param = m_limits.maxVertexAttribs;
        break;

    default:
        setError(GL_INVALID_ENUM);
        break;
    }
}

const char *ReferenceContext::getString(uint32_t pname)
{
    switch (pname)
    {
    case GL_EXTENSIONS:
        return m_limits.extensionStr.c_str();

    default:
        setError(GL_INVALID_ENUM);
        return nullptr;
    }
}

namespace rc
{

TextureLevelArray::TextureLevelArray(void)
{
}

TextureLevelArray::~TextureLevelArray(void)
{
    clear();
}

void TextureLevelArray::clear(void)
{
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(m_data) == DE_LENGTH_OF_ARRAY(m_access));

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(m_data); ndx++)
    {
        m_data[ndx].clear();
        m_access[ndx] = PixelBufferAccess();
    }
}

void TextureLevelArray::allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int depth)
{
    const int dataSize = format.getPixelSize() * width * height * depth;

    DE_ASSERT(deInBounds32(level, 0, DE_LENGTH_OF_ARRAY(m_data)));

    if (hasLevel(level))
        clearLevel(level);

    m_data[level].setStorage(dataSize);
    m_access[level] = PixelBufferAccess(format, width, height, depth, m_data[level].getPtr());
}

void TextureLevelArray::clearLevel(int level)
{
    DE_ASSERT(deInBounds32(level, 0, DE_LENGTH_OF_ARRAY(m_data)));

    m_data[level].clear();
    m_access[level] = PixelBufferAccess();
}

void TextureLevelArray::updateSamplerMode(tcu::Sampler::DepthStencilMode mode)
{
    for (int levelNdx = 0; hasLevel(levelNdx); ++levelNdx)
        m_effectiveAccess[levelNdx] = tcu::getEffectiveDepthStencilAccess(m_access[levelNdx], mode);
}

Texture::Texture(uint32_t name, Type type, bool seamless)
    : NamedObject(name)
    , m_type(type)
    , m_immutable(false)
    , m_sampler(tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                tcu::Sampler::NEAREST_MIPMAP_LINEAR, tcu::Sampler::LINEAR,
                0.0f, // LOD threshold
                true, // normalized coords
                tcu::Sampler::COMPAREMODE_NONE,
                0,               // cmp channel ndx
                tcu::Vec4(0.0f), // border color
                seamless         // seamless cube map, Default value is True.
                )
    , m_baseLevel(0)
    , m_maxLevel(1000)
{
}

Texture1D::Texture1D(uint32_t name) : Texture(name, TYPE_1D), m_view(0, nullptr)
{
}

Texture1D::~Texture1D(void)
{
}

void Texture1D::allocLevel(int level, const tcu::TextureFormat &format, int width)
{
    m_levels.allocLevel(level, format, width, 1, 1);
}

bool Texture1D::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel))
    {
        const tcu::ConstPixelBufferAccess &level0 = getLevel(baseLevel);
        const bool mipmap                         = isMipmapFilter(getSampler().minFilter);

        if (mipmap)
        {
            const TextureFormat &format = level0.getFormat();
            const int w                 = level0.getWidth();
            const int numLevels         = de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels1D(w));

            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                if (hasLevel(baseLevel + levelNdx))
                {
                    const tcu::ConstPixelBufferAccess &level = getLevel(baseLevel + levelNdx);
                    const int expectedW                      = getMipLevelSize(w, levelNdx);

                    if (level.getWidth() != expectedW || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

tcu::Vec4 Texture1D::sample(float s, float lod) const
{
    return m_view.sample(getSampler(), s, 0.0f, lod);
}

void Texture1D::sample4(tcu::Vec4 output[4], const float packetTexcoords[4], float lodBias) const
{
    const float texWidth = (float)m_view.getWidth();

    const float dFdx0 = packetTexcoords[1] - packetTexcoords[0];
    const float dFdx1 = packetTexcoords[3] - packetTexcoords[2];
    const float dFdy0 = packetTexcoords[2] - packetTexcoords[0];
    const float dFdy1 = packetTexcoords[3] - packetTexcoords[1];

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const float &dFdx = (fragNdx > 2) ? dFdx1 : dFdx0;
        const float &dFdy = (fragNdx % 2) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx), de::abs(dFdy));
        const float p  = mu * texWidth;

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] = sample(packetTexcoords[fragNdx], lod);
    }
}

void Texture1D::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel) && !isEmpty(getLevel(baseLevel)))
    {
        const int width     = getLevel(baseLevel).getWidth();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels = isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels1D(width)) : 1;

        m_levels.updateSamplerMode(mode);
        m_view = tcu::Texture2DView(numLevels, m_levels.getEffectiveLevels() + baseLevel);
    }
    else
        m_view = tcu::Texture2DView(0, nullptr);
}

Texture2D::Texture2D(uint32_t name, bool es2) : Texture(name, TYPE_2D), m_view(0, nullptr, es2)
{
}

Texture2D::~Texture2D(void)
{
}

void Texture2D::allocLevel(int level, const tcu::TextureFormat &format, int width, int height)
{
    m_levels.allocLevel(level, format, width, height, 1);
}

bool Texture2D::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel))
    {
        const tcu::ConstPixelBufferAccess &level0 = getLevel(baseLevel);
        const bool mipmap                         = isMipmapFilter(getSampler().minFilter);

        if (mipmap)
        {
            const TextureFormat &format = level0.getFormat();
            const int w                 = level0.getWidth();
            const int h                 = level0.getHeight();
            const int numLevels         = de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(w, h));

            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                if (hasLevel(baseLevel + levelNdx))
                {
                    const tcu::ConstPixelBufferAccess &level = getLevel(baseLevel + levelNdx);
                    const int expectedW                      = getMipLevelSize(w, levelNdx);
                    const int expectedH                      = getMipLevelSize(h, levelNdx);

                    if (level.getWidth() != expectedW || level.getHeight() != expectedH || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

void Texture2D::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel) && !isEmpty(getLevel(baseLevel)))
    {
        // Update number of levels in mipmap pyramid.
        const int width     = getLevel(baseLevel).getWidth();
        const int height    = getLevel(baseLevel).getHeight();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels = isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(width, height)) : 1;

        m_levels.updateSamplerMode(mode);
        m_view = tcu::Texture2DView(numLevels, m_levels.getEffectiveLevels() + baseLevel);
    }
    else
        m_view = tcu::Texture2DView(0, nullptr);
}

tcu::Vec4 Texture2D::sample(float s, float t, float lod) const
{
    return m_view.sample(getSampler(), s, t, lod);
}

void Texture2D::sample4(tcu::Vec4 output[4], const tcu::Vec2 packetTexcoords[4], float lodBias) const
{
    const float texWidth  = (float)m_view.getWidth();
    const float texHeight = (float)m_view.getHeight();

    const tcu::Vec2 dFdx0 = packetTexcoords[1] - packetTexcoords[0];
    const tcu::Vec2 dFdx1 = packetTexcoords[3] - packetTexcoords[2];
    const tcu::Vec2 dFdy0 = packetTexcoords[2] - packetTexcoords[0];
    const tcu::Vec2 dFdy1 = packetTexcoords[3] - packetTexcoords[1];

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const tcu::Vec2 &dFdx = (fragNdx & 2) ? dFdx1 : dFdx0;
        const tcu::Vec2 &dFdy = (fragNdx & 1) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx.x()), de::abs(dFdy.x()));
        const float mv = de::max(de::abs(dFdx.y()), de::abs(dFdy.y()));
        const float p  = de::max(mu * texWidth, mv * texHeight);

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] = sample(packetTexcoords[fragNdx].x(), packetTexcoords[fragNdx].y(), lod);
    }
}

TextureCube::TextureCube(uint32_t name, bool seamless) : Texture(name, TYPE_CUBE_MAP, seamless)
{
}

TextureCube::~TextureCube(void)
{
}

void TextureCube::clearLevels(void)
{
    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        m_levels[face].clear();
}

void TextureCube::allocFace(int level, tcu::CubeFace face, const tcu::TextureFormat &format, int width, int height)
{
    m_levels[face].allocLevel(level, format, width, height, 1);
}

bool TextureCube::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasFace(baseLevel, tcu::CUBEFACE_NEGATIVE_X))
    {
        const int width                  = getFace(baseLevel, tcu::CUBEFACE_NEGATIVE_X).getWidth();
        const int height                 = getFace(baseLevel, tcu::CUBEFACE_NEGATIVE_X).getHeight();
        const tcu::TextureFormat &format = getFace(baseLevel, tcu::CUBEFACE_NEGATIVE_X).getFormat();
        const bool mipmap                = isMipmapFilter(getSampler().minFilter);
        const int numLevels = mipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(width, height)) : 1;

        if (width != height)
            return false; // Non-square is not supported.

        // \note Level 0 is always checked for consistency
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const int levelW = getMipLevelSize(width, levelNdx);
            const int levelH = getMipLevelSize(height, levelNdx);

            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            {
                if (hasFace(baseLevel + levelNdx, (tcu::CubeFace)face))
                {
                    const tcu::ConstPixelBufferAccess &level = getFace(baseLevel + levelNdx, (tcu::CubeFace)face);

                    if (level.getWidth() != levelW || level.getHeight() != levelH || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

void TextureCube::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();
    const tcu::ConstPixelBufferAccess *faces[tcu::CUBEFACE_LAST];

    deMemset(&faces[0], 0, sizeof(faces));

    if (isComplete())
    {
        const int size      = getFace(baseLevel, tcu::CUBEFACE_NEGATIVE_X).getWidth();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels = isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels1D(size)) : 1;

        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            m_levels[face].updateSamplerMode(mode);
            faces[face] = m_levels[face].getEffectiveLevels() + baseLevel;
        }

        m_view = tcu::TextureCubeView(numLevels, faces);
    }
    else
        m_view = tcu::TextureCubeView(0, faces);
}

tcu::Vec4 TextureCube::sample(float s, float t, float p, float lod) const
{
    return m_view.sample(getSampler(), s, t, p, lod);
}

void TextureCube::sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias) const
{
    const float cubeSide = (float)m_view.getSize();

    // Each tex coord might be in a different face.

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const tcu::CubeFace face  = tcu::selectCubeFace(packetTexcoords[fragNdx]);
        const tcu::Vec2 coords[4] = {
            tcu::projectToFace(face, packetTexcoords[0]),
            tcu::projectToFace(face, packetTexcoords[1]),
            tcu::projectToFace(face, packetTexcoords[2]),
            tcu::projectToFace(face, packetTexcoords[3]),
        };

        const tcu::Vec2 dFdx0 = coords[1] - coords[0];
        const tcu::Vec2 dFdx1 = coords[3] - coords[2];
        const tcu::Vec2 dFdy0 = coords[2] - coords[0];
        const tcu::Vec2 dFdy1 = coords[3] - coords[1];

        const tcu::Vec2 &dFdx = (fragNdx & 2) ? dFdx1 : dFdx0;
        const tcu::Vec2 &dFdy = (fragNdx & 1) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx.x()), de::abs(dFdy.x()));
        const float mv = de::max(de::abs(dFdx.y()), de::abs(dFdy.y()));
        const float p  = de::max(mu * cubeSide, mv * cubeSide);

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] =
            sample(packetTexcoords[fragNdx].x(), packetTexcoords[fragNdx].y(), packetTexcoords[fragNdx].z(), lod);
    }
}

Texture2DArray::Texture2DArray(uint32_t name) : Texture(name, TYPE_2D_ARRAY), m_view(0, nullptr)
{
}

Texture2DArray::~Texture2DArray(void)
{
}

void Texture2DArray::allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int numLayers)
{
    m_levels.allocLevel(level, format, width, height, numLayers);
}

bool Texture2DArray::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel))
    {
        const tcu::ConstPixelBufferAccess &level0 = getLevel(baseLevel);
        const bool mipmap                         = isMipmapFilter(getSampler().minFilter);

        if (mipmap)
        {
            const TextureFormat &format = level0.getFormat();
            const int w                 = level0.getWidth();
            const int h                 = level0.getHeight();
            const int numLayers         = level0.getDepth();
            const int numLevels         = de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(w, h));

            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                if (hasLevel(baseLevel + levelNdx))
                {
                    const tcu::ConstPixelBufferAccess &level = getLevel(baseLevel + levelNdx);
                    const int expectedW                      = getMipLevelSize(w, levelNdx);
                    const int expectedH                      = getMipLevelSize(h, levelNdx);

                    if (level.getWidth() != expectedW || level.getHeight() != expectedH ||
                        level.getDepth() != numLayers || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

void Texture2DArray::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel) && !isEmpty(getLevel(baseLevel)))
    {
        const int width     = getLevel(baseLevel).getWidth();
        const int height    = getLevel(baseLevel).getHeight();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels = isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(width, height)) : 1;

        m_levels.updateSamplerMode(mode);
        m_view = tcu::Texture2DArrayView(numLevels, m_levels.getEffectiveLevels() + baseLevel);
    }
    else
        m_view = tcu::Texture2DArrayView(0, nullptr);
}

tcu::Vec4 Texture2DArray::sample(float s, float t, float r, float lod) const
{
    return m_view.sample(getSampler(), s, t, r, lod);
}

void Texture2DArray::sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias) const
{
    const float texWidth  = (float)m_view.getWidth();
    const float texHeight = (float)m_view.getHeight();

    const tcu::Vec3 dFdx0 = packetTexcoords[1] - packetTexcoords[0];
    const tcu::Vec3 dFdx1 = packetTexcoords[3] - packetTexcoords[2];
    const tcu::Vec3 dFdy0 = packetTexcoords[2] - packetTexcoords[0];
    const tcu::Vec3 dFdy1 = packetTexcoords[3] - packetTexcoords[1];

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const tcu::Vec3 &dFdx = (fragNdx & 2) ? dFdx1 : dFdx0;
        const tcu::Vec3 &dFdy = (fragNdx & 1) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx.x()), de::abs(dFdy.x()));
        const float mv = de::max(de::abs(dFdx.y()), de::abs(dFdy.y()));
        const float p  = de::max(mu * texWidth, mv * texHeight);

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] =
            sample(packetTexcoords[fragNdx].x(), packetTexcoords[fragNdx].y(), packetTexcoords[fragNdx].z(), lod);
    }
}

TextureCubeArray::TextureCubeArray(uint32_t name) : Texture(name, TYPE_CUBE_MAP_ARRAY), m_view(0, nullptr)
{
}

TextureCubeArray::~TextureCubeArray(void)
{
}

void TextureCubeArray::allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int numLayers)
{
    DE_ASSERT(numLayers % 6 == 0);
    m_levels.allocLevel(level, format, width, height, numLayers);
}

bool TextureCubeArray::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel))
    {
        const tcu::ConstPixelBufferAccess &level0 = getLevel(baseLevel);
        const bool mipmap                         = isMipmapFilter(getSampler().minFilter);

        if (mipmap)
        {
            const TextureFormat &format = level0.getFormat();
            const int w                 = level0.getWidth();
            const int h                 = level0.getHeight();
            const int numLayers         = level0.getDepth();
            const int numLevels         = de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(w, h));

            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                if (hasLevel(baseLevel + levelNdx))
                {
                    const tcu::ConstPixelBufferAccess &level = getLevel(baseLevel + levelNdx);
                    const int expectedW                      = getMipLevelSize(w, levelNdx);
                    const int expectedH                      = getMipLevelSize(h, levelNdx);

                    if (level.getWidth() != expectedW || level.getHeight() != expectedH ||
                        level.getDepth() != numLayers || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

void TextureCubeArray::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel) && !isEmpty(getLevel(baseLevel)))
    {
        const int width     = getLevel(baseLevel).getWidth();
        const int height    = getLevel(baseLevel).getHeight();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels = isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels2D(width, height)) : 1;

        m_levels.updateSamplerMode(mode);
        m_view = tcu::TextureCubeArrayView(numLevels, m_levels.getEffectiveLevels() + baseLevel);
    }
    else
        m_view = tcu::TextureCubeArrayView(0, nullptr);
}

tcu::Vec4 TextureCubeArray::sample(float s, float t, float r, float q, float lod) const
{
    return m_view.sample(getSampler(), s, t, r, q, lod);
}

void TextureCubeArray::sample4(tcu::Vec4 output[4], const tcu::Vec4 packetTexcoords[4], float lodBias) const
{
    const float cubeSide          = (float)m_view.getSize();
    const tcu::Vec3 cubeCoords[4] = {packetTexcoords[0].toWidth<3>(), packetTexcoords[1].toWidth<3>(),
                                     packetTexcoords[2].toWidth<3>(), packetTexcoords[3].toWidth<3>()};

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const tcu::CubeFace face      = tcu::selectCubeFace(cubeCoords[fragNdx]);
        const tcu::Vec2 faceCoords[4] = {
            tcu::projectToFace(face, cubeCoords[0]),
            tcu::projectToFace(face, cubeCoords[1]),
            tcu::projectToFace(face, cubeCoords[2]),
            tcu::projectToFace(face, cubeCoords[3]),
        };

        const tcu::Vec2 dFdx0 = faceCoords[1] - faceCoords[0];
        const tcu::Vec2 dFdx1 = faceCoords[3] - faceCoords[2];
        const tcu::Vec2 dFdy0 = faceCoords[2] - faceCoords[0];
        const tcu::Vec2 dFdy1 = faceCoords[3] - faceCoords[1];

        const tcu::Vec2 &dFdx = (fragNdx & 2) ? dFdx1 : dFdx0;
        const tcu::Vec2 &dFdy = (fragNdx & 1) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx.x()), de::abs(dFdy.x()));
        const float mv = de::max(de::abs(dFdx.y()), de::abs(dFdy.y()));
        const float p  = de::max(mu * cubeSide, mv * cubeSide);

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] = sample(packetTexcoords[fragNdx].x(), packetTexcoords[fragNdx].y(),
                                 packetTexcoords[fragNdx].z(), packetTexcoords[fragNdx].w(), lod);
    }
}

Texture3D::Texture3D(uint32_t name) : Texture(name, TYPE_3D), m_view(0, nullptr)
{
}

Texture3D::~Texture3D(void)
{
}

void Texture3D::allocLevel(int level, const tcu::TextureFormat &format, int width, int height, int depth)
{
    m_levels.allocLevel(level, format, width, height, depth);
}

bool Texture3D::isComplete(void) const
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel))
    {
        const tcu::ConstPixelBufferAccess &level0 = getLevel(baseLevel);
        const bool mipmap                         = isMipmapFilter(getSampler().minFilter);

        if (mipmap)
        {
            const TextureFormat &format = level0.getFormat();
            const int w                 = level0.getWidth();
            const int h                 = level0.getHeight();
            const int d                 = level0.getDepth();
            const int numLevels         = de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels3D(w, h, d));

            for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
            {
                if (hasLevel(baseLevel + levelNdx))
                {
                    const tcu::ConstPixelBufferAccess &level = getLevel(baseLevel + levelNdx);
                    const int expectedW                      = getMipLevelSize(w, levelNdx);
                    const int expectedH                      = getMipLevelSize(h, levelNdx);
                    const int expectedD                      = getMipLevelSize(d, levelNdx);

                    if (level.getWidth() != expectedW || level.getHeight() != expectedH ||
                        level.getDepth() != expectedD || level.getFormat() != format)
                        return false;
                }
                else
                    return false;
            }
        }

        return true;
    }
    else
        return false;
}

tcu::Vec4 Texture3D::sample(float s, float t, float r, float lod) const
{
    return m_view.sample(getSampler(), s, t, r, lod);
}

void Texture3D::sample4(tcu::Vec4 output[4], const tcu::Vec3 packetTexcoords[4], float lodBias) const
{
    const float texWidth  = (float)m_view.getWidth();
    const float texHeight = (float)m_view.getHeight();
    const float texDepth  = (float)m_view.getDepth();

    const tcu::Vec3 dFdx0 = packetTexcoords[1] - packetTexcoords[0];
    const tcu::Vec3 dFdx1 = packetTexcoords[3] - packetTexcoords[2];
    const tcu::Vec3 dFdy0 = packetTexcoords[2] - packetTexcoords[0];
    const tcu::Vec3 dFdy1 = packetTexcoords[3] - packetTexcoords[1];

    for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
    {
        const tcu::Vec3 &dFdx = (fragNdx & 2) ? dFdx1 : dFdx0;
        const tcu::Vec3 &dFdy = (fragNdx & 1) ? dFdy1 : dFdy0;

        const float mu = de::max(de::abs(dFdx.x()), de::abs(dFdy.x()));
        const float mv = de::max(de::abs(dFdx.y()), de::abs(dFdy.y()));
        const float mw = de::max(de::abs(dFdx.z()), de::abs(dFdy.z()));
        const float p  = de::max(de::max(mu * texWidth, mv * texHeight), mw * texDepth);

        const float lod = deFloatLog2(p) + lodBias;

        output[fragNdx] =
            sample(packetTexcoords[fragNdx].x(), packetTexcoords[fragNdx].y(), packetTexcoords[fragNdx].z(), lod);
    }
}

void Texture3D::updateView(tcu::Sampler::DepthStencilMode mode)
{
    const int baseLevel = getBaseLevel();

    if (hasLevel(baseLevel) && !isEmpty(getLevel(baseLevel)))
    {
        const int width     = getLevel(baseLevel).getWidth();
        const int height    = getLevel(baseLevel).getHeight();
        const int depth     = getLevel(baseLevel).getDepth();
        const bool isMipmap = isMipmapFilter(getSampler().minFilter);
        const int numLevels =
            isMipmap ? de::min(getMaxLevel() - baseLevel + 1, getNumMipLevels3D(width, height, depth)) : 1;

        m_levels.updateSamplerMode(mode);
        m_view = tcu::Texture3DView(numLevels, m_levels.getEffectiveLevels() + baseLevel);
    }
    else
        m_view = tcu::Texture3DView(0, nullptr);
}

Renderbuffer::Renderbuffer(uint32_t name) : NamedObject(name)
{
}

Renderbuffer::~Renderbuffer(void)
{
}

void Renderbuffer::setStorage(const TextureFormat &format, int width, int height)
{
    m_data.setStorage(format, width, height);
}

Framebuffer::Framebuffer(uint32_t name) : NamedObject(name)
{
}

Framebuffer::~Framebuffer(void)
{
}

VertexArray::VertexArray(uint32_t name, int maxVertexAttribs)
    : NamedObject(name)
    , m_elementArrayBufferBinding(nullptr)
    , m_arrays(maxVertexAttribs)
{
    for (int i = 0; i < maxVertexAttribs; ++i)
    {
        m_arrays[i].enabled       = false;
        m_arrays[i].size          = 4;
        m_arrays[i].stride        = 0;
        m_arrays[i].type          = GL_FLOAT;
        m_arrays[i].normalized    = false;
        m_arrays[i].integer       = false;
        m_arrays[i].divisor       = 0;
        m_arrays[i].bufferDeleted = false;
        m_arrays[i].bufferBinding = nullptr;
        m_arrays[i].pointer       = nullptr;
    }
}

ShaderProgramObjectContainer::ShaderProgramObjectContainer(uint32_t name, ShaderProgram *program)
    : NamedObject(name)
    , m_program(program)
    , m_deleteFlag(false)
{
}

ShaderProgramObjectContainer::~ShaderProgramObjectContainer(void)
{
}

} // namespace rc
} // namespace sglr
