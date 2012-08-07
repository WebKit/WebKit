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

#if USE(OPENGL_ES_2)
#include "Extensions3DOpenGLES.h"
#else
#include "Extensions3DOpenGL.h"
#endif
#include "GraphicsContext.h"
#include "GraphicsSurface.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "NativeImageQt.h"
#include "NotImplemented.h"
#include "OpenGLShims.h"
#include "QWebPageClient.h"
#include "SharedBuffer.h"
#include <QWindow>
#include <qpa/qplatformpixmap.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if USE(TEXTURE_MAPPER_GL)
#include <texmap/TextureMapperGL.h>
#endif

#if USE(3D_GRAPHICS)

namespace WebCore {

#if !defined(GLchar)
typedef char GLchar;
#endif

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

class GraphicsContext3DPrivate
#if USE(ACCELERATED_COMPOSITING)
        : public TextureMapperPlatformLayer
#endif
{
public:
    GraphicsContext3DPrivate(GraphicsContext3D*, HostWindow*);
    ~GraphicsContext3DPrivate();

#if USE(ACCELERATED_COMPOSITING)
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect& target, const TransformationMatrix&, float opacity, BitmapTexture* mask);
#endif
#if USE(GRAPHICS_SURFACE)
    virtual uint32_t copyToGraphicsSurface();
#endif

    QRectF boundingRect() const;
    void blitMultisampleFramebuffer() const;
    void blitMultisampleFramebufferAndRestoreContext() const;
    bool makeCurrentIfNeeded() const;
    void createGraphicsSurfaces(const IntSize&);

    GraphicsContext3D* m_context;
    HostWindow* m_hostWindow;
    PlatformGraphicsSurface3D m_surface;
    PlatformGraphicsContext3D m_platformContext;
#if USE(GRAPHICS_SURFACE)
    GraphicsSurface::Flags m_surfaceFlags;
    RefPtr<GraphicsSurface> m_frontBufferGraphicsSurface;
    RefPtr<GraphicsSurface> m_backBufferGraphicsSurface;
#endif
};

bool GraphicsContext3D::isGLES2Compliant() const
{
#if USE(OPENGL_ES_2)
    return true;
#else
    return false;
#endif
}

#if !USE(OPENGL_ES_2)
void GraphicsContext3D::releaseShaderCompiler()
{
    notImplemented();
}
#endif

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* hostWindow)
    : m_context(context)
    , m_hostWindow(hostWindow)
    , m_surface(0)
    , m_platformContext(0)
{
    if (m_hostWindow && m_hostWindow->platformPageClient()) {
        // This is the WebKit1 code path.
        QWebPageClient* webPageClient = m_hostWindow->platformPageClient();
        webPageClient->createPlatformGraphicsContext3D(&m_platformContext, &m_surface);
        if (!m_surface)
            return;

        makeCurrentIfNeeded();
        return;
    }

#if USE(GRAPHICS_SURFACE)
    // FIXME: Find a way to create a QOpenGLContext without creating a QWindow at all.
    // We need to create a surface in order to create a QOpenGLContext and make it current.
    QWindow* window = new QWindow;
    window->setSurfaceType(QSurface::OpenGLSurface);
    window->setGeometry(-10, -10, 1, 1);
    window->create();
    m_surface = window;

    m_platformContext = new QOpenGLContext(window);
    if (!m_platformContext->create())
        return;

    makeCurrentIfNeeded();
    IntSize surfaceSize(m_context->m_currentWidth, m_context->m_currentHeight);
    m_surfaceFlags = GraphicsSurface::SupportsTextureTarget
                    | GraphicsSurface::SupportsSharing;

    if (!surfaceSize.isEmpty()) {
        m_frontBufferGraphicsSurface = GraphicsSurface::create(surfaceSize, m_surfaceFlags);
        m_backBufferGraphicsSurface = GraphicsSurface::create(surfaceSize, m_surfaceFlags);
    }
#endif
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
    delete m_surface;
    m_surface = 0;
    // Platform context is assumed to be owned by surface.
    m_platformContext = 0;
}

static inline quint32 swapBgrToRgb(quint32 pixel)
{
    return ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff) | (pixel & 0xff00ff00);
}

#if USE(ACCELERATED_COMPOSITING)
void GraphicsContext3DPrivate::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, BitmapTexture* mask)
{
    blitMultisampleFramebufferAndRestoreContext();

    if (textureMapper->accelerationMode() == TextureMapper::OpenGLMode) {
        TextureMapperGL* texmapGL = static_cast<TextureMapperGL*>(textureMapper);
        TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context->m_attrs.alpha ? TextureMapperGL::SupportsBlending : 0);
        IntSize textureSize(m_context->m_currentWidth, m_context->m_currentHeight);
        texmapGL->drawTexture(m_context->m_texture, flags, textureSize, targetRect, matrix, opacity, mask);
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

#if USE(GRAPHICS_SURFACE)
uint32_t GraphicsContext3DPrivate::copyToGraphicsSurface()
{
    if (!m_frontBufferGraphicsSurface || !m_backBufferGraphicsSurface)
        return 0;

    blitMultisampleFramebufferAndRestoreContext();
    makeCurrentIfNeeded();
    m_backBufferGraphicsSurface->copyFromFramebuffer(m_context->m_fbo, IntRect(0, 0, m_context->m_currentWidth, m_context->m_currentHeight));
    std::swap(m_frontBufferGraphicsSurface, m_backBufferGraphicsSurface);
    return m_frontBufferGraphicsSurface->exportToken();
}
#endif

QRectF GraphicsContext3DPrivate::boundingRect() const
{
    return QRectF(QPointF(0, 0), QSizeF(m_context->m_currentWidth, m_context->m_currentHeight));
}

void GraphicsContext3DPrivate::blitMultisampleFramebuffer() const
{
    if (!m_context->m_attrs.antialias)
        return;
#if !USE(OPENGL_ES_2)
    glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, m_context->m_multisampleFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, m_context->m_fbo);
    glBlitFramebuffer(0, 0, m_context->m_currentWidth, m_context->m_currentHeight, 0, 0, m_context->m_currentWidth, m_context->m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
#endif
    glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context->m_boundFBO);
}

void GraphicsContext3DPrivate::blitMultisampleFramebufferAndRestoreContext() const
{
    if (!m_context->m_attrs.antialias)
        return;

    const QOpenGLContext* currentContext = QOpenGLContext::currentContext();
    QSurface* currentSurface = 0;
    if (currentContext != m_platformContext) {
        currentSurface = currentContext->surface();
        m_platformContext->makeCurrent(m_surface);
    }
    blitMultisampleFramebuffer();
    if (currentSurface)
        const_cast<QOpenGLContext*>(currentContext)->makeCurrent(currentSurface);
}

bool GraphicsContext3DPrivate::makeCurrentIfNeeded() const
{
    const QOpenGLContext* currentContext = QOpenGLContext::currentContext();
    if (currentContext == m_platformContext)
        return true;
    return m_platformContext->makeCurrent(m_surface);
}

void GraphicsContext3DPrivate::createGraphicsSurfaces(const IntSize& size)
{
#if USE(GRAPHICS_SURFACE)
    if (size.isEmpty()) {
        m_frontBufferGraphicsSurface.clear();
        m_backBufferGraphicsSurface.clear();
    } else {
        m_frontBufferGraphicsSurface = GraphicsSurface::create(size, m_surfaceFlags);
        m_backBufferGraphicsSurface = GraphicsSurface::create(size, m_surfaceFlags);
    }
#endif
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
    , m_compiler(isGLES2Compliant() ? SH_ESSL_OUTPUT : SH_GLSL_OUTPUT)
    , m_attrs(attrs)
    , m_texture(0)
    , m_compositorTexture(0)
    , m_fbo(0)
#if USE(OPENGL_ES_2)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
#endif
    , m_depthStencilBuffer(0)
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
    validateAttributes();

    if (!m_private->m_surface) {
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
#if USE(OPENGL_ES_2)
        if (m_attrs.depth)
            glGenRenderbuffers(1, &m_depthBuffer);
        if (m_attrs.stencil)
            glGenRenderbuffers(1, &m_stencilBuffer);
#endif
        if (m_attrs.stencil || m_attrs.depth)
            glGenRenderbuffers(1, &m_depthStencilBuffer);
    }

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

#if !USE(OPENGL_ES_2)
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    glClearColor(0.0, 0.0, 0.0, 0.0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    // If GraphicsContext3D init failed in constructor, m_private set to nullptr and no buffers are allocated.
    if (!m_private)
        return;

    makeContextCurrent();
    glDeleteTextures(1, &m_texture);
    glDeleteFramebuffers(1, &m_fbo);
    if (m_attrs.antialias) {
        glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        glDeleteFramebuffers(1, &m_multisampleFBO);
        if (m_attrs.stencil || m_attrs.depth)
            glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else if (m_attrs.stencil || m_attrs.depth) {
#if USE(OPENGL_ES_2)
        if (m_attrs.depth)
            glDeleteRenderbuffers(1, &m_depthBuffer);
        if (m_attrs.stencil)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#endif
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D()
{
    return m_private->m_platformContext;
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
    if (!m_private)
        return false;
    return m_private->makeCurrentIfNeeded();
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight,
                                      int canvasWidth, int canvasHeight, QPainter* context)
{
    QImage image(imagePixels, imageWidth, imageHeight, NativeImageQt::defaultFormatForAlphaEnabledImages());
    context->save();
    context->translate(0, imageHeight);
    context->scale(1, -1);
    context->setCompositionMode(QPainter::CompositionMode_Source);
    context->drawImage(QRect(0, 0, canvasWidth, -canvasHeight), image);
    context->restore();
}

#if USE(GRAPHICS_SURFACE)
void GraphicsContext3D::createGraphicsSurfaces(const IntSize& size)
{
    m_private->createGraphicsSurfaces(size);
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
        nativeImage = QImage::fromData(reinterpret_cast<const uchar*>(image->data()->data()), image->data()->size());
    else
        nativeImage = *image->nativeImageForCurrentFrame();


    AlphaOp alphaOp = AlphaDoNothing;
    switch (nativeImage.format()) {
    case QImage::Format_RGB32:
        // For opaque images, we should not premultiply or unmultiply alpha.
        break;
    case QImage::Format_ARGB32:
        if (premultiplyAlpha)
            alphaOp = AlphaDoPremultiply;
        break;
    case QImage::Format_ARGB32_Premultiplied:
        if (!premultiplyAlpha)
            alphaOp = AlphaDoUnmultiply;
        break;
    default:
        // The image has a format that is not supported in packPixels. We have to convert it here.
        nativeImage = nativeImage.convertToFormat(premultiplyAlpha ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32);
        break;
    }

    unsigned int packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, image->width(), image->height(), 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;

    outputVector.resize(packedSize);

    return packPixels(nativeImage.constBits(), SourceFormatBGRA8, image->width(), image->height(), 0, format, type, alphaOp, outputVector.data());
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
}

}

#endif // USE(3D_GRAPHICS)
