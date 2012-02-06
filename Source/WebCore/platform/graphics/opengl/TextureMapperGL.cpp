/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TextureMapperGL.h"

#include "GraphicsContext.h"
#include "Image.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(QT) && QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QPlatformPixmap>
#endif

#if PLATFORM(QT)
#include <cairo/OpenGLShims.h>
#elif defined(TEXMAP_OPENGL_ES_2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif OS(MAC_OS_X)
#include <gl.h>
#else
#include <GL/gl.h>
#endif

#if defined(TEXMAP_OPENGL_ES_2)
#include <EGL/egl.h>
#elif OS(WINDOWS)
#include <windows.h>
#elif OS(MAC_OS_X)
#include <AGL/agl.h>
#elif defined(XP_UNIX)
#include <GL/glx.h>
#endif

#if !defined(TEXMAP_OPENGL_ES_2) && !PLATFORM(QT)
extern "C" {
    void glUniform1f(GLint, GLfloat);
    void glUniform1i(GLint, GLint);
    void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid*);
    void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void glShaderSource(GLuint, GLsizei, const char**, const GLint*);
    GLuint glCreateShader(GLenum);
    void glShaderSource(GLuint, GLsizei, const char**, const GLint*);
    void glCompileShader(GLuint);
    void glDeleteShader(GLuint);
    void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*);
    GLuint glCreateProgram();
    void glAttachShader(GLuint, GLuint);
    void glLinkProgram(GLuint);
    void glUseProgram(GLuint);
    void glDisableVertexAttribArray(GLuint);
    void glEnableVertexAttribArray(GLuint);
    void glBindFramebuffer(GLenum target, GLuint framebuffer);
    void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
    void glGenFramebuffers(GLsizei n, GLuint* framebuffers);
    void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);
    void glBindBuffer(GLenum, GLuint);
    void glDeleteBuffers(GLsizei, const GLuint*);
    void glGenBuffers(GLsizei, GLuint*);
    void glBufferData(GLenum, GLsizeiptr, const GLvoid*, GLenum);
    void glBufferSubData(GLenum, GLsizeiptr, GLsizeiptr, const GLvoid*);
    void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
    void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*);
    void glGenRenderbuffers(GLsizei n, GLuint* ids);
    void glDeleteRenderbuffers(GLsizei n, const GLuint* ids);
    void glBindRenderbuffer(GLenum target, GLuint id);
    void glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
    void glFramebufferRenderbuffer(GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbufferId);
    GLenum glCheckFramebufferStatus(GLenum target);
    GLint glGetAttribLocation(GLuint program, const GLchar* name);
#if !OS(MAC_OS_X)
    GLint glGetUniformLocation(GLuint, const GLchar*);
    GLint glBindAttribLocation(GLuint, GLuint, const GLchar*);
#endif
}
#endif

namespace WebCore {

inline static void debugGLCommand(const char* command, int line)
{
    const GLenum err = glGetError();
    if (!err)
        return;
    WTFReportError(__FILE__, line, WTF_PRETTY_FUNCTION, "[TextureMapper GL] Command failed: %s (%x)\n", command, err);
    ASSERT_NOT_REACHED();
}

#ifndef NDEBUG
#define GL_CMD(x) {x, debugGLCommand(#x, __LINE__); }
#else
#define GL_CMD(x) x;
#endif

struct TextureMapperGLData {
    struct SharedGLData : public RefCounted<SharedGLData> {
#if defined(TEXMAP_OPENGL_ES_2)
        typedef EGLContext GLContext;
        static GLContext getCurrentGLContext()
        {
            return eglGetCurrentContext();
        }
#elif OS(WINDOWS)
        typedef HGLRC GLContext;
        static GLContext getCurrentGLContext()
        {
            return wglGetCurrentContext();
        }
#elif OS(MAC_OS_X)
        typedef AGLContext GLContext;
        static GLContext getCurrentGLContext()
        {
            return aglGetCurrentContext();
        }
#elif defined(XP_UNIX)
        typedef GLXContext GLContext;
        static GLContext getCurrentGLContext()
        {
            return glXGetCurrentContext();
        }
#else
        // Default implementation for unknown opengl.
        // Returns always increasing number and disables GL context data sharing.
        typedef unsigned int GLContext;
        static GLContext getCurrentGLContext()
        {
            static GLContext dummyContextCounter = 0;
            return ++dummyContextCounter;
        }

#endif

        typedef HashMap<GLContext, SharedGLData*> GLContextDataMap;
        static GLContextDataMap& glContextDataMap()
        {
            static GLContextDataMap map;
            return map;
        }

        static PassRefPtr<SharedGLData> currentSharedGLData()
        {
            GLContext currentGLConext = getCurrentGLContext();
            GLContextDataMap::iterator it = glContextDataMap().find(currentGLConext);
            if (it != glContextDataMap().end())
                return it->second;

            return adoptRef(new SharedGLData(getCurrentGLContext()));
        }

        enum ShaderProgramIndex {
            NoProgram = -1,
            SimpleProgram,
            OpacityAndMaskProgram,
            ClipProgram,

            ProgramCount
        };

        enum ShaderVariableIndex {
            InMatrixVariable,
            InSourceMatrixVariable,
            InMaskMatrixVariable,
            OpacityVariable,
            SourceTextureVariable,
            MaskTextureVariable,

            VariableCount
        };

        struct ProgramInfo {
            GLuint id;
            GLuint vertexAttrib;
            GLint vars[VariableCount];
            GLuint vertexShader;
            GLuint fragmentShader;
            ProgramInfo() : id(0) { }
        };

        GLint getUniformLocation(ShaderProgramIndex prog, ShaderVariableIndex var, const char* name)
        {
            return programs[prog].vars[var] = glGetUniformLocation(programs[prog].id, name);
        }

        void createShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource, ShaderProgramIndex index)
        {
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            GL_CMD(glShaderSource(vertexShader, 1, &vertexShaderSource, 0))
            GL_CMD(glShaderSource(fragmentShader, 1, &fragmentShaderSource, 0))
            GLuint programID = glCreateProgram();
            GL_CMD(glCompileShader(vertexShader))
            GL_CMD(glCompileShader(fragmentShader))
            GL_CMD(glAttachShader(programID, vertexShader))
            GL_CMD(glAttachShader(programID, fragmentShader))
            GL_CMD(glLinkProgram(programID))
            programs[index].vertexAttrib = glGetAttribLocation(programID, "InVertex");
            programs[index].id = programID;
            programs[index].vertexShader = vertexShader;
            programs[index].fragmentShader = fragmentShader;
        }

        void deleteShaderProgram(ShaderProgramIndex index)
        {
            ProgramInfo& programInfo = programs[index];
            GLuint programID = programInfo.id;
            if (!programID)
                return;

            GL_CMD(glDetachShader(programID, programInfo.vertexShader))
            GL_CMD(glDeleteShader(programInfo.vertexShader))
            GL_CMD(glDetachShader(programID, programInfo.fragmentShader))
            GL_CMD(glDeleteShader(programInfo.fragmentShader))
            GL_CMD(glDeleteProgram(programID))
        }

        void initializeShaders();

        ProgramInfo programs[ProgramCount];

        int stencilIndex;
        Vector<IntRect> clipStack;

        SharedGLData(GLContext glContext)
            : stencilIndex(1)
        {
            glContextDataMap().add(glContext, this);
            initializeShaders();
        }

        ~SharedGLData()
        {
            for (int i = SimpleProgram; i < ProgramCount; ++i)
                deleteShaderProgram(ShaderProgramIndex(i));

            GLContextDataMap::const_iterator end = glContextDataMap().end();
            GLContextDataMap::iterator it;
            for (it = glContextDataMap().begin(); it != end; ++it) {
                if (it->second == this)
                    break;
            }

            ASSERT(it != end);
            glContextDataMap().remove(it);
        }

    };

    SharedGLData& sharedGLData() const
    {
        return *(m_sharedGLData.get());
    }

    TextureMapperGLData()
        : currentProgram(SharedGLData::NoProgram)
        , previousProgram(0)
        , previousScissorState(0)
        , m_sharedGLData(TextureMapperGLData::SharedGLData::currentSharedGLData())
    { }

    TransformationMatrix projectionMatrix;
    int currentProgram;
    GLint previousProgram;
    GLint previousScissorState;
    GLint viewport[4];
    RefPtr<SharedGLData> m_sharedGLData;
};

class BitmapTextureGL : public BitmapTexture {
public:
    virtual void destroy();
    virtual IntSize size() const;
    virtual bool isValid() const;
    virtual void reset(const IntSize&, bool opaque);
    void bind();
    ~BitmapTextureGL() { destroy(); }
    virtual uint32_t id() const { return m_id; }
    inline FloatSize relativeSize() const { return m_relativeSize; }
    void setTextureMapper(TextureMapperGL* texmap) { m_textureMapper = texmap; }
    void updateContents(Image*, const IntRect&, const IntRect&, PixelFormat);
    void updateContents(const void*, const IntRect&);

private:
    GLuint m_id;
    FloatSize m_relativeSize;
    IntSize m_textureSize;
    IntRect m_dirtyRect;
    GLuint m_fbo;
    GLuint m_rbo;
    IntSize m_actualSize;
    bool m_surfaceNeedsReset;
    TextureMapperGL* m_textureMapper;
    BitmapTextureGL()
        : m_id(0)
        , m_fbo(0)
        , m_rbo(0)
        , m_surfaceNeedsReset(true)
        , m_textureMapper(0)
    {
    }

    friend class TextureMapperGL;
};

#define TEXMAP_GET_SHADER_VAR_LOCATION(prog, var) \
    if (getUniformLocation(prog##Program, var##Variable, #var) < 0) \
            LOG_ERROR("Couldn't find variable "#var" in program "#prog"\n");

#define TEXMAP_BUILD_SHADER(program) \
    createShaderProgram(vertexShaderSource##program, fragmentShaderSource##program, program##Program);

TextureMapperGL::TextureMapperGL()
    : m_data(new TextureMapperGLData)
    , m_context(0)
{
}

void TextureMapperGLData::SharedGLData::initializeShaders()
{
#ifndef TEXMAP_OPENGL_ES_2
#define OES2_PRECISION_DEFINITIONS \
    "#define lowp\n#define highp\n"
#define OES2_FRAGMENT_SHADER_DEFAULT_PRECISION
#else
#define OES2_PRECISION_DEFINITIONS
#define OES2_FRAGMENT_SHADER_DEFAULT_PRECISION \
    "precision mediump float; \n"
#endif

#define VERTEX_SHADER(src...) OES2_PRECISION_DEFINITIONS#src
#define FRAGMENT_SHADER(src...) OES2_PRECISION_DEFINITIONS\
                                OES2_FRAGMENT_SHADER_DEFAULT_PRECISION\
                                #src

    if (!initializeOpenGLShims())
        return;

    const char* fragmentShaderSourceOpacityAndMask =
        FRAGMENT_SHADER(
            uniform sampler2D SourceTexture, MaskTexture;
            uniform lowp float Opacity;
            varying highp vec2 OutTexCoordSource, OutTexCoordMask;
            void main(void)
            {
                lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);
                lowp vec4 maskColor = texture2D(MaskTexture, OutTexCoordMask);
                lowp float fragmentAlpha = Opacity * maskColor.a;
                gl_FragColor = vec4(color.rgb * fragmentAlpha, color.a * fragmentAlpha);
            }
        );

    const char* vertexShaderSourceOpacityAndMask =
        VERTEX_SHADER(
            uniform mat4 InMatrix, InSourceMatrix, InMaskMatrix;
            attribute vec4 InVertex;
            varying highp vec2 OutTexCoordSource, OutTexCoordMask;
            void main(void)
            {
                OutTexCoordSource = vec2(InSourceMatrix * InVertex);
                OutTexCoordMask = vec2(InMaskMatrix * InVertex);
                gl_Position = InMatrix * InVertex;
            }
        );

    const char* fragmentShaderSourceSimple =
        FRAGMENT_SHADER(
            uniform sampler2D SourceTexture;
            uniform lowp float Opacity;
            varying highp vec2 OutTexCoordSource;
            void main(void)
            {
                lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);
                gl_FragColor = vec4(color.rgb * Opacity, color.a * Opacity);
            }
        );

    const char* vertexShaderSourceSimple =
        VERTEX_SHADER(
            uniform mat4 InMatrix, InSourceMatrix;
            attribute vec4 InVertex;
            varying highp vec2 OutTexCoordSource;
            void main(void)
            {
                OutTexCoordSource = vec2(InSourceMatrix * InVertex);
                gl_Position = InMatrix * InVertex;
            }
        );
    const char* fragmentShaderSourceClip =
        FRAGMENT_SHADER(
            void main(void)
            {
                gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            }
        );

    const char* vertexShaderSourceClip =
        VERTEX_SHADER(
            uniform mat4 InMatrix;
            attribute vec4 InVertex;
            void main(void)
            {
                gl_Position = InMatrix * InVertex;
            }
        );


    TEXMAP_BUILD_SHADER(Simple)
    TEXMAP_BUILD_SHADER(OpacityAndMask)
    TEXMAP_BUILD_SHADER(Clip)

    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, InMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, InSourceMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, InMaskMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, SourceTexture)
    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, MaskTexture)
    TEXMAP_GET_SHADER_VAR_LOCATION(OpacityAndMask, Opacity)

    TEXMAP_GET_SHADER_VAR_LOCATION(Simple, InSourceMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(Simple, InMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(Simple, SourceTexture)
    TEXMAP_GET_SHADER_VAR_LOCATION(Simple, Opacity)

    TEXMAP_GET_SHADER_VAR_LOCATION(Clip, InMatrix)
}

void TextureMapperGL::beginPainting()
{
    // Make sure that no GL error code stays from previous operations.
    glGetError();

    if (!initializeOpenGLShims())
        return;

    glGetIntegerv(GL_CURRENT_PROGRAM, &data().previousProgram);
    data().previousScissorState = glIsEnabled(GL_SCISSOR_TEST);

    glEnable(GL_SCISSOR_TEST);
#if PLATFORM(QT)
    if (m_context) {
        QPainter* painter = m_context->platformContext();
        painter->save();
        painter->beginNativePainting();
    }
#endif
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glGetIntegerv(GL_VIEWPORT, data().viewport);
    bindSurface(0);
}

void TextureMapperGL::endPainting()
{
    glClearStencil(1);
    glClear(GL_STENCIL_BUFFER_BIT);
    glUseProgram(data().previousProgram);

    if (data().previousScissorState)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

#if PLATFORM(QT)
    if (!m_context)
        return;
    QPainter* painter = m_context->platformContext();
    painter->endNativePainting();
    painter->restore();
#endif
}


void TextureMapperGL::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* mask)
{
    if (!texture.isValid())
        return;
    const BitmapTextureGL& textureGL = static_cast<const BitmapTextureGL&>(texture);
    drawTexture(textureGL.id(), textureGL.isOpaque(), textureGL.relativeSize(), targetRect, matrix, opacity, mask, false);
}

void TextureMapperGL::drawTexture(uint32_t texture, bool opaque, const FloatSize& relativeSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, const BitmapTexture* maskTexture, bool flip)
{
    TextureMapperGLData::SharedGLData::ShaderProgramIndex program;
    if (maskTexture)
        program = TextureMapperGLData::SharedGLData::OpacityAndMaskProgram;
    else
        program = TextureMapperGLData::SharedGLData::SimpleProgram;

    const TextureMapperGLData::SharedGLData::ProgramInfo& programInfo = data().sharedGLData().programs[program];
    GL_CMD(glUseProgram(programInfo.id))
    data().currentProgram = program;
    GL_CMD(glEnableVertexAttribArray(programInfo.vertexAttrib))
    GL_CMD(glActiveTexture(GL_TEXTURE0))
    GL_CMD(glBindTexture(GL_TEXTURE_2D, texture))
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0))
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(programInfo.vertexAttrib, 2, GL_FLOAT, GL_FALSE, 0, unitRect))

    TransformationMatrix matrix = TransformationMatrix(data().projectionMatrix).multiply(modelViewMatrix).multiply(TransformationMatrix(
            targetRect.width(), 0, 0, 0,
            0, targetRect.height(), 0, 0,
            0, 0, 1, 0,
            targetRect.x(), targetRect.y(), 0, 1));

    const GLfloat m4[] = {
        matrix.m11(), matrix.m12(), matrix.m13(), matrix.m14(),
        matrix.m21(), matrix.m22(), matrix.m23(), matrix.m24(),
        matrix.m31(), matrix.m32(), matrix.m33(), matrix.m34(),
        matrix.m41(), matrix.m42(), matrix.m43(), matrix.m44()
    };
    const GLfloat m4src[] = {relativeSize.width(), 0, 0, 0,
                                     0, relativeSize.height() * (flip ? -1 : 1), 0, 0,
                                     0, 0, 1, 0,
                                     0, flip ? relativeSize.height() : 0, 0, 1};

    GL_CMD(glUniformMatrix4fv(programInfo.vars[TextureMapperGLData::SharedGLData::InMatrixVariable], 1, GL_FALSE, m4))
    GL_CMD(glUniformMatrix4fv(programInfo.vars[TextureMapperGLData::SharedGLData::InSourceMatrixVariable], 1, GL_FALSE, m4src))
    GL_CMD(glUniform1i(programInfo.vars[TextureMapperGLData::SharedGLData::SourceTextureVariable], 0))
    GL_CMD(glUniform1f(programInfo.vars[TextureMapperGLData::SharedGLData::OpacityVariable], opacity))

    if (maskTexture && maskTexture->isValid()) {
        const BitmapTextureGL* maskTextureGL = static_cast<const BitmapTextureGL*>(maskTexture);
        GL_CMD(glActiveTexture(GL_TEXTURE1))
        GL_CMD(glBindTexture(GL_TEXTURE_2D, maskTextureGL->id()))
        const GLfloat m4mask[] = {maskTextureGL->relativeSize().width(), 0, 0, 0,
                                         0, maskTextureGL->relativeSize().height(), 0, 0,
                                         0, 0, 1, 0,
                                         0, 0, 0, 1};
        GL_CMD(glUniformMatrix4fv(programInfo.vars[TextureMapperGLData::SharedGLData::InMaskMatrixVariable], 1, GL_FALSE, m4mask));
        GL_CMD(glUniform1i(programInfo.vars[TextureMapperGLData::SharedGLData::MaskTextureVariable], 1))
        GL_CMD(glActiveTexture(GL_TEXTURE0))
    }

    if (opaque && opacity > 0.99 && !maskTexture)
        GL_CMD(glDisable(GL_BLEND))
    else {
        GL_CMD(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA))
        GL_CMD(glEnable(GL_BLEND))
    }

    GL_CMD(glDisable(GL_DEPTH_TEST))

    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4))
    GL_CMD(glDisableVertexAttribArray(programInfo.vertexAttrib))
}

const char* TextureMapperGL::type() const
{
    return "OpenGL";
}

// This function is similar with GraphicsContext3D::texImage2DResourceSafe.
static void texImage2DResourceSafe(size_t width, size_t height)
{
    const int pixelSize = 4; // RGBA
    OwnArrayPtr<unsigned char> zero;
    if (width && height) {
        unsigned int size = width * height * pixelSize;
        zero = adoptArrayPtr(new unsigned char[size]);
        memset(zero.get(), 0, size);
    }
    GL_CMD(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero.get()))
}

void BitmapTextureGL::reset(const IntSize& newSize, bool opaque)
{
    BitmapTexture::reset(newSize, opaque);
    IntSize newTextureSize = nextPowerOfTwo(newSize);
    bool justCreated = false;
    if (!m_id) {
        GL_CMD(glGenTextures(1, &m_id))
        justCreated = true;
    }

    if (justCreated || newTextureSize.width() > m_textureSize.width() || newTextureSize.height() > m_textureSize.height()) {
        m_textureSize = newTextureSize;
        GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id))
        GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
        GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
        GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
        GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
        texImage2DResourceSafe(m_textureSize.width(), m_textureSize.height());
    }
    m_actualSize = newSize;
    m_relativeSize = FloatSize(float(newSize.width()) / m_textureSize.width(), float(newSize.height()) / m_textureSize.height());
    m_surfaceNeedsReset = true;
}


static void swizzleBGRAToRGBA(uint32_t* data, const IntSize& size)
{
    int width = size.width();
    int height = size.height();
    for (int y = 0; y < height; ++y) {
        uint32_t* p = data + y * width;
        for (int x = 0; x < width; ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
    }
}

void BitmapTextureGL::updateContents(const void* data, const IntRect& targetRect)
{
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id))
    GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, data))
}

void BitmapTextureGL::updateContents(Image* image, const IntRect& targetRect, const IntRect& sourceRect, BitmapTexture::PixelFormat format)
{
    if (!image)
        return;
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id))
    GLuint glFormat = isOpaque() ? GL_RGB : GL_RGBA;
    NativeImagePtr frameImage = image->nativeImageForCurrentFrame();
    if (!frameImage)
        return;

#if PLATFORM(QT)
    QImage qtImage;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // With QPA, we can avoid a deep copy.
    qtImage = *frameImage->handle()->buffer();
#else
    // This might be a deep copy, depending on other references to the pixmap.
    qtImage = frameImage->toImage();
#endif

    if (IntSize(qtImage.size()) != sourceRect.size())
        qtImage = qtImage.copy(sourceRect);
    if (format == BGRAFormat || format == BGRFormat)
        swizzleBGRAToRGBA(reinterpret_cast<uint32_t*>(qtImage.bits()), qtImage.size());
    GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), glFormat, GL_UNSIGNED_BYTE, qtImage.constBits()))
#endif
}

static inline TransformationMatrix createProjectionMatrix(const IntSize& size, bool flip)
{
    const float near = 9999999;
    const float far = -99999;

    return TransformationMatrix(2.0 / float(size.width()), 0, 0, 0,
                                0, (flip ? -2.0 : 2.0) / float(size.height()), 0, 0,
                                0, 0, -2.f / (far - near), 0,
                                -1, flip ? 1 : -1, -(far + near) / (far - near), 1);
}

void BitmapTextureGL::bind()
{
    int& stencilIndex = m_textureMapper->data().sharedGLData().stencilIndex;
    if (m_surfaceNeedsReset || !m_fbo) {
        if (!m_fbo)
            GL_CMD(glGenFramebuffers(1, &m_fbo))
        if (!m_rbo)
            GL_CMD(glGenRenderbuffers(1, &m_rbo));
        GL_CMD(glBindRenderbuffer(GL_RENDERBUFFER, m_rbo))
#ifdef TEXMAP_OPENGL_ES_2
        GL_CMD(glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_textureSize.width(), m_textureSize.height()))
#else
        GL_CMD(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, m_textureSize.width(), m_textureSize.height()))
#endif
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo))
        GL_CMD(glBindTexture(GL_TEXTURE_2D, 0))
        GL_CMD(glBindRenderbuffer(GL_RENDERBUFFER, 0))
        GL_CMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id(), 0))
        GL_CMD(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo))
#ifndef TEXMAP_OPENGL_ES_2
        GL_CMD(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo));
#endif
        GL_CMD(glClearColor(0, 0, 0, 0))
        GL_CMD(glClearStencil(stencilIndex - 1))
        GL_CMD(glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))
        m_surfaceNeedsReset = false;
    } else
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo))

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(stencilIndex > 1 ? GL_GEQUAL : GL_ALWAYS, stencilIndex - 1, stencilIndex - 1);
    GL_CMD(glViewport(0, 0, size().width(), size().height()))
    m_textureMapper->data().projectionMatrix = createProjectionMatrix(size(), false);
}

void BitmapTextureGL::destroy()
{
    if (m_id)
        GL_CMD(glDeleteTextures(1, &m_id))

    if (m_fbo)
        GL_CMD(glDeleteFramebuffers(1, &m_fbo))

    if (m_rbo)
        GL_CMD(glDeleteRenderbuffers(1, &m_rbo))

    m_fbo = 0;
    m_id = 0;
    m_textureSize = IntSize();
    m_relativeSize = FloatSize(1, 1);
}

bool BitmapTextureGL::isValid() const
{
    return m_id;
}

IntSize BitmapTextureGL::size() const
{
    return m_textureSize;
}

TextureMapperGL::~TextureMapperGL()
{
    delete m_data;
}

void TextureMapperGL::bindSurface(BitmapTexture *surfacePointer)
{
    BitmapTextureGL* surface = static_cast<BitmapTextureGL*>(surfacePointer);

    if (!surface) {
        IntSize viewportSize(data().viewport[2], data().viewport[3]);
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, 0))
        data().projectionMatrix = createProjectionMatrix(viewportSize, true);
        GL_CMD(glStencilFunc(data().sharedGLData().stencilIndex > 1 ? GL_EQUAL : GL_ALWAYS, data().sharedGLData().stencilIndex - 1, data().sharedGLData().stencilIndex - 1))
        GL_CMD(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP))
        GL_CMD(glViewport(0, 0, viewportSize.width(), viewportSize.height()))
        data().sharedGLData().clipStack.append(IntRect(data().viewport[0], data().viewport[1], data().viewport[2], data().viewport[3]));
        return;
    }

    surface->bind();
}

static void scissorClip(const IntRect& rect)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glScissor(rect.x(), viewport[3] - rect.maxY(), rect.width(), rect.height());
}

bool TextureMapperGL::beginScissorClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    FloatQuad quad = modelViewMatrix.projectQuad(targetRect);
    IntRect rect = quad.enclosingBoundingBox();

    // Only use scissors on rectilinear clips.
    if (!quad.isRectilinear() || rect.isEmpty()) {
        data().sharedGLData().clipStack.append(IntRect());
        return false;
    }

    // Intersect with previous clip.
    if (!data().sharedGLData().clipStack.isEmpty())
        rect.intersect(data().sharedGLData().clipStack.last());

    scissorClip(rect);
    data().sharedGLData().clipStack.append(rect);

    return true;
}

bool TextureMapperGL::endScissorClip()
{
    data().sharedGLData().clipStack.removeLast();
    ASSERT(!data().sharedGLData().clipStack.isEmpty());

    IntRect rect = data().sharedGLData().clipStack.last();
    if (rect.isEmpty())
        return false;

    scissorClip(rect);
    return true;
}

void TextureMapperGL::beginClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    if (beginScissorClip(modelViewMatrix, targetRect))
        return;
    TextureMapperGLData::SharedGLData::ShaderProgramIndex program = TextureMapperGLData::SharedGLData::ClipProgram;
    const TextureMapperGLData::SharedGLData::ProgramInfo& programInfo = data().sharedGLData().programs[program];
    GL_CMD(glUseProgram(programInfo.id))
    GL_CMD(glEnableVertexAttribArray(programInfo.vertexAttrib))
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(programInfo.vertexAttrib, 2, GL_FLOAT, GL_FALSE, 0, unitRect))

    TransformationMatrix matrix = TransformationMatrix(data().projectionMatrix)
            .multiply(modelViewMatrix)
            .multiply(TransformationMatrix(targetRect.width(), 0, 0, 0,
                0, targetRect.height(), 0, 0,
                0, 0, 1, 0,
                targetRect.x(), targetRect.y(), 0, 1));

    const GLfloat m4[] = {
        matrix.m11(), matrix.m12(), matrix.m13(), matrix.m14(),
        matrix.m21(), matrix.m22(), matrix.m23(), matrix.m24(),
        matrix.m31(), matrix.m32(), matrix.m33(), matrix.m34(),
        matrix.m41(), matrix.m42(), matrix.m43(), matrix.m44()
    };

    int& stencilIndex = data().sharedGLData().stencilIndex;

    GL_CMD(glUniformMatrix4fv(programInfo.vars[TextureMapperGLData::SharedGLData::InMatrixVariable], 1, GL_FALSE, m4))
    GL_CMD(glEnable(GL_STENCIL_TEST))
    GL_CMD(glStencilFunc(GL_NEVER, stencilIndex, stencilIndex))
    GL_CMD(glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE))
    GL_CMD(glStencilMask(0xff & ~(stencilIndex - 1)))
    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4))
    GL_CMD(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP))
    stencilIndex <<= 1;
    glStencilFunc(stencilIndex > 1 ? GL_EQUAL : GL_ALWAYS, stencilIndex - 1, stencilIndex - 1);
    GL_CMD(glDisableVertexAttribArray(programInfo.vertexAttrib))
}

void TextureMapperGL::endClip()
{
    if (endScissorClip())
        return;

    data().sharedGLData().stencilIndex >>= 1;
    glStencilFunc(data().sharedGLData().stencilIndex > 1 ? GL_EQUAL : GL_ALWAYS, data().sharedGLData().stencilIndex - 1, data().sharedGLData().stencilIndex - 1);    

    // After we've cleared the last non-rectalinear clip, we disable the stencil test.
    if (data().sharedGLData().stencilIndex == 1)
        GL_CMD(glDisable(GL_STENCIL_TEST))

}

PassRefPtr<BitmapTexture> TextureMapperGL::createTexture()
{
    BitmapTextureGL* texture = new BitmapTextureGL();
    texture->setTextureMapper(this);
    return adoptRef(texture);
}

PassOwnPtr<TextureMapper> TextureMapper::platformCreateAccelerated()
{
    return TextureMapperGL::create();
}

};
