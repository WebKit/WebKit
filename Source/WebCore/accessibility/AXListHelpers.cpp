/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXListHelpers.h"

#include "AXObjectCacheInlines.h"
#include "AXUtilities.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilityRenderObject.h"
#include "ContainerNodeInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "PseudoElement.h"

namespace WebCore {

using namespace HTMLNames;

bool AXListHelpers::isAccessibilityList(Element& element)
{
    if (hasAnyRole(element, { "list"_s, "directory"_s }))
        return true;

    // Call it a list if it has no ARIA role and a list tag.
    auto name = element.elementName();
    return hasRole(element, nullAtom()) && (name == ElementName::HTML_ul || name == ElementName::HTML_ol || name == ElementName::HTML_dl || name == ElementName::HTML_menu);
}

bool AXListHelpers::childHasPseudoVisibleListItemMarkers(const Node& node)
{
    // Check if the list item has a pseudo-element that should be accessible (e.g. an image or text)
    RefPtr element = dynamicDowncast<Element>(node);
    RefPtr beforePseudo = element ? element->beforePseudoElement() : nullptr;
    if (!beforePseudo)
        return false;

    CheckedPtr cache = protect(element->document())->axObjectCache();
    RefPtr axBeforePseudo = cache ? cache->getOrCreate(*beforePseudo) : nullptr;
    if (!axBeforePseudo)
        return false;

    if (!axBeforePseudo->isIgnored())
        return true;

    for (const auto& child : axBeforePseudo->unignoredChildren()) {
        if (!child->isIgnored())
            return true;
    }

    // Platforms which expose rendered text content through the parent element will treat
    // those renderers as "ignored" objects.
#if USE(ATSPI)
    String text = axBeforePseudo->textUnderElement();
    return !text.isEmpty() && !text.containsOnly<isASCIIWhitespace>();
#else
    return false;
#endif
}

} // namespace WebCore
