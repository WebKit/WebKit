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

#if PLATFORM(QT) && QT_VERSION >= 0x050000
#include <QOpenGLContext>
#include <QPlatformPixmap>
#endif

#if OS(WINDOWS)
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
#include <wtf/ByteArray.h>
#endif

#define GL_CMD(...) do { __VA_ARGS__; ASSERT_ARG(__VA_ARGS__, !glGetError()); } while (0)

namespace WebCore {
struct TextureMapperGLData {
    struct SharedGLData : public RefCounted<SharedGLData> {
#if PLATFORM(QT) && QT_VERSION >= 0x050000
        typedef QOpenGLContext* GLContext;
        static GLContext getCurrentGLContext()
        {
            return QOpenGLContext::currentContext();
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

        struct ClipState {
            IntRect scissorBox;
            int stencilIndex;
            ClipState(const IntRect& scissors, int stencil)
                : scissorBox(scissors)
                , stencilIndex(stencil)
            { }

            ClipState()
                : stencilIndex(1)
            { }
        };

        ClipState clipState;
        Vector<ClipState> clipStack;

        void pushClipState()
        {
            clipStack.append(clipState);
        }

        void popClipState()
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

        void applyCurrentClip()
        {
            scissorClip(clipState.scissorBox);
            GL_CMD(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
            GL_CMD(glStencilFunc(GL_EQUAL, clipState.stencilIndex - 1, clipState.stencilIndex - 1));
            if (clipState.stencilIndex == 1)
                GL_CMD(glDisable(GL_STENCIL_TEST));
            else
                GL_CMD(glEnable(GL_STENCIL_TEST));
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
    data().sharedGLData().clipState.stencilIndex = 1;
    data().sharedGLData().clipState.scissorBox = IntRect(0, 0, data().viewport[2], data().viewport[3]);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &data().targetFrameBuffer);
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


void TextureMapperGL::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* mask)
{
    if (!texture.isValid())
        return;

    if (data().sharedGLData().clipState.scissorBox.isEmpty())
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
    GL_CMD(glBindBuffer(GL_ARRAY_BUFFER, 0));
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GL_CMD(glVertexAttribPointer(shaderInfo->vertexAttrib(), 2, GL_FLOAT, GL_FALSE, 0, unitRect));

    TransformationMatrix adjustedModelViewMatrix(modelViewMatrix);
    // Check if the transformed target rect has the same shape/dimensions as the drawn texture (i.e. translated only).
    FloatQuad finalQuad = modelViewMatrix.mapQuad(FloatQuad(targetRect));
    FloatSize finalSize = finalQuad.p3() - finalQuad.p1();
    if (abs(textureSize.width() - finalSize.width()) < 0.001
        && abs(textureSize.height() - finalSize.height()) < 0.001
        && finalQuad.p2().y() == finalQuad.p1().y()
        && finalQuad.p2().x() == finalQuad.p3().x()
        && finalQuad.p4().x() == finalQuad.p1().x()
        && finalQuad.p4().y() == finalQuad.p3().y()) {
        // Pixel-align the origin of our layer's coordinate system within the frame buffer's
        // coordinate system to avoid sub-pixel interpolation.
        adjustedModelViewMatrix.setM41(floor(adjustedModelViewMatrix.m41() + 0.5));
        adjustedModelViewMatrix.setM42(floor(adjustedModelViewMatrix.m42() + 0.5));
    }

    TransformationMatrix matrix = TransformationMatrix(data().projectionMatrix).multiply(adjustedModelViewMatrix).multiply(TransformationMatrix(
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

    const GLfloat m4src[] = {
        1, 0, 0, 0,
        0, (flags & ShouldFlipTexture) ? -1 : 1, 0, 0,
        0, 0, 1, 0,
        0, (flags & ShouldFlipTexture) ? 1 : 0, 0, 1};

    GL_CMD(glUniformMatrix4fv(shaderInfo->matrixVariable(), 1, GL_FALSE, m4));
    GL_CMD(glUniformMatrix4fv(shaderInfo->sourceMatrixVariable(), 1, GL_FALSE, m4src));
    GL_CMD(glUniform1i(shaderInfo->sourceTextureVariable(), 0));

    shaderInfo->prepare(opacity, maskTexture);

    bool needsBlending = (flags & SupportsBlending) || opacity < 0.99 || maskTexture;

    if (needsBlending) {
        GL_CMD(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        GL_CMD(glEnable(GL_BLEND));
    } else
        GL_CMD(glDisable(GL_BLEND));

    GL_CMD(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
    GL_CMD(glDisableVertexAttribArray(shaderInfo->vertexAttrib()));
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

void BitmapTextureGL::didReset()
{
    if (!m_id)
        GL_CMD(glGenTextures(1, &m_id));

    m_surfaceNeedsReset = true;
    if (m_textureSize == contentSize())
        return;

    m_textureSize = contentSize();
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CMD(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CMD(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_textureSize.width(), m_textureSize.height(), 0, GL_BGRA, DEFAULT_TEXTURE_PIXEL_TRANSFER_TYPE, 0));
}

static void swizzleBGRAToRGBA(uint32_t* data, const IntSize& size, int stride = 0)
{
    int width = size.width();
    int height = size.height();
    stride = stride | width;
    for (int y = 0; y < height; ++y) {
        uint32_t* p = data + y * stride;
        for (int x = 0; x < width; ++x)
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

void BitmapTextureGL::updateContents(const void* data, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine)
{
    GLuint glFormat = GL_RGBA;
    GL_CMD(glBindTexture(GL_TEXTURE_2D, m_id));

    if (driverSupportsBGRASwizzling())
        glFormat = GL_BGRA;
    else
        swizzleBGRAToRGBA(static_cast<uint32_t*>(const_cast<void*>(data)), targetRect.size(), bytesPerLine / 4);

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

#if !defined(TEXTMAP_OPENGL_ES_2)
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
    imageData = reinterpret_cast<const char*>(cairo_image_surface_get_data(frameImage));
    bytesPerLine = cairo_image_surface_get_stride(frameImage);
#endif

    updateContents(imageData, targetRect, offset, bytesPerLine);

}

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

void BitmapTextureGL::bind()
{
    if (m_surfaceNeedsReset || !m_fbo) {
        if (!m_fbo)
            GL_CMD(glGenFramebuffers(1, &m_fbo));
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
        GL_CMD(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CMD(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id(), 0));
        GL_CMD(glClearColor(0, 0, 0, 0));
        GL_CMD(glClear(GL_COLOR_BUFFER_BIT));
        m_surfaceNeedsReset = false;
    } else
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));

    GL_CMD(glViewport(0, 0, size().width(), size().height()));
    const bool mirrored = true;
    m_textureMapper->data().projectionMatrix = createProjectionMatrix(size(), mirrored);
    m_textureMapper->beginClip(TransformationMatrix(), FloatRect(IntPoint::zero(), contentSize()));
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

void TextureMapperGL::bindSurface(BitmapTexture *surfacePointer)
{
    BitmapTextureGL* surface = static_cast<BitmapTextureGL*>(surfacePointer);

    if (!surface) {
        GL_CMD(glBindFramebuffer(GL_FRAMEBUFFER, data().targetFrameBuffer));

        IntSize viewportSize(data().viewport[2], data().viewport[3]);
        const bool mirorred = data().PaintFlags & PaintingMirrored;
        data().projectionMatrix = createProjectionMatrix(viewportSize, mirorred);
        GL_CMD(glViewport(0, 0, viewportSize.width(), viewportSize.height()));

        if (data().currentSurface)
            endClip();
        data().currentSurface.clear();
        return;
    }


    surface->bind();
    data().currentSurface = surface;
}

bool TextureMapperGL::beginScissorClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    glEnable(GL_SCISSOR_TEST);

    // 3D transforms are currently not supported in scissor clipping
    // resulting in cropped surfaces when z>0.
    if (!modelViewMatrix.isAffine())
        return false;

    FloatQuad quad = modelViewMatrix.projectQuad(targetRect);
    IntRect rect = quad.enclosingBoundingBox();

    // Only use scissors on rectilinear clips.
    if (!quad.isRectilinear() || rect.isEmpty())
        return false;

    data().sharedGLData().clipState.scissorBox.intersect(rect);
    data().sharedGLData().applyCurrentClip();
    return true;
}

void TextureMapperGL::beginClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    data().sharedGLData().pushClipState();
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

    int& stencilIndex = data().sharedGLData().clipState.stencilIndex;

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
    data().sharedGLData().applyCurrentClip();
}

void TextureMapperGL::endClip()
{
    data().sharedGLData().popClipState();
    data().sharedGLData().applyCurrentClip();
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
