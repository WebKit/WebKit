/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGImageElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGImageElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_LENGTH(SVGImageElement, SVGNames::widthAttr, Width, width)
DEFINE_ANIMATED_LENGTH(SVGImageElement, SVGNames::heightAttr, Height, height)
DEFINE_ANIMATED_PRESERVEASPECTRATIO(SVGImageElement, SVGNames::preserveAspectRatioAttr, PreserveAspectRatio, preserveAspectRatio)
DEFINE_ANIMATED_STRING(SVGImageElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGImageElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGImageElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(width)
    REGISTER_LOCAL_ANIMATED_PROPERTY(height)
    REGISTER_LOCAL_ANIMATED_PROPERTY(preserveAspectRatio)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGImageElement::SVGImageElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
    , m_width(LengthModeWidth)
    , m_height(LengthModeHeight)
    , m_imageLoader(*this)
{
    registerAnimatedPropertiesForSVGImageElement();
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

bool SVGImageElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed([] {
        HashSet<QualifiedName> set;
        SVGLangSpace::addSupportedAttributes(set);
        SVGExternalResourcesRequired::addSupportedAttributes(set);
        SVGURIReference::addSupportedAttributes(set);
        set.add({ SVGNames::xAttr.get(), SVGNames::yAttr.get(), SVGNames::widthAttr.get(), SVGNames::heightAttr.get(), SVGNames::preserveAspectRatioAttr.get() });
        return set;
    }());
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::preserveAspectRatioAttr) {
        SVGPreserveAspectRatioValue preserveAspectRatio;
        preserveAspectRatio.parse(value);
        setPreserveAspectRatioBaseValue(preserveAspectRatio);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        setXBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::widthAttr)
        setWidthBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError, ForbidNegativeLengths));
    else if (name == SVGNames::heightAttr)
        setHeightBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError, ForbidNegativeLengths));

    reportAttributeParsingError(parseError, name, value);

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);

    if (attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr) {
        invalidateSVGPresentationAttributeStyle();
        return;
    }

    bool isLengthAttribute = attrName == SVGNames::xAttr
                          || attrName == SVGNames::yAttr
                          || attrName == SVGNames::widthAttr
                          || attrName == SVGNames::heightAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    if (SVGURIReference::isKnownAttribute(attrName)) {
        m_imageLoader.updateFromElementIgnoringPreviousError();
        return;
    }

    auto* renderer = this->renderer();
    if (!renderer)
        return;

    if (isLengthAttribute) {
        if (downcast<RenderSVGImage>(*renderer).updateImageViewport())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    if (attrName == SVGNames::preserveAspectRatioAttr
        || SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

RenderPtr<RenderElement> SVGImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGImage>(*this, WTFMove(style));
}

bool SVGImageElement::haveLoadedRequiredResources()
{
    return !externalResourcesRequiredBaseValue() || !m_imageLoader.hasPendingActivity();
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

const AtomicString& SVGImageElement::imageSourceURL() const
{
    return getAttribute(XLinkNames::hrefAttr);
}

void SVGImageElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGGraphicsElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

void SVGImageElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    m_imageLoader.elementDidMoveToNewDocument();
    SVGGraphicsElement::didMoveToNewDocument(oldDocument, newDocument);
}

}
