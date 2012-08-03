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

#include "InspectorOverlay.h"

#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "Page.h"
#include "Settings.h"
#include "WebPage_p.h"


namespace WebCore {

PassOwnPtr<InspectorOverlay> InspectorOverlay::create(BlackBerry::WebKit::WebPagePrivate* page, InspectorOverlayClient* client)
{
    return adoptPtr(new InspectorOverlay(page, client));
}

InspectorOverlay::InspectorOverlay(BlackBerry::WebKit::WebPagePrivate* page, InspectorOverlayClient* client)
    : m_webPage(page)
    , m_client(client)
{
}

#if USE(ACCELERATED_COMPOSITING)
void InspectorOverlay::notifySyncRequired(const GraphicsLayer*)
{
    m_webPage->scheduleRootLayerCommit();
}

void InspectorOverlay::paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& inClip)
{
    context.save();
    IntPoint scrollPosition = m_webPage->focusedOrMainFrame()->view()->scrollPosition();
    context.translate(scrollPosition.x(), scrollPosition.y());
    m_client->paintInspectorOverlay(context);
    context.restore();
}

bool InspectorOverlay::showDebugBorders(const GraphicsLayer*) const
{
    return m_webPage->m_page->settings()->showDebugBorders();
}

bool InspectorOverlay::showRepaintCounter(const GraphicsLayer*) const
{
    return m_webPage->m_page->settings()->showRepaintCounter();
}
#endif

InspectorOverlay::~InspectorOverlay() { }

void InspectorOverlay::clear()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_overlay) {
        m_overlay->removeFromParent();
        m_overlay = nullptr;
    }
#endif
}

void InspectorOverlay::update()
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_overlay) {
        m_overlay = adoptPtr(new BlackBerry::WebKit::WebOverlay(this));
        const IntSize size = m_webPage->contentsSize();
        m_overlay->setSize(FloatSize(size.width(), size.height()));
        m_webPage->m_webPage->addOverlay(m_overlay.get());
    }

    m_overlay->setDrawsContent(true);
    m_overlay->setOpacity(1.0);
    m_overlay->invalidate();
#endif
}

}
