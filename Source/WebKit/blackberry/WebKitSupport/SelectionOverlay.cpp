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

#if USE(ACCELERATED_COMPOSITING)

#include "SelectionOverlay.h"

#include "GraphicsContext.h"
#include "LayerMessage.h"
#include "Path.h"
#include "PlatformContextSkia.h"
#include "RenderTheme.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformMessageClient.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

SelectionOverlay::SelectionOverlay(WebPagePrivate* page)
    : m_page(page)
    , m_hideDispatched(false)
{
}

SelectionOverlay::~SelectionOverlay()
{
}

void SelectionOverlay::draw(const Platform::IntRectRegion& region)
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

    m_region = region;
    FloatRect rect = IntRect(m_region.extents());
    if (rect.isEmpty()) {
        hide();
        return;
    }

    if (!m_overlay)
        m_overlay = adoptPtr(new WebOverlay(this));

    m_page->m_webPage->addOverlay(m_overlay.get());
    m_overlay->resetOverrides();
    m_overlay->setPosition(rect.location());
    m_overlay->setSize(rect.size());
    m_overlay->setDrawsContent(true);
    m_overlay->setOpacity(1.0);
    m_overlay->invalidate();
}

void SelectionOverlay::hide()
{
    // Track a dispatched message, we don't want to flood the webkit thread.
    // There can be as many as one more message enqued as needed but never less.
    if (isWebKitThread())
        m_hideDispatched = false;
    else if (m_hideDispatched) {
        // Early return if there is message already pending on the webkit thread.
        return;
    }
    if (!isWebKitThread()) {
        m_hideDispatched = true;
        // Normally, this method is called on the WebKit thread, but it can also be
        // called from the compositing thread.
        dispatchWebKitMessage(BlackBerry::Platform::createMethodCallMessage(&SelectionOverlay::hide, this));
        return;
    }
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

    if (!m_overlay)
        return;

    m_page->m_webPage->removeOverlay(m_overlay.get());
}

void SelectionOverlay::notifySyncRequired(const GraphicsLayer*)
{
    m_page->scheduleRootLayerCommit();
}

void SelectionOverlay::paintContents(const GraphicsLayer*, GraphicsContext& c, GraphicsLayerPaintingPhase, const IntRect& inClip)
{
    std::vector<Platform::IntRect> rects = m_region.rects();
    Platform::IntRect rect = m_region.extents();
    SkRegion windowRegion;

    unsigned rectCount = m_region.numRects();
    if (!rectCount)
        return;

    IntRect clip(inClip);
    clip.move(rect.x(), rect.y());
    for (unsigned i = 0; i < rectCount; ++i) {
        IntRect rectToPaint = rects[i];
        rectToPaint.intersect(clip);
        SkIRect r = SkIRect::MakeXYWH(rectToPaint.x(), rectToPaint.y(), rectToPaint.width(), rectToPaint.height());
        windowRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath pathToPaint;
    windowRegion.getBoundaryPath(&pathToPaint);

    Path path(pathToPaint);
    c.save();
    c.translate(-rect.x(), -rect.y());

    // Draw selection overlay
    Color color = RenderTheme::defaultTheme()->activeSelectionBackgroundColor();
    c.setFillColor(color, ColorSpaceDeviceRGB);
    c.fillPath(path);

    c.restore();
}

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)
