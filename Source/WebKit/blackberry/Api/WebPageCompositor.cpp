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

#include "WebPageCompositor.h"

#if USE(ACCELERATED_COMPOSITING)
#include "WebPageCompositorClient.h"
#include "WebPageCompositor_p.h"

#include "BackingStore_p.h"
#include "LayerWebKitThread.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformDebugMacros.h>
#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformMessage.h>
#include <BlackBerryPlatformMessageClient.h>
#include <GenericTimerClient.h>
#include <ThreadTimerClient.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

WebPageCompositorPrivate::WebPageCompositorPrivate(WebPagePrivate* page, WebPageCompositorClient* client)
    : m_client(client)
    , m_webPage(page)
    , m_drawsRootLayer(false)
{
    setOneShot(true); // one-shot animation client
}

WebPageCompositorPrivate::~WebPageCompositorPrivate()
{
    Platform::AnimationFrameRateController::instance()->removeClient(this);
}

void WebPageCompositorPrivate::setContext(Platform::Graphics::GLES2Context* context)
{
    if (m_context == context)
        return;

    m_context = context;
    if (!m_context) {
        m_layerRenderer.clear();
        return;
    }

    m_layerRenderer = LayerRenderer::create(m_context);
    m_layerRenderer->setRootLayer(m_rootLayer.get());
}

bool WebPageCompositorPrivate::hardwareCompositing() const
{
    return m_layerRenderer && m_layerRenderer->hardwareCompositing();
}

void WebPageCompositorPrivate::setRootLayer(LayerCompositingThread* rootLayer)
{
    m_rootLayer = rootLayer;

    if (m_layerRenderer)
        m_layerRenderer->setRootLayer(m_rootLayer.get());
}

void WebPageCompositorPrivate::commit(LayerWebKitThread* rootLayer)
{
    if (!rootLayer)
        return;

    rootLayer->commitOnCompositingThread();
}

void WebPageCompositorPrivate::render(const IntRect& dstRect, const IntRect& transformedContents)
{
    // It's not safe to call into the BackingStore if the compositor hasn't been set yet.
    // For thread safety, we have to do it using a round-trip to the WebKit thread, so the
    // embedder might call this before the round-trip to WebPagePrivate::setCompositor() is
    // done.
    if (m_webPage->compositor() != this)
        return;

    // The BackingStore is the root layer
    if (BackingStore* backingStore = m_webPage->m_backingStore)
        backingStore->d->blitContents(dstRect, transformedContents, true);
    else {
        FloatRect contents = m_webPage->mapFromTransformedFloatRect(FloatRect(transformedContents));
        drawLayers(dstRect, contents);
    }
}

bool WebPageCompositorPrivate::drawsRootLayer() const
{
    return m_drawsRootLayer;
}

bool WebPageCompositorPrivate::drawLayers(const IntRect& dstRect, const FloatRect& contents)
{
    if (!m_layerRenderer)
        return false;

    bool shouldClear = drawsRootLayer();
    if (BackingStore* backingStore = m_webPage->m_backingStore)
        shouldClear = shouldClear || !backingStore->d->isOpenGLCompositing();
    m_layerRenderer->setClearSurfaceOnDrawLayers(shouldClear);

    m_layerRenderer->drawLayers(contents, m_layoutRectForCompositing, m_contentsSizeForCompositing, dstRect);
    m_lastCompositingResults = m_layerRenderer->lastRenderingResults();

    if (m_lastCompositingResults.needsAnimationFrame) {
        Platform::AnimationFrameRateController::instance()->addClient(this);
        m_webPage->updateDelegatedOverlays();
    }

    return true;
}

void WebPageCompositorPrivate::releaseLayerResources()
{
    if (m_layerRenderer)
        m_layerRenderer->releaseLayerResources();
}

void WebPageCompositorPrivate::animationFrameChanged()
{
    BackingStore* backingStore = m_webPage->m_backingStore;
    if (!backingStore) {
        drawLayers(m_webPage->client()->userInterfaceBlittedDestinationRect(),
                   IntRect(m_webPage->client()->userInterfaceBlittedVisibleContentsRect()));
        return;
    }

    if (backingStore->d->shouldDirectRenderingToWindow()) {
        if (backingStore->d->isDirectRenderingAnimationMessageScheduled())
            return; // don't send new messages as long as we haven't rerendered

        using namespace BlackBerry::Platform;

        backingStore->d->setDirectRenderingAnimationMessageScheduled();
        webKitThreadMessageClient()->dispatchMessage(createMethodCallMessage(&BackingStorePrivate::renderVisibleContents, backingStore->d));
        return;
    }
    m_webPage->blitVisibleContents();
}

void WebPageCompositorPrivate::compositorDestroyed()
{
    if (m_client)
        m_client->compositorDestroyed();

    m_client = 0;
}

WebPageCompositor::WebPageCompositor(WebPage* page, WebPageCompositorClient* client)
{
    using namespace BlackBerry::Platform;

    RefPtr<WebPageCompositorPrivate> tmp = WebPageCompositorPrivate::create(page->d, client);

    // Keep one ref ourselves...
    d = tmp.get();
    d->ref();

    // ...And pass one ref to the web page.
    // This ensures that the compositor will be around for as long as it's
    // needed. Unfortunately RefPtr<T> is not public, so we have declare to
    // resort to manual refcounting.
    webKitThreadMessageClient()->dispatchMessage(createMethodCallMessage(&WebPagePrivate::setCompositor, d->page(), tmp));
}

WebPageCompositor::~WebPageCompositor()
{
    using namespace BlackBerry::Platform;

    webKitThreadMessageClient()->dispatchMessage(createMethodCallMessage(&WebPagePrivate::setCompositor, d->page(), PassRefPtr<WebPageCompositorPrivate>(0)));
    d->compositorDestroyed();
    d->deref();
}

WebPageCompositorClient* WebPageCompositor::client() const
{
    return 0;
}

void WebPageCompositor::prepareFrame(Platform::Graphics::GLES2Context* context, double timestamp)
{
    d->setContext(context);
}

void WebPageCompositor::render(Platform::Graphics::GLES2Context* context, const Platform::IntRect& dstRect, const Platform::IntRect& contents)
{
    d->setContext(context);
    d->render(dstRect, contents);
}

void WebPageCompositor::cleanup(Platform::Graphics::GLES2Context* context)
{
    d->setContext(0);
}

void WebPageCompositor::contextLost()
{
    // This method needs to delete the compositor in a way that not tries to
    // use any OpenGL calls (the context is gone, so we can't and don't need to
    // free any OpenGL resources.
    notImplemented();
}

} // namespace WebKit
} // namespace BlackBerry

#else // USE(ACCELERATED_COMPOSITING)

namespace BlackBerry {
namespace WebKit {

WebPageCompositor::WebPageCompositor(WebPage*, WebPageCompositorClient*)
    : d(0)
{
}

WebPageCompositor::~WebPageCompositor()
{
}

WebPageCompositorClient* WebPageCompositor::client() const
{
    return 0;
}

void WebPageCompositor::prepareFrame(Platform::Graphics::GLES2Context*, double)
{
}

void WebPageCompositor::render(Platform::Graphics::GLES2Context*, const Platform::IntRect&, const Platform::IntRect&)
{
}

void WebPageCompositor::cleanup(Platform::Graphics::GLES2Context*)
{
}

void WebPageCompositor::contextLost()
{
}

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)
