/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation.

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
#include "GraphicsContext3DPrivate.h"

#if USE(3D_GRAPHICS) || USE(ACCELERATED_COMPOSITING)

#include "HostWindow.h"
#include "NotImplemented.h"

namespace WebCore {

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
    : m_context(context)
    , m_hostWindow(hostWindow)
    , m_pendingSurfaceResize(false)
{
    if (m_hostWindow && m_hostWindow->platformPageClient()) {
        // FIXME: Implement this code path for WebKit1.
        // Get Evas object from platformPageClient and set EvasGL related members.
        return;
    }

    m_platformContext = GLPlatformContext::createContext(renderStyle);
    if (!m_platformContext)
        return;

    if (renderStyle == GraphicsContext3D::RenderOffscreen) {
#if USE(GRAPHICS_SURFACE)
        m_platformSurface = GLPlatformSurface::createTransportSurface();
#else
        m_platformSurface = GLPlatformSurface::createOffscreenSurface();
#endif
        if (!m_platformSurface) {
            m_platformContext = nullptr;
            return;
        }

        if (!m_platformContext->initialize(m_platformSurface.get()) || !m_platformContext->makeCurrent(m_platformSurface.get())) {
            releaseResources();
            m_platformContext = nullptr;
            m_platformSurface = nullptr;
#if USE(GRAPHICS_SURFACE)
        } else
            m_surfaceHandle = GraphicsSurfaceToken(m_platformSurface->handle());
#else
        }
#endif
    }
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
}

void GraphicsContext3DPrivate::releaseResources()
{
    // Release the current context and drawable only after destroying any associated gl resources.
    if (m_platformSurface)
        m_platformSurface->destroy();

    if (m_platformContext) {
        m_platformContext->destroy();
        m_platformContext->releaseCurrent();
    }
}

bool GraphicsContext3DPrivate::createSurface(PageClientEfl*, bool)
{
    notImplemented();
    return false;
}

void GraphicsContext3DPrivate::setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback> callBack)
{
    m_contextLostCallback = callBack;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformGraphicsContext3D() const
{
    return m_platformContext->handle();
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    bool success = m_platformContext->makeCurrent(m_platformSurface.get());

    if (!m_platformContext->isValid()) {
        // FIXME: Restore context
        if (m_contextLostCallback)
            m_contextLostCallback->onContextLost();

        success = false;
    }

    return success;
}

#if USE(TEXTURE_MAPPER_GL)
void GraphicsContext3DPrivate::paintToTextureMapper(TextureMapper*, const FloatRect& /* target */, const TransformationMatrix&, float /* opacity */, BitmapTexture* /* mask */)
{
    notImplemented();
}
#endif

#if USE(GRAPHICS_SURFACE)
void GraphicsContext3DPrivate::didResizeCanvas(const IntSize& size)
{
    m_pendingSurfaceResize = true;
    m_size = size;
}

uint32_t GraphicsContext3DPrivate::copyToGraphicsSurface()
{
    if (!m_platformContext || !makeContextCurrent())
        return 0;

    m_context->markLayerComposited();

    if (m_pendingSurfaceResize) {
        m_pendingSurfaceResize = false;
        m_platformSurface->setGeometry(IntRect(0, 0, m_context->m_currentWidth, m_context->m_currentHeight));
    }

    if (m_context->m_attrs.antialias) {
        bool enableScissorTest = false;
        int width = m_context->m_currentWidth;
        int height = m_context->m_currentHeight;
        // We should copy the full buffer, and not respect the current scissor bounds.
        // FIXME: It would be more efficient to track the state of the scissor test.
        if (m_context->isEnabled(GraphicsContext3D::SCISSOR_TEST)) {
            enableScissorTest = true;
            m_context->disable(GraphicsContext3D::SCISSOR_TEST);
        }

        glBindFramebuffer(Extensions3D::READ_FRAMEBUFFER, m_context->m_multisampleFBO);
        glBindFramebuffer(Extensions3D::DRAW_FRAMEBUFFER, m_context->m_fbo);

        // Use NEAREST as no scale is performed during the blit.
        m_context->getExtensions()->blitFramebuffer(0, 0, width, height, 0, 0, width, height, GraphicsContext3D::COLOR_BUFFER_BIT, GraphicsContext3D::NEAREST);

        if (enableScissorTest)
            m_context->enable(GraphicsContext3D::SCISSOR_TEST);
    }

    m_platformSurface->updateContents(m_context->m_texture, m_context->m_boundFBO, m_context->m_boundTexture0);

    return 0;
}

GraphicsSurfaceToken GraphicsContext3DPrivate::graphicsSurfaceToken() const
{
    return m_surfaceHandle;
}

IntSize GraphicsContext3DPrivate::platformLayerSize() const
{
    return m_size;
}

#endif

} // namespace WebCore

#endif // USE(3D_GRAPHICS) || USE(ACCELERATED_COMPOSITING)
