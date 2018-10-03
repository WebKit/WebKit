/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "RenderLayer.h"

#include "BoxShape.h"
#include "CSSAnimationController.h"
#include "CSSFilter.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentEventQueue.h"
#include "DocumentMarkerController.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "EventHandler.h"
#include "FEColorMatrix.h"
#include "FEMerge.h"
#include "FloatConversion.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLFormControlElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "HitTestingTransformState.h"
#include "Logging.h"
#include "OverflowEvent.h"
#include "OverlapTestRequestClient.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentContainer.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerFilters.h"
#include "RenderMarquee.h"
#include "RenderMultiColumnFlow.h"
#include "RenderReplica.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGRoot.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "ScaleTransformOperation.h"
#include "ScriptDisallowedScope.h"
#include "ScrollAnimator.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceGraphic.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include "WheelEventTestTrigger.h"
#include <stdio.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "AxisScrollSnapOffsets.h"
#endif

#define MIN_INTERSECT_FOR_REVEAL 32

namespace WebCore {

using namespace HTMLNames;

class ClipRects : public RefCounted<ClipRects> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ClipRects> create()
    {
        return adoptRef(*new ClipRects);
    }

    static Ref<ClipRects> create(const ClipRects& other)
    {
        return adoptRef(*new ClipRects(other));
    }

    void reset()
    {
        m_overflowClipRect.reset();
        m_fixedClipRect.reset();
        m_posClipRect.reset();
        m_fixed = false;
    }

    const ClipRect& overflowClipRect() const { return m_overflowClipRect; }
    void setOverflowClipRect(const ClipRect& clipRect) { m_overflowClipRect = clipRect; }

    const ClipRect& fixedClipRect() const { return m_fixedClipRect; }
    void setFixedClipRect(const ClipRect& clipRect) { m_fixedClipRect = clipRect; }

    const ClipRect& posClipRect() const { return m_posClipRect; }
    void setPosClipRect(const ClipRect& clipRect) { m_posClipRect = clipRect; }

    bool fixed() const { return m_fixed; }
    void setFixed(bool fixed) { m_fixed = fixed; }

    bool operator==(const ClipRects& other) const
    {
        return m_overflowClipRect == other.overflowClipRect()
            && m_fixedClipRect == other.fixedClipRect()
            && m_posClipRect == other.posClipRect()
            && m_fixed == other.fixed();
    }

    ClipRects& operator=(const ClipRects& other)
    {
        m_overflowClipRect = other.overflowClipRect();
        m_fixedClipRect = other.fixedClipRect();
        m_posClipRect = other.posClipRect();
        m_fixed = other.fixed();
        return *this;
    }

private:
    ClipRects() = default;

    ClipRects(const LayoutRect& clipRect)
        : m_overflowClipRect(clipRect)
        , m_fixedClipRect(clipRect)
        , m_posClipRect(clipRect)
    {
    }

    ClipRects(const ClipRects& other)
        : RefCounted()
        , m_fixed(other.fixed())
        , m_overflowClipRect(other.overflowClipRect())
        , m_fixedClipRect(other.fixedClipRect())
        , m_posClipRect(other.posClipRect())
    {
    }

    bool m_fixed { false };
    ClipRect m_overflowClipRect;
    ClipRect m_fixedClipRect;
    ClipRect m_posClipRect;
};

class ClipRectsCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipRectsCache()
    {
#ifndef NDEBUG
        for (int i = 0; i < NumCachedClipRectsTypes; ++i) {
            m_clipRectsRoot[i] = 0;
            m_scrollbarRelevancy[i] = IgnoreOverlayScrollbarSize;
        }
#endif
    }

    ClipRects* getClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow) const
    {
        return m_clipRects[getIndex(clipRectsType, respectOverflow)].get();
    }

    void setClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow, RefPtr<ClipRects>&& clipRects)
    {
        m_clipRects[getIndex(clipRectsType, respectOverflow)] = WTFMove(clipRects);
    }

#ifndef NDEBUG
    const RenderLayer* m_clipRectsRoot[NumCachedClipRectsTypes];
    OverlayScrollbarSizeRelevancy m_scrollbarRelevancy[NumCachedClipRectsTypes];
#endif

private:
    unsigned getIndex(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow) const
    {
        unsigned index = static_cast<unsigned>(clipRectsType);
        if (respectOverflow == RespectOverflowClip)
            index += static_cast<unsigned>(NumCachedClipRectsTypes);
        ASSERT_WITH_SECURITY_IMPLICATION(index < NumCachedClipRectsTypes * 2);
        return index;
    }

    RefPtr<ClipRects> m_clipRects[NumCachedClipRectsTypes * 2];
};

void makeMatrixRenderable(TransformationMatrix& matrix, bool has3DRendering)
{
#if !ENABLE(3D_TRANSFORMS)
    UNUSED_PARAM(has3DRendering);
    matrix.makeAffine();
#else
    if (!has3DRendering)
        matrix.makeAffine();
#endif
}

RenderLayer::RenderLayer(RenderLayerModelObject& rendererLayerModelObject)
    : m_isRenderViewLayer(rendererLayerModelObject.isRenderView())
    , m_forcedStackingContext(rendererLayerModelObject.isMedia())
    , m_inResizeMode(false)
    , m_scrollDimensionsDirty(true)
    , m_normalFlowListDirty(true)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_inOverflowRelayout(false)
    , m_repaintStatus(NeedsNormalRepaint)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_registeredScrollableArea(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_hasCompositingDescendant(false)
    , m_hasTransformedAncestor(false)
    , m_has3DTransformedAncestor(false)
    , m_indirectCompositingReason(static_cast<unsigned>(IndirectCompositingReason::None))
    , m_viewportConstrainedNotCompositedReason(NoNotCompositedReason)
#if PLATFORM(IOS)
    , m_adjustForIOSCaretWhenScrolling(false)
#endif
#if PLATFORM(IOS)
#if ENABLE(IOS_TOUCH_EVENTS)
    , m_registeredAsTouchEventListenerForScrolling(false)
#endif
    , m_inUserScroll(false)
    , m_requiresScrollBoundsOriginUpdate(false)
#endif
    , m_containsDirtyOverlayScrollbars(false)
    , m_updatingMarqueePosition(false)
#if !ASSERT_DISABLED
    , m_layerListMutationAllowed(true)
#endif
#if ENABLE(CSS_COMPOSITING)
    , m_blendMode(static_cast<unsigned>(BlendMode::Normal))
    , m_hasNotIsolatedCompositedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendantsStatusDirty(false)
#endif
    , m_renderer(rendererLayerModelObject)
{
    m_isNormalFlowOnly = shouldBeNormalFlowOnly();
    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    // Non-stacking containers should have empty z-order lists. As this is already the case,
    // there is no need to dirty / recompute these lists.
    m_zOrderListsDirty = isStackingContext();

    if (!renderer().firstChild()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer().style().visibility() == Visibility::Visible;
    }

    if (Element* element = renderer().element()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        m_scrollPosition = element->savedLayerScrollPosition();
        if (!m_scrollPosition.isZero())
            scrollAnimator().setCurrentPosition(m_scrollPosition);
        element->setSavedLayerScrollPosition(IntPoint());
    }
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode())
        renderer().frame().eventHandler().resizeLayerDestroyed();

    ASSERT(m_registeredScrollableArea == renderer().view().frameView().containsScrollableArea(this));

    if (m_registeredScrollableArea)
        renderer().view().frameView().removeScrollableArea(this);

#if ENABLE(IOS_TOUCH_EVENTS)
    unregisterAsTouchEventListenerForScrolling();
#endif
    if (Element* element = renderer().element())
        element->setSavedLayerScrollPosition(m_scrollPosition);

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (auto* scrollingCoordinator = renderer().page().scrollingCoordinator())
        scrollingCoordinator->willDestroyScrollableArea(*this);

    if (m_reflection)
        removeReflection();

    clearScrollCorner();
    clearResizer();

    clearLayerFilters();

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearBacking(true);

    // Layer and all its children should be removed from the tree before destruction.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(renderer().renderTreeBeingDestroyed() || !m_parent);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(renderer().renderTreeBeingDestroyed() || !m_first);
}

String RenderLayer::name() const
{
    StringBuilder name;

    if (Element* element = renderer().element()) {
        name.append(" <");
        name.append(element->tagName().convertToLowercaseWithoutLocale());
        name.append('>');

        if (element->hasID()) {
            name.appendLiteral(" id=\'");
            name.append(element->getIdAttribute());
            name.append('\'');
        }

        if (element->hasClass()) {
            name.appendLiteral(" class=\'");
            size_t classNamesToDump = element->classNames().size();
            const size_t maxNumClassNames = 7;
            bool addEllipsis = false;
            if (classNamesToDump > maxNumClassNames) {
                classNamesToDump = maxNumClassNames;
                addEllipsis = true;
            }
            
            for (size_t i = 0; i < classNamesToDump; ++i) {
                if (i > 0)
                    name.append(' ');
                name.append(element->classNames()[i]);
            }
            if (addEllipsis)
                name.append("...");
            name.append('\'');
        }
    } else
        name.append(renderer().renderName());

    if (isReflection())
        name.appendLiteral(" (reflection)");

    return name.toString();
}

RenderLayerCompositor& RenderLayer::compositor() const
{
    return renderer().view().compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    if ((changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged || changeType == ImageChanged) && compositor().updateLayerCompositingState(*this))
        compositor().setCompositingLayersNeedRebuild();

    if (m_backing)
        m_backing->contentChanged(changeType);
}

bool RenderLayer::canRender3DTransforms() const
{
    return compositor().canRender3DTransforms();
}

bool RenderLayer::paintsWithFilters() const
{
    if (!renderer().hasFilter())
        return false;
        
    if (!isComposited())
        return true;

    return !m_backing->canCompositeFilters();
}

bool RenderLayer::requiresFullLayerImageForFilters() const 
{
    if (!paintsWithFilters())
        return false;

    return m_filters && m_filters->hasFilterThatMovesPixels();
}

void RenderLayer::updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, OptionSet<UpdateLayerPositionsFlag> flags)
{
    LOG(Compositing, "RenderLayer %p updateLayerPositionsAfterLayout", this);
    RenderGeometryMap geometryMap(UseTransforms);
    if (this != rootLayer)
        geometryMap.pushMappingsToAncestor(parent(), nullptr);
    updateLayerPositions(&geometryMap, flags);
}

void RenderLayer::updateLayerPositions(RenderGeometryMap* geometryMap, OptionSet<UpdateLayerPositionsFlag> flags)
{
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.

    applyPostLayoutScrollPositionIfNeeded();

    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    // Clear our cached clip rect information.
    clearClipRects();
    
    if (hasOverflowControls()) {
        LayoutSize offsetFromRoot;
        if (geometryMap)
            offsetFromRoot = LayoutSize(toFloatSize(geometryMap->absolutePoint(FloatPoint())));
        else {
            // FIXME: It looks suspicious to call convertToLayerCoords here
            // as canUseConvertToLayerCoords may be true for an ancestor layer.
            offsetFromRoot = offsetFromAncestor(root());
        }
        positionOverflowControls(roundedIntSize(offsetFromRoot));
    }

    updateDescendantDependentFlags();

    if (flags & UpdatePagination)
        updatePagination();
    else
        m_enclosingPaginationLayer = nullptr;
    
    if (m_hasVisibleContent) {
        // FIXME: Paint offset cache does not work with RenderLayers as there is not a 1-to-1
        // mapping between them and the RenderObjects. It would be neat to enable
        // LayoutState outside the layout() phase and use it here.
        ASSERT(!renderer().view().frameView().layoutContext().isPaintOffsetCacheEnabled());

        RenderLayerModelObject* repaintContainer = renderer().containerForRepaint();
        
        auto hadRepaintLayoutRects = renderer().hasRepaintLayoutRects();
        RepaintLayoutRects oldRects = hadRepaintLayoutRects ? renderer().repaintLayoutRects() : RepaintLayoutRects();
        computeRepaintRects(repaintContainer, geometryMap);
        
        auto hasRepaintLayoutRects = renderer().hasRepaintLayoutRects();
        RepaintLayoutRects newRects = hasRepaintLayoutRects ? renderer().repaintLayoutRects() : RepaintLayoutRects();
        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if ((flags & CheckForRepaint) && hasRepaintLayoutRects) {
            if (!renderer().view().printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    if (hadRepaintLayoutRects)
                        renderer().repaintUsingContainer(repaintContainer, oldRects.m_repaintRect);
                    if (!hadRepaintLayoutRects || newRects.m_repaintRect != oldRects.m_repaintRect)
                        renderer().repaintUsingContainer(repaintContainer, newRects.m_repaintRect);
                } else if (shouldRepaintAfterLayout()) {
                    // FIXME: We will convert this to just take the old and new RepaintLayoutRects once
                    // we change other callers to use RepaintLayoutRects.
                    renderer().repaintAfterLayoutIfNeeded(repaintContainer, oldRects.m_repaintRect, oldRects.m_outlineBox, &newRects.m_repaintRect, &newRects.m_outlineBox);
                }
            }
        }
    } else
        clearRepaintRects();

    m_repaintStatus = NeedsNormalRepaint;
    m_hasTransformedAncestor = flags.contains(SeenTransformedLayer);
    m_has3DTransformedAncestor = flags.contains(Seen3DTransformedLayer);

    // Update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

    // Clear the IsCompositingUpdateRoot flag once we've found the first compositing layer in this update.
    bool isUpdateRoot = flags.contains(IsCompositingUpdateRoot);
    if (isComposited())
        flags.remove(IsCompositingUpdateRoot);

    if (renderer().isInFlowRenderFragmentedFlow()) {
        updatePagination();
        flags.add(UpdatePagination);
    }

    if (transform()) {
        flags.add(SeenTransformedLayer);
        if (!transform()->isAffine())
            flags.add(Seen3DTransformedLayer);
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(geometryMap, flags);

    if ((flags & UpdateCompositingLayers) && isComposited()) {
        OptionSet<RenderLayerBacking::UpdateAfterLayoutFlags> updateFlags;
        if (flags & NeedsFullRepaintInBacking)
            updateFlags.add(RenderLayerBacking::UpdateAfterLayoutFlags::NeedsFullRepaint);
        if (isUpdateRoot)
            updateFlags.add(RenderLayerBacking::UpdateAfterLayoutFlags::IsUpdateRoot);
        backing()->updateAfterLayout(updateFlags);
    }
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee) {
        // FIXME: would like to use SetForScope<> but it doesn't work with bitfields.
        bool oldUpdatingMarqueePosition = m_updatingMarqueePosition;
        m_updatingMarqueePosition = true;
        m_marquee->updateMarqueePosition();
        m_updatingMarqueePosition = oldUpdatingMarqueePosition;
    }

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());

    renderer().document().markers().invalidateRectsForAllMarkers();
}

LayoutRect RenderLayer::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect = renderer().repaintLayoutRects().m_repaintRect;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Don't include repaint rects for composited child layers; they will paint themselves and have a different origin.
        if (child->isComposited())
            continue;

        repaintRect.uniteIfNonZero(child->repaintRectIncludingNonCompositingDescendants());
    }
    return repaintRect;
}

void RenderLayer::setAncestorChainHasSelfPaintingLayerDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_hasSelfPaintingLayerDescendantDirty && layer->hasSelfPaintingLayerDescendant())
            break;

        layer->m_hasSelfPaintingLayerDescendantDirty = false;
        layer->m_hasSelfPaintingLayerDescendant = true;
    }
}

void RenderLayer::dirtyAncestorChainHasSelfPaintingLayerDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        layer->m_hasSelfPaintingLayerDescendantDirty = true;
        // If we have reached a self-painting layer, we know our parent should have a self-painting descendant
        // in this case, there is no need to dirty our ancestors further.
        if (layer->isSelfPaintingLayer()) {
            ASSERT(!parent() || parent()->m_hasSelfPaintingLayerDescendantDirty || parent()->hasSelfPaintingLayerDescendant());
            break;
        }
    }
}

void RenderLayer::computeRepaintRects(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap)
{
    ASSERT(!m_visibleContentStatusDirty);
    renderer().computeRepaintLayoutRects(repaintContainer, geometryMap);
}

void RenderLayer::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    computeRepaintRects(renderer().containerForRepaint());

    for (RenderLayer* layer = firstChild(); layer; layer = layer->nextSibling())
        layer->computeRepaintRectsIncludingDescendants();
}

void RenderLayer::clearRepaintRects()
{
    ASSERT(!m_visibleContentStatusDirty);

    renderer().clearRepaintLayoutRects();
}

void RenderLayer::updateLayerPositionsAfterDocumentScroll()
{
    ASSERT(this == renderer().view().layer());

    LOG(Scrolling, "RenderLayer::updateLayerPositionsAfterDocumentScroll");

    RenderGeometryMap geometryMap(UseTransforms);
    updateLayerPositionsAfterScroll(&geometryMap);
}

void RenderLayer::updateLayerPositionsAfterOverflowScroll()
{
    RenderGeometryMap geometryMap(UseTransforms);
    if (this != renderer().view().layer())
        geometryMap.pushMappingsToAncestor(parent(), nullptr);

    // FIXME: why is it OK to not check the ancestors of this layer in order to
    // initialize the HasSeenViewportConstrainedAncestor and HasSeenAncestorWithOverflowClip flags?
    updateLayerPositionsAfterScroll(&geometryMap, IsOverflowScroll);
}

void RenderLayer::updateLayerPositionsAfterScroll(RenderGeometryMap* geometryMap, OptionSet<UpdateLayerPositionsAfterScrollFlag> flags)
{
    // FIXME: This shouldn't be needed, but there are some corner cases where
    // these flags are still dirty. Update so that the check below is valid.
    updateDescendantDependentFlags();

    // If we have no visible content and no visible descendants, there is no point recomputing
    // our rectangles as they will be empty. If our visibility changes, we are expected to
    // recompute all our positions anyway.
    if (!m_hasVisibleDescendant && !m_hasVisibleContent)
        return;

    bool positionChanged = updateLayerPosition();
    if (positionChanged)
        flags.add(HasChangedAncestor);

    if (flags.containsAny({ HasChangedAncestor, HasSeenViewportConstrainedAncestor, IsOverflowScroll }))
        clearClipRects();

    if (renderer().style().hasViewportConstrainedPosition())
        flags.add(HasSeenViewportConstrainedAncestor);

    if (renderer().hasOverflowClip())
        flags.add(HasSeenAncestorWithOverflowClip);
    
    bool shouldComputeRepaintRects = (flags.contains(HasSeenViewportConstrainedAncestor) || flags.containsAll({ IsOverflowScroll, HasSeenAncestorWithOverflowClip })) && isSelfPaintingLayer();
    bool isVisuallyEmpty = !isVisuallyNonEmpty();
    bool shouldPushAndPopMappings = geometryMap && ((shouldComputeRepaintRects && !isVisuallyEmpty) || firstChild());
    if (shouldPushAndPopMappings)
        geometryMap->pushMappingsToAncestor(this, parent());

    if (shouldComputeRepaintRects) {
        // When scrolling, we don't compute repaint rects for visually non-empty layers.
        if (isVisuallyEmpty)
            clearRepaintRects();
        else // FIXME: We could track the repaint container as we walk down the tree.
            computeRepaintRects(renderer().containerForRepaint(), geometryMap);
    } else if (!renderer().view().frameView().platformWidget()) {
        // When ScrollView's m_paintsEntireContents flag flips due to layer backing changes, the repaint area transitions from
        // visual to layout overflow. When this happens the cached repaint rects become invalid and they need to be recomputed (see webkit.org/b/188121).
        // Check that our cached rects are correct.
        ASSERT(!renderer().hasRepaintLayoutRects() || renderer().repaintLayoutRects().m_repaintRect == renderer().clippedOverflowRectForRepaint(renderer().containerForRepaint()));
        ASSERT(!renderer().hasRepaintLayoutRects() || renderer().repaintLayoutRects().m_outlineBox == renderer().outlineBoundsForRepaint(renderer().containerForRepaint()));
    }
    
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScroll(geometryMap, flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (m_marquee) {
        bool oldUpdatingMarqueePosition = m_updatingMarqueePosition;
        m_updatingMarqueePosition = true;
        m_marquee->updateMarqueePosition();
        m_updatingMarqueePosition = oldUpdatingMarqueePosition;
    }

    if (shouldPushAndPopMappings)
        geometryMap->popMappingsToAncestor(parent());

    renderer().document().markers().invalidateRectsForAllMarkers();
}

#if ENABLE(CSS_COMPOSITING)

void RenderLayer::updateBlendMode()
{
    bool hadBlendMode = static_cast<BlendMode>(m_blendMode) != BlendMode::Normal;
    if (parent() && hadBlendMode != hasBlendMode()) {
        if (hasBlendMode())
            parent()->updateAncestorChainHasBlendingDescendants();
        else
            parent()->dirtyAncestorChainHasBlendingDescendants();
    }

    BlendMode newBlendMode = renderer().style().blendMode();
    if (newBlendMode != static_cast<BlendMode>(m_blendMode))
        m_blendMode = static_cast<unsigned>(newBlendMode);
}

void RenderLayer::updateAncestorChainHasBlendingDescendants()
{
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (!layer->hasNotIsolatedBlendingDescendantsStatusDirty() && layer->hasNotIsolatedBlendingDescendants())
            break;
        layer->m_hasNotIsolatedBlendingDescendants = true;
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = false;

        layer->updateSelfPaintingLayer();

        if (layer->isStackingContext())
            break;
    }
}

void RenderLayer::dirtyAncestorChainHasBlendingDescendants()
{
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (layer->hasNotIsolatedBlendingDescendantsStatusDirty())
            break;
        
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = true;

        if (layer->isStackingContext())
            break;
    }
}
#endif

void RenderLayer::updateTransform()
{
    bool hasTransform = renderer().hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = !!m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = std::make_unique<TransformationMatrix>();
        else
            m_transform = nullptr;
        
        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        clearClipRectsIncludingDescendants();
    }
    
    if (hasTransform) {
        RenderBox* box = renderBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style().applyTransform(*m_transform, snapRectToDevicePixels(box->borderBoxRect(), box->document().deviceScaleFactor()), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, canRender3DTransforms());
    }

    if (had3DTransform != has3DTransform())
        dirty3DTransformedDescendantStatus();
}

TransformationMatrix RenderLayer::currentTransform(RenderStyle::ApplyTransformOrigin applyOrigin) const
{
    if (!m_transform)
        return TransformationMatrix();
    
    RenderBox* box = renderBox();

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (auto* timeline = renderer().documentTimeline()) {
            if (timeline->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyTransform)) {
                TransformationMatrix currTransform;
                FloatRect pixelSnappedBorderRect = snapRectToDevicePixels(box->borderBoxRect(), box->document().deviceScaleFactor());
                std::unique_ptr<RenderStyle> style = timeline->animatedStyleForRenderer(renderer());
                style->applyTransform(currTransform, pixelSnappedBorderRect, applyOrigin);
                makeMatrixRenderable(currTransform, canRender3DTransforms());
                return currTransform;
            }
        }
    } else {
        if (renderer().animation().isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyTransform, AnimationBase::Running | AnimationBase::Paused)) {
            TransformationMatrix currTransform;
            FloatRect pixelSnappedBorderRect = snapRectToDevicePixels(box->borderBoxRect(), box->document().deviceScaleFactor());
            std::unique_ptr<RenderStyle> style = renderer().animation().animatedStyleForRenderer(renderer());
            style->applyTransform(currTransform, pixelSnappedBorderRect, applyOrigin);
            makeMatrixRenderable(currTransform, canRender3DTransforms());
            return currTransform;
        }
    }


    // m_transform includes transform-origin, so we need to recompute the transform here.
    if (applyOrigin == RenderStyle::ExcludeTransformOrigin) {
        TransformationMatrix currTransform;
        FloatRect pixelSnappedBorderRect = snapRectToDevicePixels(box->borderBoxRect(), box->document().deviceScaleFactor());
        box->style().applyTransform(currTransform, pixelSnappedBorderRect, RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }

    return *m_transform;
}

TransformationMatrix RenderLayer::renderableTransform(OptionSet<PaintBehavior> paintBehavior) const
{
    if (!m_transform)
        return TransformationMatrix();
    
    if (paintBehavior & PaintBehavior::FlattenCompositingLayers) {
        TransformationMatrix matrix = *m_transform;
        makeMatrixRenderable(matrix, false /* flatten 3d */);
        return matrix;
    }

    return *m_transform;
}

RenderLayer* RenderLayer::enclosingOverflowClipLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* layer = (includeSelf == IncludeSelf) ? this : parent();
    while (layer) {
        if (layer->renderer().hasOverflowClip())
            return const_cast<RenderLayer*>(layer);

        layer = layer->parent();
    }
    return nullptr;
}

// FIXME: This is terrible. Bring back a cached bit for this someday. This crawl is going to slow down all
// painting of content inside paginated layers.
bool RenderLayer::hasCompositedLayerInEnclosingPaginationChain() const
{
    // No enclosing layer means no compositing in the chain.
    if (!m_enclosingPaginationLayer)
        return false;
    
    // If the enclosing layer is composited, we don't have to check anything in between us and that
    // layer.
    if (m_enclosingPaginationLayer->isComposited())
        return true;

    // If we are the enclosing pagination layer, then we can't be composited or we'd have passed the
    // previous check.
    if (m_enclosingPaginationLayer == this)
        return false;

    // The enclosing paginated layer is our ancestor and is not composited, so we have to check
    // intermediate layers between us and the enclosing pagination layer. Start with our own layer.
    if (isComposited())
        return true;
    
    // For normal flow layers, we can recur up the layer tree.
    if (isNormalFlowOnly())
        return parent()->hasCompositedLayerInEnclosingPaginationChain();
    
    // Otherwise we have to go up the containing block chain. Find the first enclosing
    // containing block layer ancestor, and check that.
    for (const auto* containingBlock = renderer().containingBlock(); containingBlock && !is<RenderView>(*containingBlock); containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->hasLayer())
            return containingBlock->layer()->hasCompositedLayerInEnclosingPaginationChain();
    }
    return false;
}

void RenderLayer::updatePagination()
{
    m_enclosingPaginationLayer = nullptr;
    
    if (!parent())
        return;
    
    // Each layer that is inside a multicolumn flow thread has to be checked individually and
    // genuinely know if it is going to have to split itself up when painting only its contents (and not any other descendant
    // layers). We track an enclosingPaginationLayer instead of using a simple bit, since we want to be able to get back
    // to that layer easily.
    if (renderer().isInFlowRenderFragmentedFlow()) {
        m_enclosingPaginationLayer = makeWeakPtr(*this);
        return;
    }

    if (isNormalFlowOnly()) {
        // Content inside a transform is not considered to be paginated, since we simply
        // paint the transform multiple times in each column, so we don't have to use
        // fragments for the transformed content.
        if (parent()->hasTransform())
            m_enclosingPaginationLayer = nullptr;
        else
            m_enclosingPaginationLayer = makeWeakPtr(parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers));
        return;
    }

    // For the new columns code, we want to walk up our containing block chain looking for an enclosing layer. Once
    // we find one, then we just check its pagination status.
    for (const auto* containingBlock = renderer().containingBlock(); containingBlock && !is<RenderView>(*containingBlock); containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->hasLayer()) {
            // Content inside a transform is not considered to be paginated, since we simply
            // paint the transform multiple times in each column, so we don't have to use
            // fragments for the transformed content.
            if (containingBlock->layer()->hasTransform())
                m_enclosingPaginationLayer = nullptr;
            else
                m_enclosingPaginationLayer = makeWeakPtr(containingBlock->layer()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers));
            return;
        }
    }
}

void RenderLayer::setHasVisibleContent()
{ 
    if (m_hasVisibleContent && !m_visibleContentStatusDirty) {
        ASSERT(!parent() || parent()->hasVisibleDescendant());
        return;
    }

    m_visibleContentStatusDirty = false; 
    m_hasVisibleContent = true;
    computeRepaintRects(renderer().containerForRepaint());
    if (!isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking containers ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (RenderLayer* sc = stackingContext(); sc; sc = sc->stackingContext()) {
            sc->dirtyZOrderLists();
            if (sc->hasVisibleContent())
                break;
        }
    }

    if (parent())
        parent()->setAncestorChainHasVisibleDescendant();
}

void RenderLayer::dirtyVisibleContentStatus() 
{ 
    m_visibleContentStatusDirty = true; 
    if (parent())
        parent()->dirtyAncestorChainVisibleDescendantStatus();
}

void RenderLayer::dirtyAncestorChainVisibleDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer->m_visibleDescendantStatusDirty)
            break;

        layer->m_visibleDescendantStatusDirty = true;
    }
}

void RenderLayer::setAncestorChainHasVisibleDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_visibleDescendantStatusDirty && layer->hasVisibleDescendant())
            break;

        layer->m_hasVisibleDescendant = true;
        layer->m_visibleDescendantStatusDirty = false;
    }
}

void RenderLayer::updateDescendantDependentFlags()
{
    if (m_visibleDescendantStatusDirty || m_hasSelfPaintingLayerDescendantDirty || hasNotIsolatedBlendingDescendantsStatusDirty()) {
        bool hasVisibleDescendant = false;
        bool hasSelfPaintingLayerDescendant = false;
#if ENABLE(CSS_COMPOSITING)
        bool hasNotIsolatedBlendingDescendants = false;
#endif

        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateDescendantDependentFlags();

            hasVisibleDescendant |= child->m_hasVisibleContent || child->m_hasVisibleDescendant;
            hasSelfPaintingLayerDescendant |= child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant();
#if ENABLE(CSS_COMPOSITING)
            hasNotIsolatedBlendingDescendants |= child->hasBlendMode() || (child->hasNotIsolatedBlendingDescendants() && !child->isolatesBlending());
#endif

            bool allFlagsSet = hasVisibleDescendant && hasSelfPaintingLayerDescendant;
#if ENABLE(CSS_COMPOSITING)
            allFlagsSet &= hasNotIsolatedBlendingDescendants;
#endif
            if (allFlagsSet)
                break;
        }

        m_hasVisibleDescendant = hasVisibleDescendant;
        m_visibleDescendantStatusDirty = false;
        m_hasSelfPaintingLayerDescendant = hasSelfPaintingLayerDescendant;
        m_hasSelfPaintingLayerDescendantDirty = false;

#if ENABLE(CSS_COMPOSITING)
        m_hasNotIsolatedBlendingDescendants = hasNotIsolatedBlendingDescendants;
        if (m_hasNotIsolatedBlendingDescendantsStatusDirty) {
            m_hasNotIsolatedBlendingDescendantsStatusDirty = false;
            updateSelfPaintingLayer();
        }
#endif
    }

    if (m_visibleContentStatusDirty) {
        if (renderer().style().visibility() == Visibility::Visible)
            m_hasVisibleContent = true;
        else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            RenderObject* r = renderer().firstChild();
            while (r) {
                if (r->style().visibility() == Visibility::Visible && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                RenderObject* child = nullptr;
                if (!r->hasLayer() && (child = r->firstChildSlow()))
                    r = child;
                else if (r->nextSibling())
                    r = r->nextSibling();
                else {
                    do {
                        r = r->parent();
                        if (r == &renderer())
                            r = nullptr;
                    } while (r && !r->nextSibling());
                    if (r)
                        r = r->nextSibling();
                }
            }
        }    
        m_visibleContentStatusDirty = false; 
    }
}

void RenderLayer::dirty3DTransformedDescendantStatus()
{
    RenderLayer* curr = stackingContext();
    if (curr)
        curr->m_3DTransformedDescendantStatusDirty = true;
        
    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking containers.
    while (curr && curr->preserves3D()) {
        curr->m_3DTransformedDescendantStatusDirty = true;
        curr = curr->stackingContext();
    }
}

// Return true if this layer or any preserve-3d descendants have 3d.
bool RenderLayer::update3DTransformedDescendantStatus()
{
    if (m_3DTransformedDescendantStatusDirty) {
        m_has3DTransformedDescendant = false;

        updateZOrderLists();

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        if (auto* positiveZOrderList = posZOrderList()) {
            for (auto* layer : *positiveZOrderList)
                m_has3DTransformedDescendant |= layer->update3DTransformedDescendantStatus();
        }

        // Now check our negative z-index children.
        if (auto* negativeZOrderList = negZOrderList()) {
            for (auto* layer : *negativeZOrderList)
                m_has3DTransformedDescendant |= layer->update3DTransformedDescendantStatus();
        }
        
        m_3DTransformedDescendantStatusDirty = false;
    }
    
    // If we live in a 3d hierarchy, then the layer at the root of that hierarchy needs
    // the m_has3DTransformedDescendant set.
    if (preserves3D())
        return has3DTransform() || m_has3DTransformedDescendant;

    return has3DTransform();
}

bool RenderLayer::updateLayerPosition()
{
    LayoutPoint localPoint;
    LayoutSize inlineBoundingBoxOffset; // We don't put this into the RenderLayer x/y for inlines, so we need to subtract it out when done.
    if (renderer().isInline() && is<RenderInline>(renderer())) {
        auto& inlineFlow = downcast<RenderInline>(renderer());
        IntRect lineBox = inlineFlow.linesBoundingBox();
        setSize(lineBox.size());
        inlineBoundingBoxOffset = toLayoutSize(lineBox.location());
        localPoint += inlineBoundingBoxOffset;
    } else if (RenderBox* box = renderBox()) {
        // FIXME: Is snapping the size really needed here for the RenderBox case?
        setSize(snappedIntRect(box->frameRect()).size());
        box->applyTopLeftLocationOffset(localPoint);
    }

    if (!renderer().isOutOfFlowPositioned()) {
        auto* ancestor = renderer().parent();
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        while (ancestor && !ancestor->hasLayer()) {
            if (is<RenderBox>(*ancestor) && !is<RenderTableRow>(*ancestor)) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                localPoint += downcast<RenderBox>(*ancestor).topLeftLocationOffset();
            }
            ancestor = ancestor->parent();
        }
        if (is<RenderTableRow>(ancestor)) {
            // Put ourselves into the row coordinate space.
            localPoint -= downcast<RenderTableRow>(*ancestor).topLeftLocationOffset();
        }
    }
    
    // Subtract our parent's scroll offset.
    RenderLayer* positionedParent;
    if (renderer().isOutOfFlowPositioned() && (positionedParent = enclosingAncestorForPosition(renderer().style().position()))) {
        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        if (positionedParent->renderer().hasOverflowClip())
            localPoint -= toLayoutSize(positionedParent->scrollPosition());
        
        if (renderer().isOutOfFlowPositioned() && positionedParent->renderer().isInFlowPositioned() && is<RenderInline>(positionedParent->renderer())) {
            LayoutSize offset = downcast<RenderInline>(positionedParent->renderer()).offsetForInFlowPositionedInline(&downcast<RenderBox>(renderer()));
            localPoint += offset;
        }
    } else if (parent()) {
        if (parent()->renderer().hasOverflowClip())
            localPoint -= toLayoutSize(parent()->scrollPosition());
    }
    
    bool positionOrOffsetChanged = false;
    if (renderer().isInFlowPositioned()) {
        LayoutSize newOffset = downcast<RenderBoxModelObject>(renderer()).offsetForInFlowPosition();
        positionOrOffsetChanged = newOffset != m_offsetForInFlowPosition;
        m_offsetForInFlowPosition = newOffset;
        localPoint.move(m_offsetForInFlowPosition);
    } else {
        m_offsetForInFlowPosition = LayoutSize();
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint -= inlineBoundingBoxOffset;
    
    positionOrOffsetChanged |= location() != localPoint;
    setLocation(localPoint);
    return positionOrOffsetChanged;
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    RenderBox* box = renderBox();
    if (!box)
        return TransformationMatrix();
    
    if (!box->hasTransformRelatedProperty())
        return TransformationMatrix();

    const RenderStyle& style = box->style();
    if (!style.hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const FloatRect borderBox = snapRectToDevicePixels(box->borderBoxRect(), box->document().deviceScaleFactor());
    float perspectiveOriginX = floatValueForLength(style.perspectiveOriginX(), borderBox.width());
    float perspectiveOriginY = floatValueForLength(style.perspectiveOriginY(), borderBox.height());

    // A perspective origin of 0,0 makes the vanishing point in the center of the element.
    // We want it to be in the top-left, so subtract half the height and width.
    perspectiveOriginX -= borderBox.width() / 2.0f;
    perspectiveOriginY -= borderBox.height() / 2.0f;
    
    TransformationMatrix t;
    t.translate(perspectiveOriginX, perspectiveOriginY);
    t.applyPerspective(style.perspective());
    t.translate(-perspectiveOriginX, -perspectiveOriginY);
    
    return t;
}

FloatPoint RenderLayer::perspectiveOrigin() const
{
    if (!renderer().hasTransformRelatedProperty())
        return FloatPoint();

    const LayoutRect borderBox = downcast<RenderBox>(renderer()).borderBoxRect();
    const RenderStyle& style = renderer().style();

    return FloatPoint(floatValueForLength(style.perspectiveOriginX(), borderBox.width()),
                      floatValueForLength(style.perspectiveOriginY(), borderBox.height()));
}

RenderLayer* RenderLayer::stackingContext() const
{
    RenderLayer* layer = parent();
    while (layer && !layer->isStackingContext())
        layer = layer->parent();

    ASSERT(!layer || layer->isStackingContext());
    return layer;
}

static inline bool isContainerForPositioned(RenderLayer& layer, PositionType position)
{
    switch (position) {
    case PositionType::Fixed:
        return layer.renderer().canContainFixedPositionObjects();

    case PositionType::Absolute:
        return layer.renderer().canContainAbsolutelyPositionedObjects();
    
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

RenderLayer* RenderLayer::enclosingAncestorForPosition(PositionType position) const
{
    RenderLayer* curr = parent();
    while (curr && !isContainerForPositioned(*curr, position))
        curr = curr->parent();

    return curr;
}

static RenderLayer* parentLayerCrossFrame(const RenderLayer& layer)
{
    if (layer.parent())
        return layer.parent();

    HTMLFrameOwnerElement* ownerElement = layer.renderer().document().ownerElement();
    if (!ownerElement)
        return nullptr;

    RenderElement* ownerRenderer = ownerElement->renderer();
    if (!ownerRenderer)
        return nullptr;

    return ownerRenderer->enclosingLayer();
}

RenderLayer* RenderLayer::enclosingScrollableLayer() const
{
    for (RenderLayer* nextLayer = parentLayerCrossFrame(*this); nextLayer; nextLayer = parentLayerCrossFrame(*nextLayer)) {
        if (is<RenderBox>(nextLayer->renderer()) && downcast<RenderBox>(nextLayer->renderer()).canBeScrolledAndHasScrollableArea())
            return nextLayer;
    }

    return nullptr;
}

IntRect RenderLayer::scrollableAreaBoundingBox(bool* isInsideFixed) const
{
    return renderer().absoluteBoundingBoxRect(/* useTransforms */ true, isInsideFixed);
}

bool RenderLayer::isRubberBandInProgress() const
{
#if ENABLE(RUBBER_BANDING)
    if (!scrollsOverflow())
        return false;

    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();
#endif

    return false;
}

bool RenderLayer::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    return renderer().settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isRenderViewLayer() && !curr->transform())
        curr = curr->parent();

    return curr;
}

static inline const RenderLayer* compositingContainer(const RenderLayer& layer)
{
    return layer.isNormalFlowOnly() ? layer.parent() : layer.stackingContext();
}

inline bool RenderLayer::shouldRepaintAfterLayout() const
{
    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !isComposited() || backing()->paintsIntoCompositedAncestor();
}

bool compositedWithOwnBackingStore(const RenderLayer& layer)
{
    return layer.isComposited() && !layer.backing()->paintsIntoCompositedAncestor();
}

RenderLayer* RenderLayer::enclosingCompositingLayer(IncludeSelfOrNot includeSelf) const
{
    if (includeSelf == IncludeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(*this); curr; curr = compositingContainer(*curr)) {
        if (curr->isComposited())
            return const_cast<RenderLayer*>(curr);
    }
         
    return nullptr;
}

RenderLayer* RenderLayer::enclosingCompositingLayerForRepaint(IncludeSelfOrNot includeSelf) const
{
    if (includeSelf == IncludeSelf && compositedWithOwnBackingStore(*this))
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(*this); curr; curr = compositingContainer(*curr)) {
        if (compositedWithOwnBackingStore(*curr))
            return const_cast<RenderLayer*>(curr);
    }
         
    return nullptr;
}

RenderLayer* RenderLayer::enclosingFilterLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* curr = (includeSelf == IncludeSelf) ? this : parent();
    for (; curr; curr = curr->parent()) {
        if (curr->requiresFullLayerImageForFilters())
            return const_cast<RenderLayer*>(curr);
    }
    
    return nullptr;
}

RenderLayer* RenderLayer::enclosingFilterRepaintLayer() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if ((curr != this && curr->requiresFullLayerImageForFilters()) || compositedWithOwnBackingStore(*curr) || curr->isRenderViewLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return nullptr;
}

// FIXME: This neeeds a better name.
void RenderLayer::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect)
{
    ASSERT(requiresFullLayerImageForFilters());
    ASSERT(m_filters);

    if (rect.isEmpty())
        return;
    
    LayoutRect rectForRepaint = rect;
    renderer().style().filterOutsets().expandRect(rectForRepaint);

    m_filters->expandDirtySourceRect(rectForRepaint);
    
    RenderLayer* parentLayer = enclosingFilterRepaintLayer();
    ASSERT(parentLayer);
    FloatQuad repaintQuad(rectForRepaint);
    LayoutRect parentLayerRect = renderer().localToContainerQuad(repaintQuad, &parentLayer->renderer()).enclosingBoundingBox();

    if (parentLayer->isComposited()) {
        if (!parentLayer->backing()->paintsIntoWindow()) {
            parentLayer->setBackingNeedsRepaintInRect(parentLayerRect);
            return;
        }
        // If the painting goes to window, redirect the painting to the parent RenderView.
        parentLayer = renderer().view().layer();
        parentLayerRect = renderer().localToContainerQuad(repaintQuad, &parentLayer->renderer()).enclosingBoundingBox();
    }

    if (parentLayer->paintsWithFilters()) {
        parentLayer->setFilterBackendNeedsRepaintingInRect(parentLayerRect);
        return;        
    }
    
    if (parentLayer->isRenderViewLayer()) {
        downcast<RenderView>(parentLayer->renderer()).repaintViewRectangle(parentLayerRect);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

bool RenderLayer::hasAncestorWithFilterOutsets() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if (curr->renderer().style().hasFilterOutsets())
            return true;
    }
    return false;
}

RenderLayer* RenderLayer::clippingRootForPainting() const
{
    if (isComposited())
        return const_cast<RenderLayer*>(this);

    const RenderLayer* current = this;
    while (current) {
        if (current->isRenderViewLayer())
            return const_cast<RenderLayer*>(current);

        current = compositingContainer(*current);
        ASSERT(current);
        if (current->transform() || compositedWithOwnBackingStore(*current))
            return const_cast<RenderLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

LayoutPoint RenderLayer::absoluteToContents(const LayoutPoint& absolutePoint) const
{
    // We don't use convertToLayerCoords because it doesn't know about transforms
    return LayoutPoint(renderer().absoluteToLocal(absolutePoint, UseTransforms));
}

bool RenderLayer::cannotBlitToWindow() const
{
    if (isTransparent() || hasReflection() || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->cannotBlitToWindow();
}

RenderLayer* RenderLayer::transparentPaintingAncestor()
{
    if (isComposited())
        return nullptr;

    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->isComposited())
            return nullptr;
        if (curr->isTransparent())
            return curr;
    }
    return nullptr;
}

enum TransparencyClipBoxBehavior {
    PaintingTransparencyClipBox,
    HitTestingTransparencyClipBox
};

enum TransparencyClipBoxMode {
    DescendantsOfTransparencyClipBox,
    RootOfTransparencyClipBox
};

static LayoutRect transparencyClipBox(const RenderLayer&, const RenderLayer* rootLayer, TransparencyClipBoxBehavior, TransparencyClipBoxMode, OptionSet<PaintBehavior> = { });

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const RenderLayer& layer, const RenderLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior, OptionSet<PaintBehavior> paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer.renderer().hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking container. This means we can just walk the layer tree directly.
        for (RenderLayer* curr = layer.firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer.reflection() || layer.reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(*curr, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, paintBehavior));
        }
    }

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer.renderer().hasReflection()) {
        LayoutSize delta = layer.offsetFromAncestor(rootLayer);
        clipRect.move(-delta);
        clipRect.unite(layer.renderBox()->reflectedRect(clipRect));
        clipRect.move(delta);
    }
}

static LayoutRect transparencyClipBox(const RenderLayer& layer, const RenderLayer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
    TransparencyClipBoxMode transparencyMode, OptionSet<PaintBehavior> paintBehavior)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.
    
    if (rootLayer != &layer && ((transparencyBehavior == PaintingTransparencyClipBox && layer.paintsWithTransform(paintBehavior))
        || (transparencyBehavior == HitTestingTransparencyClipBox && layer.hasTransform()))) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        RenderLayer::PaginationInclusionMode mode = transparencyBehavior == HitTestingTransparencyClipBox ? RenderLayer::IncludeCompositedPaginatedLayers : RenderLayer::ExcludeCompositedPaginatedLayers;
        const RenderLayer* paginationLayer = transparencyMode == DescendantsOfTransparencyClipBox ? layer.enclosingPaginationLayer(mode) : nullptr;
        const RenderLayer* rootLayerForTransform = paginationLayer ? paginationLayer : rootLayer;
        LayoutSize delta = layer.offsetFromAncestor(rootLayerForTransform);

        TransformationMatrix transform;
        transform.translate(delta.width(), delta.height());
        transform.multiply(*layer.transform());

        // We don't use fragment boxes when collecting a transformed layer's bounding box, since it always
        // paints unfragmented.
        LayoutRect clipRect = layer.boundingBox(&layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, &layer, transparencyBehavior, paintBehavior);
        layer.renderer().style().filterOutsets().expandRect(clipRect);
        LayoutRect result = transform.mapRect(clipRect);
        if (!paginationLayer)
            return result;
        
        // We have to break up the transformed extent across our columns.
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        auto& enclosingFragmentedFlow = downcast<RenderFragmentedFlow>(paginationLayer->renderer());
        result = enclosingFragmentedFlow.fragmentsBoundingBox(result);
        result.move(paginationLayer->offsetFromAncestor(rootLayer));
        return result;
    }
    
    LayoutRect clipRect = layer.boundingBox(rootLayer, layer.offsetFromAncestor(rootLayer), transparencyBehavior == HitTestingTransparencyClipBox ? RenderLayer::UseFragmentBoxesIncludingCompositing : RenderLayer::UseFragmentBoxesExcludingCompositing);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, transparencyBehavior, paintBehavior);
    layer.renderer().style().filterOutsets().expandRect(clipRect);

    return clipRect;
}

static LayoutRect paintingExtent(const RenderLayer& currentLayer, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, OptionSet<PaintBehavior> paintBehavior)
{
    return intersection(transparencyClipBox(currentLayer, rootLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintBehavior), paintDirtyRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const LayoutRect& dirtyRect)
{
    if (context.paintingDisabled() || (paintsWithTransparency(paintingInfo.paintBehavior) && m_usedTransparency))
        return;

    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(context, paintingInfo, dirtyRect);
    
    if (paintsWithTransparency(paintingInfo.paintBehavior)) {
        ASSERT(isStackingContext());
        m_usedTransparency = true;
        context.save();
        LayoutRect adjustedClipRect = paintingExtent(*this, paintingInfo.rootLayer, dirtyRect, paintingInfo.paintBehavior);
        adjustedClipRect.move(paintingInfo.subpixelOffset);
        FloatRect pixelSnappedClipRect = snapRectToDevicePixels(adjustedClipRect, renderer().document().deviceScaleFactor());
        context.clip(pixelSnappedClipRect);

#if ENABLE(CSS_COMPOSITING)
        bool usesCompositeOperation = hasBlendMode() && !(renderer().isSVGRoot() && parent() && parent()->isRenderViewLayer());
        if (usesCompositeOperation)
            context.setCompositeOperation(context.compositeOperation(), blendMode());
#endif

        context.beginTransparencyLayer(renderer().opacity());

#if ENABLE(CSS_COMPOSITING)
        if (usesCompositeOperation)
            context.setCompositeOperation(context.compositeOperation(), BlendMode::Normal);
#endif

#ifdef REVEAL_TRANSPARENCY_LAYERS
        context.setFillColor(Color(0.0f, 0.0f, 0.5f, 0.2f));
        context.fillRect(pixelSnappedClipRect);
#endif
    }
}

#if PLATFORM(IOS)
void RenderLayer::willBeDestroyed()
{
    if (RenderLayerBacking* layerBacking = backing())
        layerBacking->layerWillBeDestroyed();
}
#endif

bool RenderLayer::isDescendantOf(const RenderLayer& layer) const
{
    for (auto* ancestor = this; ancestor; ancestor = ancestor->parent()) {
        if (&layer == ancestor)
            return true;
    }
    return false;
}

void RenderLayer::addChild(RenderLayer* child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
        ASSERT(prevSibling != child);
    } else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
        ASSERT(beforeChild != child);
    } else
        setLastChild(child);

    child->setParent(this);

    if (child->isNormalFlowOnly())
        dirtyNormalFlowList();

    if (!child->isNormalFlowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained. The stackingContext() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child->dirtyStackingContextZOrderLists();
    }

    child->updateDescendantDependentFlags();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

#if ENABLE(CSS_COMPOSITING)
    if (child->hasBlendMode() || (child->hasNotIsolatedBlendingDescendants() && !child->isolatesBlending()))
        updateAncestorChainHasBlendingDescendants();
#endif

    compositor().layerWasAdded(*this, *child);
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
    if (!renderer().renderTreeBeingDestroyed())
        compositor().layerWillBeRemoved(*this, *oldChild);

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    if (oldChild->isNormalFlowOnly())
        dirtyNormalFlowList();
    if (!oldChild->isNormalFlowOnly() || oldChild->firstChild()) { 
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the |stackingContext| value.
        oldChild->dirtyStackingContextZOrderLists();
    }

    oldChild->setPreviousSibling(nullptr);
    oldChild->setNextSibling(nullptr);
    oldChild->setParent(nullptr);
    
    oldChild->updateDescendantDependentFlags();
    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild->isSelfPaintingLayer() || oldChild->hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

#if ENABLE(CSS_COMPOSITING)
    if (oldChild->hasBlendMode() || (oldChild->hasNotIsolatedBlendingDescendants() && !oldChild->isolatesBlending()))
        dirtyAncestorChainHasBlendingDescendants();
#endif

    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    // Mark that we are about to lose our layer. This makes render tree
    // walks ignore this layer while we're removing it.
    renderer().setHasLayer(false);

    compositor().layerWillBeRemoved(*m_parent, *this);

    // Dirty the clip rects.
    clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (reflection())
        removeChild(reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        m_parent->addChild(current, nextSib);
        current->setRepaintStatus(NeedsFullRepaint);
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(this);
    renderer().destroyLayer();
}

void RenderLayer::insertOnlyThisLayer()
{
    if (!m_parent && renderer().parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        RenderLayer* parentLayer = renderer().parent()->enclosingLayer();
        ASSERT(parentLayer);
        RenderLayer* beforeChild = parentLayer->reflectionLayer() != this ? renderer().parent()->findNextLayer(parentLayer, &renderer()) : nullptr;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (auto& child : childrenOfType<RenderElement>(renderer()))
        child.moveLayers(m_parent, this);

    // Clear out all the clip rects.
    clearClipRectsIncludingDescendants();
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& roundedLocation, ColumnOffsetAdjustment adjustForColumns) const
{
    LayoutPoint location = convertToLayerCoords(ancestorLayer, roundedLocation, adjustForColumns);
    roundedLocation = roundedIntPoint(location);
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const RenderLayer* accumulateOffsetTowardsAncestor(const RenderLayer* layer, const RenderLayer* ancestorLayer, LayoutPoint& location, RenderLayer::ColumnOffsetAdjustment adjustForColumns)
{
    ASSERT(ancestorLayer != layer);

    const RenderLayerModelObject& renderer = layer->renderer();
    auto position = renderer.style().position();

    // FIXME: Special casing RenderFragmentedFlow so much for fixed positioning here is not great.
    RenderFragmentedFlow* fixedFragmentedFlowContainer = position == PositionType::Fixed ? renderer.enclosingFragmentedFlow() : nullptr;
    if (fixedFragmentedFlowContainer && !fixedFragmentedFlowContainer->isOutOfFlowPositioned())
        fixedFragmentedFlowContainer = nullptr;

    // FIXME: Positioning of out-of-flow(fixed, absolute) elements collected in a RenderFragmentedFlow
    // may need to be revisited in a future patch.
    // If the fixed renderer is inside a RenderFragmentedFlow, we should not compute location using localToAbsolute,
    // since localToAbsolute maps the coordinates from named flow to regions coordinates and regions can be
    // positioned in a completely different place in the viewport (RenderView).
    if (position == PositionType::Fixed && !fixedFragmentedFlowContainer && (!ancestorLayer || ancestorLayer == renderer.view().layer())) {
        // If the fixed layer's container is the root, just add in the offset of the view. We can obtain this by calling
        // localToAbsolute() on the RenderView.
        FloatPoint absPos = renderer.localToAbsolute(FloatPoint(), IsFixed);
        location += LayoutSize(absPos.x(), absPos.y());
        return ancestorLayer;
    }

    // For the fixed positioned elements inside a render flow thread, we should also skip the code path below
    // Otherwise, for the case of ancestorLayer == rootLayer and fixed positioned element child of a transformed
    // element in render flow thread, we will hit the fixed positioned container before hitting the ancestor layer.
    if (position == PositionType::Fixed && !fixedFragmentedFlowContainer) {
        // For a fixed layers, we need to walk up to the root to see if there's a fixed position container
        // (e.g. a transformed layer). It's an error to call offsetFromAncestor() across a layer with a transform,
        // so we should always find the ancestor at or before we find the fixed position container.
        RenderLayer* fixedPositionContainerLayer = nullptr;
        bool foundAncestor = false;
        for (RenderLayer* currLayer = layer->parent(); currLayer; currLayer = currLayer->parent()) {
            if (currLayer == ancestorLayer)
                foundAncestor = true;

            if (isContainerForPositioned(*currLayer, PositionType::Fixed)) {
                fixedPositionContainerLayer = currLayer;
                ASSERT_UNUSED(foundAncestor, foundAncestor);
                break;
            }
        }
        
        ASSERT(fixedPositionContainerLayer); // We should have hit the RenderView's layer at least.

        if (fixedPositionContainerLayer != ancestorLayer) {
            LayoutSize fixedContainerCoords = layer->offsetFromAncestor(fixedPositionContainerLayer);
            LayoutSize ancestorCoords = ancestorLayer->offsetFromAncestor(fixedPositionContainerLayer);
            location += (fixedContainerCoords - ancestorCoords);
            return ancestorLayer;
        }
    }

    if (position == PositionType::Fixed && fixedFragmentedFlowContainer) {
        ASSERT(ancestorLayer);
        if (ancestorLayer->isOutOfFlowRenderFragmentedFlow()) {
            location += toLayoutSize(layer->location());
            return ancestorLayer;
        }

        if (ancestorLayer == renderer.view().layer()) {
            // Add location in flow thread coordinates.
            location += toLayoutSize(layer->location());

            // Add flow thread offset in view coordinates since the view may be scrolled.
            FloatPoint absPos = renderer.view().localToAbsolute(FloatPoint(), IsFixed);
            location += LayoutSize(absPos.x(), absPos.y());
            return ancestorLayer;
        }
    }

    RenderLayer* parentLayer;
    if (position == PositionType::Absolute || position == PositionType::Fixed) {
        // Do what enclosingAncestorForPosition() does, but check for ancestorLayer along the way.
        parentLayer = layer->parent();
        bool foundAncestorFirst = false;
        while (parentLayer) {
            // RenderFragmentedFlow is a positioned container, child of RenderView, positioned at (0,0).
            // This implies that, for out-of-flow positioned elements inside a RenderFragmentedFlow,
            // we are bailing out before reaching root layer.
            if (isContainerForPositioned(*parentLayer, position))
                break;

            if (parentLayer == ancestorLayer) {
                foundAncestorFirst = true;
                break;
            }

            parentLayer = parentLayer->parent();
        }

        // We should not reach RenderView layer past the RenderFragmentedFlow layer for any
        // children of the RenderFragmentedFlow.
        if (renderer.enclosingFragmentedFlow() && !layer->isOutOfFlowRenderFragmentedFlow())
            ASSERT(parentLayer != renderer.view().layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingAncestorForPosition and subtract.
            RenderLayer* positionedAncestor = parentLayer->enclosingAncestorForPosition(position);
            LayoutSize thisCoords = layer->offsetFromAncestor(positionedAncestor);
            LayoutSize ancestorCoords = ancestorLayer->offsetFromAncestor(positionedAncestor);
            location += (thisCoords - ancestorCoords);
            return ancestorLayer;
        }
    } else
        parentLayer = layer->parent();
    
    if (!parentLayer)
        return nullptr;

    location += toLayoutSize(layer->location());

    if (adjustForColumns == RenderLayer::AdjustForColumns) {
        if (RenderLayer* parentLayer = layer->parent()) {
            if (is<RenderMultiColumnFlow>(parentLayer->renderer())) {
                RenderFragmentContainer* fragment = downcast<RenderMultiColumnFlow>(parentLayer->renderer()).physicalTranslationFromFlowToFragment(location);
                if (fragment)
                    location.moveBy(fragment->topLeftLocation() + -parentLayer->renderBox()->topLeftLocation());
            }
        }
    }

    return parentLayer;
}

LayoutPoint RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, const LayoutPoint& location, ColumnOffsetAdjustment adjustForColumns) const
{
    if (ancestorLayer == this)
        return location;

    const RenderLayer* currLayer = this;
    LayoutPoint locationInLayerCoords = location;
    while (currLayer && currLayer != ancestorLayer)
        currLayer = accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, locationInLayerCoords, adjustForColumns);
    return locationInLayerCoords;
}

LayoutSize RenderLayer::offsetFromAncestor(const RenderLayer* ancestorLayer, ColumnOffsetAdjustment adjustForColumns) const
{
    return toLayoutSize(convertToLayerCoords(ancestorLayer, LayoutPoint(), adjustForColumns));
}

bool RenderLayer::canUseAcceleratedTouchScrolling() const
{
#if PLATFORM(IOS) && ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    return scrollsOverflow() && (renderer().style().useTouchOverflowScrolling() || renderer().settings().alwaysUseAcceleratedOverflowScroll());
#else
    return false;
#endif
}

bool RenderLayer::hasTouchScrollableOverflow() const
{
    return canUseAcceleratedTouchScrolling() && (hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

#if ENABLE(IOS_TOUCH_EVENTS)
bool RenderLayer::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    // If we have accelerated scrolling, let the scrolling be handled outside of WebKit.
    if (hasTouchScrollableOverflow())
        return false;

    return ScrollableArea::handleTouchEvent(touchEvent);
}

void RenderLayer::registerAsTouchEventListenerForScrolling()
{
    if (!renderer().element() || m_registeredAsTouchEventListenerForScrolling)
        return;
    
    renderer().document().addTouchEventHandler(*renderer().element());
    m_registeredAsTouchEventListenerForScrolling = true;
}

void RenderLayer::unregisterAsTouchEventListenerForScrolling()
{
    if (!renderer().element() || !m_registeredAsTouchEventListenerForScrolling)
        return;

    renderer().document().removeTouchEventHandler(*renderer().element());
    m_registeredAsTouchEventListenerForScrolling = false;
}
#endif // ENABLE(IOS_TOUCH_EVENTS)

// FIXME: this is only valid after we've made layers.
bool RenderLayer::usesCompositedScrolling() const
{
    return isComposited() && backing()->hasScrollingLayer();
}

// FIXME: this is only valid after we've made layers.
bool RenderLayer::usesAsyncScrolling() const
{
    return canUseAcceleratedTouchScrolling() && usesCompositedScrolling();
}

static inline int adjustedScrollDelta(int beginningDelta)
{
    // This implemention matches Firefox's.
    // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
    const int speedReducer = 12;

    int adjustedDelta = beginningDelta / speedReducer;
    if (adjustedDelta > 1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(adjustedDelta))) - 1;
    else if (adjustedDelta < -1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(-adjustedDelta))) + 1;

    return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta)
{
    return IntSize(adjustedScrollDelta(delta.width()), adjustedScrollDelta(delta.height()));
}

void RenderLayer::panScrollFromPoint(const IntPoint& sourcePoint)
{
    IntPoint lastKnownMousePosition = renderer().frame().eventHandler().lastKnownMousePosition();
    
    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    IntSize delta = lastKnownMousePosition - sourcePoint;

    if (abs(delta.width()) <= ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= ScrollView::noPanScrollRadius)
        delta.setHeight(0);

    scrollByRecursively(adjustedScrollDelta(delta));
}

// FIXME: unify with the scrollRectToVisible() code below.
void RenderLayer::scrollByRecursively(const IntSize& delta, ScrollableArea** scrolledArea)
{
    if (delta.isZero())
        return;

    bool restrictedByLineClamp = false;
    if (renderer().parent())
        restrictedByLineClamp = !renderer().parent()->style().lineClamp().isNone();

    if (renderer().hasOverflowClip() && !restrictedByLineClamp) {
        ScrollOffset newScrollOffset = scrollOffset() + delta;
        scrollToOffset(newScrollOffset);
        if (scrolledArea)
            *scrolledArea = this;

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        IntSize remainingScrollOffset = newScrollOffset - scrollOffset();
        if (!remainingScrollOffset.isZero() && renderer().parent()) {
            if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
                scrollableLayer->scrollByRecursively(remainingScrollOffset, scrolledArea);

            renderer().frame().eventHandler().updateAutoscrollRenderer();
        }
    } else {
        // If we are here, we were called on a renderer that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        renderer().view().frameView().scrollBy(delta);
        if (scrolledArea)
            *scrolledArea = &renderer().view().frameView();

        // FIXME: If we didn't scroll the whole way, do we want to try looking at the frames ownerElement? 
        // https://bugs.webkit.org/show_bug.cgi?id=28237
    }
}

void RenderLayer::setPostLayoutScrollPosition(std::optional<ScrollPosition> position)
{
    m_postLayoutScrollPosition = position;
}

void RenderLayer::applyPostLayoutScrollPositionIfNeeded()
{
    if (!m_postLayoutScrollPosition)
        return;

    scrollToOffset(scrollOffsetFromPosition(m_postLayoutScrollPosition.value()));
    m_postLayoutScrollPosition = std::nullopt;
}

void RenderLayer::scrollToXPosition(int x, ScrollClamping clamping)
{
    ScrollPosition position(x, m_scrollPosition.y());
    scrollToOffset(scrollOffsetFromPosition(position), clamping);
}

void RenderLayer::scrollToYPosition(int y, ScrollClamping clamping)
{
    ScrollPosition position(m_scrollPosition.x(), y);
    scrollToOffset(scrollOffsetFromPosition(position), clamping);
}

ScrollOffset RenderLayer::clampScrollOffset(const ScrollOffset& scrollOffset) const
{
    return scrollOffset.constrainedBetween(IntPoint(), maximumScrollOffset());
}

void RenderLayer::scrollToOffset(const ScrollOffset& scrollOffset, ScrollClamping clamping)
{
    ScrollOffset newScrollOffset = clamping == ScrollClamping::Clamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (newScrollOffset != this->scrollOffset())
        scrollToOffsetWithoutAnimation(newScrollOffset, clamping);
}

void RenderLayer::scrollTo(const ScrollPosition& position)
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "RenderLayer::scrollTo " << position);

    ScrollPosition newPosition = position;
    if (!box->isHTMLMarquee()) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
#if PLATFORM(IOS)
        if (adjustForIOSCaretWhenScrolling()) {
            // FIXME: It's not clear what this code is trying to do. Behavior seems reasonable with it removed.
            int maxOffset = scrollWidth() - roundToInt(box->clientWidth());
            ScrollOffset newOffset = scrollOffsetFromPosition(newPosition);
            int scrollXOffset = newOffset.x();
            if (scrollXOffset > maxOffset - caretWidth) {
                scrollXOffset += caretWidth;
                if (scrollXOffset <= caretWidth)
                    scrollXOffset = 0;
            } else if (scrollXOffset < m_scrollPosition.x() - caretWidth)
                scrollXOffset -= caretWidth;

            newOffset.setX(scrollXOffset);
            newPosition = scrollPositionFromOffset(newOffset);
        }
#endif
    }
    
    if (m_scrollPosition == newPosition) {
#if PLATFORM(IOS)
        if (m_requiresScrollBoundsOriginUpdate)
            updateCompositingLayersAfterScroll();
#endif
        return;
    }

    m_scrollPosition = newPosition;

    RenderView& view = renderer().view();

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    if (!view.frameView().layoutContext().isInRenderTreeLayout()) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        updateLayerPositionsAfterOverflowScroll();
        // Update regions, scrolling may change the clip of a particular region.
#if ENABLE(DASHBOARD_SUPPORT)
        view.frameView().updateAnnotatedRegions();
#endif
        view.frameView().scheduleUpdateWidgetPositions();

        if (!m_updatingMarqueePosition) {
            // Avoid updating compositing layers if, higher on the stack, we're already updating layer
            // positions. Updating layer positions requires a full walk of up-to-date RenderLayers, and
            // in this case we're still updating their positions; we'll update compositing layers later
            // when that completes.
            updateCompositingLayersAfterScroll();
        }

#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
        renderer().document().setTouchEventRegionsNeedUpdate();
#endif
        DebugPageOverlays::didLayout(renderer().frame());
    }

    Frame& frame = renderer().frame();
    RenderLayerModelObject* repaintContainer = renderer().containerForRepaint();
    // The caret rect needs to be invalidated after scrolling
    frame.selection().setCaretRectNeedsUpdate();
    
    LayoutRect rectForRepaint = renderer().hasRepaintLayoutRects() ? renderer().repaintLayoutRects().m_repaintRect : renderer().clippedOverflowRectForRepaint(repaintContainer);

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(rectForRepaint);
    if (repaintContainer)
        quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame.eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresRepaint = true;
    if (compositor().inCompositingMode() && usesCompositedScrolling())
        requiresRepaint = false;

    // Just schedule a full repaint of our object.
    if (requiresRepaint)
        renderer().repaintUsingContainer(repaintContainer, rectForRepaint);

    // Schedule the scroll and scroll-related DOM events.
    if (Element* element = renderer().element())
        element->document().eventQueue().enqueueOrDispatchScrollEvent(*element);

    if (scrollsOverflow())
        view.frameView().didChangeScrollOffset();

    view.frameView().viewportContentsChanged();
}

static inline bool frameElementAndViewPermitScroll(HTMLFrameElementBase* frameElementBase, FrameView& frameView)
{
    // If scrollbars aren't explicitly forbidden, permit scrolling.
    if (frameElementBase && frameElementBase->scrollingMode() != ScrollbarAlwaysOff)
        return true;

    // If scrollbars are forbidden, user initiated scrolls should obviously be ignored.
    if (frameView.wasScrolledByUser())
        return false;

    // Forbid autoscrolls when scrollbars are off, but permits other programmatic scrolls,
    // like navigation to an anchor.
    return !frameView.frame().eventHandler().autoscrollInProgress();
}

bool RenderLayer::allowsCurrentScroll() const
{
    if (!renderer().hasOverflowClip())
        return false;

    // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
    // FIXME: Is this still needed? It used to be relevant for Safari RSS.
    if (renderer().parent() && !renderer().parent()->style().lineClamp().isNone())
        return false;

    RenderBox* box = renderBox();
    ASSERT(box); // Only boxes can have overflowClip set.

    if (renderer().frame().eventHandler().autoscrollInProgress()) {
        // The "programmatically" here is misleading; this asks whether the box has scrollable overflow,
        // or is a special case like a form control.
        return box->canBeProgramaticallyScrolled();
    }

    // Programmatic scrolls can scroll overflow:hidden.
    return box->hasHorizontalOverflow() || box->hasVerticalOverflow();
}

void RenderLayer::scrollRectToVisible(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions& options)
{
    LOG_WITH_STREAM(Scrolling, stream << "Layer " << this << " scrollRectToVisible " << absoluteRect);

    RenderLayer* parentLayer = nullptr;
    LayoutRect newRect = absoluteRect;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer or the layer's renderer().
    FrameView& frameView = renderer().view().frameView();

    if (renderer().parent())
        parentLayer = renderer().parent()->enclosingLayer();

    if (allowsCurrentScroll()) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        RenderBox* box = renderBox();
        ASSERT(box);
        LayoutRect localExposeRect(box->absoluteToLocalQuad(FloatQuad(FloatRect(absoluteRect))).boundingBox());
        LayoutRect layerBounds(0, 0, box->clientWidth(), box->clientHeight());
        LayoutRect revealRect = getRectToExpose(layerBounds, localExposeRect, insideFixed, options.alignX, options.alignY);

        ScrollOffset clampedScrollOffset = clampScrollOffset(scrollOffset() + toIntSize(roundedIntRect(revealRect).location()));
        if (clampedScrollOffset != scrollOffset()) {
            ScrollOffset oldScrollOffset = scrollOffset();
            scrollToOffset(clampedScrollOffset);
            IntSize scrollOffsetDifference = scrollOffset() - oldScrollOffset;
            localExposeRect.move(-scrollOffsetDifference);
            newRect = LayoutRect(box->localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
        }
    } else if (!parentLayer && renderer().isRenderView()) {
        HTMLFrameOwnerElement* ownerElement = renderer().document().ownerElement();

        if (ownerElement && ownerElement->renderer()) {
            HTMLFrameElementBase* frameElementBase = nullptr;

            if (is<HTMLFrameElementBase>(*ownerElement))
                frameElementBase = downcast<HTMLFrameElementBase>(ownerElement);

            if (frameElementAndViewPermitScroll(frameElementBase, frameView)) {
                // If this assertion fires we need to protect the ownerElement from being destroyed.
                ScriptDisallowedScope::InMainThread scriptDisallowedScope;

                LayoutRect viewRect = frameView.visibleContentRect(LegacyIOSDocumentVisibleRect);
                LayoutRect exposeRect = getRectToExpose(viewRect, absoluteRect, insideFixed, options.alignX, options.alignY);

                IntPoint scrollOffset(roundedIntPoint(exposeRect.location()));
                // Adjust offsets if they're outside of the allowable range.
                scrollOffset = scrollOffset.constrainedBetween(IntPoint(), IntPoint(frameView.contentsSize()));
                frameView.setScrollPosition(scrollOffset);

                if (options.shouldAllowCrossOriginScrolling == ShouldAllowCrossOriginScrolling::Yes || frameView.safeToPropagateScrollToParent()) {
                    parentLayer = ownerElement->renderer()->enclosingLayer();
                    // Convert the rect into the coordinate space of the parent frame's document.
                    newRect = frameView.contentsToContainingViewContents(enclosingIntRect(newRect));
                    insideFixed = false; // FIXME: ideally need to determine if this <iframe> is inside position:fixed.
                } else
                    parentLayer = nullptr;
            }
        } else {
            if (options.revealMode == SelectionRevealMode::RevealUpToMainFrame && frameView.frame().isMainFrame())
                return;

#if !PLATFORM(IOS)
            LayoutRect viewRect = frameView.visibleContentRect();
#else
            LayoutRect viewRect = frameView.unobscuredContentRect();
#endif
            // Move the target rect into "scrollView contents" coordinates.
            LayoutRect targetRect = absoluteRect;
            targetRect.move(0, frameView.headerHeight());

            LayoutRect revealRect = getRectToExpose(viewRect, targetRect, insideFixed, options.alignX, options.alignY);
            
            frameView.setScrollPosition(roundedIntPoint(revealRect.location()));

            // This is the outermost view of a web page, so after scrolling this view we
            // scroll its container by calling Page::scrollRectIntoView.
            // This only has an effect on the Mac platform in applications
            // that put web views into scrolling containers, such as Mac OS X Mail.
            // The canAutoscroll function in EventHandler also knows about this.
            page().chrome().scrollRectIntoView(snappedIntRect(absoluteRect));
        }
    }
    
    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, insideFixed, options);
}

void RenderLayer::updateCompositingLayersAfterScroll()
{
    if (compositor().inCompositingMode()) {
        // Our stacking container is guaranteed to contain all of our descendants that may need
        // repositioning, so update compositing layers from there.
        if (RenderLayer* compositingAncestor = stackingContext()->enclosingCompositingLayer()) {
            if (usesCompositedScrolling())
                compositor().updateCompositingLayers(CompositingUpdateType::OnCompositedScroll, compositingAncestor);
            else
                compositor().updateCompositingLayers(CompositingUpdateType::OnScroll, compositingAncestor);
        }
    }
}

LayoutRect RenderLayer::getRectToExpose(const LayoutRect &visibleRect, const LayoutRect &exposeRect, bool insideFixed, const ScrollAlignment& alignX, const ScrollAlignment& alignY) const
{
    FrameView& frameView = renderer().view().frameView();
    if (renderer().isRenderView() && insideFixed) {
        // If the element is inside position:fixed and we're not scaled, no amount of scrolling is going to move things around.
        if (frameView.frameScaleFactor() == 1)
            return visibleRect;

        if (renderer().settings().visualViewportEnabled()) {
            // exposeRect is in absolute coords, affected by page scale. Unscale it.
            LayoutRect unscaledExposeRect = exposeRect;
            unscaledExposeRect.scale(1 / frameView.frameScaleFactor());
            unscaledExposeRect.move(0, -frameView.headerHeight());

            // These are both in unscaled coordinates.
            LayoutRect layoutViewport = frameView.layoutViewportRect();
            LayoutRect visualViewport = frameView.visualViewportRect();

            // The rect to expose may be partially offscreen, which we can't do anything about with position:fixed.
            unscaledExposeRect.intersect(layoutViewport);
            // Make sure it's not larger than the visual viewport; if so, we'll just move to the top left.
            unscaledExposeRect.setSize(unscaledExposeRect.size().shrunkTo(visualViewport.size()));

            // Compute how much we have to move the visualViewport to reveal the part of the layoutViewport that contains exposeRect.
            LayoutRect requiredVisualViewport = getRectToExpose(visualViewport, unscaledExposeRect, false, alignX, alignY);
            // Scale it back up.
            requiredVisualViewport.scale(frameView.frameScaleFactor());
            requiredVisualViewport.move(0, frameView.headerHeight());
            return requiredVisualViewport;
        }
    }

    // Determine the appropriate X behavior.
    ScrollAlignment::Behavior scrollX;
    LayoutRect exposeRectX(exposeRect.x(), visibleRect.y(), exposeRect.width(), visibleRect.height());
    LayoutUnit intersectWidth = intersection(visibleRect, exposeRectX).width();
    if (intersectWidth == exposeRect.width() || intersectWidth >= MIN_INTERSECT_FOR_REVEAL)
        // If the rectangle is fully visible, use the specified visible behavior.
        // If the rectangle is partially visible, but over a certain threshold,
        // then treat it as fully visible to avoid unnecessary horizontal scrolling
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
    else if (intersectWidth == visibleRect.width()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
        if (scrollX == ScrollAlignment::Behavior::AlignCenter)
            scrollX = ScrollAlignment::Behavior::NoScroll;
    } else if (intersectWidth > 0)
        // If the rectangle is partially visible, but not above the minimum threshold, use the specified partial behavior
        scrollX = ScrollAlignment::getPartialBehavior(alignX);
    else
        scrollX = ScrollAlignment::getHiddenBehavior(alignX);
    // If we're trying to align to the closest edge, and the exposeRect is further right
    // than the visibleRect, and not bigger than the visible area, then align with the right.
    if (scrollX == ScrollAlignment::Behavior::AlignToClosestEdge && exposeRect.maxX() > visibleRect.maxX() && exposeRect.width() < visibleRect.width())
        scrollX = ScrollAlignment::Behavior::AlignRight;

    // Given the X behavior, compute the X coordinate.
    LayoutUnit x;
    if (scrollX == ScrollAlignment::Behavior::NoScroll)
        x = visibleRect.x();
    else if (scrollX == ScrollAlignment::Behavior::AlignRight)
        x = exposeRect.maxX() - visibleRect.width();
    else if (scrollX == ScrollAlignment::Behavior::AlignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleRect.width()) / 2;
    else
        x = exposeRect.x();

    // Determine the appropriate Y behavior.
    ScrollAlignment::Behavior scrollY;
    LayoutRect exposeRectY(visibleRect.x(), exposeRect.y(), visibleRect.width(), exposeRect.height());
    LayoutUnit intersectHeight = intersection(visibleRect, exposeRectY).height();
    if (intersectHeight == exposeRect.height())
        // If the rectangle is fully visible, use the specified visible behavior.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
    else if (intersectHeight == visibleRect.height()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
        if (scrollY == ScrollAlignment::Behavior::AlignCenter)
            scrollY = ScrollAlignment::Behavior::NoScroll;
    } else if (intersectHeight > 0)
        // If the rectangle is partially visible, use the specified partial behavior
        scrollY = ScrollAlignment::getPartialBehavior(alignY);
    else
        scrollY = ScrollAlignment::getHiddenBehavior(alignY);
    // If we're trying to align to the closest edge, and the exposeRect is further down
    // than the visibleRect, and not bigger than the visible area, then align with the bottom.
    if (scrollY == ScrollAlignment::Behavior::AlignToClosestEdge && exposeRect.maxY() > visibleRect.maxY() && exposeRect.height() < visibleRect.height())
        scrollY = ScrollAlignment::Behavior::AlignBottom;

    // Given the Y behavior, compute the Y coordinate.
    LayoutUnit y;
    if (scrollY == ScrollAlignment::Behavior::NoScroll)
        y = visibleRect.y();
    else if (scrollY == ScrollAlignment::Behavior::AlignBottom)
        y = exposeRect.maxY() - visibleRect.height();
    else if (scrollY == ScrollAlignment::Behavior::AlignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleRect.height()) / 2;
    else
        y = exposeRect.y();

    return LayoutRect(LayoutPoint(x, y), visibleRect.size());
}

void RenderLayer::autoscroll(const IntPoint& positionInWindow)
{
    IntPoint currentDocumentPosition = renderer().view().frameView().windowToContents(positionInWindow);
    scrollRectToVisible(LayoutRect(currentDocumentPosition, LayoutSize(1, 1)), false, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
}

bool RenderLayer::canResize() const
{
    // We need a special case for <iframe> because they never have
    // hasOverflowClip(). However, they do "implicitly" clip their contents, so
    // we want to allow resizing them also.
    return (renderer().hasOverflowClip() || renderer().isRenderIFrame()) && renderer().style().resize() != Resize::None;
}

void RenderLayer::resize(const PlatformMouseEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !canResize() || !renderer().element())
        return;

    // FIXME: The only case where renderer->element()->renderer() != renderer is with continuations. Do they matter here?
    // If they do it would still be better to deal with them explicitly.
    Element* element = renderer().element();
    auto* renderer = downcast<RenderBox>(element->renderer());

    Document& document = element->document();
    if (!document.frame()->eventHandler().mousePressed())
        return;

    float zoomFactor = renderer->style().effectiveZoom();

    LayoutSize newOffset = offsetFromResizeCorner(document.view()->windowToContents(evt.position()));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);
    
    LayoutSize currentSize = LayoutSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);
    
    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (shouldPlaceBlockDirectionScrollbarOnLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }
    
    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    StyledElement* styledElement = downcast<StyledElement>(element);
    bool isBoxSizingBorder = renderer->style().boxSizing() == BoxSizing::BorderBox;

    Resize resize = renderer->style().resize();
    if (resize != Resize::Vertical && difference.width()) {
        if (is<HTMLFormControlElement>(*element)) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginLeft, renderer->marginLeft() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginRight, renderer->marginRight() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseWidth = renderer->width() - (isBoxSizingBorder ? LayoutUnit() : renderer->horizontalBorderAndPaddingExtent());
        baseWidth = baseWidth / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::CSS_PX);
    }

    if (resize != Resize::Horizontal && difference.height()) {
        if (is<HTMLFormControlElement>(*element)) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginTop, renderer->marginTop() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginBottom, renderer->marginBottom() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseHeight = renderer->height() - (isBoxSizingBorder ? LayoutUnit() : renderer->verticalBorderAndPaddingExtent());
        baseHeight = baseHeight / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::CSS_PX);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

int RenderLayer::scrollSize(ScrollbarOrientation orientation) const
{
    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_hBar : m_vBar).get();
    return scrollbar ? (scrollbar->totalSize() - scrollbar->visibleSize()) : 0;
}

void RenderLayer::setScrollOffset(const ScrollOffset& offset)
{
    scrollTo(scrollPositionFromOffset(offset));
}

int RenderLayer::scrollOffset(ScrollbarOrientation orientation) const
{
    if (orientation == HorizontalScrollbar)
        return scrollOffset().x();

    if (orientation == VerticalScrollbar)
        return scrollOffset().y();

    return 0;
}

IntRect RenderLayer::visibleContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion, VisibleContentRectBehavior) const
{
    IntSize scrollbarSpace;
    if (showsOverflowControls() && scrollbarInclusion == IncludeScrollbars)
        scrollbarSpace = scrollbarIntrusion();
    
    // FIXME: This seems wrong: m_layerSize includes borders. Can we just use the ScrollableArea implementation?
    return IntRect(scrollPosition(), IntSize(std::max(0, m_layerSize.width() - scrollbarSpace.width()), std::max(0, m_layerSize.height() - scrollbarSpace.height())));
}

IntSize RenderLayer::overhangAmount() const
{
#if ENABLE(RUBBER_BANDING)
    if (!renderer().settings().rubberBandingForSubScrollableRegionsEnabled())
        return IntSize();

    IntSize stretch;

    // FIXME: use maximumScrollOffset(), or just move this to ScrollableArea.
    ScrollOffset scrollOffset = scrollOffsetFromPosition(scrollPosition());
    if (scrollOffset.y() < 0)
        stretch.setHeight(scrollOffset.y());
    else if (scrollableContentsSize().height() && scrollOffset.y() > scrollableContentsSize().height() - visibleHeight())
        stretch.setHeight(scrollOffset.y() - (scrollableContentsSize().height() - visibleHeight()));

    if (scrollOffset.x() < 0)
        stretch.setWidth(scrollOffset.x());
    else if (scrollableContentsSize().width() && scrollOffset.x() > scrollableContentsSize().width() - visibleWidth())
        stretch.setWidth(scrollOffset.x() - (scrollableContentsSize().width() - visibleWidth()));

    return stretch;
#else
    return IntSize();
#endif
}

bool RenderLayer::isActive() const
{
    return page().focusController().isActive();
}

static int cornerStart(const RenderLayer& layer, int minX, int maxX, int thickness)
{
    if (layer.shouldPlaceBlockDirectionScrollbarOnLeft())
        return minX + layer.renderer().style().borderLeftWidth();
    return maxX - thickness - layer.renderer().style().borderRightWidth();
}

static LayoutRect cornerRect(const RenderLayer& layer, const LayoutRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!layer.verticalScrollbar() && !layer.horizontalScrollbar()) {
        // FIXME: This isn't right.  We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::theme().scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (layer.verticalScrollbar() && !layer.horizontalScrollbar()) {
        horizontalThickness = layer.verticalScrollbar()->width();
        verticalThickness = horizontalThickness;
    } else if (layer.horizontalScrollbar() && !layer.verticalScrollbar()) {
        verticalThickness = layer.horizontalScrollbar()->height();
        horizontalThickness = verticalThickness;
    } else {
        horizontalThickness = layer.verticalScrollbar()->width();
        verticalThickness = layer.horizontalScrollbar()->height();
    }
    return LayoutRect(cornerStart(layer, bounds.x(), bounds.maxX(), horizontalThickness),
        bounds.maxY() - verticalThickness - layer.renderer().style().borderBottomWidth(),
        horizontalThickness, verticalThickness);
}

IntRect RenderLayer::scrollCornerRect() const
{
    // We have a scrollbar corner when a non overlay scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one non overlay scrollbar is present
    // (b) Both non overlay scrollbars are present.
    // Overlay scrollbars always fill the entire length of the box so we never have scroll corner in that case.
    bool hasHorizontalBar = m_hBar && !m_hBar->isOverlayScrollbar();
    bool hasVerticalBar = m_vBar && !m_vBar->isOverlayScrollbar();
    bool hasResizer = renderer().style().resize() != Resize::None;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return snappedIntRect(cornerRect(*this, renderBox()->borderBoxRect()));
    return IntRect();
}

static LayoutRect resizerCornerRect(const RenderLayer& layer, const LayoutRect& bounds)
{
    ASSERT(layer.renderer().isBox());
    if (layer.renderer().style().resize() == Resize::None)
        return LayoutRect();
    return cornerRect(layer, bounds);
}

LayoutRect RenderLayer::scrollCornerAndResizerRect() const
{
    RenderBox* box = renderBox();
    if (!box)
        return LayoutRect();
    LayoutRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(*this, box->borderBoxRect());
    return scrollCornerAndResizer;
}

bool RenderLayer::isScrollCornerVisible() const
{
    ASSERT(renderer().isBox());
    return !scrollCornerRect().isEmpty();
}

IntRect RenderLayer::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return renderer().view().frameView().convertFromRendererToContainingView(&renderer(), rect);
}

IntRect RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect rect = renderer().view().frameView().convertFromContainingViewToRenderer(&renderer(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayer::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return renderer().view().frameView().convertFromRendererToContainingView(&renderer(), point);
}

IntPoint RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = renderer().view().frameView().convertFromContainingViewToRenderer(&renderer(), parentPoint);
    point.move(-scrollbarOffset(scrollbar));
    return point;
}

IntSize RenderLayer::visibleSize() const
{
    RenderBox* box = renderBox();
    if (!box)
        return IntSize();

    return IntSize(roundToInt(box->clientWidth()), roundToInt(box->clientHeight()));
}

IntSize RenderLayer::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayer::scrollableContentsSize() const
{
    IntSize contentsSize = this->contentsSize();

    if (!hasScrollableHorizontalOverflow())
        contentsSize.setWidth(std::min(contentsSize.width(), visibleSize().width()));

    if (!hasScrollableVerticalOverflow())
        contentsSize.setHeight(std::min(contentsSize.height(), visibleSize().height()));

    return contentsSize;
}

void RenderLayer::availableContentSizeChanged(AvailableSizeChangeReason reason)
{
    ScrollableArea::availableContentSizeChanged(reason);

    if (reason == AvailableSizeChangeReason::ScrollbarsChanged) {
        if (is<RenderBlock>(renderer()))
            downcast<RenderBlock>(renderer()).setShouldForceRelayoutChildren(true);
        renderer().setNeedsLayout();
    }
}

bool RenderLayer::shouldSuspendScrollAnimations() const
{
    return renderer().view().frameView().shouldSuspendScrollAnimations();
}

#if PLATFORM(IOS)
void RenderLayer::didStartScroll()
{
    page().chrome().client().didStartOverflowScroll();
}

void RenderLayer::didEndScroll()
{
    page().chrome().client().didEndOverflowScroll();
}
    
void RenderLayer::didUpdateScroll()
{
    // Send this notification when we scroll, since this is how we keep selection updated.
    page().chrome().client().didLayout(ChromeClient::Scroll);
}
#endif

IntPoint RenderLayer::lastKnownMousePosition() const
{
    return renderer().frame().eventHandler().lastKnownMousePosition();
}

bool RenderLayer::isHandlingWheelEvent() const
{
    return renderer().frame().eventHandler().isHandlingWheelEvent();
}

IntRect RenderLayer::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayer::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayer::verticalScrollbarStart(int minX, int maxX) const
{
    const RenderBox* box = renderBox();
    if (shouldPlaceBlockDirectionScrollbarOnLeft())
        return minX + box->borderLeft();
    return maxX - box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayer::horizontalScrollbarStart(int minX) const
{
    const RenderBox* box = renderBox();
    int x = minX + box->borderLeft();
    if (shouldPlaceBlockDirectionScrollbarOnLeft())
        x += m_vBar ? m_vBar->width() : roundToInt(resizerCornerRect(*this, box->borderBoxRect()).width());
    return x;
}

IntSize RenderLayer::scrollbarOffset(const Scrollbar& scrollbar) const
{
    RenderBox* box = renderBox();

    if (&scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, box->width()), box->borderTop());

    if (&scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar.height());
    
    ASSERT_NOT_REACHED();
    return IntSize();
}

void RenderLayer::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (&scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    IntRect scrollRect = rect;
    RenderBox* box = renderBox();
    ASSERT(box);
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!box->parent())
        return;

    if (&scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, box->width()), box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar.height());
    LayoutRect repaintRect = scrollRect;
    renderBox()->flipForWritingMode(repaintRect);
    renderer().repaintRectangle(repaintRect);
}

void RenderLayer::invalidateScrollCornerRect(const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

static inline RenderElement* rendererForScrollbar(RenderLayerModelObject& renderer)
{
    if (Element* element = renderer.element()) {
        if (ShadowRoot* shadowRoot = element->containingShadowRoot()) {
            if (shadowRoot->mode() == ShadowRootMode::UserAgent)
                return shadowRoot->host()->renderer();
        }
    }

    return &renderer;
}

Ref<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    ASSERT(rendererForScrollbar(renderer()));
    auto& actualRenderer = *rendererForScrollbar(renderer());
    bool hasCustomScrollbarStyle = is<RenderBox>(actualRenderer) && downcast<RenderBox>(actualRenderer).style().hasPseudoStyle(PseudoId::Scrollbar);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(*this, orientation, downcast<RenderBox>(actualRenderer).element());
    else {
        widget = Scrollbar::createNativeScrollbar(*this, orientation, RegularScrollbar);
        didAddScrollbar(widget.get(), orientation);
        if (page().expectsWheelEventTriggers())
            scrollAnimator().setWheelEventTestTrigger(page().testTrigger());
    }
    renderer().view().frameView().addChild(*widget);
    return widget.releaseNonNull();
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar())
        willRemoveScrollbar(scrollbar.get(), orientation);

    scrollbar->removeFromParent();
    scrollbar = nullptr;
}

bool RenderLayer::scrollsOverflow() const
{
    if (!is<RenderBox>(renderer()))
        return false;

    return downcast<RenderBox>(renderer()).scrollsOverflow();
}

void RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar) {
        m_hBar = createScrollbar(HorizontalScrollbar);
#if ENABLE(RUBBER_BANDING)
        ScrollElasticity elasticity = scrollsOverflow() && renderer().settings().rubberBandingForSubScrollableRegionsEnabled() ? ScrollElasticityAutomatic : ScrollElasticityNone;
        ScrollableArea::setHorizontalScrollElasticity(elasticity);
#endif
    } else {
        destroyScrollbar(HorizontalScrollbar);
#if ENABLE(RUBBER_BANDING)
        ScrollableArea::setHorizontalScrollElasticity(ScrollElasticityNone);
#endif
    }

    // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
    if (renderer().document().hasAnnotatedRegions())
        renderer().document().setAnnotatedRegionsDirty(true);
#endif
}

void RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar) {
        m_vBar = createScrollbar(VerticalScrollbar);
#if ENABLE(RUBBER_BANDING)
        ScrollElasticity elasticity = scrollsOverflow() && renderer().settings().rubberBandingForSubScrollableRegionsEnabled() ? ScrollElasticityAutomatic : ScrollElasticityNone;
        ScrollableArea::setVerticalScrollElasticity(elasticity);
#endif
    } else {
        destroyScrollbar(VerticalScrollbar);
#if ENABLE(RUBBER_BANDING)
        ScrollableArea::setVerticalScrollElasticity(ScrollElasticityNone);
#endif
    }

     // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
    if (renderer().document().hasAnnotatedRegions())
        renderer().document().setAnnotatedRegionsDirty(true);
#endif
}

ScrollableArea* RenderLayer::enclosingScrollableArea() const
{
    if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
        return scrollableLayer;

    // FIXME: We should return the frame view here (or possibly an ancestor frame view,
    // if the frame view isn't scrollable.
    return nullptr;
}

bool RenderLayer::isScrollableOrRubberbandable()
{
    return renderer().isScrollableOrRubberbandableBox();
}

bool RenderLayer::hasScrollableOrRubberbandableAncestor()
{
    for (RenderLayer* nextLayer = parentLayerCrossFrame(*this); nextLayer; nextLayer = parentLayerCrossFrame(*nextLayer)) {
        if (nextLayer->isScrollableOrRubberbandable())
            return true;
    }

    return false;
}

#if ENABLE(CSS_SCROLL_SNAP)
void RenderLayer::updateSnapOffsets()
{
    // FIXME: Extend support beyond HTMLElements.
    if (!is<HTMLElement>(enclosingElement()) || !enclosingElement()->renderBox())
        return;

    RenderBox* box = enclosingElement()->renderBox();
    updateSnapOffsetsForScrollableArea(*this, *downcast<HTMLElement>(enclosingElement()), *box, box->style());
}

bool RenderLayer::isScrollSnapInProgress() const
{
    if (!scrollsOverflow())
        return false;
    
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isScrollSnapInProgress();
    
    return false;
}
#endif

bool RenderLayer::usesMockScrollAnimator() const
{
    return DeprecatedGlobalSettings::usesMockScrollAnimator();
}

void RenderLayer::logMockScrollAnimatorMessage(const String& message) const
{
    renderer().document().addConsoleMessage(MessageSource::Other, MessageLevel::Debug, "RenderLayer: " + message);
}

int RenderLayer::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar
        || !showsOverflowControls()
        || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;

    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar
        || !showsOverflowControls()
        || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;

    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = size();
    if (shouldPlaceBlockDirectionScrollbarOnLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));
    return localPoint - resizerPoint;
}

bool RenderLayer::hasOverflowControls() const
{
    return m_hBar || m_vBar || m_scrollCorner || renderer().style().resize() != Resize::None;
}

void RenderLayer::positionOverflowControls(const IntSize& offsetFromRoot)
{
    if (!m_hBar && !m_vBar && !canResize())
        return;
    
    RenderBox* box = renderBox();
    if (!box)
        return;

    const IntRect borderBox = snappedIntRect(box->borderBoxRect());
    const IntRect& scrollCorner = scrollCornerRect();
    IntRect absBounds(borderBox.location() + offsetFromRoot, borderBox.size());
    if (m_vBar) {
        IntRect vBarRect = rectForVerticalScrollbar(borderBox);
        vBarRect.move(offsetFromRoot);
        m_vBar->setFrameRect(vBarRect);
    }
    
    if (m_hBar) {
        IntRect hBarRect = rectForHorizontalScrollbar(borderBox);
        hBarRect.move(offsetFromRoot);
        m_hBar->setFrameRect(hBarRect);
    }
    
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(*this, borderBox));

    if (isComposited())
        backing()->positionOverflowControlsLayers();
}

int RenderLayer::scrollWidth() const
{
    ASSERT(renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayer*>(this)->computeScrollDimensions();
    // FIXME: This should use snappedIntSize() instead with absolute coordinates.
    return m_scrollSize.width();
}

int RenderLayer::scrollHeight() const
{
    ASSERT(renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayer*>(this)->computeScrollDimensions();
    // FIXME: This should use snappedIntSize() instead with absolute coordinates.
    return m_scrollSize.height();
}

LayoutUnit RenderLayer::overflowTop() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.y();
}

LayoutUnit RenderLayer::overflowBottom() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxY();
}

LayoutUnit RenderLayer::overflowLeft() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.x();
}

LayoutUnit RenderLayer::overflowRight() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxX();
}

void RenderLayer::computeScrollDimensions()
{
    RenderBox* box = renderBox();
    ASSERT(box);

    m_scrollDimensionsDirty = false;

    m_scrollSize.setWidth(roundToInt(overflowRight() - overflowLeft()));
    m_scrollSize.setHeight(roundToInt(overflowBottom() - overflowTop()));

    int scrollableLeftOverflow = roundToInt(overflowLeft() - box->borderLeft());
    if (shouldPlaceBlockDirectionScrollbarOnLeft())
        scrollableLeftOverflow -= verticalScrollbarWidth();
    int scrollableTopOverflow = roundToInt(overflowTop() - box->borderTop());
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));
}

bool RenderLayer::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && renderBox()->scrollsOverflowX();
}

bool RenderLayer::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && renderBox()->scrollsOverflowY();
}

bool RenderLayer::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > roundToInt(renderBox()->clientWidth());
}

bool RenderLayer::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > roundToInt(renderBox()->clientHeight());
}

static bool styleRequiresScrollbar(const RenderStyle& style, ScrollbarOrientation axis)
{
    Overflow overflow = axis == ScrollbarOrientation::HorizontalScrollbar ? style.overflowX() : style.overflowY();
    bool overflowScrollActsLikeAuto = overflow == Overflow::Scroll && !style.hasPseudoStyle(PseudoId::Scrollbar) && ScrollbarTheme::theme().usesOverlayScrollbars();
    return overflow == Overflow::Scroll && !overflowScrollActsLikeAuto;
}

static bool styleDefinesAutomaticScrollbar(const RenderStyle& style, ScrollbarOrientation axis)
{
    Overflow overflow = axis == ScrollbarOrientation::HorizontalScrollbar ? style.overflowX() : style.overflowY();
    bool overflowScrollActsLikeAuto = overflow == Overflow::Scroll && !style.hasPseudoStyle(PseudoId::Scrollbar) && ScrollbarTheme::theme().usesOverlayScrollbars();
    return overflow == Overflow::Auto || overflowScrollActsLikeAuto;
}

void RenderLayer::updateScrollbarsAfterLayout()
{
    RenderBox* box = renderBox();
    ASSERT(box);

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().appearance() == ListboxPart)
        return;

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    // If overflow requires a scrollbar, then we just need to enable or disable.
    if (m_hBar && styleRequiresScrollbar(renderer().style(), HorizontalScrollbar))
        m_hBar->setEnabled(hasHorizontalOverflow);
    if (m_vBar && styleRequiresScrollbar(renderer().style(), VerticalScrollbar))
        m_vBar->setEnabled(hasVerticalOverflow);

    // Scrollbars with auto behavior may need to lay out again if scrollbars got added or removed.
    bool autoHorizontalScrollBarChanged = box->hasHorizontalScrollbarWithAutoBehavior() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = box->hasVerticalScrollbarWithAutoBehavior() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (box->hasHorizontalScrollbarWithAutoBehavior())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (box->hasVerticalScrollbarWithAutoBehavior())
            setHasVerticalScrollbar(hasVerticalOverflow);

        updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
        if (renderer().document().hasAnnotatedRegions())
            renderer().document().setAnnotatedRegionsDirty(true);
#endif

        renderer().repaint();

        if (renderer().style().overflowX() == Overflow::Auto || renderer().style().overflowY() == Overflow::Auto) {
            if (!m_inOverflowRelayout) {
                m_inOverflowRelayout = true;
                renderer().setNeedsLayout(MarkOnlyThis);
                if (is<RenderBlock>(renderer())) {
                    RenderBlock& block = downcast<RenderBlock>(renderer());
                    block.scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block.layoutBlock(true);
                } else
                    renderer().layout();
                m_inOverflowRelayout = false;
            }
        }
        
        RenderObject* parent = renderer().parent();
        if (parent && parent->isFlexibleBox() && renderer().isBox())
            downcast<RenderFlexibleBox>(parent)->clearCachedMainSizeForChild(*renderBox());
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = roundToInt(box->clientWidth());
        int pageStep = Scrollbar::pageStep(clientWidth);
        m_hBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_hBar->setProportion(clientWidth, m_scrollSize.width());
    }
    if (m_vBar) {
        int clientHeight = roundToInt(box->clientHeight());
        int pageStep = Scrollbar::pageStep(clientHeight);
        m_vBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_vBar->setProportion(clientHeight, m_scrollSize.height());
    }

    updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

// This is called from layout code (before updateLayerPositions).
void RenderLayer::updateScrollInfoAfterLayout()
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    m_scrollDimensionsDirty = true;
    ScrollOffset originalScrollOffset = scrollOffset();

    computeScrollDimensions();

#if ENABLE(CSS_SCROLL_SNAP)
    // FIXME: Ensure that offsets are also updated in case of programmatic style changes.
    // https://bugs.webkit.org/show_bug.cgi?id=135964
    updateSnapOffsets();
#endif

    if (!box->isHTMLMarquee() && !isRubberBandInProgress()) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        ScrollOffset clampedScrollOffset = clampScrollOffset(scrollOffset());
#if PLATFORM(IOS)
        // FIXME: This looks wrong. The caret adjust mode should only be enabled on editing related entry points.
        // This code was added to fix an issue where the text insertion point would always be drawn on the right edge
        // of a text field whose content overflowed its bounds. See <rdar://problem/15579797> for more details.
        setAdjustForIOSCaretWhenScrolling(true);
#endif
        if (clampedScrollOffset != scrollOffset())
            scrollToOffset(clampedScrollOffset);

#if PLATFORM(IOS)
        setAdjustForIOSCaretWhenScrolling(false);
#endif
    }

    updateScrollbarsAfterLayout();

    if (originalScrollOffset != scrollOffset())
        scrollToOffsetWithoutAnimation(IntPoint(scrollOffset()));

    // Composited scrolling may need to be enabled or disabled if the amount of overflow changed.
    // FIXME: this should just dirty the layer and updateLayerPositions should do stuff.
    if (compositor().updateLayerCompositingState(*this))
        compositor().setCompositingLayersNeedRebuild();

    updateScrollSnapState();
}

bool RenderLayer::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = snappedIntRect(renderBox()->borderBoxRect());

    if (rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollCornerRect().intersects(localRect))
        return true;
    
    if (resizerCornerRect(*this, borderBox).intersects(localRect))
        return true;

    return false;
}

bool RenderLayer::showsOverflowControls() const
{
#if PLATFORM(IOS)
    // Don't render (custom) scrollbars if we have accelerated scrolling.
    if (canUseAcceleratedTouchScrolling())
        return false;
#endif

    return true;
}

void RenderLayer::paintOverflowControls(GraphicsContext& context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!renderer().hasOverflowClip())
        return;

    if (!showsOverflowControls())
        return;

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the 
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;

        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderLayer* paintingRoot = enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderer().view().layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_cachedOverlayScrollbarOffset;

    // Move the scrollbar widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls(toIntSize(adjustedPaintOffset));

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);
    
    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

void RenderLayer::paintScrollCorner(GraphicsContext& context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context.invalidatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint a corner if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        ScrollbarTheme::theme().paintScrollCorner(context, absRect);
}

void RenderLayer::drawPlatformResizerImage(GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
    RefPtr<Image> resizeCornerImage;
    FloatSize cornerResizerSize;
    if (renderer().document().deviceScaleFactor() >= 2) {
        static NeverDestroyed<Image*> resizeCornerImageHiRes(&Image::loadPlatformResource("textAreaResizeCorner@2x").leakRef());
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        static NeverDestroyed<Image*> resizeCornerImageLoRes(&Image::loadPlatformResource("textAreaResizeCorner").leakRef());
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (shouldPlaceBlockDirectionScrollbarOnLeft()) {
        context.save();
        context.translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context.scale(FloatSize(-1.0, 1.0));
        if (resizeCornerImage)
            context.drawImage(*resizeCornerImage, FloatRect(FloatPoint(), cornerResizerSize));
        context.restore();
        return;
    }
    
    if (!resizeCornerImage)
        return;
    FloatRect imageRect = snapRectToDevicePixels(LayoutRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize), renderer().document().deviceScaleFactor());
    context.drawImage(*resizeCornerImage, imageRect);
}

void RenderLayer::paintResizer(GraphicsContext& context, const LayoutPoint& paintOffset, const LayoutRect& damageRect)
{
    if (renderer().style().resize() == Resize::None)
        return;

    RenderBox* box = renderBox();
    ASSERT(box);

    LayoutRect absRect = resizerCornerRect(*this, box->borderBoxRect());
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context.invalidatingControlTints()) {
        updateResizerStyle();
        return;
    }
    
    if (m_resizer) {
        m_resizer->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!hasOverlayScrollbars() && (m_vBar || m_hBar)) {
        GraphicsContextStateSaver stateSaver(context);
        context.clip(absRect);
        LayoutRect largerCorner = absRect;
        largerCorner.setSize(LayoutSize(largerCorner.width() + LayoutUnit::fromPixel(1), largerCorner.height() + LayoutUnit::fromPixel(1)));
        context.setStrokeColor(Color(makeRGB(217, 217, 217)));
        context.setStrokeThickness(1.0f);
        context.setFillColor(Color::transparent);
        context.drawRect(snappedIntRect(largerCorner));
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& absolutePoint) const
{
    if (!canResize())
        return false;
    
    RenderBox* box = renderBox();
    ASSERT(box);

    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));

    IntRect localBounds(IntPoint(), snappedIntRect(box->frameRect()).size());
    return resizerCornerRect(*this, localBounds).contains(localPoint);
}

bool RenderLayer::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!m_hBar && !m_vBar && !canResize())
        return false;

    RenderBox* box = renderBox();
    ASSERT(box);
    
    IntRect resizeControlRect;
    if (renderer().style().resize() != Resize::None) {
        resizeControlRect = snappedIntRect(resizerCornerRect(*this, box->borderBoxRect()));
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = std::max(resizeControlRect.height(), 0);

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box->width()),
                            box->borderTop(),
                            m_vBar->width(),
                            box->height() - (box->borderTop() + box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = std::max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
                            box->height() - box->borderBottom() - m_hBar->height(),
                            box->width() - (box->borderLeft() + box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
                            m_hBar->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    return false;
}

bool RenderLayer::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    return ScrollableArea::scroll(direction, granularity, multiplier);
}

void RenderLayer::paint(GraphicsContext& context, const LayoutRect& damageRect, const LayoutSize& subpixelOffset, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot, OptionSet<PaintLayerFlag> paintFlags, SecurityOriginPaintPolicy paintPolicy)
{
    OverlapTestRequestMap overlapTestRequests;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, subpixelOffset, subtreePaintRoot, &overlapTestRequests, paintPolicy == SecurityOriginPaintPolicy::AccessibleOriginOnly);
    paintLayer(context, paintingInfo, paintFlags);

    for (auto& widget : overlapTestRequests.keys())
        widget->setOverlapTestResult(false);
}

void RenderLayer::paintOverlayScrollbars(GraphicsContext& context, const LayoutRect& damageRect, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot)
{
    if (!m_containsDirtyOverlayScrollbars)
        return;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), subtreePaintRoot);
    paintLayer(context, paintingInfo, PaintLayerPaintingOverlayScrollbars);

    m_containsDirtyOverlayScrollbars = false;
}

static bool inContainingBlockChain(RenderLayer* startLayer, RenderLayer* endLayer)
{
    if (startLayer == endLayer)
        return true;
    for (const auto* currentBlock = startLayer->renderer().containingBlock(); currentBlock && !is<RenderView>(*currentBlock); currentBlock = currentBlock->containingBlock()) {
        if (currentBlock->layer() == endLayer)
            return true;
    }
    
    return false;
}

void RenderLayer::clipToRect(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const ClipRect& clipRect, BorderRadiusClippingRule rule)
{
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    bool needsClipping = !clipRect.isInfinite() && clipRect.rect() != paintingInfo.paintDirtyRect;
    if (needsClipping || clipRect.affectedByRadius())
        context.save();

    if (needsClipping) {
        LayoutRect adjustedClipRect = clipRect.rect();
        adjustedClipRect.move(paintingInfo.subpixelOffset);
        context.clip(snapRectToDevicePixels(adjustedClipRect, deviceScaleFactor));
    }

    if (clipRect.affectedByRadius()) {
        // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
        // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
        // containing block chain so we check that also.
        for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? this : parent(); layer; layer = layer->parent()) {
            if (layer->renderer().hasOverflowClip() && layer->renderer().style().hasBorderRadius() && inContainingBlockChain(this, layer)) {
                LayoutRect adjustedClipRect = LayoutRect(toLayoutPoint(layer->offsetFromAncestor(paintingInfo.rootLayer, AdjustForColumns)), layer->size());
                adjustedClipRect.move(paintingInfo.subpixelOffset);
                FloatRoundedRect roundedRect = layer->renderer().style().getRoundedInnerBorderFor(adjustedClipRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                if (roundedRect.intersectionIsRectangular(paintingInfo.paintDirtyRect))
                    context.clip(snapRectToDevicePixels(intersection(paintingInfo.paintDirtyRect, adjustedClipRect), deviceScaleFactor));
                else
                    context.clipRoundedRect(roundedRect);
            }
            
            if (layer == paintingInfo.rootLayer)
                break;
        }
    }
}

void RenderLayer::restoreClip(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const ClipRect& clipRect)
{
    if ((!clipRect.isInfinite() && clipRect.rect() != paintingInfo.paintDirtyRect) || clipRect.affectedByRadius())
        context.restore();
}

static void performOverlapTests(OverlapTestRequestMap& overlapTestRequests, const RenderLayer* rootLayer, const RenderLayer* layer)
{
    Vector<OverlapTestRequestClient*> overlappedRequestClients;
    LayoutRect boundingBox = layer->boundingBox(rootLayer, layer->offsetFromAncestor(rootLayer));
    for (auto& request : overlapTestRequests) {
        if (!boundingBox.intersects(request.value))
            continue;

        request.key->setOverlapTestResult(true);
        overlappedRequestClients.append(request.key);
    }
    for (auto* client : overlappedRequestClients)
        overlapTestRequests.remove(client);
}

static inline bool shouldDoSoftwarePaint(const RenderLayer* layer, bool paintingReflection)
{
    return paintingReflection && !layer->has3DTransform();
}
    
static inline bool shouldSuppressPaintingLayer(RenderLayer* layer)
{
    if (layer->renderer().style().isNotFinal() && !layer->isRenderViewLayer() && !layer->renderer().isDocumentElementRenderer())
        return true;

    // Avoid painting all layers if the document is in a state where visual updates aren't allowed.
    // A full repaint will occur in Document::setVisualUpdatesAllowed(bool) if painting is suppressed here.
    if (!layer->renderer().document().visualUpdatesAllowed())
        return true;

    return false;
}

static inline bool paintForFixedRootBackground(const RenderLayer* layer, OptionSet<RenderLayer::PaintLayerFlag> paintFlags)
{
    return layer->renderer().isDocumentElementRenderer() && (paintFlags & RenderLayer::PaintLayerPaintingRootBackgroundOnly);
}

void RenderLayer::paintLayer(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    if (isComposited()) {
        // The performingPaintInvalidation() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (context.performingPaintInvalidation() || (paintingInfo.paintBehavior & PaintBehavior::FlattenCompositingLayers))
            paintFlags.add(PaintLayerTemporaryClipRects);
        else if (!backing()->paintsIntoWindow()
            && !backing()->paintsIntoCompositedAncestor()
            && !shouldDoSoftwarePaint(this, paintFlags.contains(PaintLayerPaintingReflection))
            && !paintForFixedRootBackground(this, paintFlags)) {
            // If this RenderLayer should paint into its backing, that will be done via RenderLayerBacking::paintIntoLayer().
            return;
        }
    } else if (viewportConstrainedNotCompositedReason() == NotCompositedForBoundsOutOfView) {
        // Don't paint out-of-view viewport constrained layers (when doing prepainting) because they will never be visible
        // unless their position or viewport size is changed.
        ASSERT(renderer().isFixedPositioned());
        return;
    }

    // Non self-painting leaf layers don't need to be painted as their renderer() should properly paint itself.
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return;

    if (shouldSuppressPaintingLayer(this))
        return;
    
    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer().opacity())
        return;

    if (paintsWithTransparency(paintingInfo.paintBehavior))
        paintFlags.add(PaintLayerHaveTransparency);

    // PaintLayerAppliedTransform is used in RenderReplica, to avoid applying the transform twice.
    if (paintsWithTransform(paintingInfo.paintBehavior) && !(paintFlags & PaintLayerAppliedTransform)) {
        TransformationMatrix layerTransform = renderableTransform(paintingInfo.paintBehavior);
        // If the transform can't be inverted, then don't paint anything.
        if (!layerTransform.isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now, assuming there is a parent
        if (paintFlags & PaintLayerHaveTransparency) {
            if (parent())
                parent()->beginTransparencyLayers(context, paintingInfo, paintingInfo.paintDirtyRect);
            else
                beginTransparencyLayers(context, paintingInfo, paintingInfo.paintDirtyRect);
        }

        if (enclosingPaginationLayer(ExcludeCompositedPaginatedLayers)) {
            paintTransformedLayerIntoFragments(context, paintingInfo, paintFlags);
            return;
        }

        // Make sure the parent's clip rects have been calculated.
        ClipRect clipRect = paintingInfo.paintDirtyRect;
        if (parent()) {
            ClipRectsContext clipRectsContext(paintingInfo.rootLayer, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            clipRect = backgroundClipRect(clipRectsContext);
            clipRect.intersect(paintingInfo.paintDirtyRect);
        
            // Push the parent coordinate space's clip.
            parent()->clipToRect(context, paintingInfo, clipRect);
        }

        paintLayerByApplyingTransform(context, paintingInfo, paintFlags);

        // Restore the clip.
        if (parent())
            parent()->restoreClip(context, paintingInfo, clipRect);

        return;
    }
    
    paintLayerContentsAndReflection(context, paintingInfo, paintFlags);
}

void RenderLayer::paintLayerContentsAndReflection(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    auto localPaintFlags = paintFlags - PaintLayerAppliedTransform;

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(context, paintingInfo, localPaintFlags | PaintLayerPaintingReflection);
        m_paintingInsideReflection = false;
    }

    localPaintFlags.add(paintLayerPaintingCompositingAllPhasesFlags());
    paintLayerContents(context, paintingInfo, localPaintFlags);
}

bool RenderLayer::setupFontSubpixelQuantization(GraphicsContext& context, bool& didQuantizeFonts)
{
    if (context.paintingDisabled())
        return false;

    bool scrollingOnMainThread = true;
#if ENABLE(ASYNC_SCROLLING)
    if (ScrollingCoordinator* scrollingCoordinator = page().scrollingCoordinator())
        scrollingOnMainThread = scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(renderer().view().frameView());
#endif

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool contentsScrollByPainting = (renderer().hasOverflowClip() && !usesCompositedScrolling()) || (renderer().frame().ownerElement());
    bool isZooming = !page().chrome().client().hasStablePageScaleFactor();
    if (scrollingOnMainThread || contentsScrollByPainting || isZooming) {
        didQuantizeFonts = context.shouldSubpixelQuantizeFonts();
        context.setShouldSubpixelQuantizeFonts(false);
        return true;
    }
    return false;
}

static inline LayoutRect computeReferenceBox(const RenderObject& renderer, const CSSBoxType& boxType, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds)
{
    // FIXME: Support different reference boxes for inline content.
    // https://bugs.webkit.org/show_bug.cgi?id=129047
    if (!renderer.isBox())
        return rootRelativeBounds;

    LayoutRect referenceBox;
    const auto& box = downcast<RenderBox>(renderer);
    switch (boxType) {
    case CSSBoxType::ContentBox:
    case CSSBoxType::FillBox:
        referenceBox = box.contentBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    case CSSBoxType::PaddingBox:
        referenceBox = box.paddingBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    case CSSBoxType::MarginBox:
        referenceBox = box.marginBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    // stroke-box, view-box compute to border-box for HTML elements.
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BorderBox:
    case CSSBoxType::BoxMissing:
        referenceBox = box.borderBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    }

    return referenceBox;
}

Path RenderLayer::computeClipPath(const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, WindRule& windRule) const
{
    const RenderStyle& style = renderer().style();
    float deviceSaleFactor = renderer().document().deviceScaleFactor();

    if (is<ShapeClipPathOperation>(*style.clipPath())) {
        auto& clipPath = downcast<ShapeClipPathOperation>(*style.clipPath());
        FloatRect referenceBox = snapRectToDevicePixels(computeReferenceBox(renderer(), clipPath.referenceBox(), offsetFromRoot, rootRelativeBounds), deviceSaleFactor);

        windRule = clipPath.windRule();
        return clipPath.pathForReferenceRect(referenceBox);
    }
    
    if (is<BoxClipPathOperation>(*style.clipPath()) && is<RenderBox>(renderer())) {

        auto& clipPath = downcast<BoxClipPathOperation>(*style.clipPath());

        FloatRoundedRect shapeRect = computeRoundedRectForBoxShape(clipPath.referenceBox(), downcast<RenderBox>(renderer())).pixelSnappedRoundedRectForPainting(deviceSaleFactor);
        shapeRect.move(offsetFromRoot);

        windRule = WindRule::NonZero;
        return clipPath.pathForReferenceRect(shapeRect);
    }
    
    return Path();
}

bool RenderLayer::setupClipPath(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
{
    if (!renderer().hasClipPath() || context.paintingDisabled())
        return false;

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, { });
        rootRelativeBoundsComputed = true;
    }

    // SVG elements get clipped in SVG code.
    if (is<RenderSVGRoot>(renderer()))
        return false;

    auto& style = renderer().style();
    LayoutSize paintingOffsetFromRoot = LayoutSize(snapSizeToDevicePixel(offsetFromRoot + paintingInfo.subpixelOffset, LayoutPoint(), renderer().document().deviceScaleFactor()));
    ASSERT(style.clipPath());
    if (is<ShapeClipPathOperation>(*style.clipPath()) || (is<BoxClipPathOperation>(*style.clipPath()) && is<RenderBox>(renderer()))) {
        WindRule windRule;
        Path path = computeClipPath(paintingOffsetFromRoot, rootRelativeBounds, windRule);
        context.save();
        context.clipPath(path, windRule);
        return true;
    }

    if (style.clipPath()->type() == ClipPathOperation::Reference) {
        ReferenceClipPathOperation* referenceClipPathOperation = static_cast<ReferenceClipPathOperation*>(style.clipPath());
        Element* element = renderer().document().getElementById(referenceClipPathOperation->fragment());
        if (element && element->renderer() && is<RenderSVGResourceClipper>(element->renderer())) {
            context.save();
            float deviceSaleFactor = renderer().document().deviceScaleFactor();
            FloatRect referenceBox = snapRectToDevicePixels(computeReferenceBox(renderer(), CSSBoxType::ContentBox, paintingOffsetFromRoot, rootRelativeBounds), deviceSaleFactor);
            FloatPoint offset {referenceBox.location()};
            context.translate(offset);
            FloatRect svgReferenceBox {FloatPoint(), referenceBox.size()};
            downcast<RenderSVGResourceClipper>(*element->renderer()).applyClippingToContext(renderer(), svgReferenceBox, paintingInfo.paintDirtyRect, context);
            context.translate(FloatPoint(-offset.x(), -offset.y()));
            return true;
        }
    }

    return false;
}

RenderLayerFilters* RenderLayer::filtersForPainting(GraphicsContext& context, OptionSet<PaintLayerFlag> paintFlags) const
{
    if (context.paintingDisabled())
        return nullptr;

    if (paintFlags & PaintLayerPaintingOverlayScrollbars)
        return nullptr;

    if (!paintsWithFilters())
        return nullptr;

    if (m_filters && m_filters->filter())
        return m_filters.get();

    return nullptr;
}

GraphicsContext* RenderLayer::setupFilters(GraphicsContext& destinationContext, LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
{
    auto* paintingFilters = filtersForPainting(destinationContext, paintFlags);
    if (!paintingFilters)
        return nullptr;

    LayoutRect filterRepaintRect = paintingFilters->dirtySourceRect();
    filterRepaintRect.move(offsetFromRoot);

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, { });
        rootRelativeBoundsComputed = true;
    }

    GraphicsContext* filterContext = paintingFilters->beginFilterEffect(destinationContext, enclosingIntRect(rootRelativeBounds), enclosingIntRect(paintingInfo.paintDirtyRect), enclosingIntRect(filterRepaintRect));
    if (!filterContext)
        return nullptr;

    paintingInfo.paintDirtyRect = paintingFilters->repaintRect();

    // If the filter needs the full source image, we need to avoid using the clip rectangles.
    // Otherwise, if for example this layer has overflow:hidden, a drop shadow will not compute correctly.
    // Note that we will still apply the clipping on the final rendering of the filter.
    paintingInfo.clipToDirtyRect = !paintingFilters->hasFilterThatMovesPixels();

    paintingInfo.requireSecurityOriginAccessForWidgets = paintingFilters->hasFilterThatShouldBeRestrictedBySecurityOrigin();

    return filterContext;
}

void RenderLayer::applyFilters(GraphicsContext& originalContext, const LayerPaintingInfo& paintingInfo, const LayerFragments& layerFragments)
{
    // FIXME: Handle more than one fragment.
    ClipRect backgroundRect = layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect;
    clipToRect(originalContext, paintingInfo, backgroundRect);
    m_filters->applyFilterEffect(originalContext);
    restoreClip(originalContext, paintingInfo, backgroundRect);
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

void RenderLayer::paintLayerContents(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    auto localPaintFlags = paintFlags - PaintLayerAppliedTransform;
    bool haveTransparency = localPaintFlags.contains(PaintLayerHaveTransparency);
    bool isSelfPaintingLayer = this->isSelfPaintingLayer();
    bool isPaintingOverlayScrollbars = paintFlags.contains(PaintLayerPaintingOverlayScrollbars);
    bool isPaintingScrollingContent = paintFlags.contains(PaintLayerPaintingCompositingScrollingPhase);
    bool isPaintingCompositedForeground = paintFlags.contains(PaintLayerPaintingCompositingForegroundPhase);
    bool isPaintingCompositedBackground = paintFlags.contains(PaintLayerPaintingCompositingBackgroundPhase);
    bool isPaintingOverflowContents = paintFlags.contains(PaintLayerPaintingOverflowContents);
    // Outline always needs to be painted even if we have no visible content. Also,
    // the outline is painted in the background phase during composited scrolling.
    // If it were painted in the foreground phase, it would move with the scrolled
    // content. When not composited scrolling, the outline is painted in the
    // foreground phase. Since scrolled contents are moved by repainting in this
    // case, the outline won't get 'dragged along'.
    bool shouldPaintOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars
        && ((isPaintingScrollingContent && isPaintingCompositedBackground)
        || (!isPaintingScrollingContent && isPaintingCompositedForeground));
    bool shouldPaintContent = m_hasVisibleContent && isSelfPaintingLayer && !isPaintingOverlayScrollbars;

    if (localPaintFlags & PaintLayerPaintingRootBackgroundOnly && !renderer().isRenderView() && !renderer().isDocumentElementRenderer())
        return;

    // Ensure our lists are up-to-date.
    updateLayerListsIfNeeded();

    LayoutSize offsetFromRoot = offsetFromAncestor(paintingInfo.rootLayer);
    LayoutRect rootRelativeBounds;
    bool rootRelativeBoundsComputed = false;

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool didQuantizeFonts = true;
    bool needToAdjustSubpixelQuantization = setupFontSubpixelQuantization(context, didQuantizeFonts);

    // Apply clip-path to context.
    LayoutSize columnAwareOffsetFromRoot = offsetFromRoot;
    if (renderer().enclosingFragmentedFlow() && (renderer().hasClipPath() || filtersForPainting(context, paintFlags)))
        columnAwareOffsetFromRoot = toLayoutSize(convertToLayerCoords(paintingInfo.rootLayer, LayoutPoint(), AdjustForColumns));

    bool hasClipPath = false;
    if (shouldApplyClipPath(paintingInfo.paintBehavior, localPaintFlags))
        hasClipPath = setupClipPath(context, paintingInfo, columnAwareOffsetFromRoot, rootRelativeBounds, rootRelativeBoundsComputed);

    bool selectionAndBackgroundsOnly = paintingInfo.paintBehavior.contains(PaintBehavior::SelectionAndBackgroundsOnly);
    bool selectionOnly = paintingInfo.paintBehavior.contains(PaintBehavior::SelectionOnly);

    SinglePaintFrequencyTracking singlePaintFrequencyTracking(m_paintFrequencyTracker, shouldPaintContent);

    LayerFragments layerFragments;
    RenderObject* subtreePaintRootForRenderer = nullptr;

    { // Scope for filter-related state changes.
        LayerPaintingInfo localPaintingInfo(paintingInfo);
        GraphicsContext* filterContext = setupFilters(context, localPaintingInfo, paintFlags, columnAwareOffsetFromRoot, rootRelativeBounds, rootRelativeBoundsComputed);
        if (filterContext && haveTransparency) {
            // If we have a filter and transparency, we have to eagerly start a transparency layer here, rather than risk a child layer lazily starts one with the wrong context.
            beginTransparencyLayers(context, localPaintingInfo, paintingInfo.paintDirtyRect);
        }
        GraphicsContext& currentContext = filterContext ? *filterContext : context;

        // If this layer's renderer is a child of the subtreePaintRoot, we render unconditionally, which
        // is done by passing a nil subtreePaintRoot down to our renderer (as if no subtreePaintRoot was ever set).
        // Otherwise, our renderer tree may or may not contain the subtreePaintRoot root, so we pass that root along
        // so it will be tested against as we descend through the renderers.
        if (localPaintingInfo.subtreePaintRoot && !renderer().isDescendantOf(localPaintingInfo.subtreePaintRoot))
            subtreePaintRootForRenderer = localPaintingInfo.subtreePaintRoot;

        if (localPaintingInfo.overlapTestRequests && isSelfPaintingLayer)
            performOverlapTests(*localPaintingInfo.overlapTestRequests, localPaintingInfo.rootLayer, this);

        OptionSet<PaintBehavior> paintBehavior = PaintBehavior::Normal;
        if (localPaintFlags & PaintLayerPaintingSkipRootBackground)
            paintBehavior.add(PaintBehavior::SkipRootBackground);
        else if (localPaintFlags & PaintLayerPaintingRootBackgroundOnly)
            paintBehavior.add(PaintBehavior::RootBackgroundOnly);

        if (paintingInfo.paintBehavior & PaintBehavior::FlattenCompositingLayers)
            paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
        
        if (paintingInfo.paintBehavior & PaintBehavior::Snapshotting)
            paintBehavior.add(PaintBehavior::Snapshotting);
        
        if ((paintingInfo.paintBehavior & PaintBehavior::TileFirstPaint) && isRenderViewLayer())
            paintBehavior.add(PaintBehavior::TileFirstPaint);

        if (paintingInfo.paintBehavior & PaintBehavior::ExcludeSelection)
            paintBehavior.add(PaintBehavior::ExcludeSelection);

        LayoutRect paintDirtyRect = localPaintingInfo.paintDirtyRect;
        if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars) {
            // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment, as well as whether or not the content of each
            // fragment should paint. If the parent's filter dictates full repaint to ensure proper filter effect,
            // use the overflow clip as dirty rect, instead of no clipping. It maintains proper clipping for overflow::scroll.
            if (!localPaintingInfo.clipToDirtyRect && renderer().hasOverflowClip()) {
                // We can turn clipping back by requesting full repaint for the overflow area.
                localPaintingInfo.clipToDirtyRect = true;
                paintDirtyRect = clipRectRelativeToAncestor(localPaintingInfo.rootLayer, offsetFromRoot, LayoutRect::infiniteRect());
            }
            collectFragments(layerFragments, localPaintingInfo.rootLayer, paintDirtyRect, ExcludeCompositedPaginatedLayers,
                (localPaintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
                (isPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, offsetFromRoot);
            updatePaintingInfoForFragments(layerFragments, localPaintingInfo, localPaintFlags, shouldPaintContent, offsetFromRoot);
        }
        
        if (isPaintingCompositedBackground) {
            // Paint only the backgrounds for all of the fragments of the layer.
            if (shouldPaintContent && !selectionOnly) {
                paintBackgroundForFragments(layerFragments, currentContext, context, paintingInfo.paintDirtyRect, haveTransparency,
                    localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);
            }
        }

        // Now walk the sorted list of children with negative z-indices.
        if ((isPaintingScrollingContent && isPaintingOverflowContents) || (!isPaintingScrollingContent && isPaintingCompositedBackground))
            paintList(negZOrderList(), currentContext, localPaintingInfo, localPaintFlags);
        
        if (isPaintingCompositedForeground) {
            if (shouldPaintContent) {
                paintForegroundForFragments(layerFragments, currentContext, context, paintingInfo.paintDirtyRect, haveTransparency,
                    localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);
            }
        }

        if (shouldPaintOutline)
            paintOutlineForFragments(layerFragments, currentContext, localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);

        if (isPaintingCompositedForeground) {
            // Paint any child layers that have overflow.
            paintList(m_normalFlowList.get(), currentContext, localPaintingInfo, localPaintFlags);
        
            // Now walk the sorted list of children with positive z-indices.
            paintList(posZOrderList(), currentContext, localPaintingInfo, localPaintFlags);
        }

        if (isPaintingOverlayScrollbars && hasScrollbars())
            paintOverflowControlsForFragments(layerFragments, currentContext, localPaintingInfo);

        if (filterContext) {
            // When we called collectFragments() last time, paintDirtyRect was reset to represent the filter bounds.
            // Now we need to compute the backgroundRect uncontaminated by filters, in order to clip the filtered result.
            // Note that we also use paintingInfo here, not localPaintingInfo which filters also contaminated.
            LayerFragments layerFragments;
            collectFragments(layerFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, ExcludeCompositedPaginatedLayers,
                (localPaintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
                (isPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, offsetFromRoot);
            updatePaintingInfoForFragments(layerFragments, paintingInfo, localPaintFlags, shouldPaintContent, offsetFromRoot);

            applyFilters(context, paintingInfo, layerFragments);
        }
    }
    
    if (shouldPaintContent && !(selectionOnly || selectionAndBackgroundsOnly)) {
        OptionSet<PaintBehavior> paintBehavior = PaintBehavior::Normal;
        if (paintingInfo.paintBehavior & PaintBehavior::FlattenCompositingLayers)
            paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
        
        if (paintingInfo.paintBehavior & PaintBehavior::Snapshotting)
            paintBehavior.add(PaintBehavior::Snapshotting);
        
        if (paintingInfo.paintBehavior & PaintBehavior::TileFirstPaint)
            paintBehavior.add(PaintBehavior::TileFirstPaint);

        if (shouldPaintMask(paintingInfo.paintBehavior, localPaintFlags)) {
            // Paint the mask for the fragments.
            paintMaskForFragments(layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer);
        }

        if (!(paintFlags & PaintLayerPaintingCompositingMaskPhase) && (paintFlags & PaintLayerPaintingCompositingClipPathPhase)) {
            // Re-use paintChildClippingMaskForFragments to paint black for the compositing clipping mask.
            paintChildClippingMaskForFragments(layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer);
        }
        
        if (localPaintFlags & PaintLayerPaintingChildClippingMaskPhase) {
            // Paint the border radius mask for the fragments.
            paintChildClippingMaskForFragments(layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer);
        }
    }

    // End our transparency layer
    if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
        context.endTransparencyLayer();
        context.restore();
        m_usedTransparency = false;
    }

    // Re-set this to whatever it was before we painted the layer.
    if (needToAdjustSubpixelQuantization)
        context.setShouldSubpixelQuantizeFonts(didQuantizeFonts);

    if (hasClipPath)
        context.restore();
}

void RenderLayer::paintLayerByApplyingTransform(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayoutSize& translationOffset)
{
    // This involves subtracting out the position of the layer in our current coordinate space, but preserving
    // the accumulated error for sub-pixel layout.
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    LayoutSize offsetFromParent = offsetFromAncestor(paintingInfo.rootLayer);
    offsetFromParent += translationOffset;
    TransformationMatrix transform(renderableTransform(paintingInfo.paintBehavior));
    // Add the subpixel accumulation to the current layer's offset so that we can always snap the translateRight value to where the renderer() is supposed to be painting.
    LayoutSize offsetForThisLayer = offsetFromParent + paintingInfo.subpixelOffset;
    FloatSize devicePixelSnappedOffsetForThisLayer = toFloatSize(roundPointToDevicePixels(toLayoutPoint(offsetForThisLayer), deviceScaleFactor));
    // We handle accumulated subpixels through nested layers here. Since the context gets translated to device pixels,
    // all we need to do is add the delta to the accumulated pixels coming from ancestor layers.
    // Translate the graphics context to the snapping position to avoid off-device-pixel positing.
    transform.translateRight(devicePixelSnappedOffsetForThisLayer.width(), devicePixelSnappedOffsetForThisLayer.height());
    // Apply the transform.
    AffineTransform oldTransfrom = context.getCTM();
    context.concatCTM(transform.toAffineTransform());

    // Now do a paint with the root layer shifted to be us.
    LayoutSize adjustedSubpixelOffset = offsetForThisLayer - LayoutSize(devicePixelSnappedOffsetForThisLayer);
    LayerPaintingInfo transformedPaintingInfo(paintingInfo);
    transformedPaintingInfo.rootLayer = this;
    transformedPaintingInfo.paintDirtyRect = LayoutRect(encloseRectToDevicePixels(transform.inverse().value_or(AffineTransform()).mapRect(paintingInfo.paintDirtyRect), deviceScaleFactor));
    transformedPaintingInfo.subpixelOffset = adjustedSubpixelOffset;
    paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags);
    context.setCTM(oldTransfrom);
}

void RenderLayer::paintList(Vector<RenderLayer*>* list, GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    if (!list)
        return;

    if (!hasSelfPaintingLayerDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(this);
#endif

    for (auto* childLayer : *list)
        childLayer->paintLayer(context, paintingInfo, paintFlags);
}

RenderLayer* RenderLayer::enclosingPaginationLayerInSubtree(const RenderLayer* rootLayer, PaginationInclusionMode mode) const
{
    // If we don't have an enclosing layer, or if the root layer is the same as the enclosing layer,
    // then just return the enclosing pagination layer (it will be 0 in the former case and the rootLayer in the latter case).
    RenderLayer* paginationLayer = enclosingPaginationLayer(mode);
    if (!paginationLayer || rootLayer == paginationLayer)
        return paginationLayer;
    
    // Walk up the layer tree and see which layer we hit first. If it's the root, then the enclosing pagination
    // layer isn't in our subtree and we return nullptr. If we hit the enclosing pagination layer first, then
    // we can return it.
    for (const RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer == rootLayer)
            return nullptr;
        if (layer == paginationLayer)
            return paginationLayer;
    }
    
    // This should never be reached, since an enclosing layer should always either be the rootLayer or be
    // our enclosing pagination layer.
    ASSERT_NOT_REACHED();
    return nullptr;
}

void RenderLayer::collectFragments(LayerFragments& fragments, const RenderLayer* rootLayer, const LayoutRect& dirtyRect, PaginationInclusionMode inclusionMode,
    ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutSize& offsetFromRoot,
    const LayoutRect* layerBoundingBox, ShouldApplyRootOffsetToFragments applyRootOffsetToFragments)
{
    RenderLayer* paginationLayer = enclosingPaginationLayerInSubtree(rootLayer, inclusionMode);
    if (!paginationLayer || hasTransform()) {
        // For unpaginated layers, there is only one fragment.
        LayerFragment fragment;
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, offsetFromRoot);
        fragments.append(fragment);
        return;
    }
    
    // Compute our offset within the enclosing pagination layer.
    LayoutSize offsetWithinPaginatedLayer = offsetFromAncestor(paginationLayer);
    
    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(paginationLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
    LayoutRect layerBoundsInFragmentedFlow;
    ClipRect backgroundRectInFragmentedFlow;
    ClipRect foregroundRectInFragmentedFlow;
    calculateRects(paginationClipRectsContext, LayoutRect::infiniteRect(), layerBoundsInFragmentedFlow, backgroundRectInFragmentedFlow, foregroundRectInFragmentedFlow,
        offsetWithinPaginatedLayer);
    
    // Take our bounding box within the flow thread and clip it.
    LayoutRect layerBoundingBoxInFragmentedFlow = layerBoundingBox ? *layerBoundingBox : boundingBox(paginationLayer, offsetWithinPaginatedLayer);
    layerBoundingBoxInFragmentedFlow.intersect(backgroundRectInFragmentedFlow.rect());
    
    auto& enclosingFragmentedFlow = downcast<RenderFragmentedFlow>(paginationLayer->renderer());
    RenderLayer* parentPaginationLayer = paginationLayer->parent()->enclosingPaginationLayerInSubtree(rootLayer, inclusionMode);
    LayerFragments ancestorFragments;
    if (parentPaginationLayer) {
        // Compute a bounding box accounting for fragments.
        LayoutRect layerFragmentBoundingBoxInParentPaginationLayer = enclosingFragmentedFlow.fragmentsBoundingBox(layerBoundingBoxInFragmentedFlow);
        
        // Convert to be in the ancestor pagination context's coordinate space.
        LayoutSize offsetWithinParentPaginatedLayer = paginationLayer->offsetFromAncestor(parentPaginationLayer);
        layerFragmentBoundingBoxInParentPaginationLayer.move(offsetWithinParentPaginatedLayer);
        
        // Now collect ancestor fragments.
        parentPaginationLayer->collectFragments(ancestorFragments, rootLayer, dirtyRect, inclusionMode, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip,
            offsetFromAncestor(rootLayer), &layerFragmentBoundingBoxInParentPaginationLayer, ApplyRootOffsetToFragments);
        
        if (ancestorFragments.isEmpty())
            return;
        
        for (auto& ancestorFragment : ancestorFragments) {
            // Shift the dirty rect into flow thread coordinates.
            LayoutRect dirtyRectInFragmentedFlow(dirtyRect);
            dirtyRectInFragmentedFlow.move(-offsetWithinParentPaginatedLayer - ancestorFragment.paginationOffset);
            
            size_t oldSize = fragments.size();
            
            // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
            // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
            enclosingFragmentedFlow.collectLayerFragments(fragments, layerBoundingBoxInFragmentedFlow, dirtyRectInFragmentedFlow);
            
            size_t newSize = fragments.size();
            
            if (oldSize == newSize)
                continue;

            for (size_t i = oldSize; i < newSize; ++i) {
                LayerFragment& fragment = fragments.at(i);
                
                // Set our four rects with all clipping applied that was internal to the flow thread.
                fragment.setRects(layerBoundsInFragmentedFlow, backgroundRectInFragmentedFlow, foregroundRectInFragmentedFlow, &layerBoundingBoxInFragmentedFlow);
                
                // Shift to the root-relative physical position used when painting the flow thread in this fragment.
                fragment.moveBy(toLayoutPoint(ancestorFragment.paginationOffset + fragment.paginationOffset + offsetWithinParentPaginatedLayer));

                // Intersect the fragment with our ancestor's background clip so that e.g., columns in an overflow:hidden block are
                // properly clipped by the overflow.
                fragment.intersect(ancestorFragment.paginationClip);
                
                // Now intersect with our pagination clip. This will typically mean we're just intersecting the dirty rect with the column
                // clip, so the column clip ends up being all we apply.
                fragment.intersect(fragment.paginationClip);
                
                if (applyRootOffsetToFragments == ApplyRootOffsetToFragments)
                    fragment.paginationOffset = fragment.paginationOffset + offsetWithinParentPaginatedLayer;
            }
        }
        
        return;
    }
    
    // Shift the dirty rect into flow thread coordinates.
    LayoutSize offsetOfPaginationLayerFromRoot = enclosingPaginationLayer(inclusionMode)->offsetFromAncestor(rootLayer);
    LayoutRect dirtyRectInFragmentedFlow(dirtyRect);
    dirtyRectInFragmentedFlow.move(-offsetOfPaginationLayerFromRoot);

    // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
    // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
    enclosingFragmentedFlow.collectLayerFragments(fragments, layerBoundingBoxInFragmentedFlow, dirtyRectInFragmentedFlow);
    
    if (fragments.isEmpty())
        return;
    
    // Get the parent clip rects of the pagination layer, since we need to intersect with that when painting column contents.
    ClipRect ancestorClipRect = dirtyRect;
    if (paginationLayer->parent()) {
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        ancestorClipRect = paginationLayer->backgroundClipRect(clipRectsContext);
        ancestorClipRect.intersect(dirtyRect);
    }

    for (auto& fragment : fragments) {
        // Set our four rects with all clipping applied that was internal to the flow thread.
        fragment.setRects(layerBoundsInFragmentedFlow, backgroundRectInFragmentedFlow, foregroundRectInFragmentedFlow, &layerBoundingBoxInFragmentedFlow);
        
        // Shift to the root-relative physical position used when painting the flow thread in this fragment.
        fragment.moveBy(toLayoutPoint(fragment.paginationOffset + offsetOfPaginationLayerFromRoot));

        // Intersect the fragment with our ancestor's background clip so that e.g., columns in an overflow:hidden block are
        // properly clipped by the overflow.
        fragment.intersect(ancestorClipRect);

        // Now intersect with our pagination clip. This will typically mean we're just intersecting the dirty rect with the column
        // clip, so the column clip ends up being all we apply.
        fragment.intersect(fragment.paginationClip);
        
        if (applyRootOffsetToFragments == ApplyRootOffsetToFragments)
            fragment.paginationOffset = fragment.paginationOffset + offsetOfPaginationLayerFromRoot;
    }
}

void RenderLayer::updatePaintingInfoForFragments(LayerFragments& fragments, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintLayerFlag> localPaintFlags,
    bool shouldPaintContent, const LayoutSize& offsetFromRoot)
{
    for (auto& fragment : fragments) {
        fragment.shouldPaintContent = shouldPaintContent;
        if (this != localPaintingInfo.rootLayer || !(localPaintFlags & PaintLayerPaintingOverflowContents)) {
            LayoutSize newOffsetFromRoot = offsetFromRoot + fragment.paginationOffset;
            fragment.shouldPaintContent &= intersectsDamageRect(fragment.layerBounds, fragment.backgroundRect.rect(), localPaintingInfo.rootLayer, newOffsetFromRoot, fragment.hasBoundingBox ? &fragment.boundingBox : 0);
        }
    }
}

void RenderLayer::paintTransformedLayerIntoFragments(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(ExcludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintingInfo.paintBehavior);
    paginatedLayer->collectFragments(enclosingPaginationFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, ExcludeCompositedPaginatedLayers,
        (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
        (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, offsetOfPaginationLayerFromRoot, &transformedExtent);
    
    for (const auto& fragment : enclosingPaginationFragments) {
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(paintingInfo.rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));
    
            ClipRectsContext clipRectsContext(paginatedLayer, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.move(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        parent()->clipToRect(context, paintingInfo, clipRect);
        paintLayerByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset);
        parent()->restoreClip(context, paintingInfo, clipRect);
    }
}

void RenderLayer::paintBackgroundForFragments(const LayerFragments& layerFragments, GraphicsContext& context, GraphicsContext& contextForTransparencyLayer,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior,
    RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent)
            continue;

        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(contextForTransparencyLayer, localPaintingInfo, transparencyPaintDirtyRect);
    
        if (localPaintingInfo.clipToDirtyRect) {
            // Paint our background first, before painting any child layers.
            // Establish the clip used to paint our background.
            clipToRect(context, localPaintingInfo, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
        }
        
        // Paint the background.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::BlockBackground, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo, fragment.backgroundRect);
    }
}

void RenderLayer::paintForegroundForFragments(const LayerFragments& layerFragments, GraphicsContext& context, GraphicsContext& contextForTransparencyLayer,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior,
    RenderObject* subtreePaintRootForRenderer)
{
    // Begin transparency if we have something to paint.
    if (haveTransparency) {
        for (const auto& fragment : layerFragments) {
            if (fragment.shouldPaintContent && !fragment.foregroundRect.isEmpty()) {
                beginTransparencyLayers(contextForTransparencyLayer, localPaintingInfo, transparencyPaintDirtyRect);
                break;
            }
        }
    }

    OptionSet<PaintBehavior> localPaintBehavior;
    if (localPaintingInfo.paintBehavior & PaintBehavior::ForceBlackText)
        localPaintBehavior = PaintBehavior::ForceBlackText;
    else if (localPaintingInfo.paintBehavior & PaintBehavior::ForceWhiteText)
        localPaintBehavior = PaintBehavior::ForceWhiteText;
    else
        localPaintBehavior = paintBehavior;

    if (localPaintingInfo.paintBehavior & PaintBehavior::ExcludeSelection)
        localPaintBehavior.add(PaintBehavior::ExcludeSelection);
    
    if (localPaintingInfo.paintBehavior & PaintBehavior::Snapshotting)
        localPaintBehavior.add(PaintBehavior::Snapshotting);
    
    if (localPaintingInfo.paintBehavior & PaintBehavior::TileFirstPaint)
        localPaintBehavior.add(PaintBehavior::TileFirstPaint);

    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && layerFragments[0].shouldPaintContent && !layerFragments[0].foregroundRect.isEmpty();
    ClipRect clippedRect;
    if (shouldClip) {
        clippedRect = layerFragments[0].foregroundRect;
        clipToRect(context, localPaintingInfo, clippedRect);
    }
    
    // We have to loop through every fragment multiple times, since we have to repaint in each specific phase in order for
    // interleaving of the fragments to work properly.
    bool selectionOnly = localPaintingInfo.paintBehavior.containsAny({ PaintBehavior::SelectionAndBackgroundsOnly, PaintBehavior::SelectionOnly });
    paintForegroundForFragmentsWithPhase(selectionOnly ? PaintPhase::Selection : PaintPhase::ChildBlockBackgrounds, layerFragments,
        context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    
    if (!selectionOnly) {
        paintForegroundForFragmentsWithPhase(PaintPhase::Float, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhase::Foreground, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhase::ChildOutlines, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    }
    
    if (shouldClip)
        restoreClip(context, localPaintingInfo, clippedRect);
}

void RenderLayer::paintForegroundForFragmentsWithPhase(PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext& context,
    const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() > 1;

    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent || fragment.foregroundRect.isEmpty())
            continue;
        
        if (shouldClip)
            clipToRect(context, localPaintingInfo, fragment.foregroundRect);
    
        PaintInfo paintInfo(context, fragment.foregroundRect.rect(), phase, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this, localPaintingInfo.requireSecurityOriginAccessForWidgets);
        if (phase == PaintPhase::Foreground)
            paintInfo.overlapTestRequests = localPaintingInfo.overlapTestRequests;
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset));
        
        if (shouldClip)
            restoreClip(context, localPaintingInfo, fragment.foregroundRect);
    }
}

void RenderLayer::paintOutlineForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (fragment.backgroundRect.isEmpty())
            continue;
    
        // Paint our own outline
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::SelfOutline, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        clipToRect(context, localPaintingInfo, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius);
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset));
        restoreClip(context, localPaintingInfo, fragment.backgroundRect);
    }
}

void RenderLayer::paintMaskForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent)
            continue;

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(context, localPaintingInfo, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.
        
        // Paint the mask.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::Mask, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset));
        
        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo, fragment.backgroundRect);
    }
}

void RenderLayer::paintChildClippingMaskForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent)
            continue;

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(context, localPaintingInfo, fragment.foregroundRect, IncludeSelfForBorderRadius); // Child clipping mask painting will handle clipping to self.

        // Paint the clipped mask.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::ClippingMask, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo, fragment.foregroundRect);
    }
}

void RenderLayer::paintOverflowControlsForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo)
{
    for (const auto& fragment : layerFragments) {
        if (fragment.backgroundRect.isEmpty())
            continue;
        clipToRect(context, localPaintingInfo, fragment.backgroundRect);
        paintOverflowControls(context, roundedIntPoint(toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelOffset)),
            snappedIntRect(fragment.backgroundRect.rect()), true);
        restoreClip(context, localPaintingInfo, fragment.backgroundRect);
    }
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool RenderLayer::hitTest(const HitTestRequest& request, const HitTestLocation& hitTestLocation, HitTestResult& result)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());
    ASSERT(!renderer().view().needsLayout());
    
    updateLayerListsIfNeeded();

    ASSERT(!isRenderFragmentedFlow());
    LayoutRect hitTestArea = renderer().view().documentRect();
    if (!request.ignoreClipping()) {
        const auto& settings = renderer().settings();
        if (settings.visualViewportEnabled() && settings.clientCoordinatesRelativeToLayoutViewport()) {
            auto& frameView = renderer().view().frameView();
            LayoutRect absoluteLayoutViewportRect = frameView.layoutViewportRect();
            auto scaleFactor = frameView.frame().frameScaleFactor();
            if (scaleFactor > 1)
                absoluteLayoutViewportRect.scale(scaleFactor);
            hitTestArea.intersect(absoluteLayoutViewportRect);
        } else
            hitTestArea.intersect(renderer().view().frameView().visibleContentRect(LegacyIOSDocumentVisibleRect));
    }

    RenderLayer* insideLayer = hitTestLayer(this, nullptr, request, result, hitTestArea, hitTestLocation, false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down, 
        // return ourselves. We do this so mouse events continue getting delivered after a drag has 
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        if (!request.isChildFrameHitTest() && (request.active() || request.release()) && isRenderViewLayer()) {
            renderer().updateHitTestResult(result, downcast<RenderView>(renderer()).flipForWritingMode(hitTestLocation.point()));
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor - if the urlElement isn't already set.
    Node* node = result.innerNode();
    if (node && !result.URLElement())
        result.setURLElement(node->enclosingLinkEventParentOrSelf());

    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Element* RenderLayer::enclosingElement() const
{
    for (RenderElement* r = &renderer(); r; r = r->parent()) {
        if (Element* e = r->element())
            return e;
    }
    return nullptr;
}

RenderLayer* RenderLayer::enclosingFragmentedFlowAncestor() const
{
    RenderLayer* curr = parent();
    for (; curr && !curr->isRenderFragmentedFlow(); curr = curr->parent()) {
        if (curr->isStackingContext() && curr->isComposited()) {
            // We only adjust the position of the first level of layers.
            return nullptr;
        }
    }
    return curr;
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding where that
// ray intersects target, and computing the z delta between those two points.
static double computeZOffset(const HitTestingTransformState& transformState)
{
    // We got an affine transform, so no z-offset
    if (transformState.m_accumulatedTransform.isAffine())
        return 0;

    // Flatten the point into the target plane
    FloatPoint targetPoint = transformState.mappedPoint();
    
    // Now map the point back through the transform, which computes Z.
    FloatPoint3D backmappedPoint = transformState.m_accumulatedTransform.mapPoint(FloatPoint3D(targetPoint));
    return backmappedPoint.z();
}

Ref<HitTestingTransformState> RenderLayer::createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
                                        const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                        const HitTestingTransformState* containerTransformState,
                                        const LayoutSize& translationOffset) const
{
    RefPtr<HitTestingTransformState> transformState;
    LayoutSize offset;
    if (containerTransformState) {
        // If we're already computing transform state, then it's relative to the container (which we know is non-null).
        transformState = HitTestingTransformState::create(*containerTransformState);
        offset = offsetFromAncestor(containerLayer);
    } else {
        // If this is the first time we need to make transform state, then base it off of hitTestLocation,
        // which is relative to rootLayer.
        transformState = HitTestingTransformState::create(hitTestLocation.transformedPoint(), hitTestLocation.transformedRect(), FloatQuad(hitTestRect));
        offset = offsetFromAncestor(rootLayer);
    }
    offset += translationOffset;

    RenderObject* containerRenderer = containerLayer ? &containerLayer->renderer() : nullptr;
    if (renderer().shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        renderer().getTransformFromContainer(containerRenderer, offset, containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offset.width(), offset.height(), HitTestingTransformState::AccumulateTransform);
    }
    
    return transformState.releaseNonNull();
}


static bool isHitCandidate(const RenderLayer* hitLayer, bool canDepthSort, double* zOffset, const HitTestingTransformState* transformState)
{
    if (!hitLayer)
        return false;

    // The hit layer is depth-sorting with other layers, so just say that it was hit.
    if (canDepthSort)
        return true;
    
    // We need to look at z-depth to decide if this layer was hit.
    if (zOffset) {
        ASSERT(transformState);
        // This is actually computing our z, but that's OK because the hitLayer is coplanar with us.
        double childZOffset = computeZOffset(*transformState);
        if (childZOffset > *zOffset) {
            *zOffset = childZOffset;
            return true;
        }
        return false;
    }

    return true;
}

// hitTestLocation and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing flattening layer.
// transformState.m_lastPlanarPoint is the hitTestLocation in the plane of the containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of the containing flattening layer.
// 
// If zOffset is non-null (which indicates that the caller wants z offset information), 
//  *zOffset on return is the z offset of the hit point relative to the containing flattening layer.
RenderLayer* RenderLayer::hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                                       const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, bool appliedTransform,
                                       const HitTestingTransformState* transformState, double* zOffset)
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return nullptr;

    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        if (enclosingPaginationLayer(IncludeCompositedPaginatedLayers))
            return hitTestTransformedLayerInFragments(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);

        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRectsContext clipRectsContext(rootLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize);
            ClipRect clipRect = backgroundClipRect(clipRectsContext);
            // Test the enclosing clip now.
            if (!clipRect.intersects(hitTestLocation))
                return nullptr;
        }

        return hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
    updateCompositingAndLayerListsIfNeeded();
    update3DTransformedDescendantStatus();

    RefPtr<HitTestingTransformState> localTransformState;
    if (appliedTransform) {
        // We computed the correct state in the caller (above code), so just reference it.
        ASSERT(transformState);
        localTransformState = const_cast<HitTestingTransformState*>(transformState);
    } else if (transformState || has3DTransformedDescendant() || preserves3D()) {
        // We need transform state for the first time, or to offset the container state, so create it here.
        localTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState);
    }

    // Check for hit test on backface if backface-visibility is 'hidden'
    if (localTransformState && renderer().style().backfaceVisibility() == BackfaceVisibility::Hidden) {
        std::optional<TransformationMatrix> invertedMatrix = localTransformState->m_accumulatedTransform.inverse();
        // If the z-vector of the matrix is negative, the back is facing towards the viewer.
        if (invertedMatrix && invertedMatrix.value().m33() < 0)
            return nullptr;
    }

    RefPtr<HitTestingTransformState> unflattenedTransformState = localTransformState;
    if (localTransformState && !preserves3D()) {
        // Keep a copy of the pre-flattening state, for computing z-offsets for the container
        unflattenedTransformState = HitTestingTransformState::create(*localTransformState);
        // This layer is flattening, so flatten the state passed to descendants.
        localTransformState->flatten();
    }

    // The following are used for keeping track of the z-depth of the hit point of 3d-transformed
    // descendants.
    double localZOffset = -std::numeric_limits<double>::infinity();
    double* zOffsetForDescendantsPtr = nullptr;
    double* zOffsetForContentsPtr = nullptr;
    
    bool depthSortDescendants = false;
    if (preserves3D()) {
        depthSortDescendants = true;
        // Our layers can depth-test with our container, so share the z depth pointer with the container, if it passed one down.
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (zOffset) {
        zOffsetForDescendantsPtr = nullptr;
        // Container needs us to give back a z offset for the hit layer.
        zOffsetForContentsPtr = zOffset;
    }

    // This variable tracks which layer the mouse ends up being inside.
    RenderLayer* candidateLayer = nullptr;
#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(this);
#endif

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    auto* hitLayer = hitTestList(posZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestList(m_normalFlowList.get(), rootLayer, request, result, hitTestRect, hitTestLocation,
                           localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Collect the fragments. This will compute the clip rectangles for each layer fragment.
    LayerFragments layerFragments;
    collectFragments(layerFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers, RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip,
        offsetFromAncestor(rootLayer));

    if (canResize() && hitTestResizerInFragments(layerFragments, hitTestLocation)) {
        renderer().updateHitTestResult(result, hitTestLocation.point());
        return this;
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer. Check
    // every fragment in reverse order.
    if (isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we only want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentForegroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestDescendants, insideFragmentForegroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (request.resultIsElementList())
                result.append(tempResult, request);
            else
                result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        } else if (insideFragmentForegroundRect && request.resultIsElementList())
            result.append(tempResult, request);
    }

    // Now check our negative z-index children.
    hitLayer = hitTestList(negZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // If we found a layer, return. Child layers, and foreground always render in front of background.
    if (candidateLayer)
        return candidateLayer;

    if (isSelfPaintingLayer()) {
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentBackgroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestSelf, insideFragmentBackgroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (request.resultIsElementList())
                result.append(tempResult, request);
            else
                result = tempResult;
            return this;
        }
        if (insideFragmentBackgroundRect && request.resultIsElementList())
            result.append(tempResult, request);
    }

    return nullptr;
}

bool RenderLayer::hitTestContentsForFragments(const LayerFragments& layerFragments, const HitTestRequest& request, HitTestResult& result,
    const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter, bool& insideClipRect) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if ((hitTestFilter == HitTestSelf && !fragment.backgroundRect.intersects(hitTestLocation))
            || (hitTestFilter == HitTestDescendants && !fragment.foregroundRect.intersects(hitTestLocation)))
            continue;
        insideClipRect = true;
        if (hitTestContents(request, result, fragment.layerBounds, hitTestLocation, hitTestFilter))
            return true;
    }
    
    return false;
}

bool RenderLayer::hitTestResizerInFragments(const LayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(*this, snappedIntRect(fragment.layerBounds)).contains(hitTestLocation.roundedPoint()))
            return true;
    }
    
    return false;
}

RenderLayer* RenderLayer::hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, HitTestingTransparencyClipBox, RootOfTransparencyClipBox);
    paginatedLayer->collectFragments(enclosingPaginationFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers,
        RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip, offsetOfPaginationLayerFromRoot, &transformedExtent);

    for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);
        
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));
    
            ClipRectsContext clipRectsContext(paginatedLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.move(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }
        
        if (!hitTestLocation.intersects(clipRect))
            continue;

        RenderLayer* hitLayer = hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation,
            transformState, zOffset, fragment.paginationOffset);
        if (hitLayer)
            return hitLayer;
    }
    
    return nullptr;
}

RenderLayer* RenderLayer::hitTestLayerByApplyingTransform(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset,
    const LayoutSize& translationOffset)
{
    // Create a transform state to accumulate this transform.
    Ref<HitTestingTransformState> newTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState, translationOffset);

    // If the transform can't be inverted, then don't hit test this layer at all.
    if (!newTransformState->m_accumulatedTransform.isInvertible())
        return nullptr;

    // Compute the point and the hit test rect in the coords of this layer by using the values
    // from the transformState, which store the point and quad in the coords of the last flattened
    // layer, and the accumulated transform which lets up map through preserve-3d layers.
    //
    // We can't just map hitTestLocation and hitTestRect because they may have been flattened (losing z)
    // by our container.
    FloatPoint localPoint = newTransformState->mappedPoint();
    FloatQuad localPointQuad = newTransformState->mappedQuad();
    LayoutRect localHitTestRect = newTransformState->boundsOfMappedArea();
    HitTestLocation newHitTestLocation;
    if (hitTestLocation.isRectBasedTest())
        newHitTestLocation = HitTestLocation(localPoint, localPointQuad);
    else
        newHitTestLocation = HitTestLocation(localPoint);

    // Now do a hit test with the root layer shifted to be us.
    return hitTestLayer(this, containerLayer, request, result, localHitTestRect, newHitTestLocation, true, newTransformState.ptr(), zOffset);
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter) const
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (!renderer().hitTest(request, result, hitTestLocation, toLayoutPoint(layerBounds.location() - renderBoxLocation()), hitTestFilter)) {
        // It's wrong to set innerNode, but then claim that you didn't hit anything, unless it is
        // a rect-based test.
        ASSERT(!result.innerNode() || (request.resultIsElementList() && result.listBasedTestResult().size()));
        return false;
    }

    // For positioned generated content, we might still not have a
    // node by the time we get to the layer level, since none of
    // the content in the layer has an element. So just walk up
    // the tree.
    if (!result.innerNode() || !result.innerNonSharedNode()) {
        if (isOutOfFlowRenderFragmentedFlow()) {
            // The flowthread doesn't have an enclosing element, so when hitting the layer of the
            // flowthread (e.g. the descent area of the RootInlineBox for the image flowed alone
            // inside the flow thread) we're letting the hit testing continue so it will hit the region.
            return false;
        }

        Element* e = enclosingElement();
        if (!result.innerNode())
            result.setInnerNode(e);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(e);
    }
        
    return true;
}

RenderLayer* RenderLayer::hitTestList(Vector<RenderLayer*>* list, RenderLayer* rootLayer,
                                      const HitTestRequest& request, HitTestResult& result,
                                      const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                      const HitTestingTransformState* transformState, 
                                      double* zOffsetForDescendants, double* zOffset,
                                      const HitTestingTransformState* unflattenedTransformState,
                                      bool depthSortDescendants)
{
    if (!list)
        return nullptr;

    if (!hasSelfPaintingLayerDescendant())
        return nullptr;

    RenderLayer* resultLayer = nullptr;
    for (size_t i = list->size(); i > 0; --i) {
        RenderLayer* childLayer = list->at(i - 1);
        RenderLayer* hitLayer = nullptr;
        HitTestResult tempResult(result.hitTestLocation());
        hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it is a list-based test, we can safely append the temporary result since it might had hit
        // nodes but not necesserily had hitLayer set.
        ASSERT(!result.isRectBasedTest() || request.resultIsElementList());
        if (request.resultIsElementList())
            result.append(tempResult, request);

        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!request.resultIsElementList())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

Ref<ClipRects> RenderLayer::updateClipRects(const ClipRectsContext& clipRectsContext)
{
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    ASSERT(clipRectsType < NumCachedClipRectsTypes);
    if (m_clipRectsCache) {
        if (auto* clipRects = m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip)) {
            ASSERT(clipRectsContext.rootLayer == m_clipRectsCache->m_clipRectsRoot[clipRectsType]);
            ASSERT(m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] == clipRectsContext.overlayScrollbarSizeRelevancy);
        
#ifdef CHECK_CACHED_CLIP_RECTS
            // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
            ClipRectsContext tempContext(clipRectsContext);
            tempContext.clipRectsType = TemporaryClipRects;
            Ref<ClipRects> tempClipRects = ClipRects::create();
            calculateClipRects(tempContext, tempClipRects);
            ASSERT(tempClipRects.get() == *clipRects);
#endif
            return *clipRects; // We have the correct cached value.
        }
    }
    
    if (!m_clipRectsCache)
        m_clipRectsCache = std::make_unique<ClipRectsCache>();
#ifndef NDEBUG
    m_clipRectsCache->m_clipRectsRoot[clipRectsType] = clipRectsContext.rootLayer;
    m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] = clipRectsContext.overlayScrollbarSizeRelevancy;
#endif

    RefPtr<ClipRects> parentClipRects;
    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    if (auto* parentLayer = (clipRectsContext.rootLayer != this ? parent() : nullptr))
        parentClipRects = parentLayer->updateClipRects(clipRectsContext);

    auto clipRects = ClipRects::create();
    calculateClipRects(clipRectsContext, clipRects);

    if (parentClipRects && *parentClipRects == clipRects) {
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, parentClipRects.copyRef());
        return parentClipRects.releaseNonNull();
    }
    m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, clipRects.copyRef());
    return clipRects;
}

ClipRects* RenderLayer::clipRects(const ClipRectsContext& context) const
{
    ASSERT(context.clipRectsType < NumCachedClipRectsTypes);
    if (!m_clipRectsCache)
        return nullptr;
    return m_clipRectsCache->getClipRects(context.clipRectsType, context.respectOverflowClip);
}

void RenderLayer::calculateClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    if (!parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset();
        return;
    }
    
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    bool useCached = clipRectsType != TemporaryClipRects;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = clipRectsContext.rootLayer != this ? parent() : nullptr;
    
    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipRects(clipRectsContext))
            clipRects = *parentLayer->clipRects(clipRectsContext);
        else {
            ClipRectsContext parentContext(clipRectsContext);
            parentContext.overlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize; // FIXME: why?
            parentLayer->calculateClipRects(parentContext, clipRects);
        }
    } else
        clipRects.reset();

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (renderer().isFixedPositioned()) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (renderer().isInFlowPositioned())
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    else if (renderer().isAbsolutelyPositioned())
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    
    // Update the clip rects that will be passed to child layers.
#if PLATFORM(IOS)
    if (renderer().hasClipOrOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || this != clipRectsContext.rootLayer)) {
#else
    if ((renderer().hasOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || this != clipRectsContext.rootLayer)) || renderer().hasClip()) {
#endif
        // This layer establishes a clip of some kind.

        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the RenderLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        LayoutPoint offset(renderer().localToContainerPoint(FloatPoint(), &clipRectsContext.rootLayer->renderer()));
        if (clipRects.fixed() && &clipRectsContext.rootLayer->renderer() == &renderer().view())
            offset -= toLayoutSize(renderer().view().frameView().scrollPositionForFixedPosition());
        
        if (renderer().hasOverflowClip()) {
            ClipRect newOverflowClip = downcast<RenderBox>(renderer()).overflowClipRectForChildLayers(offset, nullptr, clipRectsContext.overlayScrollbarSizeRelevancy);
            newOverflowClip.setAffectedByRadius(renderer().style().hasBorderRadius());
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer().isPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (renderer().hasClip()) {
            LayoutRect newPosClip = downcast<RenderBox>(renderer()).clipRect(offset, nullptr);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

Ref<ClipRects> RenderLayer::parentClipRects(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(parent());

    if (clipRectsContext.clipRectsType == TemporaryClipRects) {
        auto parentClipRects = ClipRects::create();
        parent()->calculateClipRects(clipRectsContext, parentClipRects);
        return parentClipRects;
    }

    return parent()->updateClipRects(clipRectsContext);
}

static inline ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, PositionType position)
{
    if (position == PositionType::Fixed)
        return parentRects.fixedClipRect();

    if (position == PositionType::Absolute)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect RenderLayer::backgroundClipRect(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(parent());
    auto computeParentRects = [this, &clipRectsContext] () {
        // If we cross into a different pagination context, then we can't rely on the cache.
        // Just switch over to using TemporaryClipRects.
        if (clipRectsContext.clipRectsType != TemporaryClipRects
            && parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers) != enclosingPaginationLayer(IncludeCompositedPaginatedLayers)) {
            ClipRectsContext tempContext(clipRectsContext);
            tempContext.clipRectsType = TemporaryClipRects;
            return parentClipRects(tempContext);
        }
        return parentClipRects(clipRectsContext);
    };
    
    auto parentRects = computeParentRects();
    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, renderer().style().position());
    RenderView& view = renderer().view();
    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects->fixed() && &clipRectsContext.rootLayer->renderer() == &view && !backgroundClipRect.isInfinite())
        backgroundClipRect.moveBy(view.frameView().scrollPositionForFixedPosition());
    return backgroundClipRect;
}

void RenderLayer::calculateRects(const ClipRectsContext& clipRectsContext, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, const LayoutSize& offsetFromRoot) const
{
    if (clipRectsContext.rootLayer != this && parent()) {
        backgroundRect = backgroundClipRect(clipRectsContext);
        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;

    LayoutSize offsetFromRootLocal = offsetFromRoot;

    if (clipRectsContext.rootLayer->isOutOfFlowRenderFragmentedFlow()) {
        LayoutPoint absPos = LayoutPoint(renderer().view().localToAbsolute(FloatPoint(), IsFixed));
        offsetFromRootLocal += toLayoutSize(absPos);
    }

    layerBounds = LayoutRect(toLayoutPoint(offsetFromRootLocal), size());

    foregroundRect = backgroundRect;

    // Update the clip rects that will be passed to child layers.
    if (renderer().hasClipOrOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (renderer().hasOverflowClip() && (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)) {
            foregroundRect.intersect(downcast<RenderBox>(renderer()).overflowClipRect(toLayoutPoint(offsetFromRootLocal), nullptr, clipRectsContext.overlayScrollbarSizeRelevancy));
            if (renderer().style().hasBorderRadius())
                foregroundRect.setAffectedByRadius(true);
        }

        if (renderer().hasClip()) {
            // Clip applies to *us* as well, so update the damageRect.
            LayoutRect newPosClip = downcast<RenderBox>(renderer()).clipRect(toLayoutPoint(offsetFromRootLocal), nullptr);
            backgroundRect.intersect(newPosClip);
            foregroundRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then make sure our background rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (renderBox()->hasVisualOverflow()) {
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = renderBox()->visualOverflowRect();
            renderBox()->flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // Shift the bounds to be for our region only.
            LayoutRect bounds = renderBox()->borderBoxRectInFragment(nullptr);

            bounds.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(bounds);
        }
    }
}

LayoutRect RenderLayer::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect;
    ClipRect foregroundRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, TemporaryClipRects);
    // Need to use temporary clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    calculateRects(clipRectsContext, LayoutRect::infiniteRect(), layerBounds, backgroundRect, foregroundRect, offsetFromAncestor(clipRectsContext.rootLayer));
    if (foregroundRect.rect().isInfinite())
        return renderer().view().unscaledDocumentRect();

    auto absoluteClippingRect = clippingRootLayer->renderer().localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox();
    return intersection(absoluteClippingRect, renderer().view().unscaledDocumentRect());
}

LayoutRect RenderLayer::clipRectRelativeToAncestor(RenderLayer* ancestor, LayoutSize offsetFromAncestor, const LayoutRect& constrainingRect) const
{
    LayoutRect layerBounds;
    ClipRect backgroundRect;
    ClipRect foregroundRect;
    auto clipRectType = !m_enclosingPaginationLayer || m_enclosingPaginationLayer == ancestor ? PaintingClipRects : TemporaryClipRects;
    ClipRectsContext clipRectsContext(ancestor, clipRectType);
    calculateRects(clipRectsContext, constrainingRect, layerBounds, backgroundRect, foregroundRect, offsetFromAncestor);
    return backgroundRect.rect();
}

LayoutRect RenderLayer::selfClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect clipRect = clipRectRelativeToAncestor(clippingRootLayer, offsetFromAncestor(clippingRootLayer), renderer().view().documentRect());
    return clippingRootLayer->renderer().localToAbsoluteQuad(FloatQuad(clipRect)).enclosingBoundingBox();
}

LayoutRect RenderLayer::localClipRect(bool& clipExceedsBounds) const
{
    clipExceedsBounds = false;
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutSize offsetFromRoot = offsetFromAncestor(clippingRootLayer);
    LayoutRect clipRect = clipRectRelativeToAncestor(clippingRootLayer, offsetFromRoot, LayoutRect::infiniteRect());
    if (clipRect.isInfinite())
        return clipRect;

    if (renderer().hasClip()) {
        // CSS clip may be larger than our border box.
        LayoutRect cssClipRect = downcast<RenderBox>(renderer()).clipRect(toLayoutPoint(offsetFromRoot), nullptr);
        clipExceedsBounds = !clipRect.contains(cssClipRect);
    }

    clipRect.move(-offsetFromRoot);
    return clipRect;
}

void RenderLayer::addBlockSelectionGapsBounds(const LayoutRect& bounds)
{
    m_blockSelectionGapsBounds.unite(enclosingIntRect(bounds));
}

void RenderLayer::clearBlockSelectionGapsBounds()
{
    m_blockSelectionGapsBounds = IntRect();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->clearBlockSelectionGapsBounds();
}

void RenderLayer::repaintBlockSelectionGaps()
{
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->repaintBlockSelectionGaps();

    if (m_blockSelectionGapsBounds.isEmpty())
        return;

    LayoutRect rect = m_blockSelectionGapsBounds;
    rect.moveBy(-scrollPosition());
    if (renderer().hasOverflowClip() && !usesCompositedScrolling())
        rect.intersect(downcast<RenderBox>(renderer()).overflowClipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (renderer().hasClip())
        rect.intersect(downcast<RenderBox>(renderer()).clipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (!rect.isEmpty())
        renderer().repaintRectangle(rect);
}

bool RenderLayer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot, const LayoutRect* cachedBoundingBox) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (isRenderViewLayer() || renderer().isDocumentElementRenderer())
        return true;

    if (damageRect.isInfinite())
        return true;

    if (damageRect.isEmpty())
        return false;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we can return true.
    if (!renderer().isRenderInline() && layerBounds.intersects(damageRect))
        return true;

    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect. It's possible the fragment computed the bounding box already, in which case we
    // can use the cached value.
    if (cachedBoundingBox)
        return cachedBoundingBox->intersects(damageRect);
    
    return boundingBox(rootLayer, offsetFromRoot).intersects(damageRect);
}

LayoutRect RenderLayer::localBoundingBox(OptionSet<CalculateLayerBoundsFlag> flags) const
{
    // There are three special cases we need to consider.
    // (1) Inline Flows.  For inline flows we will create a bounding box that fully encompasses all of the lines occupied by the
    // inline.  In other words, if some <span> wraps to three lines, we'll create a bounding box that fully encloses the
    // line boxes of all three lines (including overflow on those lines).
    // (2) Left/Top Overflow.  The width/height of layers already includes right/bottom overflow.  However, in the case of left/top
    // overflow, we have to create a bounding box that will extend to include this overflow.
    // (3) Floats.  When a layer has overhanging floats that it paints, we need to make sure to include these overhanging floats
    // as part of our bounding box.  We do this because we are the responsible layer for both hit testing and painting those
    // floats.
    LayoutRect result;
    if (renderer().isInline() && is<RenderInline>(renderer()))
        result = downcast<RenderInline>(renderer()).linesVisualOverflowBoundingBox();
    else if (is<RenderTableRow>(renderer())) {
        auto& tableRow = downcast<RenderTableRow>(renderer());
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderTableCell* cell = tableRow.firstCell(); cell; cell = cell->nextCell()) {
            LayoutRect bbox = cell->borderBoxRect();
            result.unite(bbox);
            LayoutRect overflowRect = tableRow.visualOverflowRect();
            if (bbox != overflowRect)
                result.unite(overflowRect);
        }
    } else {
        RenderBox* box = renderBox();
        ASSERT(box);
        if (!(flags & DontConstrainForMask) && box->hasMask()) {
            result = box->maskClipRect(LayoutPoint());
            box->flipForWritingMode(result); // The mask clip rect is in physical coordinates, so we have to flip, since localBoundingBox is not.
        } else {
            LayoutRect bbox = box->borderBoxRect();
            result = bbox;
            LayoutRect overflowRect = box->visualOverflowRect();
            if (bbox != overflowRect)
                result.unite(overflowRect);
        }
    }
    return result;
}

LayoutRect RenderLayer::boundingBox(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, OptionSet<CalculateLayerBoundsFlag> flags) const
{    
    LayoutRect result = localBoundingBox(flags);
    if (renderer().view().frameView().hasFlippedBlockRenderers()) {
        if (renderer().isBox())
            renderBox()->flipForWritingMode(result);
        else
            renderer().containingBlock()->flipForWritingMode(result);
    }

    PaginationInclusionMode inclusionMode = ExcludeCompositedPaginatedLayers;
    if (flags & UseFragmentBoxesIncludingCompositing)
        inclusionMode = IncludeCompositedPaginatedLayers;

    const RenderLayer* paginationLayer = nullptr;
    if (flags.containsAny({ UseFragmentBoxesExcludingCompositing, UseFragmentBoxesIncludingCompositing }))
        paginationLayer = enclosingPaginationLayerInSubtree(ancestorLayer, inclusionMode);
    
    const RenderLayer* childLayer = this;
    bool isPaginated = paginationLayer;
    while (paginationLayer) {
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        result.move(childLayer->offsetFromAncestor(paginationLayer));

        auto& enclosingFragmentedFlow = downcast<RenderFragmentedFlow>(paginationLayer->renderer());
        result = enclosingFragmentedFlow.fragmentsBoundingBox(result);
        
        childLayer = paginationLayer;
        paginationLayer = paginationLayer->parent()->enclosingPaginationLayerInSubtree(ancestorLayer, inclusionMode);
    }

    if (isPaginated) {
        result.move(childLayer->offsetFromAncestor(ancestorLayer));
        return result;
    }
    
    result.move(offsetFromRoot);
    return result;
}

bool RenderLayer::getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(LayoutRect& bounds, OptionSet<CalculateLayerBoundsFlag> additionalFlags) const
{
    // The animation will override the display transform, so don't include it.
    auto boundsFlags = additionalFlags | (defaultCalculateLayerBoundsFlags() - IncludeSelfTransform);

    bounds = calculateLayerBounds(this, LayoutSize(), boundsFlags);
    
    LayoutRect animatedBounds = bounds;
    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (auto* timeline = renderer().documentTimeline()) {
            if (timeline->computeExtentOfAnimation(renderer(), animatedBounds)) {
                bounds = animatedBounds;
                return true;
            }
        }
    } else {
        if (renderer().animation().computeExtentOfAnimation(renderer(), animatedBounds)) {
            bounds = animatedBounds;
            return true;
        }
    }
    
    return false;
}

IntRect RenderLayer::absoluteBoundingBox() const
{
    const RenderLayer* rootLayer = root();
    return snappedIntRect(boundingBox(rootLayer, offsetFromAncestor(rootLayer)));
}

FloatRect RenderLayer::absoluteBoundingBoxForPainting() const
{
    const RenderLayer* rootLayer = root();
    return snapRectToDevicePixels(boundingBox(rootLayer, offsetFromAncestor(rootLayer)), renderer().document().deviceScaleFactor());
}

LayoutRect RenderLayer::calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, OptionSet<CalculateLayerBoundsFlag> flags) const
{
    if (!isSelfPaintingLayer())
        return LayoutRect();

    // FIXME: This could be improved to do a check like hasVisibleNonCompositingDescendantLayers() (bug 92580).
    if ((flags & ExcludeHiddenDescendants) && this != ancestorLayer && !hasVisibleContent() && !hasVisibleDescendant())
        return LayoutRect();

    if (isRenderViewLayer()) {
        // The root layer is always just the size of the document.
        return renderer().view().unscaledDocumentRect();
    }

    LayoutRect boundingBoxRect = localBoundingBox(flags);
    if (renderer().view().frameView().hasFlippedBlockRenderers()) {
        if (is<RenderBox>(renderer()))
            downcast<RenderBox>(renderer()).flipForWritingMode(boundingBoxRect);
        else
            renderer().containingBlock()->flipForWritingMode(boundingBoxRect);
    }

    if (renderer().isDocumentElementRenderer()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxFillLayers().
        const FrameView& frameView = renderer().view().frameView();
        boundingBoxRect.setWidth(std::max(boundingBoxRect.width(), frameView.contentsWidth() - boundingBoxRect.x()));
        boundingBoxRect.setHeight(std::max(boundingBoxRect.height(), frameView.contentsHeight() - boundingBoxRect.y()));
    }

    LayoutRect unionBounds = boundingBoxRect;

    if (flags & UseLocalClipRectIfPossible) {
        bool clipExceedsBounds = false;
        LayoutRect localClipRect = this->localClipRect(clipExceedsBounds);
        if (!localClipRect.isInfinite() && !clipExceedsBounds) {
            if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehavior::Normal))
                localClipRect = transform()->mapRect(localClipRect);

            localClipRect.move(offsetFromAncestor(ancestorLayer));
            return localClipRect;
        }
    }

    // FIXME: should probably just pass 'flags' down to descendants.
    auto descendantFlags = defaultCalculateLayerBoundsFlags() | (flags & ExcludeHiddenDescendants) | (flags & IncludeCompositedDescendants);

    const_cast<RenderLayer*>(this)->updateLayerListsIfNeeded();

    if (RenderLayer* reflection = reflectionLayer()) {
        if (!reflection->isComposited()) {
            LayoutRect childUnionBounds = reflection->calculateLayerBounds(this, reflection->offsetFromAncestor(this), descendantFlags);
            unionBounds.unite(childUnionBounds);
        }
    }
    
    ASSERT(isStackingContext() || (!posZOrderList() || !posZOrderList()->size()));

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(this));
#endif

    auto computeLayersUnion = [this, &unionBounds, flags, descendantFlags] (const RenderLayer& childLayer) {
        if (!(flags & IncludeCompositedDescendants) && childLayer.isComposited())
            return;
        LayoutRect childBounds = childLayer.calculateLayerBounds(this, childLayer.offsetFromAncestor(this), descendantFlags);
        // Ignore child layer (and behave as if we had overflow: hidden) when it is positioned off the parent layer so much
        // that we hit the max LayoutUnit value.
        unionBounds.checkedUnite(childBounds);
    };

    if (auto* negZOrderList = this->negZOrderList()) {
        for (auto* childLayer : *negZOrderList)
            computeLayersUnion(*childLayer);
    }

    if (auto* posZOrderList = this->posZOrderList()) {
        for (auto* childLayer : *posZOrderList)
            computeLayersUnion(*childLayer);
    }

    if (auto* normalFlowList = this->normalFlowList()) {
        for (auto* childLayer : *normalFlowList)
            computeLayersUnion(*childLayer);
    }

    // FIXME: We can optimize the size of the composited layers, by not enlarging
    // filtered areas with the outsets if we know that the filter is going to render in hardware.
    // https://bugs.webkit.org/show_bug.cgi?id=81239
    if (flags & IncludeLayerFilterOutsets)
        renderer().style().filterOutsets().expandRect(unionBounds);

    if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehavior::Normal)) {
        TransformationMatrix* affineTrans = transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }
    unionBounds.move(offsetFromRoot);
    return unionBounds;
}

void RenderLayer::clearClipRectsIncludingDescendants(ClipRectsType typeToClear)
{
    // FIXME: it's not clear how this layer not having clip rects guarantees that no descendants have any.
    if (!m_clipRectsCache)
        return;

    clearClipRects(typeToClear);
    
    for (RenderLayer* l = firstChild(); l; l = l->nextSibling())
        l->clearClipRectsIncludingDescendants(typeToClear);
}

void RenderLayer::clearClipRects(ClipRectsType typeToClear)
{
    if (typeToClear == AllClipRectTypes)
        m_clipRectsCache = nullptr;
    else {
        ASSERT(typeToClear < NumCachedClipRectsTypes);
        m_clipRectsCache->setClipRects(typeToClear, RespectOverflowClip, nullptr);
        m_clipRectsCache->setClipRects(typeToClear, IgnoreOverflowClip, nullptr);
    }
}

RenderLayerBacking* RenderLayer::ensureBacking()
{
    if (!m_backing) {
        m_backing = std::make_unique<RenderLayerBacking>(*this);
        compositor().layerBecameComposited(*this);

        updateFilterPaintingStrategy();
    }
    return m_backing.get();
}

void RenderLayer::clearBacking(bool layerBeingDestroyed)
{
    if (m_backing && !renderer().renderTreeBeingDestroyed())
        compositor().layerBecameNonComposited(*this);
    m_backing = nullptr;

    if (!layerBeingDestroyed)
        updateFilterPaintingStrategy();
}

bool RenderLayer::hasCompositedMask() const
{
    return m_backing && m_backing->hasMaskLayer();
}

GraphicsLayer* RenderLayer::layerForScrolling() const
{
    return m_backing ? m_backing->scrollingContentsLayer() : nullptr;
}

GraphicsLayer* RenderLayer::layerForHorizontalScrollbar() const
{
    return m_backing ? m_backing->layerForHorizontalScrollbar() : nullptr;
}

GraphicsLayer* RenderLayer::layerForVerticalScrollbar() const
{
    return m_backing ? m_backing->layerForVerticalScrollbar() : nullptr;
}

GraphicsLayer* RenderLayer::layerForScrollCorner() const
{
    return m_backing ? m_backing->layerForScrollCorner() : nullptr;
}

bool RenderLayer::paintsWithTransform(OptionSet<PaintBehavior> paintBehavior) const
{
    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
    return transform() && ((paintBehavior & PaintBehavior::FlattenCompositingLayers) || paintsToWindow);
}

bool RenderLayer::shouldPaintMask(OptionSet<PaintBehavior> paintBehavior, OptionSet<PaintLayerFlag> paintFlags) const
{
    if (!renderer().hasMask())
        return false;

    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
    if (paintsToWindow || (paintBehavior & PaintBehavior::FlattenCompositingLayers))
        return true;

    return paintFlags.contains(PaintLayerPaintingCompositingMaskPhase);
}

bool RenderLayer::shouldApplyClipPath(OptionSet<PaintBehavior> paintBehavior, OptionSet<PaintLayerFlag> paintFlags) const
{
    if (!renderer().hasClipPath())
        return false;

    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
    if (paintsToWindow || (paintBehavior & PaintBehavior::FlattenCompositingLayers))
        return true;

    return paintFlags.contains(PaintLayerPaintingCompositingClipPathPhase);
}

bool RenderLayer::scrollingMayRevealBackground() const
{
    return scrollsOverflow() || usesCompositedScrolling();
}

bool RenderLayer::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return false;

    if (paintsWithTransparency(PaintBehavior::Normal))
        return false;

    if (renderer().isDocumentElementRenderer()) {
        // Normally the document element doens't have a layer.  If it does have a layer, its background propagates to the RenderView
        // so this layer doesn't draw it.
        return false;
    }

    // We can't use hasVisibleContent(), because that will be true if our renderer is hidden, but some child
    // is visible and that child doesn't cover the entire rect.
    if (renderer().style().visibility() != Visibility::Visible)
        return false;

    if (paintsWithFilters() && renderer().style().filter().hasFilterThatAffectsOpacity())
        return false;

    // FIXME: Handle simple transforms.
    if (paintsWithTransform(PaintBehavior::Normal))
        return false;

    // FIXME: Remove this check.
    // This function should not be called when layer-lists are dirty.
    // It is somehow getting triggered during style update.
    if (m_zOrderListsDirty || m_normalFlowListDirty)
        return false;

    // Table painting is special; a table paints its sections.
    if (renderer().isTablePart())
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (renderer().backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;
    
    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (renderer().hasOverflowClip())
        return false;
    
    return listBackgroundIsKnownToBeOpaqueInRect(posZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(negZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(normalFlowList(), localRect);
}

bool RenderLayer::listBackgroundIsKnownToBeOpaqueInRect(const Vector<RenderLayer*>* list, const LayoutRect& localRect) const
{
    if (!list || list->isEmpty())
        return false;

    for (auto iter = list->rbegin(); iter != list->rend(); ++iter) {
        const RenderLayer* childLayer = *iter;
        if (childLayer->isComposited())
            continue;

        if (!childLayer->canUseConvertToLayerCoords())
            continue;

        LayoutRect childLocalRect(localRect);
        childLocalRect.move(-childLayer->offsetFromAncestor(this));

        if (childLayer->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
    }
    return false;
}

void RenderLayer::setParent(RenderLayer* parent)
{
    if (parent == m_parent)
        return;

    if (m_parent && !renderer().renderTreeBeingDestroyed())
        compositor().layerWillBeRemoved(*m_parent, *this);
    
    m_parent = parent;

    if (m_parent && !renderer().renderTreeBeingDestroyed())
        compositor().layerWasAdded(*m_parent, *this);
}

void RenderLayer::dirtyZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isStackingContext());

    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;

    if (!renderer().renderTreeBeingDestroyed())
        compositor().setCompositingLayersNeedRebuild();
}

void RenderLayer::dirtyStackingContextZOrderLists()
{
    RenderLayer* sc = stackingContext();
    if (sc)
        sc->dirtyZOrderLists();
}

void RenderLayer::dirtyNormalFlowList()
{
    ASSERT(m_layerListMutationAllowed);

    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

    if (!renderer().renderTreeBeingDestroyed())
        compositor().setCompositingLayersNeedRebuild();
}

void RenderLayer::rebuildZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isDirtyStackingContext());
    rebuildZOrderLists(m_posZOrderList, m_negZOrderList);
    m_zOrderListsDirty = false;
}

void RenderLayer::rebuildZOrderLists(std::unique_ptr<Vector<RenderLayer*>>& posZOrderList, std::unique_ptr<Vector<RenderLayer*>>& negZOrderList)
{
    bool includeHiddenLayers = compositor().inCompositingMode();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        if (!m_reflection || reflectionLayer() != child)
            child->collectLayers(includeHiddenLayers, posZOrderList, negZOrderList);

    // Sort the two lists.
    if (posZOrderList)
        std::stable_sort(posZOrderList->begin(), posZOrderList->end(), compareZIndex);

    if (negZOrderList)
        std::stable_sort(negZOrderList->begin(), negZOrderList->end(), compareZIndex);
}

void RenderLayer::updateNormalFlowList()
{
    if (!m_normalFlowListDirty)
        return;

    ASSERT(m_layerListMutationAllowed);

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Ignore non-overflow layers and reflections.
        if (child->isNormalFlowOnly() && (!m_reflection || reflectionLayer() != child)) {
            if (!m_normalFlowList)
                m_normalFlowList = std::make_unique<Vector<RenderLayer*>>();
            m_normalFlowList->append(child);
        }
    }
    
    m_normalFlowListDirty = false;
}

void RenderLayer::collectLayers(bool includeHiddenLayers, std::unique_ptr<Vector<RenderLayer*>>& posBuffer, std::unique_ptr<Vector<RenderLayer*>>& negBuffer)
{
    updateDescendantDependentFlags();

    bool isStacking = isStackingContext();
    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    bool includeHiddenLayer = includeHiddenLayers || (m_hasVisibleContent || (m_hasVisibleDescendant && isStacking));
    if (includeHiddenLayer && !isNormalFlowOnly()) {
        // Determine which buffer the child should be in.
        std::unique_ptr<Vector<RenderLayer*>>& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

        // Create the buffer if it doesn't exist yet.
        if (!buffer)
            buffer = std::make_unique<Vector<RenderLayer*>>();
        
        // Append ourselves at the end of the appropriate buffer.
        buffer->append(this);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context/container.
    if ((includeHiddenLayers || m_hasVisibleDescendant) && !isStacking) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!m_reflection || reflectionLayer() != child)
                child->collectLayers(includeHiddenLayers, posBuffer, negBuffer);
        }
    }
}

void RenderLayer::updateLayerListsIfNeeded()
{
    updateZOrderLists();
    updateNormalFlowList();

    if (RenderLayer* reflectionLayer = this->reflectionLayer()) {
        reflectionLayer->updateZOrderLists();
        reflectionLayer->updateNormalFlowList();
    }
}

void RenderLayer::updateCompositingAndLayerListsIfNeeded()
{
    if (compositor().inCompositingMode()) {
        if (isDirtyStackingContext() || m_normalFlowListDirty)
            compositor().updateCompositingLayers(CompositingUpdateType::OnHitTest, this);
        return;
    }

    updateLayerListsIfNeeded();
}

void RenderLayer::repaintIncludingDescendants()
{
    renderer().repaint();
    for (RenderLayer* current = firstChild(); current; current = current->nextSibling())
        current->repaintIncludingDescendants();
}

void RenderLayer::setBackingNeedsRepaint(GraphicsLayer::ShouldClipToLayer shouldClip)
{
    ASSERT(isComposited());
    if (backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        renderer().view().repaintViewRectangle(absoluteBoundingBox());
    } else
        backing()->setContentsNeedDisplay(shouldClip);
}

void RenderLayer::setBackingNeedsRepaintInRect(const LayoutRect& r, GraphicsLayer::ShouldClipToLayer shouldClip)
{
    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(isComposited());
    if (!isComposited() || backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        LayoutRect absRect(r);
        absRect.move(offsetFromAncestor(root()));

        renderer().view().repaintViewRectangle(absRect);
    } else
        backing()->setContentsNeedDisplayInRect(r, shouldClip);
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayer::repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer)
{
    renderer().repaintUsingContainer(repaintContainer, renderer().clippedOverflowRectForRepaint(repaintContainer));

    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

static bool createsStackingContext(const RenderLayer& layer)
{
    auto& renderer = layer.renderer();
    return renderer.hasTransformRelatedProperty()
        || renderer.hasClipPath()
        || renderer.hasFilter()
        || renderer.hasMask()
        || renderer.hasBackdropFilter()
#if ENABLE(CSS_COMPOSITING)
        || renderer.hasBlendMode()
#endif
        || renderer.isTransparent()
        || renderer.isPositioned()
        || renderer.hasReflection()
        || renderer.style().hasIsolation()
#if PLATFORM(IOS)
        || layer.canUseAcceleratedTouchScrolling()
#endif
        || (renderer.style().willChange() && renderer.style().willChange()->canCreateStackingContext());
}

bool RenderLayer::shouldBeNormalFlowOnly() const
{
    if (createsStackingContext(*this))
        return false;

    return renderer().hasOverflowClip()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || (renderer().style().specifiesColumns() && !isRenderViewLayer())
        || renderer().isInFlowRenderFragmentedFlow();
}

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    if (!isNormalFlowOnly())
        return true;

    return hasOverlayScrollbars()
        || renderer().isTableRow()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || renderer().isInFlowRenderFragmentedFlow();
}

void RenderLayer::updateSelfPaintingLayer()
{
    bool isSelfPaintingLayer = shouldBeSelfPaintingLayer();
    if (m_isSelfPaintingLayer == isSelfPaintingLayer)
        return;

    m_isSelfPaintingLayer = isSelfPaintingLayer;
    if (!parent())
        return;
    if (isSelfPaintingLayer)
        parent()->setAncestorChainHasSelfPaintingLayerDescendant();
    else
        parent()->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
}

static bool hasVisibleBoxDecorationsOrBackground(const RenderElement& renderer)
{
    return renderer.hasVisibleBoxDecorations() || renderer.style().hasOutline();
}

static bool styleHasSmoothingTextMode(const RenderStyle& style)
{
    FontSmoothingMode smoothingMode = style.fontDescription().fontSmoothing();
    return smoothingMode == FontSmoothingMode::AutoSmoothing || smoothingMode == FontSmoothingMode::SubpixelAntialiased;
}

// Constrain the depth and breadth of the search for performance.
static const unsigned maxRendererTraversalCount = 200;

static void determineNonLayerDescendantsPaintedContent(const RenderElement& renderer, unsigned& renderersTraversed, RenderLayer::PaintedContentRequest& request)
{
    for (const auto& child : childrenOfType<RenderObject>(renderer)) {
        if (++renderersTraversed > maxRendererTraversalCount) {
            request.makeStatesUndetermined();
            return;
        }

        if (is<RenderText>(child)) {
            const auto& renderText = downcast<RenderText>(child);
            if (renderText.linesBoundingBox().isEmpty())
                continue;

            if (renderer.style().userSelect() != UserSelect::None)
                request.setHasPaintedContent();

            if (!renderText.text().isAllSpecialCharacters<isHTMLSpace>()) {
                request.setHasPaintedContent();

                if (request.needToDetermineSubpixelAntialiasedTextState() && styleHasSmoothingTextMode(child.style()))
                    request.setHasSubpixelAntialiasedText();
            }

            if (request.isSatisfied())
                return;
        }
        
        if (!is<RenderElement>(child))
            continue;
        
        const RenderElement& renderElementChild = downcast<RenderElement>(child);

        if (is<RenderLayerModelObject>(renderElementChild) && downcast<RenderLayerModelObject>(renderElementChild).hasSelfPaintingLayer())
            continue;

        if (hasVisibleBoxDecorationsOrBackground(renderElementChild)) {
            request.setHasPaintedContent();
            if (request.isSatisfied())
                return;
        }
        
        if (is<RenderReplaced>(renderElementChild)) {
            request.setHasPaintedContent();

            if (is<RenderImage>(renderElementChild) && request.needToDetermineSubpixelAntialiasedTextState()) {
                auto& imageRenderer = downcast<RenderImage>(renderElementChild);
                // May draw text if showing alt text, or image is an SVG image or PDF image.
                if ((imageRenderer.isShowingAltText() || imageRenderer.hasNonBitmapImage()) && styleHasSmoothingTextMode(child.style()))
                    request.setHasSubpixelAntialiasedText();
            }

            if (request.isSatisfied())
                return;
        }

        determineNonLayerDescendantsPaintedContent(renderElementChild, renderersTraversed, request);
        if (request.isSatisfied())
            return;
    }
}

bool RenderLayer::hasNonEmptyChildRenderers(PaintedContentRequest& request) const
{
    unsigned renderersTraversed = 0;
    determineNonLayerDescendantsPaintedContent(renderer(), renderersTraversed, request);
    return request.probablyHasPaintedContent();
}

bool RenderLayer::hasVisibleBoxDecorationsOrBackground() const
{
    return WebCore::hasVisibleBoxDecorationsOrBackground(renderer());
}

bool RenderLayer::hasVisibleBoxDecorations() const
{
    if (!hasVisibleContent())
        return false;

    return hasVisibleBoxDecorationsOrBackground() || hasOverflowControls();
}

bool RenderLayer::isVisuallyNonEmpty(PaintedContentRequest* request) const
{
    ASSERT(!m_visibleDescendantStatusDirty);

    if (!hasVisibleContent() || !renderer().style().opacity())
        return false;

    if (renderer().isRenderReplaced() || hasOverflowControls()) {
        if (!request)
            return true;

        request->setHasPaintedContent();
        if (request->isSatisfied())
            return true;
    }

    if (hasVisibleBoxDecorationsOrBackground()) {
        if (!request)
            return true;

        request->setHasPaintedContent();
        if (request->isSatisfied())
            return true;
    }

    PaintedContentRequest localRequest;
    if (!request)
        request = &localRequest;
    return hasNonEmptyChildRenderers(*request);
}

void RenderLayer::updateStackingContextsAfterStyleChange(const RenderStyle* oldStyle)
{
    if (!oldStyle)
        return;

    bool wasStackingContext = isStackingContext(oldStyle);
    bool isStackingContext = this->isStackingContext();
    if (isStackingContext != wasStackingContext) {
        dirtyStackingContextZOrderLists();
        if (isStackingContext)
            dirtyZOrderLists();
        else
            clearZOrderLists();

#if ENABLE(CSS_COMPOSITING)
        if (parent()) {
            if (isStackingContext) {
                if (!hasNotIsolatedBlendingDescendantsStatusDirty() && hasNotIsolatedBlendingDescendants())
                    parent()->dirtyAncestorChainHasBlendingDescendants();
            } else {
                if (hasNotIsolatedBlendingDescendantsStatusDirty())
                    parent()->dirtyAncestorChainHasBlendingDescendants();
                else if (hasNotIsolatedBlendingDescendants())
                    parent()->updateAncestorChainHasBlendingDescendants();
            }
        }
#endif

        return;
    }

    // FIXME: RenderLayer already handles visibility changes through our visiblity dirty bits. This logic could
    // likely be folded along with the rest.
    if (oldStyle->zIndex() != renderer().style().zIndex() || oldStyle->visibility() != renderer().style().visibility()) {
        dirtyStackingContextZOrderLists();
        if (isStackingContext)
            dirtyZOrderLists();
    }
}

void RenderLayer::updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle)
{
    // Overflow are a box concept.
    RenderBox* box = renderBox();
    if (!box)
        return;

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().appearance() == ListboxPart)
        return;

    Overflow overflowX = box->style().overflowX();
    Overflow overflowY = box->style().overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = box->hasOverflowClip() && ((hasHorizontalScrollbar() && styleDefinesAutomaticScrollbar(box->style(), HorizontalScrollbar)) || styleRequiresScrollbar(box->style(), HorizontalScrollbar));
    bool needsVerticalScrollbar = box->hasOverflowClip() && ((hasVerticalScrollbar() && styleDefinesAutomaticScrollbar(box->style(), VerticalScrollbar)) || styleRequiresScrollbar(box->style(), VerticalScrollbar));
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With non-overlay overflow:scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (m_hBar && needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == Overflow::Scroll && overflowX != Overflow::Scroll)
        m_hBar->setEnabled(true);

    if (m_vBar && needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == Overflow::Scroll && overflowY != Overflow::Scroll)
        m_vBar->setEnabled(true);

    if (!m_scrollDimensionsDirty)
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

void RenderLayer::styleChanged(StyleDifference diff, const RenderStyle* oldStyle)
{
    bool isNormalFlowOnly = shouldBeNormalFlowOnly();
    if (isNormalFlowOnly != m_isNormalFlowOnly) {
        m_isNormalFlowOnly = isNormalFlowOnly;
        RenderLayer* p = parent();
        if (p)
            p->dirtyNormalFlowList();
        dirtyStackingContextZOrderLists();
    }

    if (renderer().isHTMLMarquee() && renderer().style().marqueeBehavior() != MarqueeBehavior::None && renderer().isBox()) {
        if (!m_marquee)
            m_marquee = std::make_unique<RenderMarquee>(this);
        m_marquee->updateMarqueeStyle();
    }
    else if (m_marquee) {
        m_marquee = nullptr;
    }

    updateScrollbarsAfterStyleChange(oldStyle);
    updateStackingContextsAfterStyleChange(oldStyle);
    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();

    if (!hasReflection() && m_reflection)
        removeReflection();
    else if (hasReflection()) {
        if (!m_reflection)
            createReflection();
        else
            m_reflection->setStyle(createReflectionStyle());
    }
    
    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
    
    updateScrollCornerStyle();
    updateResizerStyle();

    updateDescendantDependentFlags();
    updateTransform();
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode();
#endif
    updateFiltersAfterStyleChange();
    
    compositor().layerStyleChanged(diff, *this, oldStyle);

    updateFilterPaintingStrategy();

#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
    if (diff == StyleDifference::RecompositeLayer || diff >= StyleDifference::LayoutPositionedMovementOnly)
        renderer().document().setTouchEventRegionsNeedUpdate();
#else
    UNUSED_PARAM(diff);
#endif
}

void RenderLayer::updateScrollableAreaSet(bool hasOverflow)
{
    FrameView& frameView = renderer().view().frameView();

    bool isVisibleToHitTest = renderer().visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frameView.frame().ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    bool isScrollable = hasOverflow && isVisibleToHitTest;
    bool addedOrRemoved = false;
    
    ASSERT(m_registeredScrollableArea == frameView.containsScrollableArea(this));
    
    if (isScrollable) {
        if (!m_registeredScrollableArea) {
            addedOrRemoved = frameView.addScrollableArea(this);
            m_registeredScrollableArea = true;
        }
    } else if (m_registeredScrollableArea) {
        addedOrRemoved = frameView.removeScrollableArea(this);
        m_registeredScrollableArea = false;
    }

#if ENABLE(IOS_TOUCH_EVENTS)
    if (addedOrRemoved) {
        if (isScrollable && !canUseAcceleratedTouchScrolling())
            registerAsTouchEventListenerForScrolling();
        else {
            // We only need the touch listener for unaccelerated overflow scrolling, so if we became
            // accelerated, remove ourselves as a touch event listener.
            unregisterAsTouchEventListenerForScrolling();
        }
    }
#else
    UNUSED_VARIABLE(addedOrRemoved);
#endif
}

void RenderLayer::updateScrollCornerStyle()
{
    RenderElement* actualRenderer = rendererForScrollbar(renderer());
    auto corner = renderer().hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(PseudoId::ScrollbarCorner), &actualRenderer->style()) : nullptr;

    if (!corner) {
        clearScrollCorner();
        return;
    }

    if (!m_scrollCorner) {
        m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer().document(), WTFMove(*corner));
        // FIXME: A renderer should be a child of its parent!
        m_scrollCorner->setParent(&renderer());
        m_scrollCorner->initializeStyle();
    } else
        m_scrollCorner->setStyle(WTFMove(*corner));
}

void RenderLayer::clearScrollCorner()
{
    if (!m_scrollCorner)
        return;
    m_scrollCorner->setParent(nullptr);
    m_scrollCorner = nullptr;
}

void RenderLayer::updateResizerStyle()
{
    RenderElement* actualRenderer = rendererForScrollbar(renderer());
    auto resizer = renderer().hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(PseudoId::Resizer), &actualRenderer->style()) : nullptr;

    if (!resizer) {
        clearResizer();
        return;
    }

    if (!m_resizer) {
        m_resizer = createRenderer<RenderScrollbarPart>(renderer().document(), WTFMove(*resizer));
        // FIXME: A renderer should be a child of its parent!
        m_resizer->setParent(&renderer());
        m_resizer->initializeStyle();
    } else
        m_resizer->setStyle(WTFMove(*resizer));
}

void RenderLayer::clearResizer()
{
    if (!m_resizer)
        return;
    m_resizer->setParent(nullptr);
    m_resizer = nullptr;
}

RenderLayer* RenderLayer::reflectionLayer() const
{
    return m_reflection ? m_reflection->layer() : nullptr;
}

void RenderLayer::createReflection()
{
    ASSERT(!m_reflection);
    m_reflection = createRenderer<RenderReplica>(renderer().document(), createReflectionStyle());
    // FIXME: A renderer should be a child of its parent!
    m_reflection->setParent(&renderer()); // We create a 1-way connection.
    m_reflection->initializeStyle();
}

void RenderLayer::removeReflection()
{
    if (!m_reflection->renderTreeBeingDestroyed())
        m_reflection->removeLayers(this);

    m_reflection->setParent(nullptr);
    m_reflection = nullptr;
}

RenderStyle RenderLayer::createReflectionStyle()
{
    auto newStyle = RenderStyle::create();
    newStyle.inheritFrom(renderer().style());
    
    // Map in our transform.
    TransformOperations transform;
    switch (renderer().style().boxReflect()->direction()) {
        case ReflectionBelow:
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionAbove:
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
            break;
        case ReflectionRight:
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionLeft:
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            break;
    }
    newStyle.setTransform(transform);

    // Map in our mask.
    newStyle.setMaskBoxImage(renderer().style().boxReflect()->mask());
    
    // Style has transform and mask, so needs to be stacking context.
    newStyle.setZIndex(0);

    return newStyle;
}

void RenderLayer::ensureLayerFilters()
{
    if (m_filters)
        return;
    
    m_filters = std::make_unique<RenderLayerFilters>(*this);
}

void RenderLayer::clearLayerFilters()
{
    m_filters = nullptr;
}

void RenderLayer::updateFiltersAfterStyleChange()
{
    if (!hasFilter()) {
        clearLayerFilters();
        return;
    }

    // Add the filter as a client to this renderer, unless we are a RenderLayer accommodating
    // an SVG. In that case it takes care of its own resource management for filters.
    if (renderer().style().filter().hasReferenceFilter() && !renderer().isSVGRoot()) {
        ensureLayerFilters();
        m_filters->updateReferenceFilterClients(renderer().style().filter());
    } else if (m_filters)
        m_filters->removeReferenceFilterClients();
}

void RenderLayer::updateFilterPaintingStrategy()
{
    // RenderLayerFilters is only used to render the filters in software mode,
    // so we always need to run updateFilterPaintingStrategy() after the composited
    // mode might have changed for this layer.
    if (!paintsWithFilters()) {
        // Don't delete the whole filter info here, because we might use it
        // for loading SVG reference filter files.
        if (m_filters)
            m_filters->setFilter(nullptr);

        // Early-return only if we *don't* have reference filters.
        // For reference filters, we still want the FilterEffect graph built
        // for us, even if we're composited.
        if (!renderer().style().filter().hasReferenceFilter())
            return;
    }
    
    ensureLayerFilters();
    m_filters->buildFilter(renderer(), page().deviceScaleFactor(), renderer().settings().acceleratedFiltersEnabled() ? Accelerated : Unaccelerated);
}

void RenderLayer::filterNeedsRepaint()
{
    // We use the enclosing element so that we recalculate style for the ancestor of an anonymous object.
    if (Element* element = enclosingElement())
        element->invalidateStyleAndLayerComposition();
    renderer().repaint();
}

TextStream& operator<<(TextStream& ts, const RenderLayer& layer)
{
    ts << "RenderLayer " << &layer << " " << layer.size();
    if (layer.transform())
        ts << " has transform";
    if (layer.hasFilter())
        ts << " has filter";
    if (layer.hasBackdropFilter())
        ts << " has backdrop filter";
    if (layer.hasBlendMode())
        ts << " has blend mode";
    if (layer.isolatesBlending())
        ts << " isolates blending";
    if (layer.isComposited())
        ts << " " << *layer.backing();
    return ts;
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    WTF::String output = externalRepresentation(&layer->renderer().frame(), WebCore::RenderAsTextShowAllLayers | WebCore::RenderAsTextShowLayerNesting | WebCore::RenderAsTextShowCompositedLayers | WebCore::RenderAsTextShowAddresses | WebCore::RenderAsTextShowIDAndClass | WebCore::RenderAsTextDontUpdateLayout | WebCore::RenderAsTextShowLayoutState | WebCore::RenderAsTextShowOverflow | WebCore::RenderAsTextShowSVGGeometry | WebCore::RenderAsTextShowLayerFragments);
    fprintf(stderr, "\n%s\n", output.utf8().data());
}

void showLayerTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}

#endif
