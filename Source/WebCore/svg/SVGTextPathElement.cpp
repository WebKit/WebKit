/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
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
#include "SVGTextPathElement.h"

#include "RenderSVGResource.h"
#include "RenderSVGTextPath.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextPathElement);

inline SVGTextPathElement::SVGTextPathElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document)
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::textPathTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::startOffsetAttr, &SVGTextPathElement::m_startOffset>();
        PropertyRegistry::registerProperty<SVGNames::methodAttr, SVGTextPathMethodType, &SVGTextPathElement::m_method>();
        PropertyRegistry::registerProperty<SVGNames::spacingAttr, SVGTextPathSpacingType, &SVGTextPathElement::m_spacing>();
    });
}

Ref<SVGTextPathElement> SVGTextPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGTextPathElement(tagName, document));
}

SVGTextPathElement::~SVGTextPathElement()
{
    clearResourceReferences();
}

void SVGTextPathElement::clearResourceReferences()
{
    removeElementReference();
}

void SVGTextPathElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::startOffsetAttr)
        m_startOffset->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, value, parseError));
    else if (name == SVGNames::methodAttr) {
        SVGTextPathMethodType propertyValue = SVGPropertyTraits<SVGTextPathMethodType>::fromString(value);
        if (propertyValue > 0)
            m_method->setBaseValInternal<SVGTextPathMethodType>(propertyValue);
    } else if (name == SVGNames::spacingAttr) {
        SVGTextPathSpacingType propertyValue = SVGPropertyTraits<SVGTextPathSpacingType>::fromString(value);
        if (propertyValue > 0)
            m_spacing->setBaseValInternal<SVGTextPathSpacingType>(propertyValue);
    }

    reportAttributeParsingError(parseError, name, value);

    SVGTextContentElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGTextPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);

        if (attrName == SVGNames::startOffsetAttr)
            updateRelativeLengthsInformation();

        if (auto renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        if (auto renderer = this->renderer())
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    SVGTextContentElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGTextPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGTextPath>(*this, WTFMove(style));
}

bool SVGTextPathElement::childShouldCreateRenderer(const Node& child) const
{
    if (child.isTextNode()
        || child.hasTagName(SVGNames::aTag)
        || child.hasTagName(SVGNames::trefTag)
        || child.hasTagName(SVGNames::tspanTag))
        return true;

    return false;
}

bool SVGTextPathElement::rendererIsNeeded(const RenderStyle& style)
{
    if (parentNode()
        && (parentNode()->hasTagName(SVGNames::aTag)
            || parentNode()->hasTagName(SVGNames::textTag)))
        return StyledElement::rendererIsNeeded(style);

    return false;
}

void SVGTextPathElement::buildPendingResource()
{
    clearResourceReferences();
    if (!isConnected())
        return;

    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScope());
    if (!target.element) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions().isPendingResource(*this, target.identifier))
            return;

        if (!target.identifier.isEmpty()) {
            document().accessSVGExtensions().addPendingResource(target.identifier, *this);
            ASSERT(hasPendingResources());
        }
    } else if (target.element->hasTagName(SVGNames::pathTag))
        downcast<SVGElement>(*target.element).addReferencingElement(*this);
}

Node::InsertedIntoAncestorResult SVGTextPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGTextContentElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGTextPathElement::didFinishInsertingNode()
{
    buildPendingResource();
}

void SVGTextPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGTextContentElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        clearResourceReferences();
}

bool SVGTextPathElement::selfHasRelativeLengths() const
{
    return startOffset().isRelative()
        || SVGTextContentElement::selfHasRelativeLengths();
}

}
