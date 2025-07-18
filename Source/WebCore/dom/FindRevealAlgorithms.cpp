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
#include "FindRevealAlgorithms.h"

#include "Document.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLDetailsElement.h"
#include "HTMLSlotElement.h"
#include "NodeRenderStyle.h"
#include "Settings.h"
#include "UserAgentParts.h"

namespace WebCore {

// https://html.spec.whatwg.org/#ancestor-details-revealing-algorithm
void revealClosedDetailsAncestors(Node& node)
{
    if (!node.document().settings().detailsAutoExpandEnabled())
        return;

    Ref currentNode = node;
    while (RefPtr parent = currentNode->parentInComposedTree()) {
        if (RefPtr slot = currentNode->assignedSlot(); slot && slot->userAgentPart() == UserAgentParts::detailsContent() && slot->shadowHost()) {
            currentNode = *slot->shadowHost();
            Ref details = downcast<HTMLDetailsElement>(currentNode);
            if (!details->hasAttributeWithoutSynchronization(HTMLNames::openAttr))
                details->toggleOpen();
        } else
            currentNode = *parent;
    }
}

// https://html.spec.whatwg.org/#ancestor-hidden-until-found-revealing-algorithm
void revealHiddenUntilFoundAncestors(Node& node)
{
    if (!node.document().settings().hiddenUntilFoundEnabled())
        return;

    for (RefPtr currentNode = &node; currentNode; currentNode = currentNode->parentElementInComposedTree()) {
        RefPtr element = dynamicDowncast<HTMLElement>(currentNode);
        if (element && element->isHiddenUntilFound()) {
            element->dispatchEvent(Event::create(eventNames().beforematchEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
            element->setHidden({ });
        }
    }
}

void revealClosedDetailsAndHiddenUntilFoundAncestors(Node& node)
{
    node.protectedDocument()->updateStyleIfNeeded();

    // Bail out if there is neither a hidden=until-found or details ancestor.
    if (node.renderStyle() && !node.renderStyle()->autoRevealsWhenFound())
        return;

    revealClosedDetailsAncestors(node);
    revealHiddenUntilFoundAncestors(node);
}

} // namespace WebCore
