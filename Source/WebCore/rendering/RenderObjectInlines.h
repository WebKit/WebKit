/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/DocumentPage.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/RenderElement.h>
#include <WebCore/RenderIFrame.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderObjectDocument.h>
#include <WebCore/RenderObjectNode.h>
#include <WebCore/RenderObjectStyle.h>
#include <WebCore/RenderReplaced.h>
#include <WebCore/RenderStyle+GettersInlines.h>
#include <WebCore/RenderView.h>
#include <WebCore/VisibleRectContext.h>

namespace WebCore {

inline bool RenderObject::hasTransformOrPerspective() const
{
    return hasTransformRelatedProperty() && (isTransformed() || !style().perspective().isNone());
}

inline bool RenderObject::isAtomicInlineLevelBox() const
{
    auto display = style().display();
    return display.isInlineType()
        && !(display == Style::DisplayType::InlineFlow && !isBlockLevelReplacedOrAtomicInline());
}

inline bool RenderObject::isTransformed() const
{
    return hasTransformRelatedProperty() && (style().affectsTransform() || hasSVGTransform());
}

inline LocalFrameViewLayoutContext& RenderObject::layoutContext() const
{
    return view().frameView().layoutContext();
}

inline TreeScope& RenderObject::treeScopeForSVGReferences() const
{
    return Ref { m_node.get() }->treeScopeForSVGReferences();
}

inline const RenderStyle& RenderObject::firstLineStyle() const
{
    if (isRenderText())
        return protect(parent())->firstLineStyle();
    return downcast<RenderElement>(*this).firstLineStyle();
}

inline LocalFrame& RenderObject::frame() const
{
    return *document().frame();
}

inline Page& RenderObject::page() const
{
    // The render tree will always be torn down before Frame is disconnected from Page,
    // so it's safe to assume Frame::page() is non-null as long as there are live RenderObjects.
    ASSERT(frame().page());
    return *frame().page();
}

inline FloatQuad RenderObject::localToAbsoluteQuad(const FloatQuad& quad, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    return localToContainerQuad(quad, nullptr, mode, wasFixed);
}

inline void RenderObject::setNeedsLayout(MarkingBehavior markParents)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (selfNeedsLayout())
        return;
    m_stateBitfields.setFlag(StateFlag::NeedsLayout);
    if (markParents == MarkContainingBlockChain)
        scheduleLayout(CheckedPtr { markContainingBlocksForLayout() });
    if (hasLayer())
        setLayerNeedsFullRepaint();
}

inline void RenderObject::setNeedsLayoutAndPreferredWidthsUpdate()
{
    setNeedsLayout();
    setNeedsPreferredWidthsUpdate();
}

inline bool RenderObject::isNonReplacedAtomicInlineLevelBox() const
{
    // FIXME: Check if iframe should really behave like non-replaced here.
    return (is<RenderIFrame>(*this) && isInline()) || (!is<RenderReplaced>(*this) && isAtomicInlineLevelBox());
}

inline auto RenderObject::visibleRectContextForRepaint() -> VisibleRectContext
{
    return {
        .hasPositionFixedDescendant = false,
        .dirtyRectIsFlipped = false,
        .descendantNeedsEnclosingIntRect = false,
        .options = {
            VisibleRectContext::Option::ApplyContainerClip,
            VisibleRectContext::Option::ApplyCompositedContainerScrolls
        },
        .scrollMargin = { },
    };
}

inline auto RenderObject::visibleRectContextForSpatialNavigation() -> VisibleRectContext
{
    return {
        .hasPositionFixedDescendant = false,
        .dirtyRectIsFlipped = false,
        .descendantNeedsEnclosingIntRect = false,
        .options = {
            VisibleRectContext::Option::ApplyContainerClip,
            VisibleRectContext::Option::ApplyCompositedContainerScrolls,
            VisibleRectContext::Option::ApplyCompositedClips
        },
        .scrollMargin = { },
    };
}

inline auto RenderObject::visibleRectContextForRenderTreeAsText() -> VisibleRectContext
{
    return {
        .hasPositionFixedDescendant = false,
        .dirtyRectIsFlipped = false,
        .descendantNeedsEnclosingIntRect = false,
        .options = {
            VisibleRectContext::Option::ApplyContainerClip,
            VisibleRectContext::Option::ApplyCompositedContainerScrolls,
            VisibleRectContext::Option::ApplyCompositedClips,
            VisibleRectContext::Option::CalculateAccurateRepaintRect
        },
        .scrollMargin = { },
    };
}

inline LayoutRect RenderObject::absoluteClippedOverflowRectForRepaint() const
{
    return clippedOverflowRect(nullptr, visibleRectContextForRepaint());
}

inline LayoutRect RenderObject::absoluteClippedOverflowRectForSpatialNavigation() const
{
    return clippedOverflowRect(nullptr, visibleRectContextForSpatialNavigation());
}

inline LayoutRect RenderObject::absoluteClippedOverflowRectForRenderTreeAsText() const
{
    return clippedOverflowRect(nullptr, visibleRectContextForRenderTreeAsText());
}

inline LayoutRect RenderObject::clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const
{
    return clippedOverflowRect(repaintContainer, visibleRectContextForRepaint());
}

inline LayoutRect RenderObject::computeRectForRepaint(const LayoutRect& rect, const RenderLayerModelObject* repaintContainer) const
{
    return computeRects({ rect }, repaintContainer, visibleRectContextForRepaint()).clippedOverflowRect;
}

} // namespace WebCore
