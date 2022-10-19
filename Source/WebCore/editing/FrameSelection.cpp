/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "FrameSelection.h"

#include "AXObjectCache.h"
#include "CharacterData.h"
#include "ColorBlending.h"
#include "DeleteSelectionCommand.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "ImageOverlay.h"
#include "InlineRunAndOffset.h"
#include "LegacyInlineTextBox.h"
#include "Logging.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RenderedPosition.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "SimpleRange.h"
#include "SpatialNavigation.h"
#include "StyleProperties.h"
#include "StyleTreeResolver.h"
#include "TypingCommand.h"
#include "VisibleUnits.h"
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "Chrome.h"
#include "ChromeClient.h"
#include "Color.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SelectionGeometry.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static inline LayoutUnit NoXPosForVerticalArrowNavigation()
{
    return LayoutUnit::min();
}

CaretBase::CaretBase(CaretVisibility visibility)
    : m_caretRectNeedsUpdate(true)
    , m_caretVisibility(visibility)
{
}

DragCaretController::DragCaretController()
    : CaretBase(Visible)
{
}

bool DragCaretController::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(m_position.deepEquivalent());
}

IntRect DragCaretController::caretRectInRootViewCoordinates() const
{
    if (!hasCaret())
        return { };

    if (auto* document = m_position.deepEquivalent().document()) {
        if (auto* documentView = document->view())
            return documentView->contentsToRootView(m_position.absoluteCaretBounds());
    }

    return { };
}

IntRect DragCaretController::editableElementRectInRootViewCoordinates() const
{
    if (!hasCaret())
        return { };

    RefPtr<ContainerNode> editableContainer;
    if (auto* formControl = enclosingTextFormControl(m_position.deepEquivalent()))
        editableContainer = formControl;
    else
        editableContainer = highestEditableRoot(m_position.deepEquivalent());

    if (!editableContainer)
        return { };

    auto* renderer = editableContainer->renderer();
    if (!renderer)
        return { };

    if (auto* view = editableContainer->document().view())
        return view->contentsToRootView(renderer->absoluteBoundingBoxRect()); // FIXME: Wrong for elements with visible layout overflow.

    return { };
}

static inline bool shouldAlwaysUseDirectionalSelection(Document* document)
{
    return !document || document->editor().behavior().shouldConsiderSelectionAsDirectional();
}

static inline bool isPageActive(Document* document)
{
    return document && document->page() && document->page()->focusController().isActive();
}

FrameSelection::FrameSelection(Document* document)
    : m_document(document)
    , m_granularity(TextGranularity::CharacterGranularity)
#if ENABLE(TEXT_CARET)
    , m_caretBlinkTimer(*this, &FrameSelection::caretBlinkTimerFired)
#endif
    , m_appearanceUpdateTimer(*this, &FrameSelection::appearanceUpdateTimerFired)
    , m_caretInsidePositionFixed(false)
    , m_absCaretBoundsDirty(true)
    , m_caretPaint(true)
    , m_isCaretBlinkingSuspended(false)
    , m_focused(document && document->frame() && document->page() && document->page()->focusController().focusedFrame() == document->frame())
    , m_isActive(isPageActive(document))
    , m_shouldShowBlockCursor(false)
    , m_pendingSelectionUpdate(false)
    , m_alwaysAlignCursorOnScrollWhenRevealingSelection(false)
#if PLATFORM(IOS_FAMILY)
    , m_updateAppearanceEnabled(false)
    , m_caretBlinks(true)
#endif
{
    if (shouldAlwaysUseDirectionalSelection(m_document.get()))
        m_selection.setIsDirectional(true);

    bool activeAndFocused = isFocusedAndActive();
    if (activeAndFocused)
        setSelectionFromNone();
#if USE(UIKIT_EDITING)
    // Caret blinking (blinks | does not blink)
    setCaretVisible(activeAndFocused);
#else
    setCaretVisibility(activeAndFocused ? Visible : Hidden, ShouldUpdateAppearance::No);
#endif
}

Element* FrameSelection::rootEditableElementOrDocumentElement() const
{
    Element* selectionRoot = m_selection.rootEditableElement();
    return selectionRoot ? selectionRoot : m_document->documentElement();
}

void FrameSelection::moveTo(const VisiblePosition& position, EUserTriggered userTriggered, CursorAlignOnScroll align)
{
    setSelection(VisibleSelection(position.deepEquivalent(), position.deepEquivalent(), position.affinity(), m_selection.isDirectional()),
        defaultSetSelectionOptions(userTriggered), AXTextStateChangeIntent(), align);
}

void FrameSelection::moveTo(const VisiblePosition& base, const VisiblePosition& extent, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveTo(const Position& position, Affinity affinity, EUserTriggered userTriggered)
{
    setSelection(VisibleSelection(position, affinity, m_selection.isDirectional()), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveTo(const Position& base, const Position& extent, Affinity affinity, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(base, extent, affinity, selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveWithoutValidationTo(const Position& base, const Position& extent, bool selectionHasDirection, bool shouldSetFocus, SelectionRevealMode revealMode, const AXTextStateChangeIntent& intent)
{
    VisibleSelection newSelection;
    newSelection.setWithoutValidation(base, extent);
    newSelection.setIsDirectional(selectionHasDirection);
    AXTextStateChangeIntent newIntent = intent.type == AXTextStateChangeTypeUnknown ? AXTextStateChangeIntent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, false }) : intent;
    auto options = defaultSetSelectionOptions();
    if (!shouldSetFocus)
        options.add(DoNotSetFocus);
    switch (revealMode) {
    case SelectionRevealMode::DoNotReveal:
        break;
    case SelectionRevealMode::Reveal:
        options.add(RevealSelection);
        break;
    case SelectionRevealMode::RevealUpToMainFrame:
        options.add(RevealSelectionUpToMainFrame);
        break;
    case SelectionRevealMode::DelegateMainFrameScroll:
        options.add(DelegateMainFrameScroll);
        break;
    }
    setSelection(newSelection, options, newIntent);
}

void DragCaretController::setCaretPosition(const VisiblePosition& position)
{
    if (Node* node = m_position.deepEquivalent().deprecatedNode())
        invalidateCaretRect(node);
    m_position = position;
    setCaretRectNeedsUpdate();
    Document* document = nullptr;
    if (Node* node = m_position.deepEquivalent().deprecatedNode()) {
        invalidateCaretRect(node);
        document = &node->document();
    }
    if (m_position.isNull() || m_position.isOrphan())
        clearCaretRect();
    else
        updateCaretRect(*document, m_position);
}

static void adjustEndpointsAtBidiBoundary(VisiblePosition& visibleBase, VisiblePosition& visibleExtent)
{
    RenderedPosition base(visibleBase);
    RenderedPosition extent(visibleExtent);

    if (base.isNull() || extent.isNull() || base.isEquivalent(extent))
        return;

    if (base.atLeftBoundaryOfBidiRun()) {
        if (!extent.atRightBoundaryOfBidiRun(base.bidiLevelOnRight())
            && base.isEquivalent(extent.leftBoundaryOfBidiRun(base.bidiLevelOnRight()))) {
            visibleBase = base.positionAtLeftBoundaryOfBiDiRun();
            return;
        }
        return;
    }

    if (base.atRightBoundaryOfBidiRun()) {
        if (!extent.atLeftBoundaryOfBidiRun(base.bidiLevelOnLeft())
            && base.isEquivalent(extent.rightBoundaryOfBidiRun(base.bidiLevelOnLeft()))) {
            visibleBase = base.positionAtRightBoundaryOfBiDiRun();
            return;
        }
        return;
    }

    if (extent.atLeftBoundaryOfBidiRun() && extent.isEquivalent(base.leftBoundaryOfBidiRun(extent.bidiLevelOnRight()))) {
        visibleExtent = extent.positionAtLeftBoundaryOfBiDiRun();
        return;
    }

    if (extent.atRightBoundaryOfBidiRun() && extent.isEquivalent(base.rightBoundaryOfBidiRun(extent.bidiLevelOnLeft()))) {
        visibleExtent = extent.positionAtRightBoundaryOfBiDiRun();
        return;
    }
}

void FrameSelection::setSelectionByMouseIfDifferent(const VisibleSelection& passedNewSelection, TextGranularity granularity,
    EndPointsAdjustmentMode endpointsAdjustmentMode)
{
    VisibleSelection newSelection = passedNewSelection;
    bool isDirectional = shouldAlwaysUseDirectionalSelection(m_document.get()) || newSelection.isDirectional();

    VisiblePosition base = m_originalBase.isNotNull() ? m_originalBase : newSelection.visibleBase();
    VisiblePosition newBase = base;
    VisiblePosition extent = newSelection.visibleExtent();
    VisiblePosition newExtent = extent;
    if (endpointsAdjustmentMode == EndPointsAdjustmentMode::AdjustAtBidiBoundary)
        adjustEndpointsAtBidiBoundary(newBase, newExtent);

    if (newBase != base || newExtent != extent) {
        m_originalBase = base;
        newSelection.setBase(newBase);
        newSelection.setExtent(newExtent);
    } else if (m_originalBase.isNotNull()) {
        if (m_selection.base() == newSelection.base())
            newSelection.setBase(m_originalBase);
        m_originalBase = { };
    }

    newSelection.setIsDirectional(isDirectional); // Adjusting base and extent will make newSelection always directional
    if (m_selection == newSelection || !shouldChangeSelection(newSelection))
        return;
    
    AXTextStateChangeIntent intent;
    if (AXObjectCache::accessibilityEnabled() && newSelection.isCaret())
        intent = AXTextStateChangeIntent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, false });
    else
        intent = AXTextStateChangeIntent();
    setSelection(newSelection, defaultSetSelectionOptions() | FireSelectEvent, intent, AlignCursorOnScrollIfNeeded, granularity);
}

bool FrameSelection::setSelectionWithoutUpdatingAppearance(const VisibleSelection& newSelectionPossiblyWithoutDirection, OptionSet<SetSelectionOption> options, CursorAlignOnScroll align, TextGranularity granularity)
{
    bool closeTyping = options.contains(CloseTyping);
    bool shouldClearTypingStyle = options.contains(ClearTypingStyle);

    VisibleSelection newSelection = newSelectionPossiblyWithoutDirection;
    if (shouldAlwaysUseDirectionalSelection(m_document.get()))
        newSelection.setIsDirectional(true);

    // <http://bugs.webkit.org/show_bug.cgi?id=23464>: Infinite recursion at FrameSelection::setSelection
    // if document->frame() == m_document->frame() we can get into an infinite loop
    if (Document* newSelectionDocument = newSelection.base().document()) {
        if (RefPtr<Frame> newSelectionFrame = newSelectionDocument->frame()) {
            if (m_document && newSelectionFrame != m_document->frame() && newSelectionDocument != m_document) {
                newSelectionDocument->selection().setSelection(newSelection, options, AXTextStateChangeIntent(), align, granularity);
                // It's possible that during the above set selection, this FrameSelection has been modified by
                // selectFrameElementInParentIfFullySelected, but that the selection is no longer valid since
                // the frame is about to be destroyed. If this is the case, clear our selection.
                if (newSelectionFrame->hasOneRef() && m_selection.isNoneOrOrphaned())
                    clear();
                return false;
            }
        }
    }

    VisibleSelection oldSelection = m_selection;
    bool willMutateSelection = oldSelection != newSelection;
    if (willMutateSelection && m_document)
        m_document->editor().selectionWillChange();

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (newSelection.isOrphan()) {
            ASSERT_NOT_REACHED();
            clear();
            return false;
        }

        if (!m_document || (!m_document->frame() && !newSelection.document())) {
            m_selection = newSelection;
            updateAssociatedLiveRange();
            return false;
        }

        bool selectionEndpointsBelongToMultipleDocuments = newSelection.base().document() && !newSelection.document();
        bool selectionIsInAnotherDocument = newSelection.document() && newSelection.document() != m_document.get();
        bool selectionIsInDetachedDocument = newSelection.document() && !newSelection.document()->frame();
        if (selectionEndpointsBelongToMultipleDocuments || selectionIsInAnotherDocument || selectionIsInDetachedDocument) {
            clear();
            return false;
        }
        ASSERT(m_document->frame());

        if (closeTyping)
            TypingCommand::closeTyping(*m_document);

        if (shouldClearTypingStyle)
            clearTypingStyle();

        m_granularity = granularity;
        m_selection = newSelection;
        updateAssociatedLiveRange();
    }

    // Selection offsets should increase when LF is inserted before the caret in InsertLineBreakCommand. See <https://webkit.org/b/56061>.
    if (HTMLTextFormControlElement* textControl = enclosingTextFormControl(newSelection.start()))
        textControl->selectionChanged(options.contains(FireSelectEvent));

    if (!willMutateSelection)
        return false;

    setCaretRectNeedsUpdate();

    if (!newSelection.isNone() && !(options & DoNotSetFocus)) {
        auto* oldFocusedElement = m_document->focusedElement();
        setFocusedElementIfNeeded();
        if (!m_document->frame())
            return false;
        // FIXME: Should not be needed.
        if (m_document->focusedElement() != oldFocusedElement)
            m_document->updateStyleIfNeeded();
    }

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = std::nullopt;
    selectFrameElementInParentIfFullySelected();
    m_document->editor().respondToChangedSelection(oldSelection, options);
    // https://www.w3.org/TR/selection-api/#selectionchange-event
    // FIXME: Spec doesn't specify which task source to use.
    m_document->queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(eventNames().selectionchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));

    return true;
}

void FrameSelection::setSelection(const VisibleSelection& selection, OptionSet<SetSelectionOption> options, AXTextStateChangeIntent intent, CursorAlignOnScroll align, TextGranularity granularity)
{
    LOG_WITH_STREAM(Selection, stream << "FrameSelection::setSelection " << selection);

    RefPtr protectedDocument { m_document.get() };
    if (!setSelectionWithoutUpdatingAppearance(selection, options, align, granularity))
        return;

    if (options & RevealSelectionUpToMainFrame)
        m_selectionRevealMode = SelectionRevealMode::RevealUpToMainFrame;
    else if (options & RevealSelection)
        m_selectionRevealMode = SelectionRevealMode::Reveal;
    else if (options & DelegateMainFrameScroll)
        m_selectionRevealMode = SelectionRevealMode::DelegateMainFrameScroll;
    else
        m_selectionRevealMode = SelectionRevealMode::DoNotReveal;
    m_alwaysAlignCursorOnScrollWhenRevealingSelection = align == AlignCursorOnScrollAlways;

    m_selectionRevealIntent = intent;
    m_pendingSelectionUpdate = true;

    if (protectedDocument->hasPendingStyleRecalc())
        return;

    auto frameView = protectedDocument->view();
    if (frameView && frameView->layoutContext().isLayoutPending())
        return;

    if (!(options & IsUserTriggered)) {
        scheduleAppearanceUpdateAfterStyleChange();
        return;
    }

    updateAndRevealSelection(intent, options.contains(SmoothScroll) ? ScrollBehavior::Smooth : ScrollBehavior::Instant, options.contains(RevealSelectionBounds) ? RevealExtentOption::DoNotRevealExtent : RevealExtentOption::RevealExtent, options.contains(ForceCenterScroll) ? ForceCenterScrollOption::ForceCenterScroll : ForceCenterScrollOption::DoNotForceCenterScroll);

    if (options & IsUserTriggered) {
        if (auto* client = protectedDocument->editor().client())
            client->didEndUserTriggeredSelectionChanges();
    }
}

void FrameSelection::updateSelectionAppearanceNow()
{
    if (!m_document || !m_document->hasLivingRenderTree())
        return;

    Ref document = *m_document;
#if ENABLE(TEXT_CARET)
    document->updateLayoutIgnorePendingStylesheets();
#else
    document->updateStyleIfNeeded();
#endif
    if (m_pendingSelectionUpdate)
        updateAppearance();
}

void FrameSelection::setNeedsSelectionUpdate(RevealSelectionAfterUpdate revealMode)
{
    m_selectionRevealIntent = AXTextStateChangeIntent();
    if (revealMode == RevealSelectionAfterUpdate::Forced)
        m_selectionRevealMode = SelectionRevealMode::Reveal;
    m_pendingSelectionUpdate = true;
    if (RenderView* view = m_document->renderView())
        view->selection().clear();
}

void FrameSelection::updateAndRevealSelection(const AXTextStateChangeIntent& intent, ScrollBehavior scrollBehavior, RevealExtentOption revealExtent, ForceCenterScrollOption forceCenterScroll)
{
    if (!m_pendingSelectionUpdate)
        return;

    m_pendingSelectionUpdate = false;

    updateAppearance();

    if (m_selectionRevealMode != SelectionRevealMode::DoNotReveal) {
        ScrollAlignment alignment;

        if (m_document->editor().behavior().shouldCenterAlignWhenSelectionIsRevealed())
            alignment = m_alwaysAlignCursorOnScrollWhenRevealingSelection ? ScrollAlignment::alignCenterAlways : ScrollAlignment::alignCenterIfNeeded;
        else
            alignment = m_alwaysAlignCursorOnScrollWhenRevealingSelection ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignToEdgeIfNeeded;
        
        if (forceCenterScroll == ForceCenterScrollOption::ForceCenterScroll)
            alignment = ScrollAlignment::alignCenterAlways;

        revealSelection(m_selectionRevealMode, alignment, revealExtent, scrollBehavior);
    }
    if (!m_document->editor().ignoreSelectionChanges())
        notifyAccessibilityForSelectionChange(intent);
}

void FrameSelection::updateDataDetectorsForSelection()
{
#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS_FAMILY)
    m_document->editor().scanSelectionForTelephoneNumbers();
#endif
}

static bool removingNodeRemovesPosition(Node& node, const Position& position)
{
    if (!position.anchorNode())
        return false;

    if (position.anchorNode() == &node)
        return true;

    if (!is<Element>(node))
        return false;

    return downcast<Element>(node).containsIncludingShadowDOM(position.anchorNode());
}

void DragCaretController::nodeWillBeRemoved(Node& node)
{
    if (!hasCaret() || !node.isConnected())
        return;

    if (!removingNodeRemovesPosition(node, m_position.deepEquivalent()))
        return;

    if (RenderView* view = node.document().renderView())
        view->selection().clear();

    // It's important to avoid updating style or layout here, since we're in the middle of removing the node from the document.
    clearCaretPositionWithoutUpdatingStyle();
}

void DragCaretController::clearCaretPositionWithoutUpdatingStyle()
{
    if (RefPtr node = m_position.deepEquivalent().anchorNode())
        invalidateCaretRect(node.get(), true);

    m_position = { };
    clearCaretRect();
}

void FrameSelection::nodeWillBeRemoved(Node& node)
{
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if ((isNone() && !m_document->settings().liveRangeSelectionEnabled()) || !node.isConnected())
        return;

    respondToNodeModification(node, removingNodeRemovesPosition(node, m_selection.anchor()), removingNodeRemovesPosition(node, m_selection.focus()),
        removingNodeRemovesPosition(node, m_selection.base()), removingNodeRemovesPosition(node, m_selection.extent()),
        removingNodeRemovesPosition(node, m_selection.start()), removingNodeRemovesPosition(node, m_selection.end()));
        
    if (UNLIKELY(node.contains(m_previousCaretNode.get()))) {
        m_previousCaretNode = m_selection.start().anchorNode();
        setCaretRectNeedsUpdate();
    }
}

void FrameSelection::respondToNodeModification(Node& node, bool anchorRemoved, bool focusRemoved, bool baseRemoved, bool extentRemoved, bool startRemoved, bool endRemoved)
{
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (m_document->settings().liveRangeSelectionEnabled() && (anchorRemoved || focusRemoved)) {
        Position anchor = m_selection.anchor();
        Position focus = m_selection.focus();
        if (anchorRemoved)
            updatePositionForNodeRemoval(anchor, node);
        if (focusRemoved)
            updatePositionForNodeRemoval(focus, node);

        if (anchor.isNotNull() && focus.isNotNull())
            m_selection.setWithoutValidation(anchor, focus);
        else
            clearDOMTreeSelection = true;

        clearRenderTreeSelection = true;
    } if (startRemoved || endRemoved) {
        Position start = m_selection.start();
        Position end = m_selection.end();
        if (startRemoved)
            updatePositionForNodeRemoval(start, node);
        if (endRemoved)
            updatePositionForNodeRemoval(end, node);

        if (start.isNotNull() && end.isNotNull()) {
            if (m_selection.isBaseFirst())
                m_selection.setWithoutValidation(start, end);
            else
                m_selection.setWithoutValidation(end, start);
        } else
            clearDOMTreeSelection = true;

        clearRenderTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        if (m_selection.isBaseFirst())
            m_selection.setWithoutValidation(m_selection.start(), m_selection.end());
        else
            m_selection.setWithoutValidation(m_selection.end(), m_selection.start());
    } else if (isRange()) {
        if (auto range = m_selection.firstRange(); range && intersects<ComposedTree>(*range, node)) {
            // If we did nothing here, when this node's renderer was destroyed, the rect that it
            // occupied would be invalidated, but, selection gaps that change as a result of
            // the removal wouldn't be invalidated.
            // FIXME: Don't do so much unnecessary invalidation.
            clearRenderTreeSelection = true;
        }
    }

    if (clearRenderTreeSelection) {
        if (auto* renderView = node.document().renderView()) {
            renderView->selection().clear();

            // Trigger a selection update so the selection will be set again.
            m_selectionRevealIntent = AXTextStateChangeIntent();
            m_pendingSelectionUpdate = true;
            renderView->frameView().scheduleSelectionUpdate();
        }
    }

    if (clearDOMTreeSelection)
        setSelection(VisibleSelection(), DoNotSetFocus);
}

static void updatePositionAfterAdoptingTextReplacement(Position& position, CharacterData& node, unsigned offset, unsigned oldLength, unsigned newLength)
{
    if (position.anchorNode() != &node || position.anchorType() != Position::PositionIsOffsetInAnchor)
        return;

    // See: http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
    ASSERT(position.offsetInContainerNode() >= 0);
    unsigned positionOffset = static_cast<unsigned>(position.offsetInContainerNode());
    // Replacing text can be viewed as a deletion followed by insertion.
    if (positionOffset >= offset && positionOffset <= offset + oldLength)
        position.moveToOffset(offset);

    // Adjust the offset if the position is after the end of the deleted contents
    // (positionOffset > offset + oldLength) to avoid having a stale offset.
    if (positionOffset > offset + oldLength)
        position.moveToOffset(positionOffset - oldLength + newLength);

    ASSERT(static_cast<unsigned>(position.offsetInContainerNode()) <= node.length());
}

void FrameSelection::textWasReplaced(CharacterData& node, unsigned offset, unsigned oldLength, unsigned newLength)
{
    if (isNone() || !node.isConnected())
        return;

    Position base = m_selection.base();
    Position extent = m_selection.extent();
    Position start = m_selection.start();
    Position end = m_selection.end();
    updatePositionAfterAdoptingTextReplacement(base, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(extent, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(start, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(end, node, offset, oldLength, newLength);

    if (base != m_selection.base() || extent != m_selection.extent() || start != m_selection.start() || end != m_selection.end()) {
        VisibleSelection newSelection;
        if (base != extent)
            newSelection.setWithoutValidation(base, extent);
        else if (m_selection.isDirectional() && !m_selection.isBaseFirst())
            newSelection.setWithoutValidation(end, start);
        else
            newSelection.setWithoutValidation(start, end);

        setSelection(newSelection, DoNotSetFocus);
    }
}

TextDirection FrameSelection::directionOfEnclosingBlock()
{
    return WebCore::directionOfEnclosingBlock(m_selection.extent());
}

TextDirection FrameSelection::directionOfSelection()
{
    // Get bot VisiblePositions first because visibleStart() and visibleEnd()
    // can cause layout, which has the potential to invalidate lineboxes.
    auto startPosition = m_selection.visibleStart();
    auto endPosition = m_selection.visibleEnd();
    auto startBox = startPosition.inlineBoxAndOffset().box;
    auto endBox = endPosition.inlineBoxAndOffset().box;
    if (startBox && endBox && startBox->direction() == endBox->direction())
        return startBox->direction();
    return directionOfEnclosingBlock();
}

void FrameSelection::willBeModified(EAlteration alter, SelectionDirection direction)
{
    if (alter != AlterationExtend)
        return;

    Position start = m_selection.start();
    Position end = m_selection.end();

    bool baseIsStart = true;

    if (m_selection.isDirectional()) {
        // Make base and extent match start and end so we extend the user-visible selection.
        // This only matters for cases where base and extend point to different positions than
        // start and end (e.g. after a double-click to select a word).
        if (m_selection.isBaseFirst())
            baseIsStart = true;
        else
            baseIsStart = false;
    } else {
        switch (direction) {
        case SelectionDirection::Right:
            if (directionOfSelection() == TextDirection::LTR)
                baseIsStart = true;
            else
                baseIsStart = false;
            break;
        case SelectionDirection::Forward:
            baseIsStart = true;
            break;
        case SelectionDirection::Left:
            if (directionOfSelection() == TextDirection::LTR)
                baseIsStart = false;
            else
                baseIsStart = true;
            break;
        case SelectionDirection::Backward:
            baseIsStart = false;
            break;
        }
    }
    if (baseIsStart) {
        m_selection.setBase(start);
        m_selection.setExtent(end);
    } else {
        m_selection.setBase(end);
        m_selection.setExtent(start);
    }
}

VisiblePosition FrameSelection::positionForPlatform(bool isGetStart) const
{
    // FIXME: VisibleSelection should be fixed to ensure as an invariant that
    // base/extent always point to the same nodes as start/end, but which points
    // to which depends on the value of isBaseFirst. Then this can be changed
    // to just return m_sel.extent().
    if (m_document && m_document->editor().behavior().shouldAlwaysExtendSelectionFromExtentEndpoint())
        return m_selection.isBaseFirst() ? m_selection.visibleEnd() : m_selection.visibleStart();

    return isGetStart ? m_selection.visibleStart() : m_selection.visibleEnd();
}

VisiblePosition FrameSelection::startForPlatform() const
{
    return positionForPlatform(true);
}

VisiblePosition FrameSelection::endForPlatform() const
{
    return positionForPlatform(false);
}

VisiblePosition FrameSelection::nextWordPositionForPlatform(const VisiblePosition& originalPosition)
{
    VisiblePosition positionAfterCurrentWord = nextWordPosition(originalPosition);

    if (m_document && m_document->editor().behavior().shouldSkipSpaceWhenMovingRight()) {
        // In order to skip spaces when moving right, we advance one
        // word further and then move one word back. Given the
        // semantics of previousWordPosition() this will put us at the
        // beginning of the word following.
        VisiblePosition positionAfterSpacingAndFollowingWord = nextWordPosition(positionAfterCurrentWord);
        if (positionAfterSpacingAndFollowingWord != positionAfterCurrentWord)
            positionAfterCurrentWord = previousWordPosition(positionAfterSpacingAndFollowingWord);

        bool movingBackwardsMovedPositionToStartOfCurrentWord = positionAfterCurrentWord == previousWordPosition(nextWordPosition(originalPosition));
        if (movingBackwardsMovedPositionToStartOfCurrentWord)
            positionAfterCurrentWord = positionAfterSpacingAndFollowingWord;
    }
    return positionAfterCurrentWord;
}

static void adjustPositionForUserSelectAll(VisiblePosition& pos, bool isForward)
{
    if (Node* rootUserSelectAll = Position::rootUserSelectAllForNode(pos.deepEquivalent().anchorNode()))
        pos = isForward ? positionAfterNode(rootUserSelectAll).downstream(CanCrossEditingBoundary) : positionBeforeNode(rootUserSelectAll).upstream(CanCrossEditingBoundary);
}

VisiblePosition FrameSelection::modifyExtendingRight(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingRight and modifyExtendingForward is:
    // modifyExtendingForward always extends forward logically.
    // modifyExtendingRight behaves the same as modifyExtendingForward except for extending character or word,
    // it extends forward logically if the enclosing block is TextDirection::LTR,
    // but it extends backward logically if the enclosing block is TextDirection::RTL.
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = pos.next(CannotCrossEditingBoundary);
        else
            pos = pos.previous(CannotCrossEditingBoundary);
        break;
    case TextGranularity::WordGranularity:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = nextWordPositionForPlatform(pos);
        else
            pos = previousWordPosition(pos);
        break;
    case TextGranularity::LineBoundary:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = modifyExtendingForward(granularity);
        else
            pos = modifyExtendingBackward(granularity);
        break;
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        // FIXME: implement all of the above?
        pos = modifyExtendingForward(granularity);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    adjustPositionForUserSelectAll(pos, directionOfEnclosingBlock() == TextDirection::LTR);
    return pos;
}

VisiblePosition FrameSelection::modifyExtendingForward(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        pos = pos.next(CannotCrossEditingBoundary);
        break;
    case TextGranularity::WordGranularity:
        pos = nextWordPositionForPlatform(pos);
        break;
    case TextGranularity::SentenceGranularity:
        pos = nextSentencePosition(pos);
        break;
    case TextGranularity::LineGranularity:
        pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(Extent));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = nextParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(Extent));
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    case TextGranularity::SentenceBoundary:
        pos = endOfSentence(endForPlatform());
        break;
    case TextGranularity::LineBoundary:
        pos = logicalEndOfLine(endForPlatform());
        break;
    case TextGranularity::ParagraphBoundary:
        pos = endOfParagraph(endForPlatform());
        break;
    case TextGranularity::DocumentBoundary:
        pos = endForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = endOfEditableContent(pos);
        else
            pos = endOfDocument(pos);
        break;
    }
    adjustPositionForUserSelectAll(pos, directionOfEnclosingBlock() == TextDirection::LTR);
    return pos;
}

VisiblePosition FrameSelection::modifyMovingRight(TextGranularity granularity, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
    VisiblePosition pos;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (isRange()) {
            if (directionOfSelection() == TextDirection::LTR)
                pos = VisiblePosition(m_selection.end(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.start(), m_selection.affinity());
        } else
            pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).right(true, reachedBoundary);
        break;
    case TextGranularity::WordGranularity: {
        bool skipsSpaceWhenMovingRight = m_document && m_document->editor().behavior().shouldSkipSpaceWhenMovingRight();
        VisiblePosition currentPosition(m_selection.extent(), m_selection.affinity());
        pos = rightWordPosition(currentPosition, skipsSpaceWhenMovingRight);
        if (reachedBoundary)
            *reachedBoundary = pos == currentPosition;
        break;
    }
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        // FIXME: Implement all of the above.
        pos = modifyMovingForward(granularity, reachedBoundary);
        break;
    case TextGranularity::LineBoundary:
        pos = rightBoundaryOfLine(startForPlatform(), directionOfEnclosingBlock(), reachedBoundary);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    return pos;
}

VisiblePosition FrameSelection::modifyMovingForward(TextGranularity granularity, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
    VisiblePosition currentPosition;
    switch (granularity) {
    case TextGranularity::WordGranularity:
    case TextGranularity::SentenceGranularity:
        currentPosition = VisiblePosition(m_selection.extent(), m_selection.affinity());
        break;
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        currentPosition = endForPlatform();
        break;
    default:
        break;
    }
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (isRange())
            pos = VisiblePosition(m_selection.end(), m_selection.affinity());
        else
            pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).next(CannotCrossEditingBoundary, reachedBoundary);
        break;
    case TextGranularity::WordGranularity:
        pos = nextWordPositionForPlatform(currentPosition);
        break;
    case TextGranularity::SentenceGranularity:
        pos = nextSentencePosition(currentPosition);
        break;
    case TextGranularity::LineGranularity: {
        // down-arrowing from a range selection that ends at the start of a line needs
        // to leave the selection at that line start (no need to call nextLinePosition!)
        pos = currentPosition;
        if (!isRange() || !isStartOfLine(pos))
            pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(Start));
        break;
    }
    case TextGranularity::ParagraphGranularity:
        pos = nextParagraphPosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(Start));
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    case TextGranularity::SentenceBoundary:
        pos = endOfSentence(currentPosition);
        break;
    case TextGranularity::LineBoundary:
        pos = logicalEndOfLine(endForPlatform(), reachedBoundary);
        break;
    case TextGranularity::ParagraphBoundary:
        pos = endOfParagraph(currentPosition);
        break;
    case TextGranularity::DocumentBoundary:
        pos = currentPosition;
        if (isEditablePosition(pos.deepEquivalent()))
            pos = endOfEditableContent(pos);
        else
            pos = endOfDocument(pos);
        break;
    }
    switch (granularity) {
    case TextGranularity::WordGranularity:
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        if (reachedBoundary)
            *reachedBoundary = pos == currentPosition;
        break;
    default:
        break;
    }
    return pos;
}

VisiblePosition FrameSelection::modifyExtendingLeft(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingLeft and modifyExtendingBackward is:
    // modifyExtendingBackward always extends backward logically.
    // modifyExtendingLeft behaves the same as modifyExtendingBackward except for extending character or word,
    // it extends backward logically if the enclosing block is TextDirection::LTR,
    // but it extends forward logically if the enclosing block is TextDirection::RTL.
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = pos.previous(CannotCrossEditingBoundary);
        else
            pos = pos.next(CannotCrossEditingBoundary);
        break;
    case TextGranularity::WordGranularity:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = previousWordPosition(pos);
        else
            pos = nextWordPositionForPlatform(pos);
        break;
    case TextGranularity::LineBoundary:
        if (directionOfEnclosingBlock() == TextDirection::LTR)
            pos = modifyExtendingBackward(granularity);
        else
            pos = modifyExtendingForward(granularity);
        break;
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        pos = modifyExtendingBackward(granularity);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    adjustPositionForUserSelectAll(pos, !(directionOfEnclosingBlock() == TextDirection::LTR));
    return pos;
}
       
VisiblePosition FrameSelection::modifyExtendingBackward(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

    // Extending a selection backward by word or character from just after a table selects
    // the table.  This "makes sense" from the user perspective, esp. when deleting.
    // It was done here instead of in VisiblePosition because we want VPs to iterate
    // over everything.
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        pos = pos.previous(CannotCrossEditingBoundary);
        break;
    case TextGranularity::WordGranularity:
        pos = previousWordPosition(pos);
        break;
    case TextGranularity::SentenceGranularity:
        pos = previousSentencePosition(pos);
        break;
    case TextGranularity::LineGranularity:
        pos = previousLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(Extent));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = previousParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(Extent));
        break;
    case TextGranularity::SentenceBoundary:
        pos = startOfSentence(startForPlatform());
        break;
    case TextGranularity::LineBoundary:
        pos = logicalStartOfLine(startForPlatform());
        break;
    case TextGranularity::ParagraphBoundary:
        pos = startOfParagraph(startForPlatform());
        break;
    case TextGranularity::DocumentBoundary:
        pos = startForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = startOfEditableContent(pos);
        else
            pos = startOfDocument(pos);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    adjustPositionForUserSelectAll(pos, !(directionOfEnclosingBlock() == TextDirection::LTR));
    return pos;
}

VisiblePosition FrameSelection::modifyMovingLeft(TextGranularity granularity, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
    VisiblePosition pos;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (isRange())
            if (directionOfSelection() == TextDirection::LTR)
                pos = VisiblePosition(m_selection.start(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.end(), m_selection.affinity());
        else
            pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).left(true, reachedBoundary);
        break;
    case TextGranularity::WordGranularity: {
        bool skipsSpaceWhenMovingRight = m_document && m_document->editor().behavior().shouldSkipSpaceWhenMovingRight();
        VisiblePosition currentPosition(m_selection.extent(), m_selection.affinity());
        pos = leftWordPosition(currentPosition, skipsSpaceWhenMovingRight);
        if (reachedBoundary)
            *reachedBoundary = pos == currentPosition;
        break;
    }
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        // FIXME: Implement all of the above.
        pos = modifyMovingBackward(granularity, reachedBoundary);
        break;
    case TextGranularity::LineBoundary:
        pos = leftBoundaryOfLine(startForPlatform(), directionOfEnclosingBlock(), reachedBoundary);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    return pos;
}

VisiblePosition FrameSelection::modifyMovingBackward(TextGranularity granularity, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
    VisiblePosition currentPosition;
    switch (granularity) {
    case TextGranularity::WordGranularity:
    case TextGranularity::SentenceGranularity:
        currentPosition = VisiblePosition(m_selection.extent(), m_selection.affinity());
        break;
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        currentPosition = startForPlatform();
        break;
    default:
        break;
    }
    VisiblePosition pos;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        if (isRange())
            pos = VisiblePosition(m_selection.start(), m_selection.affinity());
        else
            pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).previous(CannotCrossEditingBoundary, reachedBoundary);
        break;
    case TextGranularity::WordGranularity:
        pos = previousWordPosition(currentPosition);
        break;
    case TextGranularity::SentenceGranularity:
        pos = previousSentencePosition(currentPosition);
        break;
    case TextGranularity::LineGranularity:
        pos = previousLinePosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(Start));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = previousParagraphPosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(Start));
        break;
    case TextGranularity::SentenceBoundary:
        pos = startOfSentence(currentPosition);
        break;
    case TextGranularity::LineBoundary:
        pos = logicalStartOfLine(startForPlatform(), reachedBoundary);
        break;
    case TextGranularity::ParagraphBoundary:
        pos = startOfParagraph(currentPosition);
        break;
    case TextGranularity::DocumentBoundary:
        pos = currentPosition;
        if (isEditablePosition(pos.deepEquivalent()))
            pos = startOfEditableContent(pos);
        else
            pos = startOfDocument(pos);
        break;
    case TextGranularity::DocumentGranularity:
        ASSERT_NOT_REACHED();
        break;
    }
    switch (granularity) {
    case TextGranularity::WordGranularity:
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        if (reachedBoundary)
            *reachedBoundary = pos == currentPosition;
        break;
    default:
        break;
    }
    return pos;
}

static bool isBoundary(TextGranularity granularity)
{
    return granularity == TextGranularity::LineBoundary || granularity == TextGranularity::ParagraphBoundary || granularity == TextGranularity::DocumentBoundary;
}

AXTextStateChangeIntent FrameSelection::textSelectionIntent(EAlteration alter, SelectionDirection direction, TextGranularity granularity)
{
    AXTextStateChangeIntent intent = AXTextStateChangeIntent();
    bool flip = false;
    if (alter == FrameSelection::AlterationMove) {
        intent.type = AXTextStateChangeTypeSelectionMove;
        flip = isRange() && directionOfSelection() == TextDirection::RTL;
    } else
        intent.type = AXTextStateChangeTypeSelectionExtend;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        intent.selection.granularity = AXTextSelectionGranularityCharacter;
        break;
    case TextGranularity::WordGranularity:
        intent.selection.granularity = AXTextSelectionGranularityWord;
        break;
    case TextGranularity::SentenceGranularity:
    case TextGranularity::SentenceBoundary:
        intent.selection.granularity = AXTextSelectionGranularitySentence;
        break;
    case TextGranularity::LineGranularity:
    case TextGranularity::LineBoundary:
        intent.selection.granularity = AXTextSelectionGranularityLine;
        break;
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::ParagraphBoundary:
        intent.selection.granularity = AXTextSelectionGranularityParagraph;
        break;
    case TextGranularity::DocumentGranularity:
    case TextGranularity::DocumentBoundary:
        intent.selection.granularity = AXTextSelectionGranularityDocument;
        break;
    }
    bool boundary = false;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
    case TextGranularity::WordGranularity:
    case TextGranularity::SentenceGranularity:
    case TextGranularity::LineGranularity:
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::DocumentGranularity:
        break;
    case TextGranularity::SentenceBoundary:
    case TextGranularity::LineBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        boundary = true;
        break;
    }
    switch (direction) {
    case SelectionDirection::Right:
    case SelectionDirection::Forward:
        if (boundary)
            intent.selection.direction = flip ? AXTextSelectionDirectionBeginning : AXTextSelectionDirectionEnd;
        else
            intent.selection.direction = flip ? AXTextSelectionDirectionPrevious : AXTextSelectionDirectionNext;
        break;
    case SelectionDirection::Left:
    case SelectionDirection::Backward:
        if (boundary)
            intent.selection.direction = flip ? AXTextSelectionDirectionEnd : AXTextSelectionDirectionBeginning;
        else
            intent.selection.direction = flip ? AXTextSelectionDirectionNext : AXTextSelectionDirectionPrevious;
        break;
    }
    return intent;
}

static AXTextSelection textSelectionWithDirectionAndGranularity(SelectionDirection direction, TextGranularity granularity)
{
    // FIXME: Account for BIDI in SelectionDirection::Right & SelectionDirection::Left. (In a RTL block, Right would map to Previous/Beginning and Left to Next/End.)
    AXTextSelectionDirection intentDirection = AXTextSelectionDirectionUnknown;
    switch (direction) {
    case SelectionDirection::Forward:
        intentDirection = AXTextSelectionDirectionNext;
        break;
    case SelectionDirection::Right:
        intentDirection = AXTextSelectionDirectionNext;
        break;
    case SelectionDirection::Backward:
        intentDirection = AXTextSelectionDirectionPrevious;
        break;
    case SelectionDirection::Left:
        intentDirection = AXTextSelectionDirectionPrevious;
        break;
    }
    AXTextSelectionGranularity intentGranularity = AXTextSelectionGranularityUnknown;
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        intentGranularity = AXTextSelectionGranularityCharacter;
        break;
    case TextGranularity::WordGranularity:
        intentGranularity = AXTextSelectionGranularityWord;
        break;
    case TextGranularity::SentenceGranularity:
    case TextGranularity::SentenceBoundary: // FIXME: Boundary should affect direction.
        intentGranularity = AXTextSelectionGranularitySentence;
        break;
    case TextGranularity::LineGranularity:
        intentGranularity = AXTextSelectionGranularityLine;
        break;
    case TextGranularity::ParagraphGranularity:
    case TextGranularity::ParagraphBoundary: // FIXME: Boundary should affect direction.
        intentGranularity = AXTextSelectionGranularityParagraph;
        break;
    case TextGranularity::DocumentGranularity:
    case TextGranularity::DocumentBoundary: // FIXME: Boundary should affect direction.
        intentGranularity = AXTextSelectionGranularityDocument;
        break;
    case TextGranularity::LineBoundary:
        intentGranularity = AXTextSelectionGranularityLine;
        switch (direction) {
        case SelectionDirection::Forward:
            intentDirection = AXTextSelectionDirectionEnd;
            break;
        case SelectionDirection::Right:
            intentDirection = AXTextSelectionDirectionEnd;
            break;
        case SelectionDirection::Backward:
            intentDirection = AXTextSelectionDirectionBeginning;
            break;
        case SelectionDirection::Left:
            intentDirection = AXTextSelectionDirectionBeginning;
            break;
        }
        break;
    }
    return { intentDirection, intentGranularity, false };
}

bool FrameSelection::modify(EAlteration alter, SelectionDirection direction, TextGranularity granularity, EUserTriggered userTriggered)
{
    if (userTriggered == UserTriggered) {
        FrameSelection trialFrameSelection;
        trialFrameSelection.setSelection(m_selection);
        trialFrameSelection.modify(alter, direction, granularity, NotUserTriggered);

        bool change = shouldChangeSelection(trialFrameSelection.selection());
        if (!change)
            return false;

        if (trialFrameSelection.selection().isRange() && m_selection.isCaret() && !dispatchSelectStart())
            return false;
    }

    willBeModified(alter, direction);

    // Before modifying selection, update layout and disable post resolution callbacks.
    // That way, unaverted tree changes are avoided while browsing the document.
    auto selectionDocument = m_selection.document();
    if (!selectionDocument)
        return false;
    selectionDocument->updateLayoutIgnorePendingStylesheets();
    Style::PostResolutionCallbackDisabler disabler(*selectionDocument);

    bool reachedBoundary = false;
    bool wasRange = m_selection.isRange();
    Position originalStartPosition = m_selection.start();
    VisiblePosition position;
    switch (direction) {
    case SelectionDirection::Right:
        if (alter == AlterationMove)
            position = modifyMovingRight(granularity, &reachedBoundary);
        else
            position = modifyExtendingRight(granularity);
        break;
    case SelectionDirection::Forward:
        if (alter == AlterationExtend)
            position = modifyExtendingForward(granularity);
        else
            position = modifyMovingForward(granularity, &reachedBoundary);
        break;
    case SelectionDirection::Left:
        if (alter == AlterationMove)
            position = modifyMovingLeft(granularity, &reachedBoundary);
        else
            position = modifyExtendingLeft(granularity);
        break;
    case SelectionDirection::Backward:
        if (alter == AlterationExtend)
            position = modifyExtendingBackward(granularity);
        else
            position = modifyMovingBackward(granularity, &reachedBoundary);
        break;
    }

    if (reachedBoundary && !isRange() && userTriggered == UserTriggered && m_document && AXObjectCache::accessibilityEnabled()) {
        notifyAccessibilityForSelectionChange({ AXTextStateChangeTypeSelectionBoundary, textSelectionWithDirectionAndGranularity(direction, granularity) });
        return true;
    }

    if (position.isNull())
        return false;

    if (m_document && isSpatialNavigationEnabled(m_document->frame())) {
        if (!wasRange && alter == AlterationMove && position == originalStartPosition)
            return false;
    }

    if (m_document && AXObjectCache::accessibilityEnabled()) {
        if (AXObjectCache* cache = m_document->existingAXObjectCache())
            cache->setTextSelectionIntent(textSelectionIntent(alter, direction, granularity));
    }

    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the Start position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    LayoutUnit x = lineDirectionPointForBlockDirectionNavigation(Start);
    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(m_document.get()) || alter == AlterationExtend);

    switch (alter) {
    case AlterationMove:
        moveTo(position, userTriggered);
        break;
    case AlterationExtend:

        if (!m_selection.isCaret()
            && (granularity == TextGranularity::WordGranularity || granularity == TextGranularity::ParagraphGranularity || granularity == TextGranularity::LineGranularity)
            && m_document && !m_document->editor().behavior().shouldExtendSelectionByWordOrLineAcrossCaret()) {
            // Don't let the selection go across the base position directly. Needed to match mac
            // behavior when, for instance, word-selecting backwards starting with the caret in
            // the middle of a word and then word-selecting forward, leaving the caret in the
            // same place where it was, instead of directly selecting to the end of the word.
            VisibleSelection newSelection = m_selection;
            newSelection.setExtent(position);
            if (m_selection.isBaseFirst() != newSelection.isBaseFirst())
                position = m_selection.base();
        }

        // Standard Mac behavior when extending to a boundary is grow the selection rather than leaving the
        // base in place and moving the extent. Matches NSTextView.
        if (!m_document || !m_document->editor().behavior().shouldAlwaysGrowSelectionWhenExtendingToBoundary() || m_selection.isCaret() || !isBoundary(granularity))
            setExtent(position, userTriggered);
        else {
            TextDirection textDirection = directionOfEnclosingBlock();
            if (direction == SelectionDirection::Forward || (textDirection == TextDirection::LTR && direction == SelectionDirection::Right) || (textDirection == TextDirection::RTL && direction == SelectionDirection::Left))
                setEnd(position, userTriggered);
            else
                setStart(position, userTriggered);
        }
        break;
    }
    
    if (granularity == TextGranularity::LineGranularity || granularity == TextGranularity::ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    if (userTriggered == UserTriggered)
        m_granularity = TextGranularity::CharacterGranularity;

    setCaretRectNeedsUpdate();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool absoluteCaretY(const VisiblePosition& c, int& y)
{
    IntRect rect = c.absoluteCaretBounds();
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool FrameSelection::modify(EAlteration alter, unsigned verticalDistance, VerticalDirection direction, EUserTriggered userTriggered, CursorAlignOnScroll align)
{
    if (!verticalDistance)
        return false;

    if (userTriggered == UserTriggered) {
        FrameSelection trialFrameSelection;
        trialFrameSelection.setSelection(m_selection);
        trialFrameSelection.modify(alter, verticalDistance, direction, NotUserTriggered);

        bool change = shouldChangeSelection(trialFrameSelection.selection());
        if (!change)
            return false;
    }

    willBeModified(alter, direction == DirectionUp ? SelectionDirection::Backward : SelectionDirection::Forward);

    VisiblePosition pos;
    LayoutUnit xPos;
    switch (alter) {
    case AlterationMove:
        pos = VisiblePosition(direction == DirectionUp ? m_selection.start() : m_selection.end(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(direction == DirectionUp ? Start : End);
        m_selection.setAffinity(direction == DirectionUp ? Affinity::Upstream : Affinity::Downstream);
        break;
    case AlterationExtend:
        pos = VisiblePosition(m_selection.extent(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(Extent);
        m_selection.setAffinity(Affinity::Downstream);
        break;
    }

    int startY;
    if (!absoluteCaretY(pos, startY))
        return false;
    if (direction == DirectionUp)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        if (direction == DirectionUp)
            next = previousLinePosition(p, xPos);
        else
            next = nextLinePosition(p, xPos);

        if (next.isNull() || next == p)
            break;
        int nextY;
        if (!absoluteCaretY(next, nextY))
            break;
        if (direction == DirectionUp)
            nextY = -nextY;
        if (nextY - startY > static_cast<int>(verticalDistance))
            break;
        if (nextY >= lastY) {
            lastY = nextY;
            result = next;
        }
    }

    if (result.isNull())
        return false;

    switch (alter) {
    case AlterationMove:
        moveTo(result, userTriggered, align);
        break;
    case AlterationExtend:
        setExtent(result, userTriggered);
        break;
    }

    if (userTriggered == UserTriggered)
        m_granularity = TextGranularity::CharacterGranularity;

    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(m_document.get()) || alter == AlterationExtend);

    return true;
}

LayoutUnit FrameSelection::lineDirectionPointForBlockDirectionNavigation(PositionType type)
{
    if (isNone())
        return 0;

    // FIXME: Can we use visibleStart/End/Extent?
    Position position;
    switch (type) {
    case Start:
        position = m_selection.start();
        break;
    case End:
        position = m_selection.end();
        break;
    case Extent:
        position = m_selection.extent();
        break;
    }

    // FIXME: Why is this check needed? What's the harm in doing a little more work without a frame?
    if (!position.anchorNode()->document().frame())
        return 0;

    // FIXME: Can we do this before getting the position from the selection?
    if (m_xPosForVerticalArrowNavigation)
        return *m_xPosForVerticalArrowNavigation;

    // VisiblePosition creation can fail here if a node containing the selection becomes
    // visibility:hidden after the selection is created and before this function is called.
    VisiblePosition visiblePosition(position, m_selection.affinity());
    auto x = visiblePosition.isNotNull() ? visiblePosition.lineDirectionPointForBlockDirectionNavigation() : 0;
    m_xPosForVerticalArrowNavigation = { x };
    return x;
}

void FrameSelection::clear()
{
    m_granularity = TextGranularity::CharacterGranularity;
    setSelection(VisibleSelection());
}

void FrameSelection::willBeRemovedFromFrame()
{
    m_granularity = TextGranularity::CharacterGranularity;

#if ENABLE(TEXT_CARET)
    m_caretBlinkTimer.stop();
#endif

    if (auto* view = m_document->renderView())
        view->selection().clear();

    setSelectionWithoutUpdatingAppearance(VisibleSelection(), defaultSetSelectionOptions(), AlignCursorOnScrollIfNeeded, TextGranularity::CharacterGranularity);
    m_previousCaretNode = nullptr;
    m_typingStyle = nullptr;
    m_appearanceUpdateTimer.stop();
}

void FrameSelection::setStart(const VisiblePosition& position, EUserTriggered trigger)
{
    if (m_selection.isBaseFirst())
        setBase(position, trigger);
    else
        setExtent(position, trigger);
}

void FrameSelection::setEnd(const VisiblePosition& position, EUserTriggered trigger)
{
    if (m_selection.isBaseFirst())
        setExtent(position, trigger);
    else
        setBase(position, trigger);
}

void FrameSelection::setBase(const VisiblePosition& position, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(position.deepEquivalent(), m_selection.extent(), position.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setExtent(const VisiblePosition& position, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(m_selection.base(), position.deepEquivalent(), position.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setBase(const Position& position, Affinity affinity, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(position, m_selection.extent(), affinity, selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setExtent(const Position& position, Affinity affinity, EUserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(m_selection.base(), position, affinity, selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void CaretBase::clearCaretRect()
{
    m_caretLocalRect = LayoutRect();
}

bool CaretBase::updateCaretRect(Document& document, const VisiblePosition& caretPosition)
{
    document.updateLayoutIgnorePendingStylesheets();
    m_caretRectNeedsUpdate = false;
    RenderBlock* renderer;
    m_caretLocalRect = localCaretRectInRendererForCaretPainting(caretPosition, renderer);
    return !m_caretLocalRect.isEmpty();
}

RenderBlock* FrameSelection::caretRendererWithoutUpdatingLayout() const
{
    return rendererForCaretPainting(m_selection.start().deprecatedNode());
}

RenderBlock* DragCaretController::caretRenderer() const
{
    return rendererForCaretPainting(m_position.deepEquivalent().deprecatedNode());
}

static bool isNonOrphanedCaret(const VisibleSelection& selection)
{
    return selection.isCaret() && !selection.start().isOrphan() && !selection.end().isOrphan();
}

IntRect FrameSelection::absoluteCaretBounds(bool* insideFixed)
{
    if (!m_document)
        return IntRect();
    updateSelectionAppearanceNow();
    recomputeCaretRect();
    if (insideFixed)
        *insideFixed = m_caretInsidePositionFixed;
    return m_absCaretBounds;
}

static void repaintCaretForLocalRect(Node* node, const LayoutRect& rect)
{
    if (auto* caretPainter = rendererForCaretPainting(node))
        caretPainter->repaintRectangle(rect);
}

bool FrameSelection::recomputeCaretRect()
{
    if (!shouldUpdateCaretRect())
        return false;

    if (!m_document)
        return false;

    FrameView* v = m_document->view();
    if (!v)
        return false;

    LayoutRect oldRect = localCaretRectWithoutUpdate();

    RefPtr<Node> caretNode = m_previousCaretNode;
    if (shouldUpdateCaretRect()) {
        if (!isNonOrphanedCaret(m_selection))
            clearCaretRect();
        else {
            VisiblePosition visibleStart = m_selection.visibleStart();
            if (updateCaretRect(*m_document, visibleStart)) {
                caretNode = visibleStart.deepEquivalent().deprecatedNode();
                m_absCaretBoundsDirty = true;
            }
        }
    }
    LayoutRect newRect = localCaretRectWithoutUpdate();

    if (caretNode == m_previousCaretNode && oldRect == newRect && !m_absCaretBoundsDirty)
        return false;

    IntRect oldAbsCaretBounds = m_absCaretBounds;
    bool isInsideFixed;
    m_absCaretBounds = absoluteBoundsForLocalCaretRect(rendererForCaretPainting(caretNode.get()), newRect, &isInsideFixed);
    m_caretInsidePositionFixed = isInsideFixed;

    if (m_absCaretBoundsDirty && m_selection.isCaret()) // We should be able to always assert this condition.
        ASSERT(m_absCaretBounds == m_selection.visibleStart().absoluteCaretBounds());

    m_absCaretBoundsDirty = false;

    if (caretNode == m_previousCaretNode && oldAbsCaretBounds == m_absCaretBounds)
        return false;

#if ENABLE(TEXT_CARET)
    if (RenderView* view = m_document->renderView()) {
        bool previousOrNewCaretNodeIsContentEditable = m_selection.isContentEditable() || (m_previousCaretNode && m_previousCaretNode->isContentEditable());
        if (shouldRepaintCaret(view, previousOrNewCaretNodeIsContentEditable)) {
            if (m_previousCaretNode)
                repaintCaretForLocalRect(m_previousCaretNode.get(), oldRect);
            m_previousCaretNode = caretNode;
            repaintCaretForLocalRect(caretNode.get(), newRect);
        }
    }
#endif
    return true;
}

bool CaretBase::shouldRepaintCaret(const RenderView* view, bool isContentEditable) const
{
    ASSERT(view);
    Frame* frame = &view->frameView().frame(); // The frame where the selection started.
    bool caretBrowsing = frame && frame->settings().caretBrowsingEnabled();
    return (caretBrowsing || isContentEditable);
}

void FrameSelection::invalidateCaretRect()
{
    if (!isCaret())
        return;

    CaretBase::invalidateCaretRect(m_selection.start().deprecatedNode(), recomputeCaretRect());
}

void CaretBase::invalidateCaretRect(Node* node, bool caretRectChanged)
{
    // EDIT FIXME: This is an unfortunate hack.
    // Basically, we can't trust this layout position since we 
    // can't guarantee that the check to see if we are in unrendered 
    // content will work at this point. We may have to wait for
    // a layout and re-render of the document to happen. So, resetting this
    // flag will cause another caret layout to happen the first time
    // that we try to paint the caret after this call. That one will work since
    // it happens after the document has accounted for any editing
    // changes which may have been done.
    // And, we need to leave this layout here so the caret moves right 
    // away after clicking.
    m_caretRectNeedsUpdate = true;

    if (caretRectChanged)
        return;

    if (RenderView* view = node->document().renderView()) {
        if (shouldRepaintCaret(view, isEditableNode(*node)))
            repaintCaretForLocalRect(node, localCaretRectWithoutUpdate());
    }
}

void FrameSelection::paintCaret(GraphicsContext& context, const LayoutPoint& paintOffset, const LayoutRect& clipRect)
{
    if (m_selection.isCaret() && m_caretPaint && m_selection.start().deprecatedNode())
        CaretBase::paintCaret(*m_selection.start().deprecatedNode(), context, paintOffset, clipRect);
}

Color CaretBase::computeCaretColor(const RenderStyle& elementStyle, const Node* node)
{
    // On iOS, we want to fall back to the tintColor, and only override if CSS has explicitly specified a custom color.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    UNUSED_PARAM(node);
    return elementStyle.colorResolvingCurrentColor(elementStyle.caretColor());
#else
    RefPtr parentElement = node ? node->parentElement() : nullptr;
    auto* parentStyle = parentElement && parentElement->renderer() ? &parentElement->renderer()->style() : nullptr;
    // CSS value "auto" is treated as an invalid color.
    if (elementStyle.hasAutoCaretColor() && parentStyle) {
        auto parentBackgroundColor = parentStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        auto elementBackgroundColor = elementStyle.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        auto disappearsIntoBackground = blendSourceOver(parentBackgroundColor, elementBackgroundColor) == parentBackgroundColor;
        if (disappearsIntoBackground)
            return parentStyle->visitedDependentColorWithColorFilter(CSSPropertyCaretColor);
    }
    return elementStyle.visitedDependentColorWithColorFilter(CSSPropertyCaretColor);
#endif
}

void CaretBase::paintCaret(const Node& node, GraphicsContext& context, const LayoutPoint& paintOffset, const LayoutRect& clipRect) const
{
#if ENABLE(TEXT_CARET)
    if (m_caretVisibility == Hidden)
        return;

    auto drawingRect = localCaretRectWithoutUpdate();
    if (auto* renderer = rendererForCaretPainting(&node))
        renderer->flipForWritingMode(drawingRect);
    drawingRect.moveBy(paintOffset);
    auto caret = intersection(drawingRect, clipRect);
    if (caret.isEmpty())
        return;

    Color caretColor = Color::black;
    auto* element = is<Element>(node) ? downcast<Element>(&node) : node.parentElement();
    if (element && element->renderer())
        caretColor = CaretBase::computeCaretColor(element->renderer()->style(), &node);

    auto pixelSnappedCaretRect = snapRectToDevicePixels(caret, node.document().deviceScaleFactor());
    context.fillRect(pixelSnappedCaretRect, caretColor);
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(context);
    UNUSED_PARAM(paintOffset);
    UNUSED_PARAM(clipRect);
#endif
}

void FrameSelection::debugRenderer(RenderObject* renderer, bool selected) const
{
    if (is<Element>(*renderer->node())) {
        Element& element = downcast<Element>(*renderer->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element.localName().string().utf8().data());
    } else if (is<RenderText>(*renderer)) {
        RenderText& textRenderer = downcast<RenderText>(*renderer);
        if (textRenderer.text().isEmpty() || !textRenderer.firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        String text = textRenderer.text();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (renderer->node() == m_selection.start().containerNode())
                offset = m_selection.start().computeOffsetInContainerNode();
            else if (renderer->node() == m_selection.end().containerNode())
                offset = m_selection.end().computeOffsetInContainerNode();

            int pos;
            LegacyInlineTextBox* box = textRenderer.findNextInlineTextBox(offset, pos);
            text = text.substring(box->start(), box->len());
            
            String show;
            int mid = max / 2;
            int caret = 0;
            
            // text is shorter than max
            if (textLength < max) {
                show = text;
                caret = pos;
            } else if (pos - mid < 0) {
                // too few characters to left
                show = makeString(StringView(text).left(max - 3), "...");
                caret = pos;
            } else if (pos - mid >= 0 && pos + mid <= textLength) {
                // enough characters on each side
                show = makeString("...", StringView(text).substring(pos - mid + 3, max - 6), "...");
                caret = mid;
            } else {
                // too few characters on right
                show = makeString("...", StringView(text).right(max - 3));
                caret = pos - (textLength - show.length());
            }
            
            show = makeStringByReplacingAll(show, '\n', ' ');
            show = makeStringByReplacingAll(show, '\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.utf8().data(), pos);
            fprintf(stderr, "           ");
            for (int i = 0; i < caret; i++)
                fprintf(stderr, " ");
            fprintf(stderr, "^\n");
        } else {
            if ((int)text.length() > max)
                text = text.left(max - 3) + "...";
            else
                text = text.left(max);
            fprintf(stderr, "    #text : \"%s\"\n", text.utf8().data());
        }
    }
}

bool FrameSelection::contains(const LayoutPoint& point) const
{
    // Treat a collapsed selection like no selection.
    if (!isRange())
        return false;

    auto range = m_selection.firstRange();
    if (!range)
        return false;

    if (!m_document)
        return false;

    HitTestResult result(point);
    m_document->hitTest(HitTestRequest(), result);
    RefPtr innerNode = result.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;

    if (ImageOverlay::isInsideOverlay(*range) && ImageOverlay::isInsideOverlay(*innerNode)) {
        for (auto quad : RenderObject::absoluteTextQuads(*range, { RenderObject::BoundingRectBehavior::UseSelectionHeight })) {
            if (!quad.isEmpty() && quad.containsPoint(point))
                return true;
        }
        return false;
    }

    return WebCore::contains<ComposedTree>(*range, makeBoundaryPoint(innerNode->renderer()->positionForPoint(result.localPoint(), nullptr)));
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void FrameSelection::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    RefPtr document { m_document.get() };
    if (!document)
        return;
    RefPtr frame { document->frame() };
    if (!frame)
        return;
    RefPtr parent { dynamicDowncast<LocalFrame>(frame->tree().parent()) };
    if (!parent)
        return;
    Page* page = m_document->page();
    if (!page)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!isRange())
        return;
    if (!isStartOfDocument(selection().visibleStart()))
        return;
    if (!isEndOfDocument(selection().visibleEnd()))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    RefPtr ownerElement { m_document->ownerElement() };
    if (!ownerElement)
        return;
    RefPtr ownerElementParent { ownerElement->parentNode() };
    if (!ownerElementParent)
        return;
        
    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->hasEditableStyle())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->computeNodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(Position(ownerElementParent.get(), ownerElementNodeIndex, Position::PositionIsOffsetInAnchor)));
    VisiblePosition afterOwnerElement(VisiblePosition(Position(ownerElementParent.get(), ownerElementNodeIndex + 1, Position::PositionIsOffsetInAnchor), Affinity::Upstream));

    // Focus on the parent frame, and then select from before this element to after.
    VisibleSelection newSelection(beforeOwnerElement, afterOwnerElement);
    if (parent->selection().shouldChangeSelection(newSelection) && page) {
        CheckedRef(page->focusController())->setFocusedFrame(parent.get());
        // Previous focus can trigger DOM events, ensure the selection did not become orphan.
        if (newSelection.isOrphan())
            parent->selection().clear();
        else
            parent->selection().setSelection(newSelection);
    }
}

void FrameSelection::selectAll()
{
    Element* focusedElement = m_document->focusedElement();
    if (is<HTMLSelectElement>(focusedElement)) {
        HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*focusedElement);
        if (selectElement.canSelectAll()) {
            selectElement.selectAll();
            return;
        }
    }

    RefPtr<Node> root;
    Node* selectStartTarget = nullptr;
    if (m_selection.isContentEditable()) {
        root = highestEditableRoot(m_selection.start());
        if (Node* shadowRoot = m_selection.nonBoundaryShadowTreeRootNode())
            selectStartTarget = shadowRoot->shadowHost();
        else
            selectStartTarget = root.get();
    } else {
        if (m_selection.isNone() && focusedElement) {
            if (focusedElement->isTextField()) {
                downcast<HTMLTextFormControlElement>(*focusedElement).select();
                return;
            }
            root = focusedElement->nonBoundaryShadowTreeRootNode();
        } else
            root = m_selection.nonBoundaryShadowTreeRootNode();

        if (root)
            selectStartTarget = root->shadowHost();
        else {
            root = m_document->documentElement();
            selectStartTarget = m_document->bodyOrFrameset();
        }
    }
    if (!root)
        return;

    if (selectStartTarget) {
        auto event = Event::create(eventNames().selectstartEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
        selectStartTarget->dispatchEvent(event);
        if (event->defaultPrevented())
            return;
    }

    VisibleSelection newSelection(VisibleSelection::selectionFromContentsOfNode(root.get()));
    if (!newSelection.isOrphan() && shouldChangeSelection(newSelection)) {
        AXTextStateChangeIntent intent(AXTextStateChangeTypeSelectionExtend, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityAll, false });
        setSelection(newSelection, defaultSetSelectionOptions() | FireSelectEvent, intent);
    }
}

bool FrameSelection::setSelectedRange(const std::optional<SimpleRange>& range, Affinity affinity, ShouldCloseTyping closeTyping, EUserTriggered userTriggered)
{
    if (!range)
        return false;

    if (&range->start.document() != &range->end.document())
        return false;

    VisibleSelection newSelection(*range, affinity);

#if PLATFORM(IOS_FAMILY)
    // FIXME: Why do we need this check only in iOS?
    if (newSelection.isNone())
        return false;
#endif

    OptionSet<SetSelectionOption> selectionOptions {  ClearTypingStyle };
    if (closeTyping == ShouldCloseTyping::Yes)
        selectionOptions.add(CloseTyping);

    if (userTriggered == UserTriggered) {
        FrameSelection trialFrameSelection;

        trialFrameSelection.setSelection(newSelection, selectionOptions);

        if (!shouldChangeSelection(trialFrameSelection.selection()))
            return false;

        selectionOptions.add(IsUserTriggered);
    }

    setSelection(newSelection, selectionOptions);
    return true;
}

void FrameSelection::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();

    m_document->updateStyleIfNeeded();

#if USE(UIKIT_EDITING)
    // Caret blinking (blinks | does not blink)
    if (activeAndFocused)
        setSelectionFromNone();
    setCaretVisible(activeAndFocused);
#else
    // Because RenderObject::selectionBackgroundColor() and
    // RenderObject::selectionForegroundColor() check if the frame is active,
    // we have to update places those colors were painted.
    if (RenderView* view = m_document->renderView())
        view->selection().repaint();

    // Caret appears in the active frame.
    if (activeAndFocused)
        setSelectionFromNone();
    setCaretVisibility(activeAndFocused ? Visible : Hidden, ShouldUpdateAppearance::Yes);
#endif
}

static Vector<Style::PseudoClassChangeInvalidation> invalidateFocusedElementAndShadowIncludingAncestors(Element* focusedElement, bool activeAndFocused)
{
    Vector<Style::PseudoClassChangeInvalidation> invalidations;
    for (RefPtr element = focusedElement; element; element = element->shadowHost()) {
        invalidations.append({ *element, { { CSSSelector::PseudoClassFocus, activeAndFocused }, { CSSSelector::PseudoClassFocusVisible, activeAndFocused } } });
        for (auto& lineage : lineageOfType<Element>(*element))
            invalidations.append({ lineage, CSSSelector::PseudoClassFocusWithin, activeAndFocused });
    }
    return invalidations;
}

void FrameSelection::pageActivationChanged()
{
    bool isActive = isPageActive(m_document.get());
    RefPtr focusedElement = m_document->focusedElement();
    auto invalidations = invalidateFocusedElementAndShadowIncludingAncestors(focusedElement.get(), m_focused && isActive);
    m_isActive = isActive;

    focusedOrActiveStateChanged();
}

void FrameSelection::setFocused(bool isFocused)
{
    if (m_focused == isFocused)
        return;

    bool isActive = isPageActive(m_document.get());
    RefPtr focusedElement = m_document->focusedElement();
    auto invalidations = invalidateFocusedElementAndShadowIncludingAncestors(focusedElement.get(), isFocused && isActive);
    m_focused = isFocused;
    m_isActive = isActive;

    focusedOrActiveStateChanged();
}

bool FrameSelection::isFocusedAndActive() const
{
    return m_focused && m_document->page() && m_document->page()->focusController().isActive();
}

#if ENABLE(TEXT_CARET)
inline static bool shouldStopBlinkingDueToTypingCommand(Document* document)
{
    return document->editor().lastEditCommand() && document->editor().lastEditCommand()->shouldStopCaretBlinking();
}
#endif

void FrameSelection::updateAppearance()
{
#if PLATFORM(IOS_FAMILY)
    if (!m_updateAppearanceEnabled)
        return;
#endif

    // Paint a block cursor instead of a caret in overtype mode unless the caret is at the end of a line (in this case
    // the FrameSelection will paint a blinking caret as usual).
    VisibleSelection oldSelection = selection();

#if ENABLE(TEXT_CARET)
    bool paintBlockCursor = m_shouldShowBlockCursor && m_selection.isCaret() && !isLogicalEndOfLine(m_selection.visibleEnd());
    bool caretRectChangedOrCleared = recomputeCaretRect();

    bool caretBrowsing = m_document->settings().caretBrowsingEnabled();
    bool shouldBlink = !paintBlockCursor && caretIsVisible() && isCaret() && (oldSelection.isContentEditable() || caretBrowsing);

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (caretRectChangedOrCleared || !shouldBlink || shouldStopBlinkingDueToTypingCommand(m_document.get()))
        m_caretBlinkTimer.stop();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !m_caretBlinkTimer.isActive()) {
        if (Seconds blinkInterval = RenderTheme::singleton().caretBlinkInterval())
            m_caretBlinkTimer.startRepeating(blinkInterval);

        if (!m_caretPaint) {
            m_caretPaint = true;
            invalidateCaretRect();
        }
    }
#endif

    // Construct a new VisibleSolution, since m_selection is not necessarily valid, and the following steps
    // assume a valid selection. See <https://bugs.webkit.org/show_bug.cgi?id=69563> and <rdar://problem/10232866>.
#if ENABLE(TEXT_CARET)
    VisiblePosition endVisiblePosition = paintBlockCursor ? modifyExtendingForward(TextGranularity::CharacterGranularity) : oldSelection.visibleEnd();
    VisibleSelection selection(oldSelection.visibleStart(), endVisiblePosition);
#else
    VisibleSelection selection(oldSelection.visibleStart(), oldSelection.visibleEnd());
#endif

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        auto* view = m_document->renderView();
        if (!view)
            return;
        if (!selection.isRange()) {
            view->selection().clear();
            return;
        }
    }

    // Use the rightmost candidate for the start of the selection, and the leftmost candidate for the end of the selection.
    // Example: foo <a>bar</a>.  Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.   If we pass [foo, 3]
    // as the start of the selection, the selection painting code will think that content on the line containing 'foo' is selected
    // and will fill the gap before 'bar'.
    Position startPos = selection.start();
    Position candidate = startPos.downstream();
    if (candidate.isCandidate())
        startPos = candidate;
    Position endPos = selection.end();
    candidate = endPos.upstream();
    if (candidate.isCandidate())
        endPos = candidate;

    // We can get into a state where the selection endpoints map to the same VisiblePosition when a selection is deleted
    // because we don't yet notify the FrameSelection of text removal.
    if (auto* view = m_document->renderView(); startPos.isNotNull() && endPos.isNotNull() && selection.visibleStart() != selection.visibleEnd()) {
        RenderObject* startRenderer = startPos.deprecatedNode()->renderer();
        int startOffset = startPos.deprecatedEditingOffset();
        RenderObject* endRenderer = endPos.deprecatedNode()->renderer();
        int endOffset = endPos.deprecatedEditingOffset();
        ASSERT(startOffset >= 0 && endOffset >= 0);
        view->selection().set({ startRenderer, endRenderer, static_cast<unsigned>(startOffset), static_cast<unsigned>(endOffset) });
    }
}

void FrameSelection::setCaretVisibility(CaretVisibility visibility, ShouldUpdateAppearance doAppearanceUpdate)
{
    if (caretVisibility() == visibility)
        return;

    // FIXME: We shouldn't trigger a synchronous layout here.
    if (doAppearanceUpdate == ShouldUpdateAppearance::Yes && m_document)
        updateSelectionAppearanceNow();

#if ENABLE(TEXT_CARET)
    if (m_caretPaint) {
        m_caretPaint = false;
        invalidateCaretRect();
    }
    CaretBase::setCaretVisibility(visibility);
#endif

    if (doAppearanceUpdate == ShouldUpdateAppearance::Yes)
        updateAppearance();
}

void FrameSelection::caretBlinkTimerFired()
{
#if ENABLE(TEXT_CARET)
    if (!isCaret())
        return;
    ASSERT(caretIsVisible());
    bool caretPaint = m_caretPaint;
    if (isCaretBlinkingSuspended() && caretPaint)
        return;
    m_caretPaint = !caretPaint;
    invalidateCaretRect();
#endif
}

// Helper function that tells whether a particular node is an element that has an entire
// Frame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node* n)
{
    if (!n)
        return false;
    RenderObject* renderer = n->renderer();
    if (!is<RenderWidget>(renderer))
        return false;
    Widget* widget = downcast<RenderWidget>(*renderer).widget();
    return widget && widget->isFrameView();
}

void FrameSelection::setFocusedElementIfNeeded()
{
    if (isNone() || !isFocused())
        return;

    bool caretBrowsing = m_document->settings().caretBrowsingEnabled();
    if (caretBrowsing) {
        if (RefPtr anchor = enclosingAnchorElement(m_selection.base())) {
            CheckedRef focusController { m_document->page()->focusController() };
            focusController->setFocusedElement(anchor.get(), *m_document->frame());
            return;
        }
    }

    if (Element* target = m_selection.rootEditableElement()) {
        // Walk up the DOM tree to search for an element to focus.
        while (target) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target->isMouseFocusable() && !isFrameElement(target)) {
                CheckedRef(m_document->page()->focusController())->setFocusedElement(target, *m_document->frame());
                return;
            }
            target = target->parentOrShadowHostElement();
        }
        m_document->setFocusedElement(nullptr);
    }

    if (caretBrowsing)
        CheckedRef(m_document->page()->focusController())->setFocusedElement(nullptr, *m_document->frame());
}

void DragCaretController::paintDragCaret(Frame* frame, GraphicsContext& p, const LayoutPoint& paintOffset, const LayoutRect& clipRect) const
{
#if ENABLE(TEXT_CARET)
    if (m_position.deepEquivalent().deprecatedNode() && m_position.deepEquivalent().deprecatedNode()->document().frame() == frame)
        paintCaret(*m_position.deepEquivalent().deprecatedNode(), p, paintOffset, clipRect);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(p);
    UNUSED_PARAM(paintOffset);
    UNUSED_PARAM(clipRect);
#endif
}

RefPtr<MutableStyleProperties> FrameSelection::copyTypingStyle() const
{
    if (!m_typingStyle || !m_typingStyle->style())
        return nullptr;
    return m_typingStyle->style()->mutableCopy();
}

bool FrameSelection::shouldDeleteSelection(const VisibleSelection& selection) const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->frame() && m_document->frame()->selectionChangeCallbacksDisabled())
        return true;
#endif
    return m_document->editor().client()->shouldDeleteRange(selection.toNormalizedRange());
}

FloatRect FrameSelection::selectionBounds(ClipToVisibleContent clipToVisibleContent)
{
    if (!m_document)
        return LayoutRect();

    updateSelectionAppearanceNow();
    auto* renderView = m_document->renderView();
    if (!renderView)
        return LayoutRect();

    if (!m_selection.range())
        return LayoutRect();
    
#if PLATFORM(IOS_FAMILY)
    auto selectionGeometries = RenderObject::collectSelectionGeometries(m_selection.range().value());
    IntRect visibleSelectionRect;
    for (auto geometry : selectionGeometries)
        visibleSelectionRect.unite(geometry.rect());
    
    if (clipToVisibleContent == ClipToVisibleContent::No)
        return visibleSelectionRect;
#else
    auto& selection = renderView->selection();
    auto visibleSelectionRect = selection.boundsClippedToVisibleContent();
    
    if (clipToVisibleContent == ClipToVisibleContent::No)
        return selection.bounds();
#endif
    
    return intersection(visibleSelectionRect, renderView->frameView().visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect));

}

void FrameSelection::getClippedVisibleTextRectangles(Vector<FloatRect>& rectangles, TextRectangleHeight textRectHeight) const
{
    if (!m_document->renderView())
        return;

    auto range = selection().toNormalizedRange();
    if (!range)
        return;

    OptionSet<RenderObject::BoundingRectBehavior> behavior;
    if (textRectHeight == TextRectangleHeight::SelectionHeight)
        behavior.add(RenderObject::BoundingRectBehavior::UseSelectionHeight);

    auto visibleContentRect = m_document->view()->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
    for (auto& rect : boundingBoxes(RenderObject::absoluteTextQuads(*range, behavior))) {
        auto intersectionRect = intersection(rect, visibleContentRect);
        if (!intersectionRect.isEmpty())
            rectangles.append(intersectionRect);
    }
}

// Scans logically forward from "start", including any child frames.
static HTMLFormElement* scanForForm(Element* start)
{
    if (!start)
        return nullptr;
    for (auto& element : descendantsOfType<HTMLElement>(start->document())) {
        if (is<HTMLFormElement>(element))
            return &downcast<HTMLFormElement>(element);
        if (is<HTMLFormControlElement>(element))
            return downcast<HTMLFormControlElement>(element).form();
        if (is<HTMLFrameElementBase>(element)) {
            if (auto* contentDocument = downcast<HTMLFrameElementBase>(element).contentDocument()) {
                if (auto* frameResult = scanForForm(contentDocument->documentElement()))
                    return frameResult;
            }
        }
    }
    return nullptr;
}

// We look for either the form containing the current focus, or for one immediately after it
HTMLFormElement* FrameSelection::currentForm() const
{
    // Start looking either at the active (first responder) node, or where the selection is.
    Element* start = m_document->focusedElement();
    if (!start)
        start = m_selection.start().element();
    if (!start)
        return nullptr;

    if (auto form = lineageOfType<HTMLFormElement>(*start).first())
        return form;
    if (auto formControl = lineageOfType<HTMLFormControlElement>(*start).first())
        return formControl->form();

    // Try walking forward in the node tree to find a form element.
    return scanForForm(start);
}

void FrameSelection::revealSelection(SelectionRevealMode revealMode, const ScrollAlignment& alignment, RevealExtentOption revealExtentOption, ScrollBehavior scrollBehavior)
{
    if (revealMode == SelectionRevealMode::DoNotReveal)
        return;

    if (isNone())
        return;

    updateSelectionAppearanceNow();

    LayoutRect rect;
    bool insideFixed = false;
    if (isCaret())
        rect = absoluteCaretBounds(&insideFixed);
    else
        rect = revealExtentOption == RevealExtent ? VisiblePosition(m_selection.extent()).absoluteCaretBounds() : enclosingIntRect(selectionBounds(ClipToVisibleContent::No));

    Position start = m_selection.start();
    ASSERT(start.deprecatedNode());
    if (!start.deprecatedNode() || !start.deprecatedNode()->renderer())
        return;

#if PLATFORM(IOS_FAMILY)
    if (m_scrollingSuppressCount)
        return;
#endif

    // FIXME: This code only handles scrolling the startContainer's layer, but
    // the selection rect could intersect more than just that.
    // See <rdar://problem/4799899>.
    FrameView::scrollRectToVisible(rect, *start.deprecatedNode()->renderer(), insideFixed, { revealMode, alignment, alignment, ShouldAllowCrossOriginScrolling::Yes, scrollBehavior });
    updateAppearance();

#if PLATFORM(IOS_FAMILY)
    if (m_document->page())
        m_document->page()->chrome().client().notifyRevealedSelectionByScrollingFrame(*m_document->frame());
#endif
}

void FrameSelection::setSelectionFromNone()
{
    // Put a caret inside the body if the entire frame is editable (either the
    // entire WebView is editable or designMode is on for this document).
    bool caretBrowsing = m_document->settings().caretBrowsingEnabled();

    if (!m_document || !isNone() || !(m_document->hasEditableStyle() || caretBrowsing))
        return;

    if (auto* body = m_document->body())
        setSelection(VisibleSelection(firstPositionInOrBeforeNode(body)));
}

bool FrameSelection::shouldChangeSelection(const VisibleSelection& newSelection) const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->frame() && m_document->frame()->selectionChangeCallbacksDisabled())
        return true;
#endif
    return m_document->editor().shouldChangeSelection(selection(), newSelection, newSelection.affinity(), false);
}

bool FrameSelection::dispatchSelectStart()
{
    Node* selectStartTarget = m_selection.extent().containerNode();
    if (!selectStartTarget)
        return true;

    auto event = Event::create(eventNames().selectstartEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    selectStartTarget->dispatchEvent(event);
    return !event->defaultPrevented();
}

void FrameSelection::setShouldShowBlockCursor(bool shouldShowBlockCursor)
{
    m_shouldShowBlockCursor = shouldShowBlockCursor;

    m_document->updateLayoutIgnorePendingStylesheets();

    updateAppearance();
}

void FrameSelection::updateAppearanceAfterLayout()
{
    m_appearanceUpdateTimer.stop();
    updateAppearanceAfterLayoutOrStyleChange();
}

void FrameSelection::scheduleAppearanceUpdateAfterStyleChange()
{
    m_appearanceUpdateTimer.startOneShot(0_s);
}

void FrameSelection::appearanceUpdateTimerFired()
{
    Ref<Document> protector(*m_document);
    updateAppearanceAfterLayoutOrStyleChange();
}

void FrameSelection::updateAppearanceAfterLayoutOrStyleChange()
{
    if (auto* client = m_document->editor().client())
        client->updateEditorStateAfterLayoutIfEditabilityChanged();

    setCaretRectNeedsUpdate();
    updateAndRevealSelection(m_selectionRevealIntent);
    updateDataDetectorsForSelection();
}

#if ENABLE(TREE_DEBUGGING)

String FrameSelection::debugDescription() const
{
    return m_selection.debugDescription();
}

void FrameSelection::showTreeForThis() const
{
    m_selection.showTreeForThis();
}

#endif

#if PLATFORM(IOS_FAMILY)

void FrameSelection::expandSelectionToElementContainingCaretSelection()
{
    auto range = elementRangeContainingCaretSelection();
    if (!range)
        return;
    setSelection(VisibleSelection(*range));
}

std::optional<SimpleRange> FrameSelection::elementRangeContainingCaretSelection() const
{
    auto element = deprecatedEnclosingBlockFlowElement(m_selection.visibleStart().deepEquivalent().deprecatedNode());
    if (!element)
        return std::nullopt;

    auto start = VisiblePosition(makeContainerOffsetPosition(element, 0));
    auto end = VisiblePosition(makeContainerOffsetPosition(element, element->countChildNodes()));
    if (start.isNull() || end.isNull())
        return std::nullopt;

    auto selection = m_selection;
    selection.setBase(start);
    selection.setExtent(end);
    return selection.toNormalizedRange();
}

void FrameSelection::expandSelectionToWordContainingCaretSelection()
{
    VisibleSelection selection(wordSelectionContainingCaretSelection(m_selection));        
    if (selection.isCaretOrRange())
        setSelection(selection);
}

std::optional<SimpleRange> FrameSelection::wordRangeContainingCaretSelection()
{
    return wordSelectionContainingCaretSelection(m_selection).toNormalizedRange();
}

void FrameSelection::expandSelectionToStartOfWordContainingCaretSelection()
{
    if (m_selection.isNone() || isStartOfDocument(m_selection.start()))
        return;

    VisiblePosition s1(m_selection.start());
    VisiblePosition e1(m_selection.end());

    VisibleSelection expanded(wordSelectionContainingCaretSelection(m_selection));
    VisiblePosition s2(expanded.start());

    // Don't allow the start to become greater after the expansion.
    if (s2.isNull() || s2 > s1)
        s2 = s1;

    moveTo(s2, e1);
}

UChar FrameSelection::characterInRelationToCaretSelection(int amount) const
{
    auto position = m_selection.visibleStart();
    if (amount < 0) {
        int count = abs(amount);
        for (int i = 0; i < count; i++)
            position = position.previous();
        return position.characterBefore();
    }
    for (int i = 0; i < amount; i++)
        position = position.next();
    return position.characterAfter();
}

bool FrameSelection::selectionAtWordStart() const
{
    auto position = m_selection.visibleStart();
    if (isStartOfParagraph(position))
        return true;

    unsigned previousCount = 0;
    for (position = position.previous(); !position.isNull(); position = position.previous()) {
        previousCount++;
        if (isStartOfParagraph(position))
            return previousCount != 1;
        if (UChar c = position.characterAfter())
            return isSpaceOrNewline(c) || c == noBreakSpace || (u_ispunct(c) && c != ',' && c != '-' && c != '\'');
    }
    return true;
}

std::optional<SimpleRange> FrameSelection::rangeByMovingCurrentSelection(int amount) const
{
    return rangeByAlteringCurrentSelection(AlterationMove, amount);
}

std::optional<SimpleRange> FrameSelection::rangeByExtendingCurrentSelection(int amount) const
{
    return rangeByAlteringCurrentSelection(AlterationExtend, amount);
}

VisibleSelection FrameSelection::wordSelectionContainingCaretSelection(const VisibleSelection& selection)
{
    if (selection.isNone())
        return VisibleSelection();

    ASSERT(selection.isCaretOrRange());
    FrameSelection frameSelection;
    frameSelection.setSelection(selection);

    Position startPosBeforeExpansion(selection.start());
    Position endPosBeforeExpansion(selection.end());
    VisiblePosition startVisiblePosBeforeExpansion(startPosBeforeExpansion);
    VisiblePosition endVisiblePosBeforeExpansion(endPosBeforeExpansion);
    if (endVisiblePosBeforeExpansion.isNull())
        return VisibleSelection();

    if (isEndOfParagraph(endVisiblePosBeforeExpansion)) {
        UChar c(endVisiblePosBeforeExpansion.characterBefore());
        if (isSpaceOrNewline(c) || c == noBreakSpace) {
            // End of paragraph with space.
            return VisibleSelection();
        }
    }

    // If at end of paragraph, move backwards one character.
    // This has the effect of selecting the word on the line (which is
    // what we want, rather than selecting past the end of the line).
    if (isEndOfParagraph(endVisiblePosBeforeExpansion) && !isStartOfParagraph(endVisiblePosBeforeExpansion))
        frameSelection.modify(FrameSelection::AlterationMove, SelectionDirection::Backward, TextGranularity::CharacterGranularity);

    VisibleSelection newSelection = frameSelection.selection();
    newSelection.expandUsingGranularity(TextGranularity::WordGranularity);
    frameSelection.setSelection(newSelection, defaultSetSelectionOptions(), AXTextStateChangeIntent(), AlignCursorOnScrollIfNeeded, frameSelection.granularity());

    Position startPos(frameSelection.selection().start());
    Position endPos(frameSelection.selection().end());

    // Expansion cannot be allowed to change selection so that it is no longer
    // touches (or contains) the original, unexpanded selection.
    // Enforce this on the way into these additional calculations to give them
    // the best chance to yield a suitable answer.
    if (startPos > startPosBeforeExpansion)
        startPos = startPosBeforeExpansion;
    if (endPos < endPosBeforeExpansion)
        endPos = endPosBeforeExpansion;

    VisiblePosition startVisiblePos(startPos);
    VisiblePosition endVisiblePos(endPos);

    if (startVisiblePos.isNull() || endVisiblePos.isNull()) {
        // Start or end is nil
        return VisibleSelection();
    }

    if (isEndOfLine(endVisiblePosBeforeExpansion)) {
        VisiblePosition previous(endVisiblePos.previous());
        if (previous == endVisiblePos) {
            // Empty document
            return VisibleSelection();
        }
        UChar c(previous.characterAfter());
        if (isSpaceOrNewline(c) || c == noBreakSpace) {
            // Space at end of line
            return VisibleSelection();
        }
    }

    // Expansion has selected past end of line.
    // Try repositioning backwards.
    if (isEndOfLine(startVisiblePos) && isStartOfLine(endVisiblePos)) {
        VisiblePosition previous(startVisiblePos.previous());
        if (isEndOfLine(previous)) {
            // On empty line
            return VisibleSelection();
        }
        UChar c(previous.characterAfter());
        if (isSpaceOrNewline(c) || c == noBreakSpace) {
            // Space at end of line
            return VisibleSelection();
        }
        frameSelection.moveTo(startVisiblePos);
        frameSelection.modify(FrameSelection::AlterationExtend, SelectionDirection::Backward, TextGranularity::WordGranularity);
        startPos = frameSelection.selection().start();
        endPos = frameSelection.selection().end();
        startVisiblePos = VisiblePosition(startPos);
        endVisiblePos = VisiblePosition(endPos);
        if (startVisiblePos.isNull() || endVisiblePos.isNull()) {
            // Start or end is nil
            return VisibleSelection();
        }
    }

    // Now loop backwards until we find a non-space.
    while (endVisiblePos != startVisiblePos) {
        VisiblePosition previous(endVisiblePos.previous());
        UChar c(previous.characterAfter());
        if (!isSpaceOrNewline(c) && c != noBreakSpace)
            break;
        endVisiblePos = previous;
    }

    // Expansion cannot be allowed to change selection so that it is no longer
    // touches (or contains) the original, unexpanded selection.
    // Enforce this on the way out of the function to preserve the invariant.
    if (startVisiblePos > startVisiblePosBeforeExpansion)
        startVisiblePos = startVisiblePosBeforeExpansion;
    if (endVisiblePos < endVisiblePosBeforeExpansion)
        endVisiblePos = endVisiblePosBeforeExpansion;

    return VisibleSelection(startVisiblePos, endVisiblePos);    
}

bool FrameSelection::selectionAtSentenceStart() const
{
    auto position = m_selection.visibleStart();
    if (position.isNull())
        return false;

    if (isStartOfParagraph(position))
        return true;
 
    bool sawSpace = false;
    unsigned previousCount = 0;
    for (position = position.previous(); !position.isNull(); position = position.previous()) {
        previousCount++;
        if (isStartOfParagraph(position))
            return previousCount != 1 && (previousCount != 2 || !sawSpace);
        if (auto c = position.characterAfter()) {
            if (isSpaceOrNewline(c) || c == noBreakSpace)
                sawSpace = true;
            else
                return c == '.' || c == '!' || c == '?';
        }
    }
    return true;
}

std::optional<SimpleRange> FrameSelection::rangeByAlteringCurrentSelection(EAlteration alteration, int amount) const
{
    if (m_selection.isNone())
        return std::nullopt;

    if (!amount)
        return m_selection.toNormalizedRange();

    FrameSelection frameSelection;
    frameSelection.setSelection(m_selection);
    SelectionDirection direction = amount > 0 ? SelectionDirection::Forward : SelectionDirection::Backward;
    for (int i = 0; i < abs(amount); i++)
        frameSelection.modify(alteration, direction, TextGranularity::CharacterGranularity);
    return frameSelection.selection().toNormalizedRange();
}

void FrameSelection::clearCurrentSelection()
{
    setSelection(VisibleSelection());
}

void FrameSelection::setCaretBlinks(bool caretBlinks)
{
    if (m_caretBlinks == caretBlinks)
        return;
#if ENABLE(TEXT_CARET)
    m_document->updateLayoutIgnorePendingStylesheets();
    if (m_caretPaint) {
        m_caretPaint = false; 
        invalidateCaretRect(); 
    }
#endif
    if (caretBlinks)
        setFocusedElementIfNeeded();
    m_caretBlinks = caretBlinks;
    updateAppearance();
}

void FrameSelection::setCaretColor(const Color& caretColor)
{
    if (m_caretColor != caretColor) {
        m_caretColor = caretColor;
        if (caretIsVisible() && m_caretBlinks && isCaret())
            invalidateCaretRect();
    }
}

#endif // PLATFORM(IOS_FAMILY)

static bool containsEndpoints(const WeakPtr<Document, WeakPtrImplWithEventTargetData>& document, const std::optional<SimpleRange>& range)
{
    return document && range && document->contains(range->start.container) && document->contains(range->end.container);
}

static bool containsEndpoints(const WeakPtr<Document, WeakPtrImplWithEventTargetData>& document, const Range& liveRange)
{
    // Only need to check the start container because live ranges enforce the invariant that start and end have a common ancestor.
    return document && document->contains(liveRange.startContainer());
}

bool FrameSelection::isInDocumentTree() const
{
    return containsEndpoints(m_document, m_selection.range());
}

bool FrameSelection::isConnectedToDocument() const
{
    return selection().document() == m_document.get();
}

RefPtr<Range> FrameSelection::associatedLiveRange()
{
    if (!m_associatedLiveRange) {
        if (auto range = m_selection.range(); containsEndpoints(m_document, range)) {
            m_associatedLiveRange = createLiveRange(*range);
            m_associatedLiveRange->didAssociateWithSelection();
        }
    }
    return m_associatedLiveRange;
}

void FrameSelection::disassociateLiveRange()
{
    if (auto previouslyAssociatedLiveRange = std::exchange(m_associatedLiveRange, nullptr))
        previouslyAssociatedLiveRange->didDisassociateFromSelection();
}

void FrameSelection::associateLiveRange(Range& liveRange)
{
    disassociateLiveRange();
    m_associatedLiveRange = &liveRange;
    liveRange.didAssociateWithSelection();
    updateFromAssociatedLiveRange();
}

void FrameSelection::updateFromAssociatedLiveRange()
{
    ASSERT(m_associatedLiveRange);
    if (!containsEndpoints(m_document, *m_associatedLiveRange))
        disassociateLiveRange();
    else {
        // Don't use VisibleSelection's constructor that takes a SimpleRange, because it uses makeDeprecatedLegacyPosition instead of makeContainerOffsetPosition.
        auto start = makeContainerOffsetPosition(&m_associatedLiveRange->startContainer(), m_associatedLiveRange->startOffset());
        auto end = makeContainerOffsetPosition(&m_associatedLiveRange->endContainer(), m_associatedLiveRange->endOffset());
        setSelection({ start, end });
    }
}

void FrameSelection::updateAssociatedLiveRange()
{
    auto range = m_selection.range();
    if (!containsEndpoints(m_document, range)) {
        // The selection was cleared or is now within a shadow tree.
        disassociateLiveRange();
    } else {
        if (m_associatedLiveRange)
            m_associatedLiveRange->updateFromSelection(*range);
    }
}

}

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::FrameSelection& selection)
{
    selection.showTreeForThis();
}

void showTree(const WebCore::FrameSelection* selection)
{
    if (selection)
        selection->showTreeForThis();
}

#endif
