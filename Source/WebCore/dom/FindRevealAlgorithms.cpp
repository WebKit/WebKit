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
#include "Event.h"
#include "EventNames.h"
#include "HTMLDetailsElement.h"
#include "HTMLSlotElement.h"
#include "NodeRenderStyle.h"
#include "RenderStyle+GettersInlines.h"
#include "Settings.h"
#include "UserAgentParts.h"

namespace WebCore {

enum class RevealType : bool {
    ClosedDetails,
    HiddenUntilFound
};

// https://html.spec.whatwg.org/#ancestor-revealing-algorithm
bool revealClosedDetailsAndHiddenUntilFoundAncestors(Node& node)
{
    protect(node.document())->updateStyleIfNeeded();

    // Bail out if there is neither a hidden=until-found or details ancestor.
    if (node.renderStyle() && !node.renderStyle()->autoRevealsWhenFound())
        return false;

    auto closedDetailsElementAncestor = [](Node& node) -> RefPtr<HTMLDetailsElement> {
        RefPtr slot = node.assignedSlot();
        if (slot && slot->userAgentPart() == UserAgentParts::detailsContent() && slot->shadowHost()) {
            Ref details = downcast<HTMLDetailsElement>(*slot->shadowHost());
            if (!details->hasAttributeWithoutSynchronization(HTMLNames::openAttr))
                return details;
        }
        return nullptr;
    };

    Vector<std::pair<Ref<HTMLElement>, RevealType>> ancestors;
    for (RefPtr ancestor = node; ancestor->parentElementInComposedTree(); ancestor = ancestor->parentElementInComposedTree()) {
        if (RefPtr element = dynamicDowncast<HTMLElement>(*ancestor); element && element->isHiddenUntilFound())
            ancestors.append({ element.releaseNonNull(), RevealType::HiddenUntilFound });
        if (RefPtr details = closedDetailsElementAncestor(*ancestor))
            ancestors.append({ details.releaseNonNull(), RevealType::ClosedDetails });
    }

    auto revealed = false;
    for (auto [element, revealType] : ancestors) {
        if (!element->isConnected())
            return revealed;
        switch (revealType) {
        case RevealType::ClosedDetails:
            if (element->hasAttributeWithoutSynchronization(HTMLNames::openAttr))
                return revealed;
            element->setBooleanAttribute(HTMLNames::openAttr, true);
            revealed = true;
            break;
        case RevealType::HiddenUntilFound:
            if (!element->isHiddenUntilFound())
                return revealed;
            element->dispatchEvent(Event::create(eventNames().beforematchEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
            if (!element->isConnected() || !element->isHiddenUntilFound())
                return revealed;
            element->setHidden({ });
            revealed = true;
            break;
        }
    }
    return revealed;
}

} // namespace WebCore
