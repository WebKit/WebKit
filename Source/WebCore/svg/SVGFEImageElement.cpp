/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGFEImageElement.h"

#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "Document.h"
#include "Image.h"
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatioValue.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEImageElement);

inline SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , SVGExternalResourcesRequired(this)
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::feImageTag));
    registerAttributes();
}

Ref<SVGFEImageElement> SVGFEImageElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEImageElement(tagName, document));
}

SVGFEImageElement::~SVGFEImageElement()
{
    clearResourceReferences();
}

bool SVGFEImageElement::hasSingleSecurityOrigin() const
{
    if (!m_cachedImage)
        return true;
    auto* image = m_cachedImage->image();
    return !image || image->hasSingleSecurityOrigin();
}

void SVGFEImageElement::clearResourceReferences()
{
    if (m_cachedImage) {
        m_cachedImage->removeClient(*this);
        m_cachedImage = nullptr;
    }

    document().accessSVGExtensions().removeAllTargetReferencesForElement(this);
}

void SVGFEImageElement::requestImageResource()
{
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

    CachedResourceRequest request(ResourceRequest(document().completeURL(href())), options);
    request.setInitiator(*this);
    m_cachedImage = document().cachedResourceLoader().requestImage(WTFMove(request)).value_or(nullptr);

    if (m_cachedImage)
        m_cachedImage->addClient(*this);
}

void SVGFEImageElement::buildPendingResource()
{
    clearResourceReferences();
    if (!isConnected())
        return;

    auto target = SVGURIReference::targetElementFromIRIString(href(), document());
    if (!target.element) {
        if (target.identifier.isEmpty())
            requestImageResource();
        else {
            document().accessSVGExtensions().addPendingResource(target.identifier, this);
            ASSERT(hasPendingResources());
        }
    } else if (target.element->isSVGElement()) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions().addElementReferencingTarget(this, downcast<SVGElement>(target.element.get()));
    }

    invalidate();
}

void SVGFEImageElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::preserveAspectRatioAttr, &SVGFEImageElement::m_preserveAspectRatio>();
}

void SVGFEImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::preserveAspectRatioAttr) {
        SVGPreserveAspectRatioValue preserveAspectRatio;
        preserveAspectRatio.parse(value);
        m_preserveAspectRatio.setValue(preserveAspectRatio);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGFEImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::preserveAspectRatioAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

Node::InsertedIntoAncestorResult SVGFEImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGFilterPrimitiveStandardAttributes::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGFEImageElement::didFinishInsertingNode()
{
    buildPendingResource();
}

void SVGFEImageElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGFilterPrimitiveStandardAttributes::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        clearResourceReferences();
}

void SVGFEImageElement::notifyFinished(CachedResource&)
{
    if (!isConnected())
        return;

    auto parent = makeRefPtr(parentElement());

    if (!parent || !parent->hasTagName(SVGNames::filterTag))
        return;

    RenderElement* parentRenderer = parent->renderer();
    if (!parentRenderer)
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*parentRenderer);
}

RefPtr<FilterEffect> SVGFEImageElement::build(SVGFilterBuilder*, Filter& filter)
{
    if (m_cachedImage)
        return FEImage::createWithImage(filter, m_cachedImage->imageForRenderer(renderer()), preserveAspectRatio());
    return FEImage::createWithIRIReference(filter, document(), href(), preserveAspectRatio());
}

void SVGFEImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGFilterPrimitiveStandardAttributes::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

}
