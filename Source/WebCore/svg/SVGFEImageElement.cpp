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
#include "RenderObject.h"
#include "RenderSVGResource.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatioValue.h"
#include "SVGRenderingContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEImageElement);

inline SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
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

    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScope());
    if (!target.element) {
        if (target.identifier.isEmpty())
            requestImageResource();
        else {
            document().accessSVGExtensions().addPendingResource(target.identifier, *this);
            ASSERT(hasPendingResources());
        }
    } else if (is<SVGElement>(*target.element))
        downcast<SVGElement>(*target.element).addReferencingElement(*this);

    invalidate();
}

void SVGFEImageElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::preserveAspectRatioAttr) {
        m_preserveAspectRatio->setBaseValInternal(SVGPreserveAspectRatioValue { value });
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
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

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*parentRenderer);
}

static inline IntRect scaledImageBufferRect(const FloatRect& rect, const FloatSize& scale)
{
    auto scaledRect = rect;
    scaledRect.scale(scale);
    return enclosingIntRect(scaledRect);
}

static inline FloatSize clampingScaleForImageBufferSize(const FloatSize& size)
{
    FloatSize clampingScale(1, 1);
    ImageBuffer::sizeNeedsClamping(size, clampingScale);
    return clampingScale;
}

static RefPtr<ImageBuffer> createImageBuffer(const FloatRect& rect, const FloatSize& scale, HostWindow* hostWindow)
{
    auto scaledRect = scaledImageBufferRect(rect, scale);
    if (scaledRect.isEmpty())
        return nullptr;

    auto clampingScale = clampingScaleForImageBufferSize(scaledRect.size());

    auto imageBuffer = ImageBuffer::create(scaledRect.size() * clampingScale, RenderingMode::Unaccelerated, ShouldUseDisplayList::No, RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, hostWindow);
    if (!imageBuffer)
        return nullptr;

    imageBuffer->context().scale(clampingScale);
    imageBuffer->context().translate(-scaledRect.location());
    imageBuffer->context().scale(scale);
    return imageBuffer;
}

std::tuple<RefPtr<ImageBuffer>, FloatRect> SVGFEImageElement::imageBufferForEffect() const
{
    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScope());
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

    // FIXME: Replace this call with GraphicsContext::createImageBuffer() once the destination GraphicsContext is passed to this function.
    auto imageBuffer = createImageBuffer(imageRect, scale, renderer->hostWindow());
    if (!imageBuffer)
        return { };

    auto& context = imageBuffer->context();
    SVGRenderingContext::renderSubtreeToContext(context, *renderer, AffineTransform());

    return { imageBuffer, imageRect };
}

RefPtr<FilterEffect> SVGFEImageElement::filterEffect(const SVGFilterBuilder&, const FilterEffectVector&) const
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

    auto [imageBuffer, imageRect] = imageBufferForEffect();
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
