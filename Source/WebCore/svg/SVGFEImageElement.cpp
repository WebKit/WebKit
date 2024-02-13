/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "FEImage.h"
#include "Image.h"
#include "LegacyRenderSVGResource.h"
#include "RenderObject.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatioValue.h"
#include "SVGRenderingContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEImageElement);

inline SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::feImageTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::preserveAspectRatioAttr, &SVGFEImageElement::m_preserveAspectRatio>();
    });
}

Ref<SVGFEImageElement> SVGFEImageElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEImageElement(tagName, document));
}

SVGFEImageElement::~SVGFEImageElement()
{
    clearResourceReferences();
}

bool SVGFEImageElement::renderingTaintsOrigin() const
{
    if (!m_cachedImage)
        return false;
    auto* image = m_cachedImage->image();
    return image && image->renderingTaintsOrigin();
}

void SVGFEImageElement::clearResourceReferences()
{
    if (m_cachedImage) {
        m_cachedImage->removeClient(*this);
        m_cachedImage = nullptr;
    }

    removeElementReference();
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

    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScopeForSVGReferences());
    if (!target.element) {
        if (target.identifier.isEmpty())
            requestImageResource();
        else {
            treeScopeForSVGReferences().addPendingSVGResource(target.identifier, *this);
            ASSERT(hasPendingResources());
        }
    } else if (RefPtr element = dynamicDowncast<SVGElement>(*target.element))
        element->addReferencingElement(*this);

    updateSVGRendererForElementChange();
}

void SVGFEImageElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::preserveAspectRatioAttr)
        m_preserveAspectRatio->setBaseValInternal(SVGPreserveAspectRatioValue { newValue });

    SVGURIReference::parseAttribute(name, newValue);
    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFEImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::preserveAspectRatioAttr);
        InstanceInvalidationGuard guard(*this);
        updateSVGRendererForElementChange();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
        markFilterEffectForRebuild();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

Node::InsertedIntoAncestorResult SVGFEImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGFilterPrimitiveStandardAttributes::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGFEImageElement::didFinishInsertingNode()
{
    SVGFilterPrimitiveStandardAttributes::didFinishInsertingNode();
    buildPendingResource();
}

void SVGFEImageElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGFilterPrimitiveStandardAttributes::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        clearResourceReferences();
}

void SVGFEImageElement::notifyFinished(CachedResource&, const NetworkLoadMetrics&)
{
    if (!isConnected())
        return;

    RefPtr parent = parentElement();

    if (!parent || !parent->hasTagName(SVGNames::filterTag))
        return;

    RenderElement* parentRenderer = parent->renderer();
    if (!parentRenderer)
        return;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // FIXME: [LBSE] Implement filters.
    if (document().settings().layerBasedSVGEngineEnabled())
        return;
#endif
    LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidation(*parentRenderer);
}

std::tuple<RefPtr<ImageBuffer>, FloatRect> SVGFEImageElement::imageBufferForEffect(const GraphicsContext& destinationContext) const
{
    auto target = SVGURIReference::targetElementFromIRIString(href(), const_cast<SVGFEImageElement&>(*this).treeScopeForSVGReferences());
    if (!is<SVGElement>(target.element))
        return { };

    if (isDescendantOrShadowDescendantOf(target.element.get()))
        return { };

    auto contextNode = static_pointer_cast<SVGElement>(target.element);
    auto renderer = contextNode->renderer();
    if (!renderer)
        return { };

    auto absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*renderer);
    if (!absoluteTransform.isInvertible())
        return { };

    // Ignore 2D rotation, as it doesn't affect the image size.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());
    auto imageRect = renderer->repaintRectInLocalCoordinates();

    auto imageBuffer = destinationContext.createScaledImageBuffer(imageRect, scale);
    if (!imageBuffer)
        return { };

    auto& context = imageBuffer->context();
    SVGRenderingContext::renderSubtreeToContext(context, *renderer, AffineTransform());

    return { imageBuffer, imageRect };
}

RefPtr<FilterEffect> SVGFEImageElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const
{
    if (m_cachedImage) {
        auto image = m_cachedImage->imageForRenderer(renderer());
        if (!image || image->isNull())
            return nullptr;

        auto nativeImage = image->preTransformedNativeImageForCurrentFrame();
        if (!nativeImage)
            return nullptr;

        auto imageRect = FloatRect { { }, image->size() };
        return FEImage::create({ nativeImage.releaseNonNull() }, imageRect, preserveAspectRatio());
    }

    auto [imageBuffer, imageRect] = imageBufferForEffect(destinationContext);
    if (!imageBuffer)
        return nullptr;

    return FEImage::create({ imageBuffer.releaseNonNull() }, imageRect, preserveAspectRatio());
}

void SVGFEImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGFilterPrimitiveStandardAttributes::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

} // namespace WebCore
