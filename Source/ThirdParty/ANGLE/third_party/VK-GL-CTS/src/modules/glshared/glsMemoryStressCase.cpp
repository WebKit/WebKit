/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Module
 * ---------------------------------------------
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
 * \brief Memory object stress test
 *//*--------------------------------------------------------------------*/

#include "glsMemoryStressCase.hpp"
#include "gluShaderProgram.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"
#include "deClock.h"
#include "deString.h"

#include "glw.h"

#include <vector>
#include <iostream>

using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gls
{

static const char *glErrorToString(uint32_t error)
{
    switch (error)
    {
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";

    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";

    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";

    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";

    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";

    case 0:
        return "<none>";

    default:
        // \todo [mika] Handle uknown errors?
        DE_ASSERT(false);
        return NULL;
    }
}

static const float s_quadCoords[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

static const GLubyte s_quadIndices[] = {0, 1, 2, 2, 3, 0};

class TextureRenderer
{
public:
    TextureRenderer(tcu::TestLog &log, glu::RenderContext &renderContext);
    ~TextureRenderer(void);
    void render(uint32_t texture);

private:
    glu::ShaderProgram *m_program;
    glu::RenderContext &m_renderCtx;

    uint32_t m_coordBuffer;
    uint32_t m_indexBuffer;
    uint32_t m_vao;

    static const char *s_vertexShaderGLES2;
    static const char *s_fragmentShaderGLES2;

    static const char *s_vertexShaderGLES3;
    static const char *s_fragmentShaderGLES3;

    static const char *s_vertexShaderGL3;
    static const char *s_fragmentShaderGL3;
};

const char *TextureRenderer::s_vertexShaderGLES2 = "attribute mediump vec2 a_coord;\n"
                                                   "varying mediump vec2 v_texCoord;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "\tv_texCoord = 0.5 * (a_coord + vec2(1.0));\n"
                                                   "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                   "}\n";

const char *TextureRenderer::s_fragmentShaderGLES2 = "varying mediump vec2 v_texCoord;\n"
                                                     "uniform sampler2D u_texture;\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "\tgl_FragColor = texture2D(u_texture, v_texCoord);\n"
                                                     "}\n";

const char *TextureRenderer::s_vertexShaderGLES3 = "#version 300 es\n"
                                                   "in mediump vec2 a_coord;\n"
                                                   "out mediump vec2 v_texCoord;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "\tv_texCoord = 0.5 * (a_coord + vec2(1.0));\n"
                                                   "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                   "}\n";

const char *TextureRenderer::s_fragmentShaderGLES3 = "#version 300 es\n"
                                                     "in mediump vec2 v_texCoord;\n"
                                                     "uniform sampler2D u_texture;\n"
                                                     "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                     "void main (void)\n"
                                                     "{\n"
                                                     "\tdEQP_FragColor = texture(u_texture, v_texCoord);\n"
                                                     "}\n";

const char *TextureRenderer::s_vertexShaderGL3 = "#version 330\n"
                                                 "in mediump vec2 a_coord;\n"
                                                 "out mediump vec2 v_texCoord;\n"
                                                 "void main (void)\n"
                                                 "{\n"
                                                 "\tv_texCoord = 0.5 * (a_coord + vec2(1.0));\n"
                                                 "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                 "}\n";

const char *TextureRenderer::s_fragmentShaderGL3 = "#version 330\n"
                                                   "in mediump vec2 v_texCoord;\n"
                                                   "uniform sampler2D u_texture;\n"
                                                   "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "\tdEQP_FragColor = texture(u_texture, v_texCoord);\n"
                                                   "}\n";

TextureRenderer::TextureRenderer(tcu::TestLog &log, glu::RenderContext &renderContext)
    : m_program(NULL)
    , m_renderCtx(renderContext)
    , m_coordBuffer(0)
    , m_indexBuffer(0)
    , m_vao(0)
{
    const glu::ContextType ctxType = renderContext.getType();

    if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_300_ES))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGLES3, s_fragmentShaderGLES3));
    else if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_100_ES))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGLES2, s_fragmentShaderGLES2));
    else if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_330))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGL3, s_fragmentShaderGL3));
    else
        DE_ASSERT(false);

    if (ctxType.getProfile() == glu::PROFILE_CORE)
        GLU_CHECK_CALL(glGenVertexArrays(1, &m_vao));

    GLU_CHECK_CALL(glGenBuffers(1, &m_coordBuffer));
    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_coordBuffer));
    GLU_CHECK_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadCoords), s_quadCoords, GL_STATIC_DRAW));

    GLU_CHECK_CALL(glGenBuffers(1, &m_indexBuffer));
    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
    GLU_CHECK_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_quadIndices), s_quadIndices, GL_STATIC_DRAW));

    if (!m_program->isOk())
    {
        log << *m_program;
        TCU_CHECK_MSG(m_program->isOk(), "Shader compilation failed");
    }
}

TextureRenderer::~TextureRenderer(void)
{
    delete m_program;
    glDeleteBuffers(1, &m_coordBuffer);
    glDeleteBuffers(1, &m_indexBuffer);
}

void TextureRenderer::render(uint32_t texture)
{
    uint32_t coordLoc = -1;
    uint32_t texLoc   = -1;

    GLU_CHECK_CALL(glUseProgram(m_program->getProgram()));

    coordLoc = glGetAttribLocation(m_program->getProgram(), "a_coord");
    GLU_CHECK();
    TCU_CHECK(coordLoc != (uint32_t)-1);

    if (m_vao != 0)
        GLU_CHECK_CALL(glBindVertexArray(m_vao));

    GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));

    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_coordBuffer));
    GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL));

    GLU_CHECK_CALL(glActiveTexture(GL_TEXTURE0));
    GLU_CHECK_CALL(glBindTexture(GL_TEXTURE_2D, texture));

    texLoc = glGetUniformLocation(m_program->getProgram(), "u_texture");
    GLU_CHECK();
    TCU_CHECK(texLoc != (uint32_t)-1);

    GLU_CHECK_CALL(glUniform1i(texLoc, 0));

    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
    GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, NULL));

    GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));

    if (m_vao != 0)
        GLU_CHECK_CALL(glBindVertexArray(0));
}

class BufferRenderer
{
public:
    BufferRenderer(tcu::TestLog &log, glu::RenderContext &renderContext);
    ~BufferRenderer(void);
    void render(uint32_t buffer, int size);

private:
    glu::ShaderProgram *m_program;
    glu::RenderContext &m_renderCtx;

    uint32_t m_coordBuffer;
    uint32_t m_indexBuffer;
    uint32_t m_vao;

    static const char *s_vertexShaderGLES2;
    static const char *s_fragmentShaderGLES2;

    static const char *s_vertexShaderGLES3;
    static const char *s_fragmentShaderGLES3;

    static const char *s_vertexShaderGL3;
    static const char *s_fragmentShaderGL3;
};

const char *BufferRenderer::s_vertexShaderGLES2 = "attribute mediump vec2 a_coord;\n"
                                                  "attribute mediump vec4 a_buffer;\n"
                                                  "varying mediump vec4 v_buffer;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "\tv_buffer = a_buffer;\n"
                                                  "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                  "}\n";

const char *BufferRenderer::s_fragmentShaderGLES2 = "varying mediump vec4 v_buffer;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "\tgl_FragColor = v_buffer;\n"
                                                    "}\n";

const char *BufferRenderer::s_vertexShaderGLES3 = "#version 300 es\n"
                                                  "in mediump vec2 a_coord;\n"
                                                  "in mediump vec4 a_buffer;\n"
                                                  "out mediump vec4 v_buffer;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "\tv_buffer = a_buffer;\n"
                                                  "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                  "}\n";

const char *BufferRenderer::s_fragmentShaderGLES3 = "#version 300 es\n"
                                                    "in mediump vec4 v_buffer;\n"
                                                    "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                    "void main (void)\n"
                                                    "{\n"
                                                    "\tdEQP_FragColor = v_buffer;\n"
                                                    "}\n";

const char *BufferRenderer::s_vertexShaderGL3 = "#version 330\n"
                                                "in mediump vec2 a_coord;\n"
                                                "in mediump vec4 a_buffer;\n"
                                                "out mediump vec4 v_buffer;\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "\tv_buffer = a_buffer;\n"
                                                "\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
                                                "}\n";

const char *BufferRenderer::s_fragmentShaderGL3 = "#version 330\n"
                                                  "in mediump vec4 v_buffer;\n"
                                                  "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                  "void main (void)\n"
                                                  "{\n"
                                                  "\tdEQP_FragColor = v_buffer;\n"
                                                  "}\n";

BufferRenderer::BufferRenderer(tcu::TestLog &log, glu::RenderContext &renderContext)
    : m_program(NULL)
    , m_renderCtx(renderContext)
    , m_coordBuffer(0)
    , m_indexBuffer(0)
    , m_vao(0)
{
    const glu::ContextType ctxType = renderContext.getType();

    if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_300_ES))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGLES3, s_fragmentShaderGLES3));
    else if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_100_ES))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGLES2, s_fragmentShaderGLES2));
    else if (glu::isGLSLVersionSupported(ctxType, glu::GLSL_VERSION_330))
        m_program =
            new glu::ShaderProgram(m_renderCtx, glu::makeVtxFragSources(s_vertexShaderGL3, s_fragmentShaderGL3));
    else
        DE_ASSERT(false);

    if (ctxType.getProfile() == glu::PROFILE_CORE)
        GLU_CHECK_CALL(glGenVertexArrays(1, &m_vao));

    GLU_CHECK_CALL(glGenBuffers(1, &m_coordBuffer));
    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_coordBuffer));
    GLU_CHECK_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadCoords), s_quadCoords, GL_STATIC_DRAW));

    GLU_CHECK_CALL(glGenBuffers(1, &m_indexBuffer));
    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
    GLU_CHECK_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_quadIndices), s_quadIndices, GL_STATIC_DRAW));

    if (!m_program->isOk())
    {
        log << *m_program;
        TCU_CHECK_MSG(m_program->isOk(), "Shader compilation failed");
    }
}

BufferRenderer::~BufferRenderer(void)
{
    delete m_program;
    glDeleteBuffers(1, &m_coordBuffer);
    glDeleteBuffers(1, &m_indexBuffer);
}

void BufferRenderer::render(uint32_t buffer, int size)
{
    DE_UNREF(size);
    DE_ASSERT((size_t)size >= sizeof(GLubyte) * 4 * 6);
    GLU_CHECK_CALL(glUseProgram(m_program->getProgram()));

    uint32_t bufferLoc = glGetAttribLocation(m_program->getProgram(), "a_buffer");
    TCU_CHECK(bufferLoc != (uint32_t)-1);

    uint32_t coordLoc = glGetAttribLocation(m_program->getProgram(), "a_coord");
    TCU_CHECK(coordLoc != (uint32_t)-1);

    if (m_vao != 0)
        GLU_CHECK_CALL(glBindVertexArray(m_vao));

    GLU_CHECK_CALL(glEnableVertexAttribArray(bufferLoc));
    GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));

    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_coordBuffer));
    GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL));

    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer));
    GLU_CHECK_CALL(glVertexAttribPointer(bufferLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0));
    GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
    GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, NULL));

    GLU_CHECK_CALL(glDisableVertexAttribArray(bufferLoc));
    GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));

    if (m_vao != 0)
        GLU_CHECK_CALL(glBindVertexArray(0));
}

class MemObjectAllocator
{
public:
    enum Result
    {
        RESULT_GOT_BAD_ALLOC = 0,
        RESULT_GEN_TEXTURES_FAILED,
        RESULT_GEN_BUFFERS_FAILED,
        RESULT_BUFFER_DATA_FAILED,
        RESULT_BUFFER_SUB_DATA_FAILED,
        RESULT_TEXTURE_IMAGE_FAILED,
        RESULT_TEXTURE_SUB_IMAGE_FAILED,
        RESULT_BIND_TEXTURE_FAILED,
        RESULT_BIND_BUFFER_FAILED,
        RESULT_DELETE_TEXTURES_FAILED,
        RESULT_DELETE_BUFFERS_FAILED,
        RESULT_RENDER_FAILED,

        RESULT_LAST
    };

    MemObjectAllocator(tcu::TestLog &log, glu::RenderContext &renderContext, MemObjectType objectTypes,
                       const MemObjectConfig &config, int seed);
    ~MemObjectAllocator(void);
    bool allocUntilFailure(void);
    void clearObjects(void);
    Result getResult(void) const
    {
        return m_result;
    }
    uint32_t getGLError(void) const
    {
        return m_glError;
    }
    int getObjectCount(void) const
    {
        return m_objectCount;
    }
    uint32_t getBytes(void) const
    {
        return m_bytesRequired;
    }

    static const char *resultToString(Result result);

private:
    void allocateTexture(de::Random &rnd);
    void allocateBuffer(de::Random &rnd);

    vector<uint32_t> m_buffers;
    vector<uint32_t> m_textures;
    int m_seed;
    int m_objectCount;
    uint32_t m_bytesRequired;
    MemObjectType m_objectTypes;
    Result m_result;
    MemObjectConfig m_config;
    uint32_t m_glError;
    vector<uint8_t> m_unusedData;
    BufferRenderer m_bufferRenderer;
    TextureRenderer m_textureRenderer;
};

MemObjectAllocator::MemObjectAllocator(tcu::TestLog &log, glu::RenderContext &renderContext, MemObjectType objectTypes,
                                       const MemObjectConfig &config, int seed)
    : m_seed(seed)
    , m_objectCount(0)
    , m_bytesRequired(0)
    , m_objectTypes(objectTypes)
    , m_result(RESULT_LAST)
    , m_config(config)
    , m_glError(0)
    , m_bufferRenderer(log, renderContext)
    , m_textureRenderer(log, renderContext)
{
    DE_UNREF(renderContext);

    if (m_config.useUnusedData)
    {
        int unusedSize = deMax32(m_config.maxBufferSize, m_config.maxTextureSize * m_config.maxTextureSize * 4);
        m_unusedData   = vector<uint8_t>(unusedSize);
    }
    else if (m_config.write)
        m_unusedData = vector<uint8_t>(128);
}

MemObjectAllocator::~MemObjectAllocator(void)
{
}

bool MemObjectAllocator::allocUntilFailure(void)
{
    de::Random rnd(m_seed);
    GLU_CHECK_MSG("Error in init");
    try
    {
        const uint64_t timeoutUs = 10000000; // 10s
        uint64_t beginTimeUs     = deGetMicroseconds();
        uint64_t currentTimeUs;

        do
        {
            GLU_CHECK_MSG("Unkown Error");
            switch (m_objectTypes)
            {
            case MEMOBJECTTYPE_TEXTURE:
                allocateTexture(rnd);
                break;

            case MEMOBJECTTYPE_BUFFER:
                allocateBuffer(rnd);
                break;

            default:
            {
                if (rnd.getBool())
                    allocateBuffer(rnd);
                else
                    allocateTexture(rnd);
                break;
            }
            }

            if (m_result != RESULT_LAST)
            {
                glFinish();
                return true;
            }

            currentTimeUs = deGetMicroseconds();
        } while (currentTimeUs - beginTimeUs < timeoutUs);

        // Timeout
        if (currentTimeUs - beginTimeUs >= timeoutUs)
            return false;
        else
            return true;
    }
    catch (const std::bad_alloc &)
    {
        m_result = RESULT_GOT_BAD_ALLOC;
        return true;
    }
}

void MemObjectAllocator::clearObjects(void)
{
    uint32_t error = 0;

    if (!m_textures.empty())
    {
        glDeleteTextures((GLsizei)m_textures.size(), &(m_textures[0]));
        error = glGetError();
        if (error != 0)
        {
            m_result  = RESULT_DELETE_TEXTURES_FAILED;
            m_glError = error;
        }

        m_textures.clear();
    }

    if (!m_buffers.empty())
    {
        glDeleteBuffers((GLsizei)m_buffers.size(), &(m_buffers[0]));
        error = glGetError();
        if (error != 0)
        {
            m_result  = RESULT_DELETE_BUFFERS_FAILED;
            m_glError = error;
        }

        m_buffers.clear();
    }
}

void MemObjectAllocator::allocateTexture(de::Random &rnd)
{
    const int vectorBlockSize = 128;
    uint32_t tex              = 0;
    uint32_t error            = 0;
    int width                 = rnd.getInt(m_config.minTextureSize, m_config.maxTextureSize);
    int height                = rnd.getInt(m_config.minTextureSize, m_config.maxTextureSize);

    glGenTextures(1, &tex);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_GEN_TEXTURES_FAILED;
        m_glError = error;
        return;
    }

    if (m_textures.size() % vectorBlockSize == 0)
        m_textures.reserve(m_textures.size() + vectorBlockSize);

    m_textures.push_back(tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BIND_TEXTURE_FAILED;
        m_glError = error;
        return;
    }

    if (m_config.useUnusedData)
    {
        DE_ASSERT((int)m_unusedData.size() >= width * height * 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(m_unusedData[0]));
    }
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_TEXTURE_IMAGE_FAILED;
        m_glError = error;
        return;
    }

    if (m_config.write)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &(m_unusedData[0]));

    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_TEXTURE_SUB_IMAGE_FAILED;
        m_glError = error;
        return;
    }

    if (m_config.use)
    {
        try
        {
            m_textureRenderer.render(tex);
        }
        catch (const glu::Error &err)
        {
            m_result  = RESULT_RENDER_FAILED;
            m_glError = err.getError();
            return;
        }
        catch (const glu::OutOfMemoryError &)
        {
            m_result  = RESULT_RENDER_FAILED;
            m_glError = GL_OUT_OF_MEMORY;
            return;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BIND_TEXTURE_FAILED;
        m_glError = error;
        return;
    }

    m_objectCount++;
    m_bytesRequired += width * height * 4;
}

void MemObjectAllocator::allocateBuffer(de::Random &rnd)
{
    const int vectorBlockSize = 128;
    uint32_t buffer           = 0;
    uint32_t error            = 0;
    int size                  = rnd.getInt(m_config.minBufferSize, m_config.maxBufferSize);

    glGenBuffers(1, &buffer);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_GEN_BUFFERS_FAILED;
        m_glError = error;
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BIND_BUFFER_FAILED;
        m_glError = error;
        return;
    }

    if (m_buffers.size() % vectorBlockSize == 0)
        m_buffers.reserve(m_buffers.size() + vectorBlockSize);

    m_buffers.push_back(buffer);

    if (m_config.useUnusedData)
    {
        DE_ASSERT((int)m_unusedData.size() >= size);
        glBufferData(GL_ARRAY_BUFFER, size, &(m_unusedData[0]), GL_DYNAMIC_DRAW);
    }
    else
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);

    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BUFFER_DATA_FAILED;
        m_glError = error;
        return;
    }

    if (m_config.write)
        glBufferSubData(GL_ARRAY_BUFFER, 0, 1, &(m_unusedData[0]));

    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BUFFER_SUB_DATA_FAILED;
        m_glError = error;
        return;
    }

    if (m_config.use)
    {
        try
        {
            m_bufferRenderer.render(buffer, size);
        }
        catch (const glu::Error &err)
        {
            m_result  = RESULT_RENDER_FAILED;
            m_glError = err.getError();
            return;
        }
        catch (const glu::OutOfMemoryError &)
        {
            m_result  = RESULT_RENDER_FAILED;
            m_glError = GL_OUT_OF_MEMORY;
            return;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    error = glGetError();
    if (error != 0)
    {
        m_result  = RESULT_BIND_BUFFER_FAILED;
        m_glError = error;
        return;
    }

    m_objectCount++;
    m_bytesRequired += size;
}

const char *MemObjectAllocator::resultToString(Result result)
{
    switch (result)
    {
    case RESULT_GOT_BAD_ALLOC:
        return "Caught std::bad_alloc";

    case RESULT_GEN_TEXTURES_FAILED:
        return "glGenTextures failed";

    case RESULT_GEN_BUFFERS_FAILED:
        return "glGenBuffers failed";

    case RESULT_BUFFER_DATA_FAILED:
        return "glBufferData failed";

    case RESULT_BUFFER_SUB_DATA_FAILED:
        return "glBufferSubData failed";

    case RESULT_TEXTURE_IMAGE_FAILED:
        return "glTexImage2D failed";

    case RESULT_TEXTURE_SUB_IMAGE_FAILED:
        return "glTexSubImage2D failed";

    case RESULT_BIND_TEXTURE_FAILED:
        return "glBindTexture failed";

    case RESULT_BIND_BUFFER_FAILED:
        return "glBindBuffer failed";

    case RESULT_DELETE_TEXTURES_FAILED:
        return "glDeleteTextures failed";

    case RESULT_DELETE_BUFFERS_FAILED:
        return "glDeleteBuffers failed";

    case RESULT_RENDER_FAILED:
        return "Rendering result failed";

    default:
        DE_ASSERT(false);
        return NULL;
    }
}

MemoryStressCase::MemoryStressCase(tcu::TestContext &ctx, glu::RenderContext &renderContext, uint32_t objectTypes,
                                   int minTextureSize, int maxTextureSize, int minBufferSize, int maxBufferSize,
                                   bool write, bool use, bool useUnusedData, bool clearAfterOOM, const char *name,
                                   const char *desc)
    : tcu::TestCase(ctx, name, desc)
    , m_iteration(0)
    , m_iterationCount(5)
    , m_objectTypes((MemObjectType)objectTypes)
    , m_zeroAlloc(false)
    , m_clearAfterOOM(clearAfterOOM)
    , m_renderCtx(renderContext)
{
    m_allocated.reserve(m_iterationCount);
    m_config.maxTextureSize = maxTextureSize;
    m_config.minTextureSize = minTextureSize;
    m_config.maxBufferSize  = maxBufferSize;
    m_config.minBufferSize  = minBufferSize;
    m_config.useUnusedData  = useUnusedData;
    m_config.write          = write;
    m_config.use            = use;
}

MemoryStressCase::~MemoryStressCase(void)
{
}

void MemoryStressCase::init(void)
{
    if (!m_testCtx.getCommandLine().isOutOfMemoryTestEnabled())
    {
        m_testCtx.getLog()
            << TestLog::Message
            << "Tests that exhaust memory are disabled, use --deqp-test-oom=enable command line option to enable."
            << TestLog::EndMessage;
        throw tcu::NotSupportedError("OOM tests disabled");
    }
}

void MemoryStressCase::deinit(void)
{
    TCU_CHECK(!m_zeroAlloc);
}

tcu::TestCase::IterateResult MemoryStressCase::iterate(void)
{
    bool end          = false;
    tcu::TestLog &log = m_testCtx.getLog();

    MemObjectAllocator allocator(log, m_renderCtx, m_objectTypes, m_config, deStringHash(getName()));

    if (!allocator.allocUntilFailure())
    {
        // Allocation timed out
        allocator.clearObjects();

        log << TestLog::Message << "Timeout. Couldn't exhaust memory in timelimit. Allocated "
            << allocator.getObjectCount() << " objects." << TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }

    // Try to cancel rendering operations
    if (m_clearAfterOOM)
        GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    allocator.clearObjects();

    m_allocated.push_back(allocator.getObjectCount());

    if (m_iteration != 0 && allocator.getObjectCount() == 0)
        m_zeroAlloc = true;

    log << TestLog::Message << "Got error when allocation object count: " << allocator.getObjectCount()
        << " bytes: " << allocator.getBytes() << TestLog::EndMessage;

    if ((allocator.getGLError() == 0) && (allocator.getResult() == MemObjectAllocator::RESULT_GOT_BAD_ALLOC))
    {
        log << TestLog::Message << "std::bad_alloc" << TestLog::EndMessage;
        end = true;
        m_testCtx.setTestResult(QP_TEST_RESULT_RESOURCE_ERROR, "Memory allocation failed");
    }
    else if (allocator.getGLError() != GL_OUT_OF_MEMORY)
    {
        log << TestLog::Message << "Invalid Error " << MemObjectAllocator::resultToString(allocator.getResult())
            << " GLError: " << glErrorToString(allocator.getGLError()) << TestLog::EndMessage;

        end = true;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    }

    if ((m_iteration + 1) == m_iterationCount)
    {
        int min = m_allocated[0];
        int max = m_allocated[0];

        float threshold = 50.0f;

        for (int allocNdx = 0; allocNdx < (int)m_allocated.size(); allocNdx++)
        {
            min = deMin32(m_allocated[allocNdx], min);
            max = deMax32(m_allocated[allocNdx], max);
        }

        if (min == 0 && max != 0)
        {
            log << TestLog::Message << "Allocation count zero" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        }
        else
        {
            const float change = (float)(min - max) / (float)(max);
            if (change > threshold)
            {
                log << TestLog::Message << "Allocated objects max: " << max << ", min: " << min
                    << ", difference: " << change << "% threshold: " << threshold << "%" << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, "Allocation count variation");
            }
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        }
        end = true;
    }

    GLU_CHECK_CALL(glFinish());

    m_iteration++;
    if (end)
        return STOP;
    else
        return CONTINUE;
}

} // namespace gls
} // namespace deqp
