/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
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

#include "Attribute.h"
#include "RenderSVGResource.h"
#include "RenderSVGTextPath.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGTextPathElement, SVGNames::startOffsetAttr, StartOffset, startOffset)
DEFINE_ANIMATED_ENUMERATION(SVGTextPathElement, SVGNames::methodAttr, Method, method, SVGTextPathMethodType)
DEFINE_ANIMATED_ENUMERATION(SVGTextPathElement, SVGNames::spacingAttr, Spacing, spacing, SVGTextPathSpacingType)
DEFINE_ANIMATED_STRING(SVGTextPathElement, XLinkNames::hrefAttr, Href, href)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGTextPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(startOffset)
    REGISTER_LOCAL_ANIMATED_PROPERTY(method)
    REGISTER_LOCAL_ANIMATED_PROPERTY(spacing)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTextContentElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGTextPathElement::SVGTextPathElement(const QualifiedName& tagName, Document& document)
    : SVGTextContentElement(tagName, document)
    , m_startOffset(LengthModeOther)
    , m_method(SVGTextPathMethodAlign)
    , m_spacing(SVGTextPathSpacingExact)
{
    ASSERT(hasTagName(SVGNames::textPathTag));
    registerAnimatedPropertiesForSVGTextPathElement();
}

PassRefPtr<SVGTextPathElement> SVGTextPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGTextPathElement(tagName, document));
}

SVGTextPathElement::~SVGTextPathElement()
{
    clearResourceReferences();
}

void SVGTextPathElement::clearResourceReferences()
{
    document().accessSVGExtensions()->removeAllTargetReferencesForElement(this);
}

bool SVGTextPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::startOffsetAttr);
        supportedAttributes.get().add(SVGNames::methodAttr);
        supportedAttributes.get().add(SVGNames::spacingAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGTextPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGTextContentElement::parseAttribute(name, value);
    else if (name == SVGNames::startOffsetAttr)
        setStartOffsetBaseValue(SVGLength::construct(LengthModeOther, value, parseError));
    else if (name == SVGNames::methodAttr) {
        SVGTextPathMethodType propertyValue = SVGPropertyTraits<SVGTextPathMethodType>::fromString(value);
        if (propertyValue > 0)
            setMethodBaseValue(propertyValue);
    } else if (name == SVGNames::spacingAttr) {
        SVGTextPathSpacingType propertyValue = SVGPropertyTraits<SVGTextPathSpacingType>::fromString(value);
        if (propertyValue > 0)
            setSpacingBaseValue(propertyValue);
    } else if (SVGURIReference::parseAttribute(name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGTextPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextContentElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        return;
    }

    if (attrName == SVGNames::startOffsetAttr)
        updateRelativeLengthsInformation();

    if (auto renderer = this->renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

RenderPtr<RenderElement> SVGTextPathElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSVGTextPath>(*this, WTF::move(style));
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
    if (!inDocument())
        return;

    String id;
    Element* target = SVGURIReference::targetElementFromIRIString(href(), document(), &id);
    if (!target) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions()->isPendingResource(this, id))
            return;

        if (!id.isEmpty()) {
            document().accessSVGExtensions()->addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (target->hasTagName(SVGNames::pathTag)) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions()->addElementReferencingTarget(this, toSVGElement(target));
    }
}

Node::InsertionNotificationRequest SVGTextPathElement::insertedInto(ContainerNode& rootParent)
{
    SVGTextContentElement::insertedInto(rootParent);
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGTextPathElement::didNotifySubtreeInsertions(ContainerNode*)
{
    buildPendingResource();
}

void SVGTextPathElement::removedFrom(ContainerNode& rootParent)
{
    SVGTextContentElement::removedFrom(rootParent);
    if (rootParent.inDocument())
        clearResourceReferences();
}

bool SVGTextPathElement::selfHasRelativeLengths() const
{
    return startOffset().isRelative()
        || SVGTextContentElement::selfHasRelativeLengths();
}

}
