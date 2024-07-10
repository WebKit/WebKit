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
#include "CaretAnimator.h"
#include "CharacterData.h"
#include "ColorBlending.h"
#include "DeleteSelectionCommand.h"
#include "DictationCaretAnimator.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameTree.h"
#include "GCReachableRef.h"
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
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MutableStyleProperties.h"
#include "OpacityCaretAnimator.h"
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
#include "ShadowRoot.h"
#include "SimpleCaretAnimator.h"
#include "SimpleRange.h"
#include "SpatialNavigation.h"
#include "StyleProperties.h"
#include "StyleTreeResolver.h"
#include "TypedElementDescendantIteratorInlines.h"
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

CaretBase::CaretBase(CaretVisibility visibility)
    : m_caretRectNeedsUpdate(true)
    , m_caretVisibility(visibility)
{
}

DragCaretController::DragCaretController()
    : CaretBase(CaretVisibility::Visible)
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

    if (RefPtr document = m_position.deepEquivalent().document()) {
        if (RefPtr documentView = document->view())
            return documentView->contentsToRootView(m_position.absoluteCaretBounds());
    }

    return { };
}

IntRect DragCaretController::editableElementRectInRootViewCoordinates() const
{
    if (!hasCaret())
        return { };

    RefPtr<ContainerNode> editableContainer;
    if (RefPtr formControl = enclosingTextFormControl(m_position.deepEquivalent()))
        editableContainer = WTFMove(formControl);
    else
        editableContainer = highestEditableRoot(m_position.deepEquivalent());

    if (!editableContainer)
        return { };

    CheckedPtr renderer = editableContainer->renderer();
    if (!renderer)
        return { };

    if (RefPtr view = editableContainer->document().view())
        return view->contentsToRootView(renderer->absoluteBoundingBoxRect()); // FIXME: Wrong for elements with visible layout overflow.

    return { };
}

static inline bool shouldAlwaysUseDirectionalSelection(Document* document)
{
    return !document || document->editingBehavior().shouldConsiderSelectionAsDirectional();
}

static inline bool isPageActive(Document* document)
{
    return document && document->page() && document->page()->focusController().isActive();
}

static UniqueRef<CaretAnimator> createCaretAnimator(FrameSelection* frameSelection, std::optional<CaretAnimatorType> optionalCaretType = std::nullopt)
{
#if PLATFORM(MAC) && HAVE(REDESIGNED_TEXT_CURSOR)
    if (redesignedTextCursorEnabled()) {
        std::optional<LayoutRect> existingExpansionRect = std::nullopt;
        if (optionalCaretType)
            existingExpansionRect = frameSelection->caretAnimator().caretRepaintRectForLocalRect(LayoutRect());

        switch (optionalCaretType.value_or(CaretAnimatorType::Default)) {
        case CaretAnimatorType::Default:
            return makeUniqueRef<OpacityCaretAnimator>(*frameSelection, existingExpansionRect);
        case CaretAnimatorType::Dictation:
            return makeUniqueRef<DictationCaretAnimator>(*frameSelection);
        }
    }
#else
    UNUSED_PARAM(optionalCaretType);
#endif
    return makeUniqueRef<SimpleCaretAnimator>(*frameSelection);
}

FrameSelection::FrameSelection(Document* document)
    : m_document(document)
    , m_granularity(TextGranularity::CharacterGranularity)
    , m_caretAnimator(createCaretAnimator(this))
    , m_caretInsidePositionFixed(false)
    , m_absCaretBoundsDirty(true)
    , m_focused(document && document->frame() && document->page() && document->page()->focusController().focusedLocalFrame() == document->frame())
    , m_isActive(isPageActive(document))
    , m_shouldShowBlockCursor(false)
    , m_pendingSelectionUpdate(false)
    , m_alwaysAlignCursorOnScrollWhenRevealingSelection(false)
#if PLATFORM(IOS_FAMILY)
    , m_updateAppearanceEnabled(false)
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
    setCaretVisibility(activeAndFocused ? CaretVisibility::Visible : CaretVisibility::Hidden, ShouldUpdateAppearance::No);
#endif
}

FrameSelection::~FrameSelection() = default;

Element* FrameSelection::rootEditableElementOrDocumentElement() const
{
    Element* selectionRoot = m_selection.rootEditableElement();
    return selectionRoot ? selectionRoot : m_document->documentElement();
}

void FrameSelection::moveTo(const VisiblePosition& position, UserTriggered userTriggered, CursorAlignOnScroll align)
{
    setSelection(VisibleSelection(position.deepEquivalent(), position.deepEquivalent(), position.affinity(), m_selection.isDirectional()),
        defaultSetSelectionOptions(userTriggered), AXTextStateChangeIntent(), align);
}

void FrameSelection::moveTo(const VisiblePosition& base, const VisiblePosition& extent, UserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveTo(const Position& position, Affinity affinity, UserTriggered userTriggered)
{
    setSelection(VisibleSelection(position, affinity, m_selection.isDirectional()), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveTo(const Position& base, const Position& extent, Affinity affinity, UserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(base, extent, affinity, selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::moveWithoutValidationTo(const Position& base, const Position& extent, bool selectionHasDirection, OptionSet<SetSelectionOption> options, const AXTextStateChangeIntent& intent)
{
    VisibleSelection newSelection;
    newSelection.setWithoutValidation(base, extent);
    newSelection.setIsDirectional(selectionHasDirection);
    AXTextStateChangeIntent newIntent = intent.type == AXTextStateChangeTypeUnknown ? AXTextStateChangeIntent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, false }) : intent;
    setSelection(newSelection, options, newIntent, CursorAlignOnScroll::IfNeeded, TextGranularity::CharacterGranularity);
}

void DragCaretController::setCaretPosition(const VisiblePosition& position)
{
    if (auto node = m_position.deepEquivalent().protectedDeprecatedNode())
        invalidateCaretRect(node.get());
    m_position = position;
    setCaretRectNeedsUpdate();
    RefPtr<Document> document;
    if (auto node = m_position.deepEquivalent().protectedDeprecatedNode()) {
        invalidateCaretRect(node.get());
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
    setSelection(newSelection, defaultSetSelectionOptions() | SetSelectionOption::FireSelectEvent, intent, CursorAlignOnScroll::IfNeeded, granularity);
}

bool FrameSelection::setSelectionWithoutUpdatingAppearance(const VisibleSelection& newSelectionPossiblyWithoutDirection, OptionSet<SetSelectionOption> options, CursorAlignOnScroll align, TextGranularity granularity)
{
    bool closeTyping = options.contains(SetSelectionOption::CloseTyping);
    bool shouldClearTypingStyle = options.contains(SetSelectionOption::ClearTypingStyle);

    auto document = protectedDocument();
    VisibleSelection newSelection = newSelectionPossiblyWithoutDirection;
    if (shouldAlwaysUseDirectionalSelection(document.get()))
        newSelection.setIsDirectional(true);

    // <http://bugs.webkit.org/show_bug.cgi?id=23464>: Infinite recursion at FrameSelection::setSelection
    // if document->frame() == m_document->frame() we can get into an infinite loop
    if (RefPtr newSelectionDocument = newSelection.base().document()) {
        if (RefPtr newSelectionFrame = newSelectionDocument->frame()) {
            if (document && newSelectionFrame != document->frame() && newSelectionDocument != document) {
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
    if (willMutateSelection && document && !options.contains(SetSelectionOption::DoNotNotifyEditorClients))
        document->editor().selectionWillChange();

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (newSelection.isOrphan()) {
            ASSERT_NOT_REACHED();
            clear();
            return false;
        }

        if (!document || (!document->frame() && !newSelection.document())) {
            setNodeFlags(m_selection, false);
            m_selection = newSelection;
            setNodeFlags(m_selection, true);
            updateAssociatedLiveRange();
            return false;
        }

        bool selectionEndpointsBelongToMultipleDocuments = newSelection.base().document() && !newSelection.document();
        bool selectionIsInAnotherDocument = newSelection.document() && newSelection.document() != document.get();
        bool selectionIsInDetachedDocument = newSelection.document() && !newSelection.document()->frame();
        if (selectionEndpointsBelongToMultipleDocuments || selectionIsInAnotherDocument || selectionIsInDetachedDocument) {
            clear();
            return false;
        }
        ASSERT(document->frame());

        if (closeTyping)
            TypingCommand::closeTyping(*document);

        if (shouldClearTypingStyle)
            clearTypingStyle();

        m_granularity = granularity;
        setNodeFlags(m_selection, false);
        m_selection = newSelection;
        setNodeFlags(m_selection, true);
        updateAssociatedLiveRange();
    }

    // Selection offsets should increase when LF is inserted before the caret in InsertLineBreakCommand. See <https://webkit.org/b/56061>.
    // https://www.w3.org/TR/selection-api/#selectionchange-event
    RefPtr textControl = enclosingTextFormControl(newSelection.start());
    bool shouldScheduleSelectionChangeEvent = willMutateSelection;
    if (textControl)
        shouldScheduleSelectionChangeEvent = textControl->selectionChanged(options.contains(SetSelectionOption::FireSelectEvent));

    if (!willMutateSelection)
        return false;

    setCaretRectNeedsUpdate();

    if (!newSelection.isNone() && !(options & SetSelectionOption::DoNotSetFocus)) {
        RefPtr oldFocusedElement = document->focusedElement();
        setFocusedElementIfNeeded(options);
        if (!document->frame())
            return false;
        // FIXME: Should not be needed.
        if (document->focusedElement() != oldFocusedElement)
            document->updateStyleIfNeeded();
    }

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = std::nullopt;
    selectFrameElementInParentIfFullySelected();
    if (!options.contains(SetSelectionOption::DoNotNotifyEditorClients))
        document->editor().respondToChangedSelection(oldSelection, options);

    if (shouldScheduleSelectionChangeEvent) {
        if (textControl)
            textControl->scheduleSelectionChangeEvent();
        else if (!m_hasScheduledSelectionChangeEventOnDocument) {
            m_hasScheduledSelectionChangeEventOnDocument = true;
            document->eventLoop().queueTask(TaskSource::UserInteraction, [weakDocument = WeakPtr { document.get() }] {
                if (RefPtr document = weakDocument.get()) {
                    document->selection().m_hasScheduledSelectionChangeEventOnDocument = false;
                    document->dispatchEvent(Event::create(eventNames().selectionchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
                }
            });
        }
    }

    return true;
}

void FrameSelection::setSelection(const VisibleSelection& selection, OptionSet<SetSelectionOption> options, AXTextStateChangeIntent intent, CursorAlignOnScroll align, TextGranularity granularity)
{
    LOG_WITH_STREAM(Selection, stream << "FrameSelection::setSelection " << selection);

    RefPtr document = protectedDocument();
    if (!setSelectionWithoutUpdatingAppearance(selection, options, align, granularity))
        return;

    if (options & SetSelectionOption::RevealSelectionUpToMainFrame)
        m_selectionRevealMode = SelectionRevealMode::RevealUpToMainFrame;
    else if (options & SetSelectionOption::RevealSelection)
        m_selectionRevealMode = SelectionRevealMode::Reveal;
    else if (options & SetSelectionOption::DelegateMainFrameScroll)
        m_selectionRevealMode = SelectionRevealMode::DelegateMainFrameScroll;
    else
        m_selectionRevealMode = SelectionRevealMode::DoNotReveal;
    m_alwaysAlignCursorOnScrollWhenRevealingSelection = align == CursorAlignOnScroll::Always;

    m_selectionRevealIntent = intent;
    m_pendingSelectionUpdate = true;

    document->scheduleContentRelevancyUpdate(ContentRelevancy::Selected);

    if (document->hasPendingStyleRecalc())
        return;

    RefPtr frameView = document->view();
    if (frameView && frameView->layoutContext().isLayoutPending())
        return;

    if (!(options & SetSelectionOption::IsUserTriggered))
        return;

    updateAndRevealSelection(intent, options.contains(SetSelectionOption::SmoothScroll) ? ScrollBehavior::Smooth : ScrollBehavior::Instant,
        options.contains(SetSelectionOption::RevealSelectionBounds) ? RevealExtentOption::DoNotRevealExtent : RevealExtentOption::RevealExtent,
        options.contains(SetSelectionOption::ForceCenterScroll) ? ForceCenterScroll::Yes : ForceCenterScroll::No);

    if (options & SetSelectionOption::IsUserTriggered) {
        if (auto* client = document->editor().client())
            client->didEndUserTriggeredSelectionChanges();
    }
}

void FrameSelection::updateSelectionAppearanceNow()
{
    RefPtr document = protectedDocument();
    if (!document || !document->hasLivingRenderTree())
        return;

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

void FrameSelection::updateAndRevealSelection(const AXTextStateChangeIntent& intent, ScrollBehavior scrollBehavior, RevealExtentOption revealExtent, ForceCenterScroll forceCenterScroll)
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
        
        if (forceCenterScroll == ForceCenterScroll::Yes)
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

    RefPtr element = dynamicDowncast<Element>(node);
    return element && element->containsIncludingShadowDOM(position.anchorNode());
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

static void setNodeContainsSelectionEndPoint(const Position& position, bool value)
{
    // We use anchorNode instead of containerNode() because nodeWillBeRemoved must update position when anchored node is removed.
    for (Node* currentNode = position.anchorNode(); currentNode; currentNode = currentNode->parentOrShadowHostNode()) {
        if (currentNode->containsSelectionEndPoint() == value) {
#if ASSERT_ENABLED
            for (Node* ancestor = currentNode; ancestor; ancestor = ancestor->parentOrShadowHostNode())
                ASSERT(ancestor->containsSelectionEndPoint() == value);
#endif
            break;
        }
        currentNode->setContainsSelectionEndPoint(value);
    }
#if ASSERT_ENABLED
    for (Node* ancestor = position.anchorNode(); ancestor; ancestor = ancestor->parentOrShadowHostNode())
        ASSERT(ancestor->containsSelectionEndPoint() == value);
#endif
}

void FrameSelection::setNodeFlags(VisibleSelection& selection, bool value)
{
    if (!m_document)
        return; // Ignore local FrameSelection created in TypingCommand::deleteKeyPressed and TypingCommand::forwardDeleteKeyPressed.
#if ASSERT_ENABLED
    for (RefPtr<Node> node = selection.document(); node; node = NodeTraversal::next(*node)) {
        if (node->containsSelectionEndPoint()) {
            for (Node* ancestor = node.get(); ancestor; ancestor = ancestor->parentOrShadowHostNode())
                ASSERT(ancestor->containsSelectionEndPoint());
        }
    }
#endif
    setNodeContainsSelectionEndPoint(selection.anchor(), value);
    setNodeContainsSelectionEndPoint(selection.focus(), value);
    setNodeContainsSelectionEndPoint(selection.base(), value);
    setNodeContainsSelectionEndPoint(selection.extent(), value);
    setNodeContainsSelectionEndPoint(selection.start(), value);
    setNodeContainsSelectionEndPoint(selection.end(), value);
#if ASSERT_ENABLED
    for (RefPtr<Node> node = selection.document(); node; node = NodeTraversal::next(*node)) {
        if (node->containsSelectionEndPoint()) {
            for (Node* ancestor = node.get(); ancestor; ancestor = ancestor->parentOrShadowHostNode())
                ASSERT(ancestor->containsSelectionEndPoint());
        }
    }
#endif
}

void FrameSelection::nodeWillBeRemoved(Node& node)
{
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if ((isNone() && !m_document->settings().liveRangeSelectionEnabled()) || !node.isConnected())
        return;

    if (!node.containsSelectionEndPoint()) {
        ASSERT(!removingNodeRemovesPosition(node, m_selection.anchor()));
        ASSERT(!removingNodeRemovesPosition(node, m_selection.focus()));
        ASSERT(!removingNodeRemovesPosition(node, m_selection.base()));
        ASSERT(!removingNodeRemovesPosition(node, m_selection.extent()));
        ASSERT(!removingNodeRemovesPosition(node, m_selection.start()));
        ASSERT(!removingNodeRemovesPosition(node, m_selection.end()));
        return;
    }

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

        if (anchor.isNotNull() && focus.isNotNull()) {
            setNodeFlags(m_selection, false);
            m_selection.setWithoutValidation(anchor, focus);
            setNodeFlags(m_selection, true);
        } else
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
            setNodeFlags(m_selection, false);
            if (m_selection.isBaseFirst())
                m_selection.setWithoutValidation(start, end);
            else
                m_selection.setWithoutValidation(end, start);
            setNodeFlags(m_selection, true);
        } else
            clearDOMTreeSelection = true;

        clearRenderTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        setNodeFlags(m_selection, false);
        if (m_selection.isBaseFirst())
            m_selection.setWithoutValidation(m_selection.start(), m_selection.end());
        else
            m_selection.setWithoutValidation(m_selection.end(), m_selection.start());
        setNodeFlags(m_selection, true);
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
        if (CheckedPtr renderView = node.document().renderView()) {
            renderView->selection().clear();

            // Trigger a selection update so the selection will be set again.
            m_selectionRevealIntent = AXTextStateChangeIntent();
            m_pendingSelectionUpdate = true;
            renderView->frameView().scheduleSelectionUpdate();
        }
    }

    if (clearDOMTreeSelection)
        setSelection(VisibleSelection(), SetSelectionOption::DoNotSetFocus);
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

    Position anchor = m_selection.anchor();
    Position focus = m_selection.focus();
    Position base = m_selection.base();
    Position extent = m_selection.extent();
    Position start = m_selection.start();
    Position end = m_selection.end();
    if (m_document->settings().liveRangeSelectionEnabled()) {
        updatePositionAfterAdoptingTextReplacement(anchor, node, offset, oldLength, newLength);
        updatePositionAfterAdoptingTextReplacement(focus, node, offset, oldLength, newLength);
    }
    updatePositionAfterAdoptingTextReplacement(base, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(extent, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(start, node, offset, oldLength, newLength);
    updatePositionAfterAdoptingTextReplacement(end, node, offset, oldLength, newLength);

    bool liveRangeSelectionEnabled = node.document().settings().liveRangeSelectionEnabled();
    if ((liveRangeSelectionEnabled && (anchor != m_selection.anchor() || focus != m_selection.focus()))
        || base != m_selection.base() || extent != m_selection.extent() || start != m_selection.start() || end != m_selection.end()) {
        VisibleSelection newSelection;

        if (liveRangeSelectionEnabled)
            newSelection.setWithoutValidation(anchor, focus);
        else if (base != extent)
            newSelection.setWithoutValidation(base, extent);
        else if (m_selection.isDirectional() && !m_selection.isBaseFirst())
            newSelection.setWithoutValidation(end, start);
        else
            newSelection.setWithoutValidation(start, end);

        setSelection(newSelection, SetSelectionOption::DoNotSetFocus);
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

static bool selectionIsOrphanedOrBelongsToWrongDocument(const VisibleSelection& selection, RefPtr<Document>&& document)
{
    if (selection.isOrphan())
        return true;
    RefPtr documentOfSelection = selection.document();
    return document && documentOfSelection && document != documentOfSelection;
}

void FrameSelection::willBeModified(Alteration alter, SelectionDirection direction)
{
    if (alter != Alteration::Extend)
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
    setNodeFlags(m_selection, false);
    if (baseIsStart) {
        m_selection.setBase(start);
        m_selection.setExtent(end);
    } else {
        m_selection.setBase(end);
        m_selection.setExtent(start);
    }
    setNodeFlags(m_selection, true);
    if (selectionIsOrphanedOrBelongsToWrongDocument(m_selection, m_document.get()))
        clear();
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
    if (RefPtr rootUserSelectAll = Position::rootUserSelectAllForNode(pos.deepEquivalent().anchorNode()))
        pos = isForward ? positionAfterNode(rootUserSelectAll.get()).downstream(CanCrossEditingBoundary) : positionBeforeNode(rootUserSelectAll.get()).upstream(CanCrossEditingBoundary);
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
        pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(PositionType::Extent));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = nextParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(PositionType::Extent));
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
            pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(PositionType::Start));
        break;
    }
    case TextGranularity::ParagraphGranularity:
        pos = nextParagraphPosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(PositionType::Start));
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
        pos = previousLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(PositionType::Extent));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = previousParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(PositionType::Extent));
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
        pos = previousLinePosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(PositionType::Start));
        break;
    case TextGranularity::ParagraphGranularity:
        pos = previousParagraphPosition(currentPosition, lineDirectionPointForBlockDirectionNavigation(PositionType::Start));
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

AXTextStateChangeIntent FrameSelection::textSelectionIntent(Alteration alter, SelectionDirection direction, TextGranularity granularity)
{
    AXTextStateChangeIntent intent = AXTextStateChangeIntent();
    bool flip = false;
    if (alter == FrameSelection::Alteration::Move) {
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

bool FrameSelection::modify(Alteration alter, SelectionDirection direction, TextGranularity granularity, UserTriggered userTriggered)
{
    if (userTriggered == UserTriggered::Yes) {
        FrameSelection trialFrameSelection;
        trialFrameSelection.setSelection(m_selection);
        trialFrameSelection.modify(alter, direction, granularity, UserTriggered::No);

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
        if (alter == Alteration::Move)
            position = modifyMovingRight(granularity, &reachedBoundary);
        else
            position = modifyExtendingRight(granularity);
        break;
    case SelectionDirection::Forward:
        if (alter == Alteration::Extend)
            position = modifyExtendingForward(granularity);
        else
            position = modifyMovingForward(granularity, &reachedBoundary);
        break;
    case SelectionDirection::Left:
        if (alter == Alteration::Move)
            position = modifyMovingLeft(granularity, &reachedBoundary);
        else
            position = modifyExtendingLeft(granularity);
        break;
    case SelectionDirection::Backward:
        if (alter == Alteration::Extend)
            position = modifyExtendingBackward(granularity);
        else
            position = modifyMovingBackward(granularity, &reachedBoundary);
        break;
    }

    if (reachedBoundary && !isRange() && userTriggered == UserTriggered::Yes && m_document && AXObjectCache::accessibilityEnabled()) {
        notifyAccessibilityForSelectionChange({ AXTextStateChangeTypeSelectionBoundary, textSelectionWithDirectionAndGranularity(direction, granularity) });
        return true;
    }

    if (position.isNull())
        return false;

    if (m_document && isSpatialNavigationEnabled(m_document->frame())) {
        if (!wasRange && alter == Alteration::Move && position == originalStartPosition)
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
    LayoutUnit x = lineDirectionPointForBlockDirectionNavigation(PositionType::Start);
    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(m_document.get()) || alter == Alteration::Extend);

    switch (alter) {
    case Alteration::Move:
        moveTo(position, userTriggered);
        break;
    case Alteration::Extend:

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

    if (userTriggered == UserTriggered::Yes)
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

bool FrameSelection::modify(Alteration alter, unsigned verticalDistance, VerticalDirection direction, UserTriggered userTriggered, CursorAlignOnScroll align)
{
    if (!verticalDistance)
        return false;

    if (userTriggered == UserTriggered::Yes) {
        FrameSelection trialFrameSelection;
        trialFrameSelection.setSelection(m_selection);
        trialFrameSelection.modify(alter, verticalDistance, direction, UserTriggered::No);

        bool change = shouldChangeSelection(trialFrameSelection.selection());
        if (!change)
            return false;
    }

    willBeModified(alter, direction == VerticalDirection::Up ? SelectionDirection::Backward : SelectionDirection::Forward);

    VisiblePosition pos;
    LayoutUnit xPos;
    switch (alter) {
    case Alteration::Move:
        pos = VisiblePosition(direction == VerticalDirection::Up ? m_selection.start() : m_selection.end(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(direction == VerticalDirection::Up ? PositionType::Start : PositionType::End);
        m_selection.setAffinity(direction == VerticalDirection::Up ? Affinity::Upstream : Affinity::Downstream);
        break;
    case Alteration::Extend:
        pos = VisiblePosition(m_selection.extent(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(PositionType::Extent);
        m_selection.setAffinity(Affinity::Downstream);
        break;
    }

    int startY;
    if (!absoluteCaretY(pos, startY))
        return false;
    if (direction == VerticalDirection::Up)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        if (direction == VerticalDirection::Up)
            next = previousLinePosition(p, xPos);
        else
            next = nextLinePosition(p, xPos);

        if (next.isNull() || next == p)
            break;
        int nextY;
        if (!absoluteCaretY(next, nextY))
            break;
        if (direction == VerticalDirection::Up)
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
    case Alteration::Move:
        moveTo(result, userTriggered, align);
        break;
    case Alteration::Extend:
        setExtent(result, userTriggered);
        break;
    }

    if (userTriggered == UserTriggered::Yes)
        m_granularity = TextGranularity::CharacterGranularity;

    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(m_document.get()) || alter == Alteration::Extend);

    return true;
}

LayoutUnit FrameSelection::lineDirectionPointForBlockDirectionNavigation(PositionType type)
{
    if (isNone())
        return 0;

    // FIXME: Can we use visibleStart/End/Extent?
    Position position;
    switch (type) {
    case PositionType::Start:
        position = m_selection.start();
        break;
    case PositionType::End:
        position = m_selection.end();
        break;
    case PositionType::Extent:
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
    caretAnimator().stop();
#endif

    if (auto* view = m_document->renderView())
        view->selection().clear();

    setSelectionWithoutUpdatingAppearance(VisibleSelection(), defaultSetSelectionOptions() | SetSelectionOption::DoNotNotifyEditorClients,
        CursorAlignOnScroll::IfNeeded, TextGranularity::CharacterGranularity);
    m_previousCaretNode = nullptr;
    m_typingStyle = nullptr;
}

void FrameSelection::setStart(const VisiblePosition& position, UserTriggered trigger)
{
    if (m_selection.isBaseFirst())
        setBase(position, trigger);
    else
        setExtent(position, trigger);
}

void FrameSelection::setEnd(const VisiblePosition& position, UserTriggered trigger)
{
    if (m_selection.isBaseFirst())
        setExtent(position, trigger);
    else
        setBase(position, trigger);
}

void FrameSelection::setBase(const VisiblePosition& position, UserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(position.deepEquivalent(), m_selection.extent(), position.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setExtent(const VisiblePosition& position, UserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(m_selection.base(), position.deepEquivalent(), position.affinity(), selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setBase(const Position& position, Affinity affinity, UserTriggered userTriggered)
{
    const bool selectionHasDirection = true;
    setSelection(VisibleSelection(position, m_selection.extent(), affinity, selectionHasDirection), defaultSetSelectionOptions(userTriggered));
}

void FrameSelection::setExtent(const Position& position, Affinity affinity, UserTriggered userTriggered)
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
    if (m_position.isNull())
        return nullptr;

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

static LayoutBoxExtent computeOutsetFromInnerOuterRect(const LayoutRect& innerRect, const LayoutRect& outerRect)
{
    LayoutBoxExtent result;
    result.setLeft(std::max<LayoutUnit>(0, innerRect.x() - outerRect.x()));
    result.setTop(std::max<LayoutUnit>(0, innerRect.y() - outerRect.y()));
    result.setRight(std::max<LayoutUnit>(0, outerRect.width() - innerRect.width()));
    result.setBottom(std::max<LayoutUnit>(0, outerRect.height() - innerRect.height()));

    return result;
}

static void repaintCaretForLocalRect(Node* node, const LayoutRect& rect, CaretAnimator* caretAnimator)
{
    if (CheckedPtr caretPainter = rendererForCaretPainting(node)) {
        LayoutRect adjustedRect = caretAnimator ? caretAnimator->caretRepaintRectForLocalRect(rect) : rect;
        if (adjustedRect == rect)
            caretPainter->repaintRectangle(rect);
        else
            caretPainter->repaintRectangle(rect, RenderObject::ClipRepaintToLayer::No, RenderObject::ForceRepaint::Yes, computeOutsetFromInnerOuterRect(rect, adjustedRect));
    }
}

bool FrameSelection::recomputeCaretRect()
{
    if (!shouldUpdateCaretRect())
        return false;

    auto document = protectedDocument();
    if (!document)
        return false;

    if (!document->view())
        return false;

    LayoutRect oldRect = localCaretRectWithoutUpdate();

    RefPtr<Node> caretNode = m_previousCaretNode;
    if (shouldUpdateCaretRect()) {
        if (!isNonOrphanedCaret(m_selection))
            clearCaretRect();
        else {
            VisiblePosition visibleStart = m_selection.visibleStart();
            if (updateCaretRect(*document, visibleStart)) {
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
    if (CheckedPtr view = document->renderView()) {
        bool previousOrNewCaretNodeIsContentEditable = m_selection.isContentEditable() || (m_previousCaretNode && m_previousCaretNode->isContentEditable());
        if (shouldRepaintCaret(view.get(), previousOrNewCaretNodeIsContentEditable)) {
            if (m_previousCaretNode)
                repaintCaretForLocalRect(m_previousCaretNode.get(), oldRect, m_caretAnimator.ptr());
            m_previousCaretNode = caretNode;
            repaintCaretForLocalRect(caretNode.get(), newRect, m_caretAnimator.ptr());
        }
    }
#endif
    return true;
}

bool CaretBase::shouldRepaintCaret(const RenderView* view, bool isContentEditable) const
{
    ASSERT(view);
    bool caretBrowsing = view->frameView().frame().settings().caretBrowsingEnabled(); // The frame where the selection started.
    return (caretBrowsing || isContentEditable);
}

void FrameSelection::invalidateCaretRect()
{
    if (!isCaret())
        return;

    CaretBase::invalidateCaretRect(m_selection.start().deprecatedNode(), recomputeCaretRect(), m_caretAnimator.ptr());
}

void CaretBase::invalidateCaretRect(Node* node, bool caretRectChanged, CaretAnimator* caretAnimator)
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

    if (CheckedPtr view = node->document().renderView()) {
        if (shouldRepaintCaret(view.get(), isEditableNode(*node)))
            repaintCaretForLocalRect(node, localCaretRectWithoutUpdate(), caretAnimator);
    }
}

void FrameSelection::paintCaret(GraphicsContext& context, const LayoutPoint& paintOffset)
{
    if (m_selection.isCaret() && m_selection.start().deprecatedNode())
        CaretBase::paintCaret(*m_selection.start().deprecatedNode(), context, paintOffset, m_caretAnimator.ptr());
}

Color CaretBase::computeCaretColor(const RenderStyle& elementStyle, const Node* node)
{
    // On iOS, we want to fall back to the tintColor, and only override if CSS has explicitly specified a custom color.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    UNUSED_PARAM(node);
    if (elementStyle.hasAutoCaretColor())
        return { };
    return elementStyle.colorResolvingCurrentColor(elementStyle.caretColor());
#elif HAVE(REDESIGNED_TEXT_CURSOR)
#if HAVE(APP_ACCENT_COLORS) && PLATFORM(MAC)
    auto appUsesCustomAccentColor = node && node->document().page() && node->document().page()->appUsesCustomAccentColor();
#else
    auto appUsesCustomAccentColor = false;
#endif

    if (elementStyle.hasAutoCaretColor() && (!elementStyle.hasExplicitlySetColor() || appUsesCustomAccentColor)) {
#if PLATFORM(MAC)
        auto cssColorValue = CSSValueAppleSystemControlAccent;
#else
        auto cssColorValue = CSSValueAppleSystemBlue;
#endif
        auto styleColorOptions = node->document().styleColorOptions(&elementStyle);
        auto systemAccentColor = RenderTheme::singleton().systemColor(cssColorValue, styleColorOptions | StyleColorOptions::UseSystemAppearance);
        return elementStyle.colorByApplyingColorFilter(systemAccentColor);
    }

    return elementStyle.visitedDependentColorWithColorFilter(CSSPropertyCaretColor);
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

void CaretBase::paintCaret(const Node& node, GraphicsContext& context, const LayoutPoint& paintOffset, CaretAnimator* caretAnimator) const
{
#if ENABLE(TEXT_CARET)
    auto caretPresentationProperties = caretAnimator ? caretAnimator->presentationProperties() : CaretAnimator::PresentationProperties();
    if (m_caretVisibility == CaretVisibility::Hidden || caretPresentationProperties.blinkState == CaretAnimator::PresentationProperties::BlinkState::Off)
        return;

    auto caret = localCaretRectWithoutUpdate();
    if (CheckedPtr renderer = rendererForCaretPainting(&node))
        renderer->flipForWritingMode(caret);
    caret.moveBy(paintOffset);
    if (caret.isEmpty())
        return;

    Color caretColor = Color::black;
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        element = node.parentElement();
    if (element && element->renderer())
        caretColor = CaretBase::computeCaretColor(element->renderer()->style(), &node);

    auto pixelSnappedCaretRect = snapRectToDevicePixels(caret, node.document().deviceScaleFactor());
    if (caretAnimator)
        caretAnimator->paint(context, pixelSnappedCaretRect, caretColor, paintOffset);
    else
        context.fillRect(pixelSnappedCaretRect, caretColor);
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(context);
    UNUSED_PARAM(paintOffset);
    UNUSED_PARAM(caretAnimator);
#endif
}

void FrameSelection::setCaretBlinkingSuspended(bool suspended)
{
    caretAnimator().setBlinkingSuspended(suspended);
}

bool FrameSelection::isCaretBlinkingSuspended() const
{
    return caretAnimator().isBlinkingSuspended();
}

void FrameSelection::caretAnimationDidUpdate(CaretAnimator&)
{
    invalidateCaretRect();
}

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
void FrameSelection::setPrefersNonBlinkingCursor(bool enabled)
{
    caretAnimator().setPrefersNonBlinkingCursor(enabled);
}
#endif

#if PLATFORM(MAC)
void FrameSelection::caretAnimatorInvalidated(CaretAnimatorType caretType)
{
    m_caretAnimator = createCaretAnimator(this, caretType);
    caretAnimationDidUpdate(m_caretAnimator);
    updateAppearance();
}
#endif

Document* FrameSelection::document()
{
    return m_document.get();
}

Node* FrameSelection::caretNode()
{
    return selection().visibleStart().deepEquivalent().deprecatedNode();
}

bool FrameSelection::contains(const LayoutPoint& point) const
{
    // Treat a collapsed selection like no selection.
    if (!isRange())
        return false;

    auto range = m_selection.firstRange();
    if (!range)
        return false;

    auto document = protectedDocument();
    if (!document)
        return false;

    HitTestResult result(point);
    document->hitTest(HitTestRequest(), result);
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

    return WebCore::contains<ComposedTree>(*range, makeBoundaryPoint(innerNode->renderer()->positionForPoint(result.localPoint(), HitTestSource::User, nullptr)));
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void FrameSelection::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    RefPtr document = protectedDocument();
    if (!document)
        return;
    RefPtr frame { document->frame() };
    if (!frame)
        return;
    RefPtr parent { dynamicDowncast<LocalFrame>(frame->tree().parent()) };
    if (!parent)
        return;
    Page* page = document->page();
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
    RefPtr ownerElement { document->ownerElement() };
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
        page->checkedFocusController()->setFocusedFrame(parent.get());
        // Previous focus can trigger DOM events, ensure the selection did not become orphan.
        if (newSelection.isOrphan())
            parent->selection().clear();
        else
            parent->selection().setSelection(newSelection);
    }
}

void FrameSelection::selectAll()
{
    RefPtr focusedElement = m_document->focusedElement();
    if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(focusedElement)) {
        if (selectElement->canSelectAll()) {
            selectElement->selectAll();
            return;
        }
    }

    RefPtr<Node> root;
    RefPtr<Node> selectStartTarget;
    if (m_selection.isContentEditable()) {
        root = highestEditableRoot(m_selection.start());
        if (RefPtr shadowRoot = m_selection.nonBoundaryShadowTreeRootNode())
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
        setSelection(newSelection, defaultSetSelectionOptions() | SetSelectionOption::FireSelectEvent, intent);
    }
}

bool FrameSelection::setSelectedRange(const std::optional<SimpleRange>& range, Affinity affinity, ShouldCloseTyping closeTyping, UserTriggered userTriggered)
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

    OptionSet<SetSelectionOption> selectionOptions { SetSelectionOption::ClearTypingStyle };
    if (closeTyping == ShouldCloseTyping::Yes)
        selectionOptions.add(SetSelectionOption::CloseTyping);

    if (userTriggered == UserTriggered::Yes) {
        FrameSelection trialFrameSelection;

        trialFrameSelection.setSelection(newSelection, selectionOptions);

        if (!shouldChangeSelection(trialFrameSelection.selection()))
            return false;

        selectionOptions.add(SetSelectionOption::IsUserTriggered);
    }

    setSelection(newSelection, selectionOptions);
    return true;
}

void FrameSelection::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();

    auto document = protectedDocument();
    document->updateStyleIfNeeded();

#if USE(UIKIT_EDITING)
    // Caret blinking (blinks | does not blink)
    if (activeAndFocused)
        setSelectionFromNone();
    setCaretVisible(activeAndFocused);
#else
    // Because RenderObject::selectionBackgroundColor() and
    // RenderObject::selectionForegroundColor() check if the frame is active,
    // we have to update places those colors were painted.
    if (CheckedPtr view = document->renderView())
        view->selection().repaint();

    // Caret appears in the active frame.
    if (activeAndFocused)
        setSelectionFromNone();
    setCaretVisibility(activeAndFocused ? CaretVisibility::Visible : CaretVisibility::Hidden, ShouldUpdateAppearance::Yes);
#endif
}

static Vector<Style::PseudoClassChangeInvalidation> invalidateFocusedElementAndShadowIncludingAncestors(Element* focusedElement, bool activeAndFocused)
{
    Vector<Style::PseudoClassChangeInvalidation> invalidations;
    for (RefPtr element = focusedElement; element; element = element->shadowHost()) {
        invalidations.append({ *element, { { CSSSelector::PseudoClass::Focus, activeAndFocused }, { CSSSelector::PseudoClass::FocusVisible, activeAndFocused } } });
        for (auto& lineage : lineageOfType<Element>(*element))
            invalidations.append({ lineage, CSSSelector::PseudoClass::FocusWithin, activeAndFocused });
    }
    return invalidations;
}

void FrameSelection::pageActivationChanged()
{
    bool isActive = isPageActive(m_document.get());
    RefPtr focusedElement = m_document->focusedElement();
    {
        auto invalidations = invalidateFocusedElementAndShadowIncludingAncestors(focusedElement.get(), m_focused && isActive);
        m_isActive = isActive;
    }

    focusedOrActiveStateChanged();
}

void FrameSelection::setFocused(bool isFocused)
{
    if (m_focused == isFocused)
        return;

    bool isActive = isPageActive(m_document.get());
    RefPtr focusedElement = m_document->focusedElement();
    {
        auto invalidations = invalidateFocusedElementAndShadowIncludingAncestors(focusedElement.get(), isFocused && isActive);
        m_focused = isFocused;
        m_isActive = isActive;
    }

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

    auto document = protectedDocument();
#if ENABLE(TEXT_CARET)
    bool paintBlockCursor = m_shouldShowBlockCursor && m_selection.isCaret() && !isLogicalEndOfLine(m_selection.visibleEnd());
    bool caretRectChangedOrCleared = recomputeCaretRect();

    bool caretBrowsing = document->settings().caretBrowsingEnabled();
    bool shouldBlink = !paintBlockCursor && caretIsVisible() && isCaret() && (oldSelection.isContentEditable() || caretBrowsing);

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (caretRectChangedOrCleared || !shouldBlink || shouldStopBlinkingDueToTypingCommand(document.get()))
        caretAnimator().stop(CaretAnimatorStopReason::CaretRectChanged);

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !caretAnimator().isActive()) {
        if (document && document->domWindow())
            caretAnimator().start();

        caretAnimator().setVisible(true);
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
        CheckedPtr view = document->renderView();
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
    if (CheckedPtr view = document->renderView(); startPos.isNotNull() && endPos.isNotNull() && selection.visibleStart() != selection.visibleEnd()) {
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
    caretAnimator().setVisible(false);

    CaretBase::setCaretVisibility(visibility);
#endif

    if (doAppearanceUpdate == ShouldUpdateAppearance::Yes)
        updateAppearance();
}

// Helper function that tells whether a particular node is an element that has an entire
// Frame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node& node)
{
    auto* renderer = dynamicDowncast<RenderWidget>(node.renderer());
    if (!renderer)
        return false;
    auto* widget = renderer->widget();
    return widget && widget->isLocalFrameView();
}

void FrameSelection::setFocusedElementIfNeeded(OptionSet<SetSelectionOption> options)
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

    if (RefPtr target = m_selection.rootEditableElement()) {
        // Walk up the DOM tree to search for an element to focus.
        while (target) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target->isMouseFocusable() && !isFrameElement(*target)) {
                FocusOptions focusOptions;
                if (options & SetSelectionOption::ForBindings)
                    focusOptions.trigger = FocusTrigger::Bindings;
                m_document->page()->checkedFocusController()->setFocusedElement(target.get(), *m_document->frame(), focusOptions);
                return;
            }
            target = target->parentOrShadowHostElement();
        }
        m_document->setFocusedElement(nullptr);
    }

    if (caretBrowsing)
        m_document->page()->checkedFocusController()->setFocusedElement(nullptr, *m_document->frame());
}

void DragCaretController::paintDragCaret(LocalFrame* frame, GraphicsContext& p, const LayoutPoint& paintOffset) const
{
#if ENABLE(TEXT_CARET)
    if (m_position.deepEquivalent().deprecatedNode() && m_position.deepEquivalent().deprecatedNode()->document().frame() == frame)
        paintCaret(*m_position.deepEquivalent().deprecatedNode(), p, paintOffset, nullptr);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(p);
    UNUSED_PARAM(paintOffset);
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
    CheckedPtr renderView = m_document->renderView();
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
static RefPtr<HTMLFormElement> scanForForm(Element* start)
{
    if (!start)
        return nullptr;
    for (Ref element : descendantsOfType<HTMLElement>(start->document())) {
        if (RefPtr form = dynamicDowncast<HTMLFormElement>(element))
            return form;
        if (element->isFormListedElement())
            return element->asFormListedElement()->form();
        if (RefPtr frameElement = dynamicDowncast<HTMLFrameElementBase>(element)) {
            if (RefPtr contentDocument = frameElement->contentDocument()) {
                if (RefPtr frameResult = scanForForm(contentDocument->documentElement()))
                    return frameResult;
            }
        }
    }
    return nullptr;
}

static ValidatedFormListedElement* findFormControlElementAncestor(Element& element)
{
    for (auto& ancestor : lineageOfType<Element>(element)) {
        if (auto* formControlAncestor = ancestor.asValidatedFormListedElement())
            return formControlAncestor;
    }
    return nullptr;
}

// We look for either the form containing the current focus, or for one immediately after it
RefPtr<HTMLFormElement> FrameSelection::currentForm() const
{
    // Start looking either at the active (first responder) node, or where the selection is.
    RefPtr start = m_document->focusedElement();
    if (!start)
        start = m_selection.start().anchorElementAncestor();
    if (!start)
        return nullptr;

    if (RefPtr form = lineageOfType<HTMLFormElement>(*start).first())
        return form;
    if (RefPtr formControl = findFormControlElementAncestor(*start))
        return formControl->form();

    // Try walking forward in the node tree to find a form element.
    return scanForForm(start.get());
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
        rect = revealExtentOption == RevealExtentOption::RevealExtent ? VisiblePosition(m_selection.extent()).absoluteCaretBounds() : enclosingIntRect(selectionBounds(ClipToVisibleContent::No));

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
    LocalFrameView::scrollRectToVisible(rect, *start.deprecatedNode()->renderer(), insideFixed, { revealMode, alignment, alignment, ShouldAllowCrossOriginScrolling::Yes, scrollBehavior });
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

    if (RefPtr body = m_document->body())
        setSelection(VisibleSelection(firstPositionInOrBeforeNode(body.get())));
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
    RefPtr selectStartTarget = m_selection.extent().containerNode();
    if (!selectStartTarget)
        return true;

    auto event = Event::create(eventNames().selectstartEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes);
    selectStartTarget->dispatchEvent(event);
    return !event->defaultPrevented();
}

void FrameSelection::setShouldShowBlockCursor(bool shouldShowBlockCursor)
{
    m_shouldShowBlockCursor = shouldShowBlockCursor;

    protectedDocument()->updateLayoutIgnorePendingStylesheets();

    updateAppearance();
}

void FrameSelection::updateAppearanceAfterUpdatingRendering()
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

std::optional<SimpleRange> FrameSelection::rangeByExtendingCurrentSelection(TextGranularity granularity) const
{
    if (m_selection.isNone())
        return std::nullopt;

    FrameSelection frameSelection;
    frameSelection.setSelection(m_selection);

    frameSelection.modify(Alteration::Move, SelectionDirection::Backward, granularity);
    frameSelection.modify(Alteration::Extend, SelectionDirection::Forward, granularity);

    return frameSelection.selection().toNormalizedRange();
}

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
        int count = std::abs(amount);
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
            return deprecatedIsSpaceOrNewline(c) || c == noBreakSpace || (u_ispunct(c) && c != ',' && c != '-' && c != '\'');
    }
    return true;
}

std::optional<SimpleRange> FrameSelection::rangeByMovingCurrentSelection(int amount) const
{
    return rangeByAlteringCurrentSelection(Alteration::Move, amount);
}

std::optional<SimpleRange> FrameSelection::rangeByExtendingCurrentSelection(int amount) const
{
    return rangeByAlteringCurrentSelection(Alteration::Extend, amount);
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
        if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace) {
            // End of paragraph with space.
            return VisibleSelection();
        }
    }

    // If at end of paragraph, move backwards one character.
    // This has the effect of selecting the word on the line (which is
    // what we want, rather than selecting past the end of the line).
    if (isEndOfParagraph(endVisiblePosBeforeExpansion) && !isStartOfParagraph(endVisiblePosBeforeExpansion))
        frameSelection.modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::CharacterGranularity);

    VisibleSelection newSelection = frameSelection.selection();
    newSelection.expandUsingGranularity(TextGranularity::WordGranularity);
    frameSelection.setSelection(newSelection, defaultSetSelectionOptions(), AXTextStateChangeIntent(), CursorAlignOnScroll::IfNeeded, frameSelection.granularity());

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
        if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace) {
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
        if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace) {
            // Space at end of line
            return VisibleSelection();
        }
        frameSelection.moveTo(startVisiblePos);
        frameSelection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::WordGranularity);
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
        if (!deprecatedIsSpaceOrNewline(c) && c != noBreakSpace)
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
            if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace)
                sawSpace = true;
            else
                return c == '.' || c == '!' || c == '?';
        }
    }
    return true;
}

std::optional<SimpleRange> FrameSelection::rangeByAlteringCurrentSelection(Alteration alteration, int amount) const
{
    if (m_selection.isNone())
        return std::nullopt;

    if (!amount)
        return m_selection.toNormalizedRange();

    FrameSelection frameSelection;
    frameSelection.setSelection(m_selection);
    SelectionDirection direction = amount > 0 ? SelectionDirection::Forward : SelectionDirection::Backward;
    for (int i = 0; i < std::abs(amount); i++)
        frameSelection.modify(alteration, direction, TextGranularity::CharacterGranularity);
    return frameSelection.selection().toNormalizedRange();
}

void FrameSelection::clearCurrentSelection()
{
    setSelection(VisibleSelection());
}

void FrameSelection::setCaretColor(const Color& caretColor)
{
    if (m_caretColor != caretColor) {
        m_caretColor = caretColor;
        if (caretIsVisible() && isCaret())
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
        auto start = makeContainerOffsetPosition(m_associatedLiveRange->protectedStartContainer(), m_associatedLiveRange->startOffset());
        auto end = makeContainerOffsetPosition(m_associatedLiveRange->protectedEndContainer(), m_associatedLiveRange->endOffset());
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
