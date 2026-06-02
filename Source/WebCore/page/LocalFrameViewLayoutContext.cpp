/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalFrameViewLayoutContext.h"

#include "DebugPageOverlays.h"
#include "Document.h"
#include "DocumentEnums.h"
#include "DocumentQuirks.h"
#include "FrameInlines.h"
#include "InspectorInstrumentation.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContext.h"
#include "LayoutDisallowedScope.h"
#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutState.h"
#include "LayoutTreeBuilder.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "Quirks.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderElementInlines.h"
#include "RenderLayerCompositor.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SVGTextFragment.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleScope.h"
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalFrameViewLayoutContext);

UpdateScrollInfoAfterLayoutTransaction::UpdateScrollInfoAfterLayoutTransaction() = default;
UpdateScrollInfoAfterLayoutTransaction::~UpdateScrollInfoAfterLayoutTransaction() = default;

#ifndef NDEBUG
class RenderTreeNeedsLayoutChecker {
    WTF_MAKE_FAST_ALLOCATED;
public :
    RenderTreeNeedsLayoutChecker(const RenderView& renderView)
        : m_renderView(renderView)
    {
    }

    ~RenderTreeNeedsLayoutChecker()
    {
        auto checkAndReportNeedsLayoutError = [] (const RenderObject& renderer) {
            auto needsLayout = [&] {
                if (renderer.needsLayout())
                    return true;
                if (auto* renderBlockFlow = dynamicDowncast<RenderBlockFlow>(renderer); renderBlockFlow && renderBlockFlow->inlineLayout())
                    return renderBlockFlow->inlineLayout()->hasDetachedContent();
                return false;
            };

            if (needsLayout()) {
                WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "post-layout: dirty renderer(s)");
                renderer.showRenderTreeForThis();
                ASSERT_NOT_REACHED();
            }
        };

        checkAndReportNeedsLayoutError(m_renderView);

        for (auto* descendant = m_renderView.firstChild(); descendant; descendant = descendant->nextInPreOrder(&m_renderView))
            checkAndReportNeedsLayoutError(*descendant);
    }

private:
    const RenderView& m_renderView;
};
#endif

RepaintBlocker::RepaintBlocker(Document& document)
    : m_document(document)
{
    if (auto* view = m_document->view())
        view->layoutContext().blockRepaints();
}

RepaintBlocker::~RepaintBlocker()
{
    if (auto* view = m_document->view())
        view->layoutContext().allowRepaints();
}

class LayoutFrameScope {
public:
    LayoutFrameScope(LocalFrameViewLayoutContext& layoutContext)
        : m_view(layoutContext.view())
        , m_nestedState(layoutContext.m_layoutNestedState, layoutContext.m_layoutNestedState == LocalFrameViewLayoutContext::LayoutNestedState::NotInLayout ? LocalFrameViewLayoutContext::LayoutNestedState::NotNested : LocalFrameViewLayoutContext::LayoutNestedState::Nested)
        , m_schedulingIsEnabled(layoutContext.m_layoutSchedulingIsEnabled, false)
        , m_previousScrollType(layoutContext.view().currentScrollType())
    {
        m_view->setCurrentScrollType(ScrollType::Programmatic);
    }
        
    ~LayoutFrameScope()
    {
        m_view->setCurrentScrollType(m_previousScrollType);
    }
        
private:
    const Ref<LocalFrameView> m_view;
    SetForScope<LocalFrameViewLayoutContext::LayoutNestedState> m_nestedState;
    SetForScope<bool> m_schedulingIsEnabled;
    ScrollType m_previousScrollType;
};

LocalFrameViewLayoutContext::LocalFrameViewLayoutContext(LocalFrameView& frameView)
    : m_frameView(frameView)
    , m_layoutTimer(*this, &LocalFrameViewLayoutContext::layoutTimerFired)
    , m_postLayoutTaskTimer(*this, &LocalFrameViewLayoutContext::runPostLayoutTasks)
{
}

LocalFrameViewLayoutContext::~LocalFrameViewLayoutContext() = default;

void LocalFrameViewLayoutContext::setSubtreeScrollbarChangesState(std::optional<SubtreeScrollbarChangesState> state)
{
    m_subtreeScrollbarChangesState = state;
}

UpdateScrollInfoAfterLayoutTransaction& LocalFrameViewLayoutContext::updateScrollInfoAfterLayoutTransaction()
{
    if (!m_updateScrollInfoAfterLayoutTransaction)
        m_updateScrollInfoAfterLayoutTransaction = makeUnique<UpdateScrollInfoAfterLayoutTransaction>();
    return *m_updateScrollInfoAfterLayoutTransaction;
}

void LocalFrameViewLayoutContext::layout(bool canDeferUpdateLayerPositions)
{
    LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << &view() << " LocalFrameViewLayoutContext::layout() with size " << view().layoutSize());

    Ref protectedView(view());

    performLayouts(canDeferUpdateLayerPositions);

    if (view().hasOneRef())
        return;

    Style::Scope::LayoutDependencyUpdateContext layoutDependencyUpdateContext;
    while (document() && document()->styleScope().invalidateForLayoutDependencies(layoutDependencyUpdateContext)) {
        protect(document())->updateStyleIfNeeded();

        if (!needsLayout())
            break;

        performLayouts(canDeferUpdateLayerPositions);

        if (view().hasOneRef())
            return;
    }
}

void LocalFrameViewLayoutContext::interleavedLayout()
{
    LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << &view() << " LocalFrameViewLayoutContext::interleavedLayout() with size " << view().layoutSize());

    Ref protectedView(view());

    performLayouts(false);

    Style::Scope::LayoutDependencyUpdateContext layoutDependencyUpdateContext;
    document()->styleScope().invalidateForLayoutDependencies(layoutDependencyUpdateContext);
}

void LocalFrameViewLayoutContext::performLayouts(bool canDeferUpdateLayerPositions)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    WTFBeginSignpost(this, PerformSubtreesLayout, "subtrees: %u", m_subtreeLayoutRoots.size());
#endif
    if (isSubtreeLayout()) {
        // When performing a subtree layout, we may need to perform layout for each subtree.
        // The above might have been done in single pass, but calling performLayout() multiple times
        // has some nice side-effects such as reporting "Layout" events in the inspector with specific subtree areas.
        while (performLayout(canDeferUpdateLayerPositions, true) && !m_subtreeLayoutRoots.isEmpty()) { }
#ifndef NDEBUG
        // This function may be called recursively so let's do the checking when all the subtrees have been processed.
        if (m_subtreeLayoutRoots.isEmpty())
            RenderTreeNeedsLayoutChecker checker(*renderView());
#endif
    } else
        performLayout(canDeferUpdateLayerPositions);
#if PLATFORM(WPE) || PLATFORM(GTK)
    WTFEndSignpost(this, PerformSubtreesLayout);
#endif
}

bool LocalFrameViewLayoutContext::performLayout(bool canDeferUpdateLayerPositions, bool unchecked)
{
    Ref frame = this->frame();
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!document()->inRenderTreeUpdate());
    ASSERT(LayoutDisallowedScope::isLayoutAllowed());
    ASSERT(!view().isPainting());
    ASSERT(frame->view() == &view());
    ASSERT(document());
    ASSERT(document()->backForwardCacheState() == Document::NotInBackForwardCache
        || document()->backForwardCacheState() == Document::AboutToEnterBackForwardCache);
    if (!canPerformLayout()) {
        LOG(Layout, "  is not allowed, bailing");
        return false;
    }

    LayoutFrameScope layoutFrameScope(*this);
    TraceScope tracingScope(PerformLayoutStart, PerformLayoutEnd);
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    InspectorInstrumentation::willLayout(frame);
    RenderElement* subtreeLayoutRoot;
    SingleThreadWeakPtr<RenderElement> layoutRoot;
    
    m_layoutTimer.stop();
    m_setNeedsLayoutWasDeferred = false;

#if !LOG_DISABLED
    if (m_firstLayout && !frame->ownerElement())
        LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << &view() << " elapsed time before first layout: " << document()->timeSinceDocumentCreation());
#endif
#if PLATFORM(IOS_FAMILY)
    if (protect(view())->updateFixedPositionLayoutRect() && isSubtreeLayout())
        convertSubtreeLayoutToFullLayout();
#endif
    {
        SetForScope layoutPhase(m_layoutPhase, LayoutPhase::InPreLayout);

        if (!protect(document())->isInStyleInterleavedLayoutForSelfOrAncestor()) {
            // If this is a new top-level layout and there are any remaining tasks from the previous layout, finish them now.
            if (!isLayoutNested() && m_postLayoutTaskTimer.isActive())
                runPostLayoutTasks();

            updateStyleForLayout();
        }

        if (view().hasOneRef())
            return false;

        protect(view())->autoSizeIfEnabled();
        if (!renderView())
            return false;

        subtreeLayoutRoot = isSubtreeLayout() ? *m_subtreeLayoutRoots.begin() : nullptr;
        layoutRoot = subtreeLayoutRoot ?: renderView();
        m_needsFullRepaint = is<RenderView>(layoutRoot) && (m_firstLayout || renderView()->printing());

        LOG_WITH_STREAM(Layout, stream << "LocalFrameView " << &view() << " layout " << m_layoutUpdateCount << " - subtree root " << subtreeLayoutRoot << ", needsFullRepaint " << m_needsFullRepaint);

        protect(view())->willDoLayout(layoutRoot);
        m_firstLayout = false;
    }

    Vector<FloatQuad> layoutAreas;
    {
        TraceScope tracingScope(RenderTreeLayoutStart, RenderTreeLayoutEnd);
        SetForScope layoutPhase(m_layoutPhase, LayoutPhase::InRenderTreeLayout);
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        SubtreeLayoutStateMaintainer subtreeLayoutStateMaintainer(subtreeLayoutRoot);
        RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());
#ifndef NDEBUG
        std::unique_ptr<RenderTreeNeedsLayoutChecker> checker;
        if (!unchecked)
            checker = WTF::makeUnique<RenderTreeNeedsLayoutChecker>(*renderView());
#else
        UNUSED_PARAM(unchecked);
#endif
        layoutRoot->layout();
#if ENABLE(TEXT_AUTOSIZING)
        {
            CheckedPtr renderView = this->renderView();
            auto state = renderView ? renderView->textAutosizingState() : RenderView::TextAutosizingState::Normal;
            switch (state) {
            case RenderView::TextAutosizingState::Normal:
                applyTextSizingIfNeeded(*layoutRoot.get());
                break;
            case RenderView::TextAutosizingState::ResetScheduled: {
                renderView->resetTextAutosizing();
                // resetTextAutosizing() restores specified font sizes via setStyle. That
                // dirties the render tree only if autosized nodes existed; a dirty tree
                // forces a follow-up layout in this same resize transaction against the
                // still-stale block widths, which must also skip autosize.
                bool resetTriggeredFollowUpLayout = m_frameView->needsLayout();
                renderView->setTextAutosizingState(resetTriggeredFollowUpLayout
                    ? RenderView::TextAutosizingState::SkipAfterReset
                    : RenderView::TextAutosizingState::Normal);
                break;
            }
            case RenderView::TextAutosizingState::SkipAfterReset:
                renderView->setTextAutosizingState(RenderView::TextAutosizingState::Normal);
                break;
            }
        }
#endif
        for (auto* subtreeLayoutRoot : m_subtreeLayoutRoots)
            subtreeLayoutRoot->absoluteQuads(layoutAreas);

        if (subtreeLayoutRoot)
            removeSubtreeLayoutRoot(*subtreeLayoutRoot);
        ASSERT(m_percentHeightIgnoreList.isEmptyIgnoringNullReferences());

#if !LOG_DISABLED && ENABLE(TREE_DEBUGGING)
        auto layoutLogEnabled = [] {
            return LogLayout.state == WTFLogChannelState::On;
        };
        if (layoutLogEnabled())
            showRenderTree(renderView());
#endif
    }
    {
        SetForScope layoutPhase(m_layoutPhase, LayoutPhase::InViewSizeAdjust);
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (is<RenderView>(layoutRoot) && !renderView()->printing()) {
            // This is to protect m_needsFullRepaint's value when layout() is getting re-entered through adjustViewSize().
            SetForScope needsFullRepaint(m_needsFullRepaint);
            protect(view())->adjustViewSize();
            // FIXME: Firing media query callbacks synchronously on nested frames could produced a detached FrameView here by
            // navigating away from the current document (see webkit.org/b/173329).
            if (view().hasOneRef())
                return true;
        }
    }
    {
        SetForScope layoutPhase(m_layoutPhase, LayoutPhase::InPostLayout);
        if (m_needsFullRepaint)
            renderView()->repaintRootContents();
        ASSERT(!layoutRoot->needsLayout());
        protect(view())->didLayout(layoutRoot, canDeferUpdateLayerPositions);
        runOrScheduleAsynchronousTasks(canDeferUpdateLayerPositions);
    }
    InspectorInstrumentation::didLayout(frame, *layoutRoot, layoutAreas);
    DebugPageOverlays::didLayout(frame);
    return true;
}

void LocalFrameViewLayoutContext::runOrScheduleAsynchronousTasks(bool canDeferUpdateLayerPositions)
{
    if (m_postLayoutTaskTimer.isActive())
        return;

    if (document()->isInStyleInterleavedLayout()) {
        // We are doing layout from style resolution to resolve container queries and/or anchor-positioned elements.
        m_postLayoutTaskTimer.startOneShot(0_s);
        return;
    }

    // If we are already in performPostLayoutTasks(), defer post layout tasks until after we return
    // to avoid re-entrancy.
    if (m_inAsynchronousTasks) {
        m_postLayoutTaskTimer.startOneShot(0_s);
        return;
    }

    runPostLayoutTasks();
    if (needsLayoutInternal()) {
        // If runPostLayoutTasks() made us layout again, let's defer the tasks until after we return.
        m_postLayoutTaskTimer.startOneShot(0_s);
        layout(canDeferUpdateLayerPositions);
    }
}

void LocalFrameViewLayoutContext::runPostLayoutTasks()
{
    m_postLayoutTaskTimer.stop();
    if (m_inAsynchronousTasks)
        return;
    SetForScope inAsynchronousTasks(m_inAsynchronousTasks, true);
    protect(view())->performPostLayoutTasks();
}

void LocalFrameViewLayoutContext::flushPostLayoutTasks()
{
    if (!m_postLayoutTaskTimer.isActive())
        return;
    runPostLayoutTasks();
}

void LocalFrameViewLayoutContext::didLayout(bool canDeferUpdateLayerPositions)
{
    m_layoutUpdateCount++;

    requestUpdateLayerPositions(needsFullRepaint());

    if (!canDeferUpdateLayerPositions)
        flushUpdateLayerPositions();

    m_updateCompositingLayersIsPending = true;
}

void LocalFrameViewLayoutContext::requestUpdateLayerPositions(bool needsFullRepaint)
{
    auto updateLayerPositions = UpdateLayerPositions { needsFullRepaint };
    if (m_pendingUpdateLayerPositions)
        m_pendingUpdateLayerPositions->merge(updateLayerPositions);
    else
        m_pendingUpdateLayerPositions = updateLayerPositions;
}

void LocalFrameViewLayoutContext::flushUpdateLayerPositions()
{
    if (!m_pendingUpdateLayerPositions)
        return;

    CheckedPtr view = renderView();
    if (!view)
        return;

    auto repaintRectEnvironment = RepaintRectEnvironment { view->page().deviceScaleFactor(), document()->printing(), this->view().useFixedLayout() };
    bool environmentChanged = repaintRectEnvironment != m_lastRepaintRectEnvironment;

    auto updateLayerPositions = *std::exchange(m_pendingUpdateLayerPositions, std::nullopt);
    view->layer()->updateLayerPositionsAfterLayout(updateLayerPositions.needsFullRepaint, environmentChanged);

    m_renderLayerPositionUpdateCount++;
    m_lastRepaintRectEnvironment = WTF::move(repaintRectEnvironment);
}

bool LocalFrameViewLayoutContext::updateCompositingLayersAfterStyleChange()
{
    // If we expect to update compositing after an incipient layout, don't do so here.
    CheckedPtr view = renderView();
    if (!view)
        return false;

    if (needsLayout() || isInLayout())
        return false;

    auto repaintRectEnvironment = RepaintRectEnvironment { view->page().deviceScaleFactor(), document()->printing(), this->view().useFixedLayout() };
    bool environmentChanged = repaintRectEnvironment != m_lastRepaintRectEnvironment;

    view->layer()->updateLayerPositionsAfterStyleChange(environmentChanged);

    m_lastRepaintRectEnvironment = WTF::move(repaintRectEnvironment);

    return view->compositor().didRecalcStyleWithNoPendingLayout();
}

void LocalFrameViewLayoutContext::markForUpdateLayerPositionsAfterSVGTransformChange()
{
    CheckedPtr view = renderView();
    if (!view)
        return;

    if (needsLayout() || isInLayout())
        return;

    requestUpdateLayerPositions();
    view->page().scheduleRenderingUpdate({ RenderingUpdateStep::LayerFlush });
}

void LocalFrameViewLayoutContext::updateCompositingLayersAfterLayout()
{
    CheckedPtr renderView = this->renderView();
    if (!renderView)
        return;

    renderView->compositor().updateCompositingLayers(CompositingUpdateType::AfterLayout);
    m_updateCompositingLayersIsPending = false;
}

bool LocalFrameViewLayoutContext::updateCompositingLayersAfterLayoutIfNeeded()
{
    if (m_updateCompositingLayersIsPending) {
        updateCompositingLayersAfterLayout();
        return true;
    }

    return false;
}

void LocalFrameViewLayoutContext::reset()
{
    m_layoutPhase = LayoutPhase::OutsideLayout;
    clearSubtreeLayoutRoots();
    m_layoutSchedulingIsEnabled = true;
    m_layoutTimer.stop();
    m_firstLayout = true;
    m_postLayoutTaskTimer.stop();
    m_needsFullRepaint = true;
}

bool LocalFrameViewLayoutContext::needsLayout(OptionSet<LayoutOptions> layoutOptions) const
{
    if (needsLayoutInternal())
        return true;

    if (layoutOptions.contains(LayoutOptions::CanDeferUpdateLayerPositions))
        return false;

    return hasPendingUpdateLayerPositions();
}

bool LocalFrameViewLayoutContext::needsLayoutInternal() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.
    auto* renderView = this->renderView();
    return isLayoutPending()
        || (renderView && renderView->needsLayout())
        || isSubtreeLayout()
        || (m_disableSetNeedsLayoutCount && m_setNeedsLayoutWasDeferred);
}

void LocalFrameViewLayoutContext::setNeedsLayoutAfterViewConfigurationChange()
{
    if (m_disableSetNeedsLayoutCount) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (CheckedPtr renderView = this->renderView()) {
        ASSERT(!document()->inHitTesting());
        renderView->setNeedsLayout();
        scheduleLayout();
    }
}

void LocalFrameViewLayoutContext::enableSetNeedsLayout()
{
    ASSERT(m_disableSetNeedsLayoutCount);
    if (!--m_disableSetNeedsLayoutCount)
        m_setNeedsLayoutWasDeferred = false; // FIXME: Find a way to make the deferred layout actually happen.
}

void LocalFrameViewLayoutContext::disableSetNeedsLayout()
{
    ++m_disableSetNeedsLayoutCount;
}

void LocalFrameViewLayoutContext::scheduleLayout()
{
    // FIXME: We should assert the page is not in the back/forward cache, but that is causing
    // too many false assertions. See <rdar://problem/7218118>.
    ASSERT(frame().view() == &view());
    RefPtr document = this->document();
    if (!document)
        return;

    if (isSubtreeLayout())
        convertSubtreeLayoutToFullLayout();

    if (isLayoutPending())
        return;

    if (!isLayoutSchedulingEnabled() || !document->shouldScheduleLayout())
        return;
    if (!needsLayout())
        return;

#if !LOG_DISABLED
    if (!document->ownerElement())
        LOG(Layout, "LocalFrameView %p layout timer scheduled at %.3fs", this, document->timeSinceDocumentCreation().value());
#endif

    InspectorInstrumentation::didInvalidateLayout(protect(frame()));
    m_layoutTimer.startOneShot(0_s);
}

void LocalFrameViewLayoutContext::unscheduleLayout()
{
    if (m_postLayoutTaskTimer.isActive())
        m_postLayoutTaskTimer.stop();

    if (!m_layoutTimer.isActive())
        return;

#if !LOG_DISABLED
    if (!document()->ownerElement())
        LOG_WITH_STREAM(Layout, stream << "LocalFrameViewLayoutContext for LocalFrameView " << frame().view() << " layout timer unscheduled at " << document()->timeSinceDocumentCreation().value());
#endif

    m_layoutTimer.stop();
}

void LocalFrameViewLayoutContext::scheduleSubtreeLayout(RenderElement& layoutRoot)
{
    ASSERT(renderView());
    CheckedRef renderView = *this->renderView();

    // Try to catch unnecessary work during render tree teardown.
    ASSERT(!renderView->renderTreeBeingDestroyed());
    ASSERT(frame().view() == &view());

    if (renderView->needsLayout() && !isSubtreeLayout()) {
        layoutRoot.markContainingBlocksForLayout(renderView.ptr());
        return;
    }

    if (!isLayoutPending() && isLayoutSchedulingEnabled()) {
        ASSERT(!layoutRoot.container() || is<RenderView>(layoutRoot.container()) || !layoutRoot.container()->needsLayout());
        addSubtreeLayoutRoot(layoutRoot);
        InspectorInstrumentation::didInvalidateLayout(protect(frame()));
        m_layoutTimer.startOneShot(0_s);
        return;
    }

    if (hasSubtreeLayoutRoot(layoutRoot))
        return;

    if (!isSubtreeLayout()) {
        // We already have a pending (full) layout. Just mark the subtree for layout.
        layoutRoot.markContainingBlocksForLayout(renderView.ptr());
        InspectorInstrumentation::didInvalidateLayout(protect(frame()));
        return;
    }

    // Combine new subtree with existing ones to make sure we store only independent subtrees.
    for (auto* subtreeLayoutRoot : m_subtreeLayoutRoots) {
        if (subtreeLayoutRoot->isAncestorContainerOfRenderer(layoutRoot)) {
            // New subtree is a subtree of existing subtree.
            layoutRoot.markContainingBlocksForLayout(subtreeLayoutRoot);
            ASSERT(!subtreeLayoutRoot->container() || is<RenderView>(subtreeLayoutRoot->container()) || !subtreeLayoutRoot->container()->needsLayout());
            return;
        }
        if (layoutRoot.isAncestorContainerOfRenderer(*subtreeLayoutRoot)) {
            // Existing subtree is a subtree of new subtree.
            subtreeLayoutRoot->markContainingBlocksForLayout(&layoutRoot);
            ASSERT(!layoutRoot.container() || is<RenderView>(layoutRoot.container()) || !layoutRoot.container()->needsLayout());
            InspectorInstrumentation::didInvalidateLayout(protect(frame()));
            removeSubtreeLayoutRoot(*subtreeLayoutRoot);
            addSubtreeLayoutRoot(layoutRoot);
            return;
        }
    }

    // We already have a pending subtree layout. Just add new subtree to collection.
    addSubtreeLayoutRoot(layoutRoot);
    InspectorInstrumentation::didInvalidateLayout(protect(frame()));
}

void LocalFrameViewLayoutContext::layoutTimerFired()
{
#if !LOG_DISABLED
    if (!document()->ownerElement())
        LOG_WITH_STREAM(Layout, stream << "LocalFrameViewLayoutContext for LocalFrameView " << frame().view() << " layout timer fired at " << document()->timeSinceDocumentCreation().value());
#endif
    layout();
}

bool LocalFrameViewLayoutContext::hasSubtreeLayoutRoot(const RenderElement& subtreeLayoutRoot) const
{
    return m_subtreeLayoutRoots.contains(const_cast<RenderElement*>(&subtreeLayoutRoot));
}

void LocalFrameViewLayoutContext::clearSubtreeLayoutRoots()
{
    m_subtreeLayoutRoots.clear();
}

void LocalFrameViewLayoutContext::convertSubtreeLayoutToFullLayout()
{
    ASSERT(!m_subtreeLayoutRoots.isEmpty());
    for (auto* subtreeLayoutRoot : m_subtreeLayoutRoots)
        subtreeLayoutRoot->markContainingBlocksForLayout(renderView());
    clearSubtreeLayoutRoots();
}

void LocalFrameViewLayoutContext::addSubtreeLayoutRoot(RenderElement& subtreeLayoutRoot)
{
    m_subtreeLayoutRoots.add(&subtreeLayoutRoot);
}

void LocalFrameViewLayoutContext::removeSubtreeLayoutRoot(const RenderElement& subtreeLayoutRoot)
{
    m_subtreeLayoutRoots.remove(const_cast<RenderElement*>(&subtreeLayoutRoot));
}

bool LocalFrameViewLayoutContext::canPerformLayout() const
{
    if (isInRenderTreeLayout())
        return false;

    if (view().isPainting())
        return false;

    if (!isSubtreeLayout() && !document()->renderView())
        return false;

    return true;
}

#if ENABLE(TEXT_AUTOSIZING)
void LocalFrameViewLayoutContext::applyTextSizingIfNeeded(RenderElement& layoutRoot)
{
    ASSERT(document());
    if (protect(document())->quirks().shouldIgnoreTextAutoSizing())
        return;
    Ref settings = layoutRoot.settings();
    bool idempotentMode = settings->textAutosizingUsesIdempotentMode();
    if (!settings->textAutosizingEnabled() || idempotentMode || renderView()->printing())
        return;
    auto minimumZoomFontSize = settings->minimumZoomFontSize();
    if (!idempotentMode && !minimumZoomFontSize)
        return;
    auto textAutosizingWidth = layoutRoot.page().textAutosizingWidth();
    if (auto overrideWidth = settings->textAutosizingWindowSizeOverrideWidth())
        textAutosizingWidth = overrideWidth;
    if (!idempotentMode && !textAutosizingWidth)
        return;
    layoutRoot.adjustComputedFontSizesOnBlocks(minimumZoomFontSize, textAutosizingWidth);
    if (!layoutRoot.needsLayout())
        return;
    LOG(TextAutosizing, "Text Autosizing: minimumZoomFontSize=%.2f textAutosizingWidth=%.2f", minimumZoomFontSize, textAutosizingWidth);
    layoutRoot.layout();
}
#endif

void LocalFrameViewLayoutContext::updateStyleForLayout()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    Ref document = *this->document();

    // FIXME: This shouldn't be necessary, but see rdar://problem/36670246.
    if (!document->styleScope().resolverIfExists())
        document->styleScope().didChangeStyleSheetEnvironment();

    // Viewport-dependent media queries may cause us to need completely different style information.
    document->styleScope().evaluateMediaQueriesForViewportChange();

    document->updateElementsAffectedByMediaQueries();
    // If there is any pagination to apply, it will affect the RenderView's style, so we should
    // take care of that now.
    protect(view())->applyPaginationToViewport();
    // Always ensure our style info is up-to-date. This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    document->updateStyleIfNeeded();
}

LayoutSize LocalFrameViewLayoutContext::layoutDelta() const
{
    if (auto* layoutState = this->layoutState())
        return layoutState->layoutDelta();
    return { };
}

void LocalFrameViewLayoutContext::addLayoutDelta(const LayoutSize& delta)
{
    if (auto* layoutState = this->layoutState())
        layoutState->addLayoutDelta(delta);
}

bool LocalFrameViewLayoutContext::isSkippedContentForLayout(const RenderElement& renderer) const
{
    if (isVisiblityHiddenIgnored() || isVisiblityAutoIgnored() || (isRevealedWhenFoundIgnored() && renderer.style().autoRevealsWhenFound())) {
        // In theory we should only descend into a hidden/auto subree when hidden/auto root is ignored (see isSkippedContentRootForLayout below).
        return false;
    }
    return renderer.isSkippedContent();
}

bool LocalFrameViewLayoutContext::isSkippedContentRootForLayout(const RenderBox& renderBox) const
{
    if (!isSkippedContentRoot(renderBox))
        return false;

    auto contentVisibility = renderBox.style().contentVisibility();
    if (contentVisibility == ContentVisibility::Hidden && isVisiblityHiddenIgnored())
        return false;

    if (contentVisibility == ContentVisibility::Auto && isVisiblityAutoIgnored())
        return false;

    if (renderBox.style().autoRevealsWhenFound() && isRevealedWhenFoundIgnored())
        return false;

    return true;
}

#if ASSERT_ENABLED
bool LocalFrameViewLayoutContext::layoutDeltaMatches(const LayoutSize& delta)
{
    if (auto* layoutState = this->layoutState())
        return layoutState->layoutDeltaMatches(delta);
    return false;
}
#endif

RenderLayoutState* LocalFrameViewLayoutContext::layoutState() const
{
    if (m_layoutStateStack.isEmpty())
        return nullptr;
    return m_layoutStateStack.last().get();
}

void LocalFrameViewLayoutContext::pushLayoutState(RenderElement& root)
{
    ASSERT(!m_paintOffsetCacheDisableCount);
    ASSERT(!layoutState());

    m_layoutStateStack.append(makeUnique<RenderLayoutState>(root));
}

bool LocalFrameViewLayoutContext::pushLayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageHeight, bool pageHeightChanged)
{
    // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
    auto* layoutState = this->layoutState();
    if (!layoutState || !needsFullRepaint() || layoutState->isPaginated() || renderer.enclosingFragmentedFlow()
        || layoutState->lineGrid() || (!renderer.style().lineGrid().isNone() && renderer.isRenderBlockFlow())) {
        m_layoutStateStack.append(makeUnique<RenderLayoutState>(m_layoutStateStack
            , renderer
            , offset
            , pageHeight
            , pageHeightChanged
            , layoutState ? layoutState->lineClamp() : std::nullopt
            , layoutState ? layoutState->legacyLineClamp() : std::nullopt));
        return true;
    }
    return false;
}
    
void LocalFrameViewLayoutContext::popLayoutState()
{
    if (!layoutState())
        return;

    auto currentLineClamp = layoutState()->legacyLineClamp();

    m_layoutStateStack.removeLast();

    if (currentLineClamp) {
        // Propagates the current line clamp state to the parent.
        if (auto* layoutState = this->layoutState(); layoutState && layoutState->legacyLineClamp()) {
            ASSERT(layoutState->legacyLineClamp()->maximumLineCount == currentLineClamp->maximumLineCount);
            layoutState->setLegacyLineClamp(currentLineClamp);
        }
    }
}

bool LocalFrameViewLayoutContext::DetachedRendererList::append(RenderPtr<RenderObject>&& detachedRenderer)
{
    ASSERT(!detachedRenderer->parent());
    ASSERT(!detachedRenderer->beingDestroyed());

    if (detachedRenderer->renderTreeBeingDestroyed())
        return false;

    if (is<RenderWidget>(detachedRenderer)) {
        // FIXME: Cleanup RenderWidget's destruction process (ref vs. delete this. see RenderObject::destroy)
        return false;
    }

    static constexpr unsigned maximumNumberOfDetachedRenderers = 5000;
    if (m_renderers.size() == maximumNumberOfDetachedRenderers)
        clear();

    m_renderers.append(detachedRenderer.release());
    return true;
}

LocalFrameViewLayoutContext::DetachedInlineContentList::~DetachedInlineContentList() = default;

void LocalFrameViewLayoutContext::DetachedInlineContentList::append(std::unique_ptr<LayoutIntegration::InlineContent>&& content)
{
    static constexpr unsigned maximumNumberOfDeferredInlineContent = 5000;
    if (m_inlineContent.size() == maximumNumberOfDeferredInlineContent)
        clear();

    m_inlineContent.append(WTF::move(content));
}

void LocalFrameViewLayoutContext::DetachedInlineContentList::clear()
{
    m_inlineContent.clear();
}

void LocalFrameViewLayoutContext::detachInlineContent(std::unique_ptr<LayoutIntegration::InlineContent>&& content) const
{
    m_detachedInlineContent.append(WTF::move(content));
}

void LocalFrameViewLayoutContext::deleteDetachedInlineContentNow() const
{
    m_detachedInlineContent.clear();
}

void LocalFrameViewLayoutContext::setBoxNeedsTransformUpdateAfterContainerLayout(RenderBox& box, RenderBlock& container)
{
    auto it = m_containersWithDescendantsNeedingTransformUpdate.ensure(container, [] { return Vector<SingleThreadWeakPtr<RenderBox>>({ }); });
    it.iterator->value.append(WeakPtr { box });
}

Vector<SingleThreadWeakPtr<RenderBox>> LocalFrameViewLayoutContext::takeBoxesNeedingTransformUpdateAfterContainerLayout(RenderBlock& container)
{
    return m_containersWithDescendantsNeedingTransformUpdate.take(container);
}

void LocalFrameViewLayoutContext::disablePercentHeightResolveFor(const RenderBox& flexItem)
{
    m_percentHeightIgnoreList.add(flexItem);
}

void LocalFrameViewLayoutContext::enablePercentHeightResolveFor(const RenderBox& flexItem)
{
    m_percentHeightIgnoreList.remove(flexItem);
}

bool LocalFrameViewLayoutContext::isPercentHeightResolveDisabledFor(const RenderBox& flexItem)
{
    return m_percentHeightIgnoreList.contains(flexItem);
}

#ifndef NDEBUG
void LocalFrameViewLayoutContext::checkLayoutState()
{
    ASSERT(layoutDeltaMatches(LayoutSize()));
    ASSERT(!m_paintOffsetCacheDisableCount);
}
#endif

const AnchorScrollAdjuster* LocalFrameViewLayoutContext::anchorScrollAdjusterFor(const RenderBox& anchored) const
{
    auto index = m_anchorScrollAdjusters.findIf([&](auto& item) {
        return item.anchored() == &anchored;
    });
    if (index == WTF::notFound)
        return { };
    return &m_anchorScrollAdjusters[index];
}

AnchorScrollAdjuster::Diff LocalFrameViewLayoutContext::registerAnchorScrollAdjuster(AnchorScrollAdjuster&& scrollAdjuster)
{
    auto index = m_anchorScrollAdjusters.findIf([&](auto& item) {
        return item.anchored() == scrollAdjuster.anchored();
    });

    bool recaptureDiffers = false;
    if (WTF::notFound == index) {
        m_anchorScrollAdjusters.append(WTF::move(scrollAdjuster));
        return AnchorScrollAdjuster::New;
    }

    recaptureDiffers = m_anchorScrollAdjusters[index].recaptureDiffers(scrollAdjuster);
    m_anchorScrollAdjusters[index] = WTF::move(scrollAdjuster);
    return recaptureDiffers ? AnchorScrollAdjuster::SnapshotsDiffer : AnchorScrollAdjuster::SnapshotsMatch;
}

void LocalFrameViewLayoutContext::unregisterAnchorScrollAdjusterFor(const RenderBox& anchored)
{
    m_anchorScrollAdjusters.removeFirstMatching([&](auto& item) {
        return item.anchored() == &anchored;
    });
    ASSERT(!m_anchorScrollAdjusters.containsIf([&](auto& item) {
        return item.anchored() == &anchored;
    }));

    if (anchored.layer())
        anchored.layer()->clearAnchorScrollAdjustment();
}

void LocalFrameViewLayoutContext::invalidateAnchorDependenciesForScroller(const RenderBox& scroller)
{
    for (auto& adjuster : m_anchorScrollAdjusters)
        adjuster.invalidateForScroller(scroller);
}

void LocalFrameViewLayoutContext::removeScrollerFromAnchorScrollAdjusters(const RenderBox& scroller)
{
    if (!renderView() || renderView()->renderTreeBeingDestroyed())
        m_anchorScrollAdjusters.clear();
    else {
        for (auto& adjuster : m_anchorScrollAdjusters) {
            if (adjuster.invalidateForScroller(scroller))
                unregisterAnchorScrollAdjusterFor(*adjuster.anchored());
        }
    }
}

LocalFrame& LocalFrameViewLayoutContext::frame() const
{
    return view().frame();
}

LocalFrameView& LocalFrameViewLayoutContext::view() const
{
    return m_frameView.get();
}

RenderView* LocalFrameViewLayoutContext::renderView() const
{
    return view().renderView();
}

Document* LocalFrameViewLayoutContext::document() const
{
    return frame().document();
}

} // namespace WebCore
