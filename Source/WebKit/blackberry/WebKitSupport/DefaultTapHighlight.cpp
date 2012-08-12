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

#include "DefaultTapHighlight.h"

#include "GraphicsContext.h"
#include "Path.h"
#include "PlatformContextSkia.h"
#include "WebAnimation.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformMessageClient.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

const double ActiveTextFadeAnimationDuration = 0.3;

static const char* fadeAnimationName() { return "fade"; }

DefaultTapHighlight::DefaultTapHighlight(WebPagePrivate* page)
    : m_page(page)
    , m_visible(false)
    , m_shouldHideAfterScroll(false)
{
}

DefaultTapHighlight::~DefaultTapHighlight()
{
}

void DefaultTapHighlight::draw(const Platform::IntRectRegion& region, int red, int green, int blue, int alpha, bool hideAfterScroll)
{
    ASSERT(BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread());

    m_region = region;
    m_color = Color(red, green, blue, std::min(128, alpha));
    m_shouldHideAfterScroll = hideAfterScroll;
    FloatRect rect = IntRect(m_region.extents());
    if (rect.isEmpty())
        return;

    // Transparent color means disable the tap highlight.
    if (!m_color.alpha()) {
        hide();
        return;
    }

    {
        MutexLocker lock(m_mutex);
        m_visible = true;
    }

    if (!m_overlay) {
        m_overlay = adoptPtr(new WebOverlay(this));
        m_page->m_webPage->addOverlay(m_overlay.get());
    }

    m_overlay->resetOverrides();
    m_overlay->setPosition(rect.location());
    m_overlay->setSize(rect.size());
    m_overlay->setDrawsContent(true);
    m_overlay->removeAnimation(fadeAnimationName());
    m_overlay->setOpacity(1.0);
    m_overlay->invalidate();
}

void DefaultTapHighlight::hide()
{
    if (!m_overlay)
        return;

    {
        MutexLocker lock(m_mutex);
        if (!m_visible)
            return;
        m_visible = false;
    }

    // Since WebAnimation is not thread safe, we create a new one each time instead of reusing the same object on different
    // threads (that would introduce race conditions).
    WebAnimation fadeAnimation = WebAnimation::fadeAnimation(fadeAnimationName(), 1.0, 0.0, ActiveTextFadeAnimationDuration);

    // Normally, this method is called on the WebKit thread, but it can also be
    // called from the compositing thread.
    if (BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread())
        m_overlay->addAnimation(fadeAnimation);
    else if (BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        m_overlay->override()->addAnimation(fadeAnimation);
}

void DefaultTapHighlight::notifySyncRequired(const GraphicsLayer* layer)
{
    m_page->notifySyncRequired(layer);
}

void DefaultTapHighlight::paintContents(const GraphicsLayer*, GraphicsContext& c, GraphicsLayerPaintingPhase, const IntRect& /*inClip*/)
{
    std::vector<Platform::IntRect> rects = m_region.rects();
    Platform::IntRect rect = m_region.extents();
    SkRegion overlayRegion;

    unsigned rectCount = m_region.numRects();
    if (!rectCount)
        return;

    for (unsigned i = 0; i < rectCount; ++i) {
        Platform::IntRect rectToPaint = rects[i];
        SkIRect r = SkIRect::MakeXYWH(rectToPaint.x(), rectToPaint.y(), rectToPaint.width(), rectToPaint.height());
        overlayRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath pathToPaint;
    overlayRegion.getBoundaryPath(&pathToPaint);

    Path path(pathToPaint);
    c.save();
    c.translate(-rect.x(), -rect.y());

    // Draw tap highlight
    c.setFillColor(m_color, ColorSpaceDeviceRGB);
    c.fillPath(path);
    Color darker = Color(m_color.red(), m_color.green(), m_color.blue()); // Get rid of alpha.
    c.setStrokeColor(darker, ColorSpaceDeviceRGB);
    c.setStrokeThickness(1);
    c.strokePath(path);
    c.restore();
}

bool DefaultTapHighlight::showDebugBorders(const GraphicsLayer* layer) const
{
    return m_page->showDebugBorders(layer);
}

bool DefaultTapHighlight::showRepaintCounter(const GraphicsLayer* layer) const
{
    return m_page->showRepaintCounter(layer);
}

bool DefaultTapHighlight::contentsVisible(const GraphicsLayer*, const IntRect& contentRect) const
{
    // This layer is typically small enough that we can afford to cache all tiles and never
    // risk checkerboarding.
    return true;
}

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)
