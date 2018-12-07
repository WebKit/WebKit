/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006, 2015-2016 Apple Inc.
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
 *
 */

#pragma once

#include "FrameView.h"
#include "Region.h"
#include "RenderBlockFlow.h"
#include "RenderWidget.h"
#include "SelectionRangeData.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>

namespace WebCore {

class ImageQualityController;
class RenderLayerCompositor;
class RenderLayoutState;
class RenderQuote;

class RenderView final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderView);
public:
    RenderView(Document&, RenderStyle&&);
    virtual ~RenderView();

    WEBCORE_EXPORT bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    const char* renderName() const override { return "RenderView"; }

    bool requiresLayer() const override { return true; }

    bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    void layout() override;
    void updateLogicalWidth() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const override;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight() const;
    int viewWidth() const;
    int viewLogicalWidth() const { return style().isHorizontalWritingMode() ? viewWidth() : viewHeight(); }
    int viewLogicalHeight() const;

    LayoutUnit clientLogicalWidthForFixedPosition() const;
    LayoutUnit clientLogicalHeightForFixedPosition() const;

    float zoomFactor() const;

    FrameView& frameView() const { return m_frameView; }

    LayoutRect visualOverflowRect() const override;
    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const override;
    void repaintRootContents();
    void repaintViewRectangle(const LayoutRect&) const;
    void repaintViewAndCompositedLayers();

    void paint(PaintInfo&, const LayoutPoint&) override;
    void paintBoxDecorations(PaintInfo&, const LayoutPoint&) override;
    // Return the renderer whose background style is used to paint the root background.
    RenderElement* rendererForRootBackground() const;

    SelectionRangeData& selection() { return m_selection; }

    bool printing() const;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    LayoutRect viewRect() const;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    void setPageLogicalSize(LayoutSize);
    LayoutUnit pageOrViewLogicalHeight() const;

    // This method is used to assign a page number only when pagination modes have
    // a block progression. This happens with vertical-rl books for example, but it
    // doesn't happen for normal horizontal-tb books. This is a very specialized
    // function and should not be mistaken for a general page number API.
    unsigned pageNumberForBlockProgressionOffset(int offset) const;

    unsigned pageCount() const;

    // FIXME: These functions are deprecated. No code should be added that uses these.
    int bestTruncatedAt() const { return m_legacyPrinting.m_bestTruncatedAt; }
    void setBestTruncatedAt(int y, RenderBoxModelObject* forRenderer, bool forcedBreak = false);
    int truncatedAt() const { return m_legacyPrinting.m_truncatedAt; }
    void setTruncatedAt(int y)
    { 
        m_legacyPrinting.m_truncatedAt = y;
        m_legacyPrinting.m_bestTruncatedAt = 0;
        m_legacyPrinting.m_truncatorWidth = 0;
        m_legacyPrinting.m_forcedPageBreak = false;
    }
    const IntRect& printRect() const { return m_legacyPrinting.m_printRect; }
    void setPrintRect(const IntRect& r) { m_legacyPrinting.m_printRect = r; }
    // End deprecated functions.

    // Notification that this view moved into or out of a native window.
    void setIsInWindow(bool);

    WEBCORE_EXPORT RenderLayerCompositor& compositor();
    WEBCORE_EXPORT bool usesCompositing() const;

    bool usesFirstLineRules() const { return m_usesFirstLineRules; }
    bool usesFirstLetterRules() const { return m_usesFirstLetterRules; }
    void setUsesFirstLineRules(bool value) { m_usesFirstLineRules = value; }
    void setUsesFirstLetterRules(bool value) { m_usesFirstLetterRules = value; }

    WEBCORE_EXPORT IntRect unscaledDocumentRect() const;
    LayoutRect unextendedBackgroundRect() const;
    LayoutRect backgroundRect() const;

    WEBCORE_EXPORT IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    IntSize viewportSizeForCSSViewportUnits() const;

    bool hasQuotesNeedingUpdate() const { return m_hasQuotesNeedingUpdate; }
    void setHasQuotesNeedingUpdate(bool b) { m_hasQuotesNeedingUpdate = b; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter code.
    void addRenderCounter() { m_renderCounterCount++; }
    void removeRenderCounter() { ASSERT(m_renderCounterCount > 0); m_renderCounterCount--; }
    bool hasRenderCounters() { return m_renderCounterCount; }
    
    ImageQualityController& imageQualityController();

    void setHasSoftwareFilters(bool hasSoftwareFilters) { m_hasSoftwareFilters = hasSoftwareFilters; }
    bool hasSoftwareFilters() const { return m_hasSoftwareFilters; }

    uint64_t rendererCount() const { return m_rendererCount; }
    void didCreateRenderer() { ++m_rendererCount; }
    void didDestroyRenderer() { --m_rendererCount; }

    void updateVisibleViewportRect(const IntRect&);
    void registerForVisibleInViewportCallback(RenderElement&);
    void unregisterForVisibleInViewportCallback(RenderElement&);
    void resumePausedImageAnimationsIfNeeded(IntRect visibleRect);
    void addRendererWithPausedImageAnimations(RenderElement&, CachedImage&);
    void removeRendererWithPausedImageAnimations(RenderElement&);
    void removeRendererWithPausedImageAnimations(RenderElement&, CachedImage&);

    class RepaintRegionAccumulator {
        WTF_MAKE_NONCOPYABLE(RepaintRegionAccumulator);
    public:
        RepaintRegionAccumulator(RenderView*);
        ~RepaintRegionAccumulator();

    private:
        WeakPtr<RenderView> m_rootView;
        bool m_wasAccumulatingRepaintRegion;
    };

    void scheduleLazyRepaint(RenderBox&);
    void unscheduleLazyRepaint(RenderBox&);

    void protectRenderWidgetUntilLayoutIsDone(RenderWidget& widget) { m_protectedRenderWidgets.append(&widget); }
    void releaseProtectedRenderWidgets() { m_protectedRenderWidgets.clear(); }

#if ENABLE(CSS_SCROLL_SNAP)
    void registerBoxWithScrollSnapPositions(const RenderBox&);
    void unregisterBoxWithScrollSnapPositions(const RenderBox&);
    const HashSet<const RenderBox*>& boxesWithScrollSnapPositions() { return m_boxesWithScrollSnapPositions; }
#endif

#if !ASSERT_DISABLED
    bool inHitTesting() const { return m_inHitTesting; }
#endif

protected:
    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;
    void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const override;
    bool requiresColumns(int desiredColumnCount) const override;

private:
    void computeColumnCountAndWidth() override;

    bool shouldRepaint(const LayoutRect&) const;
    void flushAccumulatedRepaintRegion() const;

    void layoutContent(const RenderLayoutState&);

    bool isScrollableOrRubberbandableBox() const override;

private:
    FrameView& m_frameView;

    // Include this RenderView.
    uint64_t m_rendererCount { 1 };

    mutable std::unique_ptr<Region> m_accumulatedRepaintRegion;
    SelectionRangeData m_selection;

    // FIXME: Only used by embedded WebViews inside AppKit NSViews.  Find a way to remove.
    struct LegacyPrinting {
        int m_bestTruncatedAt { 0 };
        int m_truncatedAt { 0 };
        int m_truncatorWidth { 0 };
        IntRect m_printRect;
        bool m_forcedPageBreak { false };
    };
    LegacyPrinting m_legacyPrinting;
    // End deprecated members.

    bool shouldUsePrintingLayout() const;

    void lazyRepaintTimerFired();

    Timer m_lazyRepaintTimer;
    HashSet<RenderBox*> m_renderersNeedingLazyRepaint;

    std::unique_ptr<ImageQualityController> m_imageQualityController;
    std::optional<LayoutSize> m_pageLogicalSize;
    bool m_pageLogicalHeightChanged { false };
    std::unique_ptr<RenderLayerCompositor> m_compositor;

    bool m_hasQuotesNeedingUpdate { false };

    unsigned m_renderCounterCount { 0 };

    bool m_hasSoftwareFilters { false };
    bool m_usesFirstLineRules { false };
    bool m_usesFirstLetterRules { false };
#if !ASSERT_DISABLED
    bool m_inHitTesting { false };
#endif

    HashMap<RenderElement*, Vector<CachedImage*>> m_renderersWithPausedImageAnimation;
    HashSet<RenderElement*> m_visibleInViewportRenderers;
    Vector<RefPtr<RenderWidget>> m_protectedRenderWidgets;

#if ENABLE(CSS_SCROLL_SNAP)
    HashSet<const RenderBox*> m_boxesWithScrollSnapPositions;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderView, isRenderView())
