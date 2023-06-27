/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ReferencedSVGResources.h"

#include "FilterOperations.h"
#include "PathOperation.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderStyle.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFilterElement.h"
#include "SVGResourceElementClient.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

class CSSSVGResourceElementClient final : public SVGResourceElementClient {
    WTF_MAKE_ISO_ALLOCATED(CSSSVGResourceElementClient);
public:
    CSSSVGResourceElementClient(RenderElement& clientRenderer)
        : m_clientRenderer(clientRenderer)
    {
    }

    void resourceChanged(SVGElement&) final;

private:
    RenderElement& m_clientRenderer;
};

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSSVGResourceElementClient);

void CSSSVGResourceElementClient::resourceChanged(SVGElement&)
{
    if (!m_clientRenderer.renderTreeBeingDestroyed())
        m_clientRenderer.repaint();
}

WTF_MAKE_ISO_ALLOCATED_IMPL(ReferencedSVGResources);

ReferencedSVGResources::ReferencedSVGResources(RenderElement& renderer)
    : m_renderer(renderer)
{
}

ReferencedSVGResources::~ReferencedSVGResources()
{
    auto& treeScope = m_renderer.treeScopeForSVGReferences();
    for (auto& targetID : copyToVector(m_elementClients.keys()))
        removeClientForTarget(treeScope, targetID);
}

void ReferencedSVGResources::addClientForTarget(SVGElement& targetElement, const AtomString& targetID)
{
    m_elementClients.ensure(targetID, [&] {
        auto client = makeUnique<CSSSVGResourceElementClient>(m_renderer);
        targetElement.addReferencingCSSClient(*client);
        return client;
    });
}

void ReferencedSVGResources::removeClientForTarget(TreeScope& treeScope, const AtomString& targetID)
{
    auto client = m_elementClients.take(targetID);

    auto* targetElement = treeScope.getElementById(targetID);
    if (auto* svgElement = dynamicDowncast<SVGElement>(targetElement))
        svgElement->removeReferencingCSSClient(*client);
}

Vector<std::pair<AtomString, QualifiedName>> ReferencedSVGResources::referencedSVGResourceIDs(const RenderStyle& style)
{
    Vector<std::pair<AtomString, QualifiedName>> referencedResources;
    if (is<ReferencePathOperation>(style.clipPath())) {
        auto& clipPath = downcast<ReferencePathOperation>(*style.clipPath());
        if (!clipPath.fragment().isEmpty())
            referencedResources.append({ clipPath.fragment(), SVGNames::clipPathTag });
    }

    if (style.hasFilter()) {
        const auto& filterOperations = style.filter();
        for (auto& operation : filterOperations.operations()) {
            auto& filterOperation = *operation;
            if (filterOperation.type() == FilterOperation::Type::Reference) {
                const auto& referenceFilterOperation = downcast<ReferenceFilterOperation>(filterOperation);
                if (!referenceFilterOperation.fragment().isEmpty())
                    referencedResources.append({ referenceFilterOperation.fragment(), SVGNames::filterTag });
            }
        }
    }
    
    return referencedResources;
}

void ReferencedSVGResources::updateReferencedResources(TreeScope& treeScope, const Vector<std::pair<AtomString, QualifiedName>>& referencedResources)
{
    HashSet<AtomString> oldKeys;
    for (auto& key : m_elementClients.keys())
        oldKeys.add(key);

    for (auto& [targetID, tagName] : referencedResources) {
        auto* element = elementForResourceID(treeScope, targetID, tagName);
        if (!element)
            continue;

        addClientForTarget(*element, targetID);
        oldKeys.remove(targetID);
    }
    
    for (auto& targetID : oldKeys)
        removeClientForTarget(treeScope, targetID);
}

// SVG code uses getRenderSVGResourceById<>, but that works in terms of renderers. We need to find resources
// before the render tree is fully constructed, so this works on Elements.
SVGElement* ReferencedSVGResources::elementForResourceID(TreeScope& treeScope, const AtomString& resourceID, const QualifiedName& tagName)
{
    auto* element = treeScope.getElementById(resourceID);
    if (!element || !element->hasTagName(tagName))
        return nullptr;

    return downcast<SVGElement>(element);
}

SVGFilterElement* ReferencedSVGResources::referencedFilterElement(TreeScope& treeScope, const ReferenceFilterOperation& referenceFilter)
{
    if (referenceFilter.fragment().isEmpty())
        return nullptr;
    auto* element = elementForResourceID(treeScope, referenceFilter.fragment(), SVGNames::filterTag);
    return element ? downcast<SVGFilterElement>(element) : nullptr;
}

RenderSVGResourceClipper* ReferencedSVGResources::referencedClipperRenderer(TreeScope& treeScope, const ReferencePathOperation& clipPath)
{
    if (clipPath.fragment().isEmpty())
        return nullptr;
    // For some reason, SVG stores a cache of id -> renderer, rather than just using getElementById() and renderer().
    return getRenderSVGResourceById<RenderSVGResourceClipper>(treeScope, clipPath.fragment());
}

} // namespace WebCore
