/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerCompositingThread.h"

#include "LayerMessage.h"
#include "LayerRenderer.h"
#include "LayerWebKitThread.h"
#if ENABLE(VIDEO)
#include "MediaPlayer.h"
#include "MediaPlayerPrivateBlackBerry.h"
#endif
#include "PluginView.h"
#include "TextureCacheCompositingThread.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>
#include <wtf/Assertions.h>

#define DEBUG_VIDEO_CLIPPING 0

namespace WebCore {

PassRefPtr<LayerCompositingThread> LayerCompositingThread::create(LayerType type, PassRefPtr<LayerTiler> tiler)
{
    return adoptRef(new LayerCompositingThread(type, tiler));
}

LayerCompositingThread::LayerCompositingThread(LayerType type, PassRefPtr<LayerTiler> tiler)
    : LayerData(type)
    , m_layerRenderer(0)
    , m_superlayer(0)
    , m_pluginBuffer(0)
    , m_drawOpacity(0)
    , m_visible(false)
    , m_commitScheduled(false)
    , m_tiler(tiler)
{
}

LayerCompositingThread::~LayerCompositingThread()
{
    ASSERT(isCompositingThread());

    m_tiler->layerCompositingThreadDestroyed();

    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    while (m_sublayers.size())
        m_sublayers[0]->removeFromSuperlayer();

    // Delete all allocated textures
    deleteTextures();

    // We just deleted all our textures, no need for the
    // layer renderer to track us anymore
    if (m_layerRenderer)
        m_layerRenderer->removeLayer(this);
}

void LayerCompositingThread::setLayerRenderer(LayerRenderer* renderer)
{
    // It's not expected that layers will ever switch renderers.
    ASSERT(!renderer || !m_layerRenderer || renderer == m_layerRenderer);

    m_layerRenderer = renderer;
    if (m_layerRenderer)
        m_layerRenderer->addLayer(this);
}

void LayerCompositingThread::deleteTextures()
{
    releaseTextureResources();

    m_tiler->deleteTextures();
}

void LayerCompositingThread::setDrawTransform(const TransformationMatrix& matrix)
{
    m_drawTransform = matrix;

    float bx = m_bounds.width() / 2.0;
    float by = m_bounds.height() / 2.0;
    m_transformedBounds.setP1(matrix.mapPoint(FloatPoint(-bx, -by)));
    m_transformedBounds.setP2(matrix.mapPoint(FloatPoint(-bx, by)));
    m_transformedBounds.setP3(matrix.mapPoint(FloatPoint(bx, by)));
    m_transformedBounds.setP4(matrix.mapPoint(FloatPoint(bx, -by)));

    m_drawRect = m_transformedBounds.boundingBox();
}

static FloatQuad getTransformedRect(const IntSize& bounds, const IntRect& rect, const TransformationMatrix& drawTransform)
{
    float x = -bounds.width() / 2.0 + rect.x();
    float y = -bounds.height() / 2.0 + rect.y();
    float w = rect.width();
    float h = rect.height();
    FloatQuad result;
    result.setP1(drawTransform.mapPoint(FloatPoint(x, y)));
    result.setP2(drawTransform.mapPoint(FloatPoint(x, y + h)));
    result.setP3(drawTransform.mapPoint(FloatPoint(x + w, y + h)));
    result.setP4(drawTransform.mapPoint(FloatPoint(x + w, y)));

    return result;
}


FloatQuad LayerCompositingThread::getTransformedHolePunchRect() const
{
    // FIXME: the following line disables clipping a video in an iframe i.e. the fix associated with PR 99638.
    // Some revised test case (e.g. video-iframe.html) show that the original fix works correctly when scrolling
    // the contents of the frame, but fails to clip correctly if the page (main frame) is scrolled.
    static bool enableVideoClipping = false;

    if (!mediaPlayer() || !enableVideoClipping) {
        // m_holePunchClipRect is valid only when there's a media player.
        return getTransformedRect(m_bounds, m_holePunchRect, m_drawTransform);
    }

    // The hole punch rectangle may need to be clipped,
    // e.g. if the <video> is on a layer that's included and clipped by an <iframe>.

    // In order to clip we need to determine the current position of this layer, which
    // is encoded in the m_drawTransform value, which was used to initialize m_drawRect.
    IntRect drawRect = m_layerRenderer->toWebKitDocumentCoordinates(m_drawRect);

    // Assert that in this case, where the hole punch rectangle equals the size of the layer,
    // the drawRect has the same size as the hole punch.
    // ASSERT(drawRect.size() == m_holePunchRect.size());
    // Don't assert it programtically though because there may be off-by-one error due to rounding when there's zooming.

    // The difference between drawRect and m_holePunchRect is that drawRect has an accurate position
    // in WebKit document coordinates, whereas the m_holePunchRect location is (0,0) i.e. it's relative to this layer.

    // Clip the drawRect.
    // Both drawRect and m_holePunchClipRect already have correct locations, in WebKit document coordinates.
    IntPoint location = drawRect.location();
    drawRect.intersect(m_holePunchClipRect);

    // Shift the clipped drawRect to have the same kind of located-at-zero position as the original holePunchRect.
    drawRect.move(-location.x(), -location.y());

#if DEBUG_VIDEO_CLIPPING
     IntRect drawRectInWebKitDocumentCoordination = m_layerRenderer->toWebKitDocumentCoordinates(m_drawRect);
     BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "LayerCompositingThread::getTransformedHolePunchRect() - drawRect=(x=%d,y=%d,width=%d,height=%d) clipRect=(x=%d,y=%d,width=%d,height=%d) clippedRect=(x=%d,y=%d,width=%d,height=%d).",
        drawRectInWebKitDocumentCoordination.x(), drawRectInWebKitDocumentCoordination.y(), drawRectInWebKitDocumentCoordination.width(), drawRectInWebKitDocumentCoordination.height(),
        m_holePunchClipRect.x(), m_holePunchClipRect.y(), m_holePunchClipRect.width(), m_holePunchClipRect.height(),
        drawRect.x(), drawRect.y(), drawRect.width(), drawRect.height());
#endif

    return getTransformedRect(m_bounds, drawRect, m_drawTransform);
}

void LayerCompositingThread::drawTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect)
{
    float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };

    if (m_pluginView) {
        if (m_isVisible) {
            // The layer contains Flash, video, or other plugin contents.
            m_pluginBuffer = m_pluginView->lockFrontBufferForRead();

            if (!m_pluginBuffer)
                return;

            if (!BlackBerry::Platform::Graphics::lockAndBindBufferGLTexture(m_pluginBuffer, GL_TEXTURE_2D)) {
                m_pluginView->unlockFrontBuffer();
                return;
            }

            m_layerRenderer->addLayerToReleaseTextureResourcesList(this);

            glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &m_transformedBounds);
            glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        return;
    }
#if ENABLE(VIDEO)
    if (m_mediaPlayer) {
        if (m_isVisible) {
            // We need to specify the media player location in contents coordinates. The 'visibleRect'
            // specifies the content region covered by our viewport. So we transform from our
            // normalized device coordinates [-1, 1] to the 'visibleRect'.
            float vrw2 = visibleRect.width() / 2.0;
            float vrh2 = visibleRect.height() / 2.0;
            float x = m_transformedBounds.p1().x() * vrw2 + vrw2 + visibleRect.x();
            float y = -m_transformedBounds.p1().y() * vrh2 + vrh2 + visibleRect.y();
            m_mediaPlayer->paint(0, IntRect((int)(x + 0.5), (int)(y + 0.5), m_bounds.width(), m_bounds.height()));
            MediaPlayerPrivate* mpp = static_cast<MediaPlayerPrivate*>(m_mediaPlayer->platformMedia().media.qnxMediaPlayer);
            mpp->drawBufferingAnimation(m_drawTransform, positionLocation, texCoordLocation);
        }
        return;
    }
#endif
#if ENABLE(WEBGL)
    if (layerType() == LayerData::WebGLLayer) {
        m_layerRenderer->addLayerToReleaseTextureResourcesList(this);
        pthread_mutex_lock(m_frontBufferLock);
        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &m_transformedBounds);
        float canvasWidthRatio = 1.0f;
        float canvasHeightRatio = 1.0f;
        float upsideDown[4 * 2] = { 0, 1,  0, 1 - canvasHeightRatio,  canvasWidthRatio, 1 - canvasHeightRatio,  canvasWidthRatio, 1 };
        // Flip the texture Y axis because OpenGL and Skia have different origins
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, upsideDown);
        glBindTexture(GL_TEXTURE_2D, m_texID);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        // FIXME: If the canvas/texture is larger than 2048x2048, then we'll die here
        return;
    }
#endif
    if (m_texID) {
        m_layerRenderer->addLayerToReleaseTextureResourcesList(this);
        pthread_mutex_lock(m_frontBufferLock);

        glDisable(GL_SCISSOR_TEST);
        glBindTexture(GL_TEXTURE_2D, m_texID);
        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &m_transformedBounds);
        float upsideDown[4 * 2] = { 0, 1,  0, 0,  1, 0,  1, 1 };
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, upsideDown);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        return;
    }

    m_tiler->drawTextures(this, positionLocation, texCoordLocation);
}

void LayerCompositingThread::drawSurface(const TransformationMatrix& drawTransform, LayerCompositingThread* mask, int positionLocation, int texCoordLocation)
{
    if (m_layerRenderer->layerAlreadyOnSurface(this)) {
        unsigned texID = layerRendererSurface()->texture()->textureId();
        if (!texID) {
            ASSERT_NOT_REACHED();
            return;
        }
        textureCacheCompositingThread()->textureAccessed(layerRendererSurface()->texture());
        glBindTexture(GL_TEXTURE_2D, texID);

        if (mask) {
            glActiveTexture(GL_TEXTURE1);
            mask->bindContentsTexture();
            glActiveTexture(GL_TEXTURE0);
        }

        FloatQuad surfaceQuad = getTransformedRect(m_bounds, IntRect(IntPoint::zero(), m_bounds), drawTransform);
        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &surfaceQuad);

        float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

void LayerCompositingThread::drawMissingTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect)
{
    if (m_pluginView || m_texID)
        return;

#if ENABLE(VIDEO)
    if (m_mediaPlayer)
        return;
#endif

    m_tiler->drawMissingTextures(this, positionLocation, texCoordLocation);
}

void LayerCompositingThread::releaseTextureResources()
{
    if (m_pluginView && m_pluginBuffer) {
        BlackBerry::Platform::Graphics::releaseBufferGLTexture(m_pluginBuffer);
        m_pluginBuffer = 0;
        m_pluginView->unlockFrontBuffer();
    }
    if (m_texID && m_frontBufferLock)
        pthread_mutex_unlock(m_frontBufferLock);
}

void LayerCompositingThread::setPluginView(PluginView* pluginView)
{
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(BlackBerry::Platform::createMethodCallMessage(
            &LayerCompositingThread::setPluginView,
            this,
            pluginView));
        return;
    }

    m_pluginView = pluginView;
}

#if ENABLE(VIDEO)
void LayerCompositingThread::setMediaPlayer(MediaPlayer* mediaPlayer)
{
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(BlackBerry::Platform::createMethodCallMessage(
            &LayerCompositingThread::setMediaPlayer,
            this,
            mediaPlayer));
        return;
    }

    m_mediaPlayer = mediaPlayer;
}
#endif

void LayerCompositingThread::clearAnimations()
{
    // Animations don't use thread safe refcounting, and must only be
    // touched when the two threads are in sync.
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(BlackBerry::Platform::createMethodCallMessage(
            &LayerCompositingThread::clearAnimations,
            this));
        return;
    }

    m_runningAnimations.clear();
    m_suspendedAnimations.clear();
}

void LayerCompositingThread::removeSublayer(LayerCompositingThread* sublayer)
{
    ASSERT(isCompositingThread());

    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
}

int LayerCompositingThread::indexOfSublayer(const LayerCompositingThread* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

const LayerCompositingThread* LayerCompositingThread::rootLayer() const
{
    const LayerCompositingThread* layer = this;
    for (LayerCompositingThread* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerCompositingThread::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerCompositingThread::setSublayers(const Vector<RefPtr<LayerCompositingThread> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    while (m_sublayers.size()) {
        RefPtr<LayerCompositingThread> layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }

    m_sublayers.clear();

    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++) {
        RefPtr<LayerCompositingThread> sublayer = sublayers[i];
        sublayer->removeFromSuperlayer();
        sublayer->setSuperlayer(this);
        m_sublayers.insert(i, sublayer);
    }
}

void LayerCompositingThread::updateTextureContentsIfNeeded()
{
    if (m_texID || pluginView())
        return;

#if ENABLE(VIDEO)
    if (mediaPlayer())
        return;
#endif

    m_tiler->uploadTexturesIfNeeded();
}

void LayerCompositingThread::setVisible(bool visible)
{
    if (visible == m_visible)
        return;

    m_visible = visible;

    if (m_texID || pluginView())
        return;

#if ENABLE(VIDEO)
    if (mediaPlayer())
        return;
#endif

    m_tiler->layerVisibilityChanged(visible);
}

void LayerCompositingThread::setNeedsCommit()
{
    if (m_layerRenderer)
        m_layerRenderer->setNeedsCommit();
}

void LayerCompositingThread::scheduleCommit()
{
    if (!isWebKitThread()) {
        if (m_commitScheduled)
            return;

        m_commitScheduled = true;

        dispatchWebKitMessage(BlackBerry::Platform::createMethodCallMessage(&LayerCompositingThread::scheduleCommit, this));
        return;
    }

    m_commitScheduled = false;

    // FIXME: The only way to get at our LayerWebKitThread is to go through the tiler.
    if (LayerWebKitThread* layer = m_tiler->layer())
        layer->setNeedsCommit();
}

bool LayerCompositingThread::updateAnimations(double currentTime)
{
    // The commit mechanism always overwrites our state with state from the
    // WebKit thread. This means we have to restore the last animated value for
    // suspended animations.
    for (size_t i = 0; i < m_suspendedAnimations.size(); ++i) {
        LayerAnimation* animation = m_suspendedAnimations[i].get();
        // From looking at the WebCore code, it appears that when the animation
        // is paused, the timeOffset is modified so it will be an appropriate
        // elapsedTime.
        double elapsedTime = animation->timeOffset();
        animation->apply(this, elapsedTime);
    }

    for (size_t i = 0; i < m_runningAnimations.size(); ++i) {
        LayerAnimation* animation = m_runningAnimations[i].get();
        double elapsedTime = (m_suspendTime ? m_suspendTime : currentTime) - animation->startTime() + animation->timeOffset();
        animation->apply(this, elapsedTime);
    }

    return !m_runningAnimations.isEmpty();
}

bool LayerCompositingThread::hasVisibleHolePunchRect() const
{
    if (m_pluginView && !m_isVisible)
        return false;

#if ENABLE(VIDEO)
    if (m_mediaPlayer && !m_isVisible)
        return false;
#endif

    return hasHolePunchRect();
}

void LayerCompositingThread::createLayerRendererSurface()
{
    ASSERT(!m_layerRendererSurface);
    m_layerRendererSurface = adoptPtr(new LayerRendererSurface(m_layerRenderer, this));
}

}

#endif // USE(ACCELERATED_COMPOSITING)
