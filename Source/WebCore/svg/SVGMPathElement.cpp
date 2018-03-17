/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "SVGNames.h"
#include "SVGPathElement.h"
#include "XLinkNames.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGMPathElement);

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGMPathElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGMPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGMPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGMPathElement::SVGMPathElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::mpathTag));
    registerAnimatedPropertiesForSVGMPathElement();
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

    String id;
    auto target = makeRefPtr(SVGURIReference::targetElementFromIRIString(href(), document(), &id));
    if (!target) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions().isPendingResource(this, id))
            return;

        if (!id.isEmpty()) {
            document().accessSVGExtensions().addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (target->isSVGElement()) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions().addElementReferencingTarget(this, downcast<SVGElement>(target.get()));
    }

    targetPathChanged();
}

void SVGMPathElement::clearResourceReferences()
{
    document().accessSVGExtensions().removeAllTargetReferencesForElement(this);
}

Node::InsertedIntoAncestorResult SVGMPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    return InsertedIntoAncestorResult::Done;
}

void SVGMPathElement::didFinishInsertingNode()
{
    buildPendingResource();
}

void SVGMPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    notifyParentOfPathChange(&oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        clearResourceReferences();
}

void SVGMPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
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
    Element* target = targetElementFromIRIString(href(), document());
    if (is<SVGPathElement>(target))
        return downcast<SVGPathElement>(target);
    return nullptr;
}

void SVGMPathElement::targetPathChanged()
{
    notifyParentOfPathChange(parentNode());
}

void SVGMPathElement::notifyParentOfPathChange(ContainerNode* parent)
{
    if (is<SVGAnimateMotionElement>(parent))
        downcast<SVGAnimateMotionElement>(*parent).updateAnimationPath();
}

} // namespace WebCore
