/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderLayerModelObject.h"

#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleScrollSnapPoints.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLayerModelObject);

bool RenderLayerModelObject::s_wasFloating = false;
bool RenderLayerModelObject::s_hadLayer = false;
bool RenderLayerModelObject::s_hadTransform = false;
bool RenderLayerModelObject::s_layerWasSelfPainting = false;

RenderLayerModelObject::RenderLayerModelObject(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(element, WTFMove(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::RenderLayerModelObject(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(document, WTFMove(style), baseTypeFlags | RenderLayerModelObjectFlag)
{
}

RenderLayerModelObject::~RenderLayerModelObject()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderLayerModelObject::willBeDestroyed()
{
    if (isPositioned()) {
        if (style().hasViewportConstrainedPosition())
            view().frameView().removeViewportConstrainedObject(*this);
    }

    if (hasLayer()) {
        setHasLayer(false);
        destroyLayer();
    }

    RenderElement::willBeDestroyed();
}

void RenderLayerModelObject::destroyLayer()
{
    ASSERT(!hasLayer());
    ASSERT(m_layer);
    m_layer = nullptr;
}

void RenderLayerModelObject::createLayer()
{
    ASSERT(!m_layer);
    m_layer = makeUnique<RenderLayer>(*this);
    setHasLayer(true);
    m_layer->insertOnlyThisLayer(RenderLayer::LayerChangeTiming::StyleChange);
}

bool RenderLayerModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

void RenderLayerModelObject::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    s_wasFloating = isFloating();
    s_hadLayer = hasLayer();
    s_hadTransform = hasTransform();
    if (s_hadLayer)
        s_layerWasSelfPainting = layer()->isSelfPaintingLayer();

    auto* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (diff == StyleDifference::RepaintLayer && parent() && oldStyle && oldStyle->clip() != newStyle.clip())
        layer()->clearClipRectsIncludingDescendants();
    RenderElement::styleWillChange(diff, newStyle);
}

void RenderLayerModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderElement::styleDidChange(diff, oldStyle);
    updateFromStyle();

    if (requiresLayer()) {
        if (!layer() && layerCreationAllowedForSubtree()) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout();
            createLayer();
            if (parent() && !needsLayout() && containingBlock())
                layer()->setRepaintStatus(NeedsFullRepaint);
        }
    } else if (layer() && layer()->parent()) {
#if ENABLE(CSS_COMPOSITING)
        if (oldStyle && oldStyle->hasBlendMode())
            layer()->willRemoveChildWithBlendMode();
#endif
        setHasTransformRelatedProperty(false); // All transform-related properties force layers, so we know we don't have one or the object doesn't support them.
        setHasReflection(false);

        // Repaint the about to be destroyed self-painting layer when style change also triggers repaint.
        if (layer()->isSelfPaintingLayer() && layer()->repaintStatus() == NeedsFullRepaint && layer()->repaintRects())
            repaintUsingContainer(containerForRepaint(), layer()->repaintRects()->clippedOverflowRect);

        layer()->removeOnlyThisLayer(RenderLayer::LayerChangeTiming::StyleChange); // calls destroyLayer() which clears m_layer
        if (s_wasFloating && isFloating())
            setChildNeedsLayout();
        if (s_hadTransform)
            setNeedsLayoutAndPrefWidthsRecalc();
    }

    if (layer()) {
        layer()->styleChanged(diff, oldStyle);
        if (s_hadLayer && layer()->isSelfPaintingLayer() != s_layerWasSelfPainting)
            setChildNeedsLayout();
    }

    bool newStyleIsViewportConstrained = style().hasViewportConstrainedPosition();
    bool oldStyleIsViewportConstrained = oldStyle && oldStyle->hasViewportConstrainedPosition();
    if (newStyleIsViewportConstrained != oldStyleIsViewportConstrained) {
        if (newStyleIsViewportConstrained && layer())
            view().frameView().addViewportConstrainedObject(*this);
        else
            view().frameView().removeViewportConstrainedObject(*this);
    }

    const RenderStyle& newStyle = style();
    if (oldStyle && oldStyle->scrollPadding() != newStyle.scrollPadding()) {
        if (isDocumentElementRenderer()) {
            FrameView& frameView = view().frameView();
            frameView.updateScrollbarSteps();
        } else if (RenderLayer* renderLayer = layer())
            renderLayer->updateScrollbarSteps();
    }

    bool scrollMarginChanged = oldStyle && oldStyle->scrollMargin() != newStyle.scrollMargin();
    bool scrollAlignChanged = oldStyle && oldStyle->scrollSnapAlign() != newStyle.scrollSnapAlign();
    bool scrollSnapStopChanged = oldStyle && oldStyle->scrollSnapStop() != newStyle.scrollSnapStop();
    if (scrollMarginChanged || scrollAlignChanged || scrollSnapStopChanged) {
        if (auto* scrollSnapBox = enclosingScrollableContainerForSnapping())
            scrollSnapBox->setNeedsLayout();
    }
}

bool RenderLayerModelObject::shouldPlaceVerticalScrollbarOnLeft() const
{
// RTL Scrollbars require some system support, and this system support does not exist on certain versions of OS X. iOS uses a separate mechanism.
#if PLATFORM(IOS_FAMILY)
    return false;
#else
    switch (settings().userInterfaceDirectionPolicy()) {
    case UserInterfaceDirectionPolicy::Content:
        return style().shouldPlaceVerticalScrollbarOnLeft();
    case UserInterfaceDirectionPolicy::System:
        return settings().systemLayoutDirection() == TextDirection::RTL;
    }
    ASSERT_NOT_REACHED();
    return style().shouldPlaceVerticalScrollbarOnLeft();
#endif
}

std::optional<LayerRepaintRects> RenderLayerModelObject::layerRepaintRects() const
{
    return hasLayer() ? layer()->repaintRects() : std::nullopt;
}

bool RenderLayerModelObject::startAnimation(double timeOffset, const Animation& animation, const KeyframeList& keyframes)
{
    if (!layer() || !layer()->backing())
        return false;
    return layer()->backing()->startAnimation(timeOffset, animation, keyframes);
}

void RenderLayerModelObject::animationPaused(double timeOffset, const String& name)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationPaused(timeOffset, name);
}

void RenderLayerModelObject::animationFinished(const String& name)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->animationFinished(name);
}

void RenderLayerModelObject::transformRelatedPropertyDidChange()
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->transformRelatedPropertyDidChange();
}

void RenderLayerModelObject::suspendAnimations(MonotonicTime time)
{
    if (!layer() || !layer()->backing())
        return;
    layer()->backing()->suspendAnimations(time);
}

} // namespace WebCore

