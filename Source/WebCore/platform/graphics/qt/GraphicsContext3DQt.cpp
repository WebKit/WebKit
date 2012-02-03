/*
    Copyright (C) 2010 Tieto Corporation.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
     
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
     
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "GraphicsContext3D.h"

#include "WebGLObject.h"
#include <cairo/OpenGLShims.h>
#include "CanvasRenderingContext.h"
#if defined(QT_OPENGL_ES_2)
#include "Extensions3DQt.h"
#else
#include "Extensions3DOpenGL.h"
#endif
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "QWebPageClient.h"
#include "SharedBuffer.h"
#include <QAbstractScrollArea>
#include <QGraphicsObject>
#include <QGLContext>
#include <QStyleOptionGraphicsItem>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER) && USE(TEXTURE_MAPPER_GL)
#include <opengl/TextureMapperGL.h>
#endif

#if ENABLE(WEBGL)

namespace WebCore {

#if !defined(GLchar)
typedef char GLchar;
#endif

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

class GraphicsContext3DPrivate
#if USE(ACCELERATED_COMPOSITING)
#if USE(TEXTURE_MAPPER)
        : public TextureMapperPlatformLayer
#else
        : public QGraphicsObject
#endif
#endif
{
public:
    GraphicsContext3DPrivate(GraphicsContext3D*, HostWindow*);
    ~GraphicsContext3DPrivate();

    QGLWidget* getViewportGLWidget();
#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect& target, const TransformationMatrix&, float opacity, BitmapTexture* mask) const;
#endif

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);
    QRectF boundingRect() const;
    void blitMultisampleFramebuffer() const;
    void blitMultisampleFramebufferAndRestoreContext() const;
    bool makeCurrentIfNeeded() const;

    GraphicsContext3D* m_context;
    HostWindow* m_hostWindow;
    QGLWidget* m_glWidget;
    QGLWidget* m_viewportGLWidget;
};

bool GraphicsContext3D::isGLES2Compliant() const
{
#if defined (QT_OPENGL_ES_2)
    return true;
#else
    return false;
#endif
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* hostWindow)
    : m_context(context)
    , m_hostWindow(hostWindow)
    , m_glWidget(0)
    , m_viewportGLWidget(0)
{
    m_viewportGLWidget = getViewportGLWidget();

    if (m_viewportGLWidget)
        m_glWidget = new QGLWidget(0, m_viewportGLWidget);
    else
        m_glWidget = new QGLWidget();

    // Geometry can be set to zero because m_glWidget is used only for its QGLContext.
    m_glWidget->setGeometry(0, 0, 0, 0);

    makeCurrentIfNeeded();
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    delete m_glWidget;
    m_glWidget = 0;
}

QGLWidget* GraphicsContext3DPrivate::getViewportGLWidget()
{
    QWebPageClient* webPageClient = m_hostWindow->platformPageClient();
    if (!webPageClient)
        return 0;

    QAbstractScrollArea* scrollArea = qobject_cast<QAbstractScrollArea*>(webPageClient->ownerWidget());
    if (scrollArea)
        return qobject_cast<QGLWidget*>(scrollArea->viewport());

    return 0;
}

static inline quint32 swapBgrToRgb(quint32 pixel)
{
    return ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff) | (pixel & 0xff00ff00);
}

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
void GraphicsContext3DPrivate::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, BitmapTexture* mask) const
{
    blitMultisampleFramebufferAndRestoreContext();

    if (textureMapper->isOpenGLBacked()) {
        TextureMapperGL* texmapGL = static_cast<TextureMapperGL*>(textureMapper);
        texmapGL->drawTexture(m_context->m_texture, !m_context->m_attrs.alpha, FloatSize(1, 1), targetRect, matrix, opacity, mask, true /* flip */);
        return;
    }

    GraphicsContext* context = textureMapper->graphicsContext();
    QPainter* painter = context->platformContext();
    painter->save();
    painter->setTransform(matrix);
    painter->setOpacity(opacity);

    const int height = m_context->m_currentHeight;
    const int width = m_context->m_currentWidth;

    // Alternatively read pixels to a memory buffer.
    QImage offscreenImage(width, height, QImage::Format_ARGB32);
    quint32* imagePixels = reinterpret_cast<quint32*>(offscreenImage.bits());

    makeCurrentIfNeeded();
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_fbo);
    glReadPixels(/* x */ 0, /* y */ 0, width, height, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, imagePixels);
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_boundFBO);

    // OpenGL gives us ABGR on 32 bits, and with the origin at the bottom left
    // We need RGB32 or ARGB32_PM, with the origin at the top left.
    quint32* pixelsSrc = imagePixels;
    const int halfHeight = height / 2;
    for (int row = 0; row < halfHeight; ++row) {
        const int targetIdx = (height - 1 - row) * width;
        quint32* pixelsDst = imagePixels + targetIdx;
        for (int column = 0; column < width; ++column) {
            quint32 tempPixel = *pixelsSrc;
            *pixelsSrc = swapBgrToRgb(*pixelsDst);
            *pixelsDst = swapBgrToRgb(tempPixel);
            ++pixelsSrc;
            ++pixelsDst;
        }
    }
    if (static_cast<int>(height) % 2) {
        for (int column = 0; column < width; ++column) {
            *pixelsSrc = swapBgrToRgb(*pixelsSrc);
            ++pixelsSrc;
        }
    }

    painter->drawImage(targetRect, offscreenImage);
    painter->restore();
}
#endif // USE(ACCELERATED_COMPOSITING)

QRectF GraphicsContext3DPrivate::boundingRect() const
{
    return QRectF(QPointF(0, 0), QSizeF(m_context->m_currentWidth, m_context->m_currentHeight));
}

void GraphicsContext3DPrivate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget);

    QRectF rect = option ? option->rect : boundingRect();

    makeCurrentIfNeeded();
    blitMultisampleFramebuffer();

    // Use direct texture mapping if WebGL canvas has a shared OpenGL context
    // with browsers OpenGL context.
    QGLWidget* viewportGLWidget = getViewportGLWidget();
    if (viewportGLWidget && viewportGLWidget == m_viewportGLWidget && viewportGLWidget == painter->device()) {
        viewportGLWidget->drawTexture(rect, m_context->m_texture);
        return;
    }

    // Alternatively read pixels to a memory buffer.
    QImage offscreenImage(rect.width(), rect.height(), QImage::Format_ARGB32);
    quint32* imagePixels = reinterpret_cast<quint32*>(offscreenImage.bits());

    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_fbo);
    glReadPixels(/* x */ 0, /* y */ 0, rect.width(), rect.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, imagePixels);
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_boundFBO);

    // OpenGL gives us ABGR on 32 bits, and with the origin at the bottom left
    // We need RGB32 or ARGB32_PM, with the origin at the top left.
    quint32* pixelsSrc = imagePixels;
    const int height = static_cast<int>(rect.height());
    const int width = static_cast<int>(rect.width());
    const int halfHeight = height / 2;
    for (int row = 0; row < halfHeight; ++row) {
        const int targetIdx = (height - 1 - row) * width;
        quint32* pixelsDst = imagePixels + targetIdx;
        for (int column = 0; column < width; ++column) {
            quint32 tempPixel = *pixelsSrc;
            *pixelsSrc = swapBgrToRgb(*pixelsDst);
            *pixelsDst = swapBgrToRgb(tempPixel);
            ++pixelsSrc;
            ++pixelsDst;
        }
    }
    if (static_cast<int>(height) % 2) {
        for (int column = 0; column < width; ++column) {
            *pixelsSrc = swapBgrToRgb(*pixelsSrc);
            ++pixelsSrc;
        }
    }

    painter->drawImage(/* x */ 0, /* y */ 0, offscreenImage);
}

void GraphicsContext3DPrivate::blitMultisampleFramebuffer() const
{
    if (!m_context->m_attrs.antialias)
        return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, m_context->m_multisampleFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, m_context->m_fbo);
    glBlitFramebuffer(0, 0, m_context->m_currentWidth, m_context->m_currentHeight, 0, 0, m_context->m_currentWidth, m_context->m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_boundFBO);
}

void GraphicsContext3DPrivate::blitMultisampleFramebufferAndRestoreContext() const
{
    if (!m_context->m_attrs.antialias)
        return;

    const QGLContext* currentContext = QGLContext::currentContext();
    const QGLContext* widgetContext = m_glWidget->context();
    if (currentContext != widgetContext)
        m_glWidget->makeCurrent();
    blitMultisampleFramebuffer();
    if (currentContext) {
        if (currentContext != widgetContext)
            const_cast<QGLContext*>(currentContext)->makeCurrent();
    } else
        m_glWidget->doneCurrent();
}

bool GraphicsContext3DPrivate::makeCurrentIfNeeded() const
{
    const QGLContext* currentContext = QGLContext::currentContext();
    const QGLContext* widgetContext = m_glWidget->context();
    if (currentContext != widgetContext)
        m_glWidget->makeCurrent();

    return QGLContext::currentContext() == widgetContext;
}

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return 0;
    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, false));
    return context->m_private ? context.release() : 0;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, bool)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_attrs(attrs)
    , m_texture(0)
    , m_compositorTexture(0)
    , m_fbo(0)
#if defined(QT_OPENGL_ES_2)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
#else
    , m_depthStencilBuffer(0)
#endif
    , m_layerComposited(false)
    , m_internalColorFormat(0)
    , m_boundFBO(0)
    , m_activeTexture(GL_TEXTURE0)
    , m_boundTexture0(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
    , m_private(adoptPtr(new GraphicsContext3DPrivate(this, hostWindow)))
{
#if !defined(QT_OPENGL_ES_2)
    validateAttributes();
#endif

    if (!m_private->m_glWidget->isValid()) {
        LOG_ERROR("GraphicsContext3D: QGLWidget initialization failed.");
        m_private = nullptr;
        return;
    }

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    if (!success) {
        m_private = nullptr;
        return;
    }

    // Create buffers for the canvas FBO.
    glGenFramebuffers(/* count */ 1, &m_fbo);

    glGenTextures(1, &m_texture);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, m_texture);
    glTexParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    glTexParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    glTexParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    glTexParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, 0);

    // Create a multisample FBO.
    if (m_attrs.antialias) {
        glGenFramebuffers(1, &m_multisampleFBO);
        glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
        m_boundFBO = m_multisampleFBO;
        glGenRenderbuffers(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        m_boundFBO = m_fbo;
#if defined(QT_OPENGL_ES_2)
        if (m_attrs.depth)
            glGenRenderbuffers(1, &m_depthBuffer);
        if (m_attrs.stencil)
            glGenRenderbuffers(1, &m_stencilBuffer);
#else
        if (m_attrs.stencil || m_attrs.depth)
            glGenRenderbuffers(1, &m_depthStencilBuffer);
#endif
    }

#if !defined(QT_OPENGL_ES_2)
    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    ShInitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits); 
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    m_compiler.setResources(ANGLEResources);

    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    glClearColor(0.0, 0.0, 0.0, 0.0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    makeContextCurrent();
    if (!m_private->m_glWidget->isValid())
        return;
    glDeleteTextures(1, &m_texture);
    glDeleteFramebuffers(1, &m_fbo);
    if (m_attrs.antialias) {
        glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        glDeleteFramebuffers(1, &m_multisampleFBO);
        if (m_attrs.stencil || m_attrs.depth)
            glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else if (m_attrs.stencil || m_attrs.depth) {
#if defined(QT_OPENGL_ES_2)
        if (m_attrs.depth)
            glDeleteRenderbuffers(1, &m_depthBuffer);
        if (m_attrs.stencil)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#else
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
#endif
    }
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D()
{
    return m_private->m_glWidget;
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_texture;
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_private.get();
}
#endif

bool GraphicsContext3D::makeContextCurrent()
{
    return m_private->makeCurrentIfNeeded();
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context, DrawingBuffer*)
{
    makeContextCurrent();
    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();
    QPainter* painter = imageBuffer->context()->platformContext();
    m_private->paint(painter, 0, 0);
}

#if defined(QT_OPENGL_ES_2)
PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData(DrawingBuffer*)
{
    // FIXME: This needs to be implemented for proper non-premultiplied-alpha
    // support.
    return 0;
}

void GraphicsContext3D::reshape(int width, int height)
{
    if ((width == m_currentWidth && height == m_currentHeight) || (!m_private))
        return;

    m_currentWidth = width;
    m_currentHeight = height;

    makeContextCurrent();

    // Create color buffer
    glBindTexture(GraphicsContext3D::TEXTURE_2D, m_texture);
    if (m_attrs.alpha)
        glTexImage2D(GraphicsContext3D::TEXTURE_2D, /* level */ 0, GraphicsContext3D::RGBA, width, height, /* border */ 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, /* data */ 0);
    else
        glTexImage2D(GraphicsContext3D::TEXTURE_2D, /* level */ 0, GraphicsContext3D::RGB, width, height, /* border */ 0, GraphicsContext3D::RGB, GraphicsContext3D::UNSIGNED_BYTE, /* data */ 0);
    glBindTexture(GraphicsContext3D::TEXTURE_2D, 0);

    // Create depth and stencil buffers.
#if defined(QT_OPENGL_ES_2)
    if (m_attrs.depth) {
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
        glRenderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, width, height);
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    }
    if (m_attrs.stencil) {
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);
        glRenderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::STENCIL_INDEX8, width, height);
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    }
#else
    if (m_attrs.depth || m_attrs.stencil) {
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        if (m_attrs.depth && m_attrs.stencil)
            glRenderbufferStorage(GraphicsContext3D::RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        else if (m_attrs.depth)
            glRenderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT, width, height);
        else if (m_attrs.stencil)
            glRenderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::STENCIL_INDEX, width, height);
        glBindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    }
#endif

    // Construct canvas FBO.
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_texture, 0);
#if defined(QT_OPENGL_ES_2)
    if (m_attrs.depth)
        glFramebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
    if (m_attrs.stencil)
        glFramebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);
#else
    if (m_attrs.depth && m_attrs.stencil)
        glFramebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
    else if (m_attrs.depth)
        glFramebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
    else if (m_attrs.stencil)
        glFramebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
#endif

    GLenum status = glCheckFramebufferStatus(GraphicsContext3D::FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("GraphicsContext3D: Canvas FBO initialization failed.");
        return;
    }

    int clearFlags = GraphicsContext3D::COLOR_BUFFER_BIT;
    if (m_attrs.depth)
        clearFlags |= GraphicsContext3D::DEPTH_BUFFER_BIT;
    if (m_attrs.stencil)
        clearFlags |= GraphicsContext3D::STENCIL_BUFFER_BIT;

    glClear(clearFlags);
    glFlush();
}

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return IntSize(m_currentWidth, m_currentHeight);
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    makeContextCurrent();
    glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    glAttachShader(program, shader);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }

    makeContextCurrent();
    glGetAttachedShaders(program, maxCount, count, shaders);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    ASSERT(program);
    makeContextCurrent();
    glBindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    glBindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    m_boundFBO = buffer ? buffer : m_fbo;
    glBindFramebuffer(target, m_boundFBO);
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    makeContextCurrent();
    glBindRenderbuffer(target, renderbuffer);
}

void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    makeContextCurrent();
    glBindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    glBlendColor(red, green, blue, alpha);
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    makeContextCurrent();
    glBlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    makeContextCurrent();
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    makeContextCurrent();
    glBlendFunc(sfactor, dfactor);
}       

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    makeContextCurrent();
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    makeContextCurrent();
    glBufferData(target, size, /* data */ 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    makeContextCurrent();
    glBufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    makeContextCurrent();
    glBufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    makeContextCurrent();
    return glCheckFramebufferStatus(target);
}

void GraphicsContext3D::clearColor(GC3Dclampf r, GC3Dclampf g, GC3Dclampf b, GC3Dclampf a)
{
    makeContextCurrent();
    glClearColor(r, g, b, a);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    makeContextCurrent();
    glClear(mask);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
#if defined(QT_OPENGL_ES_2)
    glClearDepthf(depth);
#else
    glClearDepth(depth);
#endif
}

void GraphicsContext3D::clearStencil(GC3Dint s)
{
    makeContextCurrent();
    glClearStencil(s);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    makeContextCurrent();
    glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    makeContextCurrent();
    glCompileShader(shader);
}

void GraphicsContext3D::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void GraphicsContext3D::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    makeContextCurrent();
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    makeContextCurrent();
    glCullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    makeContextCurrent();
    glDepthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    makeContextCurrent();
    glDepthMask(flag);
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
#if defined(QT_OPENGL_ES_2)
    glDepthRangef(zNear, zFar);
#else
    glDepthRange(zNear, zFar);
#endif
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    glDetachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    makeContextCurrent();
    glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    glDisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    makeContextCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    makeContextCurrent();
    glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    makeContextCurrent();
    glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    glEnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    makeContextCurrent();
    glFinish();
}

void GraphicsContext3D::flush()
{
    makeContextCurrent();
    glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject buffer)
{
    makeContextCurrent();
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject texture, GC3Dint level)
{
    makeContextCurrent();
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    makeContextCurrent();
    glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    makeContextCurrent();
    glGenerateMipmap(target);
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    makeContextCurrent();

    GLint maxLength = 0;
    glGetProgramiv(program, GraphicsContext3D::ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);

    GLchar* name = (GLchar*) fastMalloc(maxLength);
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;

    glGetActiveAttrib(program, index, maxLength, &nameLength, &size, &type, name);

    if (!nameLength) {
        fastFree(name);
        return false;
    }

    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;

    fastFree(name);
    return true;
}
    
bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    makeContextCurrent();

    GLint maxLength = 0;
    glGetProgramiv(static_cast<GLuint>(program), GraphicsContext3D::ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

    GLchar* name = (GLchar*) fastMalloc(maxLength);
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;

    glGetActiveUniform(static_cast<GLuint>(program), index, maxLength, &nameLength, &size, &type, name);

    if (!nameLength) {
        fastFree(name);
        return false;
    }

    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;

    fastFree(name);
    return true;
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;
    
    makeContextCurrent();
    return glGetAttribLocation(program, name.utf8().data());
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    return m_attrs;
}

GC3Denum GraphicsContext3D::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<GC3Denum>::iterator iter = m_syntheticErrors.begin();
        GC3Denum err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }

    makeContextCurrent();
    return glGetError();
}

String GraphicsContext3D::getString(GC3Denum name)
{
    makeContextCurrent();
    return String((const char*) glGetString(name));
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    makeContextCurrent();
    glHint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    if (!buffer)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsBuffer(buffer);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    makeContextCurrent();
    return glIsEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsFramebuffer(framebuffer);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject program)
{
    if (!program)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsProgram(program);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsRenderbuffer(renderbuffer);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject shader)
{
    if (!shader)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsShader(shader);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject texture)
{
    if (!texture)
        return GL_FALSE;
    
    makeContextCurrent();
    return glIsTexture(texture);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    makeContextCurrent();
    glLineWidth(static_cast<float>(width));
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    makeContextCurrent();
    glLinkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum paramName, GC3Dint param)
{
    makeContextCurrent();
    glPixelStorei(paramName, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    makeContextCurrent();
    glPolygonOffset(factor, units);
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    makeContextCurrent();
    
    if (type != GraphicsContext3D::UNSIGNED_BYTE || format != GraphicsContext3D::RGBA)
        return;
        
    glReadPixels(x, y, width, height, format, type, data);
}

void GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
#if !defined(QT_OPENGL_ES_2)
    switch (internalformat) {
    case DEPTH_STENCIL:
        internalformat = GL_DEPTH24_STENCIL8;
        break;
    case DEPTH_COMPONENT16:
        internalformat = DEPTH_COMPONENT;
        break;
    case RGBA4:
    case RGB5_A1:
        internalformat = RGBA;
        break;
    case RGB565:
        internalformat = RGB;
        break;
    }
#endif
    glRenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    makeContextCurrent();
    glSampleCoverage(value, invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& source)
{
    ASSERT(shader);

    makeContextCurrent();

    String prefixedSource;

#if defined (QT_OPENGL_ES_2)
    prefixedSource.append("precision mediump float;\n");
#endif

    prefixedSource.append(source);

    CString sourceCS = prefixedSource.utf8();
    const char* data = sourceCS.data();
    int length = prefixedSource.length();
    glShaderSource((GLuint) shader, /* count */ 1, &data, &length);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    makeContextCurrent();
    glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    makeContextCurrent();
    glStencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    glStencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum paramName, GC3Dfloat value)
{
    makeContextCurrent();
    glTexParameterf(target, paramName, value);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum paramName, GC3Dint value)
{
    makeContextCurrent();
    glTexParameteri(target, paramName, value);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat v0)
{
    makeContextCurrent();
    glUniform1f(location, v0);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform1fv(location, size, array);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    glUniform2f(location, v0, v1);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform2fv(location, size, array);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    glUniform3f(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform3fv(location, size, array);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    glUniform4f(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform4fv(location, size, array);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint v0)
{
    makeContextCurrent();
    glUniform1i(location, v0);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dint* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform1iv(location, size, array);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint v0, GC3Dint v1)
{
    makeContextCurrent();
    glUniform2i(location, v0, v1);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dint* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform2iv(location, size, array);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2)
{
    makeContextCurrent();
    glUniform3i(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dint* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform3iv(location, size, array);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2, GC3Dint v3)
{
    makeContextCurrent();
    glUniform4i(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dint* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniform4iv(location, size, array);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* array, GC3Dsizei size)
{
    makeContextCurrent();
    glUniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    makeContextCurrent();
    glUseProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    ASSERT(program);
    
    makeContextCurrent();
    glValidateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat v0)
{
    makeContextCurrent();
    glVertexAttrib1f(index, v0);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* array)
{
    makeContextCurrent();
    glVertexAttrib1fv(index, array);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    glVertexAttrib2f(index, v0, v1);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* array)
{
    makeContextCurrent();
    glVertexAttrib2fv(index, array);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    glVertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* array)
{
    makeContextCurrent();
    glVertexAttrib3fv(index, array);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    glVertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* array)
{
    makeContextCurrent();
    glVertexAttrib4fv(index, array);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    makeContextCurrent();
    glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    glViewport(x, y, width, height);
}

void GraphicsContext3D::getBooleanv(GC3Denum paramName, GC3Dboolean* value)
{
    makeContextCurrent();
    glGetBooleanv(paramName, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetBufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getFloatv(GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    glGetFloatv(paramName, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetFramebufferAttachmentParameteriv(target, attachment, paramName, value);
}

void GraphicsContext3D::getIntegerv(GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetIntegerv(paramName, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetProgramiv(program, paramName, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    makeContextCurrent();

    GLint length = 0;
    glGetProgramiv(program, GraphicsContext3D::INFO_LOG_LENGTH, &length);

    GLsizei size = 0;

    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    glGetProgramInfoLog(program, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetRenderbufferParameteriv(target, paramName, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum paramName, GC3Dint* value)
{
    ASSERT(shader);
    makeContextCurrent();
    glGetShaderiv(shader, paramName, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    makeContextCurrent();

    GLint length = 0;
    glGetShaderiv(shader, GraphicsContext3D::INFO_LOG_LENGTH, &length);

    GLsizei size = 0;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    glGetShaderInfoLog(shader, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    makeContextCurrent();

    GLint length = 0;
    glGetShaderiv(shader, GraphicsContext3D::SHADER_SOURCE_LENGTH, &length);

    GLsizei size = 0;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    glGetShaderSource(shader, length, &size, info);

    String result(info);
    fastFree(info);

    return result;
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    glGetTexParameterfv(target, paramName, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetTexParameteriv(target, paramName, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    makeContextCurrent();
    glGetUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    makeContextCurrent();
    glGetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    ASSERT(program);
    
    makeContextCurrent();
    return glGetUniformLocation(program, name.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum paramName, GC3Dfloat* value)
{
    makeContextCurrent();
    glGetVertexAttribfv(index, paramName, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum paramName, GC3Dint* value)
{
    makeContextCurrent();
    glGetVertexAttribiv(index, paramName, value);
}

GC3Dsizeiptr GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum paramName)
{
    makeContextCurrent();
    
    GLvoid* pointer = 0;
    glGetVertexAttribPointerv(index, paramName, &pointer);
    return static_cast<GC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoff, GC3Dint yoff, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
}

Platform3DObject GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    GLuint handle = 0;
    glGenBuffers(/* count */ 1, &handle);
    return handle;
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    GLuint handle = 0;
    glGenFramebuffers(/* count */ 1, &handle);
    return handle;
}

Platform3DObject GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    GLuint handle = 0;
    glGenRenderbuffers(/* count */ 1, &handle);
    return handle;
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    makeContextCurrent();
    return glCreateShader(type);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    GLuint handle = 0;
    glGenTextures(1, &handle);
    return handle;
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject framebuffer)
{
    makeContextCurrent();
    glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    makeContextCurrent();
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    m_syntheticErrors.add(error);
}

void GraphicsContext3D::markLayerComposited()
{
    m_layerComposited = true;
}

void GraphicsContext3D::markContextChanged()
{
    // FIXME: Any accelerated compositor needs to be told to re-read from here.
    m_layerComposited = false;
}

bool GraphicsContext3D::layerComposited() const
{
    return m_layerComposited;
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DQt);
    return m_extensions.get();
}
#endif

bool GraphicsContext3D::getImageData(Image* image,
                                     GC3Denum format,
                                     GC3Denum type,
                                     bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile,
                                     Vector<uint8_t>& outputVector)
{
    UNUSED_PARAM(ignoreGammaAndColorProfile);
    if (!image)
        return false;
    QImage nativeImage;
    // Is image already loaded? If not, load it.
    if (image->data())
        nativeImage = QImage::fromData(reinterpret_cast<const uchar*>(image->data()->data()), image->data()->size()).convertToFormat(QImage::Format_ARGB32);
    else {
        QPixmap* nativePixmap = image->nativeImageForCurrentFrame();
        nativeImage = nativePixmap->toImage().convertToFormat(QImage::Format_ARGB32);
    }
    AlphaOp neededAlphaOp = AlphaDoNothing;
    if (premultiplyAlpha)
        neededAlphaOp = AlphaDoPremultiply;
    outputVector.resize(nativeImage.byteCount());
    return packPixels(nativeImage.bits(), SourceFormatBGRA8, image->width(), image->height(), 0, format, type, neededAlphaOp, outputVector.data());
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
}

}

#endif // ENABLE(WEBGL)
