/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGImageElement.h"

#include "CSSPropertyNames.h"
#include "RenderImageResource.h"
#include "RenderSVGImage.h"
#include "RenderSVGResource.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGImageElement);

inline SVGImageElement::SVGImageElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , SVGURIReference(this)
    , m_imageLoader(*this)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGImageElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGImageElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGImageElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGImageElement::m_height>();
        PropertyRegistry::registerProperty<SVGNames::preserveAspectRatioAttr, &SVGImageElement::m_preserveAspectRatio>();
    });
}

Ref<SVGImageElement> SVGImageElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGImageElement(tagName, document));
}

bool SVGImageElement::hasSingleSecurityOrigin() const
{
    auto* renderer = downcast<RenderSVGImage>(this->renderer());
    if (!renderer || !renderer->imageResource().cachedImage())
        return true;
    auto* image = renderer->imageResource().cachedImage()->image();
    return !image || image->hasSingleSecurityOrigin();
}

void SVGImageElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::preserveAspectRatioAttr) {
        m_preserveAspectRatio->setBaseValInternal(SVGPreserveAspectRatioValue { value });
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError));
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, value, parseError, SVGLengthNegativeValuesMode::Forbid));
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, value, parseError, SVGLengthNegativeValuesMode::Forbid));

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr) {
        InstanceInvalidationGuard guard(*this);
        updateRelativeLengthsInformation();

        if (auto* renderer = this->renderer()) {
            if (downcast<RenderSVGImage>(*renderer).updateImageViewport())
                RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        }
        return;
    }

    if (attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    if (attrName == SVGNames::preserveAspectRatioAttr) {
        InstanceInvalidationGuard guard(*this);
        if (auto* renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        m_imageLoader.updateFromElementIgnoringPreviousError();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGImage>(*this, WTFMove(style));
}

bool SVGImageElement::haveLoadedRequiredResources()
{
    return !m_imageLoader.hasPendingActivity();
}

void SVGImageElement::didAttachRenderers()
{
    if (auto* imageObj = downcast<RenderSVGImage>(renderer())) {
        if (imageObj->imageResource().cachedImage())
            return;

        imageObj->imageResource().setCachedImage(m_imageLoader.image());
    }
}

Node::InsertedIntoAncestorResult SVGImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    // Update image loader, as soon as we're living in the tree.
    // We can only resolve base URIs properly, after that!
    m_imageLoader.updateFromElement();
    return InsertedIntoAncestorResult::Done;
}

const AtomString& SVGImageElement::imageSourceURL() const
{
    return getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
}

void SVGImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGGraphicsElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

void SVGImageElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    m_imageLoader.elementDidMoveToNewDocument(oldDocument);
    SVGGraphicsElement::didMoveToNewDocument(oldDocument, newDocument);
}

}
