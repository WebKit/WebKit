/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#include "WebOverlay.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerWebKitThread.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "TextureCacheCompositingThread.h"
#include "WebAnimation.h"
#include "WebAnimation_p.h"
#include "WebOverlayClient.h"
#include "WebOverlayOverride.h"
#include "WebOverlay_p.h"
#include "WebPageCompositorClient.h"
#include "WebPageCompositor_p.h"
#include "WebPage_p.h"
#include "WebString.h"

#include <BlackBerryPlatformMessageClient.h>
#include <GLES2/gl2.h>
#include <SkDevice.h>

namespace BlackBerry {
namespace WebKit {

using namespace WebCore;

WebOverlay::WebOverlay()
    : d(0)
{
    if (Platform::webKitThreadMessageClient()->isCurrentThread()) {
        d = new WebOverlayPrivateWebKitThread;
        d->q = this;
    } else if (Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        d = new WebOverlayPrivateCompositingThread;
        d->q = this;
    }
}

WebOverlay::WebOverlay(GraphicsLayerClient* client)
    : d(0)
{
    d = new WebOverlayPrivateWebKitThread(client);
    d->q = this;
}

WebOverlay::~WebOverlay()
{
    delete d;
}

Platform::FloatPoint WebOverlay::position() const
{
    return d->position();
}

void WebOverlay::setPosition(const Platform::FloatPoint& position)
{
    d->setPosition(position);
}

Platform::FloatPoint WebOverlay::anchorPoint() const
{
    return d->anchorPoint();
}

void WebOverlay::setAnchorPoint(const Platform::FloatPoint& anchor)
{
    d->setAnchorPoint(anchor);
}

Platform::FloatSize WebOverlay::size() const
{
    return d->size();
}

void WebOverlay::setSize(const Platform::FloatSize& size)
{
    d->setSize(size);
}

bool WebOverlay::sizeIsScaleInvariant() const
{
    return d->sizeIsScaleInvariant();
}

void WebOverlay::setSizeIsScaleInvariant(bool invariant)
{
    d->setSizeIsScaleInvariant(invariant);
}

Platform::TransformationMatrix WebOverlay::transform() const
{
    // FIXME: There is no WebCore::TranformationMatrix interoperability
    // with Platform::TransformationMatrix
    TransformationMatrix transform = d->transform();
    return reinterpret_cast<const Platform::TransformationMatrix&>(transform);
}

void WebOverlay::setTransform(const Platform::TransformationMatrix& transform)
{
    d->setTransform(reinterpret_cast<const TransformationMatrix&>(transform));
}

float WebOverlay::opacity() const
{
    return d->opacity();
}

void WebOverlay::setOpacity(float opacity)
{
    d->setOpacity(opacity);
}

void WebOverlay::addAnimation(const WebAnimation& animation)
{
    d->addAnimation(animation.d->name, animation.d->animation.get(), animation.d->keyframes);
}

void WebOverlay::removeAnimation(const WebString& name)
{
    d->removeAnimation(String(PassRefPtr<StringImpl>(name.impl())));
}

WebOverlay* WebOverlay::parent() const
{
    return d->parent;
}

bool WebOverlay::addChild(WebOverlay* overlay)
{
    if (overlay->d->nativeThread != d->nativeThread)
        return false;

    overlay->d->parent = this;
    d->addChild(overlay->d);
    return true;
}

void WebOverlay::removeFromParent()
{
    d->removeFromParent();
    d->parent = 0;
}

void WebOverlay::setContentsToImage(const unsigned char* data, const Platform::IntSize& imageSize)
{
    d->setContentsToImage(data, imageSize);
}

void WebOverlay::setContentsToColor(int r, int g, int b, int a)
{
    d->setContentsToColor(Color(r, g, b, a));
}

void WebOverlay::setDrawsContent(bool drawsContent)
{
    d->setDrawsContent(drawsContent);
}

void WebOverlay::invalidate()
{
    d->invalidate();
}

void WebOverlay::setClient(WebOverlayClient* client)
{
    d->setClient(client);
}

WebOverlayOverride* WebOverlay::override()
{
    // Must be called on UI thread
    if (!Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return 0;

    return d->override();
}

void WebOverlay::resetOverrides()
{
    d->resetOverrides();
}

WebPagePrivate* WebOverlayPrivate::page() const
{
    if (m_page)
        return m_page;

    if (parent)
        return parent->d->page();

    return 0;
}

WebOverlayOverride* WebOverlayPrivate::override()
{
    if (!m_override)
        m_override = adoptPtr(new WebOverlayOverride(this));

    // Page might have changed if we were removed from the page and added to
    // some other page.
    m_override->d->setPage(page());

    return m_override.get();
}

void WebOverlayPrivate::drawContents(SkCanvas* canvas)
{
    if (!client)
        return;

    client->drawOverlayContents(q, canvas);
}

void WebOverlayPrivate::scheduleCompositingRun()
{
    if (!page())
        return;

    page()->scheduleCompositingRun();
}

WebOverlayPrivateWebKitThread::WebOverlayPrivateWebKitThread(GraphicsLayerClient* client)
    : m_layer(GraphicsLayer::create(client ? client : this))
{
    m_layerCompositingThread = m_layer->platformLayer()->layerCompositingThread();
}

FloatPoint WebOverlayPrivateWebKitThread::position() const
{
    return m_layer->position();
}

void WebOverlayPrivateWebKitThread::setPosition(const FloatPoint& position)
{
    m_layer->setPosition(position);
}

FloatPoint WebOverlayPrivateWebKitThread::anchorPoint() const
{
    FloatPoint3D anchor = m_layer->anchorPoint();
    return FloatPoint(anchor.x(), anchor.y());
}

void WebOverlayPrivateWebKitThread::setAnchorPoint(const FloatPoint& anchor)
{
    m_layer->setAnchorPoint(FloatPoint3D(anchor.x(), anchor.y(), 0));
}

FloatSize WebOverlayPrivateWebKitThread::size() const
{
    return m_layer->size();
}

void WebOverlayPrivateWebKitThread::setSize(const FloatSize& size)
{
    m_layer->setSize(size);
}

bool WebOverlayPrivateWebKitThread::sizeIsScaleInvariant() const
{
    return m_layer->platformLayer()->sizeIsScaleInvariant();
}

void WebOverlayPrivateWebKitThread::setSizeIsScaleInvariant(bool invariant)
{
    m_layer->platformLayer()->setSizeIsScaleInvariant(invariant);
}

TransformationMatrix WebOverlayPrivateWebKitThread::transform() const
{
    return m_layer->transform();
}

void WebOverlayPrivateWebKitThread::setTransform(const TransformationMatrix& transform)
{
    m_layer->setTransform(transform);
}

float WebOverlayPrivateWebKitThread::opacity() const
{
    return m_layer->opacity();
}

void WebOverlayPrivateWebKitThread::setOpacity(float opacity)
{
    m_layer->setOpacity(opacity);
}

void WebOverlayPrivateWebKitThread::addAnimation(const String& name, Animation* animation, const KeyframeValueList& keyframes)
{
    IntSize size(m_layer->size().width(), m_layer->size().height());
    m_layer->addAnimation(keyframes, size, animation, name, 0);
}

void WebOverlayPrivateWebKitThread::removeAnimation(const String& name)
{
    m_layer->removeAnimation(name);
}

void WebOverlayPrivateWebKitThread::addChild(WebOverlayPrivate* overlay)
{
    m_layer->addChild(static_cast<WebOverlayPrivateWebKitThread*>(overlay)->m_layer.get());
}

void WebOverlayPrivateWebKitThread::removeFromParent()
{
    m_layer->removeFromParent();
}

void WebOverlayPrivateWebKitThread::setContentsToImage(const unsigned char* data, const WebCore::IntSize& imageSize)
{
    notImplemented();
}

void WebOverlayPrivateWebKitThread::setContentsToColor(const Color&)
{
    notImplemented();
}

void WebOverlayPrivateWebKitThread::setDrawsContent(bool drawsContent)
{
    m_layer->setDrawsContent(drawsContent);
}

void WebOverlayPrivateWebKitThread::clear()
{
    setSize(FloatSize(0, 0));
}

void WebOverlayPrivateWebKitThread::invalidate()
{
    m_layer->setNeedsDisplay();
}

void WebOverlayPrivateWebKitThread::resetOverrides()
{
    if (Platform::webKitThreadMessageClient()->isCurrentThread())
        m_layer->platformLayer()->clearOverride();
    else if (Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        m_layerCompositingThread->clearOverride();
        scheduleCompositingRun();
    }
}

void WebOverlayPrivateWebKitThread::notifySyncRequired(const WebCore::GraphicsLayer*)
{
    if (WebPagePrivate* page = this->page())
        page->scheduleRootLayerCommit();
}

void WebOverlayPrivateWebKitThread::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext& c, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&)
{
    drawContents(c.platformContext()->canvas());
}

WebOverlayLayerCompositingThreadClient::WebOverlayLayerCompositingThreadClient()
    : m_drawsContent(false)
    , m_layerCompositingThread(0)
    , m_client(0)
{
}

void WebOverlayLayerCompositingThreadClient::setDrawsContent(bool drawsContent)
{
    m_drawsContent = drawsContent;
}

void WebOverlayLayerCompositingThreadClient::invalidate()
{
    m_texture.clear();
}

void WebOverlayLayerCompositingThreadClient::setContents(const SkBitmap& contents)
{
    m_contents = contents;
    m_color = Color();
    m_texture.clear();
}

void WebOverlayLayerCompositingThreadClient::setContentsToColor(const Color& color)
{
    m_contents = SkBitmap();
    m_color = color;
    m_texture.clear();
}

void WebOverlayLayerCompositingThreadClient::layerCompositingThreadDestroyed(WebCore::LayerCompositingThread*)
{
    delete this;
}

void WebOverlayLayerCompositingThreadClient::layerVisibilityChanged(LayerCompositingThread*, bool visible)
{
}

void WebOverlayLayerCompositingThreadClient::uploadTexturesIfNeeded(LayerCompositingThread*)
{
    if (m_contents.isNull() && !m_color.isValid() && !m_drawsContent)
        return;

    if (m_texture && m_texture->textureId())
        return;

    if (m_color.isValid()) {
        m_texture = textureCacheCompositingThread()->textureForColor(m_color);
        return;
    }

    if (m_drawsContent) {
        if (!m_client || !m_owner)
            return;

        if (m_contents.isNull()) {
            m_contents.setConfig(SkBitmap::kARGB_8888_Config, m_layerCompositingThread->bounds().width(), m_layerCompositingThread->bounds().height());
            m_contents.allocPixels();
        }

        SkDevice device(m_contents);
        SkCanvas canvas(&device);
        m_client->drawOverlayContents(m_owner, &canvas);
        canvas.flush();
    }

    m_texture = Texture::create();
    m_texture->protect(IntSize(m_contents.width(), m_contents.height()));
    IntRect bitmapRect(0, 0, m_contents.width(), m_contents.height());
    m_texture->updateContents(m_contents, bitmapRect, bitmapRect, false);
}

void WebOverlayLayerCompositingThreadClient::drawTextures(LayerCompositingThread* layer, double /*scale*/, int positionLocation, int texCoordLocation)
{
    if (!m_texture || !m_texture->textureId())
        return;

    glBindTexture(GL_TEXTURE_2D, m_texture->textureId());
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
    float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };
    glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void WebOverlayLayerCompositingThreadClient::deleteTextures(LayerCompositingThread*)
{
    m_texture.clear();
}

WebOverlayPrivateCompositingThread::WebOverlayPrivateCompositingThread(PassRefPtr<LayerCompositingThread> layerCompositingThread)
    : m_layerCompositingThreadClient(0)
{
    m_layerCompositingThread = layerCompositingThread;
}

WebOverlayPrivateCompositingThread::WebOverlayPrivateCompositingThread()
    : m_layerCompositingThreadClient(new WebOverlayLayerCompositingThreadClient)
{
    m_layerCompositingThread = LayerCompositingThread::create(LayerData::CustomLayer, m_layerCompositingThreadClient);
    m_layerCompositingThreadClient->setLayer(m_layerCompositingThread.get());
}

WebOverlayPrivateCompositingThread::~WebOverlayPrivateCompositingThread()
{
    if (m_layerCompositingThreadClient)
        m_layerCompositingThreadClient->setClient(0, 0);
}

void WebOverlayPrivateCompositingThread::setClient(WebOverlayClient* client)
{
    WebOverlayPrivate::setClient(client);
    if (m_layerCompositingThreadClient)
        m_layerCompositingThreadClient->setClient(q, client);
}

FloatPoint WebOverlayPrivateCompositingThread::position() const
{
    return m_layerCompositingThread->position();
}

void WebOverlayPrivateCompositingThread::setPosition(const FloatPoint& position)
{
    m_layerCompositingThread->setPosition(position);
    scheduleCompositingRun();
}

FloatPoint WebOverlayPrivateCompositingThread::anchorPoint() const
{
    return m_layerCompositingThread->anchorPoint();
}

void WebOverlayPrivateCompositingThread::setAnchorPoint(const FloatPoint& anchor)
{
    m_layerCompositingThread->setAnchorPoint(anchor);
    scheduleCompositingRun();
}

FloatSize WebOverlayPrivateCompositingThread::size() const
{
    IntSize bounds = m_layerCompositingThread->bounds();
    return FloatSize(bounds.width(), bounds.height());
}

void WebOverlayPrivateCompositingThread::setSize(const FloatSize& size)
{
    m_layerCompositingThread->setBounds(IntSize(size.width(), size.height()));
    scheduleCompositingRun();
}

bool WebOverlayPrivateCompositingThread::sizeIsScaleInvariant() const
{
    return m_layerCompositingThread->sizeIsScaleInvariant();
}

void WebOverlayPrivateCompositingThread::setSizeIsScaleInvariant(bool invariant)
{
    m_layerCompositingThread->setSizeIsScaleInvariant(invariant);
    scheduleCompositingRun();
}

TransformationMatrix WebOverlayPrivateCompositingThread::transform() const
{
    return m_layerCompositingThread->transform();
}

void WebOverlayPrivateCompositingThread::setTransform(const TransformationMatrix& transform)
{
    m_layerCompositingThread->setTransform(transform);
    scheduleCompositingRun();
}

float WebOverlayPrivateCompositingThread::opacity() const
{
    return m_layerCompositingThread->opacity();
}

void WebOverlayPrivateCompositingThread::setOpacity(float opacity)
{
    m_layerCompositingThread->setOpacity(opacity);
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::addAnimation(const String& name, Animation* animation, const KeyframeValueList& keyframes)
{
    IntSize boxSize = m_layerCompositingThread->bounds();
    RefPtr<LayerAnimation> layerAnimation = LayerAnimation::create(keyframes, boxSize, animation, name, 0.0);

    // FIXME: Unfortunately WebPageCompositorClient::requestAnimationFrame uses a different time coordinate system
    // than accelerated animations, so we can't use the time returned by WebPageCompositorClient::requestAnimationFrame()
    // for starttime.
    layerAnimation->setStartTime(currentTime());

    m_layerCompositingThread->addAnimation(layerAnimation.get());
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::removeAnimation(const String& name)
{
    m_layerCompositingThread->removeAnimation(name);
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::addChild(WebOverlayPrivate* overlay)
{
    m_layerCompositingThread->addSublayer(overlay->layerCompositingThread());
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::removeFromParent()
{
    if (m_layerCompositingThread->superlayer() == page()->m_compositor->compositingThreadOverlayLayer())
        page()->m_compositor->removeOverlay(m_layerCompositingThread.get());
    else
        m_layerCompositingThread->removeFromSuperlayer();
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::setContentsToImage(const unsigned char* data, const IntSize& imageSize)
{
    if (!m_layerCompositingThreadClient)
        return;

    const SkBitmap& oldContents = m_layerCompositingThreadClient->contents();
    if (!oldContents.isNull()) {
        SkAutoLockPixels lock(oldContents);
        if (data == oldContents.getPixels())
            return;
    }

    SkBitmap contents;
    contents.setConfig(SkBitmap::kARGB_8888_Config, imageSize.width(), imageSize.height());
    contents.setPixels(const_cast<unsigned char*>(data));

    m_layerCompositingThreadClient->setContents(contents);
    m_layerCompositingThread->setNeedsTexture(true);
}

void WebOverlayPrivateCompositingThread::setContentsToColor(const Color& color)
{
    if (!m_layerCompositingThreadClient)
        return;

    m_layerCompositingThreadClient->setContentsToColor(color);
    m_layerCompositingThread->setNeedsTexture(true);
}

void WebOverlayPrivateCompositingThread::setDrawsContent(bool drawsContent)
{
    if (!m_layerCompositingThreadClient)
        return;

    m_layerCompositingThreadClient->setDrawsContent(drawsContent);
    m_layerCompositingThread->setNeedsTexture(true);
}

void WebOverlayPrivateCompositingThread::clear()
{
    m_layerCompositingThread->deleteTextures();
}

void WebOverlayPrivateCompositingThread::invalidate()
{
    if (!m_layerCompositingThreadClient || !m_layerCompositingThreadClient->drawsContent())
        return;

    m_layerCompositingThreadClient->invalidate();
    scheduleCompositingRun();
}

void WebOverlayPrivateCompositingThread::resetOverrides()
{
    m_layerCompositingThread->clearOverride();
    scheduleCompositingRun();
}

}
}
#else // USE(ACCELERATED_COMPOSITING)
namespace BlackBerry {
namespace WebKit {

WebOverlay::WebOverlay()
{
}

WebOverlay::~WebOverlay()
{
}

Platform::FloatPoint WebOverlay::position() const
{
    return Platform::FloatPoint();
}

void WebOverlay::setPosition(const Platform::FloatPoint&)
{
}

Platform::FloatPoint WebOverlay::anchorPoint() const
{
    return Platform::FloatPoint();
}

void WebOverlay::setAnchorPoint(const Platform::FloatPoint&)
{
}

Platform::FloatSize WebOverlay::size() const
{
    return Platform::FloatSize();
}

void WebOverlay::setSize(const Platform::FloatSize&)
{
}

Platform::TransformationMatrix WebOverlay::transform() const
{
    return Platform::TransformationMatrix();
}

void WebOverlay::setTransform(const Platform::TransformationMatrix&)
{
}

float WebOverlay::opacity() const
{
    return 1.0f;
}

void WebOverlay::setOpacity(float)
{
}

WebOverlay* WebOverlay::parent() const
{
    return 0;
}

bool WebOverlay::addChild(WebOverlay*)
{
    return false;
}

void WebOverlay::removeFromParent()
{
}

void WebOverlay::addAnimation(const WebAnimation&)
{
}

void WebOverlay::removeAnimation(const WebString&)
{
}

void WebOverlay::setContentsToImage(const unsigned char*, const Platform::IntSize&)
{
}

void WebOverlay::setContentsToColor(int, int, int, int)
{
}

void WebOverlay::setDrawsContent(bool)
{
}

void WebOverlay::invalidate()
{
}

void WebOverlay::setClient(WebOverlayClient*)
{
}

WebOverlayOverride* WebOverlay::override()
{
}

void WebOverlay::resetOverrides()
{
}

}
}
#endif // USE(ACCELERATED_COMPOSITING)
