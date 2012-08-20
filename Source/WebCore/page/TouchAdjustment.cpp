/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "TouchAdjustment.h"

#include "ContainerNode.h"
#include "FloatPoint.h"
#include "FloatQuad.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "Node.h"
#include "NodeRenderStyle.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextBreakIterator.h"

namespace WebCore {

namespace TouchAdjustment {

const float zeroTolerance = 1e-6f;

// Class for remembering absolute quads of a target node and what node they represent.
class SubtargetGeometry {
public:
    SubtargetGeometry(Node* node, const FloatQuad& quad)
        : m_node(node)
        , m_quad(quad)
    { }

    Node* node() const { return m_node; }
    FloatQuad quad() const { return m_quad; }
    IntRect boundingBox() const { return m_quad.enclosingBoundingBox(); }

private:
    Node* m_node;
    FloatQuad m_quad;
};

typedef Vector<SubtargetGeometry> SubtargetGeometryList;
typedef bool (*NodeFilter)(Node*);
typedef void (*AppendSubtargetsForNode)(Node*, SubtargetGeometryList&);
typedef float (*DistanceFunction)(const IntPoint&, const IntRect&, const SubtargetGeometry&);

// Takes non-const Node* because isContentEditable is a non-const function.
bool nodeRespondsToTapGesture(Node* node)
{
    if (node->isMouseFocusable())
        return true;
    if (node->willRespondToMouseClickEvents() || node->willRespondToMouseMoveEvents())
        return true;
    if (node->renderStyle()) {
        // Accept nodes that has a CSS effect when touched.
        if (node->renderStyle()->affectedByActiveRules() || node->renderStyle()->affectedByHoverRules())
            return true;
    }
    return false;
}

bool nodeIsZoomTarget(Node* node)
{
    if (node->isTextNode() || node->isShadowRoot())
        return false;

    ASSERT(node->renderer());
    return node->renderer()->isBox();
}

bool providesContextMenuItems(Node* node)
{
    // This function tries to match the nodes that receive special context-menu items in
    // ContextMenuController::populate(), and should be kept uptodate with those.
    if (node->isContentEditable())
        return true;
    if (node->isLink())
        return true;
    if (node->renderer()->isImage())
        return true;
    if (node->renderer()->isMedia())
        return true;
    if (node->renderer()->canBeSelectionLeaf()) {
        // If the context menu gesture will trigger a selection all selectable nodes are valid targets.
        if (node->renderer()->frame()->editor()->behavior().shouldSelectOnContextualMenuClick())
            return true;
        // Only the selected part of the renderer is a valid target, but this will be corrected in
        // appendContextSubtargetsForNode.
        if (node->renderer()->selectionState() != RenderObject::SelectionNone)
            return true;
    }
    return false;
}

static inline void appendQuadsToSubtargetList(Vector<FloatQuad>& quads, Node* node, SubtargetGeometryList& subtargets)
{
    Vector<FloatQuad>::const_iterator it = quads.begin();
    const Vector<FloatQuad>::const_iterator end = quads.end();
    for (; it != end; ++it)
        subtargets.append(SubtargetGeometry(node, *it));
}

static inline void appendBasicSubtargetsForNode(Node* node, SubtargetGeometryList& subtargets)
{
    // Since the node is a result of a hit test, we are already ensured it has a renderer.
    ASSERT(node->renderer());

    Vector<FloatQuad> quads;
    node->renderer()->absoluteQuads(quads);

    appendQuadsToSubtargetList(quads, node, subtargets);
}

static inline void appendContextSubtargetsForNode(Node* node, SubtargetGeometryList& subtargets)
{
    // This is a variant of appendBasicSubtargetsForNode that adds special subtargets for
    // selected or auto-selectable parts of text nodes.
    ASSERT(node->renderer());

    if (!node->isTextNode())
        return appendBasicSubtargetsForNode(node, subtargets);

    Text* textNode = static_cast<WebCore::Text*>(node);
    RenderText* textRenderer = static_cast<RenderText*>(textNode->renderer());

    if (textRenderer->frame()->editor()->behavior().shouldSelectOnContextualMenuClick()) {
        // Make subtargets out of every word.
        String textValue = textNode->data();
        TextBreakIterator* wordIterator = wordBreakIterator(textValue.characters(), textValue.length());
        int lastOffset = textBreakFirst(wordIterator);
        if (lastOffset == -1)
            return;
        int offset;
        while ((offset = textBreakNext(wordIterator)) != -1) {
            if (isWordTextBreak(wordIterator)) {
                Vector<FloatQuad> quads;
                textRenderer->absoluteQuadsForRange(quads, lastOffset, offset);
                appendQuadsToSubtargetList(quads, textNode, subtargets);
            }
            lastOffset = offset;
        }
    } else {
        // Make subtargets out of only the selected part of the text.
        ASSERT(textRenderer->selectionState() != RenderObject::SelectionNone);
        int startPos, endPos;
        switch (textRenderer->selectionState()) {
        case RenderObject::SelectionInside:
            startPos = 0;
            endPos = textRenderer->textLength();
            break;
        case RenderObject::SelectionStart:
            textRenderer->selectionStartEnd(startPos, endPos);
            endPos = textRenderer->textLength();
            break;
        case RenderObject::SelectionEnd:
            textRenderer->selectionStartEnd(startPos, endPos);
            startPos = 0;
            break;
        case RenderObject::SelectionBoth:
            textRenderer->selectionStartEnd(startPos, endPos);
            break;
        default:
            appendBasicSubtargetsForNode(node, subtargets);
            return;
        }
        Vector<FloatQuad> quads;
        textRenderer->absoluteQuadsForRange(quads, startPos, endPos);
        appendQuadsToSubtargetList(quads, textNode, subtargets);
    }
}

static inline void appendZoomableSubtargets(Node* node, SubtargetGeometryList& subtargets)
{
    RenderBox* renderer = toRenderBox(node->renderer());
    ASSERT(renderer);

    Vector<FloatQuad> quads;
    FloatRect borderBoxRect = renderer->borderBoxRect();
    FloatRect contentBoxRect = renderer->contentBoxRect();
    quads.append(renderer->localToAbsoluteQuad(borderBoxRect));
    if (borderBoxRect != contentBoxRect)
        quads.append(renderer->localToAbsoluteQuad(contentBoxRect));
    // FIXME: For RenderBlocks, add column boxes and content boxes cleared for floats.

    Vector<FloatQuad>::const_iterator it = quads.begin();
    const Vector<FloatQuad>::const_iterator end = quads.end();
    for (; it != end; ++it)
        subtargets.append(SubtargetGeometry(node, *it));
}

// Compiles a list of subtargets of all the relevant target nodes.
void compileSubtargetList(const NodeList& intersectedNodes, SubtargetGeometryList& subtargets, NodeFilter nodeFilter, AppendSubtargetsForNode appendSubtargetsForNode)
{
    // Find candidates responding to tap gesture events in O(n) time.
    HashMap<Node*, Node*> responderMap;
    HashSet<Node*> ancestorsToRespondersSet;
    Vector<Node*> candidates;

    // A node matching the NodeFilter is called a responder. Candidate nodes must either be a
    // responder or have an ancestor that is a responder.
    // This iteration tests all ancestors at most once by caching earlier results.
    unsigned length = intersectedNodes.length();
    for (unsigned i = 0; i < length; ++i) {
        Node* const node = intersectedNodes.item(i);
        Vector<Node*> visitedNodes;
        Node* respondingNode = 0;
        for (Node* visitedNode = node; visitedNode; visitedNode = visitedNode->parentOrHostNode()) {
            // Check if we already have a result for a common ancestor from another candidate.
            respondingNode = responderMap.get(visitedNode);
            if (respondingNode)
                break;
            visitedNodes.append(visitedNode);
            // Check if the node filter applies, which would mean we have found a responding node.
            if (nodeFilter(visitedNode)) {
                respondingNode = visitedNode;
                // Continue the iteration to collect the ancestors of the responder, which we will need later.
                for (visitedNode = visitedNode->parentOrHostNode(); visitedNode; visitedNode = visitedNode->parentOrHostNode()) {
                    HashSet<Node*>::AddResult addResult = ancestorsToRespondersSet.add(visitedNode);
                    if (!addResult.isNewEntry)
                        break;
                }
                break;
            }
        }
        // Insert the detected responder for all the visited nodes.
        for (unsigned j = 0; j < visitedNodes.size(); j++)
            responderMap.add(visitedNodes[j], respondingNode);

        if (respondingNode)
            candidates.append(node);
    }

    // We compile the list of component absolute quads instead of using the bounding rect
    // to be able to perform better hit-testing on inline links on line-breaks.
    length = candidates.size();
    for (unsigned i = 0; i < length; i++) {
        Node* candidate = candidates[i];
        // Skip nodes who's responders are ancestors of other responders. This gives preference to
        // the inner-most event-handlers. So that a link is always preferred even when contained
        // in an element that monitors all click-events.
        Node* respondingNode = responderMap.get(candidate);
        ASSERT(respondingNode);
        if (ancestorsToRespondersSet.contains(respondingNode))
            continue;
        appendSubtargetsForNode(candidate, subtargets);
    }
}

// Compiles a list of zoomable subtargets.
void compileZoomableSubtargets(const NodeList& intersectedNodes, SubtargetGeometryList& subtargets)
{
    unsigned length = intersectedNodes.length();
    for (unsigned i = 0; i < length; ++i) {
        Node* const candidate = intersectedNodes.item(i);
        if (nodeIsZoomTarget(candidate))
            appendZoomableSubtargets(candidate, subtargets);
    }
}


float distanceSquaredToTargetCenterLine(const IntPoint& touchHotspot, const IntRect& touchArea, const SubtargetGeometry& subtarget)
{
    UNUSED_PARAM(touchArea);
    // For a better center of a line-box we use the center-line instead of the center-point.
    // We use the center-line of the bounding box of the quad though, since it is much faster
    // and gives the same result in all untransformed cases, and in transformed cases still
    // gives a better distance-function than the distance to the center-point.
    IntRect rect = subtarget.boundingBox();
    ASSERT(subtarget.node()->document());
    ASSERT(subtarget.node()->document()->view());
    // Convert from frame coordinates to window coordinates.
    rect = subtarget.node()->document()->view()->contentsToWindow(rect);

    return rect.distanceSquaredFromCenterLineToPoint(touchHotspot);
}

// This returns quotient of the target area and its intersection with the touch area.
// This will prioritize largest intersection and smallest area, while balancing the two against each other.
float zoomableIntersectionQuotient(const IntPoint& touchHotspot, const IntRect& touchArea, const SubtargetGeometry& subtarget)
{
    IntRect rect = subtarget.boundingBox();

    // Convert from frame coordinates to window coordinates.
    rect = subtarget.node()->document()->view()->contentsToWindow(rect);

    // Check the rectangle is meaningful zoom target. It should at least contain the hotspot.
    if (!rect.contains(touchHotspot))
        return std::numeric_limits<float>::infinity();
    IntRect intersection = rect;
    intersection.intersect(touchArea);

    // Return the quotient of the intersection.
    return rect.size().area() / (float)intersection.size().area();
}

// Uses a hybrid of distance to center and intersect ratio, normalizing each
// score between 0 and 1 and choosing the better score. The distance to
// centerline works best for disambiguating clicks on targets such as links,
// where the width may be significantly larger than the touch width. Using
// area of overlap in such cases can lead to a bias towards shorter links.
// Conversely, percentage of overlap can provide strong confidence in tapping
// on a small target, where the overlap is often quite high, and works well
// for tightly packed controls.
float hybridDistanceFunction(const IntPoint& touchHotspot, const IntRect& touchArea, const SubtargetGeometry& subtarget)
{
    IntRect rect = subtarget.boundingBox();

    // Convert from frame coordinates to window coordinates.
    rect = subtarget.node()->document()->view()->contentsToWindow(rect);
   
    float touchWidth = touchArea.width();
    float touchHeight = touchArea.height();
    float distanceScale =  touchWidth * touchWidth + touchHeight * touchHeight;
    float distanceToCenterScore = rect.distanceSquaredFromCenterLineToPoint(touchHotspot) / distanceScale;

    float targetArea = rect.size().area();
    rect.intersect(touchArea);
    float intersectArea = rect.size().area();
    float intersectionScore = 1 - intersectArea / targetArea;

    return intersectionScore < distanceToCenterScore ? intersectionScore : distanceToCenterScore;
}

FloatPoint contentsToWindow(FrameView *view, FloatPoint pt)
{
    int x = static_cast<int>(pt.x() + 0.5f);
    int y = static_cast<int>(pt.y() + 0.5f);
    IntPoint adjusted = view->contentsToWindow(IntPoint(x, y));
    return FloatPoint(adjusted.x(), adjusted.y());
}

bool snapTo(const SubtargetGeometry& geom, const IntPoint& touchPoint, const IntRect& touchArea, IntPoint& adjustedPoint)
{
    FrameView* view = geom.node()->document()->view();
    FloatQuad quad = geom.quad();

    if (quad.isRectilinear()) {
        IntRect contentBounds = geom.boundingBox();
        // Convert from frame coordinates to window coordinates.
        IntRect bounds = view->contentsToWindow(contentBounds);
        if (bounds.contains(touchPoint)) {
            adjustedPoint = touchPoint;
            return true;
        }
        if (bounds.intersects(touchArea)) {
            bounds.intersect(touchArea);
            adjustedPoint = bounds.center();
            return true;
        }
        return false;
    }

    // Non-rectilinear element.
    // Convert quad from content to window coordinates.
    FloatPoint p1 = contentsToWindow(view, quad.p1());
    FloatPoint p2 = contentsToWindow(view, quad.p2());
    FloatPoint p3 = contentsToWindow(view, quad.p3());
    FloatPoint p4 = contentsToWindow(view, quad.p4());
    quad = FloatQuad(p1, p2, p3, p4);

    if (quad.containsPoint(touchPoint)) {
        adjustedPoint = touchPoint;
        return true;
    }

    // Pull point towards the center of the element.
    float cx = 0.25 * (p1.x() + p2.x() + p3.x() + p4.x());
    float cy = 0.25 * (p1.y() + p2.y() + p3.y() + p4.y());
    FloatPoint center = FloatPoint(cx, cy);

    FloatSize pullDirection = center - touchPoint;
    float distanceToCenter = pullDirection.diagonalLength();

    // Use distance from center to corner of touch area to limit adjustment distance.
    float dx = 0.5f * touchArea.width();
    float dy = 0.5f * touchArea.height();
    float touchRadius = sqrt(dx * dx + dy * dy);

    float scaleFactor = touchRadius / distanceToCenter;
    if (scaleFactor > 1)
        scaleFactor = 1;
    pullDirection.scale(scaleFactor);

    int x = static_cast<int>(touchPoint.x() + pullDirection.width());
    int y = static_cast<int>(touchPoint.y() + pullDirection.height());
    IntPoint point(x, y);

    if (quad.containsPoint(point)) {
        adjustedPoint = point;
        return true;
    }
    return false;
}

// A generic function for finding the target node with the lowest distance metric. A distance metric here is the result
// of a distance-like function, that computes how well the touch hits the node.
// Distance functions could for instance be distance squared or area of intersection.
bool findNodeWithLowestDistanceMetric(Node*& targetNode, IntPoint& targetPoint, IntRect& targetArea, const IntPoint& touchHotspot, const IntRect& touchArea, SubtargetGeometryList& subtargets, DistanceFunction distanceFunction)
{
    targetNode = 0;
    float bestDistanceMetric = std::numeric_limits<float>::infinity();
    SubtargetGeometryList::const_iterator it = subtargets.begin();
    const SubtargetGeometryList::const_iterator end = subtargets.end();
    IntPoint adjustedPoint;

    for (; it != end; ++it) {
        Node* node = it->node();
        float distanceMetric = distanceFunction(touchHotspot, touchArea, *it);
        if (distanceMetric < bestDistanceMetric) {
            if (snapTo(*it, touchHotspot, touchArea, adjustedPoint)) {
                targetPoint = adjustedPoint;
                targetArea = it->boundingBox();
                targetNode = node;
                bestDistanceMetric = distanceMetric;
            }
        } else if (distanceMetric - bestDistanceMetric < zeroTolerance) {
            if (snapTo(*it, touchHotspot, touchArea, adjustedPoint)) {
                if (node->isDescendantOf(targetNode)) {
                    // Try to always return the inner-most element.
                    targetPoint = adjustedPoint;
                    targetNode = node;
                    targetArea = it->boundingBox();
                } else {
                    // Minimize adjustment distance.
                    float dx = targetPoint.x() - touchHotspot.x();
                    float dy = targetPoint.y() - touchHotspot.y();
                    float bestDistance = dx * dx + dy * dy;
                    dx = adjustedPoint.x() - touchHotspot.x();
                    dy = adjustedPoint.y() - touchHotspot.y();
                    float distance = dx * dx + dy * dy;
                    if (distance < bestDistance) {
                        targetPoint = adjustedPoint;
                        targetNode = node;
                        targetArea = it->boundingBox();
                    }
                }
            }
        }
    }
    if (targetNode) {
        targetArea = targetNode->document()->view()->contentsToWindow(targetArea);
    }
    return (targetNode);
}

} // namespace TouchAdjustment

bool findBestClickableCandidate(Node*& targetNode, IntPoint &targetPoint, const IntPoint &touchHotspot, const IntRect &touchArea, const NodeList& nodeList)
{
    IntRect targetArea;
    TouchAdjustment::SubtargetGeometryList subtargets;
    TouchAdjustment::compileSubtargetList(nodeList, subtargets, TouchAdjustment::nodeRespondsToTapGesture, TouchAdjustment::appendBasicSubtargetsForNode);
    return TouchAdjustment::findNodeWithLowestDistanceMetric(targetNode, targetPoint, targetArea, touchHotspot, touchArea, subtargets, TouchAdjustment::hybridDistanceFunction);
}

bool findBestContextMenuCandidate(Node*& targetNode, IntPoint &targetPoint, const IntPoint &touchHotspot, const IntRect &touchArea, const NodeList& nodeList)
{
    IntRect targetArea;
    TouchAdjustment::SubtargetGeometryList subtargets;
    TouchAdjustment::compileSubtargetList(nodeList, subtargets, TouchAdjustment::providesContextMenuItems, TouchAdjustment::appendContextSubtargetsForNode);
    return TouchAdjustment::findNodeWithLowestDistanceMetric(targetNode, targetPoint, targetArea, touchHotspot, touchArea, subtargets, TouchAdjustment::hybridDistanceFunction);
}

bool findBestZoomableArea(Node*& targetNode, IntRect& targetArea, const IntPoint& touchHotspot, const IntRect& touchArea, const NodeList& nodeList)
{
    IntPoint targetPoint;
    TouchAdjustment::SubtargetGeometryList subtargets;
    TouchAdjustment::compileZoomableSubtargets(nodeList, subtargets);
    return TouchAdjustment::findNodeWithLowestDistanceMetric(targetNode, targetPoint, targetArea, touchHotspot, touchArea, subtargets, TouchAdjustment::zoomableIntersectionQuotient);
}

} // namespace WebCore
