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
#include "HashMap.h"
#include "Image.h"
#include "PassRefPtr.h"
#include "RefCounted.h"
#include "Timer.h"

#if defined(TEXMAP_OPENGL_ES_2)
#include <GLES2/gl2.h>
#elif PLATFORM(MAC)
#include <gl.h>
#else
#include <GL/gl.h>
#endif

#ifndef TEXMAP_OPENGL_ES2
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
    void glGetProgramInfoLog(GLuint program, GLsizei, GLsizei*, GLchar*);

#if !PLATFORM(MAC)
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
}

#define DEBUG_GL_COMMANDS

#ifdef DEBUG_GL_COMMANDS
#define GL_CMD(x) {x, debugGLCommand(#x, __LINE__); }
#else
#define GL_CMD(x) x
#endif

class BitmapTextureGL : public BitmapTexture {
public:
    virtual void destroy();
    virtual IntSize size() const;
    virtual bool isValid() const;
    virtual void reset(const IntSize&, bool opaque);
    virtual PlatformGraphicsContext* beginPaint(const IntRect& dirtyRect);
    virtual void endPaint();
    virtual void setContentsToImage(Image*);

private:
    GLuint m_id;
    NativeImagePtr m_image;
    FloatSize m_relativeSize;
    bool m_opaque;
    IntSize m_textureSize;
    RefPtr<RGBA32PremultimpliedBuffer> m_buffer;
    IntRect m_dirtyRect;
    GLuint m_fbo;
    IntSize m_actualSize;
    bool m_surfaceNeedsReset;
    BitmapTextureGL()
        : m_id(0)
        , m_image(0)
        , m_opaque(false)
        , m_fbo(0)
        , m_surfaceNeedsReset(true)
    {
    }

    friend class TextureMapperGL;
};

static struct TexmapShaderInfo {
    enum ShaderProgramIndex {
        SimpleProgram,
        OpacityAndMaskProgram,
        TargetProgram,
        NumPrograms
    };

    enum ShaderVariableIndex {
        InMatrixVariable,
        InSourceMatrixVariable,
        InMaskMatrixVariable,
        InVertexVariable,

        OpacityVariable,
        SourceTextureVariable,
        MaskTextureVariable,
        NumVariables
    };

    struct ProgramInfo {
        GLuint id;
        GLint vars[NumVariables];
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
        GL_CMD(glBindAttribLocation(programID, 0, "InVertex"))
        GL_CMD(glLinkProgram(programID))
        programs[index].id = programID;
        char infoLog[1024];
        int len;
        GL_CMD(glGetProgramInfoLog(programID, 1024, &len, infoLog));
    }

    ProgramInfo programs[NumPrograms];

} gShaderInfo;

#define TEXMAP_GET_SHADER_VAR_LOCATION(prog, var) \
            if (gShaderInfo.getUniformLocation(TexmapShaderInfo::prog##Program, TexmapShaderInfo::var##Variable, #var) < 0) \
                    LOG_ERROR("Couldn't find variable "#var" in program "#prog"\n");
#define TEXMAP_BUILD_SHADER(program) gShaderInfo.createShaderProgram(vertexShaderSource##program, fragmentShaderSource##program, TexmapShaderInfo::program##Program);

TextureMapperGL::TextureMapperGL(GraphicsContext* context)
    : TextureMapper(context)
    , m_currentProgram(TexmapShaderInfo::TargetProgram)
{
    static bool shadersCompiled = false;
    if (shadersCompiled)
        return;
    shadersCompiled = true;
#ifndef TEXMAP_OPENGL_ES2
#define OES2_PERCISION_DEFINITIONS \
    "#define lowp\n#define highp\n"
#else
#define OES2_PERCISION_DEFINITIONS
#endif

    const char* fragmentShaderSourceOpacityAndMask =
            OES2_PERCISION_DEFINITIONS
"               uniform sampler2D SourceTexture, MaskTexture;                       \n"
"               uniform lowp float Opacity;                                         \n"
"               varying highp vec2 OutTexCoordSource, OutTexCoordMask;              \n"
"               void main(void)                                                     \n"
"               {                                                                   \n"
"                   lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);  \n"
"                   lowp vec4 maskColor = texture2D(MaskTexture, OutTexCoordMask);  \n"
"                   lowp float o = Opacity * maskColor.a;                           \n"
"                   gl_FragColor = vec4(color.rgb * o, color.a * o);                \n"
"               }                                                                   \n";

    const char* vertexShaderSourceOpacityAndMask =
            OES2_PERCISION_DEFINITIONS
"               uniform mat4 InMatrix, InSourceMatrix, InMaskMatrix;            \n"
"               attribute vec4 InVertex;                                        \n"
"               varying highp vec2 OutTexCoordSource, OutTexCoordMask;          \n"
"               void main(void)                                                 \n"
"               {                                                               \n"
"                   OutTexCoordSource = vec2(InSourceMatrix * InVertex);        \n"
"                   OutTexCoordMask = vec2(InMaskMatrix * InVertex);            \n"
"                   gl_Position = InMatrix * InVertex;                          \n"
"               }                                                               \n";

    const char* fragmentShaderSourceSimple =
            OES2_PERCISION_DEFINITIONS
"               uniform sampler2D SourceTexture;                                    \n"
"               uniform lowp float Opacity;                                         \n"
"               varying highp vec2 OutTexCoordSource;                               \n"
"               void main(void)                                                     \n"
"               {                                                                   \n"
"                   lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);  \n"
"                   gl_FragColor = vec4(color.rgb * Opacity, color.a * Opacity);    \n"
"               }                                                                   \n";

    const char* vertexShaderSourceSimple =
            OES2_PERCISION_DEFINITIONS
"               uniform mat4 InMatrix, InSourceMatrix;                      \n"
"               attribute vec4 InVertex;                                    \n"
"               varying highp vec2 OutTexCoordSource;                       \n"
"               void main(void)                                             \n"
"               {                                                           \n"
"                   OutTexCoordSource = vec2(InSourceMatrix * InVertex);    \n"
"                   gl_Position = InMatrix * InVertex;                      \n"
"               }                                                           \n";

    const char* fragmentShaderSourceTarget =
            OES2_PERCISION_DEFINITIONS
"               uniform sampler2D SourceTexture;                                            \n"
"               uniform lowp float Opacity;                                                 \n"
"               varying highp vec2 OutTexCoordSource;                                       \n"
"               void main(void)                                                             \n"
"               {                                                                           \n"
"                   lowp vec4 color = texture2D(SourceTexture, OutTexCoordSource);          \n"
"                   gl_FragColor = vec4(color.bgr * Opacity, color.a * Opacity);            \n"
"               }                                                                           \n";

    const char* vertexShaderSourceTarget = vertexShaderSourceSimple;

    TEXMAP_BUILD_SHADER(Simple)
    TEXMAP_BUILD_SHADER(OpacityAndMask)
    TEXMAP_BUILD_SHADER(Target)

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

    TEXMAP_GET_SHADER_VAR_LOCATION(Target, InSourceMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(Target, InMatrix)
    TEXMAP_GET_SHADER_VAR_LOCATION(Target, SourceTexture)
    TEXMAP_GET_SHADER_VAR_LOCATION(Target, Opacity)
}

void TextureMapperGL::drawTexture(const BitmapTexture& texture, const IntRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, const BitmapTexture* maskTexture)
{
    if (!texture.isValid())
        return;

    const BitmapTextureGL& textureGL = static_cast<const BitmapTextureGL&>(texture);

    TexmapShaderInfo::ShaderProgramIndex program;
    if (maskTexture)
        program = TexmapShaderInfo::OpacityAndMaskProgram;
    else
        program = TexmapShaderInfo::SimpleProgram;

    const TexmapShaderInfo::ProgramInfo& programInfo = gShaderInfo.programs[program];
    if (m_currentProgram != program) {
        GL_CMD(glUseProgram(programInfo.id))
        GL_CMD(glDisableVertexAttribArray(gShaderInfo.programs[m_currentProgram].vars[TexmapShaderInfo::InVertexVariable]))
        m_currentProgram = program;
        GL_CMD(glEnableVertexAttribArray(programInfo.vars[TexmapShaderInfo::InVertexVariable]))
    }

    GL_CMD(glDisable(GL_DEPTH_TEST))
    GL_CMD(glDisable(GL_STENCIL_TEST))

    GL_CMD(glActiveTexture(GL_TEXTURE0))
    GL_CMD(glBindTexture(GL_TEXTURE_2D, textureGL.m_id))
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0))
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, unitRect))

    TransformationMatrix matrix = TransformationMatrix(m_projectionMatrix).multLeft(modelViewMatrix).multLeft(TransformationMatrix(
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
    const GLfloat m4src[] = {textureGL.m_relativeSize.width(), 0, 0, 0,
                                     0, textureGL.m_relativeSize.height(), 0, 0,
                                     0, 0, 1, 0,
                                     0, 0, 0, 1};
    GL_CMD(glUniformMatrix4fv(programInfo.vars[TexmapShaderInfo::InMatrixVariable], 1, GL_FALSE, m4))
    GL_CMD(glUniformMatrix4fv(programInfo.vars[TexmapShaderInfo::InSourceMatrixVariable], 1, GL_FALSE, m4src))
    GL_CMD(glUniform1i(programInfo.vars[TexmapShaderInfo::SourceTextureVariable], 0))
    GL_CMD(glUniform1f(programInfo.vars[TexmapShaderInfo::OpacityVariable], opacity))

    if (maskTexture && maskTexture->isValid()) {
        const BitmapTextureGL* maskTextureGL = static_cast<const BitmapTextureGL*>(maskTexture);
        GL_CMD(glActiveTexture(GL_TEXTURE1))
        GL_CMD(glBindTexture(GL_TEXTURE_2D, maskTextureGL->m_id))
        const GLfloat m4mask[] = {maskTextureGL->m_relativeSize.width(), 0, 0, 0,
                                         0, maskTextureGL->m_relativeSize.height(), 0, 0,
                                         0, 0, 1, 0,
                                         0, 0, 0, 1};
        glUniformMatrix4fv(programInfo.vars[TexmapShaderInfo::InMaskMatrixVariable], 1, GL_FALSE, m4mask);
        GL_CMD(glUniform1i(programInfo.vars[TexmapShaderInfo::MaskTextureVariable], 1))
        GL_CMD(glActiveTexture(GL_TEXTURE0))
    }


    if (textureGL.m_opaque && opacity > 0.99 && !maskTexture)
        GL_CMD(glDisable(GL_BLEND))
    else {
        GL_CMD(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA))
        GL_CMD(glEnable(GL_BLEND))
    }

    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4))
}


const char* TextureMapperGL::type() const
{
    return "OpenGL";
}

void BitmapTextureGL::reset(const IntSize& newSize, bool opaque)
{
    BitmapTexture::reset(newSize, opaque);
    m_image = 0;
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
        GL_CMD(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_textureSize.width(), m_textureSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0))
    }
    m_actualSize = newSize;
    m_relativeSize = FloatSize(float(newSize.width()) / m_textureSize.width(), float(newSize.height()) / m_textureSize.height());
    m_opaque = opaque;
    m_surfaceNeedsReset = true;
}

PlatformGraphicsContext* BitmapTextureGL::beginPaint(const IntRect& dirtyRect)
{
    m_buffer = RGBA32PremultimpliedBuffer::create();
    m_dirtyRect = dirtyRect;
    return m_buffer->beginPaint(dirtyRect, m_opaque);
}

void BitmapTextureGL::endPaint()
{
    if (!m_buffer)
        return;
    m_buffer->endPaint();
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id))
    GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, m_dirtyRect.x(), m_dirtyRect.y(), m_dirtyRect.width(), m_dirtyRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, m_buffer->data()))
    m_buffer.clear();
}

struct TexmapGLShaderTextures {
    struct Entry {
        GLuint texture;
        int refCount;
    };
    HashMap<NativeImagePtr, Entry> imageToTexture;
    GLuint findOrCreate(NativeImagePtr image, bool& found)
    {
        HashMap<NativeImagePtr, Entry>::iterator it = imageToTexture.find(image);
        found = false;
        if (it != imageToTexture.end()) {
            it->second.refCount++;
            found = true;
            return it->second.texture;
        }
        Entry entry;
        GL_CMD(glGenTextures(1, &entry.texture));
        entry.refCount = 1;
        imageToTexture.add(image, entry);
        return entry.texture;
    }

    bool deref(NativeImagePtr image)
    {
        HashMap<NativeImagePtr, Entry>::iterator it = imageToTexture.find(image);
        if (it != imageToTexture.end()) {
            if (it->second.refCount < 2) {
                imageToTexture.remove(it);
                return false;
            }
        }
        return true;
    }
};

static TexmapGLShaderTextures gTextureRepository;

void BitmapTextureGL::setContentsToImage(Image* image)
{
    NativeImagePtr nativeImage = image ? image->nativeImageForCurrentFrame() : 0;
    if (!image || !nativeImage) {
        if (m_image)
            destroy();
        return;
    }

    if (nativeImage == m_image)
        return;
    bool found = false;
    GLuint newTextureID = gTextureRepository.findOrCreate(nativeImage, found);
    if (newTextureID != m_id) {
        destroy();
        m_id = newTextureID;
        reset(image->size(), false);
        m_image = nativeImage;
        if (!found) {
            GraphicsContext context(beginPaint(IntRect(0, 0, m_textureSize.width(), m_textureSize.height())));
            context.drawImage(image, DeviceColorSpace, IntPoint(0, 0), CompositeCopy);
            endPaint();
        }
    }
}

void BitmapTextureGL::destroy()
{
    if (m_id && (!m_image || !gTextureRepository.deref(m_image)))
        GL_CMD(glDeleteTextures(1, &m_id))
    if (m_fbo)
        GL_CMD(glDeleteFramebuffers(1, &m_fbo))

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

static inline TransformationMatrix createProjectionMatrix(const IntSize& size, bool flip)
{
    return TransformationMatrix(2.0 / float(size.width()), 0, 0, 0,
                                0, (flip ? -2.0 : 2.0) / float(size.height()), 0, 0,
                                0, 0, -0.000001, 0,
                                -1, flip ? 1 : -1, 0, 1);
}

void TextureMapperGL::cleanup()
{
}

void TextureMapperGL::bindSurface(BitmapTexture *surfacePointer)
{
    BitmapTextureGL* surface = static_cast<BitmapTextureGL*>(surfacePointer);

    if (!surface)
        return;
    TransformationMatrix matrix = createProjectionMatrix(surface->size(), false);
    matrix.translate(-surface->offset().x(), -surface->offset().y());

    if (surface->m_surfaceNeedsReset || !surface->m_fbo) {
        if (!surface->m_fbo)
            GL_CMD(glGenFramebuffers(1, &surface->m_fbo))
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, surface->m_fbo))
        GL_CMD(glBindTexture(GL_TEXTURE_2D, 0))
        GL_CMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, surface->m_id, 0))
        GL_CMD(glClearColor(0, 0, 0, 0))
        GL_CMD(glClear(GL_COLOR_BUFFER_BIT))
        surface->m_surfaceNeedsReset = false;
    } else {
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, surface->m_fbo))
    }

    GL_CMD(glViewport(0, 0, surface->size().width(), surface->size().height()))
    m_projectionMatrix = matrix;
}

void TextureMapperGL::setClip(const IntRect& rect)
{
    GL_CMD(glScissor(rect.x(), rect.y(), rect.width(), rect.height()))
    GL_CMD(glEnable(GL_SCISSOR_TEST))
}


void TextureMapperGL::paintToTarget(const BitmapTexture& aSurface, const IntSize& surfaceSize, const TransformationMatrix& transform, float opacity, const IntRect& visibleRect)
{
    const BitmapTextureGL& surface = static_cast<const BitmapTextureGL&>(aSurface);

    // Create the model-view-projection matrix to display on screen.
    TransformationMatrix matrix = createProjectionMatrix(surfaceSize, true).multLeft(transform).multLeft(
                TransformationMatrix(
                        surface.m_actualSize.width(), 0, 0, 0,
                        0, surface.m_actualSize.height(), 0, 0,
                        0, 0, 1, 0,
                        surface.offset().x(), surface.offset().y(), 0, 1
                )
            );

    const GLfloat m4[] = {
        matrix.m11(), matrix.m12(), matrix.m13(), matrix.m14(),
        matrix.m21(), matrix.m22(), matrix.m23(), matrix.m24(),
        matrix.m31(), matrix.m32(), matrix.m33(), matrix.m34(),
        matrix.m41(), matrix.m42(), matrix.m43(), matrix.m44()
    };

    const GLfloat m4src[] = {surface.m_relativeSize.width(), 0, 0, 0,
                                     0, surface.m_relativeSize.height(), 0, 0,
                                     0, 0, 1, 0,
                                     0, 0, 0, 1};

    // We already blended the alpha in; the result is premultiplied.
    GL_CMD(glUseProgram(gShaderInfo.programs[TexmapShaderInfo::TargetProgram].id))
    GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, 0))
    GL_CMD(glViewport(0, 0, surfaceSize.width(), surfaceSize.height()))
    GL_CMD(glDisable(GL_STENCIL_TEST))
    const TexmapShaderInfo::ProgramInfo& programInfo = gShaderInfo.programs[TexmapShaderInfo::TargetProgram];
    GL_CMD(glUniform1f(programInfo.vars[TexmapShaderInfo::OpacityVariable], opacity))
    GL_CMD(glActiveTexture(GL_TEXTURE0))
    GL_CMD(glBindTexture(GL_TEXTURE_2D, surface.m_id))
    GL_CMD(glUniform1i(programInfo.vars[TexmapShaderInfo::SourceTextureVariable], 0))
    GL_CMD(glEnableVertexAttribArray(programInfo.vars[TexmapShaderInfo::InVertexVariable]))
    GL_CMD(glUniformMatrix4fv(programInfo.vars[TexmapShaderInfo::InMatrixVariable], 1, GL_FALSE, m4))
    GL_CMD(glUniformMatrix4fv(programInfo.vars[TexmapShaderInfo::InSourceMatrixVariable], 1, GL_FALSE, m4src))
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0))
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, unitRect))
    GL_CMD(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA))
    GL_CMD(glEnable(GL_BLEND))
    setClip(visibleRect);

    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4))
    GL_CMD(glDisableVertexAttribArray(programInfo.vars[TexmapShaderInfo::InVertexVariable]))
    GL_CMD(glUseProgram(0))
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0))
    m_currentProgram = TexmapShaderInfo::TargetProgram;
}

PassRefPtr<BitmapTexture> TextureMapperGL::createTexture()
{
    return adoptRef(new BitmapTextureGL());
}

};
