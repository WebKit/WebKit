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
#include "TextureMapperShaderManager.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(QT)
#if QT_VERSION >= 0x050000
#include <QOpenGLContext>
#include <QPlatformPixmap>
#else
#include <QGLContext>
#endif // QT_VERSION
#elif OS(WINDOWS)
#include <windows.h>
#elif OS(MAC_OS_X)
#include <AGL/agl.h>
#elif defined(XP_UNIX)
#include <GL/glx.h>
#endif

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#endif

#define GL_CMD(...) do { __VA_ARGS__; ASSERT_ARG(__VA_ARGS__, !glGetError()); } while (0)
namespace WebCore {
struct TextureMapperGLData {
    struct SharedGLData : public RefCounted<SharedGLData> {
#if PLATFORM(QT)
#if QT_VERSION >= 0x050000
        typedef QOpenGLContext* GLContext;
        static GLContext getCurrentGLContext()
        {
            return QOpenGLContext::currentContext();
        }
#else
        typedef const QGLContext* GLContext;
        static GLContext getCurrentGLContext()
        {
            return QGLContext::currentContext();
        }
#endif
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



        TextureMapperShaderManager textureMapperShaderManager;

        SharedGLData(GLContext glContext)
        {
            glContextDataMap().add(glContext, this);
        }

        ~SharedGLData()
        {
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

    void initializeStencil();

    TextureMapperGLData()
        : PaintFlags(0)
        , previousProgram(0)
        , targetFrameBuffer(0)
        , didModifyStencil(false)
        , previousScissorState(0)
        , previousDepthState(0)
        , m_sharedGLData(TextureMapperGLData::SharedGLData::currentSharedGLData())
    { }

    TransformationMatrix projectionMatrix;
    TextureMapper::PaintFlags PaintFlags;
    GLint previousProgram;
    GLint targetFrameBuffer;
    bool didModifyStencil;
    GLint previousScissorState;
    GLint previousDepthState;
    GLint viewport[4];
    GLint previousScissor[4];
    RefPtr<SharedGLData> m_sharedGLData;
    RefPtr<BitmapTexture> currentSurface;
};

void TextureMapperGL::ClipStack::init(const IntRect& rect)
{
    clipStack.clear();
    clipState = TextureMapperGL::ClipState(rect);
}

void TextureMapperGL::ClipStack::push()
{
    clipStack.append(clipState);
}

void TextureMapperGL::ClipStack::pop()
{
    if (clipStack.isEmpty())
        return;
    clipState = clipStack.last();
    clipStack.removeLast();
}

static void scissorClip(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    GLint viewport[4];
    GL_CMD(glGetIntegerv(GL_VIEWPORT, viewport));
    GL_CMD(glScissor(rect.x(), viewport[3] - rect.maxY(), rect.width(), rect.height()));
}

void TextureMapperGL::ClipStack::apply()
{
    scissorClip(clipState.scissorBox);
    GL_CMD(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    GL_CMD(glStencilFunc(GL_EQUAL, clipState.stencilIndex - 1, clipState.stencilIndex - 1));
    if (clipState.stencilIndex == 1)
        GL_CMD(glDisable(GL_STENCIL_TEST));
    else
        GL_CMD(glEnable(GL_STENCIL_TEST));
}


void TextureMapperGLData::initializeStencil()
{
    if (currentSurface) {
        static_cast<BitmapTextureGL*>(currentSurface.get())->initializeStencil();
        return;
    }

    if (didModifyStencil)
        return;

    GL_CMD(glClearStencil(0));
    GL_CMD(glClear(GL_STENCIL_BUFFER_BIT));
    didModifyStencil = true;
}

BitmapTextureGL* toBitmapTextureGL(BitmapTexture* texture)
{
    if (!texture || !texture->isBackedByOpenGL())
        return 0;

    return static_cast<BitmapTextureGL*>(texture);
}

TextureMapperGL::TextureMapperGL()
    : m_data(new TextureMapperGLData)
    , m_context(0)
{
}

TextureMapperGL::ClipStack& TextureMapperGL::clipStack()
{
    return data().currentSurface ? toBitmapTextureGL(data().currentSurface.get())->m_clipStack : m_clipStack;
}

void TextureMapperGL::beginPainting(PaintFlags flags)
{
    // Make sure that no GL error code stays from previous operations.
    glGetError();

    if (!initializeOpenGLShims())
        return;

    GL_CMD(glGetIntegerv(GL_CURRENT_PROGRAM, &data().previousProgram));
    data().previousScissorState = glIsEnabled(GL_SCISSOR_TEST);
    data().previousDepthState = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
#if PLATFORM(QT)
    if (m_context) {
        QPainter* painter = m_context->platformContext();
        painter->save();
        painter->beginNativePainting();
    }
#endif
    data().didModifyStencil = false;
    GL_CMD(glDepthMask(0));
    GL_CMD(glGetIntegerv(GL_VIEWPORT, data().viewport));
    GL_CMD(glGetIntegerv(GL_SCISSOR_BOX, data().previousScissor));
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &data().targetFrameBuffer);
    m_clipStack.init(IntRect(0, 0, data().viewport[2], data().viewport[3]));
    GL_CMD(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &data().targetFrameBuffer));
    data().PaintFlags = flags;
    bindSurface(0);
}

void TextureMapperGL::endPainting()
{
    if (data().didModifyStencil) {
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    glUseProgram(data().previousProgram);

    glScissor(data().previousScissor[0], data().previousScissor[1], data().previousScissor[2], data().previousScissor[3]);
    if (data().previousScissorState)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (data().previousDepthState)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

#if PLATFORM(QT)
    if (!m_context)
        return;
    QPainter* painter = m_context->platformContext();
    painter->endNativePainting();
    painter->restore();
#endif
}

void TextureMapperGL::drawRect(const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram* shaderProgram, GLenum drawingMode, bool needsBlending)
{
    GL_CMD(glEnableVertexAttribArray(shaderProgram->vertexAttrib()));
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0));
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(shaderProgram->vertexAttrib(), 2, GL_FLOAT, GL_FALSE, 0, unitRect));

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
    GL_CMD(glUniformMatrix4fv(shaderProgram->matrixVariable(), 1, GL_FALSE, m4));

    if (needsBlending) {
        GL_CMD(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        GL_CMD(glEnable(GL_BLEND));
    } else
        GL_CMD(glDisable(GL_BLEND));

    GL_CMD(glDrawArrays(drawingMode, 0, 4));
    GL_CMD(glDisableVertexAttribArray(shaderProgram->vertexAttrib()));
}

void TextureMapperGL::drawBorder(const Color& color, float width, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix)
{
    if (clipStack().current().scissorBox.isEmpty())
        return;

    RefPtr<TextureMapperShaderProgramSolidColor> shaderInfo = data().sharedGLData().textureMapperShaderManager.solidColorProgram();
    GL_CMD(glUseProgram(shaderInfo->id()));

    float alpha = color.alpha() / 255.0;
    GL_CMD(glUniform4f(shaderInfo->colorVariable(),
                       (color.red() / 255.0) * alpha,
                       (color.green() / 255.0) * alpha,
                       (color.blue() / 255.0) * alpha,
                       alpha));
    GL_CMD(glLineWidth(width));

    drawRect(targetRect, modelViewMatrix, shaderInfo.get(), GL_LINE_LOOP, color.hasAlpha());
}

void TextureMapperGL::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* mask)
{
    if (!texture.isValid())
        return;

    if (clipStack().current().scissorBox.isEmpty())
        return;

    const BitmapTextureGL& textureGL = static_cast<const BitmapTextureGL&>(texture);
    drawTexture(textureGL.id(), textureGL.isOpaque() ? 0 : SupportsBlending, textureGL.size(), targetRect, matrix, opacity, mask);
}

void TextureMapperGL::drawTexture(uint32_t texture, Flags flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, const BitmapTexture* maskTexture)
{
    RefPtr<TextureMapperShaderProgram> shaderInfo;
    if (maskTexture)
        shaderInfo = data().sharedGLData().textureMapperShaderManager.getShaderProgram(TextureMapperShaderManager::OpacityAndMask);
    else
        shaderInfo = data().sharedGLData().textureMapperShaderManager.getShaderProgram(TextureMapperShaderManager::Simple);
    GL_CMD(glUseProgram(shaderInfo->id()));

    GL_CMD(glEnableVertexAttribArray(shaderInfo->vertexAttrib()));
    GL_CMD(glActiveTexture(GL_TEXTURE0));
    GL_CMD(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CMD(glUniform1i(shaderInfo->sourceTextureVariable(), 0));

    const GLfloat m4src[] = {
        1, 0, 0, 0,
        0, (flags & ShouldFlipTexture) ? -1 : 1, 0, 0,
        0, 0, 1, 0,
        0, (flags & ShouldFlipTexture) ? 1 : 0, 0, 1};
    GL_CMD(glUniformMatrix4fv(shaderInfo->sourceMatrixVariable(), 1, GL_FALSE, m4src));

    shaderInfo->prepare(opacity, maskTexture);

    bool needsBlending = (flags & SupportsBlending) || opacity < 0.99 || maskTexture;
    drawRect(targetRect, modelViewMatrix, shaderInfo.get(), GL_TRIANGLE_FAN, needsBlending);
}

const char* TextureMapperGL::type() const
{
    return "OpenGL";
}

bool BitmapTextureGL::canReuseWith(const IntSize& contentsSize, Flags)
{
    return contentsSize == m_textureSize;
}

#if OS(DARWIN)
#define DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#else
#define DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE GL_UNSIGNED_BYTE
#endif

static void swizzleBGRAToRGBA(uint32_t* data, const IntRect& rect, int stride = 0)
{
    stride = stride ? stride : rect.width();
    for (int y = rect.y(); y < rect.maxY(); ++y) {
        uint32_t* p = data + y * stride;
        for (int x = rect.x(); x < rect.maxX(); ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
    }
}

static bool driverSupportsBGRASwizzling()
{
#if defined(TEXMAP_OPENGL_ES_2)
    // FIXME: Implement reliable detection. See also https://bugs.webkit.org/show_bug.cgi?id=81103.
    return false;
#else
    return true;
#endif
}

static bool driverSupportsSubImage()
{
#if defined(TEXMAP_OPENGL_ES_2)
    // FIXME: Implement reliable detection.
    return false;
#else
    return true;
#endif
}

void BitmapTextureGL::didReset()
{
    if (!m_id)
        GL_CMD(glGenTextures(1, &m_id));

    m_shouldClear = true;
    if (m_textureSize == contentSize())
        return;

    GLuint format = driverSupportsBGRASwizzling() ? GL_BGRA : GL_RGBA;

    m_textureSize = contentSize();
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CMD(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_textureSize.width(), m_textureSize.height(), 0, format, DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE, 0));
}


void BitmapTextureGL::updateContents(const void* data, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine)
{
    GLuint glFormat = GL_RGBA;
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id));

    if (driverSupportsBGRASwizzling())
        glFormat = GL_BGRA;
    else
        swizzleBGRAToRGBA(reinterpret_cast<uint32_t*>(const_cast<void*>(data)), IntRect(sourceOffset, targetRect.size()), bytesPerLine / 4);

    if (bytesPerLine == targetRect.width() / 4 && sourceOffset == IntPoint::zero()) {
        GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), glFormat, DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE, (const char*)data));
        return;
    }

    // For ES drivers that don't support sub-images, transfer the pixels row-by-row.
    if (!driverSupportsSubImage()) {
        const char* bits = static_cast<const char*>(data);
        for (int y = 0; y < targetRect.height(); ++y) {
            const char *row = bits + ((sourceOffset.y() + y) * bytesPerLine + sourceOffset.x() * 4);
            GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y() + y, targetRect.width(), 1, glFormat, DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE, row));
        }
        return;
    }

#if !defined(TEXMAP_OPENGL_ES_2)
    // Use the OpenGL sub-image extension, now that we know it's available.
    GL_CMD(glPixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerLine / 4));
    GL_CMD(glPixelStorei(GL_UNPACK_SKIP_ROWS, sourceOffset.y()));
    GL_CMD(glPixelStorei(GL_UNPACK_SKIP_PIXELS, sourceOffset.x()));
    GL_CMD(glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), glFormat, DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE, (const char*)data));
    GL_CMD(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    GL_CMD(glPixelStorei(GL_UNPACK_SKIP_ROWS, 0));
    GL_CMD(glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0));
#endif
}

void BitmapTextureGL::updateContents(Image* image, const IntRect& targetRect, const IntPoint& offset)
{
    if (!image)
        return;
    NativeImagePtr frameImage = image->nativeImageForCurrentFrame();
    if (!frameImage)
        return;

    int bytesPerLine;
    const char* imageData;

#if PLATFORM(QT)
    QImage qtImage;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // With QPA, we can avoid a deep copy.
    qtImage = *frameImage->handle()->buffer();
#else
    // This might be a deep copy, depending on other references to the pixmap.
    qtImage = frameImage->toImage();
#endif
    imageData = reinterpret_cast<const char*>(qtImage.constBits());
    bytesPerLine = qtImage.bytesPerLine();
#elif USE(CAIRO)
    cairo_surface_t* surface = frameImage->surface();
    imageData = reinterpret_cast<const char*>(cairo_image_surface_get_data(surface));
    bytesPerLine = cairo_image_surface_get_stride(surface);
#endif

    updateContents(imageData, targetRect, offset, bytesPerLine);
}

#if ENABLE(CSS_FILTERS)
void TextureMapperGL::drawFiltered(const BitmapTexture& sourceTexture, const BitmapTexture& contentTexture, const FilterOperation& filter)
{
    // For standard filters, we always draw the whole texture without transformations.
    RefPtr<StandardFilterProgram> program = data().sharedGLData().textureMapperShaderManager.getShaderForFilter(filter);
    if (!program) {
        drawTexture(sourceTexture, FloatRect(FloatPoint::zero(), sourceTexture.size()), TransformationMatrix(), 1, 0);
        return;
    }
    GL_CMD(glEnableVertexAttribArray(program->vertexAttrib()));
    GL_CMD(glEnableVertexAttribArray(program->texCoordAttrib()));
    GL_CMD(glActiveTexture(GL_TEXTURE0));
    GL_CMD(glBindTexture(GL_TEXTURE_2D, static_cast<const BitmapTextureGL&>(sourceTexture).id()));
    glUniform1i(program->textureUniform(), 0);
    const GLfloat targetVertices[] = {-1, -1, 1, -1, 1, 1, -1, 1};
    const GLfloat sourceVertices[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(program->vertexAttrib(), 2, GL_FLOAT, GL_FALSE, 0, targetVertices));
    GL_CMD(glVertexAttribPointer(program->texCoordAttrib(), 2, GL_FLOAT, GL_FALSE, 0, sourceVertices));
    GL_CMD(glDisable(GL_BLEND));
    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
    GL_CMD(glDisableVertexAttribArray(program->vertexAttrib()));
    GL_CMD(glDisableVertexAttribArray(program->texCoordAttrib()));
}

PassRefPtr<BitmapTexture> BitmapTextureGL::applyFilters(const BitmapTexture& contentTexture, const FilterOperations& filters)
{
    RefPtr<BitmapTexture> previousSurface = m_textureMapper->data().currentSurface;

    RefPtr<BitmapTexture> source = this;
    RefPtr<BitmapTexture> target = m_textureMapper->acquireTextureFromPool(m_textureSize);
    for (int i = 0; i < filters.size(); ++i) {
        const FilterOperation* filter = filters.at(i);
        ASSERT(filter);

        m_textureMapper->bindSurface(target.get());
        m_textureMapper->drawFiltered(i ? *source.get() : contentTexture, contentTexture, *filter);
        std::swap(source, target);
    }

    m_textureMapper->bindSurface(previousSurface.get());
    return source;
}
#endif

static inline TransformationMatrix createProjectionMatrix(const IntSize& size, bool mirrored)
{
    const float near = 9999999;
    const float far = -99999;

    return TransformationMatrix(2.0 / float(size.width()), 0, 0, 0,
                                0, (mirrored ? 2.0 : -2.0) / float(size.height()), 0, 0,
                                0, 0, -2.f / (far - near), 0,
                                -1, mirrored ? -1 : 1, -(far + near) / (far - near), 1);
}

void BitmapTextureGL::initializeStencil()
{
    if (m_rbo)
        return;
    GL_CMD(glGenRenderbuffers(1, &m_rbo));
    GL_CMD(glBindRenderbuffer(GL_RENDERBUFFER, m_rbo));
#ifdef TEXMAP_OPENGL_ES_2
    GL_CMD(glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_textureSize.width(), m_textureSize.height()));
#else
    GL_CMD(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, m_textureSize.width(), m_textureSize.height()));
#endif
    GL_CMD(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    GL_CMD(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo));
    GL_CMD(glClearStencil(0));
    GL_CMD(glClear(GL_STENCIL_BUFFER_BIT));
}

void BitmapTextureGL::clearIfNeeded()
{
    if (!m_shouldClear)
        return;

    m_clipStack.init(IntRect(IntPoint::zero(), m_textureSize));
    m_clipStack.apply();
    GL_CMD(glClearColor(0, 0, 0, 0));
    GL_CMD(glClear(GL_COLOR_BUFFER_BIT));
    m_shouldClear = false;
}

void BitmapTextureGL::createFboIfNeeded()
{
    if (m_fbo)
        return;

    GL_CMD(glGenFramebuffers(1, &m_fbo));
    GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    GL_CMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id(), 0));
    m_shouldClear = true;
}

void BitmapTextureGL::bind()
{
    GL_CMD(glBindTexture(GL_TEXTURE_2D, 0));
    createFboIfNeeded();
    GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    GL_CMD(glViewport(0, 0, m_textureSize.width(), m_textureSize.height()));
    clearIfNeeded();
    m_textureMapper->data().projectionMatrix = createProjectionMatrix(m_textureSize, true /* mirrored */);
    m_clipStack.apply();
}

BitmapTextureGL::~BitmapTextureGL()
{
    if (m_id)
        GL_CMD(glDeleteTextures(1, &m_id));

    if (m_fbo)
        GL_CMD(glDeleteFramebuffers(1, &m_fbo));

    if (m_rbo)
        GL_CMD(glDeleteRenderbuffers(1, &m_rbo));
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

void TextureMapperGL::bindDefaultSurface()
{
    GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, data().targetFrameBuffer));
    IntSize viewportSize(data().viewport[2], data().viewport[3]);
    data().projectionMatrix = createProjectionMatrix(viewportSize, data().PaintFlags & PaintingMirrored);
    GL_CMD(glViewport(data().viewport[0], data().viewport[1], viewportSize.width(), viewportSize.height()));
    m_clipStack.apply();
    data().currentSurface.clear();
}

void TextureMapperGL::bindSurface(BitmapTexture *surface)
{
    if (!surface) {
        bindDefaultSurface();
        return;
    }

    static_cast<BitmapTextureGL*>(surface)->bind();
    data().currentSurface = surface;
}

bool TextureMapperGL::beginScissorClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    // 3D transforms are currently not supported in scissor clipping
    // resulting in cropped surfaces when z>0.
    if (!modelViewMatrix.isAffine())
        return false;

    FloatQuad quad = modelViewMatrix.projectQuad(targetRect);
    IntRect rect = quad.enclosingBoundingBox();

    // Only use scissors on rectilinear clips.
    if (!quad.isRectilinear() || rect.isEmpty())
        return false;

    clipStack().current().scissorBox.intersect(rect);
    clipStack().apply();
    return true;
}

void TextureMapperGL::beginClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    clipStack().push();
    if (beginScissorClip(modelViewMatrix, targetRect))
        return;

    data().initializeStencil();

    RefPtr<TextureMapperShaderProgram> shaderInfo = data().sharedGLData().textureMapperShaderManager.getShaderProgram(TextureMapperShaderManager::Simple);

    GL_CMD(glUseProgram(shaderInfo->id()));
    GL_CMD(glEnableVertexAttribArray(shaderInfo->vertexAttrib()));
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(shaderInfo->vertexAttrib(), 2, GL_FLOAT, GL_FALSE, 0, unitRect));

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

    const GLfloat m4all[] = {
        2, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, 1, 0,
        -1, -1, 0, 1
    };

    int& stencilIndex = clipStack().current().stencilIndex;

    GL_CMD(glEnable(GL_STENCIL_TEST));

    // Make sure we don't do any actual drawing.
    GL_CMD(glStencilFunc(GL_NEVER, stencilIndex, stencilIndex));

    // Operate only on the stencilIndex and above.
    GL_CMD(glStencilMask(0xff & ~(stencilIndex - 1)));

    // First clear the entire buffer at the current index.
    GL_CMD(glUniformMatrix4fv(shaderInfo->matrixVariable(), 1, GL_FALSE, m4all));
    GL_CMD(glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO));
    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));

    // Now apply the current index to the new quad.
    GL_CMD(glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE));
    GL_CMD(glUniformMatrix4fv(shaderInfo->matrixVariable(), 1, GL_FALSE, m4));
    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));

    // Clear the state.
    GL_CMD(glDisableVertexAttribArray(shaderInfo->vertexAttrib()));
    GL_CMD(glStencilMask(0));

    // Increase stencilIndex and apply stencil testing.
    stencilIndex *= 2;
    clipStack().apply();
}

void TextureMapperGL::endClip()
{
    clipStack().pop();
    clipStack().apply();
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
