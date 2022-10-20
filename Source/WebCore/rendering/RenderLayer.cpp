/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Adobe. All rights reserved.
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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
#include "CSSFilter.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "DebugPageOverlays.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "Element.h"
#include "ElementInlines.h"
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
#include "InspectorInstrumentation.h"
#include "LegacyRenderSVGForeignObject.h"
#include "LegacyRenderSVGRoot.h"
#include "Logging.h"
#include "OverflowEvent.h"
#include "OverlapTestRequestClient.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "ReferencedSVGResources.h"
#include "RenderAncestorIterator.h"
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
#include "RenderLayerScrollableArea.h"
#include "RenderMarquee.h"
#include "RenderMultiColumnFlow.h"
#include "RenderReplica.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderTreeMutationDisallowedScope.h"
#include "RenderView.h"
#include "SVGNames.h"
#include "ScaleTransformOperation.h"
#include "ScrollAnimator.h"
#include "ScrollSnapOffsetsInfo.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceGraphic.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "Styleable.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include "WheelEventTestMonitor.h"
#include <stdio.h>
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

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

    void setOverflowClipRectAffectedByRadius() { m_overflowClipRect.setAffectedByRadius(true); }

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
#if ASSERT_ENABLED
        for (int i = 0; i < NumCachedClipRectsTypes; ++i) {
            m_clipRectsRoot[i] = 0;
            m_scrollbarRelevancy[i] = IgnoreOverlayScrollbarSize;
        }
#endif
    }

    ClipRects* getClipRects(ClipRectsType clipRectsType, bool respectOverflowClip) const
    {
        return m_clipRects[getIndex(clipRectsType, respectOverflowClip)].get();
    }

    void setClipRects(ClipRectsType clipRectsType, bool respectOverflowClip, RefPtr<ClipRects>&& clipRects)
    {
        m_clipRects[getIndex(clipRectsType, respectOverflowClip)] = WTFMove(clipRects);
    }

#if ASSERT_ENABLED
    const RenderLayer* m_clipRectsRoot[NumCachedClipRectsTypes];
    OverlayScrollbarSizeRelevancy m_scrollbarRelevancy[NumCachedClipRectsTypes];
#endif

private:
    unsigned getIndex(ClipRectsType clipRectsType, bool respectOverflowClip) const
    {
        unsigned index = static_cast<unsigned>(clipRectsType);
        if (respectOverflowClip)
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

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, const ClipRects& clipRects)
{
    TextStream::GroupScope scope(ts);
    ts << indent << "ClipRects\n";
    ts << indent << "  overflow  : " << clipRects.overflowClipRect() << "\n";
    ts << indent << "  fixed     : " << clipRects.fixedClipRect() << "\n";
    ts << indent << "  positioned: " << clipRects.posClipRect() << "\n";

    return ts;
}

#endif

static ScrollingScope nextScrollingScope()
{
    static ScrollingScope currentScope = 0;
    return ++currentScope;
}

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLayer);

RenderLayer::RenderLayer(RenderLayerModelObject& renderer)
    : m_isRenderViewLayer(renderer.isRenderView())
    , m_forcedStackingContext(renderer.isMedia())
    , m_isNormalFlowOnly(false)
    , m_isCSSStackingContext(false)
    , m_isOpportunisticStackingContext(false)
    , m_zOrderListsDirty(false)
    , m_normalFlowListDirty(true)
    , m_hadNegativeZOrderList(false)
    , m_inResizeMode(false)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_repaintStatus(NeedsNormalRepaint)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_isFixedIntersectingViewport(false)
    , m_behavesAsFixed(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_hasCompositingDescendant(false)
    , m_hasCompositedNonContainedDescendants(false)
    , m_hasCompositedScrollingAncestor(false)
    , m_hasFixedContainingBlockAncestor(false)
    , m_hasTransformedAncestor(false)
    , m_has3DTransformedAncestor(false)
    , m_insideSVGForeignObject(false)
    , m_indirectCompositingReason(static_cast<unsigned>(IndirectCompositingReason::None))
    , m_viewportConstrainedNotCompositedReason(NoNotCompositedReason)
#if ASSERT_ENABLED
    , m_layerListMutationAllowed(true)
#endif
#if ENABLE(CSS_COMPOSITING)
    , m_blendMode(static_cast<unsigned>(BlendMode::Normal))
    , m_hasNotIsolatedCompositedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendantsStatusDirty(false)
#endif
    , m_repaintRectsValid(false)
    , m_renderer(renderer)
{
    setIsNormalFlowOnly(shouldBeNormalFlowOnly());
    setIsCSSStackingContext(shouldBeCSSStackingContext());

    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    if (isRenderViewLayer())
        m_boxScrollingScope = m_contentsScrollingScope = nextScrollingScope();

    if (renderer.firstChild())
        return;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // Leave m_visibleContentStatusDirty = true in any case. The associated renderer needs to be inserted into the
    // render tree, before we can determine the visible content status. The visible content status of a SVG renderer
    // depends on its ancestors (all children of RenderSVGHiddenContainer are recursively invisible, no matter what).
    if (renderer.isSVGLayerAwareRenderer() && renderer.document().settings().layerBasedSVGEngineEnabled())
        return;
#endif

    m_visibleContentStatusDirty = false;
    m_hasVisibleContent = renderer.style().visibility() == Visibility::Visible;
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode())
        renderer().frame().eventHandler().resizeLayerDestroyed();

    if (m_reflection)
        removeReflection();

    clearLayerScrollableArea();
    clearLayerFilters();

    if (paintsIntoProvidedBacking()) {
        auto* backingProviderLayer = this->backingProviderLayer();
        if (backingProviderLayer->backing())
            backingProviderLayer->backing()->removeBackingSharingLayer(*this);
    }

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearBacking(true);

    // Layer and all its children should be removed from the tree before destruction.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(renderer().renderTreeBeingDestroyed() || !parent());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(renderer().renderTreeBeingDestroyed() || !firstChild());
}

void RenderLayer::addChild(RenderLayer& child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child.setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(&child);
        ASSERT(prevSibling != &child);
    } else
        setFirstChild(&child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(&child);
        child.setNextSibling(beforeChild);
        ASSERT(beforeChild != &child);
    } else
        setLastChild(&child);

    child.setParent(this);

    dirtyPaintOrderListsOnChildChange(child);

    child.updateAncestorDependentState();
    child.updateDescendantDependentFlags();
    if (child.m_hasVisibleContent || child.m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child.isSelfPaintingLayer() || child.hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

    if (compositor().hasContentCompositingLayers())
        setDescendantsNeedCompositingRequirementsTraversal();

    if (child.hasDescendantNeedingCompositingRequirementsTraversal() || child.needsCompositingRequirementsTraversal())
        child.setAncestorsHaveCompositingDirtyFlag(Compositing::HasDescendantNeedingRequirementsTraversal);

    if (child.hasDescendantNeedingUpdateBackingOrHierarchyTraversal() || child.needsUpdateBackingOrHierarchyTraversal())
        child.setAncestorsHaveCompositingDirtyFlag(Compositing::HasDescendantNeedingBackingOrHierarchyTraversal);

#if ENABLE(CSS_COMPOSITING)
    if (child.hasBlendMode() || (child.hasNotIsolatedBlendingDescendants() && !child.isolatesBlending()))
        updateAncestorChainHasBlendingDescendants(); // Why not just dirty?
#endif
}

void RenderLayer::removeChild(RenderLayer& oldChild)
{
    if (!renderer().renderTreeBeingDestroyed())
        compositor().layerWillBeRemoved(*this, oldChild);

    // remove the child
    if (oldChild.previousSibling())
        oldChild.previousSibling()->setNextSibling(oldChild.nextSibling());
    if (oldChild.nextSibling())
        oldChild.nextSibling()->setPreviousSibling(oldChild.previousSibling());

    if (m_first == &oldChild)
        m_first = oldChild.nextSibling();
    if (m_last == &oldChild)
        m_last = oldChild.previousSibling();

    dirtyPaintOrderListsOnChildChange(oldChild);

    oldChild.setPreviousSibling(nullptr);
    oldChild.setNextSibling(nullptr);
    oldChild.setParent(nullptr);
    
    oldChild.updateDescendantDependentFlags();
    if (oldChild.m_hasVisibleContent || oldChild.m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild.isSelfPaintingLayer() || oldChild.hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    if (compositor().hasContentCompositingLayers())
        setDescendantsNeedCompositingRequirementsTraversal();

#if ENABLE(CSS_COMPOSITING)
    if (oldChild.hasBlendMode() || (oldChild.hasNotIsolatedBlendingDescendants() && !oldChild.isolatesBlending()))
        dirtyAncestorChainHasBlendingDescendants();
#endif
}

void RenderLayer::dirtyPaintOrderListsOnChildChange(RenderLayer& child)
{
    if (child.isNormalFlowOnly())
        dirtyNormalFlowList();

    if (!child.isNormalFlowOnly() || child.firstChild()) {
        // Dirty the z-order list in which we are contained. The stackingContext() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child.dirtyStackingContextZOrderLists();
    }
}

void RenderLayer::insertOnlyThisLayer(LayerChangeTiming timing)
{
    if (!m_parent && renderer().parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        auto* parentLayer = renderer().layerParent();
        if (!parentLayer)
            return;

        auto* beforeChild = parentLayer->reflectionLayer() != this ? renderer().layerNextSibling(*parentLayer) : nullptr;
        parentLayer->addChild(*this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (auto& child : childrenOfType<RenderElement>(renderer()))
        child.moveLayers(*this);

    if (parent()) {
        if (timing == LayerChangeTiming::StyleChange)
            renderer().view().layerChildrenChangedDuringStyleChange(*parent());
    }
    
    // Clear out all the clip rects.
    clearClipRectsIncludingDescendants();
}

void RenderLayer::removeOnlyThisLayer(LayerChangeTiming timing)
{
    if (!m_parent)
        return;

    if (timing == LayerChangeTiming::StyleChange)
        renderer().view().layerChildrenChangedDuringStyleChange(*parent());

    // Mark that we are about to lose our layer. This makes render tree
    // walks ignore this layer while we're removing it.
    renderer().setHasLayer(false);

    compositor().layerWillBeRemoved(*m_parent, *this);

    // Dirty the clip rects.
    clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (auto* reflectionLayer = this->reflectionLayer())
        removeChild(*reflectionLayer);

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(*current);
        m_parent->addChild(*current, nextSib);
        current->setRepaintStatus(NeedsFullRepaint);
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(*this);
    renderer().destroyLayer();
}

static bool canCreateStackingContext(const RenderLayer& layer)
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
        || renderer.isPositioned() // Note that this only creates stacking context in conjunction with explicit z-index.
        || renderer.hasReflection()
        || renderer.style().hasIsolation()
        || renderer.shouldApplyPaintContainment()
        || !renderer.style().hasAutoUsedZIndex()
        || (renderer.style().willChange() && renderer.style().willChange()->canCreateStackingContext())
        || layer.establishesTopLayer();
}

bool RenderLayer::shouldBeNormalFlowOnly() const
{
    if (canCreateStackingContext(*this))
        return false;

    return renderer().hasNonVisibleOverflow()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || (renderer().style().specifiesColumns() && !isRenderViewLayer())
        || renderer().isRenderFragmentedFlow();
}

bool RenderLayer::shouldBeCSSStackingContext() const
{
    return !renderer().style().hasAutoUsedZIndex() || renderer().shouldApplyLayoutOrPaintContainment() || isRenderViewLayer();
}

bool RenderLayer::setIsNormalFlowOnly(bool isNormalFlowOnly)
{
    if (isNormalFlowOnly == m_isNormalFlowOnly)
        return false;
    
    m_isNormalFlowOnly = isNormalFlowOnly;

    if (auto* p = parent())
        p->dirtyNormalFlowList();
    dirtyStackingContextZOrderLists();
    return true;
}

void RenderLayer::isStackingContextChanged()
{
    dirtyStackingContextZOrderLists();
    if (isStackingContext())
        dirtyZOrderLists();
    else
        clearZOrderLists();
}

bool RenderLayer::setIsOpportunisticStackingContext(bool isStacking)
{
    bool wasStacking = isStackingContext();
    m_isOpportunisticStackingContext = isStacking;
    if (wasStacking == isStackingContext())
        return false;

    isStackingContextChanged();
    return true;
}

bool RenderLayer::setIsCSSStackingContext(bool isCSSStackingContext)
{
    bool wasStacking = isStackingContext();
    m_isCSSStackingContext = isCSSStackingContext;
    if (wasStacking == isStackingContext())
        return false;

    isStackingContextChanged();
    return true;
}

void RenderLayer::setParent(RenderLayer* parent)
{
    if (parent == m_parent)
        return;

    if (m_parent && !renderer().renderTreeBeingDestroyed())
        compositor().layerWillBeRemoved(*m_parent, *this);

    m_parent = parent;
}

RenderLayer* RenderLayer::stackingContext() const
{
    auto* layer = parent();
    while (layer && !layer->isStackingContext())
        layer = layer->parent();

    ASSERT(!layer || layer->isStackingContext());
    ASSERT_IMPLIES(establishesTopLayer(), !layer || layer == renderer().view().layer());
    return layer;
}

void RenderLayer::dirtyZOrderLists()
{
    ASSERT(layerListMutationAllowed());
    ASSERT(isStackingContext());

    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;

    // FIXME: Ideally, we'd only dirty if the lists changed.
    if (hasCompositingDescendant())
        setNeedsCompositingPaintOrderChildrenUpdate();
}

void RenderLayer::dirtyStackingContextZOrderLists()
{
    if (auto* sc = stackingContext())
        sc->dirtyZOrderLists();
}

bool RenderLayer::willCompositeClipPath() const
{
    if (!isComposited())
        return false;

    auto* clipPath = renderer().style().clipPath();
    if (!clipPath)
        return false;

    if (renderer().hasMask())
        return false;

    return (clipPath->type() != PathOperation::Shape || clipPath->type() == PathOperation::Shape) && GraphicsLayer::supportsLayerType(GraphicsLayer::Type::Shape);
}

void RenderLayer::dirtyNormalFlowList()
{
    ASSERT(layerListMutationAllowed());

    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

    if (hasCompositingDescendant())
        setNeedsCompositingPaintOrderChildrenUpdate();
}

void RenderLayer::updateNormalFlowList()
{
    if (!m_normalFlowListDirty)
        return;

    ASSERT(layerListMutationAllowed());

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Ignore non-overflow layers and reflections.
        if (child->isNormalFlowOnly() && !isReflectionLayer(*child)) {
            if (!m_normalFlowList)
                m_normalFlowList = makeUnique<Vector<RenderLayer*>>();
            m_normalFlowList->append(child);
        }
    }

    if (m_normalFlowList)
        m_normalFlowList->shrinkToFit();

    m_normalFlowListDirty = false;
}

void RenderLayer::rebuildZOrderLists()
{
    ASSERT(layerListMutationAllowed());
    ASSERT(isDirtyStackingContext());
    
    OptionSet<Compositing> childDirtyFlags;
    rebuildZOrderLists(m_posZOrderList, m_negZOrderList, childDirtyFlags);
    m_zOrderListsDirty = false;
    
    bool hasNegativeZOrderList = m_negZOrderList && m_negZOrderList->size();
    // Having negative z-order lists affect whether a compositing layer needs a foreground layer.
    // Ideally we'd only trigger this when having z-order children changes, but we blow away the old z-order
    // lists on dirtying so we don't know the old state.
    if (hasNegativeZOrderList != m_hadNegativeZOrderList) {
        m_hadNegativeZOrderList = hasNegativeZOrderList;
        if (isComposited())
            setNeedsCompositingConfigurationUpdate();
    }

    // Building lists may have added layers with dirty flags, so make sure we propagate dirty bits up the tree.
    if (m_compositingDirtyBits.containsAll({ Compositing::DescendantsNeedRequirementsTraversal, Compositing::DescendantsNeedBackingAndHierarchyTraversal }))
        return;

    if (childDirtyFlags.containsAny(computeCompositingRequirementsFlags()))
        setDescendantsNeedCompositingRequirementsTraversal();

    if (childDirtyFlags.containsAny(updateBackingOrHierarchyFlags()))
        setDescendantsNeedUpdateBackingAndHierarchyTraversal();
}

void RenderLayer::rebuildZOrderLists(std::unique_ptr<Vector<RenderLayer*>>& posZOrderList, std::unique_ptr<Vector<RenderLayer*>>& negZOrderList, OptionSet<Compositing>& accumulatedDirtyFlags)
{
    bool includeHiddenLayers = compositor().usesCompositing();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (!isReflectionLayer(*child))
            child->collectLayers(includeHiddenLayers, posZOrderList, negZOrderList, accumulatedDirtyFlags);
    }

    auto compareZIndex = [] (const RenderLayer* first, const RenderLayer* second) -> bool {
        return first->zIndex() < second->zIndex();
    };

    // Sort the two lists.
    if (posZOrderList) {
        std::stable_sort(posZOrderList->begin(), posZOrderList->end(), compareZIndex);
        posZOrderList->shrinkToFit();
    }

    if (negZOrderList) {
        std::stable_sort(negZOrderList->begin(), negZOrderList->end(), compareZIndex);
        negZOrderList->shrinkToFit();
    }

    if (isRenderViewLayer() && renderer().document().hasTopLayerElement()) {
        auto topLayerLayers = topLayerRenderLayers(renderer().view());
        if (topLayerLayers.size()) {
            if (!posZOrderList)
                posZOrderList = makeUnique<Vector<RenderLayer*>>();

            posZOrderList->appendVector(topLayerLayers);
        }
    }
}

void RenderLayer::collectLayers(bool includeHiddenLayers, std::unique_ptr<Vector<RenderLayer*>>& positiveZOrderList, std::unique_ptr<Vector<RenderLayer*>>& negativeZOrderList, OptionSet<Compositing>& accumulatedDirtyFlags)
{
    updateDescendantDependentFlags();

    if (establishesTopLayer())
        return;

    bool isStacking = isStackingContext();
    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    bool includeHiddenLayer = includeHiddenLayers || (m_hasVisibleContent || (m_hasVisibleDescendant && isStacking));
    if (includeHiddenLayer && !isNormalFlowOnly()) {
        auto& layerList = (zIndex() >= 0) ? positiveZOrderList : negativeZOrderList;
        if (!layerList)
            layerList = makeUnique<Vector<RenderLayer*>>();
        layerList->append(this);
        accumulatedDirtyFlags.add(m_compositingDirtyBits);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context/container.
    if ((includeHiddenLayers || m_hasVisibleDescendant) && !isStacking) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!isReflectionLayer(*child))
                child->collectLayers(includeHiddenLayers, positiveZOrderList, negativeZOrderList, accumulatedDirtyFlags);
        }
    }
}

void RenderLayer::setAncestorsHaveCompositingDirtyFlag(Compositing flag)
{
    for (auto* layer = paintOrderParent(); layer; layer = layer->paintOrderParent()) {
        if (layer->m_compositingDirtyBits.contains(flag))
            break;
        layer->m_compositingDirtyBits.add(flag);
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

String RenderLayer::name() const
{
    if (!isReflection())
        return renderer().debugDescription();
    return makeString(renderer().debugDescription(), " (reflection)");
}

RenderLayerCompositor& RenderLayer::compositor() const
{
    return renderer().view().compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    if (changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged || changeType == ModelChanged || (isComposited() && changeType == ImageChanged)) {
        setNeedsPostLayoutCompositingUpdate();
        setNeedsCompositingConfigurationUpdate();
    }

    if (auto* backing = this->backing())
        backing->contentChanged(changeType);
}

bool RenderLayer::canRender3DTransforms() const
{
    return compositor().canRender3DTransforms();
}

bool RenderLayer::paintsWithFilters() const
{
    if (!renderer().hasFilter())
        return false;

    if (RenderLayerFilters::isIdentity(renderer()))
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

OptionSet<RenderLayer::UpdateLayerPositionsFlag> RenderLayer::flagsForUpdateLayerPositions(RenderLayer& startingLayer)
{
    OptionSet<UpdateLayerPositionsFlag> flags = { CheckForRepaint };

    if (auto* parent = startingLayer.parent()) {
        if (parent->hasFixedContainingBlockAncestor() || (!parent->isRenderViewLayer() && parent->renderer().canContainFixedPositionObjects()))
            flags.add(SeenFixedContainingBlockLayer);

        if (parent->hasTransformedAncestor() || parent->transform())
            flags.add(SeenTransformedLayer);

        if (parent->has3DTransformedAncestor() || (parent->transform() && !parent->transform()->isAffine()))
            flags.add(Seen3DTransformedLayer);

        if (parent->behavesAsFixed() || (parent->renderer().isFixedPositioned() && !parent->hasFixedContainingBlockAncestor()))
            flags.add(SeenFixedLayer);

        if (parent->hasCompositedScrollingAncestor() || parent->hasCompositedScrollableOverflow())
            flags.add(SeenCompositedScrollingLayer);
    }

    return flags;
}

void RenderLayer::willUpdateLayerPositions()
{
    renderer().document().markers().invalidateRectsForAllMarkers();
}

void RenderLayer::updateLayerPositionsAfterStyleChange()
{
    willUpdateLayerPositions();
    recursiveUpdateLayerPositions(nullptr, flagsForUpdateLayerPositions(*this));
}

void RenderLayer::updateLayerPositionsAfterLayout(bool isRelayoutingSubtree, bool didFullRepaint)
{
    auto updateLayerPositionFlags = [&](bool isRelayoutingSubtree, bool didFullRepaint) {
        auto flags = flagsForUpdateLayerPositions(*this);
        if (didFullRepaint) {
            flags.remove(RenderLayer::CheckForRepaint);
            flags.add(RenderLayer::NeedsFullRepaintInBacking);
        }
        if (isRelayoutingSubtree && enclosingPaginationLayer(RenderLayer::IncludeCompositedPaginatedLayers))
            flags.add(RenderLayer::UpdatePagination);
        return flags;
    };

    LOG(Compositing, "RenderLayer %p updateLayerPositionsAfterLayout", this);
    willUpdateLayerPositions();

    RenderGeometryMap geometryMap(UseTransforms);
    if (!isRenderViewLayer())
        geometryMap.pushMappingsToAncestor(parent(), nullptr);

    recursiveUpdateLayerPositions(&geometryMap, updateLayerPositionFlags(isRelayoutingSubtree, didFullRepaint));
}

void RenderLayer::recursiveUpdateLayerPositions(RenderGeometryMap* geometryMap, OptionSet<UpdateLayerPositionsFlag> flags)
{
    updateLayerPosition(&flags);
    if (m_scrollableArea)
        m_scrollableArea->applyPostLayoutScrollPositionIfNeeded();

    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    // Clear our cached clip rect information.
    clearClipRects();

    if (m_scrollableArea && m_scrollableArea->hasOverflowControls()) {
        LayoutSize offsetFromRoot;
        if (geometryMap)
            offsetFromRoot = LayoutSize(toFloatSize(geometryMap->absolutePoint(FloatPoint())));
        else {
            // FIXME: It looks suspicious to call convertToLayerCoords here
            // as canUseOffsetFromAncestor may be true for an ancestor layer.
            offsetFromRoot = offsetFromAncestor(root());
        }
        m_scrollableArea->positionOverflowControls(roundedIntSize(offsetFromRoot));
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

        auto* repaintContainer = renderer().containerForRepaint().renderer;
        
        auto oldRects = repaintRects();
        computeRepaintRects(repaintContainer, geometryMap);
        
        auto newRects = repaintRects();

        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if ((flags & CheckForRepaint) && newRects) {
            if (!renderer().view().printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    if (oldRects)
                        renderer().repaintUsingContainer(repaintContainer, oldRects->clippedOverflowRect);

                    if (!oldRects || newRects->clippedOverflowRect != oldRects->clippedOverflowRect)
                        renderer().repaintUsingContainer(repaintContainer, newRects->clippedOverflowRect);

                } else if (shouldRepaintAfterLayout()) {
                    // FIXME: We will convert this to just take the old and new RepaintLayoutRects once
                    // we change other callers to use RepaintLayoutRects.
                    auto resolvedOldRects = valueOrDefault(oldRects);
                    renderer().repaintAfterLayoutIfNeeded(repaintContainer, resolvedOldRects.clippedOverflowRect, resolvedOldRects.outlineBoundsRect,
                        &newRects->clippedOverflowRect, &newRects->outlineBoundsRect);
                }
            }
        }
    } else
        clearRepaintRects();

    m_repaintStatus = NeedsNormalRepaint;
    m_hasFixedContainingBlockAncestor = flags.contains(SeenFixedContainingBlockLayer);
    m_hasTransformedAncestor = flags.contains(SeenTransformedLayer);
    m_has3DTransformedAncestor = flags.contains(Seen3DTransformedLayer);
    m_behavesAsFixed = flags.contains(SeenFixedLayer);
    setHasCompositedScrollingAncestor(flags.contains(SeenCompositedScrollingLayer));

    // Update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

    if (renderer().isRenderFragmentedFlow()) {
        updatePagination();
        flags.add(UpdatePagination);
    }

    if (!isRenderViewLayer() && renderer().canContainFixedPositionObjects()) {
        flags.add(SeenFixedContainingBlockLayer);

        if (transform()) {
            flags.add(SeenTransformedLayer);
            if (!transform()->isAffine())
                flags.add(Seen3DTransformedLayer);
        }
    }

    // Fixed inside transform behaves like absolute (per spec).
    if (renderer().isFixedPositioned() && !m_hasFixedContainingBlockAncestor) {
        m_behavesAsFixed = true;
        flags.add(SeenFixedLayer);
    }

    if (hasCompositedScrollableOverflow())
        flags.add(SeenCompositedScrollingLayer);

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->recursiveUpdateLayerPositions(geometryMap, flags);

    if (m_scrollableArea)
        m_scrollableArea->updateMarqueePosition();

    if (renderer().isFixedPositioned() && renderer().settings().acceleratedCompositingForFixedPositionEnabled()) {
        bool intersectsViewport = compositor().fixedLayerIntersectsViewport(*this);
        if (intersectsViewport != m_isFixedIntersectingViewport) {
            m_isFixedIntersectingViewport = intersectsViewport;
            setNeedsPostLayoutCompositingUpdate();
        }
    }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (renderer().isSVGLayerAwareRenderer() && renderer().document().settings().layerBasedSVGEngineEnabled()) {
        if (!is<RenderSVGRoot>(renderer()))
            ASSERT(!renderer().isFixedPositioned());

        // Only the outermost <svg> and / <foreignObject> are potentially scrollable.
        if (is<RenderSVGModelObject>(renderer()) || is<RenderSVGText>(renderer()) || is<RenderSVGInline>(renderer()))
            ASSERT(!m_scrollableArea);
    }
#endif

    if (isComposited())
        backing()->updateAfterLayout(flags.contains(ContainingClippingLayerChangedSize), flags.contains(NeedsFullRepaintInBacking));

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
}

LayoutRect RenderLayer::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect;
    if (m_repaintRectsValid)
        repaintRect = m_repaintRects.clippedOverflowRect;
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
        if (renderer().shouldApplyPaintContainment()) {
            m_hasSelfPaintingLayerDescendant = true;
            m_hasSelfPaintingLayerDescendantDirty = false;
            break;
        }
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

    if (!isSelfPaintingLayer())
        clearRepaintRects();
    else {
        setRepaintRects({
            renderer().clippedOverflowRectForRepaint(repaintContainer),
            renderer().outlineBoundsForRepaint(repaintContainer, geometryMap)
        });
    }
}

void RenderLayer::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    computeRepaintRects(renderer().containerForRepaint().renderer);

    for (RenderLayer* layer = firstChild(); layer; layer = layer->nextSibling())
        layer->computeRepaintRectsIncludingDescendants();
}

void RenderLayer::setRepaintRects(const LayerRepaintRects& rects)
{
    m_repaintRects = rects;
    m_repaintRectsValid = true;
}

void RenderLayer::clearRepaintRects()
{
    m_repaintRectsValid = false;
}

void RenderLayer::updateLayerPositionsAfterOverflowScroll()
{
    RenderGeometryMap geometryMap(UseTransforms);
    if (!isRenderViewLayer())
        geometryMap.pushMappingsToAncestor(parent(), nullptr);

    willUpdateLayerPositions();

    // FIXME: why is it OK to not check the ancestors of this layer in order to
    // initialize the HasSeenViewportConstrainedAncestor and HasSeenAncestorWithOverflowClip flags?
    recursiveUpdateLayerPositionsAfterScroll(&geometryMap, RenderLayer::IsOverflowScroll);
}

void RenderLayer::updateLayerPositionsAfterDocumentScroll()
{
    ASSERT(isRenderViewLayer());
    LOG(Scrolling, "RenderLayer::updateLayerPositionsAfterDocumentScroll");

    willUpdateLayerPositions();

    RenderGeometryMap geometryMap(UseTransforms);
    recursiveUpdateLayerPositionsAfterScroll(&geometryMap);
}

void RenderLayer::recursiveUpdateLayerPositionsAfterScroll(RenderGeometryMap* geometryMap, OptionSet<UpdateLayerPositionsAfterScrollFlag> flags)
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

    if (renderer().hasNonVisibleOverflow())
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
            computeRepaintRects(renderer().containerForRepaint().renderer, geometryMap);
    } else if (!renderer().view().frameView().platformWidget()) {
        // When ScrollView's m_paintsEntireContents flag flips due to layer backing changes, the repaint area transitions from
        // visual to layout overflow. When this happens the cached repaint rects become invalid and they need to be recomputed (see webkit.org/b/188121).
        // Check that our cached rects are correct.
        ASSERT_IMPLIES(m_repaintRectsValid, m_repaintRects.clippedOverflowRect == renderer().clippedOverflowRectForRepaint(renderer().containerForRepaint().renderer));
        ASSERT_IMPLIES(m_repaintRectsValid, m_repaintRects.outlineBoundsRect == renderer().outlineBoundsForRepaint(renderer().containerForRepaint().renderer));
    }
    
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->recursiveUpdateLayerPositionsAfterScroll(geometryMap, flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (m_scrollableArea)
        m_scrollableArea->updateMarqueePosition();

    if (shouldPushAndPopMappings)
        geometryMap->popMappingsToAncestor(parent());
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

void RenderLayer::willRemoveChildWithBlendMode()
{
    parent()->dirtyAncestorChainHasBlendingDescendants();
}

void RenderLayer::updateAncestorChainHasBlendingDescendants()
{
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (!layer->hasNotIsolatedBlendingDescendantsStatusDirty() && layer->hasNotIsolatedBlendingDescendants())
            break;
        layer->m_hasNotIsolatedBlendingDescendants = true;
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = false;

        layer->updateSelfPaintingLayer();

        if (layer->isCSSStackingContext())
            break;
    }
}

void RenderLayer::dirtyAncestorChainHasBlendingDescendants()
{
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (layer->hasNotIsolatedBlendingDescendantsStatusDirty())
            break;
        
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = true;

        if (layer->isCSSStackingContext())
            break;
    }
}
#endif

FloatRect RenderLayer::referenceBoxRectForClipPath(CSSBoxType boxType, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds) const
{
    // FIXME: [LBSE] Upstream clipping support for SVG.

    // FIXME: Support different reference boxes for inline content.
    // https://bugs.webkit.org/show_bug.cgi?id=129047
    if (!renderer().isBox())
        return rootRelativeBounds;

    auto referenceBoxRect = renderer().referenceBoxRect(boxType);
    referenceBoxRect.move(offsetFromRoot);
    return referenceBoxRect;
}

void RenderLayer::updateTransformFromStyle(TransformationMatrix& transform, const RenderStyle& style, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    auto referenceBoxRect = snapRectToDevicePixelsIfNeeded(renderer().transformReferenceBoxRect(style), renderer());
    renderer().applyTransform(transform, style, referenceBoxRect, options);
    makeMatrixRenderable(transform, canRender3DTransforms());
}

void RenderLayer::setReferenceBoxForPathOperations()
{
    auto pathOperation = renderer().style().offsetPath();
    if (!pathOperation)
        return;
    if (is<BoxPathOperation>(pathOperation)) {
        auto& boxPathOperation = downcast<BoxPathOperation>(*pathOperation);
        auto pathReferenceBoxRect = snapRectToDevicePixelsIfNeeded(renderer().referenceBoxRect(boxPathOperation.referenceBox()), renderer());
        boxPathOperation.setPathForReferenceRect(FloatRoundedRect { pathReferenceBoxRect });
    } else if (is<RayPathOperation>(pathOperation)) {
        if (const auto* containingBlock = renderer().containingBlock()) {
            auto& rayPathOperation = downcast<RayPathOperation>(*pathOperation);
            auto pathReferenceBoxRect = snapRectToDevicePixelsIfNeeded(containingBlock->transformReferenceBoxRect(containingBlock->style()), renderer());
            if (!pathReferenceBoxRect.width())
                pathReferenceBoxRect.setWidth(pathReferenceBoxRect.height());
            if (!pathReferenceBoxRect.height())
                pathReferenceBoxRect.setHeight(pathReferenceBoxRect.width());
            rayPathOperation.setContainingBlockReferenceRect(pathReferenceBoxRect);
            auto left = renderer().style().left();
            auto top = renderer().style().top();
            rayPathOperation.setStartingPosition(FloatPoint(left.isPercent() ? left.value() / 100 * pathReferenceBoxRect.width() : left.value(), top.isPercent() ? top.value() / 100 * pathReferenceBoxRect.height() : top.value()));
        }
    }
}

void RenderLayer::updateTransform()
{
    bool hasTransform = renderer().hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = !!m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = makeUnique<TransformationMatrix>();
        else
            m_transform = nullptr;
        
        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        clearClipRectsIncludingDescendants();
    }
    
    if (hasTransform) {
        m_transform->makeIdentity();
        setReferenceBoxForPathOperations();
        updateTransformFromStyle(*m_transform, renderer().style(), RenderStyle::allTransformOperations);
    }

    if (had3DTransform != has3DTransform()) {
        dirty3DTransformedDescendantStatus();
        // Having a 3D transform affects whether enclosing perspective and preserve-3d layers composite, so trigger an update.
        setNeedsPostLayoutCompositingUpdateOnAncestors();
    }
}

TransformationMatrix RenderLayer::currentTransform(OptionSet<RenderStyle::TransformOperationOption> options) const
{
    if (!m_transform)
        return { };

    // m_transform includes transform-origin and is affected by the choice of the transform-box.
    // Therefore we can only use the cached m_transform, if the animation doesn't alter transform-box or excludes transform-origin.

    // Query the animatedStyle() to obtain the current transformation, when accelerated transform animations are running.
    auto styleable = Styleable::fromRenderer(renderer());
    if ((styleable && styleable->isRunningAcceleratedTransformAnimation()) || !options.contains(RenderStyle::TransformOperationOption::TransformOrigin)) {
        std::unique_ptr<RenderStyle> animatedStyle = renderer().animatedStyle();

        TransformationMatrix transform;
        updateTransformFromStyle(transform, *animatedStyle, options);
        return transform;
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
        if (layer->renderer().hasPotentiallyScrollableOverflow())
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
    if (renderer().isRenderFragmentedFlow()) {
        m_enclosingPaginationLayer = *this;
        return;
    }

    if (isNormalFlowOnly()) {
        // Content inside a transform is not considered to be paginated, since we simply
        // paint the transform multiple times in each column, so we don't have to use
        // fragments for the transformed content.
        if (parent()->hasTransform())
            m_enclosingPaginationLayer = nullptr;
        else
            m_enclosingPaginationLayer = parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
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
                m_enclosingPaginationLayer = containingBlock->layer()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
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
    computeRepaintRects(renderer().containerForRepaint().renderer);
    if (!isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking containers ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (auto* sc = stackingContext(); sc; sc = sc->stackingContext()) {
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
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (layer->m_visibleDescendantStatusDirty)
            break;

        layer->m_visibleDescendantStatusDirty = true;
    }
}

void RenderLayer::setAncestorChainHasVisibleDescendant()
{
    for (auto* layer = this; layer; layer = layer->parent()) {
        if (renderer().shouldApplyPaintContainment()) {
            m_hasVisibleDescendant = true;
            m_visibleDescendantStatusDirty = false;
            break;
        }
        if (!layer->m_visibleDescendantStatusDirty && layer->hasVisibleDescendant())
            break;

        layer->m_hasVisibleDescendant = true;
        layer->m_visibleDescendantStatusDirty = false;
    }
}

void RenderLayer::updateAncestorDependentState()
{
    bool insideSVGForeignObject = renderer().document().mayHaveRenderedSVGForeignObjects() && ancestorsOfType<LegacyRenderSVGForeignObject>(renderer()).first();
    if (insideSVGForeignObject == m_insideSVGForeignObject)
        return;

    m_insideSVGForeignObject = insideSVGForeignObject;
    updateSelfPaintingLayer();
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
        m_hasVisibleContent = computeHasVisibleContent();
        m_visibleContentStatusDirty = false;
    }
}

bool RenderLayer::computeHasVisibleContent() const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // FIXME: [LBSE] Eventually cache if we're part of a RenderSVGHiddenContainer subtree to avoid tree walks.
    if (renderer().document().settings().layerBasedSVGEngineEnabled() && lineageOfType<RenderSVGHiddenContainer>(renderer()).first())
        return false;
#endif
    if (renderer().style().visibility() == Visibility::Visible)
        return true;

    // Layer's renderer has visibility:hidden, but some non-layer child may have visibility:visible.
    RenderObject* r = renderer().firstChild();
    while (r) {
        if (r->style().visibility() == Visibility::Visible && !r->hasLayer())
            return true;

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

    return false;
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
        for (auto* layer : positiveZOrderLayers())
            m_has3DTransformedDescendant |= layer->update3DTransformedDescendantStatus();

        // Now check our negative z-index children.
        for (auto* layer : negativeZOrderLayers())
            m_has3DTransformedDescendant |= layer->update3DTransformedDescendantStatus();
        
        m_3DTransformedDescendantStatusDirty = false;
    }
    
    // If we live in a 3d hierarchy, then the layer at the root of that hierarchy needs
    // the m_has3DTransformedDescendant set.
    if (preserves3D())
        return has3DTransform() || m_has3DTransformedDescendant;

    return has3DTransform();
}

bool RenderLayer::updateLayerPosition(OptionSet<UpdateLayerPositionsFlag>* flags)
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
        auto newSize = snappedIntRect(box->frameRect()).size();
        if (newSize != size()) {
            if (is<RenderWidget>(*box) && downcast<RenderWidget>(*box).requiresAcceleratedCompositing()) {
                // Trigger RenderLayerCompositor::requiresCompositingForFrame() which depends on the contentBoxRect size.
                setNeedsPostLayoutCompositingUpdate();
            }

            if (flags && renderer().hasNonVisibleOverflow())
                flags->add(ContainingClippingLayerChangedSize);

            setSize(newSize);
        }
        
        box->applyTopLeftLocationOffset(localPoint);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    } else if (is<RenderSVGModelObject>(renderer())) {
        auto& svgRenderer = downcast<RenderSVGModelObject>(renderer());
        auto newSize = enclosingIntRect(svgRenderer.frameRectEquivalent()).size();
        if (newSize != size()) {
            if (flags && renderer().hasNonVisibleOverflow())
                flags->add(ContainingClippingLayerChangedSize);

            setSize(newSize);
        }

        svgRenderer.applyTopLeftLocationOffsetEquivalent(localPoint);
#endif
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
        if (positionedParent->renderer().hasNonVisibleOverflow()) {
            if (auto* positionedParentScrollableArea = positionedParent->scrollableArea())
                localPoint -= toLayoutSize(positionedParentScrollableArea->scrollPosition());
        }
        
        if (positionedParent->renderer().isInFlowPositioned() && is<RenderInline>(positionedParent->renderer())) {
            LayoutSize offset = downcast<RenderInline>(positionedParent->renderer()).offsetForInFlowPositionedInline(&downcast<RenderBox>(renderer()));
            localPoint += offset;
        }

        ASSERT(positionedParent->contentsScrollingScope());
        m_boxScrollingScope = positionedParent->contentsScrollingScope();
    } else if (auto* parentLayer = parent()) {
        if (parentLayer->renderer().hasNonVisibleOverflow()) {
            if (auto* parentLayerScrollableArea = parentLayer->scrollableArea())
                localPoint -= toLayoutSize(parentLayerScrollableArea->scrollPosition());
        }

        ASSERT(parentLayer->contentsScrollingScope());
        m_boxScrollingScope = parentLayer->contentsScrollingScope();
    }

    if (hasCompositedScrollableOverflow()) {
        if (!m_contentsScrollingScope || m_contentsScrollingScope == m_boxScrollingScope)
            m_contentsScrollingScope = nextScrollingScope();
    } else if (!m_contentsScrollingScope || m_contentsScrollingScope != m_boxScrollingScope)
        m_contentsScrollingScope = m_boxScrollingScope;

    bool positionOrOffsetChanged = false;
    if (renderer().isInFlowPositioned()) {
        LayoutSize newOffset = downcast<RenderBoxModelObject>(renderer()).offsetForInFlowPosition();
        positionOrOffsetChanged = newOffset != m_offsetForPosition;
        m_offsetForPosition = newOffset;
        localPoint.move(m_offsetForPosition);
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint -= inlineBoundingBoxOffset;
    
    positionOrOffsetChanged |= location() != localPoint;
    setLocation(localPoint);
    
    if (positionOrOffsetChanged && compositor().hasContentCompositingLayers()) {
        if (isComposited())
            setNeedsCompositingGeometryUpdate();
        // This layer's position can affect the location of a composited descendant (which may be a sibling in z-order),
        // so trigger a descendant walk from the paint-order parent.
        if (auto* paintParent = paintOrderParent())
            paintParent->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
    }

    return positionOrOffsetChanged;
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    if (!renderer().hasTransformRelatedProperty())
        return { };

    const auto& style = renderer().style();
    if (!style.hasPerspective())
        return { };

    auto transformReferenceBoxRect = snapRectToDevicePixelsIfNeeded(renderer().transformReferenceBoxRect(style), renderer());
    auto perspectiveOrigin = style.computePerspectiveOrigin(transformReferenceBoxRect);

    // In the regular case of a non-clipped, non-scrolled GraphicsLayer, all transformations
    // (via CSS 'transform' / 'perspective') are applied with respect to a predefined anchor point,
    // which depends on the chosen CSS 'transform-box' / 'transform-origin' properties.
    //
    // A transformation given by the CSS 'transform' property is applied, by translating
    // to the 'transform origin', applying the transformation, and translating back.
    // When an element specifies a CSS 'perspective' property, the perspective transformation matrix
    // that's computed here is propagated to the GraphicsLayer by calling setChildrenTransform().
    //
    // However the GraphicsLayer platform implementations (e.g. CA on macOS) apply the children transform,
    // defined on the parent, with respect to the anchor point of the parent, when rendering child elements.
    // This is wrong, as the perspective transformation (applied to a child of the element defining the
    // 3d effect), must be independant of the chosen transform-origin (the parents transform origin
    // must not affect its children).
    //
    // To circumvent this, explicitely remove the transform-origin dependency in the perspective matrix.
    auto transformOrigin = transformOriginPixelSnappedIfNeeded();

    TransformationMatrix transform;
    style.unapplyTransformOrigin(transform, transformOrigin);
    style.applyPerspective(transform, perspectiveOrigin);
    style.applyTransformOrigin(transform, transformOrigin);
    return transform;
}

FloatPoint3D RenderLayer::transformOriginPixelSnappedIfNeeded() const
{
    if (!renderer().hasTransformRelatedProperty())
        return { };

    const auto& style = renderer().style();
    auto referenceBoxRect = renderer().transformReferenceBoxRect(style);

    auto origin = style.computeTransformOrigin(referenceBoxRect);
    if (rendererNeedsPixelSnapping(renderer()))
        origin.setXY(roundPointToDevicePixels(LayoutPoint(origin.xy()), renderer().document().deviceScaleFactor()));
    return origin;
}

FloatPoint RenderLayer::perspectiveOrigin() const
{
    if (!renderer().hasTransformRelatedProperty())
        return { };
    return floatPointForLengthPoint(renderer().style().perspectiveOrigin(), renderer().transformReferenceBoxRect(renderer().style()).size());
}

static inline bool isContainerForPositioned(RenderLayer& layer, PositionType position, bool establishesTopLayer)
{
    if (establishesTopLayer)
        return layer.isRenderViewLayer();

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

bool RenderLayer::ancestorLayerIsInContainingBlockChain(const RenderLayer& ancestor, const RenderLayer* checkLimit) const
{
    if (&ancestor == this)
        return true;

    for (const auto* currentBlock = renderer().containingBlock(); currentBlock && !is<RenderView>(*currentBlock); currentBlock = currentBlock->containingBlock()) {
        auto* currLayer = currentBlock->layer();
        if (currLayer == &ancestor)
            return true;
        
        if (currLayer && currLayer == checkLimit)
            return false;
    }
    
    return false;
}

RenderLayer* RenderLayer::enclosingAncestorForPosition(PositionType position) const
{
    auto* curr = parent();
    while (curr && !isContainerForPositioned(*curr, position, establishesTopLayer()))
        curr = curr->parent();

    ASSERT_IMPLIES(establishesTopLayer(), !curr || curr == renderer().view().layer());
    return curr;
}

RenderLayer* RenderLayer::enclosingLayerInContainingBlockOrder() const
{
    for (const auto* currentBlock = renderer().containingBlock(); currentBlock; currentBlock = currentBlock->containingBlock()) {
        if (auto* layer = currentBlock->layer())
            return layer;
    }

    return nullptr;
}

RenderLayer* RenderLayer::enclosingFrameRenderLayer() const
{
    auto* ownerElement = renderer().document().ownerElement();
    if (!ownerElement)
        return nullptr;

    auto* ownerRenderer = ownerElement->renderer();
    if (!ownerRenderer)
        return nullptr;

    return ownerRenderer->enclosingLayer();
}

RenderLayer* RenderLayer::enclosingContainingBlockLayer(CrossFrameBoundaries crossFrameBoundaries) const
{
    if (auto* ancestor = enclosingLayerInContainingBlockOrder())
        return ancestor;

    if (crossFrameBoundaries == CrossFrameBoundaries::No)
        return nullptr;

    return enclosingFrameRenderLayer();
}

RenderLayer* RenderLayer::enclosingScrollableLayer(IncludeSelfOrNot includeSelf, CrossFrameBoundaries crossFrameBoundaries) const
{
    RenderTreeMutationDisallowedScope renderTreeMutationDisallowedScope;

    auto isConsideredScrollable = [](const RenderLayer& layer) {
        return is<RenderBox>(layer.renderer()) && downcast<RenderBox>(layer.renderer()).canBeScrolledAndHasScrollableArea();
    };

    if (includeSelf == IncludeSelfOrNot::IncludeSelf && isConsideredScrollable(*this))
        return const_cast<RenderLayer*>(this);
    
    for (auto* nextLayer = enclosingContainingBlockLayer(crossFrameBoundaries); nextLayer; nextLayer = nextLayer->enclosingContainingBlockLayer(crossFrameBoundaries)) {
        if (isConsideredScrollable(*nextLayer))
            return nextLayer;
    }

    return nullptr;
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isRenderViewLayer() && !curr->transform())
        curr = curr->parent();

    return curr;
}

inline bool RenderLayer::shouldRepaintAfterLayout() const
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // The SVG containers themselves never trigger repaints, only their contents are allowed to.
    // SVG container sizes/positions are only ever determined by their children, so they will
    // change as a reaction on a re-position/re-sizing of the children - which already properly
    // trigger repaints.
    if (is<RenderSVGContainer>(renderer()) && !paintsWithFilters())
        return false;
#endif

    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !isComposited() || backing()->paintsIntoCompositedAncestor();
}

void RenderLayer::setBackingProviderLayer(RenderLayer* backingProvider)
{
    if (backingProvider == m_backingProviderLayer)
        return;

    if (!renderer().renderTreeBeingDestroyed())
        clearClipRectsIncludingDescendants();

    m_backingProviderLayer = backingProvider;
}

void RenderLayer::disconnectFromBackingProviderLayer()
{
    if (!m_backingProviderLayer)
        return;
    
    ASSERT(m_backingProviderLayer->isComposited());
    if (m_backingProviderLayer->isComposited())
        m_backingProviderLayer->backing()->removeBackingSharingLayer(*this);
}

bool compositedWithOwnBackingStore(const RenderLayer& layer)
{
    return layer.isComposited() && !layer.backing()->paintsIntoCompositedAncestor();
}

RenderLayer* RenderLayer::enclosingCompositingLayer(IncludeSelfOrNot includeSelf) const
{
    if (includeSelf == IncludeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = paintOrderParent(); curr; curr = curr->paintOrderParent()) {
        if (curr->isComposited())
            return const_cast<RenderLayer*>(curr);
    }
         
    return nullptr;
}

RenderLayer::EnclosingCompositingLayerStatus RenderLayer::enclosingCompositingLayerForRepaint(IncludeSelfOrNot includeSelf) const
{
    auto repaintTargetForLayer = [](const RenderLayer& layer) -> RenderLayer* {
        if (compositedWithOwnBackingStore(layer))
            return const_cast<RenderLayer*>(&layer);
        
        if (layer.paintsIntoProvidedBacking())
            return layer.backingProviderLayer();
        
        return nullptr;
    };
    auto isEligibleForFullRepaintCheck = [&](const auto& layer) {
        return layer.isSelfPaintingLayer() && !layer.renderer().hasPotentiallyScrollableOverflow() && !is<RenderView>(layer.renderer());
    };

    auto fullRepaintAlreadyScheduled = isEligibleForFullRepaintCheck(*this) && needsFullRepaint();
    RenderLayer* repaintTarget = nullptr;
    if (includeSelf == IncludeSelf && (repaintTarget = repaintTargetForLayer(*this)))
        return { fullRepaintAlreadyScheduled, repaintTarget };

    for (const RenderLayer* curr = paintOrderParent(); curr; curr = curr->paintOrderParent()) {
        fullRepaintAlreadyScheduled = fullRepaintAlreadyScheduled || (isEligibleForFullRepaintCheck(*curr) && curr->needsFullRepaint());
        if ((repaintTarget = repaintTargetForLayer(*curr)))
            return { fullRepaintAlreadyScheduled, repaintTarget };
    }
         
    return { };
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

// FIXME: This needs a better name.
void RenderLayer::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect)
{
    ASSERT(requiresFullLayerImageForFilters());
    ASSERT(m_filters);

    if (rect.isEmpty())
        return;
    
    LayoutRect rectForRepaint = rect;
    rectForRepaint.expand(toLayoutBoxExtent(filterOutsets()));

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
        if (curr->hasFilterOutsets())
            return true;
    }
    return false;
}

RenderLayer* RenderLayer::clippingRootForPainting() const
{
    if (isComposited())
        return const_cast<RenderLayer*>(this);

    if (paintsIntoProvidedBacking())
        return backingProviderLayer();

    const RenderLayer* current = this;
    while (current) {
        if (current->isRenderViewLayer())
            return const_cast<RenderLayer*>(current);

        current = current->paintOrderParent();
        ASSERT(current);
        if (current->transform() || compositedWithOwnBackingStore(*current))
            return const_cast<RenderLayer*>(current);

        if (current->paintsIntoProvidedBacking())
            return current->backingProviderLayer();
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

    for (RenderLayer* curr = stackingContext(); curr; curr = curr->stackingContext()) {
        if (curr->isComposited())
            break;
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
            if (!layer.isReflectionLayer(*curr))
                clipRect.unite(transparencyClipBox(*curr, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, paintBehavior));
        }
    }

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer.renderer().isBox() && layer.renderer().hasReflection()) {
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
        clipRect.expand(toLayoutBoxExtent(layer.filterOutsets()));
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
    clipRect.expand(toLayoutBoxExtent(layer.filterOutsets()));

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
        ASSERT(isCSSStackingContext());
        m_usedTransparency = true;
        context.save();
        LayoutRect adjustedClipRect = paintingExtent(*this, paintingInfo.rootLayer, dirtyRect, paintingInfo.paintBehavior);
        adjustedClipRect.move(paintingInfo.subpixelOffset);
        FloatRect pixelSnappedClipRect = snapRectToDevicePixels(adjustedClipRect, renderer().document().deviceScaleFactor());
        context.clip(pixelSnappedClipRect);

#if ENABLE(CSS_COMPOSITING)
        bool usesCompositeOperation = hasBlendMode() && !(renderer().isLegacySVGRoot() && parent() && parent()->isRenderViewLayer());
        if (usesCompositeOperation)
            context.setCompositeOperation(context.compositeOperation(), blendMode());
#endif

        context.beginTransparencyLayer(renderer().opacity());

#if ENABLE(CSS_COMPOSITING)
        if (usesCompositeOperation)
            context.setCompositeOperation(context.compositeOperation(), BlendMode::Normal);
#endif

#ifdef REVEAL_TRANSPARENCY_LAYERS
        context.setFillColor(SRGBA<uint8_t> { 0, 0, 128, 51 });
        context.fillRect(pixelSnappedClipRect);
#endif
    }
}

#if PLATFORM(IOS_FAMILY)
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

static RenderLayer* findCommonAncestor(const RenderLayer& firstLayer, const RenderLayer& secondLayer)
{
    if (&firstLayer == &secondLayer)
        return const_cast<RenderLayer*>(&firstLayer);

    HashSet<const RenderLayer*> ancestorChain;
    for (auto* currLayer = &firstLayer; currLayer; currLayer = currLayer->parent())
        ancestorChain.add(currLayer);

    for (auto* currLayer = &secondLayer; currLayer; currLayer = currLayer->parent()) {
        if (ancestorChain.contains(currLayer))
            return const_cast<RenderLayer*>(currLayer);
    }
    return nullptr;
}

RenderLayer* RenderLayer::commonAncestorWithLayer(const RenderLayer& layer) const
{
    return findCommonAncestor(*this, layer);
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

    const auto& renderer = layer->renderer();
    auto position = renderer.style().position();

    // FIXME: Special casing RenderFragmentedFlow so much for fixed positioning here is not great.
    auto* fixedFragmentedFlowContainer = position == PositionType::Fixed ? renderer.enclosingFragmentedFlow() : nullptr;
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
        location.moveBy(LayoutPoint(renderer.localToAbsolute({ }, IsFixed)));
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
        for (auto* currLayer = layer->parent(); currLayer; currLayer = currLayer->parent()) {
            if (currLayer == ancestorLayer)
                foundAncestor = true;

            if (isContainerForPositioned(*currLayer, PositionType::Fixed, layer->establishesTopLayer())) {
                fixedPositionContainerLayer = currLayer;
                ASSERT_UNUSED(foundAncestor, foundAncestor);
                break;
            }
        }
        
        ASSERT(fixedPositionContainerLayer); // We should have hit the RenderView's layer at least.

        if (fixedPositionContainerLayer != ancestorLayer) {
            auto fixedContainerCoords = layer->offsetFromAncestor(fixedPositionContainerLayer);
            auto ancestorCoords = ancestorLayer->offsetFromAncestor(fixedPositionContainerLayer);
            location.move(fixedContainerCoords - ancestorCoords);
            return ancestorLayer;
        }
    }

    if (position == PositionType::Fixed && fixedFragmentedFlowContainer) {
        ASSERT(ancestorLayer);
        if (ancestorLayer == renderer.view().layer()) {
            // Add location in flow thread coordinates.
            location.moveBy(layer->location());

            // Add flow thread offset in view coordinates since the view may be scrolled.
            location.moveBy(LayoutPoint(renderer.view().localToAbsolute({ }, IsFixed)));
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
            if (isContainerForPositioned(*parentLayer, position, layer->establishesTopLayer()))
                break;

            if (parentLayer == ancestorLayer) {
                foundAncestorFirst = true;
                break;
            }

            parentLayer = parentLayer->parent();
        }

        // We should not reach RenderView layer past the RenderFragmentedFlow layer for any
        // children of the RenderFragmentedFlow.
        if (renderer.enclosingFragmentedFlow())
            ASSERT(parentLayer != renderer.view().layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingAncestorForPosition and subtract.
            auto* positionedAncestor = parentLayer->enclosingAncestorForPosition(position);
            auto thisCoords = layer->offsetFromAncestor(positionedAncestor);
            auto ancestorCoords = ancestorLayer->offsetFromAncestor(positionedAncestor);
            location.move(thisCoords - ancestorCoords);
            return ancestorLayer;
        }
    } else
        parentLayer = layer->parent();
    
    if (!parentLayer)
        return nullptr;

    location.moveBy(layer->location());

    if (adjustForColumns == RenderLayer::AdjustForColumns) {
        auto* parentLayer = layer->parent();
        if (parentLayer && parentLayer != ancestorLayer) {
            if (is<RenderMultiColumnFlow>(parentLayer->renderer())) {
                if (auto* fragment = downcast<RenderMultiColumnFlow>(parentLayer->renderer()).physicalTranslationFromFlowToFragment(location))
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

    const auto* currLayer = this;
    auto locationInLayerCoords = location;
    while (currLayer && currLayer != ancestorLayer)
        currLayer = accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, locationInLayerCoords, adjustForColumns);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // Pixel snap the whole SVG subtree as one "block" -- not individual layers down the SVG render tree.
    if (renderer().isSVGRoot())
        return LayoutPoint(roundPointToDevicePixels(locationInLayerCoords, renderer().document().deviceScaleFactor()));
#endif

    return locationInLayerCoords;
}

LayoutSize RenderLayer::offsetFromAncestor(const RenderLayer* ancestorLayer, ColumnOffsetAdjustment adjustForColumns) const
{
    return toLayoutSize(convertToLayerCoords(ancestorLayer, LayoutPoint(), adjustForColumns));
}

bool RenderLayer::shouldTryToScrollForScrollIntoView() const
{
    if (!renderer().isBox() || !renderer().hasNonVisibleOverflow())
        return false;

    // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
    // FIXME: Is this still needed? It used to be relevant for Safari RSS.
    if (renderer().parent() && !renderer().parent()->style().lineClamp().isNone())
        return false;

    auto& box = *renderBox();

    if (box.frame().eventHandler().autoscrollInProgress()) {
        // The "programmatically" here is misleading; this asks whether the box has scrollable overflow,
        // or is a special case like a form control.
        return box.canBeProgramaticallyScrolled();
    }

    // Programmatic scrolls can scroll overflow: hidden but not overflow: clip.
    return box.hasPotentiallyScrollableOverflow() && (box.hasHorizontalOverflow() || box.hasVerticalOverflow());
}

void RenderLayer::autoscroll(const IntPoint& positionInWindow)
{
    IntPoint currentDocumentPosition = renderer().view().frameView().windowToContents(positionInWindow);
    FrameView::scrollRectToVisible(LayoutRect(currentDocumentPosition, LayoutSize(1, 1)), renderer(), false, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
}

bool RenderLayer::canResize() const
{
    // We need a special case for <iframe> because they never have
    // hasNonVisibleOverflow(). However, they do "implicitly" clip their contents, so
    // we want to allow resizing them also.
    return (renderer().hasNonVisibleOverflow() || renderer().isRenderIFrame()) && renderer().style().resize() != Resize::None;
}

LayoutSize RenderLayer::minimumSizeForResizing(float zoomFactor) const
{
    // Use the resizer size as the strict minimum size
    auto resizerRect = overflowControlsRects().resizer;
    LayoutUnit minWidth = minimumValueForLength(renderer().style().minWidth(), renderer().containingBlock()->width());
    LayoutUnit minHeight = minimumValueForLength(renderer().style().minHeight(), renderer().containingBlock()->height());
    minWidth = std::max(LayoutUnit(minWidth / zoomFactor), LayoutUnit(resizerRect.width()));
    minHeight = std::max(LayoutUnit(minHeight / zoomFactor), LayoutUnit(resizerRect.height()));
    return LayoutSize(minWidth, minHeight);
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

    auto absolutePoint = document.view()->windowToContents(evt.position());
    auto localPoint = roundedIntPoint(absoluteToContents(absolutePoint));

    LayoutSize newOffset = offsetFromResizeCorner(localPoint);
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);

    LayoutSize currentSize = LayoutSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);

    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (renderer->shouldPlaceVerticalScrollbarOnLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }

    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSizeForResizing(zoomFactor)) - currentSize;

    StyledElement* styledElement = downcast<StyledElement>(element);
    bool isBoxSizingBorder = renderer->style().boxSizing() == BoxSizing::BorderBox;

    Resize resize = renderer->style().resize();
    bool canResizeWidth = resize == Resize::Horizontal || resize == Resize::Both
        || (renderer->isHorizontalWritingMode() ? resize == Resize::Inline : resize == Resize::Block);
    if (canResizeWidth && difference.width()) {
        if (is<HTMLFormControlElement>(*element)) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginLeft, renderer->marginLeft() / zoomFactor, CSSUnitType::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginRight, renderer->marginRight() / zoomFactor, CSSUnitType::CSS_PX);
        }
        LayoutUnit baseWidth = renderer->width() - (isBoxSizingBorder ? 0_lu : renderer->horizontalBorderAndPaddingExtent());
        baseWidth = baseWidth / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSUnitType::CSS_PX);
    }

    bool canResizeHeight = resize == Resize::Vertical || resize == Resize::Both
        || (renderer->isHorizontalWritingMode() ? resize == Resize::Block : resize == Resize::Inline);
    if (canResizeHeight && difference.height()) {
        if (is<HTMLFormControlElement>(*element)) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginTop, renderer->marginTop() / zoomFactor, CSSUnitType::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginBottom, renderer->marginBottom() / zoomFactor, CSSUnitType::CSS_PX);
        }
        LayoutUnit baseHeight = renderer->height() - (isBoxSizingBorder ? 0_lu : renderer->verticalBorderAndPaddingExtent());
        baseHeight = baseHeight / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSUnitType::CSS_PX);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

IntSize RenderLayer::visibleSize() const
{
    RenderBox* box = renderBox();
    if (!box)
        return IntSize();

    return IntSize(roundToInt(box->clientWidth()), roundToInt(box->clientHeight()));
}

RenderLayer::OverflowControlRects RenderLayer::overflowControlsRects() const
{
    if (m_scrollableArea)
        return m_scrollableArea->overflowControlsRects();

    ASSERT(is<RenderBox>(renderer()));
    auto& renderBox = downcast<RenderBox>(renderer());
    // Scrollbars sit inside the border box.
    auto overflowControlsPositioningRect = snappedIntRect(renderBox.paddingBoxRectIncludingScrollbar());

    bool placeVerticalScrollbarOnTheLeft = renderBox.shouldPlaceVerticalScrollbarOnLeft();
    bool haveResizer = renderer().style().resize() != Resize::None;

    OverflowControlRects result;
    auto cornerRect = [&](IntSize cornerSize) {
        if (placeVerticalScrollbarOnTheLeft) {
            auto bottomLeftCorner = overflowControlsPositioningRect.minXMaxYCorner();
            return IntRect { { bottomLeftCorner.x(), bottomLeftCorner.y() - cornerSize.height(), }, cornerSize };
        }
        return IntRect { overflowControlsPositioningRect.maxXMaxYCorner() - cornerSize, cornerSize };
    };

    if (haveResizer) {
        auto scrollbarThickness = ScrollbarTheme::theme().scrollbarThickness();
        result.resizer = cornerRect({ scrollbarThickness, scrollbarThickness });
    }

    return result;
}

String RenderLayer::debugDescription() const
{
    String compositedDescription;
    if (isComposited()) {
        TextStream stream;
        stream << " " << *backing();
        compositedDescription = stream.release();
    }

    return makeString("RenderLayer 0x", hex(reinterpret_cast<uintptr_t>(this), Lowercase),
        ' ', size().width(), 'x', size().height(),
        transform() ? " has transform" : "",
        hasFilter() ? " has filter" : "",
        hasBackdropFilter() ? " has backdrop filter" : "",
        hasBlendMode() ? " has blend mode" : "",
        isolatesBlending() ? " isolates blending" : "",
        compositedDescription);
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& localPoint) const
{
    auto resizerRect = overflowControlsRects().resizer;
    auto resizeCorner = renderer().shouldPlaceVerticalScrollbarOnLeft() ? resizerRect.minXMaxYCorner() : resizerRect.maxXMaxYCorner();
    return localPoint - resizeCorner;
}

int RenderLayer::scrollWidth() const
{
    if (m_scrollableArea)
        return m_scrollableArea->scrollWidth();

    RenderBox* box = renderBox();
    ASSERT(box);
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return roundToInt(overflowRect.maxX() - overflowRect.x());
}

int RenderLayer::scrollHeight() const
{
    if (m_scrollableArea)
        return m_scrollableArea->scrollHeight();

    RenderBox* box = renderBox();
    ASSERT(box);
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return roundToInt(overflowRect.maxY() - overflowRect.y());
}

void RenderLayer::updateScrollInfoAfterLayout()
{
    updateLayerScrollableArea();
    if (m_scrollableArea)
        m_scrollableArea->updateScrollInfoAfterLayout();
}

void RenderLayer::updateScrollbarSteps()
{
    if (m_scrollableArea)
        m_scrollableArea->updateScrollbarSteps();
}

bool RenderLayer::canUseCompositedScrolling() const
{
    if (m_scrollableArea)
        return m_scrollableArea->canUseCompositedScrolling();
    return false;
}

bool RenderLayer::hasCompositedScrollableOverflow() const
{
    if (m_scrollableArea)
        return m_scrollableArea->hasCompositedScrollableOverflow();
    return false;
}

bool RenderLayer::hasOverlayScrollbars() const
{
    if (m_scrollableArea)
        return m_scrollableArea->hasOverlayScrollbars();
    return false;
}

bool RenderLayer::usesCompositedScrolling() const
{
    if (m_scrollableArea)
        return m_scrollableArea->usesCompositedScrolling();
    return false;
}

bool RenderLayer::isPointInResizeControl(IntPoint localPoint) const
{
    if (!canResize())
        return false;

    return overflowControlsRects().resizer.contains(localPoint);
}

void RenderLayer::paint(GraphicsContext& context, const LayoutRect& damageRect, const LayoutSize& subpixelOffset, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot, OptionSet<PaintLayerFlag> paintFlags, SecurityOriginPaintPolicy paintPolicy, EventRegionContext* eventRegionContext)
{
    OverlapTestRequestMap overlapTestRequests;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, subpixelOffset, subtreePaintRoot, &overlapTestRequests, paintPolicy == SecurityOriginPaintPolicy::AccessibleOriginOnly);
    if (eventRegionContext) {
        paintingInfo.eventRegionContext = eventRegionContext;
        paintFlags.add(RenderLayer::PaintLayerFlag::CollectingEventRegion);
    }
    paintLayer(context, paintingInfo, paintFlags);

    for (auto& widget : overlapTestRequests.keys())
        widget->setOverlapTestResult(false);
}

void RenderLayer::clipToRect(GraphicsContext& context, GraphicsContextStateSaver& stateSaver, EventRegionContextStateSaver& eventRegionStateSaver, const LayerPaintingInfo& paintingInfo, OptionSet<PaintBehavior> paintBehavior, const ClipRect& clipRect, BorderRadiusClippingRule rule)
{
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    bool needsClipping = !clipRect.isInfinite() && clipRect.rect() != paintingInfo.paintDirtyRect;
    if (needsClipping || clipRect.affectedByRadius())
        stateSaver.save();

    if (needsClipping) {
        LayoutRect adjustedClipRect = clipRect.rect();
        adjustedClipRect.move(paintingInfo.subpixelOffset);
        auto snappedClipRect = snapRectToDevicePixelsIfNeeded(adjustedClipRect, renderer());
        context.clip(snappedClipRect);
        eventRegionStateSaver.pushClip(enclosingIntRect(snappedClipRect));
    }

    if (clipRect.affectedByRadius()) {
        // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
        // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
        // containing block chain so we check that also.
        for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? this : parent(); layer; layer = layer->parent()) {
            if (paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent) && layer->usesCompositedScrolling())
                break;
        
            if (layer->renderer().hasNonVisibleOverflow() && layer->renderer().style().hasBorderRadius() && ancestorLayerIsInContainingBlockChain(*layer)) {
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

static void performOverlapTests(OverlapTestRequestMap& overlapTestRequests, const RenderLayer* rootLayer, const RenderLayer* layer)
{
    if (overlapTestRequests.isEmpty())
        return;

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
    return layer->renderer().isDocumentElementRenderer() && (paintFlags & RenderLayer::PaintLayerFlag::PaintingRootBackgroundOnly);
}

void RenderLayer::paintLayer(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    auto shouldContinuePaint = [&] () {
        return backing()->paintsIntoWindow()
            || backing()->paintsIntoCompositedAncestor()
            || shouldDoSoftwarePaint(this, paintFlags.contains(PaintLayerFlag::PaintingReflection))
            || paintForFixedRootBackground(this, paintFlags);
    };

    auto paintsIntoDifferentCompositedDestination = [&]() {
        if (paintsIntoProvidedBacking())
            return true;
    
        if (isComposited() && !shouldContinuePaint())
            return true;

        return false;
    };
    
    if (paintsIntoDifferentCompositedDestination()) {
        if (!context.performingPaintInvalidation() && !(paintingInfo.paintBehavior & PaintBehavior::FlattenCompositingLayers))
            return;

        paintFlags.add(PaintLayerFlag::TemporaryClipRects);
    }

    if (viewportConstrainedNotCompositedReason() == NotCompositedForBoundsOutOfView) {
        // Don't paint out-of-view viewport constrained layers (when doing prepainting) because they will never be visible
        // unless their position or viewport size is changed.
        ASSERT(renderer().isFixedPositioned());
        return;
    }

    paintLayerWithEffects(context, paintingInfo, paintFlags);
}

void RenderLayer::paintLayerWithEffects(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    // Non self-painting leaf layers don't need to be painted as their renderer() should properly paint itself.
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return;

    if (shouldSuppressPaintingLayer(this))
        return;

    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer().opacity())
        return;

    if (paintsWithTransparency(paintingInfo.paintBehavior))
        paintFlags.add(PaintLayerFlag::HaveTransparency);

    // PaintLayerFlag::AppliedTransform is used in RenderReplica, to avoid applying the transform twice.
    if (paintsWithTransform(paintingInfo.paintBehavior) && !(paintFlags & PaintLayerFlag::AppliedTransform)) {
        TransformationMatrix layerTransform = renderableTransform(paintingInfo.paintBehavior);
        // If the transform can't be inverted, then don't paint anything.
        if (!layerTransform.isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now, assuming there is a parent
        if (paintFlags & PaintLayerFlag::HaveTransparency) {
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
        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(paintingInfo.eventRegionContext);
        if (parent()) {
            auto options = paintFlags.contains(PaintLayerFlag::PaintingOverflowContents) ? clipRectOptionsForPaintingOverflowControls : clipRectDefaultOptions;
            auto clipRectsContext = ClipRectsContext(paintingInfo.rootLayer, (paintFlags & PaintLayerFlag::TemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, options);
            clipRect = backgroundClipRect(clipRectsContext);
            clipRect.intersect(paintingInfo.paintDirtyRect);
        
            OptionSet<PaintBehavior> paintBehavior = PaintBehavior::Normal;
            if (paintFlags.contains(PaintLayerFlag::PaintingOverflowContents))
                paintBehavior.add(PaintBehavior::CompositedOverflowScrollContent);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
            // Always apply SVG viewport clipping in coordinate system before the SVG viewBox transformation is applied.
            if (is<RenderSVGRoot>(renderer())) {
                auto& svgRoot = downcast<RenderSVGRoot>(renderer());
                if (svgRoot.shouldApplyViewportClip()) {
                    auto newRect = svgRoot.borderBoxRect();

                    auto offsetFromParent = offsetFromAncestor(clipRectsContext.rootLayer);
                    auto offsetForThisLayer = offsetFromParent + paintingInfo.subpixelOffset;
                    auto devicePixelSnappedOffsetForThisLayer = toFloatSize(roundPointToDevicePixels(toLayoutPoint(offsetForThisLayer), renderer().document().deviceScaleFactor()));
                    newRect.move(devicePixelSnappedOffsetForThisLayer.width(), devicePixelSnappedOffsetForThisLayer.height());

                    clipRect.intersect(newRect);
                }
            }
#endif

            // Push the parent coordinate space's clip.
            parent()->clipToRect(context, stateSaver, eventRegionStateSaver, paintingInfo, paintBehavior, clipRect);
        }

        paintLayerByApplyingTransform(context, paintingInfo, paintFlags);
        return;
    }
    
    paintLayerContentsAndReflection(context, paintingInfo, paintFlags);
}

void RenderLayer::paintLayerContentsAndReflection(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    auto localPaintFlags = paintFlags - PaintLayerFlag::AppliedTransform;

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(context, paintingInfo, localPaintFlags | PaintLayerFlag::PaintingReflection);
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
    if (auto* scrollingCoordinator = page().scrollingCoordinator())
        scrollingOnMainThread = scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously(renderer().view().frameView());
#endif

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool contentsScrollByPainting = (renderer().hasNonVisibleOverflow() && !usesCompositedScrolling()) || (renderer().frame().ownerElement());
    bool isZooming = !page().chrome().client().hasStablePageScaleFactor();
    if (scrollingOnMainThread || contentsScrollByPainting || isZooming) {
        didQuantizeFonts = context.shouldSubpixelQuantizeFonts();
        context.setShouldSubpixelQuantizeFonts(false);
        return true;
    }
    return false;
}

std::pair<Path, WindRule> RenderLayer::computeClipPath(const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBoundsForNonBoxes) const
{
    const RenderStyle& style = renderer().style();

    if (is<ShapePathOperation>(*style.clipPath())) {
        auto& clipPath = downcast<ShapePathOperation>(*style.clipPath());
        auto referenceBoxRect = referenceBoxRectForClipPath(clipPath.referenceBox(), offsetFromRoot, rootRelativeBoundsForNonBoxes);
        auto snappedReferenceBoxRect = snapRectToDevicePixelsIfNeeded(referenceBoxRect, renderer());
        return { clipPath.pathForReferenceRect(snappedReferenceBoxRect), clipPath.windRule() };
    }

    if (is<BoxPathOperation>(*style.clipPath()) && is<RenderBox>(renderer())) {
        auto& clipPath = downcast<BoxPathOperation>(*style.clipPath());

        auto shapeRect = computeRoundedRectForBoxShape(clipPath.referenceBox(), downcast<RenderBox>(renderer())).pixelSnappedRoundedRectForPainting(renderer().document().deviceScaleFactor());
        shapeRect.move(offsetFromRoot);

        return { clipPath.pathForReferenceRect(shapeRect), WindRule::NonZero };
    }

    return { Path(), WindRule::NonZero };
}

void RenderLayer::setupClipPath(GraphicsContext& context, GraphicsContextStateSaver& stateSaver, const LayerPaintingInfo& paintingInfo, const LayoutSize& offsetFromRoot)
{
    if (!renderer().hasClipPath() || context.paintingDisabled() || paintingInfo.paintDirtyRect.isEmpty())
        return;

    // SVG elements get clipped in SVG code.
    if (is<LegacyRenderSVGRoot>(renderer()))
        return;

    auto clippedContentBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, { UseLocalClipRectIfPossible });

    auto& style = renderer().style();
    LayoutSize paintingOffsetFromRoot = LayoutSize(snapSizeToDevicePixel(offsetFromRoot + paintingInfo.subpixelOffset, LayoutPoint(), renderer().document().deviceScaleFactor()));
    ASSERT(style.clipPath());
    if (is<ShapePathOperation>(*style.clipPath()) || (is<BoxPathOperation>(*style.clipPath()) && is<RenderBox>(renderer()))) {
        // clippedContentBounds is used as the reference box for inlines, which is also poorly specified: https://github.com/w3c/csswg-drafts/issues/6383.
        auto [path, windRule] = computeClipPath(paintingOffsetFromRoot, clippedContentBounds);
        stateSaver.save();
        context.clipPath(path, windRule);
    }

    if (is<ReferencePathOperation>(style.clipPath())) {
        auto& referenceClipPathOperation = downcast<ReferencePathOperation>(*style.clipPath());
        if (auto* clipperRenderer = renderer().ensureReferencedSVGResources().referencedClipperRenderer(renderer().document(), referenceClipPathOperation)) {
            // Use the border box as the reference box, even though this is not clearly specified: https://github.com/w3c/csswg-drafts/issues/5786.
            // clippedContentBounds is used as the reference box for inlines, which is also poorly specified: https://github.com/w3c/csswg-drafts/issues/6383.
            auto referenceBox = referenceBoxRectForClipPath(CSSBoxType::BorderBox, offsetFromRoot, clippedContentBounds);
            auto snappedReferenceBox = snapRectToDevicePixelsIfNeeded(referenceBox, renderer());
            auto offset = snappedReferenceBox.location();

            auto snappedClippingBounds = snapRectToDevicePixelsIfNeeded(clippedContentBounds, renderer());
            snappedClippingBounds.moveBy(-offset);

            stateSaver.save();
            context.translate(offset);
            clipperRenderer->applyClippingToContext(context, renderer(), { { }, referenceBox.size() }, snappedClippingBounds, renderer().style().effectiveZoom());
            context.translate(-offset);
        }
    }
}

RenderLayerFilters* RenderLayer::filtersForPainting(GraphicsContext& context, OptionSet<PaintLayerFlag> paintFlags) const
{
    if (context.paintingDisabled())
        return nullptr;

    if (paintFlags & PaintLayerFlag::PaintingOverlayScrollbars)
        return nullptr;

    if (!paintsWithFilters())
        return nullptr;

    if (m_filters)
        return m_filters.get();

    return nullptr;
}

GraphicsContext* RenderLayer::setupFilters(GraphicsContext& destinationContext, LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayoutSize& offsetFromRoot)
{
    auto* paintingFilters = filtersForPainting(destinationContext, paintFlags);
    if (!paintingFilters)
        return nullptr;

    LayoutRect filterRepaintRect = paintingFilters->dirtySourceRect();
    filterRepaintRect.move(offsetFromRoot);

    auto rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, { });

    GraphicsContext* filterContext = paintingFilters->beginFilterEffect(renderer(), destinationContext, enclosingIntRect(rootRelativeBounds), enclosingIntRect(paintingInfo.paintDirtyRect), enclosingIntRect(filterRepaintRect));
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

void RenderLayer::applyFilters(GraphicsContext& originalContext, const LayerPaintingInfo& paintingInfo, OptionSet<PaintBehavior> behavior, const LayerFragments& layerFragments)
{
    // FIXME: Handle more than one fragment.
    ClipRect backgroundRect = layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect;

    GraphicsContextStateSaver stateSaver(originalContext, false);
    EventRegionContextStateSaver eventRegionStateSaver(paintingInfo.eventRegionContext);

    clipToRect(originalContext, stateSaver, eventRegionStateSaver, paintingInfo, behavior, backgroundRect);
    m_filters->applyFilterEffect(originalContext);
}

void RenderLayer::paintLayerContents(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (context.detectingContentfulPaint() && context.contentfulPaintDetected())
        return;

    auto localPaintFlags = paintFlags - PaintLayerFlag::AppliedTransform;

    bool haveTransparency = localPaintFlags.contains(PaintLayerFlag::HaveTransparency);
    bool isPaintingOverlayScrollbars = localPaintFlags.contains(PaintLayerFlag::PaintingOverlayScrollbars);
    bool isPaintingScrollingContent = localPaintFlags.contains(PaintLayerFlag::PaintingCompositingScrollingPhase);
    bool isPaintingCompositedForeground = localPaintFlags.contains(PaintLayerFlag::PaintingCompositingForegroundPhase);
    bool isPaintingCompositedBackground = localPaintFlags.contains(PaintLayerFlag::PaintingCompositingBackgroundPhase);
    bool isPaintingOverflowContents = localPaintFlags.contains(PaintLayerFlag::PaintingOverflowContents);
    bool isCollectingEventRegion = localPaintFlags.contains(PaintLayerFlag::CollectingEventRegion);

    bool isSelfPaintingLayer = this->isSelfPaintingLayer();

    // Outline always needs to be painted even if we have no visible content. Also,
    // the outline is painted in the background phase during composited scrolling.
    // If it were painted in the foreground phase, it would move with the scrolled
    // content. When not composited scrolling, the outline is painted in the
    // foreground phase. Since scrolled contents are moved by repainting in this
    // case, the outline won't get 'dragged along'.
    bool shouldPaintOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars && !isCollectingEventRegion
        && (renderer().view().printing() || renderer().view().hasRenderersWithOutline())
        && ((isPaintingScrollingContent && isPaintingCompositedBackground)
        || (!isPaintingScrollingContent && isPaintingCompositedForeground));
    bool shouldPaintContent = m_hasVisibleContent && isSelfPaintingLayer && !isPaintingOverlayScrollbars && !isCollectingEventRegion;

    if (localPaintFlags.contains(PaintLayerFlag::PaintingRootBackgroundOnly) && !renderer().isRenderView() && !renderer().isDocumentElementRenderer()) {
        // If beginTransparencyLayers was called prior to this, ensure the transparency state is cleaned up before returning.
        if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
            context.endTransparencyLayer();
            context.restore();
            m_usedTransparency = false;
        }

        return;
    }

    updateLayerListsIfNeeded();

    LayoutSize offsetFromRoot = offsetFromAncestor(paintingInfo.rootLayer);

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool didQuantizeFonts = true;
    bool needToAdjustSubpixelQuantization = setupFontSubpixelQuantization(context, didQuantizeFonts);

    // Apply clip-path to context.
    LayoutSize columnAwareOffsetFromRoot = offsetFromRoot;
    if (renderer().enclosingFragmentedFlow() && (renderer().hasClipPath() || filtersForPainting(context, paintFlags)))
        columnAwareOffsetFromRoot = toLayoutSize(convertToLayerCoords(paintingInfo.rootLayer, LayoutPoint(), AdjustForColumns));

    GraphicsContextStateSaver stateSaver(context, false);
    if (shouldApplyClipPath(paintingInfo.paintBehavior, localPaintFlags))
        setupClipPath(context, stateSaver, paintingInfo, columnAwareOffsetFromRoot);

    bool selectionAndBackgroundsOnly = paintingInfo.paintBehavior.contains(PaintBehavior::SelectionAndBackgroundsOnly);
    bool selectionOnly = paintingInfo.paintBehavior.contains(PaintBehavior::SelectionOnly);

    SinglePaintFrequencyTracking singlePaintFrequencyTracking(m_paintFrequencyTracker, page().lastRenderingUpdateTimestamp());

    LayerFragments layerFragments;
    RenderObject* subtreePaintRootForRenderer = nullptr;

    auto paintBehavior = [&]() {
        constexpr OptionSet<PaintBehavior> flagsToCopy = { PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting, PaintBehavior::ExcludeSelection };
        OptionSet<PaintBehavior> paintBehavior = paintingInfo.paintBehavior & flagsToCopy;

        if (localPaintFlags.contains(PaintLayerFlag::PaintingSkipRootBackground))
            paintBehavior.add(PaintBehavior::SkipRootBackground);
        else if (localPaintFlags.contains(PaintLayerFlag::PaintingRootBackgroundOnly))
            paintBehavior.add(PaintBehavior::RootBackgroundOnly);

        // FIXME: This seems wrong. We should retain the TileFirstPaint flag for all RenderLayers painted into the root tile cache.
        if ((paintingInfo.paintBehavior & PaintBehavior::TileFirstPaint) && isRenderViewLayer())
            paintBehavior.add(PaintBehavior::TileFirstPaint);

        if (isPaintingOverflowContents)
            paintBehavior.add(PaintBehavior::CompositedOverflowScrollContent);

        if (isCollectingEventRegion) {
            paintBehavior = paintBehavior & PaintBehavior::CompositedOverflowScrollContent;
            if (isPaintingCompositedForeground)
                paintBehavior.add(PaintBehavior::EventRegionIncludeForeground);
            if (isPaintingCompositedBackground)
                paintBehavior.add(PaintBehavior::EventRegionIncludeBackground);
        }

        return paintBehavior;
    }();

    { // Scope for filter-related state changes.
        LayerPaintingInfo localPaintingInfo(paintingInfo);
        GraphicsContext* filterContext = setupFilters(context, localPaintingInfo, paintFlags, columnAwareOffsetFromRoot);
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

        LayoutRect paintDirtyRect = localPaintingInfo.paintDirtyRect;
        if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars || isCollectingEventRegion) {
            // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment, as well as whether or not the content of each
            // fragment should paint. If the parent's filter dictates full repaint to ensure proper filter effect,
            // use the overflow clip as dirty rect, instead of no clipping. It maintains proper clipping for overflow::scroll.
            if (!localPaintingInfo.clipToDirtyRect && renderer().hasNonVisibleOverflow()) {
                // We can turn clipping back by requesting full repaint for the overflow area.
                localPaintingInfo.clipToDirtyRect = true;
                paintDirtyRect = clipRectRelativeToAncestor(localPaintingInfo.rootLayer, offsetFromRoot, LayoutRect::infiniteRect());
            }

            auto clipRectOptions = isPaintingOverflowContents ? clipRectOptionsForPaintingOverflowControls : clipRectDefaultOptions;
            collectFragments(layerFragments, localPaintingInfo.rootLayer, paintDirtyRect, ExcludeCompositedPaginatedLayers,
                (localPaintFlags & PaintLayerFlag::TemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, clipRectOptions, offsetFromRoot);
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
            paintList(negativeZOrderLayers(), currentContext, localPaintingInfo, localPaintFlags);
        
        if (isPaintingCompositedForeground) {
            if (shouldPaintContent) {
                paintForegroundForFragments(layerFragments, currentContext, context, paintingInfo.paintDirtyRect, haveTransparency,
                    localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);
            }
        }

        if (isCollectingEventRegion)
            collectEventRegionForFragments(layerFragments, currentContext, localPaintingInfo, paintBehavior);

        if (shouldPaintOutline)
            paintOutlineForFragments(layerFragments, currentContext, localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);

        if (isPaintingCompositedForeground) {
            // Paint any child layers that have overflow.
            paintList(normalFlowLayers(), currentContext, localPaintingInfo, localPaintFlags);
        
            // Now walk the sorted list of children with positive z-indices.
            paintList(positiveZOrderLayers(), currentContext, localPaintingInfo, localPaintFlags);
        }

        if (m_scrollableArea) {
            if (isPaintingOverlayScrollbars && m_scrollableArea->hasScrollbars())
                paintOverflowControlsForFragments(layerFragments, currentContext, localPaintingInfo);
        }

        if (filterContext) {
            // When we called collectFragments() last time, paintDirtyRect was reset to represent the filter bounds.
            // Now we need to compute the backgroundRect uncontaminated by filters, in order to clip the filtered result.
            // Note that we also use paintingInfo here, not localPaintingInfo which filters also contaminated.
            LayerFragments layerFragments;
            auto clipRectOptions = isPaintingOverflowContents ? clipRectOptionsForPaintingOverflowControls : clipRectDefaultOptions;
            collectFragments(layerFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, ExcludeCompositedPaginatedLayers,
                (localPaintFlags & PaintLayerFlag::TemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, clipRectOptions, offsetFromRoot);
            updatePaintingInfoForFragments(layerFragments, paintingInfo, localPaintFlags, shouldPaintContent, offsetFromRoot);

            applyFilters(context, paintingInfo, paintBehavior, layerFragments);
        }
    }
    
    if (shouldPaintContent && !(selectionOnly || selectionAndBackgroundsOnly)) {
        if (shouldPaintMask(paintingInfo.paintBehavior, localPaintFlags)) {
            // Paint the mask for the fragments.
            paintMaskForFragments(layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer);
        }

        if (!(paintFlags & PaintLayerFlag::PaintingCompositingMaskPhase) && (paintFlags & PaintLayerFlag::PaintingCompositingClipPathPhase)) {
            // Re-use paintChildClippingMaskForFragments to paint black for the compositing clipping mask.
            paintChildClippingMaskForFragments(layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer);
        }
        
        if (localPaintFlags & PaintLayerFlag::PaintingChildClippingMaskPhase) {
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
}

void RenderLayer::paintLayerByApplyingTransform(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayoutSize& translationOffset)
{
    // This involves subtracting out the position of the layer in our current coordinate space, but preserving
    // the accumulated error for sub-pixel layout.
    // Note: The pixel-snapping logic is disabled for the whole SVG render tree, except the outermost <svg>.
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    LayoutSize offsetFromParent = offsetFromAncestor(paintingInfo.rootLayer);
    offsetFromParent += translationOffset;
    TransformationMatrix transform(renderableTransform(paintingInfo.paintBehavior));
    // Add the subpixel accumulation to the current layer's offset so that we can always snap the translateRight value to where the renderer() is supposed to be painting.
    LayoutSize offsetForThisLayer = offsetFromParent + paintingInfo.subpixelOffset;
    FloatSize alignedOffsetForThisLayer = rendererNeedsPixelSnapping(renderer()) ? toFloatSize(roundPointToDevicePixels(toLayoutPoint(offsetForThisLayer), deviceScaleFactor)) : offsetForThisLayer;
    // We handle accumulated subpixels through nested layers here. Since the context gets translated to device pixels,
    // all we need to do is add the delta to the accumulated pixels coming from ancestor layers.
    // Translate the graphics context to the snapping position to avoid off-device-pixel positing.
    transform.translateRight(alignedOffsetForThisLayer.width(), alignedOffsetForThisLayer.height());
    // Apply the transform.
    auto oldTransform = context.getCTM();
    auto affineTransform = transform.toAffineTransform();
    context.concatCTM(affineTransform);

    if (paintingInfo.eventRegionContext)
        paintingInfo.eventRegionContext->pushTransform(affineTransform);

    // Only propagate the subpixel offsets to the descendant layers, if we're not the root
    // of a SVG subtree, where no pixel snapping is applied -- only the outermost <svg> layer
    // is pixel-snapped "as whole", if it's part of a compound document, e.g. inline SVG in HTML.
    LayoutSize adjustedSubpixelOffset;
    if (rendererNeedsPixelSnapping(renderer()) && !renderer().isSVGRoot())
        adjustedSubpixelOffset = offsetForThisLayer - LayoutSize(alignedOffsetForThisLayer);

    // Now do a paint with the root layer shifted to be us.
    LayerPaintingInfo transformedPaintingInfo(paintingInfo);
    transformedPaintingInfo.rootLayer = this;
    transformedPaintingInfo.paintDirtyRect = LayoutRect(encloseRectToDevicePixels(valueOrDefault(transform.inverse()).mapRect(paintingInfo.paintDirtyRect), deviceScaleFactor));
    transformedPaintingInfo.subpixelOffset = adjustedSubpixelOffset;
    paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags);

    if (paintingInfo.eventRegionContext)
        paintingInfo.eventRegionContext->popTransform();

    context.setCTM(oldTransform);
}

void RenderLayer::paintList(LayerList layerIterator, GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    if (layerIterator.begin() == layerIterator.end())
        return;

    if (!hasSelfPaintingLayerDescendant())
        return;

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(*this);
#endif

    for (auto* childLayer : layerIterator)
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
    ClipRectsType clipRectsType, OptionSet<ClipRectsOption> clipRectOptions, const LayoutSize& offsetFromRoot,
    const LayoutRect* layerBoundingBox, ShouldApplyRootOffsetToFragments applyRootOffsetToFragments)
{
    RenderLayer* paginationLayer = enclosingPaginationLayerInSubtree(rootLayer, inclusionMode);
    if (!paginationLayer || hasTransform()) {
        // For unpaginated layers, there is only one fragment.
        LayerFragment fragment;
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, clipRectOptions);
        calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, offsetFromRoot);
        fragments.append(fragment);
        return;
    }
    
    // Compute our offset within the enclosing pagination layer.
    LayoutSize offsetWithinPaginatedLayer = offsetFromAncestor(paginationLayer);
    
    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(paginationLayer, clipRectsType, clipRectOptions);
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
        parentPaginationLayer->collectFragments(ancestorFragments, rootLayer, dirtyRect, inclusionMode, clipRectsType, clipRectOptions,
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
                fragment.setRects(layerBoundsInFragmentedFlow, backgroundRectInFragmentedFlow, foregroundRectInFragmentedFlow, layerBoundingBoxInFragmentedFlow);
                
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
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, clipRectOptions);
        ancestorClipRect = paginationLayer->backgroundClipRect(clipRectsContext);
        ancestorClipRect.intersect(dirtyRect);
    }

    for (auto& fragment : fragments) {
        // Set our four rects with all clipping applied that was internal to the flow thread.
        fragment.setRects(layerBoundsInFragmentedFlow, backgroundRectInFragmentedFlow, foregroundRectInFragmentedFlow, layerBoundingBoxInFragmentedFlow);
        
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
        if (this != localPaintingInfo.rootLayer || !(localPaintFlags & PaintLayerFlag::PaintingOverflowContents)) {
            LayoutSize newOffsetFromRoot = offsetFromRoot + fragment.paginationOffset;
            fragment.shouldPaintContent &= intersectsDamageRect(fragment.layerBounds, fragment.backgroundRect.rect(), localPaintingInfo.rootLayer, newOffsetFromRoot, fragment.boundingBox);
        }
    }
}

void RenderLayer::paintTransformedLayerIntoFragments(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(ExcludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintingInfo.paintBehavior);

    auto clipRectOptions = paintFlags.contains(PaintLayerFlag::PaintingOverflowContents) ? clipRectOptionsForPaintingOverflowControls : clipRectDefaultOptions;
    paginatedLayer->collectFragments(enclosingPaginationFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, ExcludeCompositedPaginatedLayers,
        (paintFlags & PaintLayerFlag::TemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, clipRectOptions, offsetOfPaginationLayerFromRoot, &transformedExtent);
    
    for (const auto& fragment : enclosingPaginationFragments) {
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(paintingInfo.rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));

            auto clipRectsContext = ClipRectsContext(paginatedLayer, (paintFlags & PaintLayerFlag::TemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, clipRectOptions);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.move(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        OptionSet<PaintBehavior> paintBehavior = PaintBehavior::Normal;
        if (paintFlags.contains(PaintLayerFlag::PaintingOverflowContents))
            paintBehavior.add(PaintBehavior::CompositedOverflowScrollContent);

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(paintingInfo.eventRegionContext);

        parent()->clipToRect(context, stateSaver, eventRegionStateSaver, paintingInfo, paintBehavior, clipRect);
        paintLayerByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset);
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
    
        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        if (localPaintingInfo.clipToDirtyRect) {
            // Paint our background first, before painting any child layers.
            // Establish the clip used to paint our background.
            clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, paintBehavior, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
        }
        
        // Paint the background.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::BlockBackground, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
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

    // FIXME: It's unclear if this flag copying is necessary.
    if (localPaintingInfo.paintBehavior & PaintBehavior::ExcludeSelection)
        localPaintBehavior.add(PaintBehavior::ExcludeSelection);
    
    if (localPaintingInfo.paintBehavior & PaintBehavior::Snapshotting)
        localPaintBehavior.add(PaintBehavior::Snapshotting);
    
    if (localPaintingInfo.paintBehavior & PaintBehavior::TileFirstPaint)
        localPaintBehavior.add(PaintBehavior::TileFirstPaint);

    if (localPaintingInfo.paintBehavior & PaintBehavior::CompositedOverflowScrollContent)
        localPaintBehavior.add(PaintBehavior::CompositedOverflowScrollContent);

    GraphicsContextStateSaver stateSaver(context, false);
    EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && layerFragments[0].shouldPaintContent && !layerFragments[0].foregroundRect.isEmpty();
    if (shouldClip)
        clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, localPaintBehavior, layerFragments[0].foregroundRect);
    
    // We have to loop through every fragment multiple times, since we have to repaint in each specific phase in order for
    // interleaving of the fragments to work properly.
    bool selectionOnly = localPaintingInfo.paintBehavior.contains(PaintBehavior::SelectionOnly);
    bool selectionAndBackgroundsOnly = localPaintingInfo.paintBehavior.contains(PaintBehavior::SelectionAndBackgroundsOnly);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGModelObject>(renderer()) && !is<RenderSVGContainer>(renderer())) {
        // SVG containers need to propagate paint phases. This could be saved if we remember somewhere if a SVG subtree
        // contains e.g. LegacyRenderSVGForeignObject objects that do need the individual paint phases. For SVG shapes & SVG images
        // we can avoid the multiple paintForegroundForFragmentsWithPhase() calls.
        if (selectionOnly || selectionAndBackgroundsOnly)
            return;

        paintForegroundForFragmentsWithPhase(PaintPhase::Foreground, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        return;
    }
#endif

    if (!selectionOnly)
        paintForegroundForFragmentsWithPhase(PaintPhase::ChildBlockBackgrounds, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);

    if (selectionOnly || selectionAndBackgroundsOnly)
        paintForegroundForFragmentsWithPhase(PaintPhase::Selection, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    else {
        paintForegroundForFragmentsWithPhase(PaintPhase::Float, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhase::Foreground, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhase::ChildOutlines, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    }
}

void RenderLayer::paintForegroundForFragmentsWithPhase(PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext& context,
    const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() > 1;

    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent || fragment.foregroundRect.isEmpty())
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        if (shouldClip)
            clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, paintBehavior, fragment.foregroundRect);
    
        PaintInfo paintInfo(context, fragment.foregroundRect.rect(), phase, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this, localPaintingInfo.requireSecurityOriginAccessForWidgets);
        if (phase == PaintPhase::Foreground)
            paintInfo.overlapTestRequests = localPaintingInfo.overlapTestRequests;
        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
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

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, paintBehavior, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius);
        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
    }
}

void RenderLayer::paintMaskForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent)
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, paintBehavior, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.
        
        // Paint the mask.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::Mask, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
    }
}

void RenderLayer::paintChildClippingMaskForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent)
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, paintBehavior, fragment.foregroundRect, IncludeSelfForBorderRadius); // Child clipping mask painting will handle clipping to self.

        // Paint the clipped mask.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhase::ClippingMask, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer(), this);
        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
    }
}

void RenderLayer::paintOverflowControlsForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo)
{
    ASSERT(m_scrollableArea);

    for (const auto& fragment : layerFragments) {
        if (fragment.backgroundRect.isEmpty())
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        EventRegionContextStateSaver eventRegionStateSaver(localPaintingInfo.eventRegionContext);

        clipToRect(context, stateSaver, eventRegionStateSaver, localPaintingInfo, { }, fragment.backgroundRect);
        m_scrollableArea->paintOverflowControls(context, roundedIntPoint(paintOffsetForRenderer(fragment, localPaintingInfo)), snappedIntRect(fragment.backgroundRect.rect()), true);
    }
}

void RenderLayer::collectEventRegionForFragments(const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintBehavior> paintBehavior)
{
    ASSERT(localPaintingInfo.eventRegionContext);
    for (const auto& fragment : layerFragments) {
        PaintInfo paintInfo(context, fragment.foregroundRect.rect(), PaintPhase::EventRegion, paintBehavior);
        paintInfo.eventRegionContext = localPaintingInfo.eventRegionContext;
        if (localPaintingInfo.clipToDirtyRect)
            paintInfo.eventRegionContext->pushClip(enclosingIntRect(fragment.backgroundRect.rect()));

        renderer().paint(paintInfo, paintOffsetForRenderer(fragment, localPaintingInfo));
        if (localPaintingInfo.clipToDirtyRect)
            paintInfo.eventRegionContext->popClip();
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
            hitTestArea.intersect(renderer().view().frameView().visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect));
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

Vector<RenderLayer*> RenderLayer::topLayerRenderLayers(const RenderView& renderView)
{
    Vector<RenderLayer*> layers;
    for (auto& element : renderView.document().topLayerElements()) {
        auto* renderer = element->renderer();
        if (!renderer)
            continue;

        auto backdropRenderer = renderer->backdropRenderer();
        if (backdropRenderer && backdropRenderer->hasLayer() && backdropRenderer->layer()->parent())
            layers.append(backdropRenderer->layer());

        if (renderer->hasLayer() && downcast<RenderLayerModelObject>(*renderer).layer()->parent())
            layers.append(downcast<RenderLayerModelObject>(*renderer).layer());
    }
    return layers;
}

bool RenderLayer::establishesTopLayer() const
{
    return isInTopLayerOrBackdrop(renderer().style(), renderer().element());
}

void RenderLayer::establishesTopLayerWillChange()
{
    if (auto* parentLayer = parent())
        parentLayer->removeChild(*this);
}

void RenderLayer::establishesTopLayerDidChange()
{
    if (auto* parentLayer = renderer().layerParent())
        parentLayer->addChild(*this);
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
    updateLayerListsIfNeeded();

    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return nullptr;

    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        if (enclosingPaginationLayer(IncludeCompositedPaginatedLayers))
            return hitTestTransformedLayerInFragments(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);

        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRectsContext clipRectsContext(rootLayer, RootRelativeClipRects, { ClipRectsOption::RespectOverflowClip, ClipRectsOption::IncludeOverlayScrollbarSize });
            ClipRect clipRect = backgroundClipRect(clipRectsContext);
            // Test the enclosing clip now.
            if (!clipRect.intersects(hitTestLocation))
                return nullptr;
        }

        return hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
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
#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(*this);
#endif

    auto offsetFromRoot = offsetFromAncestor(rootLayer);
    // FIXME: We need to correctly hit test the clip-path when we have a RenderInline too.
    if (auto* rendererBox = this->renderBox(); rendererBox && !rendererBox->hitTestClipPath(hitTestLocation, toLayoutPoint(offsetFromRoot - toLayoutSize(rendererLocation()))))
        return nullptr;

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    auto* hitLayer = hitTestList(positiveZOrderLayers(), rootLayer, request, result, hitTestRect, hitTestLocation,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestList(normalFlowLayers(), rootLayer, request, result, hitTestRect, hitTestLocation,
                           localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Collect the fragments. This will compute the clip rectangles for each layer fragment.
    LayerFragments layerFragments;
    collectFragments(layerFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers, RootRelativeClipRects, { ClipRectsOption::RespectOverflowClip, ClipRectsOption::IncludeOverlayScrollbarSize }, offsetFromRoot);

    LayoutPoint localPoint;
    if (canResize() && m_scrollableArea && m_scrollableArea->hitTestResizerInFragments(layerFragments, hitTestLocation, localPoint)) {
        renderer().updateHitTestResult(result, localPoint);
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
    hitLayer = hitTestList(negativeZOrderLayers(), rootLayer, request, result, hitTestRect, hitTestLocation,
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

RenderLayer* RenderLayer::hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, HitTestingTransparencyClipBox, RootOfTransparencyClipBox);
    paginatedLayer->collectFragments(enclosingPaginationFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers,
        RootRelativeClipRects, { ClipRectsOption::RespectOverflowClip, ClipRectsOption::IncludeOverlayScrollbarSize }, offsetOfPaginationLayerFromRoot, &transformedExtent);

    for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);
        
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));
    
            ClipRectsContext clipRectsContext(paginatedLayer, RootRelativeClipRects, { ClipRectsOption::RespectOverflowClip, ClipRectsOption::IncludeOverlayScrollbarSize });
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
        newHitTestLocation = HitTestLocation(flooredLayoutPoint(localPoint));

    // Now do a hit test with the root layer shifted to be us.
    return hitTestLayer(this, containerLayer, request, result, localHitTestRect, newHitTestLocation, true, newTransformState.ptr(), zOffset);
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter) const
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (!renderer().hitTest(request, result, hitTestLocation, toLayoutPoint(layerBounds.location() - rendererLocation()), hitTestFilter)) {
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
        Element* e = enclosingElement();
        if (!result.innerNode())
            result.setInnerNode(e);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(e);
    }
        
    return true;
}

RenderLayer* RenderLayer::hitTestList(LayerList layerIterator, RenderLayer* rootLayer,
                                      const HitTestRequest& request, HitTestResult& result,
                                      const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                      const HitTestingTransformState* transformState, 
                                      double* zOffsetForDescendants, double* zOffset,
                                      const HitTestingTransformState* unflattenedTransformState,
                                      bool depthSortDescendants)
{
    if (layerIterator.begin() == layerIterator.end())
        return nullptr;

    if (!hasSelfPaintingLayerDescendant())
        return nullptr;

    RenderLayer* resultLayer = nullptr;

    for (auto iter = layerIterator.rbegin(); iter != layerIterator.rend(); ++iter) {
        auto* childLayer = *iter;

        HitTestResult tempResult(result.hitTestLocation());
        auto* hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it is a list-based test, we can safely append the temporary result since it might had hit
        // nodes but not necessarily had hitLayer set.
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
        if (auto* clipRects = m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip())) {
            ASSERT(clipRectsContext.rootLayer == m_clipRectsCache->m_clipRectsRoot[clipRectsType]);
            ASSERT(m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] == clipRectsContext.overlayScrollbarSizeRelevancy());
        
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
        m_clipRectsCache = makeUnique<ClipRectsCache>();
#if ASSERT_ENABLED
    m_clipRectsCache->m_clipRectsRoot[clipRectsType] = clipRectsContext.rootLayer;
    m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] = clipRectsContext.overlayScrollbarSizeRelevancy();
#endif

    RefPtr<ClipRects> parentClipRects;
    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    if (clipRectsContext.rootLayer != this && parent())
        parentClipRects = this->parentClipRects(clipRectsContext);

    auto clipRects = ClipRects::create();
    calculateClipRects(clipRectsContext, clipRects);

    if (parentClipRects && *parentClipRects == clipRects) {
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip(), parentClipRects.copyRef());
        return parentClipRects.releaseNonNull();
    }
    m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip(), clipRects.copyRef());
    return clipRects;
}

ClipRects* RenderLayer::clipRects(const ClipRectsContext& context) const
{
    ASSERT(context.clipRectsType < NumCachedClipRectsTypes);
    if (!m_clipRectsCache)
        return nullptr;
    return m_clipRectsCache->getClipRects(context.clipRectsType, context.respectOverflowClip());
}

bool RenderLayer::clipCrossesPaintingBoundary() const
{
    return parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers) != enclosingPaginationLayer(IncludeCompositedPaginatedLayers)
        || parent()->enclosingCompositingLayerForRepaint().layer != enclosingCompositingLayerForRepaint().layer;
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
            parentContext.options.remove(ClipRectsOption::IncludeOverlayScrollbarSize); // FIXME: Why?
            
            if ((parentContext.clipRectsType != TemporaryClipRects && parentContext.clipRectsType != AbsoluteClipRects) && clipCrossesPaintingBoundary())
                parentContext.clipRectsType = TemporaryClipRects;

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
#if PLATFORM(IOS_FAMILY)
    if (renderer().hasClipOrNonVisibleOverflow() && (clipRectsContext.respectOverflowClip() || this != clipRectsContext.rootLayer)) {
#else
    if ((renderer().hasNonVisibleOverflow() && (clipRectsContext.respectOverflowClip() || this != clipRectsContext.rootLayer)) || renderer().hasClip()) {
#endif
        // This layer establishes a clip of some kind.
        LayoutPoint offset;
        if (!m_hasTransformedAncestor && canUseOffsetFromAncestor())
            offset = toLayoutPoint(offsetFromAncestor(clipRectsContext.rootLayer, AdjustForColumns));
        else
            offset = LayoutPoint(renderer().localToContainerPoint(FloatPoint(), &clipRectsContext.rootLayer->renderer()));

        if (clipRects.fixed() && &clipRectsContext.rootLayer->renderer() == &renderer().view())
            offset -= toLayoutSize(renderer().view().frameView().scrollPositionForFixedPosition());

        if (renderer().hasNonVisibleOverflow()) {
            ClipRect newOverflowClip = rendererOverflowClipRectForChildLayers(offset, nullptr, clipRectsContext.overlayScrollbarSizeRelevancy());
            newOverflowClip.setAffectedByRadius(renderer().style().hasBorderRadius());
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer().canContainAbsolutelyPositionedObjects())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
            if (renderer().canContainFixedPositionObjects())
                clipRects.setFixedClipRect(intersection(newOverflowClip, clipRects.fixedClipRect()));
        }
        if (renderer().hasClip() && is<RenderBox>(renderer())) {
            LayoutRect newPosClip = downcast<RenderBox>(renderer()).clipRect(offset, nullptr);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    } else if (renderer().hasNonVisibleOverflow() && transform() && renderer().style().hasBorderRadius())
        clipRects.setOverflowClipRectAffectedByRadius();

    LOG_WITH_STREAM(ClipRects, stream << "RenderLayer " << this << " calculateClipRects " << clipRectsContext << " computed " << clipRects);
}

Ref<ClipRects> RenderLayer::parentClipRects(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(parent());

    auto containerLayer = parent();
    auto temporaryParentClipRects = [&](const ClipRectsContext& clipContext) {
        auto parentClipRects = ClipRects::create();
        containerLayer->calculateClipRects(clipContext, parentClipRects);
        return parentClipRects;
    };

    if (clipRectsContext.clipRectsType == TemporaryClipRects)
        return temporaryParentClipRects(clipRectsContext);

    if (clipRectsContext.clipRectsType != AbsoluteClipRects && clipCrossesPaintingBoundary()) {
        ClipRectsContext tempClipRectsContext(clipRectsContext);
        tempClipRectsContext.clipRectsType = TemporaryClipRects;
        return temporaryParentClipRects(tempClipRectsContext);
    }

    return containerLayer->updateClipRects(clipRectsContext);
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
    auto parentRects = parentClipRects(clipRectsContext);
    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, renderer().style().position());
    RenderView& view = renderer().view();
    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects->fixed() && &clipRectsContext.rootLayer->renderer() == &view && !backgroundClipRect.isInfinite())
        backgroundClipRect.moveBy(view.frameView().scrollPositionForFixedPosition());

    LOG_WITH_STREAM(ClipRects, stream << "RenderLayer " << this << " backgroundClipRect with context " << clipRectsContext << " returning " << backgroundClipRect);
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

    layerBounds = LayoutRect(toLayoutPoint(offsetFromRootLocal), size());

    foregroundRect = backgroundRect;

    // Update the clip rects that will be passed to child layers.
    if (renderer().hasClipOrNonVisibleOverflow()) {
        // This layer establishes a clip of some kind.
        if (renderer().hasNonVisibleOverflow()) {
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip()) {
                LayoutRect overflowClipRect = rendererOverflowClipRect(toLayoutPoint(offsetFromRootLocal), nullptr, clipRectsContext.overlayScrollbarSizeRelevancy());
                foregroundRect.intersect(overflowClipRect);
                foregroundRect.setAffectedByRadius(true);
            } else if (transform() && renderer().style().hasBorderRadius())
                foregroundRect.setAffectedByRadius(true);
        }

        if (renderer().hasClip() && is<RenderBox>(renderer())) {
            // Clip applies to *us* as well, so update the damageRect.
            LayoutRect newPosClip = downcast<RenderBox>(renderer()).clipRect(toLayoutPoint(offsetFromRootLocal), nullptr);
            backgroundRect.intersect(newPosClip);
            foregroundRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then make sure our background rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (rendererHasVisualOverflow()) {
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = rendererVisualOverflowRect();
            if (renderer().isBox())
                renderBox()->flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip())
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // Shift the bounds to be for our region only.
            LayoutRect bounds = rendererBorderBoxRectInFragment(nullptr);

            bounds.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip())
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

    if (renderer().hasClip() && is<RenderBox>(renderer())) {
        // CSS clip may be larger than our border box.
        LayoutRect cssClipRect = downcast<RenderBox>(renderer()).clipRect({ }, nullptr);
        clipExceedsBounds = !cssClipRect.isEmpty() && (clipRect.width() < cssClipRect.width() || clipRect.height() < cssClipRect.height());
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
    if (m_scrollableArea)
        rect.moveBy(-m_scrollableArea->scrollPosition());
    if (renderer().hasNonVisibleOverflow() && !usesCompositedScrolling())
        rect.intersect(downcast<RenderBox>(renderer()).overflowClipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (renderer().hasClip())
        rect.intersect(downcast<RenderBox>(renderer()).clipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (!rect.isEmpty())
        renderer().repaintRectangle(rect);
}

bool RenderLayer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot, const std::optional<LayoutRect>& cachedBoundingBox) const
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
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    else if (is<RenderSVGModelObject>(renderer()))
        result = downcast<RenderSVGModelObject>(renderer()).visualOverflowRectEquivalent();
#endif
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
        } else
            result = box->visualOverflowRect();
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
    if (auto styleable = Styleable::fromRenderer(renderer())) {
        if (styleable->computeAnimationExtent(animatedBounds)) {
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

LayoutRect RenderLayer::overlapBounds() const
{
    if (overlapBoundsIncludeChildren())
        return calculateLayerBounds(this, { }, defaultCalculateLayerBoundsFlags() | IncludeFilterOutsets);
    
    return localBoundingBox();
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
    
    ASSERT(isStackingContext() || !positiveZOrderLayers().size());

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer&>(*this));
#endif

    auto computeLayersUnion = [this, &unionBounds, flags, descendantFlags] (const RenderLayer& childLayer) {
        if (!(flags & IncludeCompositedDescendants) && (childLayer.isComposited() || childLayer.paintsIntoProvidedBacking()))
            return;
        LayoutRect childBounds = childLayer.calculateLayerBounds(this, childLayer.offsetFromAncestor(this), descendantFlags);
        // Ignore child layer (and behave as if we had overflow: hidden) when it is positioned off the parent layer so much
        // that we hit the max LayoutUnit value.
        unionBounds.checkedUnite(childBounds);
    };

    for (auto* childLayer : negativeZOrderLayers())
        computeLayersUnion(*childLayer);

    for (auto* childLayer : positiveZOrderLayers())
        computeLayersUnion(*childLayer);

    for (auto* childLayer : normalFlowLayers())
        computeLayersUnion(*childLayer);

    if (flags.contains(IncludeFilterOutsets) || (flags.contains(IncludePaintedFilterOutsets) && paintsWithFilters()))
        unionBounds.expand(toLayoutBoxExtent(filterOutsets()));

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
        m_backing = makeUnique<RenderLayerBacking>(*this);
        compositor().layerBecameComposited(*this);

        updateFilterPaintingStrategy();
    }
    return m_backing.get();
}

void RenderLayer::clearBacking(bool layerBeingDestroyed)
{
    if (!m_backing)
        return;

    if (!renderer().renderTreeBeingDestroyed())
        compositor().layerBecameNonComposited(*this);
    
    m_backing->willBeDestroyed();
    m_backing = nullptr;

    if (!layerBeingDestroyed)
        updateFilterPaintingStrategy();
}

bool RenderLayer::hasCompositedMask() const
{
    return m_backing && m_backing->hasMaskLayer();
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

    return paintFlags.contains(PaintLayerFlag::PaintingCompositingMaskPhase);
}

bool RenderLayer::shouldApplyClipPath(OptionSet<PaintBehavior> paintBehavior, OptionSet<PaintLayerFlag> paintFlags) const
{
    if (!renderer().hasClipPath())
        return false;

    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
    if (paintsToWindow || (paintBehavior & PaintBehavior::FlattenCompositingLayers))
        return true;

    return paintFlags.contains(PaintLayerFlag::PaintingCompositingClipPathPhase);
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
    if (zOrderListsDirty() || normalFlowListDirty())
        return false;

    // Table painting is special; a table paints its sections.
    if (renderer().isTablePart())
        return false;

    // A fieldset with a legend will have an irregular shape, so can't be treated as opaque.
    if (renderer().isFieldset())
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (renderer().backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;
    
    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (renderer().hasNonVisibleOverflow())
        return false;
    
    return listBackgroundIsKnownToBeOpaqueInRect(positiveZOrderLayers(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(negativeZOrderLayers(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(normalFlowLayers(), localRect);
}

bool RenderLayer::listBackgroundIsKnownToBeOpaqueInRect(const LayerList& list, const LayoutRect& localRect) const
{
    if (list.begin() == list.end())
        return false;

    for (auto iter = list.rbegin(); iter != list.rend(); ++iter) {
        const auto* childLayer = *iter;
        if (childLayer->isComposited())
            continue;

        if (!childLayer->canUseOffsetFromAncestor())
            continue;

        LayoutRect childLocalRect(localRect);
        childLocalRect.move(-childLayer->offsetFromAncestor(this));

        if (childLayer->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
    }
    return false;
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
void RenderLayer::repaintIncludingNonCompositingDescendants(const RenderLayerModelObject* repaintContainer)
{
    auto clippedOverflowRect = m_repaintRectsValid ? m_repaintRects.clippedOverflowRect : renderer().clippedOverflowRectForRepaint(repaintContainer);
    renderer().repaintUsingContainer(repaintContainer, clippedOverflowRect);

    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    if (!isNormalFlowOnly())
        return true;

    return hasOverlayScrollbars()
        || hasCompositedScrollableOverflow()
        || renderer().isTableRow()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || renderer().isRenderFragmentedFlow();
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
    else {
        parent()->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
        clearRepaintRects();
    }
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
            if (!renderText.hasRenderedText())
                continue;

            if (renderer.style().effectiveUserSelect() != UserSelect::None)
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

    return hasVisibleBoxDecorationsOrBackground() || (m_scrollableArea && m_scrollableArea->hasOverflowControls());
}

bool RenderLayer::isVisuallyNonEmpty(PaintedContentRequest* request) const
{
    ASSERT(!m_visibleDescendantStatusDirty);

    if (!hasVisibleContent() || !renderer().style().opacity())
        return false;

    if (renderer().isRenderReplaced() || (m_scrollableArea && m_scrollableArea->hasOverflowControls())) {
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

void RenderLayer::styleChanged(StyleDifference diff, const RenderStyle* oldStyle)
{
    setIsNormalFlowOnly(shouldBeNormalFlowOnly());

    if (setIsCSSStackingContext(shouldBeCSSStackingContext())) {
#if ENABLE(CSS_COMPOSITING)
        if (parent()) {
            if (isCSSStackingContext()) {
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
    }

    updateLayerScrollableArea();

    // FIXME: RenderLayer already handles visibility changes through our visibility dirty bits. This logic could
    // likely be folded along with the rest.
    if (oldStyle) {
        bool visibilityChanged = oldStyle->visibility() != renderer().style().visibility();
        if (oldStyle->usedZIndex() != renderer().style().usedZIndex() || visibilityChanged) {
            dirtyStackingContextZOrderLists();
            if (isStackingContext())
                dirtyZOrderLists();
        }

        // Visibility is input to canUseCompositedScrolling().
        if (visibilityChanged && m_scrollableArea)
            m_scrollableArea->computeHasCompositedScrollableOverflow();
    }

    if (m_scrollableArea) {
        m_scrollableArea->createOrDestroyMarquee();
        m_scrollableArea->updateScrollbarsAfterStyleChange(oldStyle);
    }
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
    if (m_scrollableArea)
        m_scrollableArea->updateAllScrollbarRelatedStyle();

    updateDescendantDependentFlags();
    updateTransform();
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode();
#endif
    updateFiltersAfterStyleChange();
    
    compositor().layerStyleChanged(diff, *this, oldStyle);

    updateFilterPaintingStrategy();

#if PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_EVENTS)
    if (diff == StyleDifference::RecompositeLayer || diff >= StyleDifference::LayoutPositionedMovementOnly)
        renderer().document().invalidateRenderingDependentRegions();
#else
    UNUSED_PARAM(diff);
#endif
}

RenderLayer* RenderLayer::reflectionLayer() const
{
    return m_reflection ? m_reflection->layer() : nullptr;
}

bool RenderLayer::isReflectionLayer(const RenderLayer& layer) const
{
    return m_reflection ? &layer == m_reflection->layer() : false;
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
    if (!m_reflection->renderTreeBeingDestroyed()) {
        if (auto* layer = m_reflection->layer())
            removeChild(*layer);
    }

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
    case ReflectionDirection::Below:
        transform.operations().append(TranslateTransformOperation::create(Length(0, LengthType::Fixed), Length(100., LengthType::Percent), TransformOperation::TRANSLATE));
        transform.operations().append(TranslateTransformOperation::create(Length(0, LengthType::Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
        transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
        break;
    case ReflectionDirection::Above:
        transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
        transform.operations().append(TranslateTransformOperation::create(Length(0, LengthType::Fixed), Length(100., LengthType::Percent), TransformOperation::TRANSLATE));
        transform.operations().append(TranslateTransformOperation::create(Length(0, LengthType::Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
        break;
    case ReflectionDirection::Right:
        transform.operations().append(TranslateTransformOperation::create(Length(100., LengthType::Percent), Length(0, LengthType::Fixed), TransformOperation::TRANSLATE));
        transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, LengthType::Fixed), TransformOperation::TRANSLATE));
        transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
        break;
    case ReflectionDirection::Left:
        transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
        transform.operations().append(TranslateTransformOperation::create(Length(100., LengthType::Percent), Length(0, LengthType::Fixed), TransformOperation::TRANSLATE));
        transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, LengthType::Fixed), TransformOperation::TRANSLATE));
        break;
    }
    newStyle.setTransform(transform);

    // Map in our mask.
    newStyle.setMaskBoxImage(renderer().style().boxReflect()->mask());

    // Style has transform and mask, so needs to be stacking context.
    newStyle.setUsedZIndex(0);

    return newStyle;
}

void RenderLayer::ensureLayerFilters()
{
    if (m_filters)
        return;
    
    m_filters = makeUnique<RenderLayerFilters>(*this);
}

void RenderLayer::clearLayerFilters()
{
    m_filters = nullptr;
}

RenderLayerScrollableArea* RenderLayer::ensureLayerScrollableArea()
{
    bool hadScrollableArea = scrollableArea();
    
    if (!m_scrollableArea)
        m_scrollableArea = makeUnique<RenderLayerScrollableArea>(*this);

    if (!hadScrollableArea) {
        if (renderer().settings().asyncOverflowScrollingEnabled())
            setNeedsCompositingConfigurationUpdate();

        m_scrollableArea->restoreScrollPosition();
    }

    return m_scrollableArea.get();
}

void RenderLayer::clearLayerScrollableArea()
{
    if (m_scrollableArea) {
        m_scrollableArea->clear();
        m_scrollableArea = nullptr;
    }
}

void RenderLayer::updateFiltersAfterStyleChange()
{
    if (!hasFilter()) {
        clearLayerFilters();
        return;
    }

    // Add the filter as a client to this renderer, unless we are a RenderLayer accommodating
    // an SVG. In that case it takes care of its own resource management for filters.
    if (renderer().style().filter().hasReferenceFilter() && !renderer().isSVGRootOrLegacySVGRoot()) {
        ensureLayerFilters();
        m_filters->updateReferenceFilterClients(renderer().style().filter());
    } else if (m_filters)
        m_filters->removeReferenceFilterClients();
}

void RenderLayer::updateLayerScrollableArea()
{
    bool hasScrollableArea = scrollableArea();
    bool needsScrollableArea = is<RenderBox>(renderer()) && downcast<RenderBox>(renderer()).requiresLayerWithScrollableArea();

    if (needsScrollableArea == hasScrollableArea)
        return;

    if (needsScrollableArea)
        ensureLayerScrollableArea();
    else {
        clearLayerScrollableArea();
        if (renderer().settings().asyncOverflowScrollingEnabled())
            setNeedsCompositingConfigurationUpdate();
    }

    InspectorInstrumentation::didAddOrRemoveScrollbars(m_renderer);
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
            m_filters->clearFilter();

        // Early-return only if we *don't* have reference filters.
        // For reference filters, we still want the FilterEffect graph built
        // for us, even if we're composited.
        if (!renderer().style().filter().hasReferenceFilter())
            return;
    }
    
    ensureLayerFilters();
    m_filters->setRenderingMode(renderer().page().acceleratedFiltersEnabled() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated);
    m_filters->setFilterScale({ page().deviceScaleFactor(), page().deviceScaleFactor() });
}

IntOutsets RenderLayer::filterOutsets() const
{
    if (m_filters)
        return m_filters->calculateOutsets(renderer(), localBoundingBox());
    return renderer().style().filterOutsets();
}

static RenderLayer* parentLayerCrossFrame(const RenderLayer& layer)
{
    if (auto* parent = layer.parent())
        return parent;

    return layer.enclosingFrameRenderLayer();
}

bool RenderLayer::isTransparentRespectingParentFrames() const
{
    static const double minimumVisibleOpacity = 0.01;

    float currentOpacity = 1;
    for (auto* layer = this; layer; layer = parentLayerCrossFrame(*layer)) {
        currentOpacity *= layer->renderer().style().opacity();
        if (currentOpacity < minimumVisibleOpacity)
            return true;
    }

    return false;
}

void RenderLayer::simulateFrequentPaint()
{
    SinglePaintFrequencyTracking { m_paintFrequencyTracker, page().lastRenderingUpdateTimestamp() };
}

RenderLayerScrollableArea* RenderLayer::scrollableArea() const
{
    return m_scrollableArea.get();
}

#if ENABLE(ASYNC_SCROLLING) && !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, RenderLayer::EventRegionInvalidationReason reason)
{
    switch (reason) {
    case RenderLayer::EventRegionInvalidationReason::Paint: ts << "Paint"; break;
    case RenderLayer::EventRegionInvalidationReason::SettingDidChange: ts << "SettingDidChange"; break;
    case RenderLayer::EventRegionInvalidationReason::Style: ts << "Style"; break;
    case RenderLayer::EventRegionInvalidationReason::NonCompositedFrame: ts << "NonCompositedFrame"; break;
    }
    return ts;
}
#endif // !LOG_DISABLED

bool RenderLayer::invalidateEventRegion(EventRegionInvalidationReason reason)
{
#if ENABLE(ASYNC_SCROLLING)
    auto* compositingLayer = enclosingCompositingLayerForRepaint().layer;

    auto shouldInvalidate = [&] {
        if (!compositingLayer)
            return false;

        if (reason == EventRegionInvalidationReason::NonCompositedFrame)
            return true;

        return compositingLayer->backing()->maintainsEventRegion();
    };

    if (!shouldInvalidate())
        return false;

    LOG_WITH_STREAM(EventRegions, stream << this << " invalidateEventRegion for reason " << reason << " invalidating in compositing layer " << compositingLayer);

    compositingLayer->backing()->setNeedsEventRegionUpdate();

    if (reason == EventRegionInvalidationReason::NonCompositedFrame) {
        auto& view = renderer().view();
        LOG_WITH_STREAM(EventRegions, stream << " calling setNeedsEventRegionUpdateForNonCompositedFrame on " << view);
        view.setNeedsEventRegionUpdateForNonCompositedFrame();
        auto visibleDebugOverlayRegions = OptionSet<DebugOverlayRegions>::fromRaw(renderer().settings().visibleDebugOverlayRegions());
        if (visibleDebugOverlayRegions.containsAny({ DebugOverlayRegions::TouchActionRegion, DebugOverlayRegions::EditableElementRegion, DebugOverlayRegions::WheelEventHandlerRegion }))
            view.setNeedsRepaintHackAfterCompositingLayerUpdateForDebugOverlaysOnly();
        view.compositor().scheduleCompositingLayerUpdate();
    }
    return true;
#else
    UNUSED_PARAM(reason);
    return false;
#endif
}

TextStream& operator<<(WTF::TextStream& ts, ClipRectsType clipRectsType)
{
    switch (clipRectsType) {
    case PaintingClipRects: ts << "painting"; break;
    case RootRelativeClipRects: ts << "root-relative"; break;
    case AbsoluteClipRects: ts << "absolute"; break;
    case TemporaryClipRects: ts << "temporary"; break;
    case NumCachedClipRectsTypes:
    case AllClipRectTypes:
        ts << "?";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const RenderLayer& layer)
{
    ts << layer.debugDescription();
    return ts;
}

TextStream& operator<<(TextStream& ts, const RenderLayer::ClipRectsContext& context)
{
    ts.dumpProperty("root layer:", context.rootLayer);
    ts.dumpProperty("type:", context.clipRectsType);
    ts.dumpProperty("overflow-clip:", context.respectOverflowClip() ? "respect" : "ignore");
    
    return ts;
}

TextStream& operator<<(TextStream& ts, IndirectCompositingReason reason)
{
    switch (reason) {
    case IndirectCompositingReason::None: ts << "none"; break;
    case IndirectCompositingReason::Clipping: ts << "clipping"; break;
    case IndirectCompositingReason::Stacking: ts << "stacking"; break;
    case IndirectCompositingReason::OverflowScrollPositioning: ts << "overflow positioning"; break;
    case IndirectCompositingReason::Overlap: ts << "overlap"; break;
    case IndirectCompositingReason::BackgroundLayer: ts << "background layer"; break;
    case IndirectCompositingReason::GraphicalEffect: ts << "graphical effect"; break;
    case IndirectCompositingReason::Perspective: ts << "perspective"; break;
    case IndirectCompositingReason::Preserve3D: ts << "preserve-3d"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, PaintBehavior behavior)
{
    switch (behavior) {
    case PaintBehavior::Normal: ts << "Normal"; break;
    case PaintBehavior::SelectionOnly: ts << "SelectionOnly"; break;
    case PaintBehavior::SkipSelectionHighlight: ts << "SkipSelectionHighlight"; break;
    case PaintBehavior::ForceBlackText: ts << "ForceBlackText"; break;
    case PaintBehavior::ForceWhiteText: ts << "ForceWhiteText"; break;
    case PaintBehavior::RenderingSVGMask: ts << "RenderingSVGMask"; break;
    case PaintBehavior::SkipRootBackground: ts << "SkipRootBackground"; break;
    case PaintBehavior::RootBackgroundOnly: ts << "RootBackgroundOnly"; break;
    case PaintBehavior::SelectionAndBackgroundsOnly: ts << "SelectionAndBackgroundsOnly"; break;
    case PaintBehavior::ExcludeSelection: ts << "ExcludeSelection"; break;
    case PaintBehavior::FlattenCompositingLayers: ts << "FlattenCompositingLayers"; break;
    case PaintBehavior::Snapshotting: ts << "Snapshotting"; break;
    case PaintBehavior::TileFirstPaint: ts << "TileFirstPaint"; break;
    case PaintBehavior::CompositedOverflowScrollContent: ts << "CompositedOverflowScrollContent"; break;
    case PaintBehavior::AnnotateLinks: ts << "AnnotateLinks"; break;
    case PaintBehavior::EventRegionIncludeForeground: ts << "EventRegionIncludeForeground"; break;
    case PaintBehavior::EventRegionIncludeBackground: ts << "EventRegionIncludeBackground"; break;
    }

    return ts;
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    String output = externalRepresentation(&layer->renderer().frame(), {
        WebCore::RenderAsTextFlag::ShowAllLayers,
        WebCore::RenderAsTextFlag::ShowLayerNesting,
        WebCore::RenderAsTextFlag::ShowCompositedLayers,
        WebCore::RenderAsTextFlag::ShowOverflow,
        WebCore::RenderAsTextFlag::ShowSVGGeometry,
        WebCore::RenderAsTextFlag::ShowLayerFragments,
        WebCore::RenderAsTextFlag::ShowAddresses,
        WebCore::RenderAsTextFlag::ShowIDAndClass,
        WebCore::RenderAsTextFlag::DontUpdateLayout,
        WebCore::RenderAsTextFlag::ShowLayoutState,
    });
    fprintf(stderr, "\n%s\n", output.utf8().data());
}

void showLayerTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}

static void outputPaintOrderTreeLegend(TextStream& stream)
{
    stream.nextLine();
    stream << "(S)tacking Context/(F)orced SC/O(P)portunistic SC, (N)ormal flow only, (O)verflow clip, (A)lpha (opacity or mask), has (B)lend mode, (I)solates blending, (T)ransform-ish, (F)ilter, Fi(X)ed position, Behaves as fi(x)ed, (C)omposited, (P)rovides backing/uses (p)rovided backing/paints to (a)ncestor, (c)omposited descendant, (s)scrolling ancestor, (t)transformed ancestor\n"
        "Dirty (z)-lists, Dirty (n)ormal flow lists\n"
        "Traversal needs: requirements (t)raversal on descendants, (b)acking or hierarchy traversal on descendants, (r)equirements traversal on all descendants, requirements traversal on all (s)ubsequent layers, (h)ierarchy traversal on all descendants, update of paint (o)rder children\n"
        "Update needs:    post-(l)ayout requirements, (g)eometry, (k)ids geometry, (c)onfig, layer conne(x)ion, (s)crolling tree\n"
        "Scrolling scope: box contents\n";
    stream.nextLine();
}

static void outputIdent(TextStream& stream, unsigned depth)
{
    unsigned i = 0;
    while (++i <= depth * 2)
        stream << " ";
}

static void outputPaintOrderTreeRecursive(TextStream& stream, const WebCore::RenderLayer& layer, const char* prefix, unsigned depth = 0)
{
    stream << (layer.isCSSStackingContext() ? "S" : (layer.isForcedStackingContext() ? "F" : (layer.isOpportunisticStackingContext() ? "P" : "-")));
    stream << (layer.isNormalFlowOnly() ? "N" : "-");
    stream << (layer.renderer().hasNonVisibleOverflow() ? "O" : "-");
    stream << (layer.isTransparent() ? "A" : "-");
    stream << (layer.hasBlendMode() ? "B" : "-");
    stream << (layer.isolatesBlending() ? "I" : "-");
    stream << (layer.renderer().hasTransformRelatedProperty() ? "T" : "-");
    stream << (layer.hasFilter() ? "F" : "-");
    stream << (layer.renderer().isFixedPositioned() ? "X" : "-");
    stream << (layer.behavesAsFixed() ? "x" : "-");
    stream << (layer.isComposited() ? "C" : "-");
    
    auto compositedPaintingDestinationString = [&layer]() {
        if (layer.paintsIntoProvidedBacking())
            return "p";

        if (!layer.isComposited())
            return "-";

        if (layer.backing()->hasBackingSharingLayers())
            return "P";
        
        if (layer.backing()->paintsIntoCompositedAncestor())
            return "a";

        return "-";
    };

    stream << compositedPaintingDestinationString();
    stream << (layer.hasCompositingDescendant() ? "c" : "-");
    stream << (layer.hasCompositedScrollingAncestor() ? "s" : "-");
    stream << (layer.hasTransformedAncestor() ? "t" : "-");

    stream << " ";

    stream << (layer.zOrderListsDirty() ? "z" : "-");
    stream << (layer.normalFlowListDirty() ? "n" : "-");

    stream << " ";

    stream << (layer.hasDescendantNeedingCompositingRequirementsTraversal() ? "t" : "-");
    stream << (layer.hasDescendantNeedingUpdateBackingOrHierarchyTraversal() ? "b" : "-");
    stream << (layer.descendantsNeedCompositingRequirementsTraversal() ? "r" : "-");
    stream << (layer.subsequentLayersNeedCompositingRequirementsTraversal() ? "s" : "-");
    stream << (layer.descendantsNeedUpdateBackingAndHierarchyTraversal() ? "h" : "-");
    stream << (layer.needsCompositingPaintOrderChildrenUpdate() ? "o" : "-");

    stream << " ";

    stream << (layer.needsPostLayoutCompositingUpdate() ? "l" : "-");
    stream << (layer.needsCompositingGeometryUpdate() ? "g" : "-");
    stream << (layer.childrenNeedCompositingGeometryUpdate() ? "k" : "-");
    stream << (layer.needsCompositingConfigurationUpdate() ? "c" : "-");
    stream << (layer.needsCompositingLayerConnection() ? "x" : "-");
    stream << (layer.needsScrollingTreeUpdate() ? "s" : "-");

    stream << " ";

    stream << layer.boxScrollingScope();
    stream << " ";
    stream << layer.contentsScrollingScope();

    stream << " ";

    outputIdent(stream, depth);

    stream << prefix;

    auto layerRect = layer.rect();

    stream << &layer << " " << layerRect;

    if (auto* scrollableArea = layer.scrollableArea())
        stream << " [SA " << scrollableArea << "]";

    if (layer.isComposited()) {
        auto& backing = *layer.backing();
        stream << " (layerID " << backing.graphicsLayer()->primaryLayerID() << ")";
        
        if (layer.indirectCompositingReason() != WebCore::IndirectCompositingReason::None)
            stream << " " << layer.indirectCompositingReason();

        auto scrollingNodeID = backing.scrollingNodeIDForRole(WebCore::ScrollCoordinationRole::Scrolling);
        auto frameHostingNodeID = backing.scrollingNodeIDForRole(WebCore::ScrollCoordinationRole::FrameHosting);
        auto viewportConstrainedNodeID = backing.scrollingNodeIDForRole(WebCore::ScrollCoordinationRole::ViewportConstrained);
        auto positionedNodeID = backing.scrollingNodeIDForRole(WebCore::ScrollCoordinationRole::Positioning);

        if (scrollingNodeID || frameHostingNodeID || viewportConstrainedNodeID || positionedNodeID) {
            stream << " {";
            bool first = true;
            if (scrollingNodeID) {
                stream << "sc " << scrollingNodeID;
                first = false;
            }

            if (frameHostingNodeID) {
                if (!first)
                    stream << ", ";
                stream << "fh " << frameHostingNodeID;
                first = false;
            }

            if (viewportConstrainedNodeID) {
                if (!first)
                    stream << ", ";
                stream << "vc " << viewportConstrainedNodeID;
                first = false;
            }

            if (positionedNodeID) {
                if (!first)
                    stream << ", ";
                stream << "pos " << positionedNodeID;
            }

            stream << "}";
        }
    }
    stream << " " << layer.name();
    stream.nextLine();

    const_cast<WebCore::RenderLayer&>(layer).updateLayerListsIfNeeded();

    for (auto* child : layer.negativeZOrderLayers())
        outputPaintOrderTreeRecursive(stream, *child, "- ", depth + 1);

    for (auto* child : layer.normalFlowLayers())
        outputPaintOrderTreeRecursive(stream, *child, "n ", depth + 1);

    for (auto* child : layer.positiveZOrderLayers())
        outputPaintOrderTreeRecursive(stream, *child, "+ ", depth + 1);
}

void showPaintOrderTree(const WebCore::RenderLayer* layer)
{
    TextStream stream;
    outputPaintOrderTreeLegend(stream);
    if (layer)
        outputPaintOrderTreeRecursive(stream, *layer, "");
    
    WTFLogAlways("%s", stream.release().utf8().data());
}

void showPaintOrderTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showPaintOrderTree(renderer->enclosingLayer());
}

#endif
