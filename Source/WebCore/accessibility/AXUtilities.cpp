/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXUtilities.h"

#include "AXLoggerBase.h"
#include "AXObjectCache.h"
#include "Document.h"
#include "Element.h"
#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Node.h"
#include "RenderImage.h"
#include "RenderTreeBuilder.h"
#include <wtf/CheckedPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

using namespace HTMLNames;

ContainerNode* composedParentIgnoringDocumentFragments(const Node& node)
{
    RefPtr ancestor = node.parentInComposedTree();
    while (is<DocumentFragment>(ancestor.get()))
        ancestor = ancestor->parentInComposedTree();
    return ancestor.get();
}

ContainerNode* composedParentIgnoringDocumentFragments(const Node* node)
{
    return node ? composedParentIgnoringDocumentFragments(*node) : nullptr;
}

NodeName elementName(Node* node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element ? element->elementName() : ElementName::Unknown;
}

NodeName elementName(Node& node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element ? element->elementName() : ElementName::Unknown;
}

const RenderStyle* safeStyleFrom(Element& element)
{
    // We cannot resolve style (as computedStyle() does) if we are downstream of an existing render tree
    // update. Otherwise, a RELEASE_ASSERT preventing re-entrancy will be hit inside RenderTreeBuilder.
    return RenderTreeBuilder::current() ? element.existingComputedStyle() : element.computedStyle();
}

bool hasAccNameAttribute(Element& element)
{
    auto trimmed = [&] (const auto& attribute) {
        const auto& value = element.attributeWithDefaultARIA(attribute);
        if (value.isEmpty())
            return emptyString();
        auto copy = value.string();
        return copy.trim(isASCIIWhitespace);
    };

    // Avoid calculating the actual description here (e.g. resolving aria-labelledby), as it's expensive.
    // The spec is generally permissive in allowing user agents to not ensure complete validity of these attributes.
    // For example, https://w3c.github.io/svg-aam/#include_elements:
    // "It has an ‘aria-labelledby’ attribute or ‘aria-describedby’ attribute containing valid IDREF tokens. User agents MAY include elements with these attributes without checking for validity."
    if (trimmed(aria_labelAttr).length() || trimmed(aria_labelledbyAttr).length() || trimmed(aria_labeledbyAttr).length() || trimmed(aria_descriptionAttr).length() || trimmed(aria_describedbyAttr).length())
        return true;

    return element.attributeWithoutSynchronization(titleAttr).length();
}

RenderImage* toSimpleImage(RenderObject& renderer)
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer);
    if (!renderImage)
        return nullptr;

    // Exclude ImageButtons because they are treated as buttons, not as images.
    RefPtr node = renderer.node();
    if (is<HTMLInputElement>(node))
        return nullptr;

    // ImageMaps are not simple images.
    if (renderImage->imageMap())
        return nullptr;

    if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(node); imgElement && imgElement->hasAttributeWithoutSynchronization(usemapAttr))
        return nullptr;

#if ENABLE(VIDEO)
    // Exclude video and audio elements.
    if (is<HTMLMediaElement>(node))
        return nullptr;
#endif // ENABLE(VIDEO)

    return renderImage.get();
}

// FIXME: This probably belongs on Element.
bool hasRole(Element& element, StringView role)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (role.isNull())
        return roleValue.isEmpty();
    if (roleValue.isEmpty())
        return false;

    return SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes);
}

bool hasAnyRole(Element& element, Vector<StringView>&& roles)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (roleValue.isEmpty())
        return false;

    for (const auto& role : roles) {
        AX_DEBUG_ASSERT(!role.isEmpty());
        if (SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes))
            return true;
    }
    return false;
}

bool hasAnyRole(Element* element, Vector<StringView>&& roles)
{
    return element ? hasAnyRole(*element, WTFMove(roles)) : false;
}

bool hasTableRole(Element& element)
{
    return hasAnyRole(element, { "grid"_s, "table"_s, "treegrid"_s });
}

bool hasCellARIARole(Element& element)
{
    return hasAnyRole(element, { "gridcell"_s, "cell"_s, "columnheader"_s, "rowheader"_s });
}

bool hasPresentationRole(Element& element)
{
    return hasAnyRole(element, { "presentation"_s, "none"_s });
}

bool isRowGroup(Element& element)
{
    auto name = element.elementName();
    return name == ElementName::HTML_thead || name == ElementName::HTML_tbody || name == ElementName::HTML_tfoot || hasRole(element, "rowgroup"_s);
}

bool isRowGroup(Node* node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element && isRowGroup(*element);
}

void dumpAccessibilityTreeToStderr(Document& document)
{
    if (CheckedPtr cache = document.existingAXObjectCache()) {
        AXTreeData data = cache->treeData();
        SAFE_FPRINTF(stderr, "==AX Trees==\n%s\n%s\n", data.liveTree.utf8(), data.isolatedTree.utf8());
    }
}

} // WebCore

