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
#include "GraphicsLayer.h"
#include "LayerCompositingThread.h"
#include "LayerWebKitThread.h"
#include "Path.h"
#include "PlatformContextSkia.h"
#include "ScaleTransformOperation.h"
#include "WebPageCompositorClient.h"
#include "WebPageCompositor_p.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformMessageClient.h>
#include <SkCornerPathEffect.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

const double ActiveTextFadeAnimationDuration = 0.3;

static const char* fadeAnimationName() { return "fade"; }

DefaultTapHighlight::DefaultTapHighlight(WebPagePrivate* page)
    : m_page(page)
    , m_visible(false)
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
    IntRect rect = m_region.extents();
    if (rect.isEmpty())
        return;

    m_visible = true;

    if (!m_layer) {
        m_layer = GraphicsLayer::create(this);
        m_page->overlayLayer()->addChild(m_layer.get());
    }

    m_layer->setPosition(rect.location());
    m_layer->setSize(rect.size());
    m_layer->setDrawsContent(true);
    m_layer->removeAnimation(fadeAnimationName());
    m_layer->setOpacity(1.0);
    m_layer->setNeedsDisplay();
}

void DefaultTapHighlight::hide()
{
    if (!m_layer)
        return;

    // This animation needs to be created anew each time, since
    // the method may be called on differend threads.
    RefPtr<Animation> fadeAnimation = Animation::create();
    fadeAnimation->setDuration(ActiveTextFadeAnimationDuration);
    KeyframeValueList keyframes(AnimatedPropertyOpacity);
    keyframes.insert(new FloatAnimationValue(0, 1.0));
    keyframes.insert(new FloatAnimationValue(1.0, 0));

    // Normally, this method is called on the WebKit thread, but it can also be
    // called from the compositing thread.
    if (BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread()) {
        if (!m_visible)
            return;
        m_visible = false;
        m_layer->addAnimation(keyframes, m_region.extents().size(), fadeAnimation.get(), fadeAnimationName(), 0.0);
    } else if (BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        RefPtr<LayerAnimation> animation = LayerAnimation::create(keyframes, m_region.extents().size(), fadeAnimation.get(), fadeAnimationName(), 0.0);
        if (WebPageCompositorClient* compositorClient = m_page->compositor()->client()) {
            double animationTime = compositorClient->requestAnimationFrame();
            compositorClient->invalidate(animationTime);
        }
        // FIXME: Unfortunately WebPageCompositorClient::requestAnimationFrame uses a different time coordinate system
        // than accelerated animations, so we can't use the time returned by RAF for starttime.
        animation->setStartTime(currentTime());
        m_layer->platformLayer()->layerCompositingThread()->addAnimation(animation.get());
    }
}

void DefaultTapHighlight::notifySyncRequired(const GraphicsLayer*)
{
    m_page->scheduleRootLayerCommit();
}

void DefaultTapHighlight::paintContents(const GraphicsLayer*, GraphicsContext& c, GraphicsLayerPaintingPhase, const IntRect& /*inClip*/)
{
    std::vector<Platform::IntRect> rects = m_region.rects();
    Platform::IntRect rect = m_region.extents();
    SkRegion windowRegion;

    unsigned rectCount = m_region.numRects();
    if (!rectCount)
        return;

    for (unsigned i = 0; i < rectCount; ++i) {
        Platform::IntRect rectToPaint = rects[i];
        SkIRect r = SkIRect::MakeXYWH(rectToPaint.x(), rectToPaint.y(), rectToPaint.width(), rectToPaint.height());
        windowRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath pathToPaint;
    windowRegion.getBoundaryPath(&pathToPaint);

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

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)
