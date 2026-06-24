/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXStitchUtilities.h"

#include "AXUtilities.h"
#include "AccessibilityNodeObject.h"
#include "ContainerNodeInlines.h"
#include "Element.h"
#include "EventNames.h"
#include "HTMLLabelElement.h"
#include "HTMLTableCellElement.h"
#include "RenderElementInlines.h"
#include "RenderLineBreak.h"
#include "RenderObjectStyle.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/Scope.h>

namespace WebCore {

StitchingContext::StitchingContext(const AccessibilityNodeObject& containingBlockFlowObject)
    : containingBlockFlowObject(containingBlockFlowObject)
{ }

static bool NODELETE hasEnclosingInputElement(Node* node)
{
    return node && is<HTMLInputElement>(node->shadowHost());
}

static bool hasStitchBreakingRole(Element& element)
{
    return hasAnyRole(element, {
        // Cell roles
        "gridcell"_s, "cell"_s, "columnheader"_s, "rowheader"_s,
        // Miscellaneous roles
        "suggestion"_s, "insertion"_s, "deletion"_s
    });
}

static bool NODELETE hasStitchBreakingTag(Element& element)
{
    switch (element.elementName()) {
    case ElementName::HTML_ins:
    case ElementName::HTML_del:
        return true;
    default:
        return false;
    }
}

static bool isClickTarget(Element& element)
{
    auto elementName = element.elementName();
    if (elementName == ElementName::HTML_body || elementName == ElementName::HTML_main) {
        // Mirror the criteria in AXCoreObject::supportsPressAction, which does not
        // currently expose clickability for main / body to avoid considering everything
        // clickable in event-delegate patterns.
        return false;
    }

    if (element.hasAnyEventListeners(std::array { eventNames().clickEvent, eventNames().mousedownEvent, eventNames().mouseupEvent }))
        return true;

    // cursor:pointer is a good signal that this element is a click target.
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    CheckedRef style = renderer->style();
    if (style->cursorType() != CursorType::Pointer || style->pointerEvents() == PointerEvents::None)
        return false;

    // CSS `cursor` inherits, so an interactive ancestor's pointer cursor appears on every
    // descendant's computed style. Treat the element as a boundary only when it *introduces*
    // the pointer cursor. Otherwise every descendant of an interactive ancestor would falsely
    // register as its own boundary, fragmenting text that should stitch as one unit.
    if (CheckedPtr parentRenderer = renderer->parent()) {
        if (parentRenderer->style().cursorType() == CursorType::Pointer)
            return false;
    }
    return true;
}

static bool isStitchBreakingElement(Element& element)
{
    return is<HTMLTableCellElement>(element)
    || is<HTMLLabelElement>(element)
    || element.isLink()
    || hasStitchBreakingRole(element)
    || hasStitchBreakingTag(element)
    || isClickTarget(element);
}

StitchAction stitchActionFor(const RenderObject& renderer, const AccessibilityObject& object, StitchingContext& context)
{
    if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(renderer); lineBreak && lineBreak->isBR()) {
        // A <br> visually separates the text around it, so don't stitch across it.
        return StitchAction::BreakAndSkip;
    }

    auto isInsideLink = [] (const RenderObject& renderer) {
        return renderer.style().insideLink() != InsideLink::NotInside;
    };

    if (context.lastRenderer && isInsideLink(renderer) && !isInsideLink(*context.lastRenderer)) {
        // Stop the current stitch when entering a link.
        return StitchAction::BreakAndSkip;
    }

    if (renderer.parent() && (renderer.parent()->isBeforeOrAfterContent() || renderer.parent()->isFirstLetter())) {
        // Stitching generated content will cause incorrect behavior because
        // some of our code that handles stitched text (e.g. stringValue) assumes
        // the presence of a Node. For now, stop stitching at generated content.
        // Ideally we remove this restriction in the future.
        return StitchAction::BreakAndSkip;
    }

    if (hasEnclosingInputElement(renderer.node())) {
        // Don't stitch within text inputs. One example of why we want to avoid
        // this is otherwise the number values of the chosen dates will get stitched
        // with the "/"s that surround them, which is a poor user experience.
        return StitchAction::BreakAndSkip;
    }

    RefPtr node = renderer.node();
    if (!node) {
        // |renderer| may be generated content. Let's get the element that generated it.
        if (renderer.parent())
            node = renderer.parent()->generatingElement();
    }

    RefPtr<ContainerNode> stitchBreakingAncestor = nullptr;
    while (node) {
        RefPtr ancestor = composedParentIgnoringDocumentFragments(*node);
        if (!ancestor)
            break;

        if (RefPtr ancestorElement = dynamicDowncast<Element>(*ancestor)) {
            if (isStitchBreakingElement(*ancestorElement)) {
                stitchBreakingAncestor = WTF::move(ancestor);
                break;
            }
        }
        node = WTF::move(ancestor);
    }

    RefPtr currentAncestor = object.parentObject();
    while (currentAncestor) {
        if (currentAncestor->owners().size()) {
            stitchBreakingAncestor = nullptr;
            return StitchAction::BreakAndSkip;
        }

        if (currentAncestor == context.containingBlockFlowObject) {
            // There are no re-ownerships on the way to our block flow, so we can stop.
            break;
        }
        currentAncestor = currentAncestor->parentObject();
    }

    auto updateContext = makeScopeExit([&] {
        if (node) {
            // If we couldn't find a node for this renderer, we can't definitively
            // say whether we had a stitch-breaking ancestor, so don't update the context.
            context.lastStitchBreakingAncestor = stitchBreakingAncestor;
        }
    });

    if (node && context.lastStitchBreakingAncestor != stitchBreakingAncestor) {
        // Breaking stitching across semantic boundaries, like cells, controls, click handlers, etc.
        // The current text is on the other side of the boundary and can start a new group.
        return StitchAction::BreakAndAdd;
    }

    return StitchAction::Continue;
}

} // namespace WebCore
