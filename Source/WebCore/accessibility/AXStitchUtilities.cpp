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
#include "Element.h"
#include "HTMLLabelElement.h"
#include "HTMLTableCellElement.h"
#include "RenderElementInlines.h"
#include "RenderStyle.h"

namespace WebCore {

StitchingContext::StitchingContext(const AccessibilityNodeObject& containingBlockFlowObject)
    : containingBlockFlowObject(containingBlockFlowObject)
{ }

static bool hasEnclosingInputElement(Node* node)
{
    return node && is<HTMLInputElement>(node->shadowHost());
}

static bool hasStitchBreakingRole(Element& element)
{
    return hasAnyRole(element, {
        // Cell roles
        "gridcell"_s, "cell"_s, "columnheader"_s, "rowheader"_s
        // Miscellaneous roles
        "suggestion"_s, "insertion"_s, "deletion"_s
    });
}

static bool hasStitchBreakingTag(Element& element)
{
    switch (element.elementName()) {
    case ElementName::HTML_ins:
    case ElementName::HTML_del:
        return true;
    default:
        return false;
    }
}

static bool isStitchBreakingElement(Element& element)
{
    return is<HTMLTableCellElement>(element)
    || is<HTMLLabelElement>(element)
    || hasStitchBreakingRole(element)
    || hasStitchBreakingTag(element);
}

bool shouldStopStitchingAt(const RenderObject& renderer, const AccessibilityObject& object, StitchingContext& context)
{
    if (renderer.style().insideLink() != InsideLink::NotInside) {
        // Stop stitching when encountering a link.
        return true;
    }

    if (renderer.parent() && (renderer.parent()->isBeforeOrAfterContent() || renderer.parent()->isFirstLetter())) {
        // Stitching generated content will cause incorrect behavior because
        // some of our code that handles stitched text (e.g. stringValue) assumes
        // the presence of a Node. For now, stop stitching at generated content.
        // Ideally we remove this restriction in the future.
        return true;
    }

    if (hasEnclosingInputElement(RefPtr { renderer.node() }.get())) {
        // Don't stitch within text inputs. One example of why we want to avoid
        // this is otherwise the number values of the chosen dates will get stitched
        // with the "/"s that surround them, which is a poor user experience.
        return true;
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
                stitchBreakingAncestor = WTFMove(ancestor);
                break;
            }
        }
        node = WTFMove(ancestor);
    }

    RefPtr currentAncestor = object.parentObject();
    while (currentAncestor) {
        if (currentAncestor->owners().size()) {
            stitchBreakingAncestor = nullptr;
            return true;
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

    if (node && context.lastStitchBreakingAncestor && context.lastStitchBreakingAncestor != stitchBreakingAncestor) {
        // Breaking stitching across semantic boundaries, like cells, controls, etc.
        return true;
    }

    return false;
}

} // namespace WebCore
