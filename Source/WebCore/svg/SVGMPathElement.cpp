/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "SVGMPathElement.h"

#include "Document.h"
#include "SVGAnimateMotionElement.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGPathElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGMPathElement);

inline SVGMPathElement::SVGMPathElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::mpathTag));
}

Ref<SVGMPathElement> SVGMPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGMPathElement(tagName, document));
}

SVGMPathElement::~SVGMPathElement()
{
    clearResourceReferences();
}

void SVGMPathElement::buildPendingResource()
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
    } else if (auto* svgElement = dynamicDowncast<SVGElement>(*target.element))
        svgElement->addReferencingElement(*this);

    targetPathChanged();
}

void SVGMPathElement::clearResourceReferences()
{
    removeElementReference();
}

Node::InsertedIntoAncestorResult SVGMPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    return result;
}

void SVGMPathElement::didFinishInsertingNode()
{
    SVGElement::didFinishInsertingNode();
    buildPendingResource();
}

void SVGMPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        clearResourceReferences();
}

void SVGMPathElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGURIReference::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGMPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        buildPendingResource();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

RefPtr<SVGPathElement> SVGMPathElement::pathElement()
{
    auto target = targetElementFromIRIString(href(), treeScopeForSVGReferences());
    return dynamicDowncast<SVGPathElement>(target.element);
}

void SVGMPathElement::targetPathChanged()
{
    auto* parent = parentNode();
    if (auto* animateMotionElement = dynamicDowncast<SVGAnimateMotionElement>(parent))
        animateMotionElement->updateAnimationPath();
}

} // namespace WebCore
