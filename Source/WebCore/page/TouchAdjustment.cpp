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
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "Node.h"
#include "NodeRenderStyle.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include <math.h>

namespace WebCore {

namespace TouchAdjustment {

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
typedef float (*DistanceFunction)(const IntPoint&, const IntRect&, const SubtargetGeometry&);

// Takes non-const Node* because isContentEditable is a non-const function.
bool nodeRespondsToTapGesture(Node* node)
{
    if (node->isLink()
        || node->isContentEditable()
        || node->isMouseFocusable())
        return true;
    if (node->isElementNode()) {
        Element* element =  static_cast<Element*>(node);
        if (element->hasTagName(HTMLNames::labelTag) && static_cast<HTMLLabelElement*>(element)->control())
            return true;
    }
    // FIXME: Implement hasDefaultEventHandler and use that instead of all of the above checks.
    if (node->hasEventListeners()
        && (node->hasEventListeners(eventNames().clickEvent)
            || node->hasEventListeners(eventNames().DOMActivateEvent)
            || node->hasEventListeners(eventNames().mousedownEvent)
            || node->hasEventListeners(eventNames().mouseupEvent)
            || node->hasEventListeners(eventNames().mousemoveEvent)
            // Checking for focus events is not necessary since they can only fire on
            // focusable elements which have already been captured above.
        ))
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

static inline void appendSubtargetsForNodeToList(Node* node, SubtargetGeometryList& subtargets)
{
    // Since the node is a result of a hit test, we are already ensured it has a renderer.
    ASSERT(node->renderer());

    Vector<FloatQuad> quads;
    node->renderer()->absoluteQuads(quads);

    Vector<FloatQuad>::const_iterator it = quads.begin();
    const Vector<FloatQuad>::const_iterator end = quads.end();
    for (; it != end; ++it)
        subtargets.append(SubtargetGeometry(node, *it));
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
void compileSubtargetList(const NodeList& intersectedNodes, SubtargetGeometryList& subtargets, NodeFilter nodeFilter)
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
        appendSubtargetsForNodeToList(candidate, subtargets);
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
        return INFINITY;
    IntRect intersection = rect;
    intersection.intersect(touchArea);

    // Return the quotient of the intersection.
    return rect.size().area() / (float)intersection.size().area();
}


// A generic function for finding the target node with the lowest distance metric. A distance metric here is the result
// of a distance-like function, that computes how well the touch hits the node.
// Distance functions could for instance be distance squared or area of intersection.
bool findNodeWithLowestDistanceMetric(Node*& targetNode, IntPoint& targetPoint, IntRect& targetArea, const IntPoint& touchHotspot, const IntRect& touchArea, SubtargetGeometryList& subtargets, DistanceFunction distanceFunction)
{
    targetNode = 0;
    float bestDistanceMetric = INFINITY;
    SubtargetGeometryList::const_iterator it = subtargets.begin();
    const SubtargetGeometryList::const_iterator end = subtargets.end();
    for (; it != end; ++it) {
        Node* node = it->node();
        float distanceMetric = distanceFunction(touchHotspot, touchArea, *it);
        if (distanceMetric < bestDistanceMetric) {
            targetPoint = roundedIntPoint(it->quad().center());
            targetArea = it->boundingBox();
            targetNode = node;
            bestDistanceMetric = distanceMetric;
        } else if (distanceMetric == bestDistanceMetric) {
            // Try to always return the inner-most element.
            if (node->isDescendantOf(targetNode)) {
                targetNode = node;
                targetArea = it->boundingBox();
            }
        }
    }
    if (targetNode) {
        targetArea = targetNode->document()->view()->contentsToWindow(targetArea);
        targetPoint = targetNode->document()->view()->contentsToWindow(targetPoint);
    }

    return (targetNode);
}

} // namespace TouchAdjustment

bool findBestClickableCandidate(Node*& targetNode, IntPoint &targetPoint, const IntPoint &touchHotspot, const IntRect &touchArea, const NodeList& nodeList)
{
    IntRect targetArea;
    TouchAdjustment::SubtargetGeometryList subtargets;
    TouchAdjustment::compileSubtargetList(nodeList, subtargets, TouchAdjustment::nodeRespondsToTapGesture);
    return TouchAdjustment::findNodeWithLowestDistanceMetric(targetNode, targetPoint, targetArea, touchHotspot, touchArea, subtargets, TouchAdjustment::distanceSquaredToTargetCenterLine);
}

bool findBestZoomableArea(Node*& targetNode, IntRect& targetArea, const IntPoint& touchHotspot, const IntRect& touchArea, const NodeList& nodeList)
{
    IntPoint targetPoint;
    TouchAdjustment::SubtargetGeometryList subtargets;
    TouchAdjustment::compileZoomableSubtargets(nodeList, subtargets);
    return TouchAdjustment::findNodeWithLowestDistanceMetric(targetNode, targetPoint, targetArea, touchHotspot, touchArea, subtargets, TouchAdjustment::zoomableIntersectionQuotient);
}

} // namespace WebCore
