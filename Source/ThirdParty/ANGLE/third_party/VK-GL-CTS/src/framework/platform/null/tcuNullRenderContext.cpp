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
 * \brief Render context implementation that does no rendering.
 *//*--------------------------------------------------------------------*/

#include "tcuNullRenderContext.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "deThreadLocal.hpp"
#include "gluRenderConfig.hpp"
#include "gluTextureUtil.hpp"
#include "glwEnums.hpp"

#include <string>
#include <vector>

namespace tcu
{
namespace null
{

using namespace glw;

#include "tcuNullRenderContextFuncs.inl"

using namespace glu;
using std::string;
using std::vector;

class ObjectManager
{
public:
    ObjectManager(void) : m_lastObject(0)
    {
    }

    uint32_t allocate(void)
    {
        uint32_t object = ++m_lastObject;
        if (object == 0)
            object = ++m_lastObject; // Just ignore overflow.
        return object;
    }

    void free(uint32_t object)
    {
        DE_UNREF(object);
    }

private:
    uint32_t m_lastObject;
};

class Context
{
public:
    Context(ContextType ctxType_);
    ~Context(void);

private:
    Context(const Context &);
    Context &operator=(const Context &);

    void addExtension(const char *name);

public:
    // GL state exposed to implementation functions.
    const ContextType ctxType;

    string vendor;
    string version;
    string renderer;
    string shadingLanguageVersion;
    string extensions;
    vector<string> extensionList;
    vector<uint32_t> compressedTextureList;

    GLenum lastError;

    int pixelPackRowLength;
    int pixelPackSkipRows;
    int pixelPackSkipPixels;
    int pixelPackAlignment;

    GLuint pixelPackBufferBufferBinding;

    ObjectManager shaders;
    ObjectManager programs;
    ObjectManager textures;
    ObjectManager buffers;
    ObjectManager renderbuffers;
    ObjectManager framebuffers;
    ObjectManager samplers;
    ObjectManager vertexArrays;
    ObjectManager queries;
    ObjectManager transformFeedbacks;
    ObjectManager programPipelines;
};

Context::Context(ContextType ctxType_)
    : ctxType(ctxType_)
    , vendor("drawElements")
    , renderer("dummy")
    , lastError(GL_NO_ERROR)
    , pixelPackRowLength(0)
    , pixelPackSkipRows(0)
    , pixelPackSkipPixels(0)
    , pixelPackAlignment(0)
    , pixelPackBufferBufferBinding(0)
{
    using glu::ApiType;

    if (ctxType.getAPI() == ApiType::es(2, 0))
    {
        version                = "OpenGL ES 2.0";
        shadingLanguageVersion = "OpenGL ES GLSL ES 1.0";
    }
    else if (ctxType.getAPI() == ApiType::es(3, 0))
    {
        version                = "OpenGL ES 3.0";
        shadingLanguageVersion = "OpenGL ES GLSL ES 3.0";
    }
    else if (ctxType.getAPI() == ApiType::es(3, 1))
    {
        version                = "OpenGL ES 3.1";
        shadingLanguageVersion = "OpenGL ES GLSL ES 3.1";
        addExtension("GL_OES_texture_stencil8");
        addExtension("GL_OES_sample_shading");
        addExtension("GL_OES_sample_variables");
        addExtension("GL_OES_shader_multisample_interpolation");
        addExtension("GL_OES_shader_image_atomic");
        addExtension("GL_OES_texture_storage_multisample_2d_array");
        addExtension("GL_KHR_blend_equation_advanced");
        addExtension("GL_KHR_blend_equation_advanced_coherent");
        addExtension("GL_EXT_shader_io_blocks");
        addExtension("GL_EXT_geometry_shader");
        addExtension("GL_EXT_geometry_point_size");
        addExtension("GL_EXT_tessellation_shader");
        addExtension("GL_EXT_tessellation_point_size");
        addExtension("GL_EXT_gpu_shader5");
        addExtension("GL_EXT_shader_implicit_conversions");
        addExtension("GL_EXT_texture_buffer");
        addExtension("GL_EXT_texture_cube_map_array");
        addExtension("GL_EXT_draw_buffers_indexed");
        addExtension("GL_EXT_texture_sRGB_decode");
        addExtension("GL_EXT_texture_border_clamp");
        addExtension("GL_KHR_debug");
        addExtension("GL_EXT_primitive_bounding_box");
        addExtension("GL_ANDROID_extension_pack_es31a");
        addExtension("GL_EXT_copy_image");
    }
    else if (ctxType.getAPI() == ApiType::es(3, 2))
    {
        version                = "OpenGL ES 3.2";
        shadingLanguageVersion = "OpenGL ES GLSL ES 3.2";
    }
    else if (glu::isContextTypeGLCore(ctxType) && ctxType.getMajorVersion() == 3)
    {
        version                = "3.3.0";
        shadingLanguageVersion = "3.30";
    }
    else if (glu::isContextTypeGLCore(ctxType) && ctxType.getMajorVersion() == 4 && ctxType.getMinorVersion() <= 4)
    {
        version                = "4.4.0";
        shadingLanguageVersion = "4.40";
    }
    else if (glu::isContextTypeGLCore(ctxType) && ctxType.getMajorVersion() == 4 && ctxType.getMinorVersion() == 5)
    {
        version                = "4.5.0";
        shadingLanguageVersion = "4.50";
    }
    else if (glu::isContextTypeGLCore(ctxType) && ctxType.getMajorVersion() == 4 && ctxType.getMinorVersion() == 6)
    {
        version                = "4.6.0";
        shadingLanguageVersion = "4.60";
    }
    else if (glu::isContextTypeGLCompatibility(ctxType) && ctxType.getMajorVersion() == 4 &&
             ctxType.getMinorVersion() <= 2)
    {
        version                = "4.2.0";
        shadingLanguageVersion = "4.20";
    }
    else
        throw tcu::NotSupportedError("Unsupported GL version", "", __FILE__, __LINE__);

    if (isContextTypeES(ctxType))
    {
        addExtension("GL_EXT_color_buffer_float");
        addExtension("GL_EXT_color_buffer_half_float");
    }

    // support compressed formats
    {
        static uint32_t compressedFormats[] = {
            GL_ETC1_RGB8_OES,
            GL_COMPRESSED_R11_EAC,
            GL_COMPRESSED_SIGNED_R11_EAC,
            GL_COMPRESSED_RG11_EAC,
            GL_COMPRESSED_SIGNED_RG11_EAC,
            GL_COMPRESSED_RGB8_ETC2,
            GL_COMPRESSED_SRGB8_ETC2,
            GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
            GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
            GL_COMPRESSED_RGBA8_ETC2_EAC,
            GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
            GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
            GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
            GL_COMPRESSED_RGBA_ASTC_5x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_6x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x8_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
            GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
            GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,
        };

        addExtension("GL_KHR_texture_compression_astc_hdr");
        addExtension("GL_KHR_texture_compression_astc_ldr");
        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(compressedFormats); ++ndx)
            compressedTextureList.push_back(compressedFormats[ndx]);
    }
}

Context::~Context(void)
{
}

void Context::addExtension(const char *name)
{
    if (!extensions.empty())
        extensions += " ";
    extensions += name;

    extensionList.push_back(name);
}

static de::ThreadLocal s_currentCtx;

void setCurrentContext(Context *context)
{
    s_currentCtx.set((void *)context);
}

Context *getCurrentContext(void)
{
    return (Context *)s_currentCtx.get();
}

GLW_APICALL GLenum GLW_APIENTRY glGetError(void)
{
    Context *const ctx   = getCurrentContext();
    const GLenum lastErr = ctx->lastError;

    ctx->lastError = GL_NO_ERROR;

    return lastErr;
}

GLW_APICALL void GLW_APIENTRY glGetIntegerv(GLenum pname, GLint *params)
{
    Context *const ctx = getCurrentContext();

    switch (pname)
    {
    case GL_NUM_EXTENSIONS:
        *params = (int)ctx->extensionList.size();
        break;

    case GL_MAX_VERTEX_ATTRIBS:
        *params = 32;
        break;

    case GL_MAX_DRAW_BUFFERS:
    case GL_MAX_COLOR_ATTACHMENTS:
        *params = 8;
        break;

    case GL_MAX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
    case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS:
    case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS:
    case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS:
        *params = 32;
        break;

    case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
    case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
    case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS:
    case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS:
    case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
        *params = 8;
        break;

    case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
        *params = 8;
        break;

    case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
        *params = 1u << 25;
        break;

    case GL_MAX_GEOMETRY_OUTPUT_VERTICES:
        *params = 256;
        break;

    case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
        *params = 2048;
        break;

    case GL_MAX_GEOMETRY_SHADER_INVOCATIONS:
        *params = 4;
        break;

    case GL_MAX_COLOR_TEXTURE_SAMPLES:
        *params = 8;
        break;

    case GL_MAX_TEXTURE_SIZE:
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
    case GL_MAX_3D_TEXTURE_SIZE:
    case GL_MAX_RENDERBUFFER_SIZE:
    case GL_MAX_TEXTURE_BUFFER_SIZE:
        *params = 2048;
        break;

    case GL_MAX_ARRAY_TEXTURE_LAYERS:
        *params = 128;
        break;

    case GL_NUM_SHADER_BINARY_FORMATS:
        *params = 0;
        break;

    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
        *params = (int)ctx->compressedTextureList.size();
        break;

    case GL_COMPRESSED_TEXTURE_FORMATS:
        deMemcpy(params, &ctx->compressedTextureList[0], ctx->compressedTextureList.size() * sizeof(uint32_t));
        break;

    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        *params = 16;
        break;

    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
        *params = 32;
        break;

    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        *params = 16;
        break;

    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        *params = GL_RGBA;
        break;

    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        *params = GL_UNSIGNED_BYTE;
        break;

    case GL_SAMPLE_BUFFERS:
        *params = 0;
        break;

    default:
        break;
    }
}

GLW_APICALL void GLW_APIENTRY glGetBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
    case GL_SHADER_COMPILER:
        *params = GL_TRUE;
        break;

    default:
        break;
    }
}

GLW_APICALL void GLW_APIENTRY glGetFloatv(GLenum pname, GLfloat *params)
{
    switch (pname)
    {
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_ALIASED_POINT_SIZE_RANGE:
        params[0] = 0.0f;
        params[1] = 64.0f;
        break;

    default:
        break;
    }
}

GLW_APICALL void GLW_APIENTRY glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize,
                                                    GLint *params)
{
    static const int s_sampleCounts[] = {16, 8, 4, 2, 1};

    DE_UNREF(internalformat);
    DE_UNREF(target);

    switch (pname)
    {
    case GL_NUM_SAMPLE_COUNTS:
        if (bufSize >= 1)
            *params = DE_LENGTH_OF_ARRAY(s_sampleCounts);
        break;

    case GL_SAMPLES:
        deMemcpy(params, s_sampleCounts, de::min(bufSize, DE_LENGTH_OF_ARRAY(s_sampleCounts)));
        break;

    default:
        break;
    }
}

GLW_APICALL const glw::GLubyte *GLW_APIENTRY glGetString(GLenum name)
{
    Context *const ctx = getCurrentContext();

    switch (name)
    {
    case GL_VENDOR:
        return (const glw::GLubyte *)ctx->vendor.c_str();
    case GL_VERSION:
        return (const glw::GLubyte *)ctx->version.c_str();
    case GL_RENDERER:
        return (const glw::GLubyte *)ctx->renderer.c_str();
    case GL_SHADING_LANGUAGE_VERSION:
        return (const glw::GLubyte *)ctx->shadingLanguageVersion.c_str();
    case GL_EXTENSIONS:
        return (const glw::GLubyte *)ctx->extensions.c_str();
    default:
        ctx->lastError = GL_INVALID_ENUM;
        return nullptr;
    }
}

GLW_APICALL const glw::GLubyte *GLW_APIENTRY glGetStringi(GLenum name, GLuint index)
{
    Context *const ctx = getCurrentContext();

    if (name == GL_EXTENSIONS)
    {
        if ((size_t)index < ctx->extensionList.size())
            return (const glw::GLubyte *)ctx->extensionList[index].c_str();
        else
        {
            ctx->lastError = GL_INVALID_VALUE;
            return nullptr;
        }
    }
    else
    {
        ctx->lastError = GL_INVALID_ENUM;
        return nullptr;
    }
}

GLW_APICALL GLuint GLW_APIENTRY glCreateProgram()
{
    Context *const ctx = getCurrentContext();
    return (GLuint)ctx->programs.allocate();
}

GLW_APICALL GLuint GLW_APIENTRY glCreateShader(GLenum type)
{
    Context *const ctx = getCurrentContext();
    DE_UNREF(type);
    return (GLuint)ctx->shaders.allocate();
}

GLW_APICALL void GLW_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    DE_UNREF(shader);

    if (pname == GL_COMPILE_STATUS)
        *params = GL_TRUE;
}

GLW_APICALL void GLW_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    DE_UNREF(program);

    if (pname == GL_LINK_STATUS)
        *params = GL_TRUE;
}

GLW_APICALL void GLW_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
    Context *const ctx = getCurrentContext();

    if (textures)
    {
        for (int ndx = 0; ndx < n; ndx++)
            textures[ndx] = ctx->textures.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenQueries(GLsizei n, GLuint *ids)
{
    Context *const ctx = getCurrentContext();

    if (ids)
    {
        for (int ndx = 0; ndx < n; ndx++)
            ids[ndx] = ctx->queries.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
    Context *const ctx = getCurrentContext();

    if (buffers)
    {
        for (int ndx = 0; ndx < n; ndx++)
            buffers[ndx] = ctx->buffers.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    Context *const ctx = getCurrentContext();

    if (renderbuffers)
    {
        for (int ndx = 0; ndx < n; ndx++)
            renderbuffers[ndx] = ctx->renderbuffers.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    Context *const ctx = getCurrentContext();

    if (framebuffers)
    {
        for (int ndx = 0; ndx < n; ndx++)
            framebuffers[ndx] = ctx->framebuffers.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays)
{
    Context *const ctx = getCurrentContext();

    if (arrays)
    {
        for (int ndx = 0; ndx < n; ndx++)
            arrays[ndx] = ctx->vertexArrays.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenSamplers(GLsizei count, GLuint *samplers)
{
    Context *const ctx = getCurrentContext();

    if (samplers)
    {
        for (int ndx = 0; ndx < count; ndx++)
            samplers[ndx] = ctx->samplers.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
    Context *const ctx = getCurrentContext();

    if (ids)
    {
        for (int ndx = 0; ndx < n; ndx++)
            ids[ndx] = ctx->transformFeedbacks.allocate();
    }
}

GLW_APICALL void GLW_APIENTRY glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
    Context *const ctx = getCurrentContext();

    if (pipelines)
    {
        for (int ndx = 0; ndx < n; ndx++)
            pipelines[ndx] = ctx->programPipelines.allocate();
    }
}

GLW_APICALL GLvoid *GLW_APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    Context *const ctx = getCurrentContext();

    DE_UNREF(target);
    DE_UNREF(offset);
    DE_UNREF(length);
    DE_UNREF(access);

    if (ctx->lastError == GL_NO_ERROR)
        ctx->lastError = GL_INVALID_OPERATION;

    return (GLvoid *)0;
}

GLW_APICALL GLenum GLW_APIENTRY glCheckFramebufferStatus(GLenum target)
{
    DE_UNREF(target);
    return GL_FRAMEBUFFER_COMPLETE;
}

GLW_APICALL void GLW_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                           GLvoid *pixels)
{
    DE_UNREF(x);
    DE_UNREF(y);

    Context *const ctx = getCurrentContext();
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // black
    const tcu::TextureFormat transferFormat = glu::mapGLTransferFormat(format, type);

    // invalid formats
    if (transferFormat.order == TextureFormat::CHANNELORDER_LAST ||
        transferFormat.type == TextureFormat::CHANNELTYPE_LAST)
    {
        if (ctx->lastError == GL_NO_ERROR)
            ctx->lastError = GL_INVALID_ENUM;
        return;
    }

    // unsupported formats
    if (!(format == GL_RGBA && type == GL_UNSIGNED_BYTE) && !(format == GL_RGBA_INTEGER && type == GL_INT) &&
        !(format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT) && !(format == GL_RGBA && type == GL_FLOAT))
    {
        if (ctx->lastError == GL_NO_ERROR)
            ctx->lastError = GL_INVALID_ENUM;
        return;
    }

    // invalid arguments
    if (width < 0 || height < 0)
    {
        if (ctx->lastError == GL_NO_ERROR)
            ctx->lastError = GL_INVALID_OPERATION;
        return;
    }

    // read to buffer
    if (ctx->pixelPackBufferBufferBinding)
        return;

    // read to use pointer
    {
        const int targetRowLength  = (ctx->pixelPackRowLength != 0) ? (ctx->pixelPackRowLength) : (width);
        const int targetSkipRows   = ctx->pixelPackSkipRows;
        const int targetSkipPixels = ctx->pixelPackSkipPixels;
        const int infiniteHeight   = targetSkipRows + height; // as much as needed
        const int targetRowPitch =
            (ctx->pixelPackAlignment == 0) ?
                (targetRowLength * transferFormat.getPixelSize()) :
                (deAlign32(targetRowLength * transferFormat.getPixelSize(), ctx->pixelPackAlignment));

        // Create access to the whole copy target
        const tcu::PixelBufferAccess targetAccess(transferFormat, targetRowLength, infiniteHeight, 1, targetRowPitch, 0,
                                                  pixels);

        // Select (skip_pixels, skip_rows, width, height) subregion from it. Clip to horizontal boundaries
        const tcu::PixelBufferAccess targetRectAccess =
            tcu::getSubregion(targetAccess, de::clamp(targetSkipPixels, 0, targetAccess.getWidth() - 1), targetSkipRows,
                              de::clamp(width, 0, de::max(0, targetAccess.getWidth() - targetSkipPixels)), height);

        tcu::clear(targetRectAccess, clearColor);
    }
}

GLW_APICALL void GLW_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
    Context *const ctx = getCurrentContext();

    if (target == GL_PIXEL_PACK_BUFFER)
        ctx->pixelPackBufferBufferBinding = buffer;
}

GLW_APICALL void GLW_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    Context *const ctx = getCurrentContext();

    for (GLsizei ndx = 0; ndx < n; ++ndx)
        if (buffers[ndx] && buffers[ndx] == ctx->pixelPackBufferBufferBinding)
            ctx->pixelPackBufferBufferBinding = 0;
}

GLW_APICALL GLint GLW_APIENTRY glGetAttribLocation(GLuint program, const GLchar *name)
{
    DE_UNREF(program);
    return (GLint)(deStringHash(name) & 0x7FFFFFFF);
}

void initFunctions(glw::Functions *gl)
{
#include "tcuNullRenderContextInitFuncs.inl"
}

static tcu::RenderTarget toRenderTarget(const RenderConfig &renderCfg)
{
    const int width       = getValueOrDefault(renderCfg, &RenderConfig::width, 256);
    const int height      = getValueOrDefault(renderCfg, &RenderConfig::height, 256);
    const int redBits     = getValueOrDefault(renderCfg, &RenderConfig::redBits, 8);
    const int greenBits   = getValueOrDefault(renderCfg, &RenderConfig::greenBits, 8);
    const int blueBits    = getValueOrDefault(renderCfg, &RenderConfig::blueBits, 8);
    const int alphaBits   = getValueOrDefault(renderCfg, &RenderConfig::alphaBits, 8);
    const int depthBits   = getValueOrDefault(renderCfg, &RenderConfig::depthBits, 24);
    const int stencilBits = getValueOrDefault(renderCfg, &RenderConfig::stencilBits, 8);
    const int numSamples  = getValueOrDefault(renderCfg, &RenderConfig::numSamples, 0);

    return tcu::RenderTarget(width, height, tcu::PixelFormat(redBits, greenBits, blueBits, alphaBits), depthBits,
                             stencilBits, numSamples);
}

RenderContext::RenderContext(const RenderConfig &renderCfg)
    : m_ctxType(renderCfg.type)
    , m_renderTarget(toRenderTarget(renderCfg))
    , m_context(nullptr)
{
    m_context = new Context(m_ctxType);

    initFunctions(&m_functions);
    setCurrentContext(m_context);
}

RenderContext::~RenderContext(void)
{
    setCurrentContext(nullptr);
    delete m_context;
}

void RenderContext::postIterate(void)
{
}

void RenderContext::makeCurrent(void)
{
}

} // namespace null
} // namespace tcu
