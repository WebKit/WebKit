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

#include "CSSBasicShapes.h"
#include "LegacyRenderSVGPath.h"
#include "LegacyRenderSVGResource.h"
#include "MutableStyleProperties.h"
#include "RenderSVGPath.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGMPathElement.h"
#include "SVGNames.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include "SVGRenderStyle.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGPathElement);

class PathSegListCache {
public:
    static PathSegListCache& singleton();

    std::optional<DataRef<SVGPathByteStream::Data>> get(const AtomString& attributeValue) const;
    void add(const AtomString& attributeValue, DataRef<SVGPathByteStream::Data>);
    void clear();

private:
    friend class NeverDestroyed<PathSegListCache, MainThreadAccessTraits>;
    PathSegListCache() = default;

    HashMap<AtomString, DataRef<SVGPathByteStream::Data>> m_cache;
    uint64_t m_sizeInBytes { 0 };
    static constexpr uint64_t maxItemSizeInBytes = 5 * 1024; // 5 Kb.
    static constexpr uint64_t maxCacheSizeInBytes = 150 * 1024; // 150 Kb.
};

PathSegListCache& PathSegListCache::singleton()
{
    static MainThreadNeverDestroyed<PathSegListCache> cache;
    return cache;
}

std::optional<DataRef<SVGPathByteStream::Data>> PathSegListCache::get(const AtomString& attributeValue) const
{
    return m_cache.getOptional(attributeValue);
}

void PathSegListCache::add(const AtomString& attributeValue, DataRef<SVGPathByteStream::Data> data)
{
    size_t newDataSize = data->size();
    if (UNLIKELY(newDataSize > maxItemSizeInBytes))
        return;

    m_sizeInBytes += newDataSize;
    while (m_sizeInBytes > maxCacheSizeInBytes) {
        ASSERT(!m_cache.isEmpty());
        auto iteratorToRemove = m_cache.random();
        ASSERT(iteratorToRemove != m_cache.end());
        ASSERT(m_sizeInBytes >= iteratorToRemove->value->size());
        m_sizeInBytes -= iteratorToRemove->value->size();
        m_cache.remove(iteratorToRemove);
    }
    m_cache.add(attributeValue, WTFMove(data));
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
            Ref { m_pathSegList }->baseVal()->clearByteStreamData();
        else if (auto data = cache.get(newValue))
            Ref { m_pathSegList }->baseVal()->updateByteStreamData(WTFMove(data.value()));
        else if (Ref { m_pathSegList }->baseVal()->parse(newValue))
            cache.add(newValue, m_pathSegList->baseVal()->existingPathByteStream().data());
        else
            protectedDocument()->checkedSVGExtensions()->reportError(makeString("Problem parsing d=\""_s, newValue, "\""_s));
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

        if (CheckedPtr path = dynamicDowncast<RenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        if (CheckedPtr path = dynamicDowncast<LegacyRenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        updateSVGRendererForElementChange();
        if (document().settings().cssDPropertyEnabled())
            setPresentationalHintStyleIsDirty();
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    for (auto& element : referencingElements()) {
        if (RefPtr mpathElement = dynamicDowncast<SVGMPathElement>(element.get()))
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
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    return getTotalLengthOfSVGPathByteStream(pathByteStream());
}

ExceptionOr<Ref<SVGPoint>> SVGPathElement::getPointAtLength(float distance) const
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    // Spec: Clamp distance to [0, length].
    distance = clampTo<float>(distance, 0, getTotalLength());

    // Spec: Return a newly created, detached SVGPoint object.
    return SVGPoint::create(getPointAtLengthOfSVGPathByteStream(pathByteStream(), distance));
}

unsigned SVGPathElement::getPathSegAtLength(float length) const
{
    protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    return getSVGPathSegAtLengthFromSVGPathByteStream(pathByteStream(), length);
}

FloatRect SVGPathElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    if (styleUpdateStrategy == AllowStyleUpdate)
        protectedDocument()->updateLayoutIgnorePendingStylesheets({ LayoutOptions::ContentVisibilityForceLayout }, this);

    // FIXME: Eventually we should support getBBox for detached elements.
    // FIXME: If the path is null it means we're calling getBBox() before laying out this element,
    // which is an error.

    if (CheckedPtr path = dynamicDowncast<RenderSVGPath>(renderer()); path && path->hasPath())
        return path->path().boundingRect();

    if (CheckedPtr path = dynamicDowncast<LegacyRenderSVGPath>(renderer()); path && path->hasPath())
        return path->path().boundingRect();

    return { };
}

RenderPtr<RenderElement> SVGPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGPath>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGPath>(*this, WTFMove(style));
}

const SVGPathByteStream& SVGPathElement::pathByteStream() const
{
    if (document().settings().cssDPropertyEnabled()) {
        if (CheckedPtr renderer = this->renderer()) {
            if (RefPtr basicShapePath = renderer->style().d()) {
                if (WeakPtr pathData = basicShapePath->pathData())
                    return *pathData;
            }
            return SVGPathByteStream::empty();
        }
    }

    return Ref { m_pathSegList }->currentPathByteStream();
}

Path SVGPathElement::path() const
{
    if (document().settings().cssDPropertyEnabled()) {
        if (CheckedPtr renderer = this->renderer()) {
            if (RefPtr basicShapePath = renderer->style().d())
                return basicShapePath->path({ });
            return { };
        }
    }

    return Ref { m_pathSegList }->currentPath();
}

void SVGPathElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == SVGNames::dAttr && document().settings().cssDPropertyEnabled())
        collectDPresentationalHint(style);
    else
        SVGGeometryElement::collectPresentationalHintsForAttribute(name, value, style);
}

void SVGPathElement::collectExtraStyleForPresentationalHints(MutableStyleProperties& style)
{
    if (!document().settings().cssDPropertyEnabled())
        return;
    if (style.findPropertyIndex(CSSPropertyD) == -1)
        collectDPresentationalHint(style);
}

void SVGPathElement::collectDPresentationalHint(MutableStyleProperties& style)
{
    ASSERT(document().settings().cssDPropertyEnabled());
    // In the case of the `d` property, we want to avoid providing a string value since it will require
    // the path data to be parsed again and path data can be unwieldy.
    auto property = cssPropertyIdForSVGAttributeName(SVGNames::dAttr, document().protectedSettings());
    // The WindRule value passed here is not relevant for the `d` property.
    auto cssPathValue = CSSPathValue::create(Ref { m_pathSegList }->currentPathByteStream(), WindRule::NonZero);
    addPropertyToPresentationalHintStyle(style, property, WTFMove(cssPathValue));
}

void SVGPathElement::pathDidChange()
{
    invalidateMPathDependencies();
}

}
