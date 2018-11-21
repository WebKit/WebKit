/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
#include "FrameViewLayoutContext.h"

#include "CSSAnimationController.h"
#include "DebugPageOverlays.h"
#include "Document.h"
#include "FrameView.h"
#include "InspectorInstrumentation.h"
#include "LayoutDisallowedScope.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
#include "FormattingState.h"
#include "LayoutContainer.h"
#include "LayoutFormattingState.h"
#include "LayoutTreeBuilder.h"
#endif

#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
static void layoutUsingFormattingContext(const RenderView& renderView)
{
    auto initialContainingBlock = Layout::TreeBuilder::createLayoutTree(renderView);
    auto layoutState = std::make_unique<Layout::LayoutState>(*initialContainingBlock, renderView.size());
    layoutState->setInQuirksMode(renderView.document().inQuirksMode());
    layoutState->updateLayout();
    layoutState->verifyAndOutputMismatchingLayoutTree(renderView);
} 
#endif

static bool isObjectAncestorContainerOf(RenderElement& ancestor, RenderElement& descendant)
{
    for (auto* renderer = &descendant; renderer; renderer = renderer->container()) {
        if (renderer == &ancestor)
            return true;
    }
    return false;
}

#ifndef NDEBUG
class RenderTreeNeedsLayoutChecker {
public :
    RenderTreeNeedsLayoutChecker(const RenderElement& layoutRoot)
        : m_layoutRoot(layoutRoot)
    {
    }

    ~RenderTreeNeedsLayoutChecker()
    {
        auto reportNeedsLayoutError = [] (const RenderObject& renderer) {
            WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "post-layout: dirty renderer(s)");
            renderer.showRenderTreeForThis();
            ASSERT_NOT_REACHED();
        };

        if (m_layoutRoot.needsLayout()) {
            reportNeedsLayoutError(m_layoutRoot);
            return;
        }

        for (auto* descendant = m_layoutRoot.firstChild(); descendant; descendant = descendant->nextInPreOrder(&m_layoutRoot)) {
            if (!descendant->needsLayout())
                continue;
            
            reportNeedsLayoutError(*descendant);
            return;
        }
    }

private:
    const RenderElement& m_layoutRoot;
};
#endif

class LayoutScope {
public:
    LayoutScope(FrameViewLayoutContext& layoutContext)
        : m_view(layoutContext.view())
        , m_nestedState(layoutContext.m_layoutNestedState, layoutContext.m_layoutNestedState == FrameViewLayoutContext::LayoutNestedState::NotInLayout ? FrameViewLayoutContext::LayoutNestedState::NotNested : FrameViewLayoutContext::LayoutNestedState::Nested)
        , m_schedulingIsEnabled(layoutContext.m_layoutSchedulingIsEnabled, false)
        , m_inProgrammaticScroll(layoutContext.view().inProgrammaticScroll())
    {
        m_view.setInProgrammaticScroll(true);
    }
        
    ~LayoutScope()
    {
        m_view.setInProgrammaticScroll(m_inProgrammaticScroll);
    }
        
private:
    FrameView& m_view;
    SetForScope<FrameViewLayoutContext::LayoutNestedState> m_nestedState;
    SetForScope<bool> m_schedulingIsEnabled;
    bool m_inProgrammaticScroll { false };
};

FrameViewLayoutContext::FrameViewLayoutContext(FrameView& frameView)
    : m_frameView(frameView)
    , m_layoutTimer(*this, &FrameViewLayoutContext::layoutTimerFired)
    , m_asynchronousTasksTimer(*this, &FrameViewLayoutContext::runAsynchronousTasks)
{
}

FrameViewLayoutContext::~FrameViewLayoutContext()
{
}

void FrameViewLayoutContext::layout()
{
    LOG_WITH_STREAM(Layout, stream << "FrameView " << &view() << " FrameViewLayoutContext::layout() with size " << view().layoutSize());

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!frame().document()->inRenderTreeUpdate() || ScriptDisallowedScope::LayoutAssertionDisableScope::shouldDisable());
    ASSERT(LayoutDisallowedScope::isLayoutAllowed());
    ASSERT(!view().isPainting());
    ASSERT(frame().view() == &view());
    ASSERT(frame().document());
    ASSERT(frame().document()->pageCacheState() == Document::NotInPageCache);
    if (!canPerformLayout()) {
        LOG(Layout, "  is not allowed, bailing");
        return;
    }

    Ref<FrameView> protectView(view());
    LayoutScope layoutScope(*this);
    TraceScope tracingScope(LayoutStart, LayoutEnd);
    InspectorInstrumentationCookie inspectorLayoutScope(InspectorInstrumentation::willLayout(view().frame()));
    AnimationUpdateBlock animationUpdateBlock(&view().frame().animation());
    WeakPtr<RenderElement> layoutRoot;
    
    m_layoutTimer.stop();
    m_delayedLayout = false;
    m_setNeedsLayoutWasDeferred = false;

#if !LOG_DISABLED
    if (m_firstLayout && !frame().ownerElement())
        LOG(Layout, "FrameView %p elapsed time before first layout: %.3fs", this, document()->timeSinceDocumentCreation().value());
#endif
#if PLATFORM(IOS_FAMILY)
    if (view().updateFixedPositionLayoutRect() && subtreeLayoutRoot())
        convertSubtreeLayoutToFullLayout();
#endif
    if (handleLayoutWithFrameFlatteningIfNeeded())
        return;

    {
        SetForScope<LayoutPhase> layoutPhase(m_layoutPhase, LayoutPhase::InPreLayout);

        // If this is a new top-level layout and there are any remaining tasks from the previous layout, finish them now.
        if (!isLayoutNested() && m_asynchronousTasksTimer.isActive() && !view().isInChildFrameWithFrameFlattening())
            runAsynchronousTasks();

        updateStyleForLayout();
        if (view().hasOneRef())
            return;

        view().autoSizeIfEnabled();
        if (!renderView())
            return;

        layoutRoot = makeWeakPtr(subtreeLayoutRoot() ? subtreeLayoutRoot() : renderView());
        m_needsFullRepaint = is<RenderView>(layoutRoot.get()) && (m_firstLayout || renderView()->printing());
        view().willDoLayout(layoutRoot);
        m_firstLayout = false;
    }
    {
        SetForScope<LayoutPhase> layoutPhase(m_layoutPhase, LayoutPhase::InRenderTreeLayout);
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        SubtreeLayoutStateMaintainer subtreeLayoutStateMaintainer(subtreeLayoutRoot());
        RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());
#ifndef NDEBUG
        RenderTreeNeedsLayoutChecker checker(*layoutRoot);
#endif
        layoutRoot->layout();
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    layoutUsingFormattingContext(*renderView());
#endif
        ++m_layoutCount;
#if ENABLE(TEXT_AUTOSIZING)
        applyTextSizingIfNeeded(*layoutRoot.get());
#endif
        clearSubtreeLayoutRoot();
    }
    {
        SetForScope<LayoutPhase> layoutPhase(m_layoutPhase, LayoutPhase::InViewSizeAdjust);
        if (is<RenderView>(layoutRoot.get()) && !renderView()->printing()) {
            // This is to protect m_needsFullRepaint's value when layout() is getting re-entered through adjustViewSize().
            SetForScope<bool> needsFullRepaint(m_needsFullRepaint);
            view().adjustViewSize();
            // FIXME: Firing media query callbacks synchronously on nested frames could produced a detached FrameView here by
            // navigating away from the current document (see webkit.org/b/173329).
            if (view().hasOneRef())
                return;
        }
    }
    {
        SetForScope<LayoutPhase> layoutPhase(m_layoutPhase, LayoutPhase::InPostLayout);
        if (m_needsFullRepaint)
            renderView()->repaintRootContents();
        ASSERT(!layoutRoot->needsLayout());
        view().didLayout(layoutRoot);
        runOrScheduleAsynchronousTasks();
    }
    InspectorInstrumentation::didLayout(inspectorLayoutScope, *layoutRoot);
    DebugPageOverlays::didLayout(view().frame());
}

void FrameViewLayoutContext::runOrScheduleAsynchronousTasks()
{
    if (m_asynchronousTasksTimer.isActive())
        return;

    if (view().isInChildFrameWithFrameFlattening()) {
        // While flattening frames, we defer post layout tasks to avoid getting stuck in a cycle,
        // except updateWidgetPositions() which is required to kick off subframe layout in certain cases.
        if (!m_inAsynchronousTasks)
            view().updateWidgetPositions();
        m_asynchronousTasksTimer.startOneShot(0_s);
        return;
    }

    // If we are already in performPostLayoutTasks(), defer post layout tasks until after we return
    // to avoid re-entrancy.
    if (m_inAsynchronousTasks) {
        m_asynchronousTasksTimer.startOneShot(0_s);
        return;
    }

    runAsynchronousTasks();
    if (needsLayout()) {
        // If runAsynchronousTasks() made us layout again, let's defer the tasks until after we return.
        m_asynchronousTasksTimer.startOneShot(0_s);
        layout();
    }
}

void FrameViewLayoutContext::runAsynchronousTasks()
{
    m_asynchronousTasksTimer.stop();
    if (m_inAsynchronousTasks)
        return;
    SetForScope<bool> inAsynchronousTasks(m_inAsynchronousTasks, true);
    view().performPostLayoutTasks();
}

void FrameViewLayoutContext::flushAsynchronousTasks()
{
    if (!m_asynchronousTasksTimer.isActive())
        return;
    runAsynchronousTasks();
}

void FrameViewLayoutContext::reset()
{
    m_layoutPhase = LayoutPhase::OutsideLayout;
    clearSubtreeLayoutRoot();
    m_layoutCount = 0;
    m_layoutSchedulingIsEnabled = true;
    m_delayedLayout = false;
    m_layoutTimer.stop();
    m_firstLayout = true;
    m_asynchronousTasksTimer.stop();
    m_needsFullRepaint = true;
}

bool FrameViewLayoutContext::needsLayout() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.
    auto* renderView = this->renderView();
    return isLayoutPending()
        || (renderView && renderView->needsLayout())
        || subtreeLayoutRoot()
        || (m_disableSetNeedsLayoutCount && m_setNeedsLayoutWasDeferred);
}

void FrameViewLayoutContext::setNeedsLayout()
{
    if (m_disableSetNeedsLayoutCount) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (auto* renderView = this->renderView()) {
        ASSERT(!renderView->inHitTesting());
        renderView->setNeedsLayout();
    }
}

void FrameViewLayoutContext::enableSetNeedsLayout()
{
    ASSERT(m_disableSetNeedsLayoutCount);
    if (!--m_disableSetNeedsLayoutCount)
        m_setNeedsLayoutWasDeferred = false; // FIXME: Find a way to make the deferred layout actually happen.
}

void FrameViewLayoutContext::disableSetNeedsLayout()
{
    ++m_disableSetNeedsLayoutCount;
}

void FrameViewLayoutContext::scheduleLayout()
{
    // FIXME: We should assert the page is not in the page cache, but that is causing
    // too many false assertions. See <rdar://problem/7218118>.
    ASSERT(frame().view() == &view());

    if (subtreeLayoutRoot())
        convertSubtreeLayoutToFullLayout();
    if (!isLayoutSchedulingEnabled())
        return;
    if (!needsLayout())
        return;
    if (!frame().document()->shouldScheduleLayout())
        return;
    InspectorInstrumentation::didInvalidateLayout(frame());
    // When frame flattening is enabled, the contents of the frame could affect the layout of the parent frames.
    // Also invalidate parent frame starting from the owner element of this frame.
    if (frame().ownerRenderer() && view().isInChildFrameWithFrameFlattening())
        frame().ownerRenderer()->setNeedsLayout(MarkContainingBlockChain);

    Seconds delay = frame().document()->minimumLayoutDelay();
    if (m_layoutTimer.isActive() && m_delayedLayout && !delay)
        unscheduleLayout();

    if (m_layoutTimer.isActive())
        return;

    m_delayedLayout = delay.value();

#if !LOG_DISABLED
    if (!frame().document()->ownerElement())
        LOG(Layout, "FrameView %p scheduling layout for %.3fs", this, delay.value());
#endif

    m_layoutTimer.startOneShot(delay);
}

void FrameViewLayoutContext::unscheduleLayout()
{
    if (m_asynchronousTasksTimer.isActive())
        m_asynchronousTasksTimer.stop();

    if (!m_layoutTimer.isActive())
        return;

#if !LOG_DISABLED
    if (!frame().document()->ownerElement())
        LOG(Layout, "FrameView %p layout timer unscheduled at %.3fs", this, frame().document()->timeSinceDocumentCreation().value());
#endif

    m_layoutTimer.stop();
    m_delayedLayout = false;
}

void FrameViewLayoutContext::scheduleSubtreeLayout(RenderElement& layoutRoot)
{
    ASSERT(renderView());
    auto& renderView = *this->renderView();

    // Try to catch unnecessary work during render tree teardown.
    ASSERT(!renderView.renderTreeBeingDestroyed());
    ASSERT(frame().view() == &view());

    if (renderView.needsLayout() && !subtreeLayoutRoot()) {
        layoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No);
        return;
    }

    if (!isLayoutPending() && isLayoutSchedulingEnabled()) {
        Seconds delay = renderView.document().minimumLayoutDelay();
        ASSERT(!layoutRoot.container() || is<RenderView>(layoutRoot.container()) || !layoutRoot.container()->needsLayout());
        setSubtreeLayoutRoot(layoutRoot);
        InspectorInstrumentation::didInvalidateLayout(frame());
        m_delayedLayout = delay.value();
        m_layoutTimer.startOneShot(delay);
        return;
    }

    auto* subtreeLayoutRoot = this->subtreeLayoutRoot();
    if (subtreeLayoutRoot == &layoutRoot)
        return;

    if (!subtreeLayoutRoot) {
        // We already have a pending (full) layout. Just mark the subtree for layout.
        layoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No);
        InspectorInstrumentation::didInvalidateLayout(frame());
        return;
    }

    if (isObjectAncestorContainerOf(*subtreeLayoutRoot, layoutRoot)) {
        // Keep the current root.
        layoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No, subtreeLayoutRoot);
        ASSERT(!subtreeLayoutRoot->container() || is<RenderView>(subtreeLayoutRoot->container()) || !subtreeLayoutRoot->container()->needsLayout());
        return;
    }

    if (isObjectAncestorContainerOf(layoutRoot, *subtreeLayoutRoot)) {
        // Re-root at newRelayoutRoot.
        subtreeLayoutRoot->markContainingBlocksForLayout(ScheduleRelayout::No, &layoutRoot);
        setSubtreeLayoutRoot(layoutRoot);
        ASSERT(!layoutRoot.container() || is<RenderView>(layoutRoot.container()) || !layoutRoot.container()->needsLayout());
        InspectorInstrumentation::didInvalidateLayout(frame());
        return;
    }
    // Two disjoint subtrees need layout. Mark both of them and issue a full layout instead.
    convertSubtreeLayoutToFullLayout();
    layoutRoot.markContainingBlocksForLayout(ScheduleRelayout::No);
    InspectorInstrumentation::didInvalidateLayout(frame());
}

void FrameViewLayoutContext::layoutTimerFired()
{
#if !LOG_DISABLED
    if (!frame().document()->ownerElement())
        LOG(Layout, "FrameView %p layout timer fired at %.3fs", this, frame().document()->timeSinceDocumentCreation().value());
#endif
    layout();
}

void FrameViewLayoutContext::convertSubtreeLayoutToFullLayout()
{
    ASSERT(subtreeLayoutRoot());
    subtreeLayoutRoot()->markContainingBlocksForLayout(ScheduleRelayout::No);
    clearSubtreeLayoutRoot();
}

void FrameViewLayoutContext::setSubtreeLayoutRoot(RenderElement& layoutRoot)
{
    m_subtreeLayoutRoot = makeWeakPtr(layoutRoot);
}

bool FrameViewLayoutContext::canPerformLayout() const
{
    if (isInRenderTreeLayout())
        return false;

    if (layoutDisallowed())
        return false;

    if (view().isPainting())
        return false;

    if (!subtreeLayoutRoot() && !frame().document()->renderView())
        return false;

    return true;
}

#if ENABLE(TEXT_AUTOSIZING)
void FrameViewLayoutContext::applyTextSizingIfNeeded(RenderElement& layoutRoot)
{
    auto& settings = layoutRoot.settings();
    if (!settings.textAutosizingEnabled() || renderView()->printing())
        return;
    auto minimumZoomFontSize = settings.minimumZoomFontSize();
    if (!minimumZoomFontSize)
        return;
    auto textAutosizingWidth = layoutRoot.page().textAutosizingWidth();
    if (auto overrideWidth = settings.textAutosizingWindowSizeOverride().width())
        textAutosizingWidth = overrideWidth;
    if (!textAutosizingWidth)
        return;
    layoutRoot.adjustComputedFontSizesOnBlocks(minimumZoomFontSize, textAutosizingWidth);
    if (!layoutRoot.needsLayout())
        return;
    LOG(TextAutosizing, "Text Autosizing: minimumZoomFontSize=%.2f textAutosizingWidth=%.2f", minimumZoomFontSize, textAutosizingWidth);
    layoutRoot.layout();
}
#endif

void FrameViewLayoutContext::updateStyleForLayout()
{
    Document& document = *frame().document();

    // FIXME: This shouldn't be necessary, but see rdar://problem/36670246.
    if (!document.styleScope().resolverIfExists())
        document.styleScope().didChangeStyleSheetEnvironment();

    // Viewport-dependent media queries may cause us to need completely different style information.
    document.styleScope().evaluateMediaQueriesForViewportChange();

    document.evaluateMediaQueryList();
    // If there is any pagination to apply, it will affect the RenderView's style, so we should
    // take care of that now.
    view().applyPaginationToViewport();
    // Always ensure our style info is up-to-date. This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    document.updateStyleIfNeeded();
}

bool FrameViewLayoutContext::handleLayoutWithFrameFlatteningIfNeeded()
{
    if (!view().isInChildFrameWithFrameFlattening())
        return false;
    
    startLayoutAtMainFrameViewIfNeeded();
    auto* layoutRoot = subtreeLayoutRoot() ? subtreeLayoutRoot() : frame().document()->renderView();
    return !layoutRoot || !layoutRoot->needsLayout();
}

void FrameViewLayoutContext::startLayoutAtMainFrameViewIfNeeded()
{
    // When we start a layout at the child level as opposed to the topmost frame view and this child
    // frame requires flattening, we need to re-initiate the layout at the topmost view. Layout
    // will hit this view eventually.
    auto* parentView = view().parentFrameView();
    if (!parentView)
        return;

    // In the middle of parent layout, no need to restart from topmost.
    if (parentView->layoutContext().isInLayout())
        return;

    // Parent tree is clean. Starting layout from it would have no effect.
    if (!parentView->needsLayout())
        return;

    while (parentView->parentFrameView())
        parentView = parentView->parentFrameView();

    LOG(Layout, "  frame flattening, starting from root");
    parentView->layoutContext().layout();
}

LayoutSize FrameViewLayoutContext::layoutDelta() const
{
    if (auto* layoutState = this->layoutState())
        return layoutState->layoutDelta();
    return { };
}
    
void FrameViewLayoutContext::addLayoutDelta(const LayoutSize& delta)
{
    if (auto* layoutState = this->layoutState())
        layoutState->addLayoutDelta(delta);
}
    
#if !ASSERT_DISABLED
bool FrameViewLayoutContext::layoutDeltaMatches(const LayoutSize& delta)
{
    if (auto* layoutState = this->layoutState())
        return layoutState->layoutDeltaMatches(delta);
    return false;
}
#endif

LayoutState* FrameViewLayoutContext::layoutState() const
{
    if (m_layoutStateStack.isEmpty())
        return nullptr;
    return m_layoutStateStack.last().get();
}

void FrameViewLayoutContext::pushLayoutState(RenderElement& root)
{
    ASSERT(!m_paintOffsetCacheDisableCount);
    ASSERT(!layoutState());

    m_layoutStateStack.append(std::make_unique<LayoutState>(root));
}

bool FrameViewLayoutContext::pushLayoutStateForPaginationIfNeeded(RenderBlockFlow& layoutRoot)
{
    if (layoutState())
        return false;
    m_layoutStateStack.append(std::make_unique<LayoutState>(layoutRoot, LayoutState::IsPaginated::Yes));
    return true;
}
    
bool FrameViewLayoutContext::pushLayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageHeight, bool pageHeightChanged)
{
    // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
    auto* layoutState = this->layoutState();
    if (!layoutState || !needsFullRepaint() || layoutState->isPaginated() || renderer.enclosingFragmentedFlow()
        || layoutState->lineGrid() || (renderer.style().lineGrid() != RenderStyle::initialLineGrid() && renderer.isRenderBlockFlow())) {
        m_layoutStateStack.append(std::make_unique<LayoutState>(m_layoutStateStack, renderer, offset, pageHeight, pageHeightChanged));
        return true;
    }
    return false;
}
    
void FrameViewLayoutContext::popLayoutState()
{
    m_layoutStateStack.removeLast();
}
    
#ifndef NDEBUG
void FrameViewLayoutContext::checkLayoutState()
{
    ASSERT(layoutDeltaMatches(LayoutSize()));
    ASSERT(!m_paintOffsetCacheDisableCount);
}
#endif

Frame& FrameViewLayoutContext::frame() const
{
    return view().frame();
}

FrameView& FrameViewLayoutContext::view() const
{
    return m_frameView;
}

RenderView* FrameViewLayoutContext::renderView() const
{
    return view().renderView();
}

Document* FrameViewLayoutContext::document() const
{
    return frame().document();
}

} // namespace WebCore
