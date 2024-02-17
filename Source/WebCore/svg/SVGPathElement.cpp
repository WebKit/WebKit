/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "SVGPathElement.h"

#include "LegacyRenderSVGPath.h"
#include "LegacyRenderSVGResource.h"
#include "RenderSVGPath.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGMPathElement.h"
#include "SVGNames.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGPathElement);

class PathSegListCache {
public:
    static PathSegListCache& singleton();

    const SVGPathByteStream::Data* get(const AtomString& attributeValue) const;
    void add(const AtomString& attributeValue, const SVGPathByteStream::Data&);
    void clear();

private:
    friend class NeverDestroyed<PathSegListCache, MainThreadAccessTraits>;
    PathSegListCache() = default;

    HashMap<AtomString, SVGPathByteStream::Data> m_cache;
    uint64_t m_sizeInBytes { 0 };
    static constexpr uint64_t maxItemSizeInBytes = 5 * 1024; // 5 Kb.
    static constexpr uint64_t maxCacheSizeInBytes = 150 * 1024; // 150 Kb.
};

PathSegListCache& PathSegListCache::singleton()
{
    static MainThreadNeverDestroyed<PathSegListCache> cache;
    return cache;
}

const SVGPathByteStream::Data* PathSegListCache::get(const AtomString& attributeValue) const
{
    auto iterator = m_cache.find(attributeValue);
    return iterator != m_cache.end() ? &iterator->value : nullptr;
}

void PathSegListCache::add(const AtomString& attributeValue, const SVGPathByteStream::Data& data)
{
    size_t newDataSize = data.size();
    if (UNLIKELY(newDataSize > maxItemSizeInBytes))
        return;

    m_sizeInBytes += newDataSize;
    while (m_sizeInBytes > maxCacheSizeInBytes) {
        ASSERT(!m_cache.isEmpty());
        auto iteratorToRemove = m_cache.random();
        ASSERT(iteratorToRemove != m_cache.end());
        ASSERT(m_sizeInBytes >= iteratorToRemove->value.size());
        m_sizeInBytes -= iteratorToRemove->value.size();
        m_cache.remove(iteratorToRemove);
    }
    m_cache.add(attributeValue, data);
}

void PathSegListCache::clear()
{
    m_cache.clear();
    m_sizeInBytes = 0;
}

inline SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::pathTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::dAttr, &SVGPathElement::m_pathSegList>();
    });
}

Ref<SVGPathElement> SVGPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGPathElement(tagName, document));
}

void SVGPathElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::dAttr) {
        auto& cache = PathSegListCache::singleton();
        if (newValue.isEmpty())
            m_pathSegList->baseVal()->updateByteStreamData({ });
        else if (auto* data = cache.get(newValue))
            m_pathSegList->baseVal()->updateByteStreamData(*data);
        else if (m_pathSegList->baseVal()->parse(newValue))
            cache.add(newValue, m_pathSegList->baseVal()->existingPathByteStream().data());
        else
            document().accessSVGExtensions().reportError("Problem parsing d=\"" + newValue + "\"");
    }

    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGPathElement::clearCache()
{
    PathSegListCache::singleton().clear();
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::dAttr);
        InstanceInvalidationGuard guard(*this);
        invalidateMPathDependencies();

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* path = dynamicDowncast<RenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();
#endif
        if (auto* path = dynamicDowncast<LegacyRenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        updateSVGRendererForElementChange();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    for (auto& element : referencingElements()) {
        if (auto* mpathElement = dynamicDowncast<SVGMPathElement>(element.get()))
            mpathElement->targetPathChanged();
    }
}

Node::InsertedIntoAncestorResult SVGPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGGeometryElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    invalidateMPathDependencies();
    return result;
}

void SVGPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGGeometryElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    invalidateMPathDependencies();
}

float SVGPathElement::getTotalLength() const
{
    return getTotalLengthOfSVGPathByteStream(pathByteStream());
}

ExceptionOr<Ref<SVGPoint>> SVGPathElement::getPointAtLength(float distance) const
{
    // Spec: Clamp distance to [0, length].
    distance = clampTo<float>(distance, 0, getTotalLength());

    // Spec: Return a newly created, detached SVGPoint object.
    return SVGPoint::create(getPointAtLengthOfSVGPathByteStream(pathByteStream(), distance));
}

unsigned SVGPathElement::getPathSegAtLength(float length) const
{
    return getSVGPathSegAtLengthFromSVGPathByteStream(pathByteStream(), length);
}

FloatRect SVGPathElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    if (styleUpdateStrategy == AllowStyleUpdate)
        document().updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    // FIXME: Eventually we should support getBBox for detached elements.
    // FIXME: If the path is null it means we're calling getBBox() before laying out this element,
    // which is an error.

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* path = dynamicDowncast<RenderSVGPath>(renderer()); path && path->hasPath())
            return path->path().boundingRect();
#endif
        if (auto* path = dynamicDowncast<LegacyRenderSVGPath>(renderer()); path && path->hasPath())
            return path->path().boundingRect();

    return { };
}

RenderPtr<RenderElement> SVGPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGPath>(*this, WTFMove(style));
#endif
    return createRenderer<LegacyRenderSVGPath>(*this, WTFMove(style));
}

}
