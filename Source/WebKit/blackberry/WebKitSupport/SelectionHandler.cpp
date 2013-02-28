/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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
#include "SelectionHandler.h"

#include "DOMSupport.h"
#include "Document.h"
#include "FatFingers.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "InputHandler.h"
#include "IntRect.h"
#include "SelectionOverlay.h"
#include "TouchEventHandler.h"
#include "WebPageClient.h"
#include "WebPage_p.h"

#include "htmlediting.h"
#include "visible_units.h"

#include <BlackBerryPlatformKeyboardEvent.h>
#include <BlackBerryPlatformLog.h>

#include <sys/keycodes.h>

// Note: This generates a lot of logs when dumping rects lists. It will seriously
// impact performance. Do not enable this during performance tests.
#define SHOWDEBUG_SELECTIONHANDLER 0
#define SHOWDEBUG_SELECTIONHANDLER_TIMING 0

using namespace BlackBerry::Platform;
using namespace WebCore;

#if SHOWDEBUG_SELECTIONHANDLER
#define SelectionLog(severity, format, ...) Platform::logAlways(severity, format, ## __VA_ARGS__)
#else
#define SelectionLog(severity, format, ...)
#endif // SHOWDEBUG_SELECTIONHANDLER

#if SHOWDEBUG_SELECTIONHANDLER_TIMING
#define SelectionTimingLog(severity, format, ...) Platform::logAlways(severity, format, ## __VA_ARGS__)
#else
#define SelectionTimingLog(severity, format, ...)
#endif // SHOWDEBUG_SELECTIONHANDLER_TIMING

namespace BlackBerry {
namespace WebKit {

SelectionHandler::SelectionHandler(WebPagePrivate* page)
    : m_webPage(page)
    , m_selectionActive(false)
    , m_caretActive(false)
    , m_lastUpdatedEndPointIsValid(false)
    , m_didSuppressCaretPositionChangedNotification(false)
{
}

SelectionHandler::~SelectionHandler()
{
}

void SelectionHandler::cancelSelection()
{
    m_selectionActive = false;
    m_lastSelectionRegion = IntRectRegion();

    if (m_webPage->m_selectionOverlay)
        m_webPage->m_selectionOverlay->hide();
    // Notify client with empty selection to ensure the handles are removed if
    // rendering happened prior to processing on webkit thread
    m_webPage->m_client->notifySelectionDetailsChanged(SelectionDetails());

    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::cancelSelection");

    if (m_webPage->m_inputHandler->isInputMode())
        m_webPage->m_inputHandler->cancelSelection();
    else
        m_webPage->focusedOrMainFrame()->selection()->clear();
}

BlackBerry::Platform::String SelectionHandler::selectedText() const
{
    return m_webPage->focusedOrMainFrame()->editor()->selectedText();
}

WebCore::IntRect SelectionHandler::clippingRectForVisibleContent() const
{
    // Get the containing content rect for the frame.
    Frame* frame = m_webPage->focusedOrMainFrame();
    WebCore::IntRect clipRect = WebCore::IntRect(WebCore::IntPoint(0, 0), frame->view()->contentsSize());
    if (frame != m_webPage->mainFrame()) {
        clipRect = m_webPage->getRecursiveVisibleWindowRect(frame->view(), true /* no clip to main frame window */);
        clipRect = m_webPage->m_mainFrame->view()->windowToContents(clipRect);
    }

    // Get the input field containing box.
    WebCore::IntRect inputBoundingBox = m_webPage->m_inputHandler->boundingBoxForInputField();
    if (!inputBoundingBox.isEmpty()) {
        // Adjust the bounding box to the frame offset.
        inputBoundingBox = m_webPage->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(inputBoundingBox));
        clipRect.intersect(inputBoundingBox);
    }
    return clipRect;
}

void SelectionHandler::regionForTextQuads(Vector<FloatQuad> &quadList, IntRectRegion& region, bool shouldClipToVisibleContent) const
{
    ASSERT(region.isEmpty());

    if (!quadList.isEmpty()) {
        FrameView* frameView = m_webPage->focusedOrMainFrame()->view();

        // frameRect is in frame coordinates.
        WebCore::IntRect frameRect(WebCore::IntPoint(0, 0), frameView->contentsSize());

        // framePosition is in main frame coordinates.
        WebCore::IntPoint framePosition = m_webPage->frameOffset(m_webPage->focusedOrMainFrame());

        // Get the visibile content rect.
        WebCore::IntRect clippingRect = shouldClipToVisibleContent ? clippingRectForVisibleContent() : WebCore::IntRect(-1, -1, 0, 0);

        // Convert the text quads into a more platform friendy
        // IntRectRegion and adjust for subframes.
        Platform::IntRect selectionBoundingBox;
        std::vector<Platform::IntRect> adjustedIntRects;
        for (unsigned i = 0; i < quadList.size(); i++) {
            WebCore::IntRect enclosingRect = quadList[i].enclosingBoundingBox();
            enclosingRect.intersect(frameRect);
            enclosingRect.move(framePosition.x(), framePosition.y());

            // Clip to the visible content.
            if (clippingRect.location() != DOMSupport::InvalidPoint)
                enclosingRect.intersect(clippingRect);

            adjustedIntRects.push_back(enclosingRect);
            selectionBoundingBox = unionOfRects(enclosingRect, selectionBoundingBox);
        }
        region = IntRectRegion(selectionBoundingBox, adjustedIntRects.size(), adjustedIntRects);
    }
}

static VisiblePosition visiblePositionForPointIgnoringClipping(const Frame& frame, const WebCore::IntPoint& framePoint)
{
    // Frame::visiblePositionAtPoint hard-codes ignoreClipping=false in the
    // call to hitTestResultAtPoint. This has a bug where some pages (such as
    // metafilter) will return the wrong VisiblePosition for points that are
    // outside the visible rect. To work around the bug, this is a copy of
    // visiblePositionAtPoint which which passes ignoreClipping=true.
    // See RIM Bug #4315.
    HitTestResult result = frame.eventHandler()->hitTestResultAtPoint(framePoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowShadowContent | HitTestRequest::IgnoreClipping);

    Node* node = result.innerNode();
    if (!node || node->document() != frame.document())
        return VisiblePosition();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    VisiblePosition visiblePos = renderer->positionForPoint(result.localPoint());
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(Position(createLegacyEditingPosition(node, 0)));

    return visiblePos;
}

static unsigned directionOfPointRelativeToRect(const WebCore::IntPoint& point, const WebCore::IntRect& rect, const bool useTopPadding = true, const bool useBottomPadding = true)
{
    ASSERT(!rect.contains(point));

    // Padding to prevent accidental trigger of up/down when intending to do horizontal movement.
    const int verticalPadding = 5;

    // Do height movement check first but add padding. We may be off on both x & y axis and only
    // want to move in one direction at a time.
    if (point.y() - (useTopPadding ? verticalPadding : 0) < rect.y())
        return KEYCODE_UP;
    if (point.y() > rect.maxY() + (useBottomPadding ? verticalPadding : 0))
        return KEYCODE_DOWN;
    if (point.x() < rect.location().x())
        return KEYCODE_LEFT;
    if (point.x() > rect.maxX())
        return KEYCODE_RIGHT;

    return 0;
}

bool SelectionHandler::shouldUpdateSelectionOrCaretForPoint(const WebCore::IntPoint& point, const WebCore::IntRect& caretRect, bool startCaret) const
{
    ASSERT(m_webPage->m_inputHandler->isInputMode());

    // If the point isn't valid don't block change as it is not actually changing.
    if (point == DOMSupport::InvalidPoint)
        return true;

    VisibleSelection currentSelection = m_webPage->focusedOrMainFrame()->selection()->selection();

    // If the input field is single line or we are on the first or last
    // line of a multiline input field only horizontal movement is supported.
    bool aboveCaret = point.y() < caretRect.y();
    bool belowCaret = point.y() >= caretRect.maxY();

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::shouldUpdateSelectionOrCaretForPoint multiline = %s above = %s below = %s first line = %s last line = %s start = %s",
        m_webPage->m_inputHandler->isMultilineInputMode() ? "true" : "false",
        aboveCaret ? "true" : "false",
        belowCaret ? "true" : "false",
        inSameLine(currentSelection.visibleStart(), startOfEditableContent(currentSelection.visibleStart())) ? "true" : "false",
        inSameLine(currentSelection.visibleEnd(), endOfEditableContent(currentSelection.visibleEnd())) ? "true" : "false",
        startCaret ? "true" : "false");

    if (!m_webPage->m_inputHandler->isMultilineInputMode() && (aboveCaret || belowCaret))
        return false;
    if (startCaret && inSameLine(currentSelection.visibleStart(), startOfEditableContent(currentSelection.visibleStart())) && aboveCaret)
        return false;
    if (!startCaret && inSameLine(currentSelection.visibleEnd(), endOfEditableContent(currentSelection.visibleEnd())) && belowCaret)
        return false;

    return true;
}

void SelectionHandler::setCaretPosition(const WebCore::IntPoint& position)
{
    if (!m_webPage->m_inputHandler->isInputMode() || !m_webPage->focusedOrMainFrame()->document()->focusedNode())
        return;

    m_caretActive = true;

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::setCaretPosition requested point %s",
        Platform::IntPoint(position).toString().c_str());

    Frame* focusedFrame = m_webPage->focusedOrMainFrame();
    FrameSelection* controller = focusedFrame->selection();
    WebCore::IntPoint relativePoint = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), focusedFrame, position);
    WebCore::IntRect currentCaretRect = controller->selection().visibleStart().absoluteCaretBounds();

    if (relativePoint == DOMSupport::InvalidPoint || !shouldUpdateSelectionOrCaretForPoint(relativePoint, currentCaretRect)) {
        selectionPositionChanged(true /* forceUpdateWithoutChange */);
        return;
    }

    VisiblePosition visibleCaretPosition(focusedFrame->visiblePositionForPoint(relativePoint));

    if (RenderObject* focusedRenderer = focusedFrame->document()->focusedNode()->renderer()) {
        WebCore::IntRect nodeOutlineBounds(focusedRenderer->absoluteOutlineBounds());
        if (!nodeOutlineBounds.contains(relativePoint)) {
            if (unsigned character = directionOfPointRelativeToRect(relativePoint, currentCaretRect))
                m_webPage->m_inputHandler->handleKeyboardInput(Platform::KeyboardEvent(character));

            // Send the selection changed in case this does not trigger a selection change to
            // ensure the caret position is accurate. This may be a duplicate event.
            selectionPositionChanged(true /* forceUpdateWithoutChange */);
            return;
        }
    }

    VisibleSelection newSelection(visibleCaretPosition);
    if (controller->selection() == newSelection) {
        selectionPositionChanged(true /* forceUpdateWithoutChange */);
        return;
    }

    controller->setSelection(newSelection);

    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::setCaretPosition point valid, cursor updated");
}

void SelectionHandler::inputHandlerDidFinishProcessingChange()
{
    if (m_didSuppressCaretPositionChangedNotification)
        notifyCaretPositionChangedIfNeeded(false);
}

// This function makes sure we are not reducing the selection to a caret selection.
static bool shouldExtendSelectionInDirection(const VisibleSelection& selection, unsigned character)
{
    FrameSelection tempSelection;
    tempSelection.setSelection(selection);
    switch (character) {
    case KEYCODE_LEFT:
        tempSelection.modify(FrameSelection::AlterationExtend, DirectionLeft, CharacterGranularity);
        break;
    case KEYCODE_RIGHT:
        tempSelection.modify(FrameSelection::AlterationExtend, DirectionRight, CharacterGranularity);
        break;
    case KEYCODE_UP:
        tempSelection.modify(FrameSelection::AlterationExtend, DirectionBackward, LineGranularity);
        break;
    case KEYCODE_DOWN:
        tempSelection.modify(FrameSelection::AlterationExtend, DirectionForward, LineGranularity);
        break;
    default:
        break;
    }

    if ((character == KEYCODE_LEFT || character == KEYCODE_RIGHT)
        && (!inSameLine(selection.visibleStart(), tempSelection.selection().visibleStart())
           || !inSameLine(selection.visibleEnd(), tempSelection.selection().visibleEnd())))
        return false;

    return tempSelection.selection().selectionType() == VisibleSelection::RangeSelection;
}

static int clamp(const int min, const int value, const int max)
{
    return value < min ? min : std::min(value, max);
}

static VisiblePosition directionalVisiblePositionAtExtentOfBox(Frame* frame, const WebCore::IntRect& boundingBox, unsigned direction, const WebCore::IntPoint& basePoint)
{
    ASSERT(frame);

    if (!frame)
        return VisiblePosition();

    switch (direction) {
    case KEYCODE_LEFT:
        // Extend x to start and clamp y to the edge of bounding box.
        return frame->visiblePositionForPoint(WebCore::IntPoint(boundingBox.x(), clamp(boundingBox.y(), basePoint.y(), boundingBox.maxY())));
    case KEYCODE_RIGHT:
        // Extend x to end and clamp y to the edge of bounding box.
        return frame->visiblePositionForPoint(WebCore::IntPoint(boundingBox.maxX(), clamp(boundingBox.y(), basePoint.y(), boundingBox.maxY())));
    case KEYCODE_UP:
        // Extend y to top and clamp x to the edge of bounding box.
        return frame->visiblePositionForPoint(WebCore::IntPoint(clamp(boundingBox.x(), basePoint.x(), boundingBox.maxX()), boundingBox.y()));
    case KEYCODE_DOWN:
        // Extend y to bottom and clamp x to the edge of bounding box.
        return frame->visiblePositionForPoint(WebCore::IntPoint(clamp(boundingBox.x(), basePoint.x(), boundingBox.maxX()), boundingBox.maxY()));
    default:
        break;
    }

    return frame->visiblePositionForPoint(WebCore::IntPoint(basePoint.x(), basePoint.y()));
}

static bool pointIsOutsideOfBoundingBoxInDirection(unsigned direction, const WebCore::IntPoint& selectionPoint, const WebCore::IntRect& boundingBox)
{
    if ((direction == KEYCODE_LEFT && selectionPoint.x() < boundingBox.x())
        || (direction == KEYCODE_UP && selectionPoint.y() < boundingBox.y())
        || (direction == KEYCODE_RIGHT && selectionPoint.x() > boundingBox.maxX())
        || (direction == KEYCODE_DOWN && selectionPoint.y() > boundingBox.maxY()))
        return true;

    return false;
}

unsigned SelectionHandler::extendSelectionToFieldBoundary(bool isStartHandle, const WebCore::IntPoint& selectionPoint, VisibleSelection& newSelection)
{
    Frame* focusedFrame = m_webPage->focusedOrMainFrame();
    if (!focusedFrame->document()->focusedNode() || !focusedFrame->document()->focusedNode()->renderer())
        return 0;

    FrameSelection* controller = focusedFrame->selection();

    WebCore::IntRect caretRect = isStartHandle ? controller->selection().visibleStart().absoluteCaretBounds()
                                      : controller->selection().visibleEnd().absoluteCaretBounds();

    WebCore::IntRect nodeBoundingBox = focusedFrame->document()->focusedNode()->renderer()->absoluteBoundingBoxRect();
    nodeBoundingBox.inflate(-1);

    // Start handle is outside of the field. Treat it as the changed handle and move
    // relative to the start caret rect.
    unsigned character = directionOfPointRelativeToRect(selectionPoint, caretRect, isStartHandle /* useTopPadding */, !isStartHandle /* useBottomPadding */);

    // Prevent incorrect movement, handles can only extend the selection this way
    // to prevent inversion of the handles.
    if (isStartHandle && (character == KEYCODE_RIGHT || character == KEYCODE_DOWN)
        || !isStartHandle && (character == KEYCODE_LEFT || character == KEYCODE_UP))
        character = 0;

    VisiblePosition newVisiblePosition = isStartHandle ? controller->selection().extent() : controller->selection().base();
    // Extend the selection to the bounds of the box before doing incremental scroll if the point is outside the node.
    // Don't extend selection and handle the character at the same time.
    if (pointIsOutsideOfBoundingBoxInDirection(character, selectionPoint, nodeBoundingBox))
        newVisiblePosition = directionalVisiblePositionAtExtentOfBox(focusedFrame, nodeBoundingBox, character, selectionPoint);

    if (isStartHandle)
        newSelection = VisibleSelection(newVisiblePosition, newSelection.extent(), true /* isDirectional */);
    else
        newSelection = VisibleSelection(newSelection.base(), newVisiblePosition, true /* isDirectional */);

    // If no selection will be changed, return the character to extend using navigation.
    if (controller->selection() == newSelection)
        return character;

    // Selection has been updated.
    return 0;
}

// Returns true if handled.
bool SelectionHandler::updateOrHandleInputSelection(VisibleSelection& newSelection, const WebCore::IntPoint& relativeStart
                                                    , const WebCore::IntPoint& relativeEnd)
{
    ASSERT(m_webPage->m_inputHandler->isInputMode());

    Frame* focusedFrame = m_webPage->focusedOrMainFrame();
    Node* focusedNode = focusedFrame->document()->focusedNode();
    if (!focusedNode || !focusedNode->renderer())
        return false;

    FrameSelection* controller = focusedFrame->selection();

    WebCore::IntRect currentStartCaretRect = controller->selection().visibleStart().absoluteCaretBounds();
    WebCore::IntRect currentEndCaretRect = controller->selection().visibleEnd().absoluteCaretBounds();

    // Check if the handle movement is valid.
    if (!shouldUpdateSelectionOrCaretForPoint(relativeStart, currentStartCaretRect, true /* startCaret */)
        || !shouldUpdateSelectionOrCaretForPoint(relativeEnd, currentEndCaretRect, false /* startCaret */)) {
        selectionPositionChanged(true /* forceUpdateWithoutChange */);
        return true;
    }

    WebCore::IntRect nodeBoundingBox = focusedNode->renderer()->absoluteBoundingBoxRect();

    // Only do special handling if one handle is outside of the node.
    bool startIsOutsideOfField = relativeStart != DOMSupport::InvalidPoint && !nodeBoundingBox.contains(relativeStart);
    bool endIsOutsideOfField = relativeEnd != DOMSupport::InvalidPoint && !nodeBoundingBox.contains(relativeEnd);
    if (startIsOutsideOfField && endIsOutsideOfField)
        return false;

    unsigned character = 0;
    bool needToInvertDirection = false;
    if (startIsOutsideOfField) {
        character = extendSelectionToFieldBoundary(true /* isStartHandle */, relativeStart, newSelection);
        if (character && controller->selection().isBaseFirst()) {
            // Invert the selection so that the cursor point is at the beginning.
            controller->setSelection(VisibleSelection(controller->selection().end(), controller->selection().start(), true /* isDirectional */));
            needToInvertDirection = true;
        }
    } else if (endIsOutsideOfField) {
        character = extendSelectionToFieldBoundary(false /* isStartHandle */, relativeEnd, newSelection);
        if (character && !controller->selection().isBaseFirst()) {
            // Reset the selection so that the end is the edit point.
            controller->setSelection(VisibleSelection(controller->selection().start(), controller->selection().end(), true /* isDirectional */));
        }
    }

    if (!character)
        return false;

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::updateOrHandleInputSelection making selection change attempt using key event %d",
        character);

    if (shouldExtendSelectionInDirection(controller->selection(), character))
        m_webPage->m_inputHandler->handleKeyboardInput(Platform::KeyboardEvent(character, Platform::KeyboardEvent::KeyDown, KEYMOD_SHIFT));

    if (needToInvertDirection)
        controller->setSelection(VisibleSelection(controller->selection().extent(), controller->selection().base(), true /* isDirectional */));

    // Send the selection changed in case this does not trigger a selection change to
    // ensure the caret position is accurate. This may be a duplicate event.
    selectionPositionChanged(true /* forceUpdateWithoutChange */);
    return true;
}

void SelectionHandler::setSelection(const WebCore::IntPoint& start, const WebCore::IntPoint& end)
{
    m_selectionActive = true;

    ASSERT(m_webPage);
    ASSERT(m_webPage->focusedOrMainFrame());
    ASSERT(m_webPage->focusedOrMainFrame()->selection());

    Frame* focusedFrame = m_webPage->focusedOrMainFrame();
    FrameSelection* controller = focusedFrame->selection();

#if SHOWDEBUG_SELECTIONHANDLER_TIMING
    m_timer.start();
#endif

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::setSelection adjusted points %s, %s",
        Platform::IntPoint(start).toString().c_str(),
        Platform::IntPoint(end).toString().c_str());

    // Note that IntPoint(-1, -1) is being our sentinel so far for
    // clipped out selection starting or ending location.
    bool startIsValid = start != DOMSupport::InvalidPoint;
    m_lastUpdatedEndPointIsValid = end != DOMSupport::InvalidPoint;

    // At least one of the locations must be valid.
    ASSERT(startIsValid || m_lastUpdatedEndPointIsValid);

    WebCore::IntPoint relativeStart = start;
    WebCore::IntPoint relativeEnd = end;

    VisibleSelection newSelection(controller->selection());

    // We need the selection to be ordered base then extent.
    if (!controller->selection().isBaseFirst())
        controller->setSelection(VisibleSelection(controller->selection().start(), controller->selection().end(), true /* isDirectional */));

    // We don't return early in the following, so that we can do input field scrolling if the
    // handle is outside the bounds of the field. This can be extended to handle sub-region
    // scrolling as well
    if (startIsValid) {
        relativeStart = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), focusedFrame, start);

        VisiblePosition base = visiblePositionForPointIgnoringClipping(*focusedFrame, clipPointToVisibleContainer(start));
        if (base.isNotNull()) {
            // The function setBase validates the "base"
            newSelection.setBase(base);
            newSelection.setWithoutValidation(newSelection.base(), controller->selection().end());
            // Don't return early.
        }
    }

    if (m_lastUpdatedEndPointIsValid) {
        relativeEnd = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), focusedFrame, end);

        VisiblePosition extent = visiblePositionForPointIgnoringClipping(*focusedFrame, clipPointToVisibleContainer(end));
        if (extent.isNotNull()) {
            // The function setExtent validates the "extent"
            newSelection.setExtent(extent);
            newSelection.setWithoutValidation(controller->selection().start(), newSelection.extent());
            // Don't return early.
        }
    }

    newSelection.setIsDirectional(true);

    if (m_webPage->m_inputHandler->isInputMode()) {
        if (updateOrHandleInputSelection(newSelection, relativeStart, relativeEnd))
            return;
    }

    if (controller->selection() == newSelection) {
        selectionPositionChanged(true /* forceUpdateWithoutChange */);
        return;
    }

    // If the selection size is reduce to less than a character, selection type becomes
    // Caret. As long as it is still a range, it's a valid selection. Selection cannot
    // be cancelled through this function.
    Vector<FloatQuad> quads;
    DOMSupport::visibleTextQuads(newSelection, quads);

    IntRectRegion unclippedRegion;
    regionForTextQuads(quads, unclippedRegion, false /* shouldClipToVisibleContent */);

    if (unclippedRegion.isEmpty()) {
        // Requested selection results in an empty selection, skip this change.
        selectionPositionChanged(true /* forceUpdateWithoutChange */);

        SelectionLog(Platform::LogLevelWarn, "SelectionHandler::setSelection selection points invalid, selection not updated.");
        return;
    }

    // Check if the handles reversed position.
    if (m_selectionActive && !newSelection.isBaseFirst())
        m_webPage->m_client->notifySelectionHandlesReversed();

    controller->setSelection(newSelection);
    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::setSelection selection points valid, selection updated.");
}

// FIXME re-use this in context. Must be updated to include an option to return the href.
// This function should be moved to a new unit file. Names suggetions include DOMQueries
// and NodeTypes. Functions currently in InputHandler.cpp, SelectionHandler.cpp and WebPage.cpp
// can all be moved in.
static Node* enclosingLinkEventParentForNode(Node* node)
{
    if (!node)
        return 0;

    Node* linkNode = node->enclosingLinkEventParentOrSelf();
    return linkNode && linkNode->isLink() ? linkNode : 0;
}

bool SelectionHandler::selectNodeIfFatFingersResultIsLink(FatFingersResult fatFingersResult)
{
    if (!fatFingersResult.isValid())
        return false;
    Node* targetNode = fatFingersResult.node(FatFingersResult::ShadowContentNotAllowed);
    ASSERT(targetNode);
    // If the node at the point is a link, focus on the entire link, not a word.
    if (Node* link = enclosingLinkEventParentForNode(targetNode)) {
        selectObject(link);
        return true;
    }
    return false;
}

void SelectionHandler::selectAtPoint(const WebCore::IntPoint& location)
{
    // If point is invalid trigger selection based expansion.
    if (location == DOMSupport::InvalidPoint) {
        selectObject(WordGranularity);
        return;
    }

    WebCore::IntPoint targetPosition;
    // FIXME: Factory this get right fat finger code into a helper.
    const FatFingersResult lastFatFingersResult = m_webPage->m_touchEventHandler->lastFatFingersResult();
    if (selectNodeIfFatFingersResultIsLink(lastFatFingersResult))
        return;

    if (lastFatFingersResult.resultMatches(location, FatFingers::Text) && lastFatFingersResult.positionWasAdjusted() && lastFatFingersResult.nodeAsElementIfApplicable()) {
        targetNode = lastFatFingersResult.node(FatFingersResult::ShadowContentNotAllowed);
        targetPosition = lastFatFingersResult.adjustedPosition();
    } else {
        FatFingersResult newFatFingersResult = FatFingers(m_webPage, location, FatFingers::Text).findBestPoint();
        if (!newFatFingersResult.positionWasAdjusted())
            return;

        if (selectNodeIfFatFingersResultIsLink(newFatFingersResult))
            return;

        targetPosition = newFatFingersResult.adjustedPosition();
    }

    // selectAtPoint API currently only supports WordGranularity but may be extended in the future.
    selectObject(targetPosition, WordGranularity);
}

static bool expandSelectionToGranularity(Frame* frame, VisibleSelection selection, TextGranularity granularity, bool isInputMode)
{
    ASSERT(frame);
    ASSERT(frame->selection());

    if (!(selection.start().anchorNode() && selection.start().anchorNode()->isTextNode()))
        return false;

    if (granularity == WordGranularity)
        selection = DOMSupport::visibleSelectionForClosestActualWordStart(selection);

    selection.expandUsingGranularity(granularity);
    selection.setAffinity(frame->selection()->affinity());

    if (isInputMode && !frame->selection()->shouldChangeSelection(selection))
        return false;

    frame->selection()->setSelection(selection);
    return true;
}

void SelectionHandler::selectObject(const WebCore::IntPoint& location, TextGranularity granularity)
{
    ASSERT(location.x() >= 0 && location.y() >= 0);
    ASSERT(m_webPage && m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());
    Frame* focusedFrame = m_webPage->focusedOrMainFrame();

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::selectObject adjusted points %s",
        Platform::IntPoint(location).toString().c_str());

    WebCore::IntPoint relativePoint = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), focusedFrame, location);
    // Clear input focus if we're not selecting in old input field.
    if (!m_webPage->m_inputHandler->boundingBoxForInputField().contains(relativePoint))
        m_webPage->clearFocusNode();

    VisiblePosition pointLocation(focusedFrame->visiblePositionForPoint(relativePoint));
    VisibleSelection selection = VisibleSelection(pointLocation, pointLocation);

    m_selectionActive = expandSelectionToGranularity(focusedFrame, selection, granularity, m_webPage->m_inputHandler->isInputMode());
}

void SelectionHandler::selectObject(TextGranularity granularity)
{
    ASSERT(m_webPage && m_webPage->m_inputHandler);
    // Using caret location, must be inside an input field.
    if (!m_webPage->m_inputHandler->isInputMode())
        return;

    ASSERT(m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());
    Frame* focusedFrame = m_webPage->focusedOrMainFrame();

    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::selectObject using current selection");

    ASSERT(focusedFrame->selection()->selectionType() != VisibleSelection::NoSelection);

    // Use the current selection as the selection point.
    VisibleSelection selectionOrigin = focusedFrame->selection()->selection();

    // If this is the end of the input field, make sure we select the last word.
    if (m_webPage->m_inputHandler->isCaretAtEndOfText())
        selectionOrigin = previousWordPosition(selectionOrigin.start());

    m_selectionActive = expandSelectionToGranularity(focusedFrame, selectionOrigin, granularity, true /* isInputMode */);
}

void SelectionHandler::selectObject(Node* node)
{
    if (!node)
        return;

    // Clear input focus if we're not selecting text there.
    if (node != m_webPage->m_inputHandler->currentFocusElement().get())
        m_webPage->clearFocusNode();

    m_selectionActive = true;

    ASSERT(m_webPage && m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());
    Frame* focusedFrame = m_webPage->focusedOrMainFrame();

    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::selectNode");

    VisibleSelection selection = VisibleSelection::selectionFromContentsOfNode(node);
    focusedFrame->selection()->setSelection(selection);
}

static TextDirection directionOfEnclosingBlock(FrameSelection* selection)
{
    Node* enclosingBlockNode = enclosingBlock(selection->selection().extent().deprecatedNode());
    if (!enclosingBlockNode)
        return LTR;

    if (RenderObject* renderer = enclosingBlockNode->renderer())
        return renderer->style()->direction();

    return LTR;
}

// Returns > 0 if p1 is "closer" to referencePoint, < 0 if p2 is "closer", 0 if they are equidistant.
// Because text is usually arranged in horizontal rows, distance is measured along the y-axis, with x-axis used only to break ties.
// If rightGravity is true, the right-most x-coordinate is chosen, otherwise teh left-most coordinate is chosen.
static inline int comparePointsToReferencePoint(const WebCore::IntPoint& p1, const WebCore::IntPoint& p2, const WebCore::IntPoint& referencePoint, bool rightGravity)
{
    int dy1 = abs(referencePoint.y() - p1.y());
    int dy2 = abs(referencePoint.y() - p2.y());
    if (dy1 != dy2)
        return dy2 - dy1;

    // Same y-coordinate, choose the farthest right (or left) point.
    if (p1.x() == p2.x())
        return 0;

    if (p1.x() > p2.x())
        return rightGravity ? 1 : -1;

    return rightGravity ? -1 : 1;
}

// NOTE/FIXME: Due to r77286, we are getting off-by-one results in the IntRect class counterpart implementation of the
//             methods below. As done in r89803, r77928 and a few others, lets use local method to fix it.
//             We should keep our eyes very open on it, since it can affect BackingStore very badly.
static WebCore::IntPoint minXMinYCorner(const WebCore::IntRect& rect) { return rect.location(); } // typically topLeft
static WebCore::IntPoint maxXMinYCorner(const WebCore::IntRect& rect) { return WebCore::IntPoint(rect.x() + rect.width() - 1, rect.y()); } // typically topRight
static WebCore::IntPoint minXMaxYCorner(const WebCore::IntRect& rect) { return WebCore::IntPoint(rect.x(), rect.y() + rect.height() - 1); } // typically bottomLeft
static WebCore::IntPoint maxXMaxYCorner(const WebCore::IntRect& rect) { return WebCore::IntPoint(rect.x() + rect.width() - 1, rect.y() + rect.height() - 1); } // typically bottomRight

// The caret is a one-pixel wide line down either the right or left edge of a
// rect, depending on the text direction.
static inline bool caretIsOnLeft(bool isStartCaret, bool isRTL)
{
    if (isStartCaret)
        return !isRTL;

    return isRTL;
}

static inline WebCore::IntPoint caretLocationForRect(const WebCore::IntRect& rect, bool isStartCaret, bool isRTL)
{
    return caretIsOnLeft(isStartCaret, isRTL) ? minXMinYCorner(rect) : maxXMinYCorner(rect);
}

static inline WebCore::IntPoint caretComparisonPointForRect(const WebCore::IntRect& rect, bool isStartCaret, bool isRTL)
{
    if (isStartCaret)
        return caretIsOnLeft(isStartCaret, isRTL) ? minXMinYCorner(rect) : maxXMinYCorner(rect);

    return caretIsOnLeft(isStartCaret, isRTL) ? minXMaxYCorner(rect) : maxXMaxYCorner(rect);
}

static void adjustCaretRects(WebCore::IntRect& startCaret, bool isStartCaretClippedOut,
                             WebCore::IntRect& endCaret, bool isEndCaretClippedOut,
                             const std::vector<Platform::IntRect> rectList,
                             const WebCore::IntPoint& startReferencePoint,
                             const WebCore::IntPoint& endReferencePoint,
                             bool isRTL)
{
    // startReferencePoint is the best guess at the top left of the selection; endReferencePoint is the best guess at the bottom right.
    if (isStartCaretClippedOut)
        startCaret.setLocation(DOMSupport::InvalidPoint);
    else {
        startCaret = rectList[0];
        startCaret.setLocation(caretLocationForRect(startCaret, true, isRTL));
        // Reset width to 1 as we are strictly interested in caret location.
        startCaret.setWidth(1);
    }

    if (isEndCaretClippedOut)
        endCaret.setLocation(DOMSupport::InvalidPoint);
    else {
        endCaret = rectList[0];
        endCaret.setLocation(caretLocationForRect(endCaret, false, isRTL));
        // Reset width to 1 as we are strictly interested in caret location.
        endCaret.setWidth(1);
    }

    if (isStartCaretClippedOut && isEndCaretClippedOut)
        return;

    for (unsigned i = 1; i < rectList.size(); i++) {
        WebCore::IntRect currentRect(rectList[i]);

        // Compare and update the start and end carets with their respective reference points.
        if (!isStartCaretClippedOut && comparePointsToReferencePoint(
                    caretComparisonPointForRect(currentRect, true, isRTL),
                    caretComparisonPointForRect(startCaret, true, isRTL),
                    startReferencePoint, isRTL) > 0) {
            startCaret.setLocation(caretLocationForRect(currentRect, true, isRTL));
            startCaret.setHeight(currentRect.height());
        }

        if (!isEndCaretClippedOut && comparePointsToReferencePoint(
                    caretComparisonPointForRect(currentRect, false, isRTL),
                    caretComparisonPointForRect(endCaret, false, isRTL),
                    endReferencePoint, !isRTL) > 0) {
            endCaret.setLocation(caretLocationForRect(currentRect, false, isRTL));
            endCaret.setHeight(currentRect.height());
        }
    }
}

WebCore::IntPoint SelectionHandler::clipPointToVisibleContainer(const WebCore::IntPoint& point) const
{
    ASSERT(m_webPage->m_mainFrame && m_webPage->m_mainFrame->view());

    Frame* frame = m_webPage->focusedOrMainFrame();
    WebCore::IntPoint clippedPoint = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), frame, point, true /* clampToTargetFrame */);

    if (m_webPage->m_inputHandler->isInputMode()
            && frame->document()->focusedNode()
            && frame->document()->focusedNode()->renderer()) {
        WebCore::IntRect boundingBox(frame->document()->focusedNode()->renderer()->absoluteBoundingBoxRect());
        boundingBox.inflate(-1);
        clippedPoint = WebCore::IntPoint(clamp(boundingBox.x(), clippedPoint.x(), boundingBox.maxX()), clamp(boundingBox.y(), clippedPoint.y(), boundingBox.maxY()));
    }

    return clippedPoint;
}

static WebCore::IntPoint referencePoint(const VisiblePosition& position, const WebCore::IntRect& boundingRect, const WebCore::IntPoint& framePosition, bool isStartCaret, bool isRTL)
{
    // If one of the carets is invalid (this happens, for instance, if the
    // selection ends in an empty div) fall back to using the corner of the
    // entire region (which is already in frame coordinates so doesn't need
    // adjusting).
    WebCore::IntRect startCaretBounds(position.absoluteCaretBounds());
    startCaretBounds.move(framePosition.x(), framePosition.y());
    if (startCaretBounds.isEmpty() || !boundingRect.contains(startCaretBounds))
        startCaretBounds = boundingRect;

    return caretComparisonPointForRect(startCaretBounds, isStartCaret, isRTL);
}

// Check all rects in the region for a point match. The region is non-banded
// and non-sorted so all must be checked.
static bool regionRectListContainsPoint(const IntRectRegion& region, const WebCore::IntPoint& point)
{
    if (!region.extents().contains(point))
        return false;

    std::vector<Platform::IntRect> rectList = region.rects();
    for (unsigned int i = 0; i < rectList.size(); i++) {
        if (rectList[i].contains(point))
            return true;
    }
    return false;
}

bool SelectionHandler::inputNodeOverridesTouch() const
{
    if (!m_webPage->m_inputHandler->isInputMode())
        return false;

    Node* focusedNode = m_webPage->focusedOrMainFrame()->document()->focusedNode();
    if (!focusedNode || !focusedNode->isElementNode())
        return false;

    // TODO consider caching this in InputHandler so it is only calculated once per focus.
    DEFINE_STATIC_LOCAL(QualifiedName, selectionTouchOverrideAttr, (nullAtom, "data-blackberry-end-selection-on-touch", nullAtom));
    Element* element = static_cast<Element*>(focusedNode);
    return DOMSupport::elementAttributeState(element, selectionTouchOverrideAttr) == DOMSupport::On;
}

RequestedHandlePosition SelectionHandler::requestedSelectionHandlePosition(const VisibleSelection& selection) const
{
    Element* element = DOMSupport::selectionContainerElement(selection);
    return DOMSupport::elementHandlePositionAttribute(element);
}

// Note: This is the only function in SelectionHandler in which the coordinate
// system is not entirely WebKit.
void SelectionHandler::selectionPositionChanged(bool forceUpdateWithoutChange)
{
    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::selectionPositionChanged forceUpdateWithoutChange = %s",
        forceUpdateWithoutChange ? "true" : "false");

    // This method can get called during WebPage shutdown process.
    // If that is the case, just bail out since the client is not
    // in a safe state of trust to request anything else from it.
    if (!m_webPage->m_mainFrame)
        return;

    if (m_webPage->m_inputHandler->isInputMode() && m_webPage->m_inputHandler->processingChange()) {
        if (m_webPage->m_selectionOverlay)
            m_webPage->m_selectionOverlay->hide();
        m_webPage->m_client->cancelSelectionVisuals();

        // Since we're not calling notifyCaretPositionChangedIfNeeded now, we have to do so at the end of processing
        // to avoid dropping a notification.
        m_didSuppressCaretPositionChangedNotification = true;
        return;
    }

    notifyCaretPositionChangedIfNeeded();

    // Enter selection mode if selection type is RangeSelection, and disable selection if
    // selection is active and becomes caret selection.
    Frame* frame = m_webPage->focusedOrMainFrame();

    if (frame->view()->needsLayout())
        return;

    WebCore::IntPoint framePos = m_webPage->frameOffset(frame);
    if (m_selectionActive && (m_caretActive || frame->selection()->isNone()))
        m_selectionActive = false;
    else if (frame->selection()->isRange())
        m_selectionActive = true;
    else if (!m_selectionActive)
        return;

    SelectionTimingLog(Platform::LogLevelInfo,
        "SelectionHandler::selectionPositionChanged starting at %f",
        m_timer.elapsed());

    WebCore::IntRect startCaret(DOMSupport::InvalidPoint, WebCore::IntSize());
    WebCore::IntRect endCaret(DOMSupport::InvalidPoint, WebCore::IntSize());

    // Get the text rects from the selections range.
    Vector<FloatQuad> quads;
    DOMSupport::visibleTextQuads(frame->selection()->selection(), quads);

    IntRectRegion unclippedRegion;
    regionForTextQuads(quads, unclippedRegion, false /* shouldClipToVisibleContent */);

    // If there is no change in selected text and the visual rects
    // have not changed then don't bother notifying anything.
    if (!forceUpdateWithoutChange && m_lastSelectionRegion.isEqual(unclippedRegion))
        return;

    m_lastSelectionRegion = unclippedRegion;
    bool isRTL = directionOfEnclosingBlock(frame->selection()) == RTL;

    IntRectRegion visibleSelectionRegion;
    if (!unclippedRegion.isEmpty()) {
        WebCore::IntRect unclippedStartCaret;
        WebCore::IntRect unclippedEndCaret;

        WebCore::IntPoint startCaretReferencePoint = referencePoint(frame->selection()->selection().visibleStart(), unclippedRegion.extents(), framePos, true /* isStartCaret */, isRTL);
        WebCore::IntPoint endCaretReferencePoint = referencePoint(frame->selection()->selection().visibleEnd(), unclippedRegion.extents(), framePos, false /* isStartCaret */, isRTL);

        adjustCaretRects(unclippedStartCaret, false /* unclipped */, unclippedEndCaret, false /* unclipped */, unclippedRegion.rects(), startCaretReferencePoint, endCaretReferencePoint, isRTL);

        regionForTextQuads(quads, visibleSelectionRegion);

#if SHOWDEBUG_SELECTIONHANDLER // Don't rely just on SelectionLog to avoid loop.
        for (unsigned i = 0; i < unclippedRegion.numRects(); i++) {
            SelectionLog(Platform::LogLevelInfo,
                "Rect list - Unmodified #%d, %s",
                i,
                unclippedRegion.rects()[i].toString().c_str());
        }
        for (unsigned i = 0; i < visibleSelectionRegion.numRects(); i++) {
            SelectionLog(Platform::LogLevelInfo,
                "Rect list  - Clipped to Visible #%d, %s",
                i,
                visibleSelectionRegion.rects()[i].toString().c_str());
        }
#endif

        bool shouldCareAboutPossibleClippedOutSelection = frame != m_webPage->mainFrame() || m_webPage->m_inputHandler->isInputMode();

        if (!visibleSelectionRegion.isEmpty() || shouldCareAboutPossibleClippedOutSelection) {
            // Adjust the handle markers to be at the end of the painted rect. When selecting links
            // and other elements that may have a larger visible area than needs to be rendered a gap
            // can exist between the handle and overlay region.

            bool shouldClipStartCaret = !regionRectListContainsPoint(visibleSelectionRegion, unclippedStartCaret.location());
            bool shouldClipEndCaret = !regionRectListContainsPoint(visibleSelectionRegion, unclippedEndCaret.location());

            // Find the top corner and bottom corner.
            adjustCaretRects(startCaret, shouldClipStartCaret, endCaret, shouldClipEndCaret, visibleSelectionRegion.rects(), startCaretReferencePoint, endCaretReferencePoint, isRTL);
        }
    }

    if (!frame->selection()->selection().isBaseFirst() || isRTL ) {
        // End handle comes before start, invert the caret reference points.
        WebCore::IntRect tmpCaret(startCaret);
        startCaret = endCaret;
        endCaret = tmpCaret;
    }

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::selectionPositionChanged Start Rect=%s End Rect=%s",
        Platform::IntRect(startCaret).toString().c_str(),
        Platform::IntRect(endCaret).toString().c_str());

    if (m_webPage->m_selectionOverlay)
        m_webPage->m_selectionOverlay->draw(visibleSelectionRegion);


    VisibleSelection currentSelection = frame->selection()->selection();
    SelectionDetails details(startCaret, endCaret, visibleSelectionRegion, inputNodeOverridesTouch(),
        m_lastSelection != currentSelection, requestedSelectionHandlePosition(frame->selection()->selection()));

    m_webPage->m_client->notifySelectionDetailsChanged(details);
    m_lastSelection = currentSelection;
    SelectionTimingLog(Platform::LogLevelInfo,
        "SelectionHandler::selectionPositionChanged completed at %f",
        m_timer.elapsed());
}


void SelectionHandler::notifyCaretPositionChangedIfNeeded(bool userTouchTriggered)
{
    m_didSuppressCaretPositionChangedNotification = false;

    if (m_caretActive || (m_webPage->m_inputHandler->isInputMode() && m_webPage->focusedOrMainFrame()->selection()->isCaret())) {
        // This may update the caret to no longer be active.
        caretPositionChanged(userTouchTriggered);
    }
}

void SelectionHandler::caretPositionChanged(bool userTouchTriggered)
{
    SelectionLog(Platform::LogLevelInfo, "SelectionHandler::caretPositionChanged");

    bool isFatFingerOnTextField = userTouchTriggered && m_webPage->m_touchEventHandler->lastFatFingersResult().isTextInput();

    WebCore::IntRect caretLocation;
    // If the input field is not active, we must be turning off the caret.
    if (!m_webPage->m_inputHandler->isInputMode() && m_caretActive) {
        m_caretActive = false;
        // Send an empty caret change to turn off the caret.
        m_webPage->m_client->notifyCaretChanged(caretLocation, isFatFingerOnTextField);
        return;
    }

    ASSERT(m_webPage && m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());

    // This function should only reach this point if input mode is active.
    ASSERT(m_webPage->m_inputHandler->isInputMode());

    WebCore::IntPoint frameOffset(m_webPage->frameOffset(m_webPage->focusedOrMainFrame()));
    WebCore::IntRect clippingRectForContent(clippingRectForVisibleContent());
    if (m_webPage->focusedOrMainFrame()->selection()->selectionType() == VisibleSelection::CaretSelection) {
        caretLocation = m_webPage->focusedOrMainFrame()->selection()->selection().visibleStart().absoluteCaretBounds();
        caretLocation.move(frameOffset.x(), frameOffset.y());

        // Clip against the containing frame and node boundaries.
        caretLocation.intersect(clippingRectForContent);
    }

    m_caretActive = !caretLocation.isEmpty();

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::caretPositionChanged caret Rect %s",
        Platform::IntRect(caretLocation).toString().c_str());

    bool isSingleLineInput = !m_webPage->m_inputHandler->isMultilineInputMode();
    WebCore::IntRect nodeBoundingBox = isSingleLineInput ? m_webPage->m_inputHandler->boundingBoxForInputField() : WebCore::IntRect();

    if (!nodeBoundingBox.isEmpty()) {
        nodeBoundingBox.move(frameOffset.x(), frameOffset.y());

        // Clip against the containing frame and node boundaries.
        nodeBoundingBox.intersect(clippingRectForContent);
    }

    SelectionLog(Platform::LogLevelInfo,
        "SelectionHandler::caretPositionChanged: %s line input, single line bounding box %s%s",
        isSingleLineInput ? "single" : "multi",
        Platform::IntRect(nodeBoundingBox).toString().c_str(),
        m_webPage->m_inputHandler->elementText().isEmpty() ? ", empty text field" : "");

    m_webPage->m_client->notifyCaretChanged(caretLocation, isFatFingerOnTextField, isSingleLineInput, nodeBoundingBox, m_webPage->m_inputHandler->elementText().isEmpty());
}

bool SelectionHandler::selectionContains(const WebCore::IntPoint& point)
{
    ASSERT(m_webPage && m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());
    return m_webPage->focusedOrMainFrame()->selection()->contains(point);
}

}
}
