/*
    Copyright (C) 2012 Samsung Electronics

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

#include "AcceleratedCompositingContext.h"
#include "Bitmap.h"
#include "FrameView.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebView.h"

#if USE(TEXTURE_MAPPER)
#include "GraphicsLayerTextureMapper.h"
#include "TextureMapperLayer.h"
#endif

const double compositingFrameRate = 60;

namespace WebCore {

AcceleratedCompositingContext::AcceleratedCompositingContext(BWebView* view)
    : m_view(view)
    , m_rootLayer(nullptr)
    , m_syncTimer(*this, &AcceleratedCompositingContext::syncLayers)
{
    ASSERT(m_view);

#if USE(TEXTURE_MAPPER)
    m_textureMapper = TextureMapper::create();
#endif
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
    m_syncTimer.stop();
#if USE(TEXTURE_MAPPER)
    m_textureMapper = nullptr;
#endif
}

void AcceleratedCompositingContext::syncLayers()
{
    flushAndRenderLayers();
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    // Check that we aren't being deleted...
    if (!m_view->LockLooper()) return;
    BWebPage* page = m_view->WebPage();
    m_view->UnlockLooper();

    if (!page) return;

    Frame& frame = *(Frame*)(page->MainFrame()->Frame());
    if (!frame.contentRenderer() || !frame.view())
        return;
    frame.view()->updateLayoutAndStyleIfNeededRecursive();

    if (flushPendingLayerChanges())
        paintToGraphicsContext();
}

bool AcceleratedCompositingContext::flushPendingLayerChanges()
{
#if USE(TEXTURE_MAPPER)
    if (m_rootLayer)
        m_rootLayer->flushCompositingStateForThisLayerOnly();
#endif
    return m_view->WebPage()->MainFrame()->Frame()->view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::paintToGraphicsContext()
{
    BView* target = m_view->OffscreenView();
    GraphicsContext context(target);

    if(target->LockLooper()) {
        compositeLayers(target->Bounds());
        target->Sync();
        target->UnlockLooper();
    }

    if(m_view->LockLooper()) {
        m_view->Invalidate(m_updateRect);
        m_view->UnlockLooper();
    }
}

void AcceleratedCompositingContext::compositeLayers(BRect updateRect)
{
#if USE(TEXTURE_MAPPER)
    if (!m_rootLayer || !m_rootLayer->isGraphicsLayerTextureMapper())
        return;

    TextureMapperLayer& currentRootLayer = downcast<GraphicsLayerTextureMapper>(m_rootLayer)->layer();

    m_updateRect = updateRect;

    currentRootLayer.setTextureMapper(m_textureMapper.get());

    m_textureMapper->beginPainting();

    currentRootLayer.paint();
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper);
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (currentRootLayer.descendantsOrSelfHaveRunningAnimations() && !m_syncTimer.isActive())
        m_syncTimer.startOneShot(WTF::Seconds(1 / compositingFrameRate));
#endif
}

void AcceleratedCompositingContext::setRootGraphicsLayer(GraphicsLayer* rootLayer)
{
#if USE(TEXTURE_MAPPER)
    m_rootLayer = rootLayer;
#endif

    if (!m_syncTimer.isActive())
        m_syncTimer.startOneShot(WTF::Seconds(0));
}

} // namespace WebCore
