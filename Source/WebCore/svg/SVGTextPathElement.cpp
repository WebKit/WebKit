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

#if ENABLE(SVG)
#include "SVGTextPathElement.h"

#include "Attribute.h"
#include "NodeRenderingContext.h"
#include "RenderSVGResource.h"
#include "RenderSVGTextPath.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"

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

inline SVGTextPathElement::SVGTextPathElement(const QualifiedName& tagName, Document* document)
    : SVGTextContentElement(tagName, document)
    , m_startOffset(LengthModeOther)
    , m_method(SVGTextPathMethodAlign)
    , m_spacing(SVGTextPathSpacingExact)
{
    ASSERT(hasTagName(SVGNames::textPathTag));
    registerAnimatedPropertiesForSVGTextPathElement();
}

PassRefPtr<SVGTextPathElement> SVGTextPathElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGTextPathElement(tagName, document));
}

bool SVGTextPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::startOffsetAttr);
        supportedAttributes.add(SVGNames::methodAttr);
        supportedAttributes.add(SVGNames::spacingAttr);
    }
    return supportedAttributes.contains<QualifiedName, SVGAttributeHashTranslator>(attrName);
}

void SVGTextPathElement::parseAttribute(Attribute* attr)
{
    SVGParsingError parseError = NoError;
    const AtomicString& value = attr->value();

    if (!isSupportedAttribute(attr->name()))
        SVGTextContentElement::parseAttribute(attr);
    else if (attr->name() == SVGNames::startOffsetAttr)
        setStartOffsetBaseValue(SVGLength::construct(LengthModeOther, value, parseError));
    else if (attr->name() == SVGNames::methodAttr) {
        SVGTextPathMethodType propertyValue = SVGPropertyTraits<SVGTextPathMethodType>::fromString(value);
        if (propertyValue > 0)
            setMethodBaseValue(propertyValue);
    } else if (attr->name() == SVGNames::spacingAttr) {
        SVGTextPathSpacingType propertyValue = SVGPropertyTraits<SVGTextPathSpacingType>::fromString(value);
        if (propertyValue > 0)
            setSpacingBaseValue(propertyValue);
    } else if (SVGURIReference::parseAttribute(attr)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, attr);
}

void SVGTextPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGTextContentElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::startOffsetAttr)
        updateRelativeLengthsInformation();

    if (RenderObject* object = renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
}

RenderObject* SVGTextPathElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGTextPath(this);
}

bool SVGTextPathElement::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    if (childContext.node()->isTextNode()
        || childContext.node()->hasTagName(SVGNames::aTag)
        || childContext.node()->hasTagName(SVGNames::trefTag)
        || childContext.node()->hasTagName(SVGNames::tspanTag))
        return true;

    return false;
}

bool SVGTextPathElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    if (parentNode()
        && (parentNode()->hasTagName(SVGNames::aTag)
            || parentNode()->hasTagName(SVGNames::textTag)))
        return StyledElement::rendererIsNeeded(context);

    return false;
}

Node::InsertionNotificationRequest SVGTextPathElement::insertedInto(Node* rootParent)
{
    SVGStyledElement::insertedInto(rootParent);
    if (!rootParent->inDocument())
        return InsertionDone;

    String id;
    Element* targetElement = SVGURIReference::targetElementFromIRIString(href(), document(), &id);
    if (!targetElement) {
        if (hasPendingResources() || id.isEmpty())
            return InsertionDone;

        ASSERT(!hasPendingResources());
        document()->accessSVGExtensions()->addPendingResource(id, this);
        ASSERT(hasPendingResources());
    }

    return InsertionDone;
}

bool SVGTextPathElement::selfHasRelativeLengths() const
{
    return startOffset().isRelative()
        || SVGTextContentElement::selfHasRelativeLengths();
}

}

#endif // ENABLE(SVG)
