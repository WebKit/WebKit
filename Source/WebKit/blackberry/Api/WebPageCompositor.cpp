/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "WebPageCompositor_p.h"

#include "BackingStore_p.h"
#include "LayerWebKitThread.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformMessage.h>
#include <BlackBerryPlatformMessageClient.h>
#include <GenericTimerClient.h>
#include <ThreadTimerClient.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

WebPageCompositorPrivate::WebPageCompositorPrivate(WebPagePrivate* page)
    : m_webPage(page)
    , m_context(GLES2Context::create(page))
    , m_layerRenderer(LayerRenderer::create(m_context.get()))
    , m_backingStoreUsesOpenGL(false)
    , m_animationTimer(this, &WebPageCompositorPrivate::animationTimerFired)
    , m_timerClient(new Platform::GenericTimerClient(Platform::userInterfaceThreadTimerClient()))
{
    m_animationTimer.setClient(m_timerClient);
}

WebPageCompositorPrivate::~WebPageCompositorPrivate()
{
    m_animationTimer.stop();
    delete m_timerClient;
}

bool WebPageCompositorPrivate::hardwareCompositing() const
{
    return m_layerRenderer->hardwareCompositing();
}

void WebPageCompositorPrivate::setRootLayer(LayerCompositingThread* rootLayer)
{
    m_rootLayer = rootLayer;
    m_layerRenderer->setRootLayer(m_rootLayer.get());
}

void WebPageCompositorPrivate::setBackingStoreUsesOpenGL(bool backingStoreUsesOpenGL)
{
    m_backingStoreUsesOpenGL = backingStoreUsesOpenGL;
    m_layerRenderer->setClearSurfaceOnDrawLayers(!backingStoreUsesOpenGL);
}

void WebPageCompositorPrivate::commit(LayerWebKitThread* rootLayer)
{
    if (!rootLayer)
        return;

    rootLayer->commitOnCompositingThread();
}

bool WebPageCompositorPrivate::drawLayers(const IntRect& dstRect, const FloatRect& contents)
{
    m_layerRenderer->drawLayers(contents, m_layoutRectForCompositing, m_contentsSizeForCompositing, dstRect);
    m_lastCompositingResults = m_layerRenderer->lastRenderingResults();

    if (m_lastCompositingResults.needsAnimationFrame) {
        // Using a timeout of 0 actually won't start a timer, it will send a message.
        m_animationTimer.start(1.0 / 60.0);
        m_webPage->updateDelegatedOverlays();
    }

    return true;
}

void WebPageCompositorPrivate::releaseLayerResources()
{
    m_layerRenderer->releaseLayerResources();
}

void WebPageCompositorPrivate::animationTimerFired()
{
    if (m_webPage->m_backingStore->d->shouldDirectRenderingToWindow()) {
        if (m_webPage->m_backingStore->d->isDirectRenderingAnimationMessageScheduled())
            return; // don't send new messages as long as we haven't rerendered

        m_webPage->m_backingStore->d->setDirectRenderingAnimationMessageScheduled();
        BlackBerry::Platform::webKitThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &BackingStorePrivate::renderVisibleContents, m_webPage->m_backingStore->d));
        return;
    }
    m_webPage->blitVisibleContents();
}

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)
