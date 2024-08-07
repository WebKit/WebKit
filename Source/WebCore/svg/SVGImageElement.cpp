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
#include "HTMLParserIdioms.h"
#include "LegacyRenderSVGImage.h"
#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderImageResource.h"
#include "RenderSVGImage.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGImageElement);

inline SVGImageElement::SVGImageElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this), TypeFlag::HasDidMoveToNewDocument)
    , SVGURIReference(this)
    , m_imageLoader(*this)
{
    ASSERT(hasTagName(SVGNames::imageTag));

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

CachedImage* SVGImageElement::cachedImage() const
{
    return m_imageLoader.image();
}

bool SVGImageElement::renderingTaintsOrigin() const
{
    CachedResourceHandle cachedImage = this->cachedImage();
    if (!cachedImage)
        return false;

    RefPtr image = cachedImage->image();
    if (!image)
        return false;

    if (image->renderingTaintsOrigin())
        return true;

    if (image->sourceURL().protocolIsData())
        return false;

    return cachedImage->isCORSCrossOrigin();
}

void SVGImageElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;
    switch (name.nodeName()) {
    case AttributeNames::preserveAspectRatioAttr:
        Ref { m_preserveAspectRatio }->setBaseValInternal(SVGPreserveAspectRatioValue { newValue });
        return;
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError, SVGLengthNegativeValuesMode::Forbid));
        break;
    case AttributeNames::crossoriginAttr:
        if (parseCORSSettingsAttribute(oldValue) != parseCORSSettingsAttribute(newValue))
            m_imageLoader.updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr) {
            updateRelativeLengthsInformation();

            if (is<RenderSVGImage>(renderer()))
                updateSVGRendererForElementChange();

            if (CheckedPtr image = dynamicDowncast<LegacyRenderSVGImage>(renderer())) {
                if (!image->updateImageViewport())
                    return;
                updateSVGRendererForElementChange();
            }
        } else if (attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr)
            setPresentationalHintStyleIsDirty();
        else {
            ASSERT(attrName == SVGNames::preserveAspectRatioAttr);
            updateSVGRendererForElementChange();
        }
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        m_imageLoader.updateFromElementIgnoringPreviousError();
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGImage>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGImage>(*this, WTFMove(style));
}

bool SVGImageElement::haveLoadedRequiredResources()
{
    return !m_imageLoader.hasPendingActivity();
}

void SVGImageElement::didAttachRenderers()
{
    SVGGraphicsElement::didAttachRenderers();

    if (CheckedPtr image = dynamicDowncast<RenderSVGImage>(renderer()); image && !image->imageResource().cachedImage()) {
        image->checkedImageResource()->setCachedImage(m_imageLoader.protectedImage());
        return;
    }

    if (CheckedPtr image = dynamicDowncast<LegacyRenderSVGImage>(renderer()); image && !image->imageResource().cachedImage()) {
        image->checkedImageResource()->setCachedImage(m_imageLoader.protectedImage());
        return;
    }
}

Node::InsertedIntoAncestorResult SVGImageElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    // Update image loader, as soon as we're living in the tree.
    // We can only resolve base URIs properly, after that!
    m_imageLoader.updateFromElement();
    return result;
}

const AtomString& SVGImageElement::imageSourceURL() const
{
    return getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
}

void SVGImageElement::setCrossOrigin(const AtomString& value)
{
    setAttributeWithoutSynchronization(HTMLNames::crossoriginAttr, value);
}

String SVGImageElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(HTMLNames::crossoriginAttr));
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
