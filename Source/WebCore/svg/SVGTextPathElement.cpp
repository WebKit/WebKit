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

#include "LegacyRenderSVGResource.h"
#include "NodeName.h"
#include "RenderSVGTextPath.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGPathElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTextPathElement);

inline SVGTextPathElement::SVGTextPathElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
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

void SVGTextPathElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    switch (name.nodeName()) {
    case AttributeNames::startOffsetAttr:
        m_startOffset->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Other, newValue, parseError));
        break;
    case AttributeNames::methodAttr: {
        SVGTextPathMethodType propertyValue = SVGPropertyTraits<SVGTextPathMethodType>::fromString(newValue);
        if (propertyValue > 0)
            m_method->setBaseValInternal<SVGTextPathMethodType>(propertyValue);
        break;
    }
    case AttributeNames::spacingAttr: {
        SVGTextPathSpacingType propertyValue = SVGPropertyTraits<SVGTextPathSpacingType>::fromString(newValue);
        if (propertyValue > 0)
            m_spacing->setBaseValInternal<SVGTextPathSpacingType>(propertyValue);
        break;
    }
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGTextContentElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGTextPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);

        if (attrName == SVGNames::startOffsetAttr)
            updateRelativeLengthsInformation();

        updateSVGRendererForElementChange();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        updateSVGRendererForElementChange();
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

    auto target = SVGURIReference::targetElementFromIRIString(href(), treeScopeForSVGReferences());
    if (!target.element) {
        // Do not register as pending if we are already pending this resource.
        auto& treeScope = treeScopeForSVGReferences();
        if (treeScope.isPendingSVGResource(*this, target.identifier))
            return;

        if (!target.identifier.isEmpty()) {
            treeScope.addPendingSVGResource(target.identifier, *this);
            ASSERT(hasPendingResources());
        }
    } else if (RefPtr pathElement = dynamicDowncast<SVGPathElement>(*target.element))
        pathElement->addReferencingElement(*this);
}

Node::InsertedIntoAncestorResult SVGTextPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGTextContentElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void SVGTextPathElement::didFinishInsertingNode()
{
    SVGTextContentElement::buildPendingResource();
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
